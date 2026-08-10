// ChangShaMahjongRoom.cpp

#include "ChangShaMahjongRoom.h"
#include "ChangShaMahjongAvatar.h"
#include "ChangShaMahjongMessages.h"
#include "ChangShaMahjongRecordTask.h"
#include "Base/BaseUtils.h"
#include "Game/GetCapitalTask.h"
#include "Game/GameMessages.h"
#include "Game/ReplayUtils.h"
#include "Game/WalletEventTask.h"
#include "Game/DebtLiquidation.h"
#include "Game/RiskControlCollector.h"
#include "Game/RandomAuditLogger.h"
#include "Mahjong/MahjongTile.h"
#include "Mahjong/MahjongRule.h"
#include "Network/MsgSession.h"
#include "Base/Log.h"
#include "MySql/MysqlPool.h"

#include <json/json.h>
#include <mysql/jdbc.h>
#include <algorithm>
#include <cmath>
#include <map>
#include <sstream>
#include <unordered_map>

namespace NiuMa
{
	namespace
	{
		bool isNumberTile(const MahjongTile& mt) {
			int p = static_cast<int>(mt.getPattern()) - static_cast<int>(MahjongTile::Pattern::Tong);
			int n = static_cast<int>(mt.getNumber());
			return p >= 0 && p < 3 && n >= 1 && n <= 9;
		}

		bool is258(const MahjongTile::Tile& tile) {
			int n = static_cast<int>(tile.getNumber());
			return n == 2 || n == 5 || n == 8;
		}

		int tileKey(const MahjongTile& mt) {
			return static_cast<int>(mt.getPattern()) * 10 + static_cast<int>(mt.getNumber());
		}

		class ChangShaMahjongRule : public MahjongRule
		{
		public:
			bool hasZiPai() const override { return false; }

		protected:
			bool isValidJiangTile(const MahjongTile::Tile& tile) const override {
				return is258(tile);
			}
		};
	}

	ChangShaMahjongRoom::ChangShaMahjongRoom(const std::string& venueId, const std::string& number, int level, const std::string& ruleConfig)
		: MahjongRoom(std::make_shared<ChangShaMahjongRule>(), venueId, static_cast<int>(GameType::ChangShaMahjong), 4)
		, _number(number)
		, _level(level)
		, _roundState(StageState::NotStarted)
		, _disbandState(StageState::NotStarted)
		, _roundNo(0)
		, _backupBanker(0)
		, _disbander(-1)
		, _disbandTick(0)
		, _qiShouHuTriggered(false)
		, _qiShouHuSeat(-1)
		, _qiShouHuType(0)
		, _qiShouHuScore(0)
			, _birdMultiple(1)
			, _diZhu(1)
			, _maxScore(300)
			, _roomFee(0)
		, _roundCount(8)
		, _allowChi(true)
		, _allowPeng(true)
		, _allowGang(true)
		, _allowZiMo(true)
		, _allowDianPao(true)
		, _dissolveVote(true)
		, _bankerRule(0)
		, _maxFan(8)
		, _require258Jiang(true)
		, _queYiSeEnabled(true)
		, _banBanHuEnabled(true)
		, _daSiXiEnabled(true)
		, _liuLiuShunEnabled(true)
		, _jieJieGaoEnabled(true)
		, _sanTongEnabled(true)
		, _yiZhiHuaEnabled(true)
		, _zhongNiaoEnabled(true)
		, _birdCount(2)
		, _birdDouble(true)
		, _birdCapMax(true)
	{
		for (int i = 0; i < 6; i++)
			_distances[i] = 0.0;
			for (int i = 0; i < 4; i++) {
				_kicks[i] = false;
				_disbandChoices[i] = 0;
				_totalWinGolds[i] = 0;
			}
		parseRuleConfig(ruleConfig);
		_chi = _allowChi;
		_dianPao = _allowDianPao;
		_anGangVisible = true;
		setCashPledge(_diZhu * 8);
	}

	ChangShaMahjongRoom::~ChangShaMahjongRoom() {}

	void ChangShaMahjongRoom::parseRuleConfig(const std::string& ruleConfig) {
		if (ruleConfig.empty())
			return;
		Json::Value root;
		Json::CharReaderBuilder builder;
		Json::CharReader* reader = builder.newCharReader();
		std::string errs;
		if (!reader->parse(ruleConfig.c_str(), ruleConfig.c_str() + ruleConfig.size(), &root, &errs)) {
			delete reader;
			return;
		}
		delete reader;

		if (root.isMember("base_score") && root["base_score"].isInt())
			_diZhu = root["base_score"].asInt();
		else if (root.isMember("di_zhu") && root["di_zhu"].isInt())
			_diZhu = root["di_zhu"].asInt();
		if (root.isMember("max_score") && root["max_score"].isInt())
			_maxScore = root["max_score"].asInt();
			if (root.isMember("room_fee") && root["room_fee"].isInt64())
				_roomFee = root["room_fee"].asInt64();
			else if (root.isMember("room_fee") && root["room_fee"].isInt())
				_roomFee = root["room_fee"].asInt();
			else if (root.isMember("room_fee_type") && root["room_fee_type"].isInt())
				_roomFee = root["room_fee_type"].asInt();
		if (root.isMember("round_count") && root["round_count"].isInt())
			_roundCount = root["round_count"].asInt();
		if (root.isMember("allow_chi") && root["allow_chi"].isBool())
			_allowChi = root["allow_chi"].asBool();
		if (root.isMember("allow_peng") && root["allow_peng"].isBool())
			_allowPeng = root["allow_peng"].asBool();
		if (root.isMember("allow_gang") && root["allow_gang"].isBool())
			_allowGang = root["allow_gang"].asBool();
		if (root.isMember("allow_zimo") && root["allow_zimo"].isBool())
			_allowZiMo = root["allow_zimo"].asBool();
		if (root.isMember("allow_dianpao") && root["allow_dianpao"].isBool())
			_allowDianPao = root["allow_dianpao"].asBool();
		if (root.isMember("dissolve_vote") && root["dissolve_vote"].isBool())
			_dissolveVote = root["dissolve_vote"].asBool();
		if (root.isMember("banker_rule") && root["banker_rule"].isInt())
			_bankerRule = root["banker_rule"].asInt();
		if (root.isMember("max_fan") && root["max_fan"].isInt())
			_maxFan = root["max_fan"].asInt();
		if (root.isMember("require_258_jiang") && root["require_258_jiang"].isBool())
			_require258Jiang = root["require_258_jiang"].asBool();

		// 起手胡开关
		if (root.isMember("queyise_enabled") && root["queyise_enabled"].isBool())
			_queYiSeEnabled = root["queyise_enabled"].asBool();
		if (root.isMember("banbanhu_enabled") && root["banbanhu_enabled"].isBool())
			_banBanHuEnabled = root["banbanhu_enabled"].asBool();
		if (root.isMember("dasixi_enabled") && root["dasixi_enabled"].isBool())
			_daSiXiEnabled = root["dasixi_enabled"].asBool();
		if (root.isMember("liuliushun_enabled") && root["liuliushun_enabled"].isBool())
			_liuLiuShunEnabled = root["liuliushun_enabled"].asBool();
		if (root.isMember("jiejiegao_enabled") && root["jiejiegao_enabled"].isBool())
			_jieJieGaoEnabled = root["jiejiegao_enabled"].asBool();
		if (root.isMember("santong_enabled") && root["santong_enabled"].isBool())
			_sanTongEnabled = root["santong_enabled"].asBool();
		if (root.isMember("yizhihua_enabled") && root["yizhihua_enabled"].isBool())
			_yiZhiHuaEnabled = root["yizhihua_enabled"].asBool();

		// 中鸟配置
		if (root.isMember("zhongniao_enabled") && root["zhongniao_enabled"].isBool())
			_zhongNiaoEnabled = root["zhongniao_enabled"].asBool();
		if (root.isMember("bird_count") && root["bird_count"].isInt())
			_birdCount = root["bird_count"].asInt();
		if (root.isMember("bird_double") && root["bird_double"].isBool())
			_birdDouble = root["bird_double"].asBool();
		if (root.isMember("bird_cap_max") && root["bird_cap_max"].isBool())
			_birdCapMax = root["bird_cap_max"].asBool();

		_chi = _allowChi;
		_dianPao = _allowDianPao;
	}

	GameAvatar::Ptr ChangShaMahjongRoom::createAvatar(const std::string& playerId, int seat, bool robot) const {
		return std::make_shared<ChangShaMahjongAvatar>(playerId, seat, robot);
	}

	bool ChangShaMahjongRoom::checkEnter(const std::string& playerId, std::string& errMsg, bool robot) const {
		if (_roundState == StageState::Underway) {
			errMsg = "游戏正在进行中，不能进入房间";
			return false;
		}
		return true;
	}

	int ChangShaMahjongRoom::checkLeave(const std::string& playerId, std::string& errMsg) const {
		if (_roundState == StageState::Underway) {
			errMsg = "游戏正在进行中，不能离开房间";
			return 1;
		}
		return 0;
	}

	void ChangShaMahjongRoom::getAvatarExtraInfo(const GameAvatar::Ptr& avatar, std::string& base64) const {
		std::shared_ptr<GetCapitalTask> task = std::make_shared<GetCapitalTask>(avatar->getPlayerId());
		MysqlPool::getSingleton().syncQuery(task);
		int64_t gold = avatar->getCashPledge();
		int64_t diamond = 0LL;
		if (task->getSucceed() && task->getRows() > 0) {
			diamond = task->getDiamond();
		}
		Json::Value tmp(Json::objectValue);
		tmp["gold"] = static_cast<Json::Int64>(gold);
		tmp["diamond"] = static_cast<Json::Int64>(diamond);
		if (!avatar->isOffline()) {
			Session::Ptr session = avatar->getSession();
			if (session)
				tmp["ip"] = session->getRemoteIp();
		}
		std::string json = tmp.toStyledString();
		BaseUtils::encodeBase64(base64, json.data(), static_cast<int>(json.size()));
	}

	void ChangShaMahjongRoom::onAvatarLeaved(int seat, const std::string& playerId) {
		if (getAvatarCount() == 0)
			gameOver();
	}

	void ChangShaMahjongRoom::clean() {
		MahjongRoom::clean();
		_qiShouHuTriggered = false;
		_qiShouHuSeat = -1;
		_qiShouHuType = 0;
		_qiShouHuScore = 0;
		_birdTiles.clear();
		_birdHitSeats.clear();
		_birdMultiple = 1;
		for (int i = 0; i < 4; i++)
			_kicks[i] = false;
	}

	bool ChangShaMahjongRoom::canShuffleCardsBeforeNextRound(const std::string& playerId,
		int& nextRoundNo,
		int& roundCount,
		std::string& errMsg) const {
		nextRoundNo = _roundNo + 1;
		roundCount = _roundCount;
		if (!hasAvatar(playerId)) {
			errMsg = "你不在当前房间内";
			return false;
		}
		if (_roundNo <= 0) {
			errMsg = "首局开始前不能洗牌";
			return false;
		}
		if (_roundCount > 0 && _roundNo >= _roundCount) {
			errMsg = "全部对局已结束，不能洗牌";
			return false;
		}
		if (_roundState != StageState::NotStarted) {
			errMsg = "下局开始前才能洗牌";
			return false;
		}
		return true;
	}

	double* ChangShaMahjongRoom::getDistances() {
		return _distances;
	}

	void ChangShaMahjongRoom::getDistances(std::vector<int>& distances) const {
	}

	int ChangShaMahjongRoom::getDistanceIndex(int seat1, int seat2) const {
		if (seat1 > seat2) {
			int tmp = seat1;
			seat1 = seat2;
			seat2 = tmp;
		}
		// 4人距离矩阵索引
		int idx = 0;
		for (int i = 0; i < 4; i++) {
			for (int j = i + 1; j < 4; j++) {
				if (i == seat1 && j == seat2)
					return idx;
				idx++;
			}
		}
		return 0;
	}

	void ChangShaMahjongRoom::calcHuScore() const {
		if (!_hu)
			return;

		int score = 0;
		int scores[4] = { 0, 0, 0, 0 };
		ChangShaMahjongAvatar* avatar1 = nullptr;
		ChangShaMahjongAvatar* avatar2 = nullptr;
		for (int i = 0; i < getMaxPlayerNums(); i++) {
			avatar1 = dynamic_cast<ChangShaMahjongAvatar*>(getAvatar(i).get());
			if (avatar1 == nullptr || !avatar1->isHu())
				continue;

			const_cast<ChangShaMahjongRoom*>(this)->calculateBird(i);
			score = avatar1->calcHuScore() * _birdMultiple;
			if (_maxScore > 0 && score > _maxScore)
				score = _maxScore;

			if (avatar1->isDianPao()) {
				avatar2 = dynamic_cast<ChangShaMahjongAvatar*>(getAvatar(_actor).get());
				if (avatar2 != nullptr) {
					avatar1->addLoseScore(_actor, -score);
					avatar2->addLoseScore(i, score);
				}
			}
			else {
				for (int j = 0; j < getMaxPlayerNums(); j++) {
					if (i == j)
						continue;
					avatar2 = dynamic_cast<ChangShaMahjongAvatar*>(getAvatar(j).get());
					if (avatar2 == nullptr)
						continue;
					avatar1->addLoseScore(j, -score);
					avatar2->addLoseScore(i, score);
				}
			}
		}

		int loseScores[4] = { 0 };
		for (int i = 0; i < getMaxPlayerNums(); i++) {
			avatar1 = dynamic_cast<ChangShaMahjongAvatar*>(getAvatar(i).get());
			if (avatar1 == nullptr)
				continue;
			avatar1->getLoseScores(loseScores);
			for (int j = 0; j < 4; j++) {
				if (i != j)
					scores[j] += loseScores[j];
			}
		}
		for (int i = 0; i < getMaxPlayerNums(); i++) {
			avatar1 = dynamic_cast<ChangShaMahjongAvatar*>(getAvatar(i).get());
			if (avatar1 != nullptr)
				avatar1->setScore(scores[i]);
		}

		bool test = false;
		double capital = 0.0;
		DebtNode* node = nullptr;
		std::unordered_map<int, DebtNode*> debtNet;
		for (int i = 0; i < getMaxPlayerNums(); i++) {
			avatar1 = dynamic_cast<ChangShaMahjongAvatar*>(getAvatar(i).get());
			if (avatar1 == nullptr)
				continue;
			test = false;
			avatar1->getLoseScores(loseScores);
			for (int j = 0; j < 4; j++) {
				if ((i == j) || (loseScores[j] == 0))
					continue;
				test = true;
				break;
			}
			if (!test)
				continue;
			capital = static_cast<double>(avatar1->getCashPledge());
			node = new DebtNode(i, capital);
			debtNet.insert(std::make_pair(i, node));
		}
		std::unordered_map<int, DebtNode*>::const_iterator it1 = debtNet.begin();
		std::unordered_map<int, DebtNode*>::const_iterator it2;
		while (it1 != debtNet.end()) {
			node = it1->second;
			avatar1 = dynamic_cast<ChangShaMahjongAvatar*>(getAvatar(it1->first).get());
			avatar1->getLoseScores(loseScores);
			for (int j = 0; j < 4; j++) {
				if (((it1->first) == j) || (loseScores[j] == 0))
					continue;
				it2 = debtNet.find(j);
				if (it2 != debtNet.end())
					node->tally((it2->second), static_cast<double>(_diZhu) * loseScores[j]);
			}
			++it1;
		}
		std::string logDebt;
		DebtLiquidation dl;
		dl.printDebtNet(debtNet, logDebt);
		if (!dl(debtNet)) {
			dl.releaseDebtNet(debtNet);
			ErrorS << "清算结果不正确，原始债务网：" << logDebt;
			return;
		}
		it1 = debtNet.begin();
		while (it1 != debtNet.end()) {
			node = it1->second;
			if (node->getCapital() < 0.0) {
				dl.releaseDebtNet(debtNet);
				ErrorS << "清算之后存在负数结果，原始债务网：" << logDebt;
				return;
			}
			++it1;
		}
		it1 = debtNet.begin();
		while (it1 != debtNet.end()) {
			avatar1 = dynamic_cast<ChangShaMahjongAvatar*>(getAvatar(it1->first).get());
			node = it1->second;
			capital = node->getCapital();
			avatar1->setWinGold(capital - static_cast<double>(avatar1->getCashPledge()));
			++it1;
		}
		dl.releaseDebtNet(debtNet);
	}

	void ChangShaMahjongRoom::doJieSuan() {
		_roundState = StageState::NotStarted;
		const bool allRoundsFinished = (_roundCount > 0 && _roundNo >= _roundCount);

		MsgChangShaSettlement msg;
		getSettlementData(&(msg.data));
		msg.birdTiles = _birdTiles;
		msg.hitSeats = _birdHitSeats;
		msg.birdMultiple = _birdMultiple;
		msg.qiShouHuSeat = _qiShouHuSeat;
		msg.qiShouHuType = _qiShouHuType;
		msg.qiShouHuScore = _qiShouHuScore;

		double delta = 0.0;
		int64_t cashPledge = 0LL;
		bool test = true;
		GameAvatar::Ptr ptr;
		ChangShaMahjongAvatar* avatar = nullptr;
		for (int i = 0; i < getMaxPlayerNums(); i++) {
			ptr = getAvatar(i);
			avatar = dynamic_cast<ChangShaMahjongAvatar*>(ptr.get());
			if (avatar == nullptr)
				continue;
			delta = floor(avatar->getWinGold() + 0.5);
			avatar->setWinGold(delta);
			msg.winGolds[i] = static_cast<int>(delta);
			_totalWinGolds[i] += msg.winGolds[i];
			cashPledge = avatar->getCashPledge();
			cashPledge += msg.winGolds[i];
			if (cashPledge < 0)
				cashPledge = 0;
			test = true;
			if (cashPledge != avatar->getCashPledge()) {
				test = updateCashPledge(avatar->getPlayerId(), cashPledge);
				if (test)
					avatar->setCashPledge(cashPledge);
			}
			msg.golds[i] = avatar->getCashPledge();
			if (!test || avatar->getCashPledge() <= 0)
				_kicks[i] = true;
		}
		if (allRoundsFinished) {
			std::vector<std::pair<std::string, int64_t>> netWins;
			for (int i = 0; i < getMaxPlayerNums(); i++) {
				avatar = dynamic_cast<ChangShaMahjongAvatar*>(getAvatar(i).get());
				if (avatar == nullptr)
					continue;
				netWins.emplace_back(avatar->getPlayerId(), _totalWinGolds[i]);
			}
			calcRoomFeeSettlementData(_roomFee, netWins,
				msg.roomFeeTotal, msg.roomFeePlayerIds, msg.roomFeeAmounts);
			getShuffleFeeSettlementData(msg.shuffleFeeTotal,
				msg.shuffleFeePlayerIds, msg.shuffleFeeAmounts);
		}

		for (int i = 0; i < getMaxPlayerNums(); i++) {
			avatar = dynamic_cast<ChangShaMahjongAvatar*>(getAvatar(i).get());
			if (avatar == nullptr)
				continue;
			msg.kick = _kicks[i] && !allRoundsFinished;
			msg.send(avatar->getSession());
		}
	}

	void ChangShaMahjongRoom::afterHu() {
		saveRoundRecord();
		const bool allRoundsFinished = (_roundCount > 0 && _roundNo >= _roundCount);
		for (int i = 0; i < getMaxPlayerNums(); i++) {
			GameAvatar::Ptr avatar = getAvatar(i);
			if (!avatar)
				continue;
			if (_kicks[i] && !allRoundsFinished)
				kickAvatar(avatar);
			else
				avatar->setReady(false);
		}
		if (allRoundsFinished) {
			publishFinalRoomFee();
			kickAllAvatars();
			gameOver();
		}
	}

	void ChangShaMahjongRoom::onTimer() {
		if (_disbandState == StageState::Underway) {
			time_t nowTick = BaseUtils::getCurrentMillisecond();
			int deltaTicks = static_cast<int>(nowTick - _disbandTick);
			if (deltaTicks > 300000) {
				for (int i = 0; i < getMaxPlayerNums(); i++) {
					if (_disbandChoices[i] == 0)
						doDisbandChoose(i, 1);
				}
			}
		}
	}

	bool ChangShaMahjongRoom::onMessage(const NetMessage::Ptr& netMsg) {
		if (MahjongRoom::onMessage(netMsg))
			return true;

		bool ret = true;
		const std::string& type = netMsg->getType();
		if (type == MsgChangShaSync::TYPE)
			onSyncMahjong(netMsg);
		else if (type == MsgPlayerReady::TYPE)
			onPlayerReady(netMsg);
		else if (type == MsgDisbandRequest::TYPE)
			onDisbandRequest(netMsg);
		else if (type == MsgDisbandChoose::TYPE)
			onDisbandChoose(netMsg);
		else
			ret = false;
		return ret;
	}

	void ChangShaMahjongRoom::onSyncMahjong(const NetMessage::Ptr& netMsg) {
		MsgChangShaSync* inst = dynamic_cast<MsgChangShaSync*>(netMsg->getMessage().get());
		if (inst == nullptr)
			return;
		ChangShaMahjongAvatar* avatar = dynamic_cast<ChangShaMahjongAvatar*>(getAvatar(inst->getPlayerId()).get());
		if (avatar == nullptr)
			return;
		std::shared_ptr<GetCapitalTask> task = std::make_shared<GetCapitalTask>(inst->getPlayerId());
		MysqlPool::getSingleton().syncQuery(task);
		if (!task->getSucceed() || task->getRows() < 1) {
			ErrorS << "查询玩家资产失败，场地Id: " << getId() << ", 玩家Id: " << inst->getPlayerId();
			return;
		}

		MsgChangShaSyncResp msg;
		msg.number = _number;
		msg.gold = avatar->getCashPledge();
		msg.diamond = task->getDiamond();
		msg.diZhu = _diZhu;
		msg.chi = _allowChi;
		msg.dianPao = _allowDianPao;
		msg.seat = avatar->getSeat();
		msg.roundState = static_cast<int>(_roundState);
		msg.disbandState = static_cast<int>(_disbandState);
		msg.banker = _banker;
		msg.playerCount = getMaxPlayerNums();
		msg.roundNo = _roundNo;
		msg.roundCount = _roundCount;
		msg.leftTiles = _dealer.getTileLeft();
		msg.qiShouHuSeat = _qiShouHuSeat;
		msg.qiShouHuType = _qiShouHuType;
		msg.qiShouHuScore = _qiShouHuScore;
		msg.birdTiles = _birdTiles;
		msg.hitSeats = _birdHitSeats;
		msg.birdMultiple = _birdMultiple;

		ChangShaMahjongAvatar* tmpAvatar = nullptr;
		if (_roundState == StageState::Underway) {
			for (int i = 0; i < getMaxPlayerNums(); i++) {
				tmpAvatar = dynamic_cast<ChangShaMahjongAvatar*>(getAvatar(i).get());
				if (tmpAvatar == nullptr)
					continue;
				if (i == avatar->getSeat()) {
					msg.handTiles = tmpAvatar->getTiles();
					if ((i == _actor) && !_actions.empty()) {
						const MahjongAction& ma = _actions.back();
						if (ma.getType() == MahjongAction::Type::Fetch) {
							int tileId = tmpAvatar->getFetchedTileId();
							msg.hasFetch = true;
							msg.fetchTile.setId(tileId);
							_dealer.getTileById(msg.fetchTile);
						}
					}
				}
				msg.handTileNums[i] = tmpAvatar->getTileNums();
				tmpAvatar->getPlayedTilesNoAction(msg.playedTiles[i]);
				msg.chapters[i] = tmpAvatar->getChapters();
			}
		}
		msg.send(netMsg->getSession());
		sendAvatars(netMsg->getSession());
		if (_roundState == StageState::Underway) {
			notifyActorUpdated(inst->getPlayerId());
			if ((_state == StateMachine::Action) || (_state == StateMachine::Play))
				notifyWaitingAction(inst->getPlayerId());
			if (avatar->hasActionOption())
				notifyActionOptions(avatar);
			notifyTingTile(avatar);
			if (_disbandState == StageState::Underway)
				notifyDisbandVote(inst->getPlayerId());
		}
	}

	void ChangShaMahjongRoom::onPlayerReady(const NetMessage::Ptr& netMsg) {
		if (_roundState != StageState::NotStarted)
			return;
		if (_roundCount > 0 && _roundNo >= _roundCount)
			return;
		MsgPlayerReady* inst = dynamic_cast<MsgPlayerReady*>(netMsg->getMessage().get());
		if (inst == nullptr)
			return;
		ChangShaMahjongAvatar* avatar = dynamic_cast<ChangShaMahjongAvatar*>(getAvatar(inst->getPlayerId()).get());
		if (avatar == nullptr)
			return;
		avatar->setReady(true);
		MsgPlayerReadyResp msg;
		msg.playerId = avatar->getPlayerId();
		msg.seat = avatar->getSeat();
		sendMessageToAll(msg);
		if (isFull() && isAllReady())
			startRound();
	}

	void ChangShaMahjongRoom::startRound() {
		if (_roundCount > 0 && _roundNo >= _roundCount)
			return;
		if (_roundNo == 0) {
			class GetMaxRoundNoTask : public MysqlQueryTask {
			public:
				GetMaxRoundNoTask(const std::string& venueId)
					: _venueId(venueId)
					, _maxRoundNo(0)
				{}

				virtual ~GetMaxRoundNoTask() {}

			public:
				virtual QueryType buildQuery(std::string& sql) override {
					std::stringstream ss;
					ss << "select max(`round_no`) from `game_changsha_mahjong_record` where `venue_id` = \"" << _venueId << "\"";
					sql = ss.str();
					return QueryType::Select;
				}

				virtual int fetchResult(sql::ResultSet* res) override {
					int rows = 0;
					while (res->next()) {
						_maxRoundNo = res->getInt(1);
						rows++;
					}
					return rows;
				}

			public:
				const std::string _venueId;
				int _maxRoundNo;
			};
			std::shared_ptr<GetMaxRoundNoTask> task = std::make_shared<GetMaxRoundNoTask>(getId());
			MysqlPool::getSingleton().syncQuery(task);
			if (task->getSucceed() && task->getRows() > 0)
				_roundNo = task->_maxRoundNo;
		}
		if (_roundCount > 0 && _roundNo >= _roundCount)
			return;

		_roundState = StageState::Underway;
		_backupBanker = _banker;
		clean();
		_playbackData = ChangShaMahjongPlaybackData();
		_dealer.shuffle();
		_roundNo++;
		_qiShouHuTriggered = false;
		_qiShouHuSeat = -1;
		_qiShouHuType = 0;
		_qiShouHuScore = 0;
		_birdTiles.clear();
		_birdHitSeats.clear();
		_birdMultiple = 1;

		_riskCollector.startRound(getId(), static_cast<int>(GameType::ChangShaMahjong), _roundNo);
		for (int i = 0; i < 4; i++) {
			GameAvatar::Ptr av = getAvatar(i);
			if (av) {
				av->setReady(false);
				_riskCollector.recordPlayer(av->getPlayerId(), i);
			}
		}

		MsgChangShaStartRound msg;
		msg.banker = _banker;
		msg.playerCount = getMaxPlayerNums();
		msg.roundNo = _roundNo;
		msg.roundCount = _roundCount;
		msg.birdCount = _birdCount;
		msg.zhongNiaoEnabled = _zhongNiaoEnabled;
		msg.require258Jiang = _require258Jiang;

		std::ostringstream oss;
		oss << "长沙麻将牌桌(Id:" << getId() << ")开局，各玩家Id: ";
		for (int i = 0; i < getMaxPlayerNums(); i++) {
			ChangShaMahjongAvatar* avatar = dynamic_cast<ChangShaMahjongAvatar*>(getAvatar(i).get());
			if (avatar == nullptr)
				continue;
			if (i > 0)
				oss << "、";
			oss << avatar->getPlayerId();
			msg.send(avatar->getSession());
		}
		LOG_INFO(oss.str());

		dealTiles();
	}

	void ChangShaMahjongRoom::dealTiles() {
		MahjongAvatar* pAvatar = nullptr;
		MahjongTile mt;
		for (int i = 0; i < getMaxPlayerNums(); i++) {
			pAvatar = dynamic_cast<MahjongAvatar*>(getAvatar(i).get());
			if (pAvatar == nullptr)
				continue;
			MahjongTileArray& lstTiles = pAvatar->getTiles();
			lstTiles.clear();
			for (unsigned int j = 0; j < 13; j++) {
				_dealer.fetchTile(mt);
				lstTiles.push_back(mt);
			}
			pAvatar->sortTiles();
			pAvatar->backupDealedTiles();
			_rule->checkTingPai(pAvatar->getTiles(), pAvatar->getGangTiles(), pAvatar->getTingTiles(), pAvatar);
		}

		notifyDealTiles();
		for (int i = 0; i < getMaxPlayerNums(); i++) {
			pAvatar = dynamic_cast<MahjongAvatar*>(getAvatar(i).get());
			if (pAvatar == nullptr)
				continue;
			notifyTingTile(pAvatar);
		}

		updateCurrentActor(_banker);
		fetchTile();
		checkQiShouHu();
	}

	void ChangShaMahjongRoom::checkQiShouHu() {
		// 发牌后立即检测每个玩家是否有起手胡，长沙起手胡结算后继续本局。
		for (int i = 0; i < 4; i++) {
			GameAvatar::Ptr av = getAvatar(i);
			if (!av) continue;
			int type = detectQiShouHuType(i);
			if (type > 0) {
				_qiShouHuTriggered = true;
				auto avatar = std::dynamic_pointer_cast<ChangShaMahjongAvatar>(av);
				if (avatar) {
					avatar->setQiShouHuType(type);
					int score = getQiShouHuScore(type);
					avatar->setQiShouHuScore(score);
					if (_qiShouHuSeat < 0) {
						_qiShouHuSeat = i;
						_qiShouHuType = type;
						_qiShouHuScore = score;
					}
				}

				// 通知所有玩家
				MsgChangShaQiShouHu msg;
				msg.seat = i;
				msg.huType = type;
				msg.score = avatar ? avatar->getQiShouHuScore() : 0;
				sendMessageToAll(msg);

				// 执行起手胡结算
				// 胡牌者获得其他玩家赔付
				if (avatar) {
					int score = avatar->getQiShouHuScore();
					avatar->setWinGold(avatar->getWinGold() + score * 3.0);
					for (int j = 0; j < 4; j++) {
						if (j != i) {
							auto other = std::dynamic_pointer_cast<ChangShaMahjongAvatar>(getAvatar(j));
							if (other) {
								other->addLoseScore(i, score);
								other->setWinGold(other->getWinGold() - score);
							}
						}
					}
				}
			}
		}
	}

	int ChangShaMahjongRoom::getQiShouHuScore(int type) const {
		int multiple = 1;
		switch (type) {
		case 3: multiple = 4; break; // 四喜/大四喜
		case 4: // 六六顺
		case 5: // 节节高
		case 6: // 三同
			multiple = 2;
			break;
		default:
			multiple = 1;
			break;
		}
		int score = _diZhu * multiple;
		if (_maxScore > 0 && score > _maxScore)
			score = _maxScore;
		return score;
	}

	int ChangShaMahjongRoom::detectQiShouHuType(int seat) const {
		if (_queYiSeEnabled && checkQueYiSe(seat))
			return 1;
		if (_banBanHuEnabled && checkBanBanHu(seat))
			return 2;
		if (_daSiXiEnabled && checkDaSiXi(seat))
			return 3;
		if (_liuLiuShunEnabled && checkLiuLiuShun(seat))
			return 4;
		if (_jieJieGaoEnabled && checkJieJieGao(seat))
			return 5;
		if (_sanTongEnabled && checkSanTong(seat))
			return 6;
		if (_yiZhiHuaEnabled && checkYiZhiHua(seat))
			return 7;
		return 0;
	}

	bool ChangShaMahjongRoom::checkQueYiSe(int seat) const {
		auto avatar = std::dynamic_pointer_cast<MahjongAvatar>(getAvatar(seat));
		if (!avatar)
			return false;
		const MahjongTileArray& tiles = avatar->getTiles();
		int wanCount = 0, tiaoCount = 0, tongCount = 0;
		for (const auto& t : tiles) {
			auto p = t.getPattern();
			if (p == MahjongTile::Pattern::Wan) wanCount++;
			else if (p == MahjongTile::Pattern::Tiao) tiaoCount++;
			else if (p == MahjongTile::Pattern::Tong) tongCount++;
		}
		return (wanCount == 0 || tiaoCount == 0 || tongCount == 0);
	}

	bool ChangShaMahjongRoom::checkBanBanHu(int seat) const {
		auto avatar = std::dynamic_pointer_cast<MahjongAvatar>(getAvatar(seat));
		if (!avatar)
			return false;
		const MahjongTileArray& tiles = avatar->getTiles();
		for (const auto& t : tiles) {
			if (isNumberTile(t) && is258(t.getTile()))
				return false;
		}
		return true;
	}

	bool ChangShaMahjongRoom::checkDaSiXi(int seat) const {
		auto avatar = std::dynamic_pointer_cast<MahjongAvatar>(getAvatar(seat));
		if (!avatar)
			return false;
		const MahjongTileArray& tiles = avatar->getTiles();
		std::unordered_map<int, int> freqs;
		for (const auto& t : tiles) {
			if (isNumberTile(t))
				freqs[tileKey(t)]++;
		}
		for (const auto& kv : freqs) {
			if (kv.second >= 4)
				return true;
		}
		return false;
	}

	bool ChangShaMahjongRoom::checkLiuLiuShun(int seat) const {
		auto avatar = std::dynamic_pointer_cast<MahjongAvatar>(getAvatar(seat));
		if (!avatar)
			return false;
		const MahjongTileArray& tiles = avatar->getTiles();
		std::unordered_map<int, int> freqs;
		for (const auto& t : tiles) {
			int key = static_cast<int>(t.getPattern()) * 10 + static_cast<int>(t.getNumber());
			freqs[key]++;
		}
		int keZiCount = 0;
		for (const auto& kv : freqs) {
			if (kv.second >= 3)
				keZiCount++;
		}
		return keZiCount >= 2;
	}

	bool ChangShaMahjongRoom::checkJieJieGao(int seat) const {
		auto avatar = std::dynamic_pointer_cast<MahjongAvatar>(getAvatar(seat));
		if (!avatar)
			return false;
		const MahjongTileArray& tiles = avatar->getTiles();
		int counts[3][9] = { {0} };
		for (const auto& t : tiles) {
			if (!isNumberTile(t))
				continue;
			int p = static_cast<int>(t.getPattern()) - static_cast<int>(MahjongTile::Pattern::Tong);
			int n = static_cast<int>(t.getNumber()) - 1;
			counts[p][n]++;
		}
		for (int p = 0; p < 3; p++) {
			for (int n = 0; n <= 6; n++) {
				if (counts[p][n] >= 2 && counts[p][n + 1] >= 2 && counts[p][n + 2] >= 2)
					return true;
			}
		}
		return false;
	}

	bool ChangShaMahjongRoom::checkSanTong(int seat) const {
		auto avatar = std::dynamic_pointer_cast<MahjongAvatar>(getAvatar(seat));
		if (!avatar)
			return false;
		const MahjongTileArray& tiles = avatar->getTiles();
		int counts[3][9] = { {0} };
		for (const auto& t : tiles) {
			if (!isNumberTile(t))
				continue;
			int p = static_cast<int>(t.getPattern()) - static_cast<int>(MahjongTile::Pattern::Tong);
			int n = static_cast<int>(t.getNumber()) - 1;
			counts[p][n]++;
		}
		for (int n = 0; n < 9; n++) {
			if (counts[0][n] >= 2 && counts[1][n] >= 2 && counts[2][n] >= 2)
				return true;
		}
		return false;
	}

	bool ChangShaMahjongRoom::checkYiZhiHua(int seat) const {
		auto avatar = std::dynamic_pointer_cast<MahjongAvatar>(getAvatar(seat));
		if (!avatar)
			return false;
		const MahjongTileArray& tiles = avatar->getTiles();
		int jiangCount = 0;
		bool hasFive = false;
		for (const auto& t : tiles) {
			if (!isNumberTile(t))
				continue;
			if (is258(t.getTile())) {
				jiangCount++;
				if (static_cast<int>(t.getNumber()) == 5)
					hasFive = true;
			}
		}
		return jiangCount == 1 && hasFive;
	}

	void ChangShaMahjongRoom::calculateBird(int huSeat) {
		if (!_zhongNiaoEnabled || _birdCount <= 0)
			return;

		_birdTiles.clear();
		_birdHitSeats.clear();
		_birdMultiple = 1;

		// 从牌墙末尾翻出 birdCount 张牌作为鸟牌，按庄家为 1 顺位数鸟。
		for (int i = 0; i < _birdCount; i++) {
			MahjongTile mt;
			if (!_dealer.fetchTile1(mt))
				break;
			_birdTiles.push_back(mt.getId());

			int tileValue = static_cast<int>(mt.getNumber());
			if (tileValue < 1 || tileValue > 9)
				tileValue = 1;
			int hitOffset = (tileValue - 1) % 4;
			int hitSeat = (_banker + hitOffset) % 4;
			_birdHitSeats.push_back(hitSeat);
		}

		// 计算中鸟倍数
		int hitCount = 0;
		for (int seat : _birdHitSeats) {
			if (seat == huSeat)
				hitCount++;
		}

		if (_birdDouble) {
			_birdMultiple = 1;
			for (int i = 0; i < hitCount; i++)
				_birdMultiple *= 2;
		}
		else {
			_birdMultiple = 1 + hitCount;
		}

		if (_birdCapMax && _birdMultiple > _maxFan)
			_birdMultiple = _maxFan;

		notifyBird();
	}

	void ChangShaMahjongRoom::notifyBird() {
		MsgChangShaBird msg;
		msg.birdTiles = _birdTiles;
		msg.hitSeats = _birdHitSeats;
		msg.multiple = _birdMultiple;
		sendMessageToAll(msg);
	}

	void ChangShaMahjongRoom::onDisbandRequest(const NetMessage::Ptr& netMsg) {
		if (!_dissolveVote)
			return;
		if (_disbandState == StageState::Underway)
			return;
		MsgDisbandRequest* inst = dynamic_cast<MsgDisbandRequest*>(netMsg->getMessage().get());
		if (inst == nullptr)
			return;
		GameAvatar::Ptr av = getAvatar(inst->getPlayerId());
		if (av)
			_disbander = av->getSeat();
		if (_disbander < 0)
			return;

		_disbandState = StageState::Underway;
		_disbandTick = BaseUtils::getCurrentMillisecond();
		for (int i = 0; i < 4; i++)
			_disbandChoices[i] = 0;
		_disbandChoices[_disbander] = 1; // 发起者默认同意

		notifyDisbandVote(std::string(""));
	}

	void ChangShaMahjongRoom::notifyDisbandVote(const std::string& playerId) {
		MsgChangShaDisbandVote msg;
		msg.disbander = _disbander;
		time_t nowTick = BaseUtils::getCurrentMillisecond();
		int elapsed = static_cast<int>((nowTick - _disbandTick) / 1000LL);
		msg.remainTime = 300 - elapsed;
		if (msg.remainTime < 0)
			msg.remainTime = 0;
		for (int i = 0; i < 4; i++)
			msg.choices[i] = _disbandChoices[i];
		if (playerId.empty())
			sendMessageToAll(msg);
		else
			sendMessage(msg, playerId);
	}

	void ChangShaMahjongRoom::onDisbandChoose(const NetMessage::Ptr& netMsg) {
		MsgDisbandChoose* inst = dynamic_cast<MsgDisbandChoose*>(netMsg->getMessage().get());
		if (inst == nullptr)
			return;
		if ((inst->choice != 1) && (inst->choice != 2))
			return;
		GameAvatar::Ptr avatar = getAvatar(inst->getPlayerId());
		if (avatar)
			doDisbandChoose(avatar->getSeat(), inst->choice);
	}

	void ChangShaMahjongRoom::doDisbandChoose(int seat, int choice) {
		if (_disbandState != StageState::Underway)
			return;
		if (seat < 0 || seat >= 4)
			return;
		_disbandChoices[seat] = choice;

		// 检查投票结果
		int agree = 0, reject = 0;
		for (int i = 0; i < 4; i++) {
			if (_disbandChoices[i] == 1)
				agree++;
			else if (_disbandChoices[i] == 2)
				reject++;
		}
		MsgDisbandChoice msg;
		msg.seat = seat;
		msg.choice = choice;
		sendMessageToAll(msg);

		if (agree >= 3) {
			disbandRoom();
		}
		else if (reject >= 2) {
			disbandObsolete();
		}
	}

	void ChangShaMahjongRoom::disbandRoom() {
		_disbandState = StageState::Finished;
		if (_roundNo > 0) {
			MsgChangShaSettlement settlement;
			settlement.kick = false;
			std::vector<std::pair<std::string, int64_t>> netWins;
			for (int i = 0; i < getMaxPlayerNums(); i++) {
				auto avatar = std::dynamic_pointer_cast<ChangShaMahjongAvatar>(getAvatar(i));
				if (!avatar)
					continue;
				settlement.winGolds[i] = static_cast<int>(_totalWinGolds[i]);
				netWins.emplace_back(avatar->getPlayerId(), _totalWinGolds[i]);
			}
			calcRoomFeeSettlementData(_roomFee, netWins,
				settlement.roomFeeTotal, settlement.roomFeePlayerIds, settlement.roomFeeAmounts);
			getShuffleFeeSettlementData(settlement.shuffleFeeTotal,
				settlement.shuffleFeePlayerIds, settlement.shuffleFeeAmounts);
			sendMessageToAll(settlement);
		}
		MsgDisband msg;
		sendMessageToAll(msg);
		_roundState = StageState::NotStarted;
		publishFinalRoomFee();
		kickAllAvatars();
		gameOver();
	}

	void ChangShaMahjongRoom::disbandObsolete() {
		_disbandState = StageState::NotStarted;
		_disbander = -1;
		MsgDisbandObsolete msg;
		sendMessageToAll(msg);
	}

	void ChangShaMahjongRoom::saveRoundRecord() {
		auto task = std::make_shared<ChangShaMahjongRecordTask>();
		task->_venueId = getId();
		task->_roundNo = _roundNo;
		task->_banker = _backupBanker;

		getPlaybackData(_playbackData);
		_playbackData.qiShouHuSeat = _qiShouHuSeat;
		_playbackData.qiShouHuType = _qiShouHuType;
		_playbackData.birdTiles = _birdTiles;
		_playbackData.hitSeats = _birdHitSeats;
		_playbackData.birdMultiple = _birdMultiple;

		// 生成随机种子hash
		_playbackData.randomSeedHash = ReplayUtils::generateSeedHash(getId(), _roundNo, _backupBanker);
		task->_randomSeedHash = _playbackData.randomSeedHash;

		int idx = 0;
		for (int i = 0; i < 4; i++) {
			auto avatar = std::dynamic_pointer_cast<ChangShaMahjongAvatar>(getAvatar(i));
			if (avatar && idx < 4) {
				task->_playerIds[idx] = avatar->getPlayerId();
				task->_scores[idx] = avatar->getScore();
				task->_winGolds[idx] = avatar->getWinGold();
				_playbackData.scores[idx] = task->_scores[idx];
				_playbackData.winGolds[idx] = task->_winGolds[idx];
			}
			idx++;
		}

		// 序列化回放数据
		std::string replayData;
		msgpack::sbuffer sbuf;
		msgpack::pack(sbuf, _playbackData);
		ReplayUtils::compressReplay(sbuf.data(), static_cast<int>(sbuf.size()), replayData);
		task->_playback = replayData;

		MysqlPool::getSingleton().asyncQuery(task);

		// 风控采集：记录得分并结束
		_riskCollector.setRandomSeedHash(_playbackData.randomSeedHash);
		for (int i = 0; i < 4; i++) {
			auto avatar = std::dynamic_pointer_cast<ChangShaMahjongAvatar>(getAvatar(i));
			if (avatar)
				_riskCollector.recordScore(avatar->getPlayerId(), avatar->getScore(), static_cast<int64_t>(avatar->getWinGold()));
		}
		_riskCollector.finishRound();

		// 随机审计日志
		std::vector<int> cardOrder;
		std::vector<std::string> playerIds;
		for (int i = 0; i < 4; i++) {
			GameAvatar::Ptr av = getAvatar(i);
			if (av)
				playerIds.push_back(av->getPlayerId());
		}
		RandomAuditLogger::logAudit(getId(), static_cast<int>(GameType::ChangShaMahjong),
			_roundNo, _banker, _playbackData.randomSeedHash, cardOrder, playerIds);

		// 发送积分变动MQ事件
		for (int i = 0; i < 4; i++) {
			auto avatar = std::dynamic_pointer_cast<ChangShaMahjongAvatar>(getAvatar(i));
			if (!avatar)
				continue;
			double winGold = avatar->getWinGold();
			if (winGold > 0) {
				WalletEventTask::publish(
					avatar->getPlayerId(), "GAME_WIN", static_cast<int64_t>(winGold),
					"ChangShaMahjong", getId(), "长沙麻将赢得金币");
			}
			else if (winGold < 0) {
				WalletEventTask::publish(
					avatar->getPlayerId(), "GAME_LOSE", static_cast<int64_t>(-winGold),
					"ChangShaMahjong", getId(), "长沙麻将输掉金币");
			}
		}
	}

	void ChangShaMahjongRoom::publishFinalRoomFee() {
		if (_roundNo <= 0)
			return;
		std::vector<std::pair<std::string, int64_t>> netWins;
		for (int i = 0; i < getMaxPlayerNums(); i++) {
			GameAvatar::Ptr avatar = getAvatar(i);
			if (!avatar)
				continue;
			netWins.emplace_back(avatar->getPlayerId(), _totalWinGolds[i]);
		}
		publishRoomFeeOnGameOver(_roomFee, netWins, "ChangShaMahjong", "长沙麻将整场房费");
	}
}
