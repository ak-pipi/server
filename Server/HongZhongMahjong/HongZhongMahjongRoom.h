// HongZhongMahjongRoom.h

#ifndef _NIU_MA_HONGZHONG_MAHJONG_ROOM_H_
#define _NIU_MA_HONGZHONG_MAHJONG_ROOM_H_

#include "MahjongRoom.h"
#include "../GameDefines.h"

#include <string>
#include <vector>

namespace NiuMa
{
	class HongZhongMahjongAvatar;

		/**
		 * 红中麻将游戏房
		 * 红中赖子玩法，2人麻将，红中作为万能牌可替代任何牌
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
		virtual bool fetchTile(bool bBack = false) override;
		virtual void dealTiles() override;
		virtual bool earlyTermination() const override;
		virtual bool executeGang() override;
		virtual void passActionOption(const std::string& playerId) override;
		virtual bool shouldAllowDianPaoForAvatar(MahjongAvatar* pAvatar, const MahjongTile& mt) const override;
		virtual bool canCreateDianPaoOption(MahjongAvatar* pAvatar, const MahjongTile& mt, std::string& passed) const override;
		virtual void notifyFetchTile(MahjongAvatar* pAvatar, bool bBack) override;
		virtual bool canShuffleCardsBeforeNextRound(const std::string& playerId,
			int& nextRoundNo,
			int& roundCount,
			std::string& errMsg) const override;

	private:
		static int resolvePlayerCount(const std::string& ruleConfig);
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
			void publishFinalRoomFee();
			void shuffleHongZhongTiles();
			bool fetchHongZhongTile(MahjongTile& mt, bool bBack = false);
			bool fetchHongZhongTile(MahjongTile& mt, const std::string& tileName);
			int getHongZhongTileLeft() const;
			int getBirdMultiplier(const MahjongTile& mt) const;
			void ensureBirdTile() const;
			bool hasHongZhongInHand(MahjongAvatar* pAvatar) const;
			bool isCheckingQiangGang() const;
			bool canHuWithIncomingTile(MahjongAvatar* pAvatar, const MahjongTile& mt) const;
			void applyGangScore(HongZhongMahjongAvatar* gangAvatar);
			void settleScoresFromLoseScores() const;
			HongZhongMahjongAvatar* findOpeningFourHongZhongAvatar() const;
			void doOpeningFourHongZhongHu(HongZhongMahjongAvatar* avatar);

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
			int64_t _roomFee;
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

			int _tilePool[112];
			int _tileStart;
			int _tileEnd;
			mutable int _birdTileId;
			mutable int _birdMultiplier;
			int _gangScores[4];
			int64_t _totalWinGolds[4];
		};
}

#endif // !_NIU_MA_HONGZHONG_MAHJONG_ROOM_H_
