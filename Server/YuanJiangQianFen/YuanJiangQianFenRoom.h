// YuanJiangQianFenRoom.h
#ifndef _NIU_MA_YUANJIANG_QIANFEN_ROOM_H_
#define _NIU_MA_YUANJIANG_QIANFEN_ROOM_H_

#include "Game/GameRoom.h"
#include "PokerGenre.h"
#include "PokerDealer.h"
#include "PokerRule.h"
#include "../PaoDeKuai/PaoDeKuaiRule.h"
#include "../PaoDeKuai/PaoDeKuaiAvatar.h"
#include "YuanJiangQianFenPlayback.h"
#include "Game/RiskControlCollector.h"
#include <string>
#include <vector>
#include <unordered_map>

namespace NiuMa
{
	class YuanJiangQianFenRoom : public GameRoom
	{
	public:
		YuanJiangQianFenRoom(const std::string& venueId, const std::string& number, int level, const std::string& ruleConfig);
		virtual ~YuanJiangQianFenRoom();

	private:
		enum class GameState { None, Ready, CallScore, Playing, Settling };

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
		void parseRuleConfig(const std::string& cfg);
		void setState(GameState s);

		void startRound();
		void dealCards();
		void beginCallScore();
		void processCallScore(int seat, int score);
		void beginPlay();
		void doPlay(int seat, const std::vector<int>& cardIds);
		void settleRound();
		void checkGameEnd();
		void saveRoundRecord();

		int getNextSeat(int seat) const { return (seat + 1) % _playerCount; }
		bool allReady() const;
		int calcScoreCards(const CardArray& cards) const;

		void onSyncTable(const NetMessage::Ptr& netMsg);
		void onReady(const NetMessage::Ptr& netMsg);
		void onCallScore(const NetMessage::Ptr& netMsg);
		void onPlay(const NetMessage::Ptr& netMsg);

		void notifyDeal(const std::string& playerId);
		void notifyCallScore(int seat, int score, int nextSeat);
		void notifyPlay(int seat, const std::vector<int>& cardIds);
		void notifyRoundResult();
		void notifyFinalResult();

	private:
		std::string _number;
		int _level;
		GameState _gameState;
		int _roundNo;
		int _bankerSeat;			// 叫分庄座位
		int _currentPlayer;
		int _callScoreCurrent;		// 当前叫分玩家
		int _highestCallScore;		// 最高叫分
		int _highestCallSeat;		// 最高叫分玩家
		int _callScoreCount;		// 已叫分人数
		int _lastPlaySeat;
		bool _isFirstPlay;
		PokerGenre _lastPlayGenre;
		int _roundCount;				// 当前已玩局数

		std::shared_ptr<PaoDeKuaiRule> _rule;
		PokerDealer _dealer;

		// 每个玩家的累计得分
		int _totalScores[4];
		// 每个玩家本轮得分（计分牌）
		int _roundScores[4];

		// 规则配置
		int _playerCount;
		int _targetScore;
		bool _callScoreEnabled;
		int _bankerRule;		// 0=叫分庄, 1=随机庄, 2=轮庄
		int _deckCount;
		bool _bombEnabled;
		std::unordered_map<int, int> _scoreCardMap; // point -> score
		int _roundLimit;
		int _maxScore;

		QianFenPlaybackData _playbackData;

		// 风控数据采集
		RiskControlCollector _riskCollector;
	};
}
#endif
