// PaoDeKuaiRoom.cpp

#include "PaoDeKuaiRoom.h"
#include "PaoDeKuaiAvatar.h"
#include "GameDefines.h"
#include "PaoDeKuaiMessages.h"
#include "Game/ReplayUtils.h"
#include "Game/WalletEventTask.h"
#include "Game/DebtLiquidation.h"
#include "PaoDeKuaiRecordTask.h"
#include "Game/RiskControlCollector.h"
#include "Game/RandomAuditLogger.h"
#include "Network/MsgSession.h"
#include "Base/Log.h"
#include "MySql/MysqlPool.h"

#include <json/json.h>
#include <algorithm>
#include <sstream>

namespace NiuMa
{
	PaoDeKuaiRoom::PaoDeKuaiRoom(const std::shared_ptr<PaoDeKuaiRule>& rule,
		const std::string& venueId,
		const std::string& number,
		int level,
		const std::string& ruleConfig)
		: GameRoom(venueId, static_cast<int>(GameType::PaoDeKuai), 2)
		, _rule(rule)
		, _dealer(rule)
		, _ruleConfig(ruleConfig)
		, _number(number)
		, _level(level)
		, _gameState(GameState::None)
		, _stateTime(0)
		, _roundNo(0)
		, _banker(0)
		, _currentPlayer(0)
		, _lastPlaySeat(-1)
		, _isFirstPlay(true)
		, _hasFirstPlayed(false)
		, _bombCount(0)
		, _autoPlayTime(0)
		, _dissolveRequested(false)
	{
		_dissolveVotes[0] = 0;
		_dissolveVotes[1] = 0;

		// 加载规则配置
		_rule->loadConfig(ruleConfig);
	}

	PaoDeKuaiRoom::~PaoDeKuaiRoom() {}

	GameAvatar::Ptr PaoDeKuaiRoom::createAvatar(const std::string& playerId, int seat, bool robot) const {
		return std::make_shared<PaoDeKuaiAvatar>(_rule, playerId, seat, robot);
	}

	bool PaoDeKuaiRoom::checkEnter(const std::string& playerId, std::string& errMsg, bool robot) const {
		if (getAvatarCount() >= _rule->getPlayerCount()) {
			errMsg = "房间已满";
			return false;
		}
		return true;
	}

	int PaoDeKuaiRoom::checkLeave(const std::string& playerId, std::string& errMsg) const {
		if (_gameState == GameState::Playing) {
			errMsg = "游戏进行中，无法离开";
			return 2; // 不能离开
		}
		return 0;
	}

	void PaoDeKuaiRoom::onAvatarLeaved(int seat, const std::string& playerId) {
		if (_gameState == GameState::Ready || _gameState == GameState::None) {
			// 重置状态
			if (getAvatarCount() == 0)
				setState(GameState::None);
		}
	}

	void PaoDeKuaiRoom::clean() {
		_gameState = GameState::None;
		_roundNo = 0;
		_currentPlayer = 0;
		_lastPlaySeat = -1;
		_isFirstPlay = true;
		_hasFirstPlayed = false;
		_bombCount = 0;
		_dissolveRequested = false;
		_dissolveVotes[0] = 0;
		_dissolveVotes[1] = 0;
		_playback = PaoDeKuaiPlaybackData();
  for (int _si = 0; _si < 2; _si++) {
  	auto avatar = std::dynamic_pointer_cast<PaoDeKuaiAvatar>(GameRoom::getAvatar(_si));
  	if (!avatar) continue;
			if (avatar)
				avatar->clear();
		}
	}

	void PaoDeKuaiRoom::onTimer() {
		time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
		if (_gameState == GameState::Playing) {
			// 检查托管超时
			if (_autoPlayTime > 0 && now >= _autoPlayTime) {
				autoPlay();
				_autoPlayTime = 0;
			}
		}
	}

	bool PaoDeKuaiRoom::onMessage(const NetMessage::Ptr& netMsg) {
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
  	if (!avatar) continue;
			if (avatar && !avatar->isReady())
				return false;
		}
		return true;
	}

	int PaoDeKuaiRoom::getNextSeat(int seat) const {
		return (seat + 1) % _rule->getPlayerCount();
	}

	int PaoDeKuaiRoom::determineFirstPlayer() const {
		// 首局随机选一个玩家出牌
		// 后续由上一局赢家先出
		// 如果配置了must_include_spade3，则持有黑桃3的玩家先出
		if (_rule->getMustIncludeSpade3()) {
   for (int _si = 0; _si < 2; _si++) {
   	auto avatar = std::dynamic_pointer_cast<PaoDeKuaiAvatar>(GameRoom::getAvatar(_si));
   	if (!avatar) continue;
				if (avatar && hasSpade3(_si))
					return _si;
			}
		}
		// 默认从座位0开始
		return 0;
	}

	bool PaoDeKuaiRoom::hasSpade3(int seat) const {
		auto avatar = getAvatar(seat);
		if (!avatar)
			return false;
		const CardArray& cards = avatar->getCards();
		for (auto& c : cards) {
			if (c.getPoint() == static_cast<int>(PokerPoint::Three) &&
				c.getSuit() == static_cast<int>(PokerSuit::Spade))
				return true;
		}
		return false;
	}

	bool PaoDeKuaiRoom::cardsContainSpade3(const std::vector<int>& cardIds) const {
		for (int id : cardIds) {
			PokerCard c;
			_dealer.getCard(c, id);
			if (c.getPoint() == static_cast<int>(PokerPoint::Three) &&
				c.getSuit() == static_cast<int>(PokerSuit::Spade))
				return true;
		}
		return false;
	}

	void PaoDeKuaiRoom::startRound() {
		_roundNo++;
		_bombCount = 0;
		_hasFirstPlayed = false;
		_playback = PaoDeKuaiPlaybackData();
		_playback.venueId = getId();
		_playback.roundNo = _roundNo;
		_playback.banker = _banker;
		_playback.playerCount = _rule->getPlayerCount();
		int idx = 0;
  for (int _si = 0; _si < 2; _si++) {
  	auto avatar = std::dynamic_pointer_cast<PaoDeKuaiAvatar>(GameRoom::getAvatar(_si));
  	if (!avatar) continue;
			_playback.playerIds[idx] = avatar->getPlayerId();
			idx++;
		}

		// 风控采集：开始新局
		_riskCollector.startRound(getId(), static_cast<int>(GameType::PaoDeKuai), _roundNo);
  for (int _si = 0; _si < 2; _si++) {
  	auto avatar = std::dynamic_pointer_cast<PaoDeKuaiAvatar>(GameRoom::getAvatar(_si));
  	if (!avatar) continue;
			_riskCollector.recordPlayer(avatar->getPlayerId(), _si);
		}

		// 清理玩家状态
  for (int _si = 0; _si < 2; _si++) {
  	auto avatar = std::dynamic_pointer_cast<PaoDeKuaiAvatar>(GameRoom::getAvatar(_si));
  	if (!avatar) continue;
			if (avatar) {
				avatar->clear();
				avatar->setRoundScore(0);
				avatar->setWinGold(0);
			}
		}

		// 洗牌发牌
		_dealer.shuffle();
		dealCards();

		// 确定首出玩家
		_currentPlayer = determineFirstPlayer();
		_lastPlaySeat = -1;
		_isFirstPlay = true;
		_lastPlayGenre.clear();

		setState(GameState::Playing);

		// 通知所有玩家
  for (int _si = 0; _si < 2; _si++) {
  	auto avatar = std::dynamic_pointer_cast<PaoDeKuaiAvatar>(GameRoom::getAvatar(_si));
  	if (!avatar) continue;
			notifyDeal(avatar->getPlayerId());
		}

		// 设置托管超时
		_autoPlayTime = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()) +
			_rule->getAutoPlayTimeout() / 1000;

		{ std::ostringstream oss; oss << "跑得快游戏开始，场地: " << getId() << "，局号: " << _roundNo; LOG_INFO(oss.str()); }
	}

	void PaoDeKuaiRoom::dealCards() {
		int playerCount = _rule->getPlayerCount();
		int cardCount = _rule->getCardCount();

		// 发牌：每人cardCount张
		std::vector<CardArray> heaps;
		_dealer.shuffle();
		_dealer.handOutCards(heaps, playerCount);

		// 将牌分配给各玩家
		int idx = 0;
  for (int _si = 0; _si < 2; _si++) {
  	auto avatar = std::dynamic_pointer_cast<PaoDeKuaiAvatar>(GameRoom::getAvatar(_si));
  	if (!avatar) continue;
			if (avatar && idx < static_cast<int>(heaps.size())) {
				// 取前cardCount张
				CardArray hand;
				int n = std::min(cardCount, static_cast<int>(heaps[idx].size()));
				for (int i = 0; i < n; i++)
					hand.push_back(heaps[idx][i]);
				avatar->setCards(hand);
				avatar->sortCards();

				// 记录初始手牌
				std::vector<int> ids;
				for (auto& c : hand)
					ids.push_back(c.getId());
				_playback.initCards[idx] = ids;
			}
			idx++;
		}
	}

	void PaoDeKuaiRoom::nextPlayer() {
		_currentPlayer = getNextSeat(_currentPlayer);

		// 如果轮回到上一次出牌的人，说明其他人都过牌了
		if (_currentPlayer == _lastPlaySeat) {
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

		// 首出必须包含黑桃3
		if (!_hasFirstPlayed && _rule->getMustIncludeSpade3() && !cardsContainSpade3(cardIds))
			return PlayResult::MustIncludeSpade3;

		// 压牌检查
		if (!_isFirstPlay) {
			int cmp = _rule->compareGenre(genre, _lastPlayGenre);
			if (cmp != 1) // 不是大于
				return PlayResult::CannotBeat;
		}

		return PlayResult::OK;
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

			// 通知
			notifyPlay(seat, cardIds, 0, getNextSeat(seat));
			nextPlayer();
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
		if (genre.getGenre() == static_cast<int>(PaoDeKuaiGenre::Bomb) ||
			genre.getGenre() == static_cast<int>(PaoDeKuaiGenre::Rocket)) {
			_bombCount++;
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
  	if (!avatar) continue;
			if (avatar && avatar->isFinished()) {
				winnerSeat = _si;
				break;
			}
		}
		if (winnerSeat < 0)
			return;

		calculateScores(winnerSeat);

		// 通知结算
		notifySettlement(winnerSeat);

		// 保存回放记录
		saveRoundRecord();

		// 发送积分变动MQ事件
  for (int _si = 0; _si < 2; _si++) {
  	auto avatar = std::dynamic_pointer_cast<PaoDeKuaiAvatar>(GameRoom::getAvatar(_si));
  	if (!avatar) continue;
			if (!avatar)
				continue;
			int64_t winGold = avatar->getWinGold();
			if (winGold > 0) {
				WalletEventTask::publish(
					avatar->getPlayerId(), "GAME_WIN", winGold,
					"PaoDeKuai", getId(), "跑得快赢得金币");
			}
			else if (winGold < 0) {
				WalletEventTask::publish(
					avatar->getPlayerId(), "GAME_LOSE", -winGold,
					"PaoDeKuai", getId(), "跑得快输掉金币");
			}
		}

		// 重置准备状态
  for (int _si = 0; _si < 2; _si++) {
  	auto avatar = std::dynamic_pointer_cast<PaoDeKuaiAvatar>(GameRoom::getAvatar(_si));
  	if (!avatar) continue;
			if (avatar)
				avatar->setReady(false);
		}

		// 更新庄家（赢家做庄）
		_banker = winnerSeat;

		setState(GameState::Ready);
	}

	void PaoDeKuaiRoom::calculateScores(int winnerSeat) {
		int baseScore = 1;
		// 炸弹翻倍
		if (_rule->getBombDouble()) {
			for (int i = 0; i < _bombCount; i++)
				baseScore *= 2;
		}
		// 加上炸弹额外分数
		int bombExtra = _bombCount * _rule->getBombScore();

		// 封顶
		int maxScore = _rule->getMaxRoundScore();
		int loserScore = baseScore + bombExtra;
		if (loserScore > maxScore)
			loserScore = maxScore;

  for (int _si = 0; _si < 2; _si++) {
  	auto avatar = std::dynamic_pointer_cast<PaoDeKuaiAvatar>(GameRoom::getAvatar(_si));
  	if (!avatar) continue;
			if (!avatar)
				continue;
			if (_si == winnerSeat) {
				avatar->setRoundScore(loserScore);
				avatar->setWinGold(static_cast<int64_t>(loserScore) * _level);
				avatar->setTotalScore(avatar->getTotalScore() + loserScore);
			}
			else {
				avatar->setRoundScore(-loserScore);
				avatar->setWinGold(-static_cast<int64_t>(loserScore) * _level);
				avatar->setTotalScore(avatar->getTotalScore() - loserScore);
			}
		}

		// 记录到回放
		_playback.scores[winnerSeat] = loserScore;
		int idx = 0;
  for (int _si = 0; _si < 2; _si++) {
  	auto avatar = std::dynamic_pointer_cast<PaoDeKuaiAvatar>(GameRoom::getAvatar(_si));
  	if (!avatar) continue;
			if (avatar) {
				_playback.scores[idx] = avatar->getRoundScore();
				_playback.winGolds[idx] = avatar->getWinGold();
			}
			idx++;
		}
	}

	void PaoDeKuaiRoom::saveRoundRecord() {
		auto task = std::make_shared<PaoDeKuaiRecordTask>();
		task->_venueId = getId();
		task->_roundNo = _roundNo;
		task->_banker = _banker;

		// 生成随机种子hash
		_playback.randomSeedHash = ReplayUtils::generateSeedHash(getId(), _roundNo, _banker);
		task->_randomSeedHash = _playback.randomSeedHash;

		int idx = 0;
  for (int _si = 0; _si < 2; _si++) {
  	auto avatar = std::dynamic_pointer_cast<PaoDeKuaiAvatar>(GameRoom::getAvatar(_si));
  	if (!avatar) continue;
			if (avatar) {
				task->_playerIds[idx] = avatar->getPlayerId();
				task->_scores[idx] = avatar->getRoundScore();
				task->_winGolds[idx] = avatar->getWinGold();
			}
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
  	if (!avatar) continue;
			if (avatar)
				_riskCollector.recordScore(avatar->getPlayerId(), avatar->getRoundScore(), avatar->getWinGold());
		}
		_riskCollector.finishRound();

		// 随机审计日志
		std::vector<int> cardOrder;
		std::vector<std::string> playerIds;
  for (int _si = 0; _si < 2; _si++) {
  	auto avatar = std::dynamic_pointer_cast<PaoDeKuaiAvatar>(GameRoom::getAvatar(_si));
  	if (!avatar) continue;
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
			// 首出：出最小的牌
			avatar->analyzeCombinations();
			avatar->candidateCombinations();
			PokerCombination::Ptr comb = avatar->getFirstCandidate();
			if (comb) {
				std::vector<int> cardIds;
				comb->getCards(cardIds);
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
			// 压牌：尝试出能大过的最小牌
			avatar->analyzeCombinations();
			avatar->candidateCombinations(_lastPlayGenre);
			if (avatar->hasCandidate()) {
				PokerCombination::Ptr comb = avatar->getFirstCandidate();
				if (comb) {
					std::vector<int> cardIds;
					comb->getCards(cardIds);
					doPlay(_currentPlayer, cardIds);
				}
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

	// 消息处理
	void PaoDeKuaiRoom::onSyncTable(const NetMessage::Ptr& netMsg) {
		auto msg = std::dynamic_pointer_cast<MsgPaoDeKuaiSync>(netMsg);
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

		// 查找玩家座位
  for (int _si = 0; _si < 2; _si++) {
  	auto avatar = std::dynamic_pointer_cast<PaoDeKuaiAvatar>(GameRoom::getAvatar(_si));
  	if (!avatar) continue;
			if (avatar->getPlayerId() == playerId) {
				resp->mySeat = _si;
				resp->currentPlayer = _currentPlayer;
				if (avatar) {
					const CardArray& cards = avatar->getCards();
					for (auto& c : cards)
						resp->myCards.push_back(c.getId());
				}
				break;
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
	}

	void PaoDeKuaiRoom::onReady(const NetMessage::Ptr& netMsg) {
		if (_gameState != GameState::None && _gameState != GameState::Ready)
			return;

		auto msg = std::dynamic_pointer_cast<MsgPaoDeKuaiReady>(netMsg);
		if (!msg)
			return;

  for (int _si = 0; _si < 2; _si++) {
  	auto avatar = std::dynamic_pointer_cast<PaoDeKuaiAvatar>(GameRoom::getAvatar(_si));
  	if (!avatar) continue;
			if (avatar->getPlayerId() == msg->getPlayerId()) {
				if (avatar)
					avatar->setReady(true);
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

		auto msg = std::dynamic_pointer_cast<MsgPaoDeKuaiPlay>(netMsg);
		if (!msg)
			return;

		// 查找玩家座位
		int seat = -1;
  for (int _si = 0; _si < 2; _si++) {
  	auto avatar = std::dynamic_pointer_cast<PaoDeKuaiAvatar>(GameRoom::getAvatar(_si));
  	if (!avatar) continue;
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
				case PlayResult::MustIncludeSpade3:
					resp->errMsg = "首出必须包含黑桃3";
					break;
				default:
					resp->errMsg = "出牌失败";
					break;
				}
				resp->send(session);
			}
		}
	}

	void PaoDeKuaiRoom::onDissolveVote(const NetMessage::Ptr& netMsg) {
		// 简化的解散投票处理
	}

	// 消息发送
	void PaoDeKuaiRoom::notifyDeal(const std::string& playerId) {
		int seat = -1;
  for (int _si = 0; _si < 2; _si++) {
  	auto avatar = std::dynamic_pointer_cast<PaoDeKuaiAvatar>(GameRoom::getAvatar(_si));
  	if (!avatar) continue;
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

		sendMessage(*msg, playerId);
	}

	void PaoDeKuaiRoom::notifyPlay(int seat, const std::vector<int>& cardIds, int genre, int nextPlayer) {
		auto msg = std::make_shared<MsgPaoDeKuaiPlayNotify>();
		msg->seat = seat;
		msg->cardIds = cardIds;
		msg->genre = genre;
		msg->nextPlayer = nextPlayer;
		sendMessageToAll(*msg);
	}

	void PaoDeKuaiRoom::notifySettlement(int winnerSeat) {
		auto msg = std::make_shared<MsgPaoDeKuaiSettlement>();
		msg->winnerSeat = winnerSeat;

		int idx = 0;
  for (int _si = 0; _si < 2; _si++) {
  	auto avatar = std::dynamic_pointer_cast<PaoDeKuaiAvatar>(GameRoom::getAvatar(_si));
  	if (!avatar) continue;
			if (avatar) {
				msg->scores[idx] = avatar->getRoundScore();
				msg->winGolds[idx] = avatar->getWinGold();
				// 剩余手牌
				const CardArray& cards = avatar->getCards();
				for (auto& c : cards)
					msg->remainCards[idx].push_back(c.getId());
			}
			idx++;
		}

		sendMessageToAll(*msg);
	}

	void PaoDeKuaiRoom::notifyGameState(const std::string& playerId) {
		// 复用sync消息通知状态
	}
}
