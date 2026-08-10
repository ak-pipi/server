// DouDiZhuRoom.cpp

#include "DouDiZhuRoom.h"
#include "DouDiZhuMessages.h"
#include "GameDefines.h"
#include "Game/WalletEventTask.h"
#include "Network/MsgSession.h"
#include "Base/BaseUtils.h"
#include "Base/Log.h"
#include "Constant/RedisKeys.h"
#include "Redis/RedisPool.h"

#include <algorithm>
#include <chrono>
#include <json/json.h>
#include <limits>
#include <random>
#include <sstream>

	namespace NiuMa
	{
		namespace
		{
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
		}

		DouDiZhuRoom::DouDiZhuRoom(const std::shared_ptr<DouDiZhuGameRule>& rule,
			const std::string& venueId,
		const std::string& number,
		int level,
		const std::string& ruleConfig,
		int districtId)
		: GameRoom(venueId, static_cast<int>(GameType::DouDiZhu), 2)
		, _rule(rule)
		, _dealer(rule)
		, _ruleConfig(ruleConfig)
		, _number(number)
		, _level(level)
		, _districtId(districtId)
		, _gameState(GameState::None)
		, _stateTime(0)
		, _roundNo(0)
		, _banker(-1)
		, _currentPlayer(-1)
		, _landlordSeat(-1)
		, _callStarter(-1)
		, _callTurn(-1)
		, _highestBidSeat(-1)
		, _highestBid(0)
			, _callCount(0)
			, _multiplier(1)
			, _spring(false)
			, _roomFee(0)
		, _lastPlaySeat(-1)
		, _isFirstPlay(true)
		, _autoActionTime(0)
		, _dissolveRequested(false)
		, _dissolveRequester(-1)
		, _dissolveTick(0)
	{
			_playCounts[0] = 0;
			_playCounts[1] = 0;
			_dissolveVotes[0] = 0;
			_dissolveVotes[1] = 0;
			_rule->loadConfig(ruleConfig);
			_roomFee = readRoomFeeAmount(ruleConfig);
		}

	DouDiZhuRoom::~DouDiZhuRoom() {}

	GameAvatar::Ptr DouDiZhuRoom::createAvatar(const std::string& playerId, int seat, bool robot) const {
		return std::make_shared<DouDiZhuAvatar>(_rule, playerId, seat, robot);
	}

	bool DouDiZhuRoom::checkEnter(const std::string& playerId, std::string& errMsg, bool robot) const {
		(void)playerId;
		(void)robot;
		if (getAvatarCount() >= _rule->getPlayerCount()) {
			errMsg = "房间已满";
			return false;
		}
		return true;
	}

	int DouDiZhuRoom::checkLeave(const std::string& playerId, std::string& errMsg) const {
		(void)playerId;
		if (_gameState == GameState::Bidding || _gameState == GameState::Playing) {
			errMsg = "游戏进行中，不能直接离开房间";
			return 1;
		}
		return 0;
	}

	void DouDiZhuRoom::getAvatarExtraInfo(const GameAvatar::Ptr& avatar, std::string& base64) const {
		if (!avatar)
			return;
		int64_t gold = avatar->getCashPledge();
		Json::Value tmp(Json::objectValue);
		tmp["gold"] = static_cast<Json::Int64>(gold);
		tmp["diamond"] = static_cast<Json::Int64>(0);
		if (!avatar->isOffline()) {
			Session::Ptr session = avatar->getSession();
			if (session)
				tmp["ip"] = session->getRemoteIp();
		}
		tmp["authorize"] = avatar->isAuthorize();
		int winNum = 0;
		int loseNum = 0;
		int drawNum = 0;
		avatar->getScoreboard(winNum, loseNum, drawNum);
		tmp["winNum"] = winNum;
		tmp["loseNum"] = loseNum;
		tmp["drawNum"] = drawNum;
		std::string json = tmp.toStyledString();
		BaseUtils::encodeBase64(base64, json.data(), static_cast<int>(json.size()));
	}

	void DouDiZhuRoom::onAvatarJoined(int seat, const std::string& playerId) {
		GameRoom::onAvatarJoined(seat, playerId);
		updateDistrictNotFull();
	}

	void DouDiZhuRoom::onAvatarLeaved(int seat, const std::string& playerId) {
		(void)seat;
		if (getAvatarCount() == 0)
			setState(GameState::None);
		updateDistrictNotFull();
		recordDistrictPlayerTrack(playerId);
	}

	void DouDiZhuRoom::clean() {
		GameRoom::clean();
		_gameState = GameState::None;
		_roundNo = 0;
		_currentPlayer = -1;
		_landlordSeat = -1;
		_banker = -1;
		_callStarter = -1;
		_callTurn = -1;
		_highestBidSeat = -1;
		_highestBid = 0;
		_callCount = 0;
		_multiplier = 1;
		_playCounts[0] = 0;
		_playCounts[1] = 0;
		_spring = false;
		_dissolveRequested = false;
		_dissolveRequester = -1;
		_dissolveTick = 0;
		_dissolveVotes[0] = 0;
		_dissolveVotes[1] = 0;
		_lastPlaySeat = -1;
		_isFirstPlay = true;
		_lastPlayGenre.clear();
		_bottomCards.clear();
		_discardedCards.clear();
		for (int i = 0; i < 2; i++) {
			std::shared_ptr<DouDiZhuAvatar> avatar = getAvatar(i);
			if (avatar)
				avatar->clear();
		}
	}

	bool DouDiZhuRoom::canShuffleCardsBeforeNextRound(const std::string& playerId,
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

	void DouDiZhuRoom::onTimer() {
		if (_dissolveRequested && _dissolveTick > 0) {
			time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
			if (now - _dissolveTick >= 300) {
				for (int i = 0; i < 2; i++) {
					if (_dissolveVotes[i] == 0)
						doDisbandChoose(i, 1);
				}
			}
		}
		if (_gameState != GameState::Bidding && _gameState != GameState::Playing)
			return;
		time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
		if (_autoActionTime > 0 && now >= _autoActionTime) {
			_autoActionTime = 0;
			autoAction();
		}
	}

	bool DouDiZhuRoom::onMessage(const NetMessage::Ptr& netMsg) {
		const std::string& type = netMsg->getType();
		if (type == MsgDouDiZhuSync::TYPE) {
			onSyncTable(netMsg);
			return true;
		}
		if (type == MsgDouDiZhuReady::TYPE || type == MsgPlayerReady::TYPE) {
			onReady(netMsg);
			return true;
		}
		if (type == MsgDouDiZhuCall::TYPE) {
			onCall(netMsg);
			return true;
		}
		if (type == MsgDouDiZhuPlay::TYPE) {
			onPlay(netMsg);
			return true;
		}
		if (type == MsgDisbandRequest::TYPE) {
			onDisbandRequest(netMsg);
			return true;
		}
		if (type == MsgDisbandChoose::TYPE) {
			onDisbandChoose(netMsg);
			return true;
		}
		return GameRoom::onMessage(netMsg);
	}

	std::shared_ptr<DouDiZhuAvatar> DouDiZhuRoom::getAvatar(int seat) const {
		return std::dynamic_pointer_cast<DouDiZhuAvatar>(GameRoom::getAvatar(seat));
	}

	void DouDiZhuRoom::setState(GameState state) {
		_gameState = state;
		_stateTime = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
		switch (state) {
		case GameState::None:
			setRoomState(RoomState::Waiting);
			break;
		case GameState::Ready:
			setRoomState(RoomState::Ready);
			break;
		case GameState::Bidding:
		case GameState::Playing:
			setRoomState(RoomState::Playing);
			break;
		case GameState::Settling:
			setRoomState(RoomState::Settling);
			break;
		}
	}

	bool DouDiZhuRoom::allReady() const {
		if (getAvatarCount() < _rule->getPlayerCount())
			return false;
		for (int i = 0; i < 2; i++) {
			std::shared_ptr<DouDiZhuAvatar> avatar = getAvatar(i);
			if (!avatar || !avatar->isReady())
				return false;
		}
		return true;
	}

	int DouDiZhuRoom::getNextSeat(int seat) const {
		return (seat + 1) % _rule->getPlayerCount();
	}

	int DouDiZhuRoom::findSeatByPlayer(const std::string& playerId) const {
		for (int i = 0; i < 2; i++) {
			std::shared_ptr<DouDiZhuAvatar> avatar = getAvatar(i);
			if (avatar && avatar->getPlayerId() == playerId)
				return i;
		}
		return -1;
	}

	void DouDiZhuRoom::fillHandCounts(int counts[2]) const {
		for (int i = 0; i < 2; i++) {
			std::shared_ptr<DouDiZhuAvatar> avatar = getAvatar(i);
			counts[i] = avatar ? avatar->getCardNums() : 0;
		}
	}

	void DouDiZhuRoom::fillCardIds(const CardArray& cards, std::vector<int>& ids) const {
		ids.clear();
		for (const PokerCard& c : cards)
			ids.push_back(c.getId());
	}

	void DouDiZhuRoom::startRound(bool advanceRound) {
		if (advanceRound && _rule->getRoundCount() > 0 && _roundNo >= _rule->getRoundCount())
			return;
		if (advanceRound || _roundNo <= 0)
			_roundNo++;
		_currentPlayer = -1;
		_landlordSeat = -1;
		static std::mt19937 rng(static_cast<unsigned>(
			std::chrono::system_clock::now().time_since_epoch().count()));
		std::uniform_int_distribution<int> seatDist(0, _rule->getPlayerCount() - 1);
		_banker = seatDist(rng);
		_callStarter = _banker;
		_callTurn = _callStarter;
		_highestBidSeat = -1;
		_highestBid = 0;
		_callCount = 0;
		_multiplier = 1;
		_playCounts[0] = 0;
		_playCounts[1] = 0;
		_spring = false;
		_lastPlaySeat = -1;
		_isFirstPlay = true;
		_lastPlayGenre.clear();
		_bottomCards.clear();
		_discardedCards.clear();

		for (int i = 0; i < 2; i++) {
			std::shared_ptr<DouDiZhuAvatar> avatar = getAvatar(i);
			if (avatar) {
				avatar->clear();
				avatar->setRoundScore(0);
				avatar->setWinGold(0);
			}
		}
		_dealer.shuffle();
		dealCards();
		setState(GameState::Bidding);
		for (int i = 0; i < 2; i++) {
			std::shared_ptr<DouDiZhuAvatar> avatar = getAvatar(i);
			if (avatar)
				notifyDeal(avatar->getPlayerId());
		}
		_autoActionTime = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()) +
			_rule->getCallTimeout() / 1000;
		InfoS << "斗地主开始叫地主，场地: " << getId() << "，局号: " << _roundNo
			<< "，庄家: " << _banker;
	}

	void DouDiZhuRoom::dealCards() {
		for (int i = 0; i < 2; i++) {
			CardArray cards;
			_dealer.handOutCards(cards, _rule->getHandCardCount());
			std::shared_ptr<DouDiZhuAvatar> avatar = getAvatar(i);
			if (avatar) {
				avatar->setCards(cards);
				avatar->sortCards();
			}
		}
		_dealer.handOutCards(_bottomCards, _rule->getBottomCardCount());
		_dealer.handOutCards(_discardedCards, _dealer.getCardLeft());
	}

	DouDiZhuRoom::CallResult DouDiZhuRoom::doCall(int seat, int score) {
		if (_gameState != GameState::Bidding || seat != _callTurn)
			return CallResult::NotYourTurn;
		if (score < 0 || score > 3)
			return CallResult::InvalidScore;
		if (score > 0 && score <= _highestBid)
			return CallResult::InvalidScore;

		if (score > 0) {
			_highestBid = score;
			_highestBidSeat = seat;
		}
		_callCount++;
		int nextSeat = -1;
		if (score == 3) {
			notifyCall(seat, score, -1);
			finishBidding(seat, score);
			return CallResult::OK;
		}
		if (_callCount >= _rule->getPlayerCount()) {
			notifyCall(seat, score, -1);
			if (_highestBidSeat >= 0)
				finishBidding(_highestBidSeat, _highestBid);
			else
				startRound(false);
			return CallResult::OK;
		}
		nextSeat = getNextSeat(seat);
		_callTurn = nextSeat;
		notifyCall(seat, score, nextSeat);
		_autoActionTime = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()) +
			_rule->getCallTimeout() / 1000;
		return CallResult::OK;
	}

	void DouDiZhuRoom::finishBidding(int landlordSeat, int score) {
		_landlordSeat = landlordSeat;
		_banker = landlordSeat;
		_multiplier = std::max(1, score);
		std::shared_ptr<DouDiZhuAvatar> landlord = getAvatar(landlordSeat);
		if (landlord) {
			landlord->setLandlord(true);
			for (const PokerCard& c : _bottomCards)
				landlord->addCard(c);
			landlord->sortCards();
		}
		_currentPlayer = landlordSeat;
		_lastPlaySeat = -1;
		_isFirstPlay = true;
		_lastPlayGenre.clear();
		setState(GameState::Playing);
		notifyLandlord();
		_autoActionTime = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()) +
			_rule->getAutoPlayTimeout() / 1000;
	}

	DouDiZhuRoom::PlayResult DouDiZhuRoom::validatePlay(int seat, const std::vector<int>& cardIds, PokerGenre& genre) const {
		if (_gameState != GameState::Playing || seat != _currentPlayer)
			return PlayResult::NotYourTurn;
		if (cardIds.empty()) {
			if (_isFirstPlay)
				return PlayResult::CannotPass;
			return PlayResult::OK;
		}
		std::shared_ptr<DouDiZhuAvatar> avatar = getAvatar(seat);
		if (!avatar)
			return PlayResult::InvalidCards;
		CardArray cards;
		if (!avatar->getCardsByIds(cardIds, cards))
			return PlayResult::InvalidCards;
		std::sort(cards.begin(), cards.end(), CardComparator(_rule));
		genre.setCards(cards, _rule);
		if (genre.getGenre() == static_cast<int>(DouDiZhuGenre::Invalid))
			return PlayResult::InvalidGenre;
		if (!_isFirstPlay) {
			int cmp = _rule->compareGenre(genre, _lastPlayGenre);
			if (cmp != 1)
				return PlayResult::CannotBeat;
		}
		return PlayResult::OK;
	}

	DouDiZhuRoom::PlayResult DouDiZhuRoom::doPlay(int seat, const std::vector<int>& cardIds) {
		PokerGenre genre;
		PlayResult result = validatePlay(seat, cardIds, genre);
		if (result != PlayResult::OK)
			return result;
		if (cardIds.empty()) {
			advanceTurn();
			notifyPlay(seat, cardIds, 0, _currentPlayer);
			return PlayResult::OK;
		}
		std::shared_ptr<DouDiZhuAvatar> avatar = getAvatar(seat);
		if (!avatar)
			return PlayResult::InvalidCards;
		avatar->removeCardsByIds(cardIds);
		avatar->setOuttedGenre(genre);
		_playCounts[seat]++;
		_lastPlaySeat = seat;
		_lastPlayGenre = genre;
		_isFirstPlay = false;
		if (genre.getGenre() == static_cast<int>(DouDiZhuGenre::Bomb) ||
			genre.getGenre() == static_cast<int>(DouDiZhuGenre::Rocket))
			_multiplier *= 2;
		if (avatar->isFinished()) {
			notifyPlay(seat, cardIds, genre.getGenre(), -1);
			settle(seat);
			return PlayResult::OK;
		}
		advanceTurn();
		notifyPlay(seat, cardIds, genre.getGenre(), _currentPlayer);
		return PlayResult::OK;
	}

	void DouDiZhuRoom::advanceTurn() {
		_currentPlayer = getNextSeat(_currentPlayer);
		if (_currentPlayer == _lastPlaySeat) {
			_isFirstPlay = true;
			_lastPlaySeat = -1;
			_lastPlayGenre.clear();
		}
		_autoActionTime = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()) +
			_rule->getAutoPlayTimeout() / 1000;
	}

	void DouDiZhuRoom::settle(int winnerSeat) {
		setState(GameState::Settling);
		int farmerSeat = getNextSeat(_landlordSeat);
		if (winnerSeat == _landlordSeat && _playCounts[farmerSeat] == 0)
			_spring = true;
		else if (winnerSeat != _landlordSeat && _playCounts[_landlordSeat] <= 1)
			_spring = true;
			if (_spring)
				_multiplier *= 2;
			calculateScores(winnerSeat);
			bool finishRoom = (_rule->getRoundCount() > 0 && _roundNo >= _rule->getRoundCount());
			for (int i = 0; i < 2; i++) {
				std::shared_ptr<DouDiZhuAvatar> avatar = getAvatar(i);
				if (!avatar)
					continue;
				int64_t cashPledge = avatar->getCashPledge() + avatar->getWinGold();
				if (cashPledge < 0)
					cashPledge = 0;
				if (updateCashPledge(avatar->getPlayerId(), cashPledge))
					avatar->setCashPledge(cashPledge);
				else
					finishRoom = true;
				if (avatar->getCashPledge() <= 0)
					finishRoom = true;
			}
			notifySettlement(winnerSeat);
		for (int i = 0; i < 2; i++) {
			std::shared_ptr<DouDiZhuAvatar> avatar = getAvatar(i);
			if (!avatar)
				continue;
			int64_t winGold = avatar->getWinGold();
			if (winGold > 0)
				WalletEventTask::publish(avatar->getPlayerId(), "GAME_WIN", winGold, "DouDiZhu", getId(), "斗地主赢得金币");
			else if (winGold < 0)
				WalletEventTask::publish(avatar->getPlayerId(), "GAME_LOSE", -winGold, "DouDiZhu", getId(), "斗地主输掉金币");
			avatar->setReady(false);
		}
		_banker = -1;
			_landlordSeat = -1;
			_callStarter = -1;
			_callTurn = -1;
			_currentPlayer = -1;
				if (finishRoom) {
					publishFinalRoomFee();
					kickAllAvatars();
					gameOver();
					return;
				}
			setState(GameState::Ready);
		}

		void DouDiZhuRoom::publishFinalRoomFee() {
			if (_roundNo <= 0)
				return;
			std::vector<std::pair<std::string, int64_t>> netWins;
			for (int i = 0; i < 2; i++) {
				std::shared_ptr<DouDiZhuAvatar> avatar = getAvatar(i);
				if (!avatar)
					continue;
				netWins.emplace_back(avatar->getPlayerId(), avatar->getTotalScore());
			}
			publishRoomFeeOnGameOver(_roomFee, netWins, "DouDiZhu", "斗地主整场房费");
		}

		void DouDiZhuRoom::onDisbandRequest(const NetMessage::Ptr& netMsg) {
			if (_gameState != GameState::Bidding && _gameState != GameState::Playing)
				return;
			if (_dissolveRequested)
				return;
			MsgDisbandRequest* inst = dynamic_cast<MsgDisbandRequest*>(netMsg->getMessage().get());
			if (inst == nullptr)
				return;
			std::shared_ptr<DouDiZhuAvatar> avatar = std::dynamic_pointer_cast<DouDiZhuAvatar>(GameRoom::getAvatar(inst->getPlayerId()));
			if (!avatar)
				return;
			_dissolveRequested = true;
			_dissolveRequester = avatar->getSeat();
			_dissolveTick = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
			for (int i = 0; i < 2; i++)
				_dissolveVotes[i] = 0;
			_dissolveVotes[_dissolveRequester] = 1;
			notifyDisbandVote(std::string(""));
		}

		void DouDiZhuRoom::onDisbandChoose(const NetMessage::Ptr& netMsg) {
			MsgDisbandChoose* inst = dynamic_cast<MsgDisbandChoose*>(netMsg->getMessage().get());
			if (inst == nullptr)
				return;
			if (inst->choice != 1 && inst->choice != 2)
				return;
			std::shared_ptr<DouDiZhuAvatar> avatar = std::dynamic_pointer_cast<DouDiZhuAvatar>(GameRoom::getAvatar(inst->getPlayerId()));
			if (avatar)
				doDisbandChoose(avatar->getSeat(), inst->choice);
		}

		void DouDiZhuRoom::doDisbandChoose(int seat, int choice) {
			if (!_dissolveRequested)
				return;
			if (seat < 0 || seat >= 2)
				return;
			_dissolveVotes[seat] = choice;

			MsgDisbandChoice msg;
			msg.seat = seat;
			std::shared_ptr<DouDiZhuAvatar> avatar = getAvatar(seat);
			if (avatar)
				msg.playerId = avatar->getPlayerId();
			msg.choice = choice;
			sendMessageToAll(msg);

			int agree = 0;
			int reject = 0;
			for (int i = 0; i < 2; i++) {
				if (_dissolveVotes[i] == 1)
					agree++;
				else if (_dissolveVotes[i] == 2)
					reject++;
			}
			if (agree >= 2)
				disbandRoom();
			else if (reject > 0)
				disbandObsolete();
		}

		void DouDiZhuRoom::notifyDisbandVote(const std::string& playerId) {
			MsgDouDiZhuDisbandVote msg;
			msg.disbander = _dissolveRequester;
			time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
			msg.remainTime = 300 - static_cast<int>(now - _dissolveTick);
			if (msg.remainTime < 0)
				msg.remainTime = 0;
			for (int i = 0; i < 2; i++)
				msg.choices[i] = _dissolveVotes[i];
			if (playerId.empty())
				sendMessageToAll(msg);
			else
				sendMessage(msg, playerId);
		}

		void DouDiZhuRoom::disbandRoom() {
			_dissolveRequested = false;
			if (_roundNo > 0) {
				MsgDouDiZhuSettlement settlement;
				settlement.winnerSeat = 0;
				settlement.landlordSeat = _landlordSeat;
				int64_t bestScore = std::numeric_limits<int64_t>::min();
				std::vector<std::pair<std::string, int64_t>> netWins;
				for (int i = 0; i < 2; i++) {
					std::shared_ptr<DouDiZhuAvatar> avatar = getAvatar(i);
					if (!avatar)
						continue;
					int64_t totalScore = avatar->getTotalScore();
					settlement.scores[i] = static_cast<int>(totalScore);
					settlement.winGolds[i] = totalScore;
					for (const PokerCard& c : avatar->getCards())
						settlement.remainCards[i].push_back(c.getId());
					netWins.emplace_back(avatar->getPlayerId(), totalScore);
					if (totalScore > bestScore) {
						bestScore = totalScore;
						settlement.winnerSeat = i;
					}
				}
				settlement.multiplier = _multiplier;
				settlement.spring = _spring;
				calcRoomFeeSettlementData(_roomFee, netWins,
					settlement.roomFeeTotal, settlement.roomFeePlayerIds, settlement.roomFeeAmounts);
				getShuffleFeeSettlementData(settlement.shuffleFeeTotal,
					settlement.shuffleFeePlayerIds, settlement.shuffleFeeAmounts);
				sendMessageToAll(settlement);
			}
			MsgDisband msg;
			sendMessageToAll(msg);
			publishFinalRoomFee();
			kickAllAvatars();
			gameOver();
		}

		void DouDiZhuRoom::disbandObsolete() {
			_dissolveRequested = false;
			_dissolveRequester = -1;
			_dissolveTick = 0;
			_dissolveVotes[0] = 0;
			_dissolveVotes[1] = 0;
			MsgDisbandObsolete msg;
			sendMessageToAll(msg);
		}

		void DouDiZhuRoom::calculateScores(int winnerSeat) {
			int score = _rule->getBaseScore() * std::max(1, _multiplier);
			if (_rule->getMaxRoundScore() > 0)
				score = std::min(score, _rule->getMaxRoundScore());
			int loserSeat = getNextSeat(winnerSeat);
			std::shared_ptr<DouDiZhuAvatar> loser = getAvatar(loserSeat);
			if (loser)
				score = static_cast<int>(std::min<int64_t>(score, std::max<int64_t>(0LL, loser->getCashPledge())));
			for (int i = 0; i < 2; i++) {
			std::shared_ptr<DouDiZhuAvatar> avatar = getAvatar(i);
			if (!avatar)
				continue;
			bool win = (i == winnerSeat);
			int roundScore = win ? score : -score;
			avatar->setRoundScore(roundScore);
			avatar->setWinGold(roundScore);
			avatar->setTotalScore(avatar->getTotalScore() + roundScore);
		}
	}

	void DouDiZhuRoom::autoAction() {
		if (_gameState == GameState::Bidding) {
			doCall(_callTurn, 0);
			return;
		}
		if (_gameState != GameState::Playing)
			return;
		if (!_isFirstPlay) {
			std::vector<int> empty;
			doPlay(_currentPlayer, empty);
			return;
		}
		std::shared_ptr<DouDiZhuAvatar> avatar = getAvatar(_currentPlayer);
		if (!avatar || avatar->getCards().empty())
			return;
		std::vector<int> ids;
		ids.push_back(avatar->getCards().front().getId());
		doPlay(_currentPlayer, ids);
	}

	std::string DouDiZhuRoom::playErrorText(PlayResult result) const {
		switch (result) {
		case PlayResult::NotYourTurn: return "不是你的回合";
		case PlayResult::InvalidCards: return "无效的牌";
		case PlayResult::InvalidGenre: return "无效的牌型";
		case PlayResult::CannotBeat: return "无法大过上家";
		case PlayResult::CannotPass: return "首出不能不要";
		default: return "出牌失败";
		}
	}

	void DouDiZhuRoom::updateDistrictNotFull() {
		if (_districtId == 0)
			return;
		std::string redisKey = RedisKeys::DISTRICT_NOT_FULL_VENUES + std::to_string(_districtId);
		if (isFull())
			RedisPool::getSingleton().hdel(redisKey, getId());
		else
			RedisPool::getSingleton().hset(redisKey, getId(), getAvatarCount());
	}

	void DouDiZhuRoom::recordDistrictPlayerTrack(const std::string& playerId) {
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

	void DouDiZhuRoom::onSyncTable(const NetMessage::Ptr& netMsg) {
		std::shared_ptr<MsgDouDiZhuSync> msg = std::dynamic_pointer_cast<MsgDouDiZhuSync>(netMsg->getMessage());
		if (!msg || !netMsg->getSession())
			return;
		int seat = findSeatByPlayer(msg->getPlayerId());
		std::shared_ptr<DouDiZhuAvatar> avatar = getAvatar(seat);
		std::shared_ptr<MsgDouDiZhuSyncResp> resp = std::make_shared<MsgDouDiZhuSyncResp>();
		resp->gameState = static_cast<int>(_gameState);
		resp->currentPlayer = _currentPlayer;
		resp->mySeat = seat;
		resp->landlordSeat = _landlordSeat;
		resp->banker = _banker;
		resp->callStarter = _callStarter;
		resp->highestBidSeat = _highestBidSeat;
		resp->highestBid = _highestBid;
		resp->callTurn = _callTurn;
		resp->callCount = _callCount;
		resp->multiplier = _multiplier;
		resp->lastPlaySeat = _lastPlaySeat;
		resp->lastPlayGenre = _lastPlayGenre.getGenre();
		resp->isFirstPlay = _isFirstPlay;
		resp->roundNo = _roundNo;
		resp->playerCount = _rule->getPlayerCount();
		resp->number = _number;
		resp->level = _level;
		resp->baseScore = _rule->getBaseScore();
		resp->roundCount = _rule->getRoundCount();
		fillHandCounts(resp->handCounts);
		for (int i = 0; i < _rule->getPlayerCount(); i++) {
			std::shared_ptr<DouDiZhuAvatar> item = getAvatar(i);
			if (!item)
				continue;
			AvatarInfo info;
			info.playerId = item->getPlayerId();
			info.nickname = item->getNickname();
			info.headUrl = item->getHeadUrl();
			info.seat = item->getSeat();
			info.sex = item->getSex();
			info.ready = item->isReady();
			info.offline = item->isOffline();
			getAvatarExtraInfo(item, info.base64);
			resp->avatars.push_back(info);
		}
		if (avatar) {
			for (const PokerCard& c : avatar->getCards())
				resp->myCards.push_back(c.getId());
		}
		if (_landlordSeat >= 0)
			fillCardIds(_bottomCards, resp->bottomCards);
		fillCardIds(_lastPlayGenre.getCards(), resp->lastPlayCards);
		resp->send(netMsg->getSession());
		sendAvatars(netMsg->getSession());
	}

	void DouDiZhuRoom::onReady(const NetMessage::Ptr& netMsg) {
		if (_gameState != GameState::None && _gameState != GameState::Ready)
			return;
		if (_rule->getRoundCount() > 0 && _roundNo >= _rule->getRoundCount())
			return;
		std::shared_ptr<MsgPlayerSignature> msg = std::dynamic_pointer_cast<MsgPlayerSignature>(netMsg->getMessage());
		if (!msg)
			return;
		int seat = findSeatByPlayer(msg->getPlayerId());
		std::shared_ptr<DouDiZhuAvatar> avatar = getAvatar(seat);
		if (!avatar)
			return;
		avatar->setReady(true);
		setState(GameState::Ready);
		notifyReady(seat);
		if (allReady())
			startRound();
	}

	void DouDiZhuRoom::onCall(const NetMessage::Ptr& netMsg) {
		std::shared_ptr<MsgDouDiZhuCall> msg = std::dynamic_pointer_cast<MsgDouDiZhuCall>(netMsg->getMessage());
		if (!msg)
			return;
		int seat = findSeatByPlayer(msg->getPlayerId());
		CallResult result = doCall(seat, msg->score);
		if (result != CallResult::OK && netMsg->getSession()) {
			std::shared_ptr<MsgDouDiZhuCallFailed> resp = std::make_shared<MsgDouDiZhuCallFailed>();
			resp->errMsg = result == CallResult::NotYourTurn ? "不是你的叫分回合" : "叫分无效";
			resp->callTurn = _callTurn;
			resp->highestBid = _highestBid;
			resp->highestBidSeat = _highestBidSeat;
			resp->send(netMsg->getSession());
		}
	}

	void DouDiZhuRoom::onPlay(const NetMessage::Ptr& netMsg) {
		std::shared_ptr<MsgDouDiZhuPlay> msg = std::dynamic_pointer_cast<MsgDouDiZhuPlay>(netMsg->getMessage());
		if (!msg)
			return;
		int seat = findSeatByPlayer(msg->getPlayerId());
		PlayResult result = doPlay(seat, msg->cardIds);
		if (result != PlayResult::OK && netMsg->getSession()) {
			std::shared_ptr<MsgDouDiZhuPlayFailed> resp = std::make_shared<MsgDouDiZhuPlayFailed>();
			resp->errMsg = playErrorText(result);
			resp->send(netMsg->getSession());
		}
	}

	void DouDiZhuRoom::notifyReady(int seat) {
		std::shared_ptr<DouDiZhuAvatar> avatar = getAvatar(seat);
		if (!avatar)
			return;
		MsgPlayerReadyResp msg;
		msg.playerId = avatar->getPlayerId();
		msg.seat = seat;
		sendMessageToAll(msg);
	}

	void DouDiZhuRoom::notifyDeal(const std::string& playerId) {
		int seat = findSeatByPlayer(playerId);
		std::shared_ptr<DouDiZhuAvatar> avatar = getAvatar(seat);
		if (!avatar)
			return;
		std::shared_ptr<MsgDouDiZhuDeal> msg = std::make_shared<MsgDouDiZhuDeal>();
		for (const PokerCard& c : avatar->getCards())
			msg->cards.push_back(c.getId());
		msg->banker = _banker;
		msg->callStarter = _callStarter;
		msg->roundNo = _roundNo;
		msg->roundCount = _rule->getRoundCount();
		msg->baseScore = _rule->getBaseScore();
		fillHandCounts(msg->handCounts);
		sendMessage(*msg, playerId);
	}

	void DouDiZhuRoom::notifyCall(int seat, int score, int nextSeat) {
		MsgDouDiZhuCallNotify msg;
		msg.seat = seat;
		msg.score = score;
		msg.highestBid = _highestBid;
		msg.highestBidSeat = _highestBidSeat;
		msg.nextSeat = nextSeat;
		msg.callCount = _callCount;
		sendMessageToAll(msg);
	}

	void DouDiZhuRoom::notifyLandlord() {
		MsgDouDiZhuLandlord msg;
		msg.landlordSeat = _landlordSeat;
		fillCardIds(_bottomCards, msg.bottomCards);
		msg.multiplier = _multiplier;
		msg.currentPlayer = _currentPlayer;
		fillHandCounts(msg.handCounts);
		sendMessageToAll(msg);
	}

	void DouDiZhuRoom::notifyPlay(int seat, const std::vector<int>& cardIds, int genre, int nextPlayer) {
		MsgDouDiZhuPlayNotify msg;
		msg.seat = seat;
		msg.cardIds = cardIds;
		msg.genre = genre;
		msg.nextPlayer = nextPlayer;
		msg.multiplier = _multiplier;
		fillHandCounts(msg.handCounts);
		sendMessageToAll(msg);
	}

	void DouDiZhuRoom::notifySettlement(int winnerSeat) {
		MsgDouDiZhuSettlement msg;
		msg.winnerSeat = winnerSeat;
		msg.landlordSeat = _landlordSeat;
		msg.multiplier = _multiplier;
		msg.spring = _spring;
		for (int i = 0; i < 2; i++) {
			std::shared_ptr<DouDiZhuAvatar> avatar = getAvatar(i);
			if (!avatar)
				continue;
			msg.scores[i] = avatar->getRoundScore();
			msg.winGolds[i] = avatar->getWinGold();
			for (const PokerCard& c : avatar->getCards())
				msg.remainCards[i].push_back(c.getId());
		}
		if (_rule->getRoundCount() > 0 && _roundNo >= _rule->getRoundCount()) {
			std::vector<std::pair<std::string, int64_t>> netWins;
			for (int i = 0; i < 2; i++) {
				std::shared_ptr<DouDiZhuAvatar> avatar = getAvatar(i);
				if (!avatar)
					continue;
				netWins.emplace_back(avatar->getPlayerId(), avatar->getTotalScore());
			}
			calcRoomFeeSettlementData(_roomFee, netWins,
				msg.roomFeeTotal, msg.roomFeePlayerIds, msg.roomFeeAmounts);
			getShuffleFeeSettlementData(msg.shuffleFeeTotal,
				msg.shuffleFeePlayerIds, msg.shuffleFeeAmounts);
		}
		sendMessageToAll(msg);
	}
}
