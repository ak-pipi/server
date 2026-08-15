// PaoDeKuaiRoom.cpp

#include "PaoDeKuaiRoom.h"
#include "PaoDeKuaiAvatar.h"
#include "GameDefines.h"
#include "Game/GameMessages.h"
#include "PaoDeKuaiMessages.h"
#include "Game/ReplayUtils.h"
#include "Game/GetCapitalTask.h"
#include "Game/DebtLiquidation.h"
#include "PaoDeKuaiRecordTask.h"
#include "Game/RiskControlCollector.h"
#include "Game/RandomAuditLogger.h"
#include "Network/MsgSession.h"
#include "Base/Log.h"
#include "Base/BaseUtils.h"
#include "Constant/RedisKeys.h"
#include "Redis/RedisPool.h"
#include "MySql/MysqlPool.h"

#include <json/json.h>
#include <algorithm>
#include <climits>
#include <chrono>
#include <cmath>
#include <sstream>

namespace NiuMa
{
	namespace
	{
			bool isPaoDeKuaiBombGenre(int genre)
			{
				return genre == static_cast<int>(PaoDeKuaiGenre::Bomb) ||
					genre == static_cast<int>(PaoDeKuaiGenre::Rocket);
			}

			bool isHeartTen(const PokerCard& c)
			{
				return c.getPoint() == static_cast<int>(PokerPoint::Ten) &&
					c.getSuit() == static_cast<int>(PokerSuit::Heart);
			}

			bool isZhaNiaoDistrict(int districtId)
			{
				return districtId == 28 || districtId == 52;
			}

			int64_t resolvePaoDeKuaiMinCarryScore(int baseScore, int roundCount, bool zhaNiao)
			{
				if (roundCount == 1) {
					if (baseScore == 50)
						return 300;
					if (baseScore == 100)
						return 600;
				}
				else if (roundCount == 8) {
					if (baseScore == 3)
						return 30;
					if (baseScore == 5)
						return 50;
					if (baseScore == 10)
						return zhaNiao ? 200 : 100;
					if (baseScore == 20)
						return 400;
				}
				return static_cast<int64_t>(baseScore) * 8;
			}

			int64_t readRoomFeeAmount(const std::string& ruleConfig)
			{
				if (ruleConfig.empty())
					return 0;
				Json::Value root;
				Json::CharReaderBuilder builder;
				Json::CharReader* reader = builder.newCharReader();
				std::string errs;
				if (!reader->parse(ruleConfig.c_str(), ruleConfig.c_str() + ruleConfig.size(), &root, &errs)) {
					delete reader;
					return 0;
				}
				delete reader;
				if (root.isMember("room_fee") && root["room_fee"].isInt64())
					return root["room_fee"].asInt64();
				if (root.isMember("room_fee") && root["room_fee"].isInt())
					return root["room_fee"].asInt();
				if (root.isMember("room_fee_type") && root["room_fee_type"].isInt())
					return root["room_fee_type"].asInt();
				return 0;
			}

			int64_t readMinCarryScore(const std::string& ruleConfig, int64_t fallback)
			{
				if (ruleConfig.empty())
					return fallback;
				Json::Value root;
				Json::CharReaderBuilder builder;
				Json::CharReader* reader = builder.newCharReader();
				std::string errs;
				if (!reader->parse(ruleConfig.c_str(), ruleConfig.c_str() + ruleConfig.size(), &root, &errs)) {
					delete reader;
					return fallback;
				}
				delete reader;
				const char* keys[] = {"min_carry_score", "minCarryScore"};
				for (const char* key : keys) {
					if (!root.isMember(key))
						continue;
					const Json::Value& value = root[key];
					int64_t score = 0;
					if (value.isInt64() || value.isInt())
						score = value.asInt64();
					else if (value.isUInt64() || value.isUInt())
						score = static_cast<int64_t>(value.asUInt64());
					if (score > 0)
						return score;
				}
				return fallback;
			}
		}

	PaoDeKuaiRoom::PaoDeKuaiRoom(const std::shared_ptr<PaoDeKuaiRule>& rule,
		const std::string& venueId,
		const std::string& number,
		int level,
		const std::string& ruleConfig,
		int districtId)
		: GameRoom(venueId, static_cast<int>(GameType::PaoDeKuai), 2)
		, _rule(rule)
		, _dealer(rule)
		, _ruleConfig(ruleConfig)
		, _number(number)
		, _level(level)
		, _districtId(districtId)
		, _gameState(GameState::None)
		, _stateTime(0)
		, _roundNo(0)
		, _banker(0)
		, _currentPlayer(0)
		, _lastPlaySeat(-1)
		, _isFirstPlay(true)
		, _hasFirstPlayed(false)
			, _bombCount(0)
			, _multiplier(1)
			, _birdMultiplier(1)
			, _birdHit(false)
			, _birdSeat(-1)
			, _pendingBombSeat(-1)
				, _spring(false)
			, _roomFee(0)
			, _autoPlayTime(0)
			, _dissolveRequested(false)
			, _dissolveRequester(-1)
			, _dissolveTick(0)
			, _roomScoreboardRecorded(false)
	{
		_dissolveVotes[0] = 0;
		_dissolveVotes[1] = 0;
		for (int i = 0; i < 2; i++) {
			_bombScoreDeltas[i] = 0;
			_bombWinCounts[i] = 0;
		}

			// 加载规则配置
			_rule->loadConfig(ruleConfig);
			_roomFee = readRoomFeeAmount(ruleConfig);
			_playback.scoreScale = _rule->getScoreScale();
			int64_t minCarryScore = resolvePaoDeKuaiMinCarryScore(
				_rule->getBaseScore(), _rule->getRoundCount(), isZhaNiaoEnabled());
			setCashPledge(readMinCarryScore(ruleConfig, minCarryScore));
		}

	PaoDeKuaiRoom::~PaoDeKuaiRoom() {}

	GameAvatar::Ptr PaoDeKuaiRoom::createAvatar(const std::string& playerId, int seat, bool robot) const {
		return std::make_shared<PaoDeKuaiAvatar>(_rule, playerId, seat, robot);
	}

	bool PaoDeKuaiRoom::checkEnter(const std::string& playerId, std::string& errMsg, bool robot) const {
		(void)playerId;
		(void)robot;
		if (getAvatarCount() >= _rule->getPlayerCount()) {
			errMsg = "房间已满";
			return false;
		}
		return true;
	}

	int PaoDeKuaiRoom::checkLeave(const std::string& playerId, std::string& errMsg) const {
		(void)playerId;
		if (_gameState == GameState::Playing) {
			errMsg = "游戏进行中，不能直接离开房间";
			return 1;
		}
		return 0;
	}

	void PaoDeKuaiRoom::getAvatarExtraInfo(const GameAvatar::Ptr& avatar, std::string& base64) const {
		std::shared_ptr<GetCapitalTask> task = std::make_shared<GetCapitalTask>(avatar->getPlayerId());
		MysqlPool::getSingleton().syncQuery(task);
			double gold = avatar->getCashPledge();
			int64_t diamond = 0LL;
		if (task->getSucceed() && task->getRows() > 0) {
			diamond = task->getDiamond();
		}
		Json::Value tmp(Json::objectValue);
			tmp["gold"] = gold;
		tmp["diamond"] = static_cast<Json::Int64>(diamond);
		if (!avatar->isOffline()) {
			Session::Ptr session = avatar->getSession();
			if (session)
				tmp["ip"] = session->getRemoteIp();
		}
		std::string json = tmp.toStyledString();
		BaseUtils::encodeBase64(base64, json.data(), static_cast<int>(json.size()));
	}

	void PaoDeKuaiRoom::onAvatarJoined(int seat, const std::string& playerId) {
		GameRoom::onAvatarJoined(seat, playerId);
		updateDistrictNotFull();
	}

	void PaoDeKuaiRoom::onAvatarLeaved(int seat, const std::string& playerId) {
		(void)seat;
		if (_gameState == GameState::Ready || _gameState == GameState::None) {
			// 重置状态
			if (getAvatarCount() == 0)
				setState(GameState::None);
		}
		updateDistrictNotFull();
		recordDistrictPlayerTrack(playerId);
	}

	void PaoDeKuaiRoom::clean() {
		GameRoom::clean();
		_gameState = GameState::None;
		_roundNo = 0;
		_currentPlayer = 0;
		_lastPlaySeat = -1;
		_isFirstPlay = true;
		_hasFirstPlayed = false;
		_bombCount = 0;
		_pendingBombSeat = -1;
		_multiplier = 1;
		_spring = false;
		_dissolveRequested = false;
		_dissolveRequester = -1;
		_dissolveTick = 0;
		_roomScoreboardRecorded = false;
		_dissolveVotes[0] = 0;
		_dissolveVotes[1] = 0;
		_playback = PaoDeKuaiPlaybackData();
		for (int _si = 0; _si < 2; _si++) {
			auto avatar = std::dynamic_pointer_cast<PaoDeKuaiAvatar>(GameRoom::getAvatar(_si));
			if (!avatar)
				continue;
			avatar->clear();
		}
	}

	bool PaoDeKuaiRoom::canShuffleCardsBeforeNextRound(const std::string& playerId,
		int& nextRoundNo,
		int& roundCount,
		std::string& errMsg) const {
		nextRoundNo = _roundNo + 1;
		roundCount = _rule ? _rule->getRoundCount() : 0;
		if (!hasAvatar(playerId)) {
			errMsg = "你不在当前房间内";
			return false;
		}
		if (_roundNo <= 0) {
			errMsg = "首局开始前不能洗牌";
			return false;
		}
		if (roundCount > 0 && _roundNo >= roundCount) {
			errMsg = "全部对局已结束，不能洗牌";
			return false;
		}
		if (_gameState != GameState::None && _gameState != GameState::Ready) {
			errMsg = "下局开始前才能洗牌";
			return false;
		}
		return true;
	}

	void PaoDeKuaiRoom::onTimer() {
		time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
		if (_dissolveRequested && _dissolveTick > 0 && now - _dissolveTick >= 300) {
			for (int _si = 0; _si < 2; _si++) {
				if (_dissolveVotes[_si] == 0)
					doDisbandChoose(_si, 1);
			}
		}
		if (_gameState == GameState::Playing) {
			// 检查托管超时
			if (_autoPlayTime > 0 && now >= _autoPlayTime) {
				autoPlay();
				_autoPlayTime = 0;
			}
		}
	}

	bool PaoDeKuaiRoom::onMessage(const NetMessage::Ptr& netMsg) {
		if (GameRoom::onMessage(netMsg))
			return true;
		const std::string& type = netMsg->getType();
		if (type == MsgPaoDeKuaiSync::TYPE) {
			onSyncTable(netMsg);
			return true;
		}
		else if (type == MsgPaoDeKuaiReady::TYPE) {
			onReady(netMsg);
			return true;
		}
		else if (type == MsgPaoDeKuaiPlay::TYPE) {
			onPlay(netMsg);
			return true;
		}
		else if (type == MsgDisbandRequest::TYPE) {
			onDisbandRequest(netMsg);
			return true;
		}
		else if (type == MsgDisbandChoose::TYPE) {
			onDisbandChoose(netMsg);
			return true;
		}
		return false;
	}

	std::shared_ptr<PaoDeKuaiAvatar> PaoDeKuaiRoom::getAvatar(int seat) const {
		return std::dynamic_pointer_cast<PaoDeKuaiAvatar>(GameRoom::getAvatar(seat));
	}

	void PaoDeKuaiRoom::setState(GameState s) {
		_gameState = s;
		_stateTime = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
	}

	bool PaoDeKuaiRoom::allReady() const {
		if (getAvatarCount() < _rule->getPlayerCount())
			return false;
		for (int _si = 0; _si < 2; _si++) {
			auto avatar = std::dynamic_pointer_cast<PaoDeKuaiAvatar>(GameRoom::getAvatar(_si));
			if (!avatar)
				continue;
			if (!avatar->isReady())
				return false;
		}
		return true;
	}

	int PaoDeKuaiRoom::getNextSeat(int seat) const {
		return (seat + 1) % _rule->getPlayerCount();
	}

	int PaoDeKuaiRoom::determineFirstPlayer() const {
		std::vector<int> seats;
		for (int _si = 0; _si < 2; _si++) {
			auto avatar = std::dynamic_pointer_cast<PaoDeKuaiAvatar>(GameRoom::getAvatar(_si));
			if (avatar)
				seats.push_back(_si);
		}
		if (seats.empty())
			return 0;
		if (_roundNo <= 1)
			return seats[BaseUtils::randInt(0, static_cast<int>(seats.size()))];
		for (int seat : seats) {
			if (seat == _banker)
				return seat;
		}
		return seats[0];
	}

	void PaoDeKuaiRoom::startRound() {
		if (_rule->getRoundCount() > 0 && _roundNo >= _rule->getRoundCount())
			return;
		_roundNo++;
		_bombCount = 0;
		_multiplier = 1;
		_birdMultiplier = 1;
		_birdHit = false;
		_birdSeat = -1;
		_pendingBombSeat = -1;
			for (int i = 0; i < 2; i++) {
			_bombScoreDeltas[i] = 0;
			_bombWinCounts[i] = 0;
		}
		_spring = false;
		_hasFirstPlayed = false;
		_playback = PaoDeKuaiPlaybackData();
		_playback.venueId = getId();
		_playback.roundNo = _roundNo;
		_playback.playerCount = _rule->getPlayerCount();
		int idx = 0;
		for (int _si = 0; _si < 2; _si++) {
			auto avatar = std::dynamic_pointer_cast<PaoDeKuaiAvatar>(GameRoom::getAvatar(_si));
			if (!avatar)
				continue;
			_playback.playerIds[idx] = avatar->getPlayerId();
			idx++;
		}

		// 风控采集：开始新局
		_riskCollector.startRound(getId(), static_cast<int>(GameType::PaoDeKuai), _roundNo);
		for (int _si = 0; _si < 2; _si++) {
			auto avatar = std::dynamic_pointer_cast<PaoDeKuaiAvatar>(GameRoom::getAvatar(_si));
			if (!avatar)
				continue;
			_riskCollector.recordPlayer(avatar->getPlayerId(), _si);
		}

		// 清理玩家状态
		for (int _si = 0; _si < 2; _si++) {
			auto avatar = std::dynamic_pointer_cast<PaoDeKuaiAvatar>(GameRoom::getAvatar(_si));
			if (!avatar)
				continue;
			avatar->clear();
			avatar->setRoundScore(0);
			avatar->setWinGold(0);
		}

		// 洗牌发牌
		_dealer.shuffle();
		dealCards();

		// 确定首出玩家
		_currentPlayer = determineFirstPlayer();
		_banker = _currentPlayer;
		_playback.banker = _banker;
		_lastPlaySeat = -1;
		_isFirstPlay = true;
		_lastPlayGenre.clear();

		setState(GameState::Playing);

		// 通知所有玩家
		for (int _si = 0; _si < 2; _si++) {
			auto avatar = std::dynamic_pointer_cast<PaoDeKuaiAvatar>(GameRoom::getAvatar(_si));
			if (!avatar)
				continue;
			notifyDeal(avatar->getPlayerId());
		}

		// 设置托管超时
		_autoPlayTime = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()) +
			_rule->getAutoPlayTimeout() / 1000;

		{ std::ostringstream oss; oss << "跑得快游戏开始，场地: " << getId() << "，局号: " << _roundNo; LOG_INFO(oss.str()); }
	}

	void PaoDeKuaiRoom::dealCards() {
		int cardCount = _rule->getCardCount();
		// 15张跑得快使用45张牌库，两人各发15张，剩余15张不参与本局。
		int idx = 0;
		for (int _si = 0; _si < 2; _si++) {
			auto avatar = std::dynamic_pointer_cast<PaoDeKuaiAvatar>(GameRoom::getAvatar(_si));
			if (!avatar)
				continue;
			CardArray hand;
			CardArray drawn;
			if (!_dealer.handOutCards(drawn, cardCount)) {
				ErrorS << "跑得快发牌失败，场地Id: " << getId() << ", 座位: " << _si;
				continue;
			}
			hand.insert(hand.end(), drawn.begin(), drawn.end());
			avatar->setCards(hand);
			avatar->sortCards();

			// 记录初始手牌
			std::vector<int> ids;
			for (auto& c : hand)
				ids.push_back(c.getId());
			_playback.initCards[idx] = ids;
			if (isZhaNiaoEnabled()) {
				for (const PokerCard& c : hand) {
					if (isHeartTen(c)) {
						_birdHit = true;
						_birdSeat = _si;
						_birdMultiplier = 2;
						break;
					}
				}
			}
			idx++;
		}
	}

	bool PaoDeKuaiRoom::isZhaNiaoEnabled() const {
		return (_rule && _rule->isZhaNiaoEnabled()) || isZhaNiaoDistrict(_districtId);
	}

	void PaoDeKuaiRoom::finalizePendingBomb() {
		if (_pendingBombSeat < 0 || _pendingBombSeat >= 2)
			return;
		int baseScore = _rule ? _rule->getBaseScore() : 1;
		int bombScore = baseScore * 10;
		int opponent = getNextSeat(_pendingBombSeat);
		_bombScoreDeltas[_pendingBombSeat] += bombScore;
		_bombScoreDeltas[opponent] -= bombScore;
		_bombWinCounts[_pendingBombSeat]++;
		_pendingBombSeat = -1;
	}

	void PaoDeKuaiRoom::nextPlayer() {
		_currentPlayer = getNextSeat(_currentPlayer);

		// 如果轮回到上一次出牌的人，说明其他人都过牌了
		if (_currentPlayer == _lastPlaySeat) {
			finalizePendingBomb();
			_isFirstPlay = true;
			_lastPlayGenre.clear();
			_lastPlaySeat = -1;
		}

		// 设置托管超时
		_autoPlayTime = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()) +
			_rule->getAutoPlayTimeout() / 1000;
	}

	PaoDeKuaiRoom::PlayResult PaoDeKuaiRoom::validatePlay(int seat, const std::vector<int>& cardIds, PokerGenre& genre) const {
		// 检查是否轮到该玩家
		if (seat != _currentPlayer)
			return PlayResult::NotYourTurn;

		// 过牌（空牌）
		if (cardIds.empty()) {
			if (_isFirstPlay)
				return PlayResult::InvalidCards; // 首出不能过牌
			if (!_rule->getAllowPass())
				return PlayResult::InvalidCards;
			if (_rule->getForcePlayIfCanBeat() && hasBeatingPlay(seat))
				return PlayResult::CannotPass;
			return PlayResult::OK;
		}

		auto avatar = getAvatar(seat);
		if (!avatar)
			return PlayResult::InvalidCards;

		// 检查牌是否属于玩家手牌
		CardArray cards;
		if (!avatar->getCardsByIds(cardIds, cards))
			return PlayResult::InvalidCards;

		// 排序
		CardComparator comp(_rule);
		std::sort(cards.begin(), cards.end(), comp);

		// 判定牌型
		genre.setCards(cards, _rule);
		int g = _rule->predicateCardGenre(genre);
		if (g == static_cast<int>(PaoDeKuaiGenre::Invalid))
			return PlayResult::InvalidGenre;

		// 压牌检查
		if (!_isFirstPlay) {
			int cmp = _rule->compareGenre(genre, _lastPlayGenre);
			if (cmp != 1) // 不是大于
				return PlayResult::CannotBeat;
		}

		return PlayResult::OK;
	}

	bool PaoDeKuaiRoom::hasBeatingPlay(int seat) const {
		std::vector<int> ids;
		return findAutoPlay(seat, false, ids);
	}

	bool PaoDeKuaiRoom::betterAutoPlay(const PokerGenre& candidate, const PokerGenre& current, bool firstPlay) const {
		if (current.getGenre() <= 0)
			return true;
		if (firstPlay) {
			if (candidate.getCardNums() != current.getCardNums())
				return candidate.getCardNums() < current.getCardNums();
			int cmp = _rule->compareGenre(candidate, current);
			if (cmp == 2)
				return true;
			if (cmp == 0 && candidate.getGenre() != current.getGenre())
				return candidate.getGenre() < current.getGenre();
			return false;
		}

		bool candidateBomb = isPaoDeKuaiBombGenre(candidate.getGenre());
		bool currentBomb = isPaoDeKuaiBombGenre(current.getGenre());
		bool targetBomb = isPaoDeKuaiBombGenre(_lastPlayGenre.getGenre());
		if (!targetBomb && candidateBomb != currentBomb)
			return !candidateBomb;
		if (candidate.getGenre() != current.getGenre())
			return candidate.getGenre() < current.getGenre();
		int cmp = _rule->compareGenre(candidate, current);
		return cmp == 2;
	}

	bool PaoDeKuaiRoom::findAutoPlay(int seat, bool firstPlay, std::vector<int>& cardIds) const {
		auto avatar = getAvatar(seat);
		if (!avatar)
			return false;
		const CardArray& hand = avatar->getCards();
		int n = static_cast<int>(hand.size());
		if (n <= 0 || n > 20)
			return false;

		PokerGenre bestGenre;
		std::vector<int> bestIds;
		int total = 1 << n;
		CardComparator comp(_rule);
		for (int mask = 1; mask < total; mask++) {
			CardArray cards;
			std::vector<int> ids;
			for (int i = 0; i < n; i++) {
				if ((mask & (1 << i)) == 0)
					continue;
				const PokerCard& c = hand[i];
				cards.push_back(c);
				ids.push_back(c.getId());
			}
			std::sort(cards.begin(), cards.end(), comp);
			PokerGenre genre;
			genre.setCards(cards, _rule);
			if (genre.getGenre() <= static_cast<int>(PaoDeKuaiGenre::Invalid))
				continue;
			if (!firstPlay) {
				int cmp = _rule->compareGenre(genre, _lastPlayGenre);
				if (cmp != 1)
					continue;
			}
			if (betterAutoPlay(genre, bestGenre, firstPlay)) {
				bestGenre = genre;
				bestIds = ids;
			}
		}

		if (bestIds.empty())
			return false;
		cardIds = bestIds;
		return true;
	}

	PaoDeKuaiRoom::PlayResult PaoDeKuaiRoom::doPlay(int seat, const std::vector<int>& cardIds) {
		PokerGenre genre;
		PlayResult result = validatePlay(seat, cardIds, genre);
		if (result != PlayResult::OK)
			return result;

		auto avatar = getAvatar(seat);
		if (!avatar)
			return PlayResult::InvalidCards;

		if (cardIds.empty()) {
			// 过牌
			PaoDeKuaiStep step;
			step.action = 1;
			step.seat = seat;
			step.timestamp = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
			_playback.steps.push_back(step);

			nextPlayer();
			// 通知。过牌后同步下一位出牌玩家。
			notifyPlay(seat, cardIds, 0, _currentPlayer);
			return PlayResult::OK;
		}

		// 出牌
		avatar->removeCardsByIds(cardIds);
		avatar->setOuttedGenre(genre);
		avatar->analyzeCombinations();

		// 记录
		_hasFirstPlayed = true;
		_lastPlaySeat = seat;
		_lastPlayGenre = genre;
		_isFirstPlay = false;

		// 炸弹计数
		if (isPaoDeKuaiBombGenre(genre.getGenre())) {
			_bombCount++;
			_pendingBombSeat = seat;
		}

		// 记录回放步骤
		PaoDeKuaiStep step;
		step.action = 0;
		step.seat = seat;
		step.cardIds = cardIds;
		step.timestamp = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
		_playback.steps.push_back(step);

		// 通知
		notifyPlay(seat, cardIds, genre.getGenre(), getNextSeat(seat));

		// 检查是否出完牌
		if (avatar->isFinished()) {
			// 该玩家赢了
			settle();
			return PlayResult::OK;
		}

		nextPlayer();
		return PlayResult::OK;
	}

	void PaoDeKuaiRoom::settle() {
		// 找出赢家
		int winnerSeat = -1;
		for (int _si = 0; _si < 2; _si++) {
			auto avatar = std::dynamic_pointer_cast<PaoDeKuaiAvatar>(GameRoom::getAvatar(_si));
			if (!avatar)
				continue;
			if (avatar->isFinished()) {
				winnerSeat = _si;
				break;
			}
		}
		if (winnerSeat < 0)
			return;

		calculateScores(winnerSeat);

		bool finishRoom = (_rule->getRoundCount() > 0 && _roundNo >= _rule->getRoundCount());
			int scoreScale = _rule ? _rule->getScoreScale() : 1;
			if (scoreScale <= 0)
				scoreScale = 1;
			for (int _si = 0; _si < 2; _si++) {
				auto avatar = std::dynamic_pointer_cast<PaoDeKuaiAvatar>(GameRoom::getAvatar(_si));
				if (!avatar)
					continue;
				double cashPledge = avatar->getCashPledge() +
					static_cast<double>(avatar->getWinGold()) / static_cast<double>(scoreScale);
				if (cashPledge < 0.0)
					cashPledge = 0.0;
				if (updateCashPledge(avatar->getPlayerId(), cashPledge))
					avatar->setCashPledge(cashPledge);
				else
					finishRoom = true;
				if (avatar->getCashPledge() <= 0.0)
					finishRoom = true;
			}

			// 通知结算
			notifySettlement(winnerSeat, finishRoom);

		// 保存回放记录
		saveRoundRecord();

		// 重置准备状态
		for (int _si = 0; _si < 2; _si++) {
			auto avatar = std::dynamic_pointer_cast<PaoDeKuaiAvatar>(GameRoom::getAvatar(_si));
			if (!avatar)
				continue;
			avatar->setReady(false);
		}

		// 更新庄家（赢家做庄）
		_banker = winnerSeat;

		if (finishRoom) {
			recordFinalRoomScoreboard();
			publishFinalRoomFee();
			kickAllAvatars();
			gameOver();
			return;
		}

		setState(GameState::Ready);
	}

	void PaoDeKuaiRoom::publishFinalRoomFee() {
		if (_roundNo <= 0)
			return;
		std::vector<std::pair<std::string, int64_t>> netWins;
		for (int _si = 0; _si < 2; _si++) {
			auto avatar = std::dynamic_pointer_cast<PaoDeKuaiAvatar>(GameRoom::getAvatar(_si));
			if (!avatar)
				continue;
			netWins.emplace_back(avatar->getPlayerId(), avatar->getTotalScore());
		}
		publishRoomFeeOnGameOver(_roomFee, netWins, "PaoDeKuai", "跑得快整场房费");
	}

	void PaoDeKuaiRoom::recordFinalRoomScoreboard() {
		if (_roomScoreboardRecorded || _roundNo <= 0)
			return;
		_roomScoreboardRecorded = true;
		for (int _si = 0; _si < 2; _si++) {
			auto avatar = std::dynamic_pointer_cast<PaoDeKuaiAvatar>(GameRoom::getAvatar(_si));
			if (!avatar)
				continue;
			int64_t totalScore = avatar->getTotalScore();
			if (totalScore > 0)
				incWinNum(avatar->getPlayerId());
			else if (totalScore < 0)
				incLoseNum(avatar->getPlayerId());
			else
				incDrawNum(avatar->getPlayerId());
		}
	}

			void PaoDeKuaiRoom::calculateScores(int winnerSeat) {
		finalizePendingBomb();
		int baseScore = _rule->getBaseScore();
		int scoreScale = _rule->getScoreScale();
		int multiplier = _birdMultiplier;
		int loserSeat = getNextSeat(winnerSeat);
		auto loser = getAvatar(loserSeat);
		int loserCards = loser ? loser->getCardNums() : 0;
		_spring = false;
		int scoreCards = 0;
		if (loserCards > 1)
			scoreCards = loserCards;

		int64_t cardScore = static_cast<int64_t>(scoreCards) * baseScore * multiplier;
		int maxScore = _rule->getMaxRoundScore();
		if (maxScore > 0 && cardScore > maxScore)
			cardScore = maxScore;
		_multiplier = multiplier;

		int64_t roundDeltas[2] = {0, 0};
		roundDeltas[winnerSeat] += cardScore;
		roundDeltas[loserSeat] -= cardScore;
		for (int _si = 0; _si < 2; _si++)
			roundDeltas[_si] += _bombScoreDeltas[_si];

		for (int _si = 0; _si < 2; _si++) {
			if (roundDeltas[_si] >= 0)
				continue;
			auto avatar = std::dynamic_pointer_cast<PaoDeKuaiAvatar>(GameRoom::getAvatar(_si));
				int64_t cashPledge = avatar ? static_cast<int64_t>(
					std::llround(std::max(0.0, avatar->getCashPledge()) * scoreScale)) : 0LL;
			int64_t loss = -roundDeltas[_si];
			if (loss <= cashPledge)
				continue;
			int opponent = getNextSeat(_si);
			int64_t overflow = loss - cashPledge;
			roundDeltas[_si] = -cashPledge;
			roundDeltas[opponent] -= overflow;
		}

		for (int _si = 0; _si < 2; _si++) {
			auto avatar = std::dynamic_pointer_cast<PaoDeKuaiAvatar>(GameRoom::getAvatar(_si));
			if (!avatar)
				continue;
			int roundScore = static_cast<int>(roundDeltas[_si]);
			avatar->setRoundScore(roundScore);
			avatar->setWinGold(static_cast<int64_t>(roundScore));
			avatar->setTotalScore(avatar->getTotalScore() + roundScore);
		}

		// 记录到回放
		int idx = 0;
		for (int _si = 0; _si < 2; _si++) {
			auto avatar = std::dynamic_pointer_cast<PaoDeKuaiAvatar>(GameRoom::getAvatar(_si));
			if (!avatar)
				continue;
			_playback.scores[idx] = avatar->getRoundScore();
			_playback.winGolds[idx] = avatar->getWinGold();
			idx++;
		}
		Json::Value settlement(Json::objectValue);
		settlement["winnerSeat"] = winnerSeat;
		settlement["loserSeat"] = loserSeat;
		settlement["loserCards"] = loserCards;
		settlement["scoreCards"] = scoreCards;
		settlement["baseScore"] = baseScore;
		settlement["scoreScale"] = scoreScale;
		settlement["bombCount"] = _bombCount;
		settlement["multiplier"] = _multiplier;
		settlement["zhaNiao"] = isZhaNiaoEnabled();
		settlement["birdHit"] = _birdHit;
		settlement["birdSeat"] = _birdSeat;
		settlement["birdMultiplier"] = _birdMultiplier;
		settlement["cardScore"] = static_cast<Json::Int64>(cardScore);
		settlement["bombScores"][0] = _bombScoreDeltas[0];
		settlement["bombScores"][1] = _bombScoreDeltas[1];
		settlement["bombWinCounts"][0] = _bombWinCounts[0];
		settlement["bombWinCounts"][1] = _bombWinCounts[1];
		settlement["spring"] = _spring;
		_playback.settlement = settlement.toStyledString();
	}

	void PaoDeKuaiRoom::saveRoundRecord() {
		auto task = std::make_shared<PaoDeKuaiRecordTask>();
		task->_venueId = getId();
		task->_roundNo = _roundNo;
		task->_banker = _banker;
		task->_scoreScale = _rule ? _rule->getScoreScale() : 1;

		// 生成随机种子hash
		_playback.randomSeedHash = ReplayUtils::generateSeedHash(getId(), _roundNo, _banker);
		task->_randomSeedHash = _playback.randomSeedHash;

		int idx = 0;
		for (int _si = 0; _si < 2; _si++) {
			auto avatar = std::dynamic_pointer_cast<PaoDeKuaiAvatar>(GameRoom::getAvatar(_si));
			if (!avatar)
				continue;
			task->_playerIds[idx] = avatar->getPlayerId();
			task->_scores[idx] = avatar->getRoundScore();
			task->_winGolds[idx] = avatar->getWinGold();
			idx++;
		}

		// 序列化回放数据
		std::string replayData;
		msgpack::sbuffer sbuf;
		msgpack::pack(sbuf, _playback);
		ReplayUtils::compressReplay(sbuf.data(), static_cast<int>(sbuf.size()), replayData);
		task->_playback = replayData;

		MysqlPool::getSingleton().asyncQuery(task);

		// 风控采集：记录得分并结束
		_riskCollector.setRandomSeedHash(_playback.randomSeedHash);
		for (int _si = 0; _si < 2; _si++) {
			auto avatar = std::dynamic_pointer_cast<PaoDeKuaiAvatar>(GameRoom::getAvatar(_si));
			if (!avatar)
				continue;
			_riskCollector.recordScore(avatar->getPlayerId(), avatar->getRoundScore(), avatar->getWinGold());
		}
		_riskCollector.finishRound();

		// 随机审计日志
		std::vector<int> cardOrder;
		std::vector<std::string> playerIds;
		for (int _si = 0; _si < 2; _si++) {
			auto avatar = std::dynamic_pointer_cast<PaoDeKuaiAvatar>(GameRoom::getAvatar(_si));
			if (!avatar)
				continue;
			playerIds.push_back(avatar->getPlayerId());
		}
		RandomAuditLogger::logAudit(getId(), static_cast<int>(GameType::PaoDeKuai),
			_roundNo, _banker, _playback.randomSeedHash, cardOrder, playerIds);
	}

	void PaoDeKuaiRoom::autoPlay() {
		auto avatar = getAvatar(_currentPlayer);
		if (!avatar)
			return;

		// 标记托管
		avatar->setTrusteeship(true);

		if (_isFirstPlay) {
			std::vector<int> cardIds;
			if (findAutoPlay(_currentPlayer, true, cardIds)) {
				doPlay(_currentPlayer, cardIds);
			}
			else {
				// 没有可出的牌（不应该发生）
				const CardArray& cards = avatar->getCards();
				if (!cards.empty()) {
					std::vector<int> ids;
					ids.push_back(cards[0].getId());
					doPlay(_currentPlayer, ids);
				}
			}
		}
		else {
			std::vector<int> cardIds;
			if (findAutoPlay(_currentPlayer, false, cardIds)) {
				doPlay(_currentPlayer, cardIds);
			}
			else {
				// 过牌
				if (_rule->getAllowPass()) {
					std::vector<int> emptyIds;
					doPlay(_currentPlayer, emptyIds);
				}
			}
		}
	}

	void PaoDeKuaiRoom::updateDistrictNotFull() {
		if (_districtId == 0)
			return;
		std::string redisKey = RedisKeys::DISTRICT_NOT_FULL_VENUES + std::to_string(_districtId);
		if (isFull())
			RedisPool::getSingleton().hdel(redisKey, getId());
		else
			RedisPool::getSingleton().hset(redisKey, getId(), getAvatarCount());
	}

	void PaoDeKuaiRoom::recordDistrictPlayerTrack(const std::string& playerId) {
		if (_districtId == 0 || playerId.empty())
			return;
		std::string redisKey = RedisKeys::DISTRICT_PLAYER_TRACK;
		std::string::size_type pos = redisKey.find("{0}");
		if (pos != std::string::npos)
			redisKey.replace(pos, 3, std::to_string(_districtId));
		pos = redisKey.find("{1}");
		if (pos != std::string::npos)
			redisKey.replace(pos, 3, playerId);
		RedisPool::getSingleton().hset(redisKey, getId(), BaseUtils::getCurrentMillisecond());
	}

	// 消息处理
	void PaoDeKuaiRoom::onSyncTable(const NetMessage::Ptr& netMsg) {
		auto msg = std::dynamic_pointer_cast<MsgPaoDeKuaiSync>(netMsg->getMessage());
		if (!msg)
			return;

		const std::string& playerId = msg->getPlayerId();
		auto session = netMsg->getSession();
		if (!session)
			return;

		auto resp = std::make_shared<MsgPaoDeKuaiSyncResp>();
		resp->gameState = static_cast<int>(_gameState);
		resp->roundNo = _roundNo;
		resp->banker = _banker;
		resp->playerCount = _rule->getPlayerCount();
		resp->number = _number;
		resp->level = _level;
		resp->baseScore = _rule->getBaseScore();
		resp->scoreScale = _rule->getScoreScale();
		resp->roundCount = _rule->getRoundCount();
		resp->bombCount = _bombCount;
		resp->multiplier = _multiplier;

		// 查找玩家座位
		for (int _si = 0; _si < 2; _si++) {
			auto avatar = std::dynamic_pointer_cast<PaoDeKuaiAvatar>(GameRoom::getAvatar(_si));
			if (!avatar)
				continue;
			resp->remainCounts[_si] = avatar->getCardNums();
			resp->scores[_si] = avatar->getRoundScore();
			resp->bombScores[_si] = _bombScoreDeltas[_si];
			resp->bombWinCounts[_si] = _bombWinCounts[_si];
			AvatarInfo info;
			info.playerId = avatar->getPlayerId();
			info.nickname = avatar->getNickname();
			info.headUrl = avatar->getHeadUrl();
			info.seat = avatar->getSeat();
			info.sex = avatar->getSex();
			info.ready = avatar->isReady();
			info.offline = avatar->isOffline();
			getAvatarExtraInfo(avatar, info.base64);
			resp->avatars.push_back(info);
			if (avatar->getPlayerId() == playerId) {
				resp->mySeat = _si;
				resp->currentPlayer = _currentPlayer;
				const CardArray& cards = avatar->getCards();
				for (auto& c : cards)
					resp->myCards.push_back(c.getId());
			}
		}

		// 填充最新出牌信息
		if (_lastPlaySeat >= 0) {
			resp->lastPlaySeat = _lastPlaySeat;
			resp->lastPlayGenre = _lastPlayGenre.getGenre();
			const CardArray& playCards = _lastPlayGenre.getCards();
			for (auto& c : playCards)
				resp->lastPlayCards.push_back(c.getId());
		}
		resp->isFirstPlay = _isFirstPlay;

		resp->send(session);
		sendAvatars(session);
	}

	void PaoDeKuaiRoom::onReady(const NetMessage::Ptr& netMsg) {
		if (_gameState != GameState::None && _gameState != GameState::Ready)
			return;
		if (_rule->getRoundCount() > 0 && _roundNo >= _rule->getRoundCount())
			return;

		auto msg = std::dynamic_pointer_cast<MsgPaoDeKuaiReady>(netMsg->getMessage());
		if (!msg)
			return;

		for (int _si = 0; _si < 2; _si++) {
			auto avatar = std::dynamic_pointer_cast<PaoDeKuaiAvatar>(GameRoom::getAvatar(_si));
			if (!avatar)
				continue;
			if (avatar->getPlayerId() == msg->getPlayerId()) {
				avatar->setReady(true);
				MsgPlayerReadyResp resp;
				resp.playerId = avatar->getPlayerId();
				resp.seat = avatar->getSeat();
				sendMessageToAll(resp);
				break;
			}
		}

		setState(GameState::Ready);

		if (allReady()) {
			startRound();
		}
	}

	void PaoDeKuaiRoom::onPlay(const NetMessage::Ptr& netMsg) {
		if (_gameState != GameState::Playing)
			return;

		auto msg = std::dynamic_pointer_cast<MsgPaoDeKuaiPlay>(netMsg->getMessage());
		if (!msg)
			return;

		// 查找玩家座位
		int seat = -1;
		for (int _si = 0; _si < 2; _si++) {
			auto avatar = std::dynamic_pointer_cast<PaoDeKuaiAvatar>(GameRoom::getAvatar(_si));
			if (!avatar)
				continue;
			if (avatar->getPlayerId() == msg->getPlayerId()) {
				seat = _si;
				break;
			}
		}
		if (seat < 0)
			return;

		auto session = netMsg->getSession();
		PlayResult result = doPlay(seat, msg->cardIds);
		if (result != PlayResult::OK) {
			if (session) {
				auto resp = std::make_shared<MsgPaoDeKuaiPlayFailed>();
				switch (result) {
				case PlayResult::NotYourTurn:
					resp->errMsg = "不是你的回合";
					break;
				case PlayResult::InvalidCards:
					resp->errMsg = "无效的牌";
					break;
				case PlayResult::InvalidGenre:
					resp->errMsg = "无效的牌型";
					break;
				case PlayResult::CannotBeat:
					resp->errMsg = "无法大过上家";
					break;
				case PlayResult::CannotPass:
					resp->errMsg = "有牌能大过上家，不能过牌";
					break;
				default:
					resp->errMsg = "出牌失败";
					break;
				}
				resp->send(session);
			}
		}
	}

	void PaoDeKuaiRoom::onDisbandRequest(const NetMessage::Ptr& netMsg) {
		if (_gameState != GameState::Playing)
			return;
		if (_dissolveRequested)
			return;
		MsgDisbandRequest* inst = dynamic_cast<MsgDisbandRequest*>(netMsg->getMessage().get());
		if (inst == nullptr)
			return;
		auto avatar = std::dynamic_pointer_cast<PaoDeKuaiAvatar>(GameRoom::getAvatar(inst->getPlayerId()));
		if (!avatar)
			return;
		_dissolveRequested = true;
		_dissolveRequester = avatar->getSeat();
		_dissolveTick = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
		for (int _si = 0; _si < 2; _si++)
			_dissolveVotes[_si] = 0;
		_dissolveVotes[_dissolveRequester] = 1;
		notifyDisbandVote(std::string(""));
	}

	void PaoDeKuaiRoom::onDisbandChoose(const NetMessage::Ptr& netMsg) {
		MsgDisbandChoose* inst = dynamic_cast<MsgDisbandChoose*>(netMsg->getMessage().get());
		if (inst == nullptr)
			return;
		if (inst->choice != 1 && inst->choice != 2)
			return;
		auto avatar = std::dynamic_pointer_cast<PaoDeKuaiAvatar>(GameRoom::getAvatar(inst->getPlayerId()));
		if (avatar)
			doDisbandChoose(avatar->getSeat(), inst->choice);
	}

	void PaoDeKuaiRoom::doDisbandChoose(int seat, int choice) {
		if (!_dissolveRequested)
			return;
		if (seat < 0 || seat >= 2)
			return;
		_dissolveVotes[seat] = choice;

		MsgDisbandChoice msg;
		msg.seat = seat;
		auto avatar = getAvatar(seat);
		if (avatar)
			msg.playerId = avatar->getPlayerId();
		msg.choice = choice;
		sendMessageToAll(msg);

		int agree = 0;
		int reject = 0;
		for (int _si = 0; _si < 2; _si++) {
			if (_dissolveVotes[_si] == 1)
				agree++;
			else if (_dissolveVotes[_si] == 2)
				reject++;
		}
		if (agree >= 2)
			disbandRoom();
		else if (reject > 0)
			disbandObsolete();
	}

	void PaoDeKuaiRoom::notifyDisbandVote(const std::string& playerId) {
		MsgPaoDeKuaiDisbandVote msg;
		msg.disbander = _dissolveRequester;
		time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
		msg.remainTime = 300 - static_cast<int>(now - _dissolveTick);
		if (msg.remainTime < 0)
			msg.remainTime = 0;
		for (int _si = 0; _si < 2; _si++)
			msg.choices[_si] = _dissolveVotes[_si];
		if (playerId.empty())
			sendMessageToAll(msg);
		else
			sendMessage(msg, playerId);
	}

	void PaoDeKuaiRoom::disbandRoom() {
		_dissolveRequested = false;
		if (_roundNo > 0) {
			MsgPaoDeKuaiSettlement settlement;
			settlement.winnerSeat = 0;
			int64_t bestScore = LLONG_MIN;
			std::vector<std::pair<std::string, int64_t>> netWins;
			for (int _si = 0; _si < 2; _si++) {
				auto avatar = std::dynamic_pointer_cast<PaoDeKuaiAvatar>(GameRoom::getAvatar(_si));
				if (!avatar)
					continue;
				int64_t totalScore = avatar->getTotalScore();
				settlement.scores[_si] = static_cast<int>(totalScore);
				settlement.winGolds[_si] = totalScore;
				settlement.golds[_si] = avatar->getCashPledge();
				const CardArray& cards = avatar->getCards();
				for (auto& c : cards)
					settlement.remainCards[_si].push_back(c.getId());
				netWins.emplace_back(avatar->getPlayerId(), totalScore);
				if (totalScore > bestScore) {
					bestScore = totalScore;
					settlement.winnerSeat = _si;
				}
			}
			settlement.roundNo = _roundNo;
			settlement.roundCount = _rule ? _rule->getRoundCount() : 0;
			settlement.baseScore = _rule->getBaseScore();
			settlement.scoreScale = _rule->getScoreScale();
			settlement.bombCount = _bombCount;
			settlement.multiplier = _multiplier;
			settlement.spring = _spring;
			settlement.zhaNiao = isZhaNiaoEnabled();
			settlement.birdHit = _birdHit;
			settlement.birdSeat = _birdSeat;
			settlement.birdMultiplier = _birdMultiplier;
			for (int _si = 0; _si < 2; _si++) {
				settlement.bombScores[_si] = _bombScoreDeltas[_si];
				settlement.bombWinCounts[_si] = _bombWinCounts[_si];
			}
			calcRoomFeeSettlementData(_roomFee, netWins,
				settlement.roomFeeTotal, settlement.roomFeePlayerIds, settlement.roomFeeAmounts);
			getShuffleFeeSettlementData(settlement.shuffleFeeTotal,
				settlement.shuffleFeePlayerIds, settlement.shuffleFeeAmounts);
			settlement.roomFinished = true;
			sendMessageToAll(settlement);
		}
		MsgDisband msg;
		sendMessageToAll(msg);
		recordFinalRoomScoreboard();
		publishFinalRoomFee();
		kickAllAvatars();
		gameOver();
	}

	void PaoDeKuaiRoom::disbandObsolete() {
		_dissolveRequested = false;
		_dissolveRequester = -1;
		_dissolveTick = 0;
		for (int _si = 0; _si < 2; _si++)
			_dissolveVotes[_si] = 0;
		MsgDisbandObsolete msg;
		sendMessageToAll(msg);
	}

	// 消息发送
	void PaoDeKuaiRoom::notifyDeal(const std::string& playerId) {
		int seat = -1;
		for (int _si = 0; _si < 2; _si++) {
			auto avatar = std::dynamic_pointer_cast<PaoDeKuaiAvatar>(GameRoom::getAvatar(_si));
			if (!avatar)
				continue;
			if (avatar->getPlayerId() == playerId) {
				seat = _si;
				break;
			}
		}
		if (seat < 0)
			return;

		auto avatar = getAvatar(seat);
		if (!avatar)
			return;

		auto msg = std::make_shared<MsgPaoDeKuaiDeal>();
		const CardArray& cards = avatar->getCards();
		for (auto& c : cards)
			msg->cards.push_back(c.getId());
		msg->firstPlayer = _currentPlayer;
		msg->roundNo = _roundNo;
		msg->banker = _banker;
		msg->roundCount = _rule->getRoundCount();
		msg->baseScore = _rule->getBaseScore();
		msg->scoreScale = _rule->getScoreScale();

		sendMessage(*msg, playerId);
	}

	void PaoDeKuaiRoom::notifyPlay(int seat, const std::vector<int>& cardIds, int genre, int nextPlayer) {
		auto msg = std::make_shared<MsgPaoDeKuaiPlayNotify>();
		msg->seat = seat;
		msg->cardIds = cardIds;
		msg->genre = genre;
		msg->nextPlayer = nextPlayer;
		auto avatar = getAvatar(seat);
		msg->remainCount = avatar ? avatar->getCardNums() : 0;
		msg->multiplier = _multiplier;
		msg->bombCount = _bombCount;
		for (int _si = 0; _si < 2; _si++) {
			auto pdkAvatar = getAvatar(_si);
			if (!pdkAvatar)
				continue;
			msg->scores[_si] = pdkAvatar->getRoundScore();
			msg->bombScores[_si] = _bombScoreDeltas[_si];
			msg->bombWinCounts[_si] = _bombWinCounts[_si];
		}
		sendMessageToAll(*msg);
	}

		void PaoDeKuaiRoom::notifySettlement(int winnerSeat, bool roomFinished) {
			auto msg = std::make_shared<MsgPaoDeKuaiSettlement>();
			msg->winnerSeat = winnerSeat;
			msg->roundNo = _roundNo;
		msg->roundCount = _rule->getRoundCount();
		msg->baseScore = _rule->getBaseScore();
		msg->scoreScale = _rule->getScoreScale();
		msg->bombCount = _bombCount;
		msg->multiplier = _multiplier;
		msg->spring = _spring;
		msg->zhaNiao = isZhaNiaoEnabled();
			msg->birdHit = _birdHit;
			msg->birdSeat = _birdSeat;
			msg->birdMultiplier = _birdMultiplier;
			msg->roomFinished = roomFinished;

		for (int _si = 0; _si < 2; _si++) {
			auto avatar = std::dynamic_pointer_cast<PaoDeKuaiAvatar>(GameRoom::getAvatar(_si));
			if (!avatar)
				continue;
			msg->scores[_si] = avatar->getRoundScore();
			msg->winGolds[_si] = avatar->getWinGold();
			msg->golds[_si] = avatar->getCashPledge();
			msg->bombScores[_si] = _bombScoreDeltas[_si];
			msg->bombWinCounts[_si] = _bombWinCounts[_si];
			// 剩余手牌
			const CardArray& cards = avatar->getCards();
			for (auto& c : cards)
				msg->remainCards[_si].push_back(c.getId());
		}
			if (roomFinished) {
				std::vector<std::pair<std::string, int64_t>> netWins;
				for (int _si = 0; _si < 2; _si++) {
				auto avatar = std::dynamic_pointer_cast<PaoDeKuaiAvatar>(GameRoom::getAvatar(_si));
				if (!avatar)
					continue;
				netWins.emplace_back(avatar->getPlayerId(), avatar->getTotalScore());
			}
			calcRoomFeeSettlementData(_roomFee, netWins,
				msg->roomFeeTotal, msg->roomFeePlayerIds, msg->roomFeeAmounts);
			getShuffleFeeSettlementData(msg->shuffleFeeTotal,
				msg->shuffleFeePlayerIds, msg->shuffleFeeAmounts);
		}

		sendMessageToAll(*msg);
	}

	void PaoDeKuaiRoom::notifyGameState(const std::string& playerId) {
		(void)playerId;
		// 复用sync消息通知状态
	}
}
