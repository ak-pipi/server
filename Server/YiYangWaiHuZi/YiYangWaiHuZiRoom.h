// YiYangWaiHuZiRoom.h
#ifndef _NIU_MA_YIYANG_WAIHUZI_ROOM_H_
#define _NIU_MA_YIYANG_WAIHUZI_ROOM_H_

#include "Game/GameRoom.h"
#include "YiYangWaiHuZiCard.h"
#include "YiYangWaiHuZiPlayback.h"
#include "Game/RiskControlCollector.h"
#include <string>
#include <vector>

namespace NiuMa
{
	class YiYangWaiHuZiAvatar;

	class YiYangWaiHuZiRoom : public GameRoom
	{
	public:
		YiYangWaiHuZiRoom(const std::string& venueId, const std::string& number, int level, const std::string& ruleConfig);
		virtual ~YiYangWaiHuZiRoom();

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
		enum class GameState { None, Ready, Playing, Settling };

		std::shared_ptr<YiYangWaiHuZiAvatar> getAvatar(int seat) const;
		void setState(GameState s);
		void parseRuleConfig(const std::string& cfg);

		// 牌池管理
		void initTilePool();
		void shufflePool();
		bool drawCard(int seat);	// 摸牌

		// 游戏流程
		void startRound();
		void dealCards();
		int getNextSeat(int seat) const;
		bool allReady() const;

		// 出牌/操作处理
		void onSyncTable(const NetMessage::Ptr& netMsg);
		void onReady(const NetMessage::Ptr& netMsg);
		void onDiscard(const NetMessage::Ptr& netMsg);
		void onAction(const NetMessage::Ptr& netMsg);
		void doDiscard(int seat, int cardId);
		void doAction(int seat, int action, const std::vector<int>& cardIds);
		void checkAvailableActions(int seat);

		// 操作检测
		bool canChi(int seat, const WaiHuZiCard& card) const;
		bool canPeng(int seat, const WaiHuZiCard& card) const;
		bool canWei(int seat) const;	// 摸牌后检测偎
		bool canPao(int seat) const;	// 摸牌后检测跑
		bool canTi(int seat) const;	// 摸牌后检测提
		bool canHu(int seat) const;

		// 胡息计算
		int calcHuXiForCombination(int type, const std::vector<int>& cardIds) const;
		int calcTotalHuXi(int seat) const;
		int calcPenaltyHuXi() const;

		// 结算
		void settle(int huSeat);
		void saveRoundRecord();

		// 消息发送
		void notifyDeal(const std::string& playerId);
		void notifyDraw(int seat, int cardId);
		void notifyDiscard(int seat, int cardId);
		void notifyAction(int seat, int action, const std::vector<int>& cardIds);
		void notifySettlement(int huSeat);

	private:
		std::string _number;
		int _level;
		GameState _gameState;
		time_t _stateTime;
		int _roundNo;
		int _banker;
		int _currentPlayer;
		int _lastDiscardSeat;
		int _lastDiscardId;

		// 牌池
		std::vector<WaiHuZiCard> _tilePool;
		int _tileIndex;		// 当前牌池索引
		int _drawCount;		// 已摸牌数

		// 规则配置
		int _playerCount;
		int _minHuXi;
		int _maxScore;
		int _tunScoreRate;
		bool _allowChi;
		bool _allowPeng;
		bool _allowWei;
		bool _allowPao;
		bool _allowTi;
		int _zhuangRule;

		// 回放
		WaiHuZiPlaybackData _playbackData;

		// 风控数据采集
		RiskControlCollector _riskCollector;
	};
}
#endif
