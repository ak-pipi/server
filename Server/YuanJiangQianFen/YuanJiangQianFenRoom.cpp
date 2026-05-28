// YuanJiangQianFenRoom.cpp

#include "YuanJiangQianFenRoom.h"
#include "PokerAvatar.h"
#include "YuanJiangQianFenMessages.h"
#include "Game/ReplayUtils.h"
#include "Game/WalletEventTask.h"
#include "Game/RiskControlCollector.h"
#include "Game/RandomAuditLogger.h"
#include "GameDefines.h"
#include "Network/MsgSession.h"
#include "Base/Log.h"
#include "MySql/MysqlPool.h"

#include <json/json.h>
#include <algorithm>

namespace NiuMa
{
	YuanJiangQianFenRoom::YuanJiangQianFenRoom(const std::string& venueId, const std::string& number, int level, const std::string& ruleConfig)
		: GameRoom(venueId, static_cast<int>(GameType::YuanJiangQianFen), 4)
		, _number(number), _level(level), _gameState(GameState::None)
		, _roundNo(0), _bankerSeat(0), _currentPlayer(0)
		, _callScoreCurrent(0), _highestCallScore(0), _highestCallSeat(-1), _callScoreCount(0)
		, _lastPlaySeat(-1), _isFirstPlay(true), _roundCount(0)
		, _rule(std::make_shared<PokerRule>())
		, _playerCount(4), _targetScore(1000), _callScoreEnabled(true), _bankerRule(0)
		, _deckCount(2), _bombEnabled(false), _roundLimit(0), _maxScore(500)
	{
		_rule->initialise();
		_dealer = PokerDealer(_rule);
		for (int i = 0; i < 4; i++) { _totalScores[i] = 0; _roundScores[i] = 0; }
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
		if (root.isMember("round_limit")) _roundLimit = root["round_limit"].asInt();
		if (root.isMember("max_score")) _maxScore = root["max_score"].asInt();
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
		return std::make_shared<PokerAvatar>(_rule, playerId, seat, robot);
	}

	bool YuanJiangQianFenRoom::checkEnter(const std::string& playerId, std::string& errMsg, bool robot) const {
		if (_avatars.size() >= 4) { errMsg = "房间已满"; return false; }
		return true;
	}

	int YuanJiangQianFenRoom::checkLeave(const std::string& playerId, std::string& errMsg) {
		if (_gameState == GameState::Playing || _gameState == GameState::CallScore) { errMsg = "游戏进行中"; return 2; }
		return 0;
	}

	void YuanJiangQianFenRoom::onAvatarLeaved(int seat, const std::string& playerId) {
		if (_avatars.empty()) _gameState = GameState::None;
	}

	void YuanJiangQianFenRoom::clean() {
		_gameState = GameState::None;
		_roundNo = 0; _roundCount = 0;
		for (int i = 0; i < 4; i++) { _totalScores[i] = 0; _roundScores[i] = 0; }
		_playbackData = QianFenPlaybackData();
	}

	void YuanJiangQianFenRoom::onTimer() {}
	void YuanJiangQianFenRoom::setState(GameState s) { _gameState = s; }

	bool YuanJiangQianFenRoom::allReady() const {
		if (_avatars.size() < 4) return false;
		for (auto& kv : _avatars) {
			auto a = std::dynamic_pointer_cast<PokerAvatar>(kv.second);
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
		for (int i = 0; i < 4; i++) _roundScores[i] = 0;

		_playbackData = QianFenPlaybackData();
		_playbackData.venueId = _venueId;
		_playbackData.roundNo = _roundNo;
		_playbackData.banker = _bankerSeat;
		_playbackData.playerCount = _playerCount;

		// 风控采集：开始新局
		_riskCollector.startRound(_venueId, static_cast<int>(GameType::YuanJiangQianFen), _roundNo);
		for (auto& kv : _avatars) {
			_riskCollector.recordPlayer(kv.second->getPlayerId(), kv.first);
		}

		_dealer.shuffle();
		dealCards();

		if (_callScoreEnabled)
			beginCallScore();
		else
			beginPlay();

		LOG_INFO("沅江千分游戏开始，场地: " << _venueId << "，局号: " << _roundNo);
	}

	void YuanJiangQianFenRoom::dealCards() {
		std::vector<CardArray> heaps;
		_dealer.shuffle();
		_dealer.handOutCards(heaps, 4);

		int idx = 0;
		for (auto& kv : _avatars) {
			auto a = std::dynamic_pointer_cast<PokerAvatar>(kv.second);
			if (a && idx < static_cast<int>(heaps.size())) {
				a->setCards(heaps[idx]);
				a->sortCards();
				std::vector<int> ids;
				for (auto& c : heaps[idx]) ids.push_back(c.getId());
				_playbackData.playerIds[idx] = kv.second->getPlayerId();
				_playbackData.initCards[idx] = ids;
			}
			idx++;
		}
		for (auto& kv : _avatars) notifyDeal(kv.second->getPlayerId());
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
		auto a = std::dynamic_pointer_cast<PokerAvatar>(_avatars[seat]);
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
		for (auto& kv : _avatars) {
			auto a = std::dynamic_pointer_cast<PokerAvatar>(kv.second);
			if (!a) continue;
			int score = calcScoreCards(a->getCards());
			_roundScores[kv.first] = -score; // 剩余牌的分数为负
			_totalScores[kv.first] += _roundScores[kv.first];
		}

		// 赢家（出完牌的人）
		int winnerSeat = -1;
		for (auto& kv : _avatars) {
			auto a = std::dynamic_pointer_cast<PokerAvatar>(kv.second);
			if (a && a->getCardNums() == 0) { winnerSeat = kv.first; break; }
		}
		if (winnerSeat >= 0) {
			int winnerScore = 0;
			for (int i = 0; i < 4; i++) winnerScore -= _roundScores[i]; // 赢家得其他人负分的绝对值之和
			_roundScores[winnerSeat] = winnerScore;
			_totalScores[winnerSeat] += winnerScore;
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
		// 检查局数限制
		if (_roundLimit > 0 && _roundCount >= _roundLimit) gameEnd = true;

		if (gameEnd) {
			notifyFinalResult();
			_gameState = GameState::None;
		}
		else {
			_bankerSeat = getNextSeat(_bankerSeat);
			_gameState = GameState::Ready;
		}
	}

	void YuanJiangQianFenRoom::saveRoundRecord() {
		auto task = std::make_shared<YuanJiangQianFenRecordTask>();
		task->_venueId = _venueId; task->_roundNo = _roundNo; task->_banker = _bankerSeat;
		_playbackData.randomSeedHash = ReplayUtils::generateSeedHash(_venueId, _roundNo, _bankerSeat);
		task->_randomSeedHash = _playbackData.randomSeedHash;
		int idx = 0;
		for (auto& kv : _avatars) {
			if (idx < 4) {
				task->_playerIds[idx] = kv.second->getPlayerId();
				task->_scores[idx] = _roundScores[idx];
				task->_winGolds[idx] = _roundScores[idx] * _level;
			}
			idx++;
		}
		std::string rd;
		ReplayUtils::compressReplay(_playbackData, rd);
		task->_playback = rd;
		MysqlPool::getSingleton().asyncQuery(task);

		// 风控采集：记录得分并结束
		_riskCollector.setRandomSeedHash(_playbackData.randomSeedHash);
		for (auto& kv : _avatars) {
			_riskCollector.recordScore(kv.second->getPlayerId(), _roundScores[kv.first],
				static_cast<int64_t>(_roundScores[kv.first]) * _level);
		}
		_riskCollector.finishRound();

		// 随机审计日志
		std::vector<int> cardOrder;
		std::vector<std::string> playerIds;
		for (auto& kv : _avatars) {
			playerIds.push_back(kv.second->getPlayerId());
		}
		RandomAuditLogger::logAudit(_venueId, static_cast<int>(GameType::YuanJiangQianFen),
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
		auto session = msg->getSession();
		if (!session) return;
		auto resp = std::make_shared<MsgQianFenSyncResp>();
		resp->gameState = static_cast<int>(_gameState);
		resp->roundNo = _roundNo; resp->banker = _bankerSeat; resp->bankerSeat = _bankerSeat;
		resp->playerCount = _playerCount; resp->currentPlayer = _currentPlayer;
		for (auto& kv : _avatars) {
			if (kv.second->getPlayerId() == msg->playerId) {
				resp->mySeat = kv.first;
				auto a = std::dynamic_pointer_cast<PokerAvatar>(kv.second);
				if (a) { const CardArray& cards = a->getCards(); for (auto& c : cards) resp->myCards.push_back(c.getId()); }
				break;
			}
		}
		session->sendMsg(resp);
	}

	void YuanJiangQianFenRoom::onReady(const NetMessage::Ptr& netMsg) {
		if (_gameState != GameState::None && _gameState != GameState::Ready) return;
		auto msg = std::dynamic_pointer_cast<MsgQianFenReady>(netMsg);
		if (!msg) return;
		// 简化：收到第4个准备就开始
		bool allHere = (_avatars.size() >= 4);
		if (allHere) { setState(GameState::Ready); startRound(); }
	}

	void YuanJiangQianFenRoom::onCallScore(const NetMessage::Ptr& netMsg) {
		if (_gameState != GameState::CallScore) return;
		auto msg = std::dynamic_pointer_cast<MsgQianFenCallScore>(netMsg);
		if (!msg) return;
		for (auto& kv : _avatars) {
			if (kv.second->getPlayerId() == msg->playerId) { processCallScore(kv.first, msg->score); break; }
		}
	}

	void YuanJiangQianFenRoom::onPlay(const NetMessage::Ptr& netMsg) {
		if (_gameState != GameState::Playing) return;
		auto msg = std::dynamic_pointer_cast<MsgQianFenPlay>(netMsg);
		if (!msg) return;
		for (auto& kv : _avatars) {
			if (kv.second->getPlayerId() == msg->playerId) { doPlay(kv.first, msg->cardIds); break; }
		}
	}

	void YuanJiangQianFenRoom::notifyDeal(const std::string& playerId) {
		for (auto& kv : _avatars) {
			if (kv.second->getPlayerId() == playerId) {
				auto a = std::dynamic_pointer_cast<PokerAvatar>(kv.second);
				if (!a) return;
				auto msg = std::make_shared<MsgQianFenDeal>();
				const CardArray& cards = a->getCards();
				for (auto& c : cards) msg->cards.push_back(c.getId());
				msg->roundNo = _roundNo; msg->banker = _bankerSeat;
				sendToPlayer(playerId, msg);
				return;
			}
		}
	}

	void YuanJiangQianFenRoom::notifyCallScore(int seat, int score, int nextSeat) {
		auto msg = std::make_shared<MsgQianFenCallScoreNotify>();
		msg->seat = seat; msg->score = score; msg->nextSeat = nextSeat; msg->bankerSeat = _bankerSeat;
		sendToAll(msg);
	}

	void YuanJiangQianFenRoom::notifyPlay(int seat, const std::vector<int>& cardIds) {
		auto msg = std::make_shared<MsgQianFenPlayNotify>();
		msg->seat = seat; msg->cardIds = cardIds; msg->nextPlayer = _currentPlayer;
		sendToAll(msg);
	}

	void YuanJiangQianFenRoom::notifyRoundResult() {
		auto msg = std::make_shared<MsgQianFenRoundResult>();
		for (int i = 0; i < 4; i++) { msg->scores[i] = _roundScores[i]; msg->winGolds[i] = _roundScores[i] * _level; }
		sendToAll(msg);
	}

	void YuanJiangQianFenRoom::notifyFinalResult() {
		auto msg = std::make_shared<MsgQianFenFinalResult>();
		for (auto& kv : _avatars) {
			int s = kv.first;
			if (s < 4) { msg->totalScores[s] = _totalScores[s]; msg->totalGolds[s] = _totalScores[s] * _level; msg->playerIds[s] = kv.second->getPlayerId(); }
		}
		sendToAll(msg);
		// MQ
		for (auto& kv : _avatars) {
			int64_t g = _totalScores[kv.first] * _level;
			if (g > 0) WalletEventTask::publish(kv.second->getPlayerId(), "GAME_WIN", g, "YuanJiangQianFen", _venueId, "沅江千分赢得金币");
			else if (g < 0) WalletEventTask::publish(kv.second->getPlayerId(), "GAME_LOSE", -g, "YuanJiangQianFen", _venueId, "沅江千分输掉金币");
		}
	}
}
