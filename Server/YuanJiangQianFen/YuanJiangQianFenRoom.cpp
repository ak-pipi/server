// YuanJiangQianFenRoom.cpp

#include "YuanJiangQianFenRoom.h"
#include "YuanJiangQianFenMessages.h"
#include "Game/ReplayUtils.h"
#include "Game/WalletEventTask.h"
#include "Game/RiskControlCollector.h"
#include "YuanJiangQianFenRecordTask.h"
#include "Game/RandomAuditLogger.h"
#include "GameDefines.h"
#include "Network/MsgSession.h"
#include "Base/Log.h"
#include "MySql/MysqlPool.h"

#include <json/json.h>
#include <algorithm>
#include <sstream>
#include <msgpack.hpp>

namespace NiuMa
{
	YuanJiangQianFenRoom::YuanJiangQianFenRoom(const std::string& venueId, const std::string& number, int level, const std::string& ruleConfig)
		: GameRoom(venueId, static_cast<int>(GameType::YuanJiangQianFen), 4)
		, _number(number), _level(level), _gameState(GameState::None)
		, _roundNo(0), _bankerSeat(0), _currentPlayer(0)
		, _callScoreCurrent(0), _highestCallScore(0), _highestCallSeat(-1), _callScoreCount(0)
		, _lastPlaySeat(-1), _isFirstPlay(true), _roundCount(0)
			, _rule(std::make_shared<PaoDeKuaiRule>())
			, _dealer(_rule)
			, _playerCount(4), _targetScore(1000), _callScoreEnabled(true), _bankerRule(0)
			, _deckCount(2), _bombEnabled(false), _roundLimit(0), _maxScore(500), _roomFee(0)
	{
		_rule->initialise();
		for (int i = 0; i < 4; i++) { _totalScores[i] = 0; _roundScores[i] = 0; _totalGolds[i] = 0; _roundGolds[i] = 0; }
		parseRuleConfig(ruleConfig);
	}

	YuanJiangQianFenRoom::~YuanJiangQianFenRoom() {}

	void YuanJiangQianFenRoom::parseRuleConfig(const std::string& cfg) {
		if (cfg.empty()) return;
		Json::Value root;
		Json::CharReaderBuilder builder;
		Json::CharReader* reader = builder.newCharReader();
		std::string errs;
		if (!reader->parse(cfg.c_str(), cfg.c_str() + cfg.size(), &root, &errs)) { delete reader; return; }
		delete reader;
		if (root.isMember("player_count")) _playerCount = root["player_count"].asInt();
		if (root.isMember("target_score")) _targetScore = root["target_score"].asInt();
		if (root.isMember("call_score_enabled")) _callScoreEnabled = root["call_score_enabled"].asBool();
		if (root.isMember("banker_rule")) _bankerRule = root["banker_rule"].asInt();
		if (root.isMember("deck_count")) _deckCount = root["deck_count"].asInt();
		if (root.isMember("bomb_enabled")) _bombEnabled = root["bomb_enabled"].asBool();
		if (root.isMember("round_count")) _roundLimit = root["round_count"].asInt();
		if (root.isMember("round_limit")) _roundLimit = root["round_limit"].asInt();
		if (_roundLimit <= 0 || (_roundLimit > 1 && _roundLimit < 8))
			_roundLimit = 8;
		if (root.isMember("max_score")) _maxScore = root["max_score"].asInt();
		if (root.isMember("room_fee") && root["room_fee"].isInt64())
			_roomFee = root["room_fee"].asInt64();
		else if (root.isMember("room_fee") && root["room_fee"].isInt())
			_roomFee = root["room_fee"].asInt();
		else if (root.isMember("room_fee_type") && root["room_fee_type"].isInt())
			_roomFee = root["room_fee_type"].asInt();
		if (root.isMember("score_cards") && root["score_cards"].isObject()) {
			for (const auto& key : root["score_cards"].getMemberNames())
				_scoreCardMap[std::atoi(key.c_str())] = root["score_cards"][key].asInt();
		}
		// 默认计分牌配置
		if (_scoreCardMap.empty()) {
			_scoreCardMap[5] = 5; _scoreCardMap[10] = 10;
			_scoreCardMap[static_cast<int>(PokerPoint::King)] = 10;
		}
	}

	GameAvatar::Ptr YuanJiangQianFenRoom::createAvatar(const std::string& playerId, int seat, bool robot) const {
		return std::make_shared<PaoDeKuaiAvatar>(_rule, playerId, seat, robot);
	}

	bool YuanJiangQianFenRoom::checkEnter(const std::string& playerId, std::string& errMsg, bool robot) const {
		if (getAvatarCount() >= 4) { errMsg = "房间已满"; return false; }
		return true;
	}

	int YuanJiangQianFenRoom::checkLeave(const std::string& playerId, std::string& errMsg) const {
		if (_gameState == GameState::Playing || _gameState == GameState::CallScore) { errMsg = "游戏进行中"; return 2; }
		return 0;
	}

	void YuanJiangQianFenRoom::onAvatarLeaved(int seat, const std::string& playerId) {
		if (getAvatarCount() == 0) _gameState = GameState::None;
	}

	void YuanJiangQianFenRoom::clean() {
		GameRoom::clean();
		_gameState = GameState::None;
		_roundNo = 0; _roundCount = 0;
		for (int i = 0; i < 4; i++) { _totalScores[i] = 0; _roundScores[i] = 0; _totalGolds[i] = 0; _roundGolds[i] = 0; }
		_playbackData = QianFenPlaybackData();
	}

	bool YuanJiangQianFenRoom::canShuffleCardsBeforeNextRound(const std::string& playerId,
		int& nextRoundNo,
		int& roundCount,
		std::string& errMsg) const {
		nextRoundNo = _roundNo + 1;
		roundCount = _roundLimit;
		if (!hasAvatar(playerId)) {
			errMsg = "你不在当前房间内";
			return false;
		}
		if (_roundNo <= 0) {
			errMsg = "首局开始前不能洗牌";
			return false;
		}
		if (_roundLimit > 0 && _roundCount >= _roundLimit) {
			errMsg = "全部对局已结束，不能洗牌";
			return false;
		}
		if (_gameState != GameState::Ready && _gameState != GameState::None) {
			errMsg = "下局开始前才能洗牌";
			return false;
		}
		return true;
	}

	void YuanJiangQianFenRoom::onTimer() {}
	void YuanJiangQianFenRoom::setState(GameState s) { _gameState = s; }

	bool YuanJiangQianFenRoom::allReady() const {
		if (getAvatarCount() < 4) return false;
		for (int _si = 0; _si < 4; _si++) {
			auto a = std::dynamic_pointer_cast<PokerAvatar>(GameRoom::getAvatar(_si));
			if (a && !a->isPlayed() && _gameState == GameState::None) return false;
		}
		return true;
	}

	int YuanJiangQianFenRoom::calcScoreCards(const CardArray& cards) const {
		int score = 0;
		for (auto& c : cards) {
			auto it = _scoreCardMap.find(c.getPoint());
			if (it != _scoreCardMap.end())
				score += it->second;
		}
		return score;
	}

	void YuanJiangQianFenRoom::startRound() {
		_roundNo++; _roundCount++;
		_highestCallScore = 0; _highestCallSeat = -1; _callScoreCount = 0;
		_isFirstPlay = true; _lastPlaySeat = -1; _lastPlayGenre.clear();
		for (int i = 0; i < 4; i++) { _roundScores[i] = 0; _roundGolds[i] = 0; }

		_playbackData = QianFenPlaybackData();
		_playbackData.venueId = getId();
		_playbackData.roundNo = _roundNo;
		_playbackData.banker = _bankerSeat;
		_playbackData.playerCount = _playerCount;

		// 风控采集：开始新局
		_riskCollector.startRound(getId(), static_cast<int>(GameType::YuanJiangQianFen), _roundNo);
		for (int _si = 0; _si < 4; _si++) {
			auto _av = GameRoom::getAvatar(_si);
			if (_av) _riskCollector.recordPlayer(_av->getPlayerId(), _si);
		}

		_dealer.shuffle();
		dealCards();

		if (_callScoreEnabled)
			beginCallScore();
		else
			beginPlay();

		{ std::ostringstream _oss; _oss << "沅江千分游戏开始，场地: " << getId() << "，局号: " << _roundNo; LOG_INFO(_oss.str().c_str()); }
	}

	void YuanJiangQianFenRoom::dealCards() {
		std::vector<CardArray> heaps;
		_dealer.shuffle();
		_dealer.handOutCards(heaps, 4);

		int idx = 0;
		for (int _si = 0; _si < 4; _si++) {
			auto a = std::dynamic_pointer_cast<PokerAvatar>(GameRoom::getAvatar(_si));
			if (a && idx < static_cast<int>(heaps.size())) {
				a->setCards(heaps[idx]);
				a->sortCards();
				std::vector<int> ids;
				for (auto& c : heaps[idx]) ids.push_back(c.getId());
				_playbackData.playerIds[idx] = a->getPlayerId();
				_playbackData.initCards[idx] = ids;
			}
			idx++;
		}
		for (int _si = 0; _si < 4; _si++) {
			auto _av = GameRoom::getAvatar(_si);
			if (_av) notifyDeal(_av->getPlayerId());
		}
	}

	void YuanJiangQianFenRoom::beginCallScore() {
		_gameState = GameState::CallScore;
		_callScoreCurrent = _bankerSeat;
		_currentPlayer = _callScoreCurrent;
	}

	void YuanJiangQianFenRoom::processCallScore(int seat, int score) {
		_callScoreCount++;
		if (score > _highestCallScore) {
			_highestCallScore = score;
			_highestCallSeat = seat;
		}
		int nextSeat = getNextSeat(seat);
		notifyCallScore(seat, score, nextSeat);

		if (_callScoreCount >= _playerCount) {
			// 叫分结束
			_bankerSeat = (_highestCallSeat >= 0) ? _highestCallSeat : _bankerSeat;
			beginPlay();
		}
		else {
			_callScoreCurrent = nextSeat;
			_currentPlayer = _callScoreCurrent;
		}
	}

	void YuanJiangQianFenRoom::beginPlay() {
		_gameState = GameState::Playing;
		_currentPlayer = _bankerSeat;
		_isFirstPlay = true;
	}

	void YuanJiangQianFenRoom::doPlay(int seat, const std::vector<int>& cardIds) {
		if (seat != _currentPlayer || cardIds.empty()) return;
		auto a = std::dynamic_pointer_cast<PokerAvatar>(GameRoom::getAvatar(seat));
		if (!a) return;

		CardArray cards;
		if (!a->getCardsByIds(cardIds, cards)) return;

		PokerGenre genre;
		genre.setCards(cards, _rule);
		int g = _rule->predicateCardGenre(genre);
		if (g <= 0) return;

		if (!_isFirstPlay) {
			int cmp = _rule->compareGenre(genre, _lastPlayGenre);
			if (cmp != 1) return;
		}

		a->removeCardsByIds(cardIds);
		_lastPlaySeat = seat;
		_lastPlayGenre = genre;
		_isFirstPlay = false;

		notifyPlay(seat, cardIds);

		// 轮到下一家
		int next = getNextSeat(seat);
		// 如果轮回到上一个出牌者，新一轮
		if (next == _lastPlaySeat) {
			_isFirstPlay = true;
			_lastPlayGenre.clear();
		}
		_currentPlayer = next;

		// 检查是否有人出完牌
		if (a->getCardNums() == 0) {
			settleRound();
		}
	}

	void YuanJiangQianFenRoom::settleRound() {
		// 计算每人本轮得分
		for (int _si = 0; _si < 4; _si++) {
			auto a = std::dynamic_pointer_cast<PokerAvatar>(GameRoom::getAvatar(_si));
			if (!a) continue;
			int score = calcScoreCards(a->getCards());
			_roundScores[_si] = -score; // 剩余牌的分数为负
			_totalScores[_si] += _roundScores[_si];
		}

		// 赢家（出完牌的人）
		int winnerSeat = -1;
		for (int _si = 0; _si < 4; _si++) {
			auto a = std::dynamic_pointer_cast<PokerAvatar>(GameRoom::getAvatar(_si));
			if (a && a->getCardNums() == 0) { winnerSeat = _si; break; }
		}
		if (winnerSeat >= 0) {
			int winnerScore = 0;
			for (int i = 0; i < 4; i++) winnerScore -= _roundScores[i]; // 赢家得其他人负分的绝对值之和
			_roundScores[winnerSeat] = winnerScore;
			_totalScores[winnerSeat] += winnerScore;
		}

		int64_t desiredGolds[4] = { 0LL, 0LL, 0LL, 0LL };
		int64_t totalDesiredWin = 0LL;
		int64_t totalPaid = 0LL;
		for (int i = 0; i < 4; i++) {
			desiredGolds[i] = static_cast<int64_t>(_roundScores[i]) * _level;
			_roundGolds[i] = 0LL;
			auto avatar = GameRoom::getAvatar(i);
			if (!avatar)
				continue;
			if (desiredGolds[i] < 0LL) {
				int64_t cashPledgeLimit = static_cast<int64_t>(std::max(0.0, avatar->getCashPledge()));
				int64_t loss = std::min<int64_t>(-desiredGolds[i], cashPledgeLimit);
				_roundGolds[i] = -loss;
				totalPaid += loss;
			}
			else if (desiredGolds[i] > 0LL)
				totalDesiredWin += desiredGolds[i];
		}
		int64_t distributed = 0LL;
		int lastWinner = -1;
		if (totalDesiredWin > 0LL) {
			for (int i = 0; i < 4; i++) {
				if (desiredGolds[i] <= 0LL)
					continue;
				lastWinner = i;
				_roundGolds[i] = desiredGolds[i] * totalPaid / totalDesiredWin;
				distributed += _roundGolds[i];
			}
			if (lastWinner >= 0)
				_roundGolds[lastWinner] += totalPaid - distributed;
		}
		for (int i = 0; i < 4; i++) {
			auto avatar = GameRoom::getAvatar(i);
			if (!avatar)
				continue;
			_totalGolds[i] += _roundGolds[i];
			int64_t cashPledge = avatar->getCashPledge() + _roundGolds[i];
			if (cashPledge < 0)
				cashPledge = 0;
			if (updateCashPledge(avatar->getPlayerId(), cashPledge))
				avatar->setCashPledge(cashPledge);
		}

		notifyRoundResult();
		saveRoundRecord();
		checkGameEnd();
	}

	void YuanJiangQianFenRoom::checkGameEnd() {
		bool gameEnd = false;
		// 检查是否有人达到目标分数
		for (int i = 0; i < 4; i++) {
			if (_totalScores[i] >= _targetScore) { gameEnd = true; break; }
		}
		for (int i = 0; i < 4 && !gameEnd; i++) {
			auto avatar = GameRoom::getAvatar(i);
			if (avatar && avatar->getCashPledge() <= 0)
				gameEnd = true;
		}
		// 检查局数限制
		if (_roundLimit > 0 && _roundCount >= _roundLimit) gameEnd = true;

		if (gameEnd) {
			notifyFinalResult();
			publishFinalRoomFee();
			kickAllAvatars();
			gameOver();
			_gameState = GameState::None;
		}
		else {
			_bankerSeat = getNextSeat(_bankerSeat);
			_gameState = GameState::Ready;
		}
	}

	void YuanJiangQianFenRoom::saveRoundRecord() {
		auto task = std::make_shared<YuanJiangQianFenRecordTask>();
		task->_venueId = getId(); task->_roundNo = _roundNo; task->_banker = _bankerSeat;
		_playbackData.randomSeedHash = ReplayUtils::generateSeedHash(getId(), _roundNo, _bankerSeat);
		task->_randomSeedHash = _playbackData.randomSeedHash;
		int idx = 0;
		for (int _si = 0; _si < 4; _si++) {
			auto _av = GameRoom::getAvatar(_si);
			if (idx < 4 && _av) {
				task->_playerIds[idx] = _av->getPlayerId();
				task->_scores[idx] = _roundScores[idx];
				task->_winGolds[idx] = _roundGolds[idx];
			}
			idx++;
		}
		std::string rd;
		msgpack::sbuffer _sbuf;
		msgpack::pack(_sbuf, _playbackData);
		ReplayUtils::compressReplay(_sbuf.data(), static_cast<int>(_sbuf.size()), rd);
		task->_playback = rd;
		MysqlPool::getSingleton().asyncQuery(task);

		// 风控采集：记录得分并结束
		_riskCollector.setRandomSeedHash(_playbackData.randomSeedHash);
		for (int _si = 0; _si < 4; _si++) {
			auto _av = GameRoom::getAvatar(_si);
				if (_av)
					_riskCollector.recordScore(_av->getPlayerId(), _roundScores[_si],
						_roundGolds[_si]);
		}
		_riskCollector.finishRound();

		// 随机审计日志
		std::vector<int> cardOrder;
		std::vector<std::string> playerIds;
		for (int _si = 0; _si < 4; _si++) {
			auto _av = GameRoom::getAvatar(_si);
			if (_av) playerIds.push_back(_av->getPlayerId());
		}
		RandomAuditLogger::logAudit(getId(), static_cast<int>(GameType::YuanJiangQianFen),
			_roundNo, _bankerSeat, _playbackData.randomSeedHash, cardOrder, playerIds);
	}

	bool YuanJiangQianFenRoom::onMessage(const NetMessage::Ptr& netMsg) {
		const std::string& type = netMsg->getType();
		if (type == MsgQianFenSync::TYPE) { onSyncTable(netMsg); return true; }
		if (type == MsgQianFenReady::TYPE) { onReady(netMsg); return true; }
		if (type == MsgQianFenCallScore::TYPE) { onCallScore(netMsg); return true; }
		if (type == MsgQianFenPlay::TYPE) { onPlay(netMsg); return true; }
		return false;
	}

	void YuanJiangQianFenRoom::onSyncTable(const NetMessage::Ptr& netMsg) {
		auto msg = std::dynamic_pointer_cast<MsgQianFenSync>(netMsg);
		if (!msg) return;
		auto session = netMsg->getSession();
		if (!session) return;
		auto resp = std::make_shared<MsgQianFenSyncResp>();
		resp->gameState = static_cast<int>(_gameState);
		resp->roundNo = _roundNo; resp->banker = _bankerSeat; resp->bankerSeat = _bankerSeat;
		resp->playerCount = _playerCount; resp->currentPlayer = _currentPlayer;
		const std::string& playerId = msg->getPlayerId();
		for (int _si = 0; _si < 4; _si++) {
			auto _av = GameRoom::getAvatar(_si);
			if (_av && _av->getPlayerId() == playerId) {
				resp->mySeat = _si;
				auto a = std::dynamic_pointer_cast<PokerAvatar>(_av);
				if (a) { const CardArray& cards = a->getCards(); for (auto& c : cards) resp->myCards.push_back(c.getId()); }
				break;
			}
		}
		resp->send(session);
	}

	void YuanJiangQianFenRoom::onReady(const NetMessage::Ptr& netMsg) {
		if (_gameState != GameState::None && _gameState != GameState::Ready) return;
		auto msg = std::dynamic_pointer_cast<MsgQianFenReady>(netMsg);
		if (!msg) return;
		// 简化：收到第4个准备就开始
		bool allHere = (getAvatarCount() >= 4);
		if (allHere) { setState(GameState::Ready); startRound(); }
	}

	void YuanJiangQianFenRoom::onCallScore(const NetMessage::Ptr& netMsg) {
		if (_gameState != GameState::CallScore) return;
		auto msg = std::dynamic_pointer_cast<MsgQianFenCallScore>(netMsg);
		if (!msg) return;
		const std::string& playerId = msg->getPlayerId();
		for (int _si = 0; _si < 4; _si++) {
			auto _av = GameRoom::getAvatar(_si);
			if (_av && _av->getPlayerId() == playerId) { processCallScore(_si, msg->score); break; }
		}
	}

	void YuanJiangQianFenRoom::onPlay(const NetMessage::Ptr& netMsg) {
		if (_gameState != GameState::Playing) return;
		auto msg = std::dynamic_pointer_cast<MsgQianFenPlay>(netMsg);
		if (!msg) return;
		const std::string& playerId = msg->getPlayerId();
		for (int _si = 0; _si < 4; _si++) {
			auto _av = GameRoom::getAvatar(_si);
			if (_av && _av->getPlayerId() == playerId) { doPlay(_si, msg->cardIds); break; }
		}
	}

	void YuanJiangQianFenRoom::notifyDeal(const std::string& playerId) {
		auto _av = GameRoom::getAvatar(playerId);
		if (!_av) return;
		int seat = -1;
		for (int _si = 0; _si < 4; _si++) {
			auto tmp = GameRoom::getAvatar(_si);
			if (tmp && tmp->getPlayerId() == playerId) { seat = _si; break; }
		}
		if (seat < 0) return;
		auto a = std::dynamic_pointer_cast<PokerAvatar>(_av);
		if (!a) return;
		auto msg = std::make_shared<MsgQianFenDeal>();
		const CardArray& cards = a->getCards();
		for (auto& c : cards) msg->cards.push_back(c.getId());
		msg->roundNo = _roundNo; msg->banker = _bankerSeat;
		sendMessage(*msg, playerId);
	}

	void YuanJiangQianFenRoom::notifyCallScore(int seat, int score, int nextSeat) {
		auto msg = std::make_shared<MsgQianFenCallScoreNotify>();
		msg->seat = seat; msg->score = score; msg->nextSeat = nextSeat; msg->bankerSeat = _bankerSeat;
		sendMessageToAll(*msg);
	}

	void YuanJiangQianFenRoom::notifyPlay(int seat, const std::vector<int>& cardIds) {
		auto msg = std::make_shared<MsgQianFenPlayNotify>();
		msg->seat = seat; msg->cardIds = cardIds; msg->nextPlayer = _currentPlayer;
		sendMessageToAll(*msg);
	}

	void YuanJiangQianFenRoom::notifyRoundResult() {
		auto msg = std::make_shared<MsgQianFenRoundResult>();
		for (int i = 0; i < 4; i++) { msg->scores[i] = _roundScores[i]; msg->winGolds[i] = _roundGolds[i]; }
		sendMessageToAll(*msg);
	}

	void YuanJiangQianFenRoom::notifyFinalResult() {
		auto msg = std::make_shared<MsgQianFenFinalResult>();
		for (int _si = 0; _si < 4; _si++) {
			auto _av = GameRoom::getAvatar(_si);
			if (_av) {
				msg->totalScores[_si] = _totalScores[_si];
					msg->totalGolds[_si] = _totalGolds[_si];
					msg->playerIds[_si] = _av->getPlayerId();
				}
			}
			{
				std::vector<std::pair<std::string, int64_t>> netWins;
				for (int _si = 0; _si < 4; _si++) {
					auto _av = GameRoom::getAvatar(_si);
					if (!_av)
						continue;
					netWins.emplace_back(_av->getPlayerId(), _totalGolds[_si]);
				}
				calcRoomFeeSettlementData(_roomFee, netWins,
					msg->roomFeeTotal, msg->roomFeePlayerIds, msg->roomFeeAmounts);
				getShuffleFeeSettlementData(msg->shuffleFeeTotal,
					msg->shuffleFeePlayerIds, msg->shuffleFeeAmounts);
			}
			sendMessageToAll(*msg);
		// MQ
			for (int _si = 0; _si < 4; _si++) {
				auto _av = GameRoom::getAvatar(_si);
				if (!_av) continue;
				int64_t g = _totalGolds[_si];
				if (g > 0) WalletEventTask::publish(_av->getPlayerId(), "GAME_WIN", g, "YuanJiangQianFen", getId(), "沅江千分赢得金币");
				else if (g < 0) WalletEventTask::publish(_av->getPlayerId(), "GAME_LOSE", -g, "YuanJiangQianFen", getId(), "沅江千分输掉金币");
			}
		}

		void YuanJiangQianFenRoom::publishFinalRoomFee() {
			if (_roundCount <= 0)
				return;
			std::vector<std::pair<std::string, int64_t>> netWins;
			for (int _si = 0; _si < 4; _si++) {
				auto _av = GameRoom::getAvatar(_si);
				if (!_av)
					continue;
				netWins.emplace_back(_av->getPlayerId(), _totalGolds[_si]);
			}
				publishRoomFeeOnGameOver(_roomFee, netWins, "YuanJiangQianFen", "沅江千分整场房费");
			}
		}
