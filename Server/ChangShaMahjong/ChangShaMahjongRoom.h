// ChangShaMahjongRoom.h
// 长沙麻将游戏房间

#ifndef _NIU_MA_CHANGSHA_MAHJONG_ROOM_H_
#define _NIU_MA_CHANGSHA_MAHJONG_ROOM_H_

#include "MahjongRoom.h"
#include "../GameDefines.h"
#include "ChangShaMahjongPlayback.h"
#include "Game/RiskControlCollector.h"

#include <string>

namespace NiuMa
{
	/**
	 * 长沙麻将游戏房
	 * 4人麻将，支持起手胡（缺一色、板板胡、大四喜、六六顺、节节高、三同、一枝花）
	 * 支持中鸟结算
	 */
	class ChangShaMahjongRoom : public MahjongRoom
	{
	public:
		ChangShaMahjongRoom(const std::string& venueId, const std::string& number, int level, const std::string& ruleConfig);
		virtual ~ChangShaMahjongRoom();

	public:
		virtual bool onMessage(const NetMessage::Ptr& netMsg) override;
		virtual void onTimer() override;

		// 重写
	protected:
		virtual GameAvatar::Ptr createAvatar(const std::string& playerId, int seat, bool robot) const override;
		virtual bool checkEnter(const std::string& playerId, std::string& errMsg, bool robot = false) const override;
		virtual int checkLeave(const std::string& playerId, std::string& errMsg) const override;
		virtual void getAvatarExtraInfo(const GameAvatar::Ptr& avatar, std::string& base64) const override;
		virtual void onAvatarLeaved(int seat, const std::string& playerId) override;
		virtual void clean() override;
		virtual double* getDistances() override;
		virtual void getDistances(std::vector<int>& distances) const override;
		virtual int getDistanceIndex(int seat1, int seat2) const override;
		virtual void calcHuScore() const override;
		virtual void doJieSuan() override;
		virtual void afterHu() override;
		virtual void dealTiles() override;

	private:
		void parseRuleConfig(const std::string& ruleConfig);
		void onSyncMahjong(const NetMessage::Ptr& netMsg);
		void onPlayerReady(const NetMessage::Ptr& netMsg);
		void onDisbandRequest(const NetMessage::Ptr& netMsg);
		void onDisbandChoose(const NetMessage::Ptr& netMsg);
		void startRound();
		void notifyDisbandVote(const std::string& playerId);
		void doDisbandChoose(int seat, int choice);
		void disbandRoom();
		void disbandObsolete();
		void saveRoundRecord();

		// 起手胡检测
		void checkQiShouHu();
		int detectQiShouHuType(int seat) const;

		// 缺一色检测：手牌中缺一种花色（万/条/筒）
		bool checkQueYiSe(int seat) const;

		// 板板胡检测：手牌中没有将（对子）
		bool checkBanBanHu(int seat) const;

		// 大四喜检测：手牌中有4张相同的风牌
		bool checkDaSiXi(int seat) const;

		// 六六顺检测：手牌中有两个刻子（三张相同的牌）
		bool checkLiuLiuShun(int seat) const;

		// 节节高检测：手牌中有三种相同花色的连续牌
		bool checkJieJieGao(int seat) const;

		// 三同检测：手牌中有三张相同的牌
		bool checkSanTong(int seat) const;

		// 一枝花检测：手牌中只有一种花色
		bool checkYiZhiHua(int seat) const;

		// 中鸟计算
		void calculateBird(int huSeat);
		void notifyBird();
		int getQiShouHuScore(int type) const;

	private:
		const std::string _number;
		const int _level;
		StageState _roundState;
		StageState _disbandState;
		int _roundNo;
		int _backupBanker;
		int _disbander;
		time_t _disbandTick;
		int _disbandChoices[4];
		double _distances[6];
		bool _kicks[4];

		// 起手胡相关
		bool _qiShouHuTriggered;
		int _qiShouHuSeat;
		int _qiShouHuType;
		int _qiShouHuScore;

		// 中鸟相关
		std::vector<int> _birdTiles;
		std::vector<int> _birdHitSeats;
		int _birdMultiple;

		// 玩法配置
		int _diZhu;
		int _maxScore;
		int _roomFeeType;
		int _roundCount;
		bool _allowChi;
		bool _allowPeng;
		bool _allowGang;
		bool _allowZiMo;
		bool _allowDianPao;
		bool _dissolveVote;
		int _bankerRule;
		int _maxFan;
		bool _require258Jiang;

		// 起手胡开关
		bool _queYiSeEnabled;
		bool _banBanHuEnabled;
		bool _daSiXiEnabled;
		bool _liuLiuShunEnabled;
		bool _jieJieGaoEnabled;
		bool _sanTongEnabled;
		bool _yiZhiHuaEnabled;

		// 中鸟开关
		bool _zhongNiaoEnabled;
		int _birdCount;
		bool _birdDouble;
		bool _birdCapMax;

		// 回放数据
		ChangShaMahjongPlaybackData _playbackData;

		// 风控数据采集
		RiskControlCollector _riskCollector;
	};
}

#endif // _NIU_MA_CHANGSHA_MAHJONG_ROOM_H_
