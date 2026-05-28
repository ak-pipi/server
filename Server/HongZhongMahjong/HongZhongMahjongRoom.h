// HongZhongMahjongRoom.h

#ifndef _NIU_MA_HONGZHONG_MAHJONG_ROOM_H_
#define _NIU_MA_HONGZHONG_MAHJONG_ROOM_H_

#include "MahjongRoom.h"
#include "../GameDefines.h"

#include <string>

namespace NiuMa
{
	/**
	 * 红中麻将游戏房
	 * 红中赖子玩法，4人麻将，红中作为万能牌可替代任何牌
	 * 支持好友房、匹配房、练习房
	 */
	class HongZhongMahjongRoom : public MahjongRoom
	{
	public:
		HongZhongMahjongRoom(const std::string& venueId, const std::string& number, int level, const std::string& ruleConfig);
		virtual ~HongZhongMahjongRoom();

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
		bool _qiduiEnabled;
		bool _pengpenghuEnabled;
		bool _zimoDouble;
		bool _dissolveVote;
		int _bankerRule;
	};
}

#endif // !_NIU_MA_HONGZHONG_MAHJONG_ROOM_H_
