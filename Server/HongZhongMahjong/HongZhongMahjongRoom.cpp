// HongZhongMahjongRoom.cpp

#include "Base/BaseUtils.h"
#include "Base/Log.h"
#include "Game/GetCapitalTask.h"
#include "MySql/MysqlPool.h"
#include "Venue/VenueInnerHandler.h"
#include "HongZhongMahjongAvatar.h"
#include "HongZhongMahjongRoom.h"
#include "HongZhongMahjongRule.h"
#include "HongZhongMahjongMessages.h"
#include "HongZhongMahjongPlayback.h"
#include "HongZhongMahjongRecordTask.h"
#include "Game/GameMessages.h"
#include "Game/DebtLiquidation.h"
#include "Game/ReplayUtils.h"
#include "Player/PlayerManager.h"
#include "jsoncpp/include/json/json.h"

#include <sstream>
#include <algorithm>
#include <cmath>
#include <zlib.h>

#include <mysql/jdbc.h>

namespace NiuMa
{
	HongZhongMahjongRoom::HongZhongMahjongRoom(const std::string& venueId, const std::string& number, int level, const std::string& ruleConfig)
		: MahjongRoom(std::make_shared<HongZhongMahjongRule>(), venueId, static_cast<int>(GameType::HongZhongMahjong), resolvePlayerCount(ruleConfig))
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
		, _qiduiEnabled(true)
		, _pengpenghuEnabled(true)
		, _zimoDouble(true)
		, _dissolveVote(true)
		, _bankerRule(0)
		, _tilePool{ 0 }
		, _tileStart(0)
		, _tileEnd(0)
		, _birdTileId(MahjongTile::INVALID_ID)
		, _birdMultiplier(1)
	{
		_anGangVisible = true;
		_chi = false;
		_dianPao = _allowDianPao;
		_allDianPao = false;

		for (int i = 0; i < 6; i++)
			_distances[i] = -1;
		for (int i = 0; i < 4; i++) {
			_disbandChoices[i] = 0;
			_kicks[i] = false;
		}

		if (!ruleConfig.empty())
			parseRuleConfig(ruleConfig);

		_allowChi = false;
		_chi = false;
		_dianPao = _allowDianPao;
		setCashPledge(_diZhu * 50);
	}

	HongZhongMahjongRoom::~HongZhongMahjongRoom()
	{}

	int HongZhongMahjongRoom::resolvePlayerCount(const std::string&) {
		return 2;
	}

	void HongZhongMahjongRoom::parseRuleConfig(const std::string& ruleConfig) {
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
		if (root.isMember("qidui_enabled") && root["qidui_enabled"].isBool())
			_qiduiEnabled = root["qidui_enabled"].asBool();
		if (root.isMember("pengpenghu_enabled") && root["pengpenghu_enabled"].isBool())
			_pengpenghuEnabled = root["pengpenghu_enabled"].asBool();
		if (root.isMember("zimo_double") && root["zimo_double"].isBool())
			_zimoDouble = root["zimo_double"].asBool();
		if (root.isMember("banker_rule") && root["banker_rule"].isInt())
			_bankerRule = root["banker_rule"].asInt();
		if (root.isMember("dissolve_vote") && root["dissolve_vote"].isBool())
			_dissolveVote = root["dissolve_vote"].asBool();

		_allowChi = false;
		_dianPao = _allowDianPao;
		_chi = false;
	}

	GameAvatar::Ptr HongZhongMahjongRoom::createAvatar(const std::string& playerId, int seat, bool robot) const {
		return std::make_shared<HongZhongMahjongAvatar>(playerId, seat, robot);
	}

	void HongZhongMahjongRoom::onAvatarLeaved(int seat, const std::string& playerId) {
		clearDistances(seat, _distances);
		if (getAvatarCount() == 0)
			gameOver();
	}

	bool HongZhongMahjongRoom::checkEnter(const std::string& playerId, std::string& errMsg, bool robot) const {
		if (_roundState == StageState::Underway) {
			errMsg = "游戏正在进行中，不能进入房间";
			return false;
		}
		return true;
	}

	int HongZhongMahjongRoom::checkLeave(const std::string& playerId, std::string& errMsg) const {
		if (_roundState == StageState::Underway) {
			errMsg = "游戏正在进行中，不能离开房间";
			return 1;
		}
		return 0;
	}

	void HongZhongMahjongRoom::getAvatarExtraInfo(const GameAvatar::Ptr& avatar, std::string& base64) const {
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

	void HongZhongMahjongRoom::clean() {
		MahjongRoom::clean();
		for (int i = 0; i < 4; i++)
			_kicks[i] = false;
		_birdTileId = MahjongTile::INVALID_ID;
		_birdMultiplier = 1;
	}

	bool HongZhongMahjongRoom::onMessage(const NetMessage::Ptr& netMsg) {
		if (MahjongRoom::onMessage(netMsg))
			return true;

		bool ret = true;
		const std::string& msgType = netMsg->getType();
		if (msgType == MsgHZSync::TYPE)
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

	void HongZhongMahjongRoom::onTimer() {
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

	void HongZhongMahjongRoom::onSyncMahjong(const NetMessage::Ptr& netMsg) {
		MsgHZSync* inst = dynamic_cast<MsgHZSync*>(netMsg->getMessage().get());
		if (inst == nullptr)
			return;
		HongZhongMahjongAvatar* avatar = dynamic_cast<HongZhongMahjongAvatar*>(getAvatar(inst->getPlayerId()).get());
		if (avatar == nullptr)
			return;
		std::shared_ptr<GetCapitalTask> task = std::make_shared<GetCapitalTask>(inst->getPlayerId());
		MysqlPool::getSingleton().syncQuery(task);
		if (!task->getSucceed() || (task->getRows() < 1)) {
			ErrorS << "查询玩家资产失败，场地Id: " << getId() << ", 玩家Id: " << inst->getPlayerId();
			return;
		}
		MsgHZSyncResp msg;
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
		msg.playerCount = getMaxPlayerNums();
		msg.roundNo = _roundNo;
		msg.roundCount = _roundCount;
		msg.leftTiles = getHongZhongTileLeft();
		HongZhongMahjongAvatar* tmpAvatar = NULL;
		if (_roundState == StageState::Underway) {
			for (int i = 0; i < getMaxPlayerNums(); i++) {
				tmpAvatar = dynamic_cast<HongZhongMahjongAvatar*>(getAvatar(i).get());
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

	void HongZhongMahjongRoom::onPlayerReady(const NetMessage::Ptr& netMsg) {
		if (_roundState != StageState::NotStarted)
			return;
		if (_roundCount > 0 && _roundNo >= _roundCount)
			return;
		MsgPlayerReady* inst = dynamic_cast<MsgPlayerReady*>(netMsg->getMessage().get());
		if (inst == nullptr)
			return;
		HongZhongMahjongAvatar* avatar = dynamic_cast<HongZhongMahjongAvatar*>(getAvatar(inst->getPlayerId()).get());
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

	void HongZhongMahjongRoom::startRound() {
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
					ss << "select max(`round_no`) from `game_hongzhong_mahjong_record` where `venue_id` = \"" << _venueId << "\"";
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
		shuffleHongZhongTiles();
		_roundNo++;

		MsgHZStartRound msg;
		msg.banker = _banker;
		msg.playerCount = getMaxPlayerNums();
		msg.roundNo = _roundNo;
		msg.roundCount = _roundCount;
		std::ostringstream os;
		os << "红中麻将牌桌(Id:" << getId() << ")开局，各玩家Id: ";
		HongZhongMahjongAvatar* avatar = nullptr;
		for (int i = 0; i < getMaxPlayerNums(); i++) {
			avatar = dynamic_cast<HongZhongMahjongAvatar*>(getAvatar(i).get());
			if (avatar == nullptr)
				continue;
			if (i > 0)
				os << "、";
			os << avatar->getPlayerId();
			msg.send(avatar->getSession());
		}
		LOG_INFO(os.str());

		dealTiles();
	}

	double* HongZhongMahjongRoom::getDistances() {
		return _distances;
	}

	void HongZhongMahjongRoom::getDistances(std::vector<int>& distances) const {
		for (int i = 0; i < 6; i++)
			distances.push_back(static_cast<int>(_distances[i]));
	}

	int HongZhongMahjongRoom::getDistanceIndex(int seat1, int seat2) const {
		static const int RULER_TABLE[6] = { 0x01, 0x02, 0x03, 0x12, 0x13, 0x23 };
		int i1 = seat1;
		int i2 = seat2;
		if (seat1 > seat2) {
			i1 = seat2;
			i2 = seat1;
		}
		int i3 = (i1 << 4) | i2;
		int ret = -1;
		for (int j = 0; j < 6; j++) {
			if (i3 == RULER_TABLE[j]) {
				ret = j;
				break;
			}
		}
		return ret;
	}

	void HongZhongMahjongRoom::shuffleHongZhongTiles() {
		std::vector<int> ids;
		ids.reserve(112);
		for (int i = 0; i < 108; i++)
			ids.push_back(i);
		for (int i = 124; i < 128; i++)
			ids.push_back(i);

		for (int i = 0; i < 112; i++) {
			int index = BaseUtils::randInt(0, static_cast<int>(ids.size()));
			_tilePool[i] = ids[index];
			ids.erase(ids.begin() + index);
		}
		_tileStart = 0;
		_tileEnd = 112;
		_birdTileId = MahjongTile::INVALID_ID;
		_birdMultiplier = 1;
	}

	bool HongZhongMahjongRoom::fetchHongZhongTile(MahjongTile& mt, bool bBack) {
		if (_tileStart >= _tileEnd)
			return false;
		if (bBack)
			mt.setId(_tilePool[--_tileEnd]);
		else
			mt.setId(_tilePool[_tileStart++]);
		return MahjongDealer::getTileById(mt);
	}

	bool HongZhongMahjongRoom::fetchHongZhongTile(MahjongTile& mt, const std::string& tileName) {
		MahjongTile::Tile tile = MahjongTile::Tile::fromString(tileName);
		if (!tile.isValid())
			return false;
		MahjongTile target;
		target.setTile(tile);
		if (!MahjongDealer::getIdByTile(target))
			return false;
		if (!HongZhongMahjongRule::isHongZhong(target) && !target.getTile().isNumbered())
			return false;

		int found = -1;
		for (int i = _tileStart; i < _tileEnd; i++) {
			mt.setId(_tilePool[i]);
			if (!MahjongDealer::getTileById(mt))
				continue;
			if (mt.isSame(target)) {
				found = i;
				break;
			}
		}
		if (found < 0)
			return false;
		for (int i = found; i + 1 < _tileEnd; i++)
			_tilePool[i] = _tilePool[i + 1];
		_tileEnd--;
		return true;
	}

	int HongZhongMahjongRoom::getHongZhongTileLeft() const {
		return _tileEnd - _tileStart;
	}

	bool HongZhongMahjongRoom::earlyTermination() const {
		return _tilesLeft >= getHongZhongTileLeft();
	}

	void HongZhongMahjongRoom::dealTiles() {
		if (_tileEnd <= _tileStart)
			shuffleHongZhongTiles();

		HongZhongMahjongAvatar* avatar = nullptr;
		MahjongTile mt;
		for (int i = 0; i < getMaxPlayerNums(); i++) {
			avatar = dynamic_cast<HongZhongMahjongAvatar*>(getAvatar(i).get());
			if (avatar == nullptr)
				continue;
			MahjongTileArray& tiles = avatar->getTiles();
			tiles.clear();
			for (int j = 0; j < 13; j++) {
				if (fetchHongZhongTile(mt))
					tiles.push_back(mt);
			}
			avatar->sortTiles();
			avatar->backupDealedTiles();
			_rule->checkTingPai(avatar->getTiles(), avatar->getGangTiles(), avatar->getTingTiles(), avatar);
		}

		notifyDealTiles();
		for (int i = 0; i < getMaxPlayerNums(); i++) {
			avatar = dynamic_cast<HongZhongMahjongAvatar*>(getAvatar(i).get());
			if (avatar != nullptr)
				notifyTingTile(avatar);
		}

		HongZhongMahjongAvatar* openingHu = findOpeningFourHongZhongAvatar();
		if (openingHu != nullptr) {
			updateCurrentActor(openingHu->getSeat());
			doOpeningFourHongZhongHu(openingHu);
			return;
		}

		updateCurrentActor(_banker);
		fetchTile();
	}

	bool HongZhongMahjongRoom::fetchTile(bool bBack) {
		if (earlyTermination()) {
			noMoreTile();
			return false;
		}
		HongZhongMahjongAvatar* avatar = dynamic_cast<HongZhongMahjongAvatar*>(getAvatar(_actor).get());
		if (avatar == nullptr)
			return false;

		MahjongTile mt;
		bool testTile = false;
		const std::string& nextTile = avatar->getNextTile();
		if (!nextTile.empty()) {
			testTile = fetchHongZhongTile(mt, nextTile);
			avatar->setNextTile(std::string(""));
		}
		if (!testTile && !fetchHongZhongTile(mt, bBack))
			return false;

		changeState(StateMachine::Fetched);
		if (avatar->canHu(mt)) {
			int id = _acOpIdAlloc.askForId();
			if (id < ACTION_OPTION_POOL_SIZE) {
				_acOpPool[id].setType(MahjongAction::Type::ZiMo);
				_acOpPool[id].setId(id);
				_acOpPool[id].setPlayer(_actor);
				_acOpPool[id].setTileId1(mt.getId());
				_acOps1[0].push_back(id);
				avatar->addActionOption(id);
			}
		}
		avatar->fetchTile(mt);

		MahjongAction ma(MahjongAction::Type::Fetch, (static_cast<int>(_actors.size()) - 1), mt.getId());
		_actions.push_back(ma);
		notifyFetchTile(avatar, bBack);
		afterFetchChiPeng(avatar, mt.getId());
		return true;
	}

	void HongZhongMahjongRoom::notifyFetchTile(MahjongAvatar* avatar, bool bBack) {
		if (avatar == nullptr)
			return;
		MsgFetchTile msg;
		msg.player = _actor;
		msg.back = bBack;
		msg.nums = getHongZhongTileLeft();
		msg.tile.setId(avatar->getFetchedTileId());
		MahjongDealer::getTileById(msg.tile);
		msg.send(avatar->getSession());

		msg.tile = MahjongTile();
		sendMessageToAll(msg, avatar->getPlayerId());
	}

	bool HongZhongMahjongRoom::hasHongZhongInHand(MahjongAvatar* avatar) const {
		if (avatar == nullptr)
			return false;
		return HongZhongMahjongRule::countHongZhong(avatar->getTiles()) > 0;
	}

	bool HongZhongMahjongRoom::isCheckingQiangGang() const {
		if (_actions.empty())
			return false;
		return _actions.back().getType() == MahjongAction::Type::JiaGang;
	}

	bool HongZhongMahjongRoom::shouldAllowDianPaoForAvatar(MahjongAvatar* avatar, const MahjongTile&) const {
		if (isCheckingQiangGang())
			return true;
		return !hasHongZhongInHand(avatar);
	}

	bool HongZhongMahjongRoom::canCreateDianPaoOption(MahjongAvatar* avatar, const MahjongTile& mt, std::string& passed) const {
		if (avatar == nullptr)
			return false;
		return canDianPao() && avatar->canHu(mt) && avatar->canDianPao(mt, passed) && shouldAllowDianPaoForAvatar(avatar, mt);
	}

	HongZhongMahjongAvatar* HongZhongMahjongRoom::findOpeningFourHongZhongAvatar() const {
		for (int i = 0; i < getMaxPlayerNums(); i++) {
			HongZhongMahjongAvatar* avatar = dynamic_cast<HongZhongMahjongAvatar*>(getAvatar(i).get());
			if (avatar != nullptr && HongZhongMahjongRule::countHongZhong(avatar->getTiles()) >= 4)
				return avatar;
		}
		return nullptr;
	}

	void HongZhongMahjongRoom::doOpeningFourHongZhongHu(HongZhongMahjongAvatar* avatar) {
		if (avatar == nullptr)
			return;
		clearActionOptions();
		changeState(StateMachine::End);
		_hu = true;

		MahjongTile huTile;
		HongZhongMahjongRule::getHongZhongTile(huTile);
		for (const MahjongTile& mt : avatar->getTiles()) {
			if (HongZhongMahjongRule::isHongZhong(mt)) {
				huTile = mt;
				break;
			}
		}
		_huTileId = huTile.getId();
		avatar->addZiMo();
		avatar->addHuTimes();
		avatar->addHuWay(MahjongGenre::HuWay::ZiMo);
		avatar->setHuContext(static_cast<int>(MahjongGenre::HuStyle::PingHu), HongZhongMahjongRule::countHongZhong(avatar->getTiles()));

		notifyHuTile();
		notifyShowTiles();
		calcHuScore();
		doJieSuan();
		bankerNextRound(1, avatar->getSeat());

		MahjongAction ma;
		ma.setType(MahjongAction::Type::ZiMo);
		ma.setSlot(static_cast<int>(_actors.size()) - 1);
		ma.setTile(_huTileId);
		_actions.push_back(ma);
		afterHu();
	}

	void HongZhongMahjongRoom::calcHuScore() const {
		if (!_hu)
			return;
		int score = 0;
		int scores[4] = { 0, 0, 0, 0 };
		HongZhongMahjongAvatar* avatar1 = nullptr;
		HongZhongMahjongAvatar* avatar2 = nullptr;
		ensureBirdTile();
		for (int i = 0; i < getMaxPlayerNums(); i++) {
			avatar1 = dynamic_cast<HongZhongMahjongAvatar*>(getAvatar(i).get());
			if (avatar1 == nullptr)
				continue;
			if (avatar1->isHu()) {
				avatar1->setBirdMultiplier(_birdMultiplier);
				score = avatar1->calcHuScore();
				if (_maxScore > 0 && score > _maxScore)
					score = _maxScore;
				if (avatar1->isDianPao()) {
					// 点炮，放炮者一人承担
					avatar2 = dynamic_cast<HongZhongMahjongAvatar*>(getAvatar(_actor).get());
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
						avatar2 = dynamic_cast<HongZhongMahjongAvatar*>(getAvatar(j).get());
						if (avatar2 == nullptr)
							continue;
						avatar1->addLoseScore(j, -score);
						avatar2->addLoseScore(i, score);
					}
				}
			}
		}
		// 汇总分数
		int loseScores[4] = { 0 };
		for (int i = 0; i < getMaxPlayerNums(); i++) {
			avatar1 = dynamic_cast<HongZhongMahjongAvatar*>(getAvatar(i).get());
			if (avatar1 == NULL)
				continue;
			avatar1->getLoseScores(loseScores);
			for (int j = 0; j < 4; j++) {
				if (i != j)
					scores[j] += loseScores[j];
			}
		}
		for (int i = 0; i < getMaxPlayerNums(); i++) {
			avatar1 = dynamic_cast<HongZhongMahjongAvatar*>(getAvatar(i).get());
			if (avatar1 != NULL)
				avatar1->setScore(scores[i]);
		}
		// DebtLiquidation清算
		bool test = false;
		double diZhu = _diZhu;
		double capital = 0.0f;
		DebtNode* node = NULL;
		std::unordered_map<int, DebtNode*> debtNet;
		for (int i = 0; i < getMaxPlayerNums(); i++) {
			avatar1 = dynamic_cast<HongZhongMahjongAvatar*>(getAvatar(i).get());
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
			avatar1 = dynamic_cast<HongZhongMahjongAvatar*>(getAvatar(it1->first).get());
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
			avatar1 = dynamic_cast<HongZhongMahjongAvatar*>(getAvatar(it1->first).get());
			node = it1->second;
			capital = node->getCapital();
			winGold = capital - static_cast<double>(avatar1->getCashPledge());
			avatar1->setWinGold(winGold);
			++it1;
		}
		dl.releaseDebtNet(debtNet);
	}

	void HongZhongMahjongRoom::doJieSuan() {
		_roundState = StageState::NotStarted;
		const bool allRoundsFinished = (_roundCount > 0 && _roundNo >= _roundCount);
		ensureBirdTile();

		MsgHZSettlement msg;
		getSettlementData(&(msg.data));
		if (_birdTileId != MahjongTile::INVALID_ID) {
			msg.birdTile.setId(_birdTileId);
			MahjongDealer::getTileById(msg.birdTile);
		}
		msg.birdMultiplier = _birdMultiplier;

		double delta = 0.0;
		int64_t cashPledge = 0LL;
		int64_t goldNeed = getCashPledge();
		bool test = true;
		GameAvatar::Ptr ptr;
		HongZhongMahjongAvatar* avatar = NULL;
		std::shared_ptr<GetCapitalTask> task;
		for (int i = 0; i < getMaxPlayerNums(); i++) {
			ptr = getAvatar(i);
			avatar = dynamic_cast<HongZhongMahjongAvatar*>(ptr.get());
			if (avatar == NULL)
				continue;
			delta = avatar->getWinGold();
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
			avatar = dynamic_cast<HongZhongMahjongAvatar*>(getAvatar(i).get());
			if (avatar == NULL)
				continue;
			msg.kick = _kicks[i] && !allRoundsFinished;
			msg.send(avatar->getSession());
		}
	}

	void HongZhongMahjongRoom::afterHu() {
		saveRoundRecord();
		const bool allRoundsFinished = (_roundCount > 0 && _roundNo >= _roundCount);

		GameAvatar::Ptr avatar;
		for (int i = 0; i < getMaxPlayerNums(); i++) {
			avatar = getAvatar(i);
			if (!avatar)
				continue;
			if (_kicks[i] && !allRoundsFinished)
				kickAvatar(avatar);
			else
				avatar->setReady(false);
		}
	}

	void HongZhongMahjongRoom::onDisbandRequest(const NetMessage::Ptr& netMsg) {
		if (!_dissolveVote)
			return;
		if (_disbandState == StageState::Underway)
			return;
		MsgDisbandRequest* inst = dynamic_cast<MsgDisbandRequest*>(netMsg->getMessage().get());
		if (inst == nullptr)
			return;
		HongZhongMahjongAvatar* avatar = dynamic_cast<HongZhongMahjongAvatar*>(getAvatar(inst->getPlayerId()).get());
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

	void HongZhongMahjongRoom::notifyDisbandVote(const std::string& playerId) {
		MsgHZDisbandVote msg;
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

	void HongZhongMahjongRoom::onDisbandChoose(const NetMessage::Ptr& netMsg) {
		MsgDisbandChoose* inst = dynamic_cast<MsgDisbandChoose*>(netMsg->getMessage().get());
		if (inst == nullptr)
			return;
		if ((inst->choice != 1) && (inst->choice != 2))
			return;
		HongZhongMahjongAvatar* avatar = dynamic_cast<HongZhongMahjongAvatar*>(getAvatar(inst->getPlayerId()).get());
		if (avatar != NULL)
			doDisbandChoose(avatar->getSeat(), inst->choice);
	}

	void HongZhongMahjongRoom::doDisbandChoose(int seat, int choice) {
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

	void HongZhongMahjongRoom::disbandRoom() {
		_disbandState = StageState::Finished;

		MsgDisband msg;
		sendMessageToAll(msg);

		_roundState = StageState::NotStarted;
		kickAllAvatars();
		gameOver();
	}

	void HongZhongMahjongRoom::disbandObsolete() {
		_disbandState = StageState::NotStarted;

		MsgDisbandObsolete msg;
		sendMessageToAll(msg);
	}

	void HongZhongMahjongRoom::saveRoundRecord() {
		std::shared_ptr<HongZhongMahjongRecordTask> task = std::make_shared<HongZhongMahjongRecordTask>();
		task->_venueId = getId();
		task->_roundNo = _roundNo;
		task->_banker = _backupBanker;
		HongZhongMahjongPlaybackData data;
		getSettlementData(&(data.settlement));
		getPlaybackData(data);
		if (_birdTileId != MahjongTile::INVALID_ID) {
			data.birdTile.setId(_birdTileId);
			MahjongDealer::getTileById(data.birdTile);
		}
		data.birdMultiplier = _birdMultiplier;
		HongZhongMahjongAvatar* avatar = nullptr;
		for (int i = 0; i < getMaxPlayerNums(); i++) {
			avatar = dynamic_cast<HongZhongMahjongAvatar*>(getAvatar(i).get());
			if (avatar == nullptr)
				continue;
			task->_playerIds[i] = avatar->getPlayerId();
			task->_scores[i] = avatar->getScore();
			task->_winGolds[i] = static_cast<int>(avatar->getWinGold());
			data.winGolds[i] = task->_winGolds[i];
		}
		// 生成随机种子hash（合规随机算法审计）
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
				LOG_ERROR("红中麻将牌局回放数据压缩失败");
			else {
				if (!BaseUtils::encodeBase64(base64, reinterpret_cast<const char*>(dstBuf), static_cast<int>(dstLen))) {
					LOG_ERROR("红中麻将牌局回放数据打包Base64失败");
				}
			}
			delete[] dstBuf;
		}
		task->_playback = base64;
		MysqlPool::getSingleton().asyncQuery(task);
	}

	int HongZhongMahjongRoom::getBirdMultiplier(const MahjongTile& mt) const {
		if (!mt.isValid())
			return 1;
		if (HongZhongMahjongRule::isHongZhong(mt))
			return 10;
		if (mt.getTile().isNumbered())
			return static_cast<int>(mt.getNumber());
		return 1;
	}

	void HongZhongMahjongRoom::ensureBirdTile() const {
		if (_birdTileId != MahjongTile::INVALID_ID)
			return;
		MahjongTile mt;
		if (!const_cast<HongZhongMahjongRoom*>(this)->fetchHongZhongTile(mt, true)) {
			_birdTileId = MahjongTile::INVALID_ID;
			_birdMultiplier = 1;
			return;
		}
		_birdTileId = mt.getId();
		_birdMultiplier = getBirdMultiplier(mt);
	}
}
