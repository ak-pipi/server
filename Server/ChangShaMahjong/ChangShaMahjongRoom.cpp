// ChangShaMahjongRoom.cpp

#include "ChangShaMahjongRoom.h"
#include "ChangShaMahjongAvatar.h"
#include "ChangShaMahjongMessages.h"
#include "ChangShaMahjongRecordTask.h"
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
#include <algorithm>

namespace NiuMa
{
	ChangShaMahjongRoom::ChangShaMahjongRoom(const std::string& venueId, const std::string& number, int level, const std::string& ruleConfig)
		: MahjongRoom(std::make_shared<MahjongRule>(), venueId, static_cast<int>(GameType::ChangShaMahjong))
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
		, _birdMultiple(1)
		, _diZhu(1)
		, _maxScore(300)
		, _roomFeeType(0)
		, _roundCount(8)
		, _allowChi(false)
		, _allowPeng(true)
		, _allowGang(true)
		, _allowZiMo(true)
		, _allowDianPao(true)
		, _dissolveVote(true)
		, _bankerRule(0)
		, _maxFan(8)
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
		}
		parseRuleConfig(ruleConfig);
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

		if (root.isMember("di_zhu") && root["di_zhu"].isInt())
			_diZhu = root["di_zhu"].asInt();
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
		if (root.isMember("dissolve_vote") && root["dissolve_vote"].isBool())
			_dissolveVote = root["dissolve_vote"].asBool();
		if (root.isMember("banker_rule") && root["banker_rule"].isInt())
			_bankerRule = root["banker_rule"].asInt();
		if (root.isMember("max_fan") && root["max_fan"].isInt())
			_maxFan = root["max_fan"].asInt();

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
	}

	GameAvatar::Ptr ChangShaMahjongRoom::createAvatar(const std::string& playerId, int seat, bool robot) const {
		return std::make_shared<ChangShaMahjongAvatar>(playerId, seat, robot);
	}

	bool ChangShaMahjongRoom::checkEnter(const std::string& playerId, std::string& errMsg, bool robot) const {
		if (getAvatarCount() >= 4) {
			errMsg = "房间已满";
			return false;
		}
		return true;
	}

	int ChangShaMahjongRoom::checkLeave(const std::string& playerId, std::string& errMsg) const {
		if (_roundState == StageState::Underway) {
			errMsg = "游戏进行中";
			return 2;
		}
		return 0;
	}

	void ChangShaMahjongRoom::getAvatarExtraInfo(const GameAvatar::Ptr& avatar, std::string& base64) const {
		// 长沙麻将额外信息
	}

	void ChangShaMahjongRoom::onAvatarLeaved(int seat, const std::string& playerId) {
		if (seat >= 0 && seat < 4)
			_kicks[seat] = false;
		if (getAvatarCount() == 0)
			_roundState = StageState::NotStarted;
	}

	void ChangShaMahjongRoom::clean() {
		_roundState = StageState::NotStarted;
		_roundNo = 0;
		_qiShouHuTriggered = false;
		_qiShouHuSeat = -1;
		_qiShouHuType = 0;
		_birdTiles.clear();
		_birdHitSeats.clear();
		_birdMultiple = 1;
		_playbackData = ChangShaMahjongPlaybackData();
		for (int i = 0; i < 4; i++) {
			auto avatar = std::dynamic_pointer_cast<ChangShaMahjongAvatar>(getAvatar(i));
			if (avatar)
				avatar->clear();
		}
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
		// 胡牌分数计算在doJieSuan中处理
	}

	void ChangShaMahjongRoom::doJieSuan() {
		// 基础结算逻辑
		// 胡牌者得分，其他玩家扣分
		// 起手胡、中鸟翻倍等在此计算
	}

	void ChangShaMahjongRoom::afterHu() {
		// 胡牌后处理：中鸟、结算、保存记录
		saveRoundRecord();
	}

	void ChangShaMahjongRoom::onTimer() {
		time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

		// 解散超时处理
		if (_disbandState == StageState::Underway && _disbandTick > 0 && now >= _disbandTick) {
			disbandRoom();
		}
	}

	bool ChangShaMahjongRoom::onMessage(const NetMessage::Ptr& netMsg) {
		const std::string& type = netMsg->getType();
		if (type == MsgChangShaSync::TYPE) {
			onSyncMahjong(netMsg);
			return true;
		}
		else if (type == MsgChangShaReady::TYPE) {
			onPlayerReady(netMsg);
			return true;
		}
		else if (type == MsgChangShaDisband::TYPE) {
			auto msg = std::dynamic_pointer_cast<MsgChangShaDisband>(netMsg);
			if (!msg)
				return true;
			if (msg->choice == 1) {
				onDisbandRequest(netMsg);
			}
			else {
				// 查找座位
				GameAvatar::Ptr av = getAvatar(msg->getPlayerId());
				if (av) {
					int seat = av->getSeat();
					doDisbandChoose(seat, msg->choice);
				}
			}
			return true;
		}
		return MahjongRoom::onMessage(netMsg);
	}

	void ChangShaMahjongRoom::onSyncMahjong(const NetMessage::Ptr& netMsg) {
		// 复用基类的同步消息处理
		MahjongRoom::onMessage(netMsg);
	}

	void ChangShaMahjongRoom::onPlayerReady(const NetMessage::Ptr& netMsg) {
		auto msg = std::dynamic_pointer_cast<MsgChangShaReady>(netMsg);
		if (!msg)
			return;
		const std::string& playerId = msg->getPlayerId();
		GameAvatar::Ptr av = getAvatar(playerId);
		if (av) {
			auto avatar = std::dynamic_pointer_cast<ChangShaMahjongAvatar>(av);
			if (avatar)
				avatar->setReady(true);
		}
		// 检查是否所有人都准备了
		bool allReady = true;
		if (getAvatarCount() < 4)
			allReady = false;
		if (allReady) {
			for (int i = 0; i < 4; i++) {
				auto avatar = std::dynamic_pointer_cast<ChangShaMahjongAvatar>(getAvatar(i));
				if (avatar && !avatar->isReady()) {
					allReady = false;
					break;
				}
			}
		}
		if (allReady) {
			startRound();
		}
	}

	void ChangShaMahjongRoom::startRound() {
		_roundNo++;
		_qiShouHuTriggered = false;
		_qiShouHuSeat = -1;
		_qiShouHuType = 0;
		_birdTiles.clear();
		_birdHitSeats.clear();
		_birdMultiple = 1;

		// 重置玩家状态
		for (int i = 0; i < 4; i++) {
			auto avatar = std::dynamic_pointer_cast<ChangShaMahjongAvatar>(getAvatar(i));
			if (avatar) {
				avatar->clear();
				avatar->setReady(false);
			}
		}

		// 风控采集：开始新局
		_riskCollector.startRound(getId(), static_cast<int>(GameType::ChangShaMahjong), _roundNo);
		for (int i = 0; i < 4; i++) {
			GameAvatar::Ptr av = getAvatar(i);
			if (av)
				_riskCollector.recordPlayer(av->getPlayerId(), i);
		}

		// 调用基类的开始发牌
		// MahjongRoom基类会处理洗牌、发牌、分配座位等
		_roundState = StageState::Underway;

		{
			std::ostringstream oss;
			oss << "长沙麻将游戏开始，场地: " << getId() << "，局号: " << _roundNo;
			LOG_INFO(oss.str());
		}
	}

	void ChangShaMahjongRoom::checkQiShouHu() {
		// 发牌后立即检测每个玩家是否有起手胡
		for (int i = 0; i < 4; i++) {
			GameAvatar::Ptr av = getAvatar(i);
			if (!av) continue;
			int type = detectQiShouHuType(i);
			if (type > 0) {
				_qiShouHuTriggered = true;
				_qiShouHuSeat = i;
				_qiShouHuType = type;

				auto avatar = std::dynamic_pointer_cast<ChangShaMahjongAvatar>(av);
				if (avatar) {
					avatar->setQiShouHuType(type);
					int score = _diZhu;
					switch (type) {
					case 1: score *= 1; break; // 缺一色
					case 2: score *= 1; break; // 板板胡
					case 3: score *= 4; break; // 大四喜
					case 4: score *= 2; break; // 六六顺
					case 5: score *= 2; break; // 节节高
					case 6: score *= 2; break; // 三同
					case 7: score *= 1; break; // 一枝花
					}
					if (score > _maxScore)
						score = _maxScore;
					avatar->setQiShouHuScore(score);
				}

				// 通知所有玩家
				auto msg = std::make_shared<MsgChangShaQiShouHu>();
				msg->seat = i;
				msg->huType = type;
				msg->score = avatar ? avatar->getQiShouHuScore() : 0;
				sendMessageToAll(*msg);

				// 执行起手胡结算
				// 胡牌者获得其他玩家赔付
				if (avatar) {
					int score = avatar->getQiShouHuScore();
					avatar->setWinGold(score * 3.0 * _level);
					for (int j = 0; j < 4; j++) {
						if (j != i) {
							auto other = std::dynamic_pointer_cast<ChangShaMahjongAvatar>(getAvatar(j));
							if (other) {
								other->addLoseScore(i, score);
								other->setWinGold(-score * _level);
							}
						}
					}
				}
				break; // 只处理第一个检测到的起手胡
			}
		}
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
		// 统计每种相同牌的数量（使用id判断同类型同数值）
		std::unordered_map<int, int> freqs;
		for (const auto& t : tiles) {
			// 使用pattern+number作为key
			int key = static_cast<int>(t.getPattern()) * 10 + static_cast<int>(t.getNumber());
			freqs[key]++;
		}
		for (const auto& kv : freqs) {
			if (kv.second >= 2)
				return false;
		}
		return true;
	}

	bool ChangShaMahjongRoom::checkDaSiXi(int seat) const {
		auto avatar = std::dynamic_pointer_cast<MahjongAvatar>(getAvatar(seat));
		if (!avatar)
			return false;
		const MahjongTileArray& tiles = avatar->getTiles();
		// 统计风牌数量
		std::unordered_map<int, int> freqs;
		for (const auto& t : tiles) {
			auto p = t.getPattern();
			if (p == MahjongTile::Pattern::Dong || p == MahjongTile::Pattern::Nan ||
				p == MahjongTile::Pattern::Xi || p == MahjongTile::Pattern::Bei) {
				int key = static_cast<int>(p);
				freqs[key]++;
			}
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
		// 三种花色各有一张同数值的牌
		std::unordered_map<int, int> wanFreqs, tiaoFreqs, tongFreqs;
		for (const auto& t : tiles) {
			auto n = static_cast<int>(t.getNumber());
			auto p = t.getPattern();
			if (p == MahjongTile::Pattern::Wan) wanFreqs[n]++;
			else if (p == MahjongTile::Pattern::Tiao) tiaoFreqs[n]++;
			else if (p == MahjongTile::Pattern::Tong) tongFreqs[n]++;
		}
		for (int i = 1; i <= 9; i++) {
			if (wanFreqs[i] > 0 && tiaoFreqs[i] > 0 && tongFreqs[i] > 0)
				return true;
		}
		return false;
	}

	bool ChangShaMahjongRoom::checkSanTong(int seat) const {
		auto avatar = std::dynamic_pointer_cast<MahjongAvatar>(getAvatar(seat));
		if (!avatar)
			return false;
		const MahjongTileArray& tiles = avatar->getTiles();
		std::unordered_map<int, int> freqs;
		for (const auto& t : tiles) {
			auto p = t.getPattern();
			// 非风牌
			if (p == MahjongTile::Pattern::Wan || p == MahjongTile::Pattern::Tiao || p == MahjongTile::Pattern::Tong) {
				int key = static_cast<int>(p) * 10 + static_cast<int>(t.getNumber());
				freqs[key]++;
			}
		}
		for (const auto& kv : freqs) {
			if (kv.second >= 3)
				return true;
		}
		return false;
	}

	bool ChangShaMahjongRoom::checkYiZhiHua(int seat) const {
		auto avatar = std::dynamic_pointer_cast<MahjongAvatar>(getAvatar(seat));
		if (!avatar)
			return false;
		const MahjongTileArray& tiles = avatar->getTiles();
		int wanCount = 0, tiaoCount = 0, tongCount = 0, fengCount = 0, jianCount = 0;
		for (const auto& t : tiles) {
			auto p = t.getPattern();
			if (p == MahjongTile::Pattern::Wan) wanCount++;
			else if (p == MahjongTile::Pattern::Tiao) tiaoCount++;
			else if (p == MahjongTile::Pattern::Tong) tongCount++;
			else if (p == MahjongTile::Pattern::Dong || p == MahjongTile::Pattern::Nan ||
					 p == MahjongTile::Pattern::Xi || p == MahjongTile::Pattern::Bei)
				fengCount++;
			else if (p == MahjongTile::Pattern::Zhong || p == MahjongTile::Pattern::Fa ||
					 p == MahjongTile::Pattern::Bai)
				jianCount++;
		}
		int hasCount = 0;
		if (wanCount > 0) hasCount++;
		if (tiaoCount > 0) hasCount++;
		if (tongCount > 0) hasCount++;
		return hasCount == 1 && fengCount == 0 && jianCount == 0;
	}

	void ChangShaMahjongRoom::calculateBird(int huSeat) {
		if (!_zhongNiaoEnabled || _birdCount <= 0)
			return;

		_birdTiles.clear();
		_birdHitSeats.clear();
		_birdMultiple = 1;

		// 从牌墙末尾翻出birdCount张牌作为鸟牌
		// 使用随机数确定鸟牌
		for (int i = 0; i < _birdCount; i++) {
			// 随机生成鸟牌ID（0~33）
			int birdTile = (i * 9 + 3) % 34; // 简化实现：使用固定偏移
			_birdTiles.push_back(birdTile);

			// 判断鸟牌命中的玩家（鸟牌值%4 = 玩家座位偏移）
			int tileValue = birdTile % 9; // 牌面值
			int hitOffset = tileValue % 4;
			int hitSeat = (huSeat + hitOffset) % 4;
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
		auto msg = std::make_shared<MsgChangShaBird>();
		msg->birdTiles = _birdTiles;
		msg->hitSeats = _birdHitSeats;
		msg->multiple = _birdMultiple;
		sendMessageToAll(*msg);
	}

	void ChangShaMahjongRoom::onDisbandRequest(const NetMessage::Ptr& netMsg) {
		if (!_dissolveVote || _roundState != StageState::Underway)
			return;
		auto msg = std::dynamic_pointer_cast<MsgChangShaDisband>(netMsg);
		if (!msg)
			return;
		GameAvatar::Ptr av = getAvatar(msg->getPlayerId());
		if (av)
			_disbander = av->getSeat();
		if (_disbander < 0)
			return;

		_disbandState = StageState::Underway;
		_disbandTick = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()) + 180; // 3分钟投票时间
		for (int i = 0; i < 4; i++)
			_disbandChoices[i] = 0;
		_disbandChoices[_disbander] = 1; // 发起者默认同意

		for (int i = 0; i < 4; i++) {
			GameAvatar::Ptr av2 = getAvatar(i);
			if (av2)
				notifyDisbandVote(av2->getPlayerId());
		}
	}

	void ChangShaMahjongRoom::notifyDisbandVote(const std::string& playerId) {
		auto msg = std::make_shared<MsgChangShaDisbandVote>();
		msg->disbander = _disbander;
		time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
		msg->remainTime = static_cast<int>(_disbandTick - now);
		if (msg->remainTime < 0)
			msg->remainTime = 0;
		for (int i = 0; i < 4; i++)
			msg->choices[i] = _disbandChoices[i];
		sendMessage(*msg, playerId);
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
		// 通知所有人最新投票状态
		for (int i = 0; i < 4; i++) {
			GameAvatar::Ptr av = getAvatar(i);
			if (av)
				notifyDisbandVote(av->getPlayerId());
		}

		if (agree >= 3) {
			disbandRoom();
		}
		else if (reject >= 2) {
			_disbandState = StageState::NotStarted;
			_disbander = -1;
		}
	}

	void ChangShaMahjongRoom::disbandRoom() {
		_disbandState = StageState::Finished;
		_roundState = StageState::NotStarted;
		// 清理房间
		clean();
	}

	void ChangShaMahjongRoom::disbandObsolete() {
		// 超时解散
		disbandRoom();
	}

	void ChangShaMahjongRoom::saveRoundRecord() {
		auto task = std::make_shared<ChangShaMahjongRecordTask>();
		task->_venueId = getId();
		task->_roundNo = _roundNo;
		task->_banker = _banker;

		// 生成随机种子hash
		_playbackData.randomSeedHash = ReplayUtils::generateSeedHash(getId(), _roundNo, _banker);
		task->_randomSeedHash = _playbackData.randomSeedHash;

		int idx = 0;
		for (int i = 0; i < 4; i++) {
			auto avatar = std::dynamic_pointer_cast<ChangShaMahjongAvatar>(getAvatar(i));
			if (avatar && idx < 4) {
				task->_playerIds[idx] = avatar->getPlayerId();
				task->_scores[idx] = avatar->getRoundScore();
				task->_winGolds[idx] = avatar->getWinGold();
			}
			idx++;
		}

		// 序列化回放数据
		std::string replayData;
		ReplayUtils::compressReplay(_playbackData, replayData);
		task->_playback = replayData;

		MysqlPool::getSingleton().asyncQuery(task);

		// 风控采集：记录得分并结束
		_riskCollector.setRandomSeedHash(_playbackData.randomSeedHash);
		for (int i = 0; i < 4; i++) {
			auto avatar = std::dynamic_pointer_cast<ChangShaMahjongAvatar>(getAvatar(i));
			if (avatar)
				_riskCollector.recordScore(avatar->getPlayerId(), avatar->getRoundScore(), static_cast<int64_t>(avatar->getWinGold()));
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
}
