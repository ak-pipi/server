// TaoJiangMahjongRoom.cpp

#include "Base/BaseUtils.h"
#include "Base/Log.h"
#include "Game/GetCapitalTask.h"
#include "MySql/MysqlPool.h"
#include "Venue/VenueInnerHandler.h"
#include "TaoJiangMahjongAvatar.h"
#include "TaoJiangMahjongRoom.h"
#include "TaoJiangMahjongMessages.h"
#include "TaoJiangMahjongPlayback.h"
#include "TaoJiangMahjongRecordTask.h"
#include "Game/GameMessages.h"
#include "Game/DebtLiquidation.h"
#include "Game/ReplayUtils.h"
#include "Player/PlayerManager.h"
#include "jsoncpp/include/json/json.h"

#include <sstream>
#include <zlib.h>

#include <mysql/jdbc.h>

namespace NiuMa
{
	/**
	 * 桃江麻将规则：无字牌（东南西北中发白），108张牌
	 */
	class TaoJiangMahjongRule : public MahjongRule
	{
	public:
		bool hasZiPai() const override { return false; }

		// 桃江麻将将牌必须是2、5、8数字的牌
		bool isValidJiangTile(const MahjongTile::Tile& tile) const override {
			MahjongTile::Number num = tile.getNumber();
			return (num == MahjongTile::Number::Er
				|| num == MahjongTile::Number::Wu
				|| num == MahjongTile::Number::Ba);
		}
	};

	TaoJiangMahjongRoom::TaoJiangMahjongRoom(const std::string& venueId, const std::string& number, int level, const std::string& ruleConfig)
		: MahjongRoom(std::make_shared<TaoJiangMahjongRule>(), venueId, static_cast<int>(GameType::TaoJiangMahjong), 2)
		, _number(number)
		, _level(level)
		, _roundState(StageState::NotStarted)
		, _disbandState(StageState::NotStarted)
		, _roundNo(0)
		, _backupBanker(0)
		, _disbander(0)
		, _disbandTick(0)
		, _diZhu(1)
		, _maxScore(0)
		, _roomFeeType(0)
		, _roundCount(8)
		, _allowChi(false)
		, _allowPeng(true)
		, _allowGang(true)
		, _allowZiMo(true)
		, _allowDianPao(true)
		, _laiziEnabled(false)
		, _hongzhongEnabled(false)
		, _bankerRule(0)
		, _dissolveVote(true)
	{
		_anGangVisible = true;
		_chi = _allowChi;
		_dianPao = _allowDianPao;

		for (int i = 0; i < 6; i++)
			_distances[i] = -1;
		for (int i = 0; i < 4; i++) {
			_disbandChoices[i] = 0;
			_kicks[i] = false;
		}

		// 解析玩法配置
		if (!ruleConfig.empty())
			parseRuleConfig(ruleConfig);

		// 押金数额为底注的50倍
		setCashPledge(_diZhu * 50);
	}

	TaoJiangMahjongRoom::~TaoJiangMahjongRoom()
	{}

	void TaoJiangMahjongRoom::parseRuleConfig(const std::string& ruleConfig) {
		Json::Reader reader;
		Json::Value root;
		if (!reader.parse(ruleConfig, root))
			return;

		if (root.isMember("base_score") && root["base_score"].isInt())
			_diZhu = root["base_score"].asInt();
		if (root.isMember("max_score") && root["max_score"].isInt())
			_maxScore = root["max_score"].asInt();
		if (root.isMember("room_fee_type") && root["room_fee_type"].isInt())
			_roomFeeType = root["room_fee_type"].asInt();
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
		if (root.isMember("laizi_enabled") && root["laizi_enabled"].isBool())
			_laiziEnabled = root["laizi_enabled"].asBool();
		if (root.isMember("hongzhong_enabled") && root["hongzhong_enabled"].isBool())
			_hongzhongEnabled = root["hongzhong_enabled"].asBool();
		if (root.isMember("banker_rule") && root["banker_rule"].isInt())
			_bankerRule = root["banker_rule"].asInt();
		if (root.isMember("dissolve_vote") && root["dissolve_vote"].isBool())
			_dissolveVote = root["dissolve_vote"].asBool();

		// 同步到MahjongRoom基类字段
		_chi = _allowChi;
		_dianPao = _allowDianPao;
	}

	GameAvatar::Ptr TaoJiangMahjongRoom::createAvatar(const std::string& playerId, int seat, bool robot) const {
		return std::make_shared<TaoJiangMahjongAvatar>(playerId, seat, robot);
	}

	void TaoJiangMahjongRoom::onAvatarLeaved(int seat, const std::string& playerId) {
		clearDistances(seat, _distances);
		if (getAvatarCount() == 0)
			gameOver();
	}

	bool TaoJiangMahjongRoom::checkEnter(const std::string& playerId, std::string& errMsg, bool robot) const {
		if (_roundState == StageState::Underway) {
			errMsg = "游戏正在进行中，不能进入房间";
			return false;
		}
		return true;
	}

	int TaoJiangMahjongRoom::checkLeave(const std::string& playerId, std::string& errMsg) const {
		if (_roundState == StageState::Underway) {
			errMsg = "游戏正在进行中，不能离开房间";
			return 1;
		}
		return 0;
	}

	void TaoJiangMahjongRoom::getAvatarExtraInfo(const GameAvatar::Ptr& avatar, std::string& base64) const {
		std::shared_ptr<GetCapitalTask> task = std::make_shared<GetCapitalTask>(avatar->getPlayerId());
		MysqlPool::getSingleton().syncQuery(task);
		int64_t gold = avatar->getCashPledge();
		int64_t diamond = 0LL;
		if (task->getSucceed() && task->getRows() > 0) {
			gold += task->getGold();
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

	void TaoJiangMahjongRoom::clean() {
		MahjongRoom::clean();

		for (int i = 0; i < 4; i++)
			_kicks[i] = false;
	}

	bool TaoJiangMahjongRoom::onMessage(const NetMessage::Ptr& netMsg) {
		if (MahjongRoom::onMessage(netMsg))
			return true;

		bool ret = true;
		const std::string& msgType = netMsg->getType();
		if (msgType == MsgTJSync::TYPE)
			onSyncMahjong(netMsg);
		else if (msgType == MsgPlayerReady::TYPE)
			onPlayerReady(netMsg);
		else if (msgType == MsgDisbandRequest::TYPE)
			onDisbandRequest(netMsg);
		else if (msgType == MsgDisbandChoose::TYPE)
			onDisbandChoose(netMsg);
		else
			ret = false;

		return ret;
	}

	void TaoJiangMahjongRoom::onTimer() {
		if (_disbandState == StageState::Underway) {
			time_t nowTick = BaseUtils::getCurrentMillisecond();
			int deltaTicks = static_cast<int>(nowTick - _disbandTick);
			if (deltaTicks > 300000) {
				// 超过300秒(5分钟)不选择默认同意解散
				for (int i = 0; i < getMaxPlayerNums(); i++) {
					if (_disbandChoices[i] == 0)
						doDisbandChoose(i, 1);
				}
			}
		}
	}

	void TaoJiangMahjongRoom::onSyncMahjong(const NetMessage::Ptr& netMsg) {
		MsgTJSync* inst = dynamic_cast<MsgTJSync*>(netMsg->getMessage().get());
		if (inst == nullptr)
			return;
		TaoJiangMahjongAvatar* avatar = dynamic_cast<TaoJiangMahjongAvatar*>(getAvatar(inst->getPlayerId()).get());
		if (avatar == nullptr)
			return;
		std::shared_ptr<GetCapitalTask> task = std::make_shared<GetCapitalTask>(inst->getPlayerId());
		MysqlPool::getSingleton().syncQuery(task);
		if (!task->getSucceed() || (task->getRows() < 1)) {
			ErrorS << "查询玩家资产失败，场地Id: " << getId() << ", 玩家Id: " << inst->getPlayerId();
			return;
		}
		MsgTJSyncResp msg;
		msg.number = _number;
		msg.gold = task->getGold();
		msg.diamond = task->getDiamond();
		msg.diZhu = _diZhu;
		msg.chi = _allowChi;
		msg.dianPao = _allowDianPao;
		msg.seat = avatar->getSeat();
		msg.roundState = static_cast<int>(_roundState);
		msg.disbandState = static_cast<int>(_disbandState);
		msg.banker = _banker;
		msg.leftTiles = _dealer.getTileLeft();
		TaoJiangMahjongAvatar* tmpAvatar = NULL;
		if (_roundState == StageState::Underway) {
			for (int i = 0; i < getMaxPlayerNums(); i++) {
				tmpAvatar = dynamic_cast<TaoJiangMahjongAvatar*>(getAvatar(i).get());
				if (tmpAvatar == NULL)
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

	void TaoJiangMahjongRoom::onPlayerReady(const NetMessage::Ptr& netMsg) {
		if (_roundState != StageState::NotStarted)
			return;
		MsgPlayerReady* inst = dynamic_cast<MsgPlayerReady*>(netMsg->getMessage().get());
		if (inst == nullptr)
			return;
		TaoJiangMahjongAvatar* avatar = dynamic_cast<TaoJiangMahjongAvatar*>(getAvatar(inst->getPlayerId()).get());
		if (avatar == nullptr)
			return;
		avatar->setReady(true);
		MsgPlayerReadyResp msg;
		msg.playerId = avatar->getPlayerId();
		msg.seat = avatar->getSeat();
		sendMessageToAll(msg);
		if (isFull() && isAllReady()) {
			// 所有人都已经准备好，开始一局
			startRound();
		}
	}

	void TaoJiangMahjongRoom::startRound() {
		// 开始一局
		_roundState = StageState::Underway;
		_backupBanker = _banker;

		clean();

		MsgTJStartRound msg;
		msg.banker = _banker;
		std::ostringstream os;
		os << "桃江麻将牌桌(Id:" << getId() << ")开局，各玩家Id: ";
		TaoJiangMahjongAvatar* avatar = nullptr;
		for (int i = 0; i < getMaxPlayerNums(); i++) {
			avatar = dynamic_cast<TaoJiangMahjongAvatar*>(getAvatar(i).get());
			if (avatar == nullptr)
				continue;
			if (i > 0)
				os << "、";
			os << avatar->getPlayerId();
			msg.send(avatar->getSession());
		}
		LOG_INFO(os.str());

		dealTiles();

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
					ss << "select max(`round_no`) from `game_taojiang_mahjong_record` where `venue_id` = \"" << _venueId << "\"";
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
		_roundNo++;
	}

	double* TaoJiangMahjongRoom::getDistances() {
		return _distances;
	}

	void TaoJiangMahjongRoom::getDistances(std::vector<int>& distances) const {
		// 两人模式只有一对距离
		distances.push_back(static_cast<int>(_distances[0]));
	}

	int TaoJiangMahjongRoom::getDistanceIndex(int seat1, int seat2) const {
		// 两人模式只有一对: 座位0-1
		if ((seat1 == 0 && seat2 == 1) || (seat1 == 1 && seat2 == 0))
			return 0;
		return -1;
	}

	void TaoJiangMahjongRoom::calcHuScore() const {
		// 流局不算分
		if (!_hu)
			return;
		int seat = 0;
		int score = 0;
		int scores[4] = { 0, 0, 0, 0 };
		TaoJiangMahjongAvatar* avatar1 = nullptr;
		TaoJiangMahjongAvatar* avatar2 = nullptr;
		MahjongChapter::Type chapterType = MahjongChapter::Type::Invalid;
		MahjongChapterArray::const_iterator it;
		for (int i = 0; i < getMaxPlayerNums(); i++) {
			avatar1 = dynamic_cast<TaoJiangMahjongAvatar*>(getAvatar(i).get());
			if (avatar1 == nullptr)
				continue;
			// 算胡分
			if (avatar1->isHu()) {
				score = avatar1->calcHuScore();
				// 封顶处理
				if (_maxScore > 0 && score > _maxScore)
					score = _maxScore;
				if (avatar1->isDianPao()) {
					// 点炮，放炮者一人承担
					avatar2 = dynamic_cast<TaoJiangMahjongAvatar*>(getAvatar(_actor).get());
					if (avatar2 != nullptr) {
						avatar1->addLoseScore(_actor, -score);
						avatar2->addLoseScore(i, score);
					}
				}
				else {
					// 自摸，其余玩家各承担一份
					for (int j = 0; j < getMaxPlayerNums(); j++) {
						if (i == j)
							continue;
						avatar2 = dynamic_cast<TaoJiangMahjongAvatar*>(getAvatar(j).get());
						if (avatar2 == nullptr)
							continue;
						avatar1->addLoseScore(j, -score);
						avatar2->addLoseScore(i, score);
					}
				}
			}
			// 算杠分
			const MahjongChapterArray& lstChapters = avatar1->getChapters();
			it = lstChapters.begin();
			while (it != lstChapters.end()) {
				if (it->isVetoed()) {
					++it;
					continue;
				}
				chapterType = it->getType();
				if (chapterType == MahjongChapter::Type::ZhiGang) {
					seat = it->getTargetPlayer();
					avatar2 = dynamic_cast<TaoJiangMahjongAvatar*>(getAvatar(seat).get());
					if (avatar2 != nullptr) {
						avatar1->addLoseScore(seat, -1);
						avatar2->addLoseScore(i, 1);
					}
				}
				else if (chapterType == MahjongChapter::Type::JiaGang) {
					for (int j = 0; j < getMaxPlayerNums(); j++) {
						if (i == j)
							continue;
						avatar2 = dynamic_cast<TaoJiangMahjongAvatar*>(getAvatar(j).get());
						if (avatar2 == nullptr)
							continue;
						avatar1->addLoseScore(j, -1);
						avatar2->addLoseScore(i, 1);
					}
				}
				else if (chapterType == MahjongChapter::Type::AnGang) {
					for (int j = 0; j < getMaxPlayerNums(); j++) {
						if (i == j)
							continue;
						avatar2 = dynamic_cast<TaoJiangMahjongAvatar*>(getAvatar(j).get());
						if (avatar2 == nullptr)
							continue;
						avatar1->addLoseScore(j, -2);
						avatar2->addLoseScore(i, 2);
					}
				}
				++it;
			}
		}
		int loseScores[4] = { 0 };
		for (int i = 0; i < getMaxPlayerNums(); i++) {
			avatar1 = dynamic_cast<TaoJiangMahjongAvatar*>(getAvatar(i).get());
			if (avatar1 == NULL)
				continue;
			avatar1->getLoseScores(loseScores);
			for (int j = 0; j < 4; j++) {
				if (i != j)
					scores[j] += loseScores[j];
			}
		}
		for (int i = 0; i < getMaxPlayerNums(); i++) {
			avatar1 = dynamic_cast<TaoJiangMahjongAvatar*>(getAvatar(i).get());
			if (avatar1 != NULL)
				avatar1->setScore(scores[i]);
		}
		// 使用DebtLiquidation清算多边债务
		bool test = false;
		double diZhu = _diZhu;
		double capital = 0.0f;
		DebtNode* node = NULL;
		std::unordered_map<int, DebtNode*> debtNet;
		for (int i = 0; i < getMaxPlayerNums(); i++) {
			avatar1 = dynamic_cast<TaoJiangMahjongAvatar*>(getAvatar(i).get());
			if (avatar1 == NULL)
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
			avatar1 = dynamic_cast<TaoJiangMahjongAvatar*>(getAvatar(it1->first).get());
			avatar1->getLoseScores(loseScores);
			for (int j = 0; j < 4; j++) {
				if (((it1->first) == j) || (loseScores[j] == 0))
					continue;
				it2 = debtNet.find(j);
				if (it2 != debtNet.end())
					node->tally((it2->second), diZhu * loseScores[j]);
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
		test = false;
		it1 = debtNet.begin();
		while (it1 != debtNet.end()) {
			node = it1->second;
			if (node->getCapital() < 0.0) {
				test = true;
				break;
			}
			++it1;
		}
		if (test) {
			dl.releaseDebtNet(debtNet);
			ErrorS << "清算之后存在负数结果，原始债务网：" << logDebt;
			return;
		}
		double winGold = 0.0;
		it1 = debtNet.begin();
		while (it1 != debtNet.end()) {
			avatar1 = dynamic_cast<TaoJiangMahjongAvatar*>(getAvatar(it1->first).get());
			node = it1->second;
			capital = node->getCapital();
			winGold = capital - static_cast<double>(avatar1->getCashPledge());
			avatar1->setWinGold(winGold);
			++it1;
		}
		dl.releaseDebtNet(debtNet);
	}

	void TaoJiangMahjongRoom::doJieSuan() {
		_roundState = StageState::NotStarted;

		MsgTJSettlement msg;
		getSettlementData(&(msg.data));

		double delta = 0.0;
		int64_t tmp = 0LL;
		int64_t cashPledge = 0LL;
		int64_t goldNeed = getCashPledge();
		bool test = true;
		GameAvatar::Ptr ptr;
		TaoJiangMahjongAvatar* avatar = NULL;
		std::shared_ptr<GetCapitalTask> task;
		for (int i = 0; i < getMaxPlayerNums(); i++) {
			ptr = getAvatar(i);
			avatar = dynamic_cast<TaoJiangMahjongAvatar*>(ptr.get());
			if (avatar == NULL)
				continue;
			delta = avatar->getWinGold();
			// 四舍五入
			delta = floor(delta + 0.5);
			avatar->setWinGold(delta);
			msg.winGolds[i] = static_cast<int>(delta);
			cashPledge = avatar->getCashPledge();
			cashPledge += msg.winGolds[i];
			test = true;
			if (msg.winGolds[i] != 0) {
				if (cashPledge < goldNeed) {
					test = deductCashPledge(ptr);
				}
				else {
					updateCashPledge(avatar->getPlayerId(), cashPledge);
				}
			}
			task = std::make_shared<GetCapitalTask>(avatar->getPlayerId());
			MysqlPool::getSingleton().syncQuery(task);
			if (task->getSucceed() && task->getRows() > 0)
				msg.golds[i] = task->getGold() + avatar->getCashPledge();
			if (!test)
				_kicks[i] = true;
		}
		for (int i = 0; i < getMaxPlayerNums(); i++) {
			avatar = dynamic_cast<TaoJiangMahjongAvatar*>(getAvatar(i).get());
			if (avatar == NULL)
				continue;
			msg.kick = _kicks[i];
			msg.send(avatar->getSession());
		}
	}

	void TaoJiangMahjongRoom::afterHu() {
		saveRoundRecord();

		GameAvatar::Ptr avatar;
		for (int i = 0; i < getMaxPlayerNums(); i++) {
			avatar = getAvatar(i);
			if (!avatar)
				continue;
			if (_kicks[i])
				kickAvatar(avatar);
			else
				avatar->setReady(false);
		}
	}

	void TaoJiangMahjongRoom::onDisbandRequest(const NetMessage::Ptr& netMsg) {
		if (!_dissolveVote)
			return;
		if (_disbandState == StageState::Underway)
			return;
		MsgDisbandRequest* inst = dynamic_cast<MsgDisbandRequest*>(netMsg->getMessage().get());
		if (inst == nullptr)
			return;
		TaoJiangMahjongAvatar* avatar = dynamic_cast<TaoJiangMahjongAvatar*>(getAvatar(inst->getPlayerId()).get());
		if (avatar == NULL)
			return;
		_disbander = avatar->getSeat();
		for (int i = 0; i < getMaxPlayerNums(); i++) {
			if (i == _disbander)
				_disbandChoices[i] = 1;
			else
				_disbandChoices[i] = 0;
		}
		_disbandState = StageState::Underway;
		_disbandTick = BaseUtils::getCurrentMillisecond();
		notifyDisbandVote(std::string(""));
	}

	void TaoJiangMahjongRoom::notifyDisbandVote(const std::string& playerId) {
		MsgTJDisbandVote msg;
		msg.disbander = _disbander;
		time_t nowTick = BaseUtils::getCurrentMillisecond();
		msg.elapsed = static_cast<int>((nowTick - _disbandTick) / 1000LL);

		for (int i = 0; i < getMaxPlayerNums(); i++)
			msg.choices[i] = _disbandChoices[i];
		if (playerId.empty())
			sendMessageToAll(msg);
		else
			sendMessage(msg, playerId);
	}

	void TaoJiangMahjongRoom::onDisbandChoose(const NetMessage::Ptr& netMsg) {
		MsgDisbandChoose* inst = dynamic_cast<MsgDisbandChoose*>(netMsg->getMessage().get());
		if (inst == nullptr)
			return;
		if ((inst->choice != 1) && (inst->choice != 2))
			return;
		TaoJiangMahjongAvatar* avatar = dynamic_cast<TaoJiangMahjongAvatar*>(getAvatar(inst->getPlayerId()).get());
		if (avatar != NULL)
			doDisbandChoose(avatar->getSeat(), inst->choice);
	}

	void TaoJiangMahjongRoom::doDisbandChoose(int seat, int choice) {
		if (seat < 0 || seat >= getMaxPlayerNums())
			return;
		int half = (getMaxPlayerNums() >> 1);
		int nums1 = 0;
		int nums2 = 0;
		_disbandChoices[seat] = choice;
		for (int i = 0; i < getMaxPlayerNums(); i++) {
			if (_disbandChoices[i] == 1)
				nums1++;
			else if (_disbandChoices[i] == 2)
				nums2++;
		}
		MsgDisbandChoice msg;
		msg.seat = seat;
		msg.choice = choice;
		sendMessageToAll(msg);

		if (nums1 > half)
			disbandRoom();
		else if (nums2 >= half)
			disbandObsolete();
	}

	void TaoJiangMahjongRoom::disbandRoom() {
		_disbandState = StageState::Finished;

		MsgDisband msg;
		sendMessageToAll(msg);

		_roundState = StageState::NotStarted;

		kickAllAvatars();

		gameOver();
	}

	void TaoJiangMahjongRoom::disbandObsolete() {
		_disbandState = StageState::NotStarted;

		MsgDisbandObsolete msg;
		sendMessageToAll(msg);
	}

	void TaoJiangMahjongRoom::saveRoundRecord() {
		std::shared_ptr<TaoJiangMahjongRecordTask> task = std::make_shared<TaoJiangMahjongRecordTask>();
		task->_venueId = getId();
		task->_roundNo = _roundNo;
		task->_banker = _backupBanker;
		TaoJiangMahjongPlaybackData data;
		getSettlementData(&(data.settlement));
		getPlaybackData(data);
		TaoJiangMahjongAvatar* avatar = nullptr;
		for (int i = 0; i < getMaxPlayerNums(); i++) {
			avatar = dynamic_cast<TaoJiangMahjongAvatar*>(getAvatar(i).get());
			if (avatar == nullptr)
				continue;
			task->_playerIds[i] = avatar->getPlayerId();
			task->_scores[i] = avatar->getScore();
			task->_winGolds[i] = static_cast<int>(avatar->getWinGold());
			data.winGolds[i] = task->_winGolds[i];
		}
		// 生成随机种子hash
		task->_randomSeedHash = ReplayUtils::generateSeedHash(getId(), _roundNo, _backupBanker);
		data.randomSeedHash = task->_randomSeedHash;

		msgpack::sbuffer sbuf;
		msgpack::pack(sbuf, data);
		uLongf srcLen = static_cast<uLongf>(sbuf.size());
		std::string base64;
		if (srcLen > 0) {
			uLongf dstLen = srcLen + 100;
			unsigned char* dstBuf = new unsigned char[dstLen];
			int ret = compress(dstBuf, &dstLen, reinterpret_cast<const unsigned char*>(sbuf.data()), srcLen);
			if (ret != Z_OK)
				LOG_ERROR("桃江麻将牌局回放数据压缩失败");
			else {
				if (!BaseUtils::encodeBase64(base64, reinterpret_cast<const char*>(dstBuf), static_cast<int>(dstLen))) {
					LOG_ERROR("桃江麻将牌局回放数据打包Base64失败");
				}
			}
			delete[] dstBuf;
		}
		task->_playback = base64;
		// 异步保存到数据库
		MysqlPool::getSingleton().asyncQuery(task);
	}
}
