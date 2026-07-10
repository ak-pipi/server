// DouDiZhuMessages.h

#ifndef _NIU_MA_DOU_DI_ZHU_MESSAGES_H_
#define _NIU_MA_DOU_DI_ZHU_MESSAGES_H_

#include "Game/GameMessages.h"

namespace NiuMa
{
	class DouDiZhuMessages
	{
	private:
		DouDiZhuMessages() {}

	public:
		virtual ~DouDiZhuMessages() {}
		static void registMessages();
	};

	class MsgDouDiZhuSync : public MsgVenueInner {
	public:
		static const std::string TYPE;
		virtual const std::string& getType() const { return TYPE; }
		MSGPACK_DEFINE_MAP(playerId, timestamp, nonce, signature, venueId);
	};

	class MsgDouDiZhuSyncResp : public MsgBase {
	public:
		MsgDouDiZhuSyncResp();
		static const std::string TYPE;
		virtual const std::string& getType() const { return TYPE; }
		MSG_PACK_IMPL

	public:
		int gameState;
		int currentPlayer;
		int mySeat;
		std::vector<int> myCards;
		std::vector<int> bottomCards;
		int landlordSeat;
		int callStarter;
		int highestBidSeat;
		int highestBid;
		int callTurn;
		int callCount;
		int multiplier;
		std::vector<int> lastPlayCards;
		int lastPlaySeat;
		int lastPlayGenre;
		bool isFirstPlay;
		int roundNo;
		int playerCount;
		int handCounts[2];

		MSGPACK_DEFINE_MAP(gameState, currentPlayer, mySeat, myCards, bottomCards,
			landlordSeat, callStarter, highestBidSeat, highestBid, callTurn, callCount,
			multiplier, lastPlayCards, lastPlaySeat, lastPlayGenre, isFirstPlay,
			roundNo, playerCount, handCounts);
	};

	class MsgDouDiZhuReady : public MsgVenueInner {
	public:
		static const std::string TYPE;
		virtual const std::string& getType() const { return TYPE; }
		MSGPACK_DEFINE_MAP(playerId, timestamp, nonce, signature, venueId);
	};

	class MsgDouDiZhuDeal : public MsgBase {
	public:
		MsgDouDiZhuDeal();
		static const std::string TYPE;
		virtual const std::string& getType() const { return TYPE; }
		MSG_PACK_IMPL

	public:
		std::vector<int> cards;
		int callStarter;
		int roundNo;
		int handCounts[2];

		MSGPACK_DEFINE_MAP(cards, callStarter, roundNo, handCounts);
	};

	class MsgDouDiZhuCall : public MsgVenueInner {
	public:
		MsgDouDiZhuCall() : score(0) {}
		static const std::string TYPE;
		virtual const std::string& getType() const { return TYPE; }

	public:
		int score;
		MSGPACK_DEFINE_MAP(playerId, timestamp, nonce, signature, venueId, score);
	};

	class MsgDouDiZhuCallNotify : public MsgBase {
	public:
		MsgDouDiZhuCallNotify();
		static const std::string TYPE;
		virtual const std::string& getType() const { return TYPE; }
		MSG_PACK_IMPL

	public:
		int seat;
		int score;
		int highestBid;
		int highestBidSeat;
		int nextSeat;
		int callCount;

		MSGPACK_DEFINE_MAP(seat, score, highestBid, highestBidSeat, nextSeat, callCount);
	};

	class MsgDouDiZhuLandlord : public MsgBase {
	public:
		MsgDouDiZhuLandlord();
		static const std::string TYPE;
		virtual const std::string& getType() const { return TYPE; }
		MSG_PACK_IMPL

	public:
		int landlordSeat;
		std::vector<int> bottomCards;
		int multiplier;
		int currentPlayer;
		int handCounts[2];

		MSGPACK_DEFINE_MAP(landlordSeat, bottomCards, multiplier, currentPlayer, handCounts);
	};

	class MsgDouDiZhuPlay : public MsgVenueInner {
	public:
		static const std::string TYPE;
		virtual const std::string& getType() const { return TYPE; }

	public:
		std::vector<int> cardIds;
		MSGPACK_DEFINE_MAP(playerId, timestamp, nonce, signature, venueId, cardIds);
	};

	class MsgDouDiZhuPlayNotify : public MsgBase {
	public:
		MsgDouDiZhuPlayNotify();
		static const std::string TYPE;
		virtual const std::string& getType() const { return TYPE; }
		MSG_PACK_IMPL

	public:
		int seat;
		std::vector<int> cardIds;
		int genre;
		int nextPlayer;
		int multiplier;
		int handCounts[2];

		MSGPACK_DEFINE_MAP(seat, cardIds, genre, nextPlayer, multiplier, handCounts);
	};

	class MsgDouDiZhuPlayFailed : public MsgBase {
	public:
		static const std::string TYPE;
		virtual const std::string& getType() const { return TYPE; }
		MSG_PACK_IMPL

	public:
		std::string errMsg;
		MSGPACK_DEFINE_MAP(errMsg);
	};

	class MsgDouDiZhuSettlement : public MsgBase {
	public:
		MsgDouDiZhuSettlement();
		static const std::string TYPE;
		virtual const std::string& getType() const { return TYPE; }
		MSG_PACK_IMPL

	public:
		int winnerSeat;
		int landlordSeat;
		int scores[2];
		int64_t winGolds[2];
		std::vector<int> remainCards[2];
		int multiplier;
		bool spring;

		MSGPACK_DEFINE_MAP(winnerSeat, landlordSeat, scores, winGolds, remainCards, multiplier, spring);
	};
}

#endif
