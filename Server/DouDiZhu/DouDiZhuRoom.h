// DouDiZhuRoom.h

#ifndef _NIU_MA_DOU_DI_ZHU_ROOM_H_
#define _NIU_MA_DOU_DI_ZHU_ROOM_H_

#include "Game/GameRoom.h"
#include "PokerDealer.h"
#include "DouDiZhuAvatar.h"
#include "DouDiZhuGameRule.h"

namespace NiuMa
{
	class DouDiZhuRoom : public GameRoom
	{
	public:
		DouDiZhuRoom(const std::shared_ptr<DouDiZhuGameRule>& rule,
			const std::string& venueId,
			const std::string& number,
			int level,
			const std::string& ruleConfig,
			int districtId = 0);
		virtual ~DouDiZhuRoom();

	private:
		enum class GameState : int
		{
			None,
			Ready,
			Bidding,
			Playing,
			Settling
		};

		enum class PlayResult : int
		{
			OK,
			NotYourTurn,
			InvalidCards,
			InvalidGenre,
			CannotBeat,
			CannotPass
		};

		enum class CallResult : int
		{
			OK,
			NotYourTurn,
			InvalidScore
		};

	protected:
		virtual GameAvatar::Ptr createAvatar(const std::string& playerId, int seat, bool robot) const override;
		virtual bool checkEnter(const std::string& playerId, std::string& errMsg, bool robot = false) const override;
		virtual int checkLeave(const std::string& playerId, std::string& errMsg) const override;
		virtual void getAvatarExtraInfo(const GameAvatar::Ptr& avatar, std::string& base64) const override;
		virtual void onAvatarJoined(int seat, const std::string& playerId) override;
		virtual void onAvatarLeaved(int seat, const std::string& playerId) override;
		virtual void clean() override;

	public:
		virtual void onTimer() override;
		virtual bool onMessage(const NetMessage::Ptr& netMsg) override;

	private:
		std::shared_ptr<DouDiZhuAvatar> getAvatar(int seat) const;
		void setState(GameState state);
		bool allReady() const;
		int getNextSeat(int seat) const;
		int findSeatByPlayer(const std::string& playerId) const;
		void fillHandCounts(int counts[2]) const;
		void fillCardIds(const CardArray& cards, std::vector<int>& ids) const;
		void startRound(bool advanceRound = true);
		void dealCards();
		void finishBidding(int landlordSeat, int score);
		CallResult doCall(int seat, int score);
		PlayResult validatePlay(int seat, const std::vector<int>& cardIds, PokerGenre& genre) const;
		PlayResult doPlay(int seat, const std::vector<int>& cardIds);
		void advanceTurn();
		void settle(int winnerSeat);
		void calculateScores(int winnerSeat);
		void autoAction();
			std::string playErrorText(PlayResult result) const;
			void updateDistrictNotFull();
			void recordDistrictPlayerTrack(const std::string& playerId);
			void publishFinalRoomFee();
			void onDisbandRequest(const NetMessage::Ptr& netMsg);
			void onDisbandChoose(const NetMessage::Ptr& netMsg);
			void doDisbandChoose(int seat, int choice);
			void notifyDisbandVote(const std::string& playerId);
			void disbandRoom();
			void disbandObsolete();

	private:
		void onSyncTable(const NetMessage::Ptr& netMsg);
		void onReady(const NetMessage::Ptr& netMsg);
		void onCall(const NetMessage::Ptr& netMsg);
		void onPlay(const NetMessage::Ptr& netMsg);
		void notifyReady(int seat);
		void notifyDeal(const std::string& playerId);
		void notifyCall(int seat, int score, int nextSeat);
		void notifyLandlord();
		void notifyPlay(int seat, const std::vector<int>& cardIds, int genre, int nextPlayer);
		void notifySettlement(int winnerSeat);

	private:
		std::shared_ptr<DouDiZhuGameRule> _rule;
		PokerDealer _dealer;
		std::string _ruleConfig;
		std::string _number;
		int _level;
		int _districtId;
		GameState _gameState;
		time_t _stateTime;
		int _roundNo;
		int _banker;
		int _currentPlayer;
		int _landlordSeat;
		int _callStarter;
		int _callTurn;
		int _highestBidSeat;
		int _highestBid;
		int _callCount;
			int _multiplier;
			int _playCounts[2];
			bool _spring;
			int64_t _roomFee;
			int _lastPlaySeat;
		PokerGenre _lastPlayGenre;
		bool _isFirstPlay;
		time_t _autoActionTime;
		bool _dissolveRequested;
		int _dissolveRequester;
		time_t _dissolveTick;
		int _dissolveVotes[2];
		CardArray _bottomCards;
		CardArray _discardedCards;
	};
}

#endif
