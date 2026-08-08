// PaoDeKuaiRoom.h
// 跑得快游戏房间

#ifndef _NIU_MA_PAODEKUAI_ROOM_H_
#define _NIU_MA_PAODEKUAI_ROOM_H_

#include "Game/GameRoom.h"
#include "PokerGenre.h"
#include "PokerDealer.h"
#include "PaoDeKuaiRule.h"
#include "PaoDeKuaiPlayback.h"
#include "Game/RiskControlCollector.h"

#include <memory>

namespace NiuMa
{
	class PaoDeKuaiAvatar;

	class PaoDeKuaiRoom : public GameRoom
	{
	public:
		PaoDeKuaiRoom(const std::shared_ptr<PaoDeKuaiRule>& rule,
			const std::string& venueId,
			const std::string& number,
			int level,
			const std::string& ruleConfig,
			int districtId = 0);
		virtual ~PaoDeKuaiRoom();

	private:
		// 游戏状态
		enum class GameState : int
		{
			None,		// 空闲
			Ready,		// 等待准备
			Playing,	// 出牌中
			Settling	// 结算中
		};

		// 出牌结果
		enum class PlayResult : int
		{
			OK,				// 出牌成功
			NotYourTurn,	// 不是你的回合
			InvalidCards,	// 不是你的手牌
			InvalidGenre,	// 牌型不合法
			CannotBeat,		// 无法大过上家
			CannotPass		// 有牌能大过上家，不能过牌
		};

	private:
		// 跑得快规则
		std::shared_ptr<PaoDeKuaiRule> _rule;

		// 发牌器
		PokerDealer _dealer;

		// 规则配置JSON
		std::string _ruleConfig;

		// 房间编号
		std::string _number;

		// 等级
		int _level;

		// 区域ID，0表示好友房/练习房
		int _districtId;

		// 当前游戏状态
		GameState _gameState;

		// 进入当前状态的时间
		time_t _stateTime;

		// 局号
		int _roundNo;

		// 庄家座位号
		int _banker;

		// 当前出牌玩家座位号
		int _currentPlayer;

		// 上一次有效出牌的座位号（用于判断是否一轮都没人出牌）
		int _lastPlaySeat;

		// 上一次有效出牌的牌型
		PokerGenre _lastPlayGenre;

		// 是否为新一轮首出
		bool _isFirstPlay;

		// 是否已经出过牌（用于恢复新一轮首出状态）
		bool _hasFirstPlayed;

		// 炸弹翻倍次数
		int _bombCount;

		// 当前结算倍数
		int _multiplier;

			// 本局是否关门/春天
			bool _spring;

			int64_t _roomFee;

			// 托管超时计时器
		time_t _autoPlayTime;

		// 回放数据
		PaoDeKuaiPlaybackData _playback;

		// 风控数据采集
		RiskControlCollector _riskCollector;

		// 解散投票
		int _dissolveVotes[2];

		// 是否有人发起解散
		bool _dissolveRequested;

		// 解散发起者
		int _dissolveRequester;

		// 解散发起时间
		time_t _dissolveTick;

	protected:
		virtual GameAvatar::Ptr createAvatar(const std::string& playerId, int seat, bool robot) const override;
		virtual bool checkEnter(const std::string& playerId, std::string& errMsg, bool robot = false) const override;
		virtual int checkLeave(const std::string& playerId, std::string& errMsg) const override;
		virtual void getAvatarExtraInfo(const GameAvatar::Ptr& avatar, std::string& base64) const override;
		virtual void onAvatarJoined(int seat, const std::string& playerId) override;
		virtual void onAvatarLeaved(int seat, const std::string& playerId) override;
		virtual void clean() override;
		virtual bool canShuffleCardsBeforeNextRound(const std::string& playerId,
			int& nextRoundNo,
			int& roundCount,
			std::string& errMsg) const override;

	public:
		virtual void onTimer() override;
		virtual bool onMessage(const NetMessage::Ptr& netMsg) override;

	private:
		// 获取指定座位号的Avatar
		std::shared_ptr<PaoDeKuaiAvatar> getAvatar(int seat) const;

		// 设置状态
		void setState(GameState s);

		// 开始新一局
		void startRound();

		// 发牌
		void dealCards();

		// 确定首出玩家
		int determineFirstPlayer() const;

		// 切换到下一个出牌玩家
		void nextPlayer();

		// 检查是否所有人都已准备
		bool allReady() const;

		// 执行出牌
		PlayResult doPlay(int seat, const std::vector<int>& cardIds);

		// 验证出牌
		PlayResult validatePlay(int seat, const std::vector<int>& cardIds, PokerGenre& genre) const;

		// 指定玩家是否有能大过上手的牌
		bool hasBeatingPlay(int seat) const;

		// 枚举手牌，查找一手自动出牌
		bool findAutoPlay(int seat, bool firstPlay, std::vector<int>& cardIds) const;

		// 当前牌型是否优于已选自动牌型
		bool betterAutoPlay(const PokerGenre& candidate, const PokerGenre& current, bool firstPlay) const;

		// 结算
		void settle();

		// 计算得分
		void calculateScores(int winnerSeat);

		// 保存回放记录
		void saveRoundRecord();

		// 托管自动出牌
		void autoPlay();

		// 获取下一个座位号
		int getNextSeat(int seat) const;

		// 更新区域匹配房间未满列表
		void updateDistrictNotFull();

			// 记录区域玩家离场轨迹
			void recordDistrictPlayerTrack(const std::string& playerId);

			void publishFinalRoomFee();

			void onDisbandRequest(const NetMessage::Ptr& netMsg);
			void onDisbandChoose(const NetMessage::Ptr& netMsg);
			void doDisbandChoose(int seat, int choice);
			void notifyDisbandVote(const std::string& playerId);
			void disbandRoom();
			void disbandObsolete();

			// 消息处理
		void onSyncTable(const NetMessage::Ptr& netMsg);
		void onReady(const NetMessage::Ptr& netMsg);
		void onPlay(const NetMessage::Ptr& netMsg);

		// 消息发送
		void notifyDeal(const std::string& playerId);
		void notifyPlay(int seat, const std::vector<int>& cardIds, int genre, int nextPlayer);
		void notifySettlement(int winnerSeat);
		void notifyGameState(const std::string& playerId);
	};
}

#endif // _NIU_MA_PAODEKUAI_ROOM_H_
