// YiYangWaiHuZiRoom.cpp

#include "YiYangWaiHuZiRoom.h"
#include "YiYangWaiHuZiAvatar.h"
#include "YiYangWaiHuZiMessages.h"
#include "Game/ReplayUtils.h"
#include "Game/WalletEventTask.h"
#include "Game/RiskControlCollector.h"
#include "YiYangWaiHuZiRecordTask.h"
#include "Game/RandomAuditLogger.h"
#include "GameDefines.h"
#include "Network/MsgSession.h"
#include "Base/Log.h"
#include "MySql/MysqlPool.h"

#include <json/json.h>
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <sstream>
#include <msgpack.hpp>

namespace NiuMa
{
	YiYangWaiHuZiRoom::YiYangWaiHuZiRoom(const std::string& venueId, const std::string& number, int level, const std::string& ruleConfig)
		: GameRoom(venueId, static_cast<int>(GameType::YiYangWaiHuZi), 3)
		, _number(number), _level(level), _gameState(GameState::None), _stateTime(0)
		, _roundNo(0), _banker(0), _currentPlayer(0), _lastDiscardSeat(-1), _lastDiscardId(-1)
			, _tileIndex(0), _drawCount(0)
			, _playerCount(3), _minHuXi(15), _maxScore(300), _tunScoreRate(1)
			, _roundLimit(0), _roomFee(0)
			, _allowChi(true), _allowPeng(true), _allowWei(true), _allowPao(true), _allowTi(true)
			, _zhuangRule(0)
		{
			for (int i = 0; i < 3; i++)
				_totalWinGolds[i] = 0;
			parseRuleConfig(ruleConfig);
		}

	YiYangWaiHuZiRoom::~YiYangWaiHuZiRoom() {}

	void YiYangWaiHuZiRoom::parseRuleConfig(const std::string& cfg) {
		if (cfg.empty()) return;
		Json::Value root;
		Json::CharReaderBuilder builder;
		Json::CharReader* reader = builder.newCharReader();
		std::string errs;
		if (!reader->parse(cfg.c_str(), cfg.c_str() + cfg.size(), &root, &errs)) { delete reader; return; }
		delete reader;
		if (root.isMember("player_count")) _playerCount = root["player_count"].asInt();
		if (root.isMember("min_huxi")) _minHuXi = root["min_huxi"].asInt();
			if (root.isMember("max_score")) _maxScore = root["max_score"].asInt();
			if (root.isMember("tun_score_rate")) _tunScoreRate = root["tun_score_rate"].asInt();
			if (root.isMember("round_count")) _roundLimit = root["round_count"].asInt();
			if (root.isMember("round_limit")) _roundLimit = root["round_limit"].asInt();
			if (root.isMember("room_fee") && root["room_fee"].isInt64())
				_roomFee = root["room_fee"].asInt64();
			else if (root.isMember("room_fee") && root["room_fee"].isInt())
				_roomFee = root["room_fee"].asInt();
			else if (root.isMember("room_fee_type") && root["room_fee_type"].isInt())
				_roomFee = root["room_fee_type"].asInt();
			if (root.isMember("allow_chi")) _allowChi = root["allow_chi"].asBool();
		if (root.isMember("allow_peng")) _allowPeng = root["allow_peng"].asBool();
		if (root.isMember("allow_wei")) _allowWei = root["allow_wei"].asBool();
		if (root.isMember("allow_pao")) _allowPao = root["allow_pao"].asBool();
		if (root.isMember("allow_ti")) _allowTi = root["allow_ti"].asBool();
		if (root.isMember("zhuang_rule")) _zhuangRule = root["zhuang_rule"].asInt();
	}

	GameAvatar::Ptr YiYangWaiHuZiRoom::createAvatar(const std::string& playerId, int seat, bool robot) const {
		return std::make_shared<YiYangWaiHuZiAvatar>(playerId, seat, robot);
	}

	bool YiYangWaiHuZiRoom::checkEnter(const std::string& playerId, std::string& errMsg, bool robot) const {
		if (getAvatarCount() >= _playerCount) { errMsg = "房间已满"; return false; }
		return true;
	}

	int YiYangWaiHuZiRoom::checkLeave(const std::string& playerId, std::string& errMsg) const {
		if (_gameState == GameState::Playing) { errMsg = "游戏进行中"; return 2; }
		return 0;
	}

	void YiYangWaiHuZiRoom::onAvatarLeaved(int seat, const std::string& playerId) {
		if (getAvatarCount() == 0) _gameState = GameState::None;
	}

	void YiYangWaiHuZiRoom::clean() {
		_gameState = GameState::None;
		_roundNo = 0;
		_tilePool.clear();
		_tileIndex = 0;
		_drawCount = 0;
		_playbackData = WaiHuZiPlaybackData();
		for (int _si = 0; _si < _playerCount; _si++) {
			auto a = getAvatar(_si);
			if (a) a->clear();
		}
	}

	void YiYangWaiHuZiRoom::onTimer() {}
	void YiYangWaiHuZiRoom::setState(GameState s) { _gameState = s; _stateTime = std::time(nullptr); }

	std::shared_ptr<YiYangWaiHuZiAvatar> YiYangWaiHuZiRoom::getAvatar(int seat) const {
		return std::dynamic_pointer_cast<YiYangWaiHuZiAvatar>(GameRoom::getAvatar(seat));
	}

	int YiYangWaiHuZiRoom::getNextSeat(int seat) const { return (seat + 1) % _playerCount; }

	bool YiYangWaiHuZiRoom::allReady() const {
		if (getAvatarCount() < _playerCount) return false;
		for (int _si = 0; _si < _playerCount; _si++) {
			auto a = getAvatar(_si);
			if (a && !a->isReady()) return false;
		}
		return true;
	}

	void YiYangWaiHuZiRoom::initTilePool() {
		_tilePool.clear();
		int id = 0;
		// 小字1-10，各4张
		for (int pt = 1; pt <= 10; pt++) {
			for (int c = 0; c < 4; c++)
				_tilePool.push_back(WaiHuZiCard(pt, id++));
		}
		// 大字11-20，各4张
		for (int pt = 11; pt <= 20; pt++) {
			for (int c = 0; c < 4; c++)
				_tilePool.push_back(WaiHuZiCard(pt, id++));
		}
	}

	void YiYangWaiHuZiRoom::shufflePool() {
		srand(static_cast<unsigned>(std::time(nullptr)));
		for (size_t i = _tilePool.size() - 1; i > 0; i--) {
			int j = rand() % (static_cast<int>(i) + 1);
			std::swap(_tilePool[i], _tilePool[j]);
		}
		_tileIndex = 0;
		_drawCount = 0;
	}

	bool YiYangWaiHuZiRoom::drawCard(int seat) {
		if (_tileIndex >= static_cast<int>(_tilePool.size()))
			return false;
		auto a = getAvatar(seat);
		if (!a) return false;
		WaiHuZiCard card = _tilePool[_tileIndex++];
		a->addCard(card);
		_drawCount++;

		// 通知摸牌
		notifyDraw(seat, card.getId());

		// 检查偎/跑/提
		checkAvailableActions(seat);
		return true;
	}

	void YiYangWaiHuZiRoom::startRound() {
		_roundNo++;
		_lastDiscardSeat = -1;
		_lastDiscardId = -1;

		// 清理
		_playbackData = WaiHuZiPlaybackData();
		_playbackData.venueId = getId();
		_playbackData.roundNo = _roundNo;
		_playbackData.banker = _banker;
		_playbackData.playerCount = _playerCount;

		for (int _si = 0; _si < _playerCount; _si++) {
			auto a = getAvatar(_si);
			if (a) { a->clear(); a->setReady(false); }
		}

		// 风控采集：开始新局
		_riskCollector.startRound(getId(), static_cast<int>(GameType::YiYangWaiHuZi), _roundNo);
		for (int _si = 0; _si < _playerCount; _si++) {
			auto _av = GameRoom::getAvatar(_si);
			if (_av) _riskCollector.recordPlayer(_av->getPlayerId(), _si);
		}

		// 初始化牌池并洗牌
		initTilePool();
		shufflePool();

		// 发牌
		dealCards();

		_currentPlayer = _banker;
		setState(GameState::Playing);

		// 庄家先摸牌
		drawCard(_currentPlayer);

		{ std::ostringstream _oss; _oss << "益阳歪胡子游戏开始，场地: " << getId() << "，局号: " << _roundNo; LOG_INFO(_oss.str().c_str()); }
	}

	void YiYangWaiHuZiRoom::dealCards() {
		// 3人各发20张，剩余牌堆
		int cardsPerPlayer = 20;
		int idx = 0;
		for (int _si = 0; _si < _playerCount; _si++) {
			auto a = getAvatar(_si);
			if (!a) continue;
			WaiHuZiCardArray hand;
			for (int i = 0; i < cardsPerPlayer && _tileIndex < static_cast<int>(_tilePool.size()); i++) {
				hand.push_back(_tilePool[_tileIndex++]);
			}
			a->setHandCards(hand);
			std::vector<int> ids;
			for (auto& c : hand) ids.push_back(c.getId());
			_playbackData.playerIds[idx] = a->getPlayerId();
			_playbackData.initCards[idx] = ids;
			idx++;
		}

		// 通知发牌
		for (int _si = 0; _si < _playerCount; _si++) {
			auto _av = GameRoom::getAvatar(_si);
			if (_av) notifyDeal(_av->getPlayerId());
		}
	}

	void YiYangWaiHuZiRoom::checkAvailableActions(int seat) {
		auto a = getAvatar(seat);
		if (!a) return;
		// 检查偎/跑/提/胡
		// 实际实现中需要详细检查手牌，此处简化
	}

	bool YiYangWaiHuZiRoom::canChi(int seat, const WaiHuZiCard& card) const {
		if (!_allowChi) return false;
		auto a = getAvatar(seat);
		if (!a) return false;
		// 简化：检查手牌中是否有能与打出的牌组成顺子的牌
		return false;
	}

	bool YiYangWaiHuZiRoom::canPeng(int seat, const WaiHuZiCard& card) const {
		if (!_allowPeng) return false;
		auto a = getAvatar(seat);
		if (!a) return false;
		int count = 0;
		for (auto& c : a->getHandCards()) {
			if (c.getPoint() == card.getPoint())
				count++;
		}
		return count >= 2;
	}

	bool YiYangWaiHuZiRoom::canWei(int seat) const {
		if (!_allowWei) return false;
		return false;
	}

	bool YiYangWaiHuZiRoom::canPao(int seat) const {
		if (!_allowPao) return false;
		return false;
	}

	bool YiYangWaiHuZiRoom::canTi(int seat) const {
		if (!_allowTi) return false;
		return false;
	}

	bool YiYangWaiHuZiRoom::canHu(int seat) const {
		auto a = getAvatar(seat);
		if (!a) return false;
		int huxi = calcTotalHuXi(seat);
		return huxi >= _minHuXi;
	}

	int YiYangWaiHuZiRoom::calcHuXiForCombination(int type, const std::vector<int>& cardIds) const {
		// 小字碰=1息, 大字碰=3息
		// 小字偎=3息, 大字偎=6息
		// 小字跑=6息, 大字跑=9息
		// 小字提=9息, 大字提=12息
		// 小字坎=3息, 大字坎=6息
		if (cardIds.empty()) return 0;
		// 取第一张牌判断大小
		int pt = -1;
		for (auto& c : _tilePool) {
			if (c.getId() == cardIds[0]) { pt = c.getPoint(); break; }
		}
		if (pt <= 0) return 0;
		bool big = (pt >= 11);
		switch (static_cast<WaiHuZiCombType>(type)) {
		case WaiHuZiCombType::Peng: return big ? 3 : 1;
		case WaiHuZiCombType::Wei: return big ? 6 : 3;
		case WaiHuZiCombType::Kan: return big ? 6 : 3;
		case WaiHuZiCombType::Pao: return big ? 9 : 6;
		case WaiHuZiCombType::Ti: return big ? 12 : 9;
		default: return 0;
		}
	}

	int YiYangWaiHuZiRoom::calcTotalHuXi(int seat) const {
		auto a = getAvatar(seat);
		if (!a) return 0;
		return a->calcHuXi();
	}

	int YiYangWaiHuZiRoom::calcPenaltyHuXi() const {
		// 底息惩罚
		return _tunScoreRate;
	}

	void YiYangWaiHuZiRoom::doDiscard(int seat, int cardId) {
		if (seat != _currentPlayer) return;
		auto a = getAvatar(seat);
		if (!a || !a->hasCard(cardId)) return;

		a->removeCardById(cardId);
		a->setDiscarded(true);
		_lastDiscardSeat = seat;
		_lastDiscardId = cardId;

		notifyDiscard(seat, cardId);

		// 检查下家能否吃/碰
		int nextSeat = getNextSeat(seat);
		_currentPlayer = nextSeat;

		// 摸牌
		if (!drawCard(nextSeat)) {
			// 牌池空了，荒庄
			setState(GameState::Settling);
		}
	}

	void YiYangWaiHuZiRoom::doAction(int seat, int action, const std::vector<int>& cardIds) {
		auto a = getAvatar(seat);
		if (!a) return;

		switch (static_cast<WaiHuZiAction>(action)) {
		case WaiHuZiAction::Hu: {
			if (canHu(seat)) {
				a->setHu(true);
				settle(seat);
			}
			break;
		}
		case WaiHuZiAction::Peng: {
			WaiHuZiCombination comb;
			comb.type = static_cast<int>(WaiHuZiCombType::Peng);
			if (_lastDiscardId >= 0) {
				// 组合碰牌
				for (auto& c : a->getHandCards()) {
					if (cardIds.empty() || c.getPoint() == _tilePool[cardIds[0] < static_cast<int>(_tilePool.size()) ? cardIds[0] : 0].getPoint()) {
						comb.cards.push_back(c.getId());
						if (comb.cards.size() >= 2) break;
					}
				}
				comb.cards.push_back(_lastDiscardId);
				comb.huXi = calcHuXiForCombination(comb.type, comb.cards);
				a->addExposedComb(comb);
				a->removeCardsByIds(cardIds);
				_currentPlayer = seat;
				notifyAction(seat, action, comb.cards);
			}
			break;
		}
		case WaiHuZiAction::Pass:
			// 过牌，轮到下一家
			_currentPlayer = getNextSeat(seat);
			if (!drawCard(_currentPlayer)) {
				setState(GameState::Settling);
			}
			break;
		default:
			break;
		}
	}

	void YiYangWaiHuZiRoom::settle(int huSeat) {
		int huxi = calcTotalHuXi(huSeat);
		int baseScore = huxi * _tunScoreRate;
		if (baseScore > _maxScore) baseScore = _maxScore;

		for (int _si = 0; _si < _playerCount; _si++) {
			auto a = getAvatar(_si);
			if (!a) continue;
				if (_si == huSeat) {
					a->setRoundScore(baseScore * (_playerCount - 1));
					a->setWinGold(baseScore * (_playerCount - 1) * _level);
				}
				else {
					a->setRoundScore(-baseScore);
					a->setWinGold(-baseScore * _level);
				}
				if (_si < 3)
					_totalWinGolds[_si] += static_cast<int64_t>(a->getWinGold());
			}

			notifySettlement(huSeat);
			saveRoundRecord();
			_banker = huSeat;
			if (_roundLimit > 0 && _roundNo >= _roundLimit) {
				publishFinalRoomFee();
				gameOver();
				return;
			}
			setState(GameState::Ready);
		}

		void YiYangWaiHuZiRoom::publishFinalRoomFee() {
			if (_roundNo <= 0)
				return;
			std::vector<std::pair<std::string, int64_t>> netWins;
			for (int _si = 0; _si < _playerCount && _si < 3; _si++) {
				auto a = getAvatar(_si);
				if (!a)
					continue;
				netWins.emplace_back(a->getPlayerId(), _totalWinGolds[_si]);
			}
			publishRoomFeeOnGameOver(_roomFee, netWins, "YiYangWaiHuZi", "益阳歪胡子整场房费");
		}

	void YiYangWaiHuZiRoom::saveRoundRecord() {
		auto task = std::make_shared<YiYangWaiHuZiRecordTask>();
		task->_venueId = getId();
		task->_roundNo = _roundNo;
		task->_banker = _banker;
		_playbackData.randomSeedHash = ReplayUtils::generateSeedHash(getId(), _roundNo, _banker);
		task->_randomSeedHash = _playbackData.randomSeedHash;

		int idx = 0;
		for (int _si = 0; _si < _playerCount; _si++) {
			auto a = getAvatar(_si);
			if (a && idx < 3) {
				task->_playerIds[idx] = a->getPlayerId();
				task->_scores[idx] = a->getRoundScore();
				task->_winGolds[idx] = a->getWinGold();
				task->_huXi[idx] = a->getTotalHuXi();
			}
			idx++;
		}

		std::string replayData;
		msgpack::sbuffer _sbuf;
		msgpack::pack(_sbuf, _playbackData);
		ReplayUtils::compressReplay(_sbuf.data(), static_cast<int>(_sbuf.size()), replayData);
		task->_playback = replayData;
		MysqlPool::getSingleton().asyncQuery(task);

		// 风控采集：记录得分并结束
		_riskCollector.setRandomSeedHash(_playbackData.randomSeedHash);
		for (int _si = 0; _si < _playerCount; _si++) {
			auto a = getAvatar(_si);
			if (a)
				_riskCollector.recordScore(a->getPlayerId(), a->getRoundScore(), static_cast<int64_t>(a->getWinGold()));
		}
		_riskCollector.finishRound();

		// 随机审计日志
		std::vector<int> cardOrder;
		std::vector<std::string> playerIds;
		for (int _si = 0; _si < _playerCount; _si++) {
			auto _av = GameRoom::getAvatar(_si);
			if (_av) playerIds.push_back(_av->getPlayerId());
		}
		RandomAuditLogger::logAudit(getId(), static_cast<int>(GameType::YiYangWaiHuZi),
			_roundNo, _banker, _playbackData.randomSeedHash, cardOrder, playerIds);

		// MQ事件
		for (int _si = 0; _si < _playerCount; _si++) {
			auto a = getAvatar(_si);
			if (!a) continue;
			double g = a->getWinGold();
			if (g > 0) WalletEventTask::publish(a->getPlayerId(), "GAME_WIN", static_cast<int64_t>(g), "YiYangWaiHuZi", getId(), "歪胡子赢得金币");
			else if (g < 0) WalletEventTask::publish(a->getPlayerId(), "GAME_LOSE", static_cast<int64_t>(-g), "YiYangWaiHuZi", getId(), "歪胡子输掉金币");
		}
	}

	bool YiYangWaiHuZiRoom::onMessage(const NetMessage::Ptr& netMsg) {
		const std::string& type = netMsg->getType();
		if (type == MsgWaiHuZiSync::TYPE) { onSyncTable(netMsg); return true; }
		if (type == MsgWaiHuZiReady::TYPE) { onReady(netMsg); return true; }
		if (type == MsgWaiHuZiDiscard::TYPE) { onDiscard(netMsg); return true; }
		if (type == MsgWaiHuZiAction::TYPE) { onAction(netMsg); return true; }
		return false;
	}

	void YiYangWaiHuZiRoom::onSyncTable(const NetMessage::Ptr& netMsg) {
		auto msg = std::dynamic_pointer_cast<MsgWaiHuZiSync>(netMsg);
		if (!msg) return;
		auto session = netMsg->getSession();
		if (!session) return;
		auto resp = std::make_shared<MsgWaiHuZiSyncResp>();
		resp->gameState = static_cast<int>(_gameState);
		resp->roundNo = _roundNo;
		resp->banker = _banker;
		resp->playerCount = _playerCount;
		resp->currentPlayer = _currentPlayer;
		resp->lastDiscardId = _lastDiscardId;
		resp->lastDiscardSeat = _lastDiscardSeat;
		const std::string& playerId = msg->getPlayerId();
		for (int _si = 0; _si < _playerCount; _si++) {
			auto _av = GameRoom::getAvatar(_si);
			if (_av && _av->getPlayerId() == playerId) {
				resp->mySeat = _si;
				auto a = getAvatar(_si);
				if (a) for (auto& c : a->getHandCards()) resp->myCards.push_back(c.getId());
				break;
			}
		}
		resp->send(session);
	}

	void YiYangWaiHuZiRoom::onReady(const NetMessage::Ptr& netMsg) {
		auto msg = std::dynamic_pointer_cast<MsgWaiHuZiReady>(netMsg);
		if (!msg) return;
		const std::string& playerId = msg->getPlayerId();
		for (int _si = 0; _si < _playerCount; _si++) {
			auto _av = GameRoom::getAvatar(_si);
			if (_av && _av->getPlayerId() == playerId) {
				auto a = getAvatar(_si);
				if (a) a->setReady(true);
				break;
			}
		}
		setState(GameState::Ready);
		if (allReady()) startRound();
	}

	void YiYangWaiHuZiRoom::onDiscard(const NetMessage::Ptr& netMsg) {
		if (_gameState != GameState::Playing) return;
		auto msg = std::dynamic_pointer_cast<MsgWaiHuZiDiscard>(netMsg);
		if (!msg) return;
		const std::string& playerId = msg->getPlayerId();
		for (int _si = 0; _si < _playerCount; _si++) {
			auto _av = GameRoom::getAvatar(_si);
			if (_av && _av->getPlayerId() == playerId) { doDiscard(_si, msg->cardId); break; }
		}
	}

	void YiYangWaiHuZiRoom::onAction(const NetMessage::Ptr& netMsg) {
		if (_gameState != GameState::Playing) return;
		auto msg = std::dynamic_pointer_cast<MsgWaiHuZiAction>(netMsg);
		if (!msg) return;
		const std::string& playerId = msg->getPlayerId();
		for (int _si = 0; _si < _playerCount; _si++) {
			auto _av = GameRoom::getAvatar(_si);
			if (_av && _av->getPlayerId() == playerId) { doAction(_si, msg->action, msg->cardIds); break; }
		}
	}

	// 消息发送
	void YiYangWaiHuZiRoom::notifyDeal(const std::string& playerId) {
		auto _av = GameRoom::getAvatar(playerId);
		if (!_av) return;
		int seat = -1;
		for (int _si = 0; _si < _playerCount; _si++) {
			auto tmp = GameRoom::getAvatar(_si);
			if (tmp && tmp->getPlayerId() == playerId) { seat = _si; break; }
		}
		if (seat < 0) return;
		auto a = getAvatar(seat);
		if (!a) return;
		auto msg = std::make_shared<MsgWaiHuZiDeal>();
		for (auto& c : a->getHandCards()) msg->cards.push_back(c.getId());
		msg->firstPlayer = _currentPlayer;
		msg->roundNo = _roundNo;
		msg->banker = _banker;
		sendMessage(*msg, playerId);
	}

	void YiYangWaiHuZiRoom::notifyDraw(int seat, int cardId) {
		auto msg = std::make_shared<MsgWaiHuZiDrawNotify>();
		msg->seat = seat;
		msg->cardId = cardId;
		msg->remainCount = static_cast<int>(_tilePool.size()) - _tileIndex;
		sendMessageToAll(*msg);
		// 摸牌者发实际牌面，其他人只通知摸牌
		auto a = getAvatar(seat);
		if (a) {
			// 需要单独给摸牌者发送cardId
		}
	}

	void YiYangWaiHuZiRoom::notifyDiscard(int seat, int cardId) {
		auto msg = std::make_shared<MsgWaiHuZiDiscardNotify>();
		msg->seat = seat;
		msg->cardId = cardId;
		msg->nextPlayer = _currentPlayer;
		sendMessageToAll(*msg);
	}

	void YiYangWaiHuZiRoom::notifyAction(int seat, int action, const std::vector<int>& cardIds) {
		auto msg = std::make_shared<MsgWaiHuZiActionNotify>();
		msg->seat = seat;
		msg->action = action;
		msg->cardIds = cardIds;
		msg->nextPlayer = _currentPlayer;
		sendMessageToAll(*msg);
	}

	void YiYangWaiHuZiRoom::notifySettlement(int huSeat) {
		auto msg = std::make_shared<MsgWaiHuZiSettlement>();
		msg->huSeat = huSeat;
		int idx = 0;
		for (int _si = 0; _si < _playerCount; _si++) {
			auto a = getAvatar(_si);
			if (a && idx < 3) {
				msg->scores[idx] = a->getRoundScore();
				msg->winGolds[idx] = a->getWinGold();
			}
			idx++;
		}
		sendMessageToAll(*msg);
	}
}
