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
			const std::string& ruleConfig);
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
			MustIncludeSpade3 // 首出必须包含黑桃3
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

		// 是否已经出过牌（首次出牌黑桃3检查用）
		bool _hasFirstPlayed;

		// 炸弹翻倍次数
		int _bombCount;

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

	protected:
		virtual GameAvatar::Ptr createAvatar(const std::string& playerId, int seat, bool robot) const override;
		virtual bool checkEnter(const std::string& playerId, std::string& errMsg, bool robot = false) const override;
		virtual int checkLeave(const std::string& playerId, std::string& errMsg) const override;
		virtual void onAvatarLeaved(int seat, const std::string& playerId) override;
		virtual void clean() override;

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

		// 检查手牌中是否有黑桃3
		bool hasSpade3(int seat) const;

		// 出牌是否包含黑桃3
		bool cardsContainSpade3(const std::vector<int>& cardIds) const;

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

		// 消息处理
		void onSyncTable(const NetMessage::Ptr& netMsg);
		void onReady(const NetMessage::Ptr& netMsg);
		void onPlay(const NetMessage::Ptr& netMsg);
		void onDissolveVote(const NetMessage::Ptr& netMsg);

		// 消息发送
		void notifyDeal(const std::string& playerId);
		void notifyPlay(int seat, const std::vector<int>& cardIds, int genre, int nextPlayer);
		void notifySettlement(int winnerSeat);
		void notifyGameState(const std::string& playerId);
	};
}

#endif // _NIU_MA_PAODEKUAI_ROOM_H_
