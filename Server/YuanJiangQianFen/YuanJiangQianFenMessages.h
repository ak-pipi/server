// YuanJiangQianFenMessages.h
#ifndef _NIU_MA_YUANJIANG_QIANFEN_MESSAGES_H_
#define _NIU_MA_YUANJIANG_QIANFEN_MESSAGES_H_

#include "Game/GameMessages.h"
#include "PokerGenre.h"

namespace NiuMa
{
	class YuanJiangQianFenMessages {
	private:
		YuanJiangQianFenMessages() {}
	public:
		virtual ~YuanJiangQianFenMessages() {}
		static void registMessages();
	};

	class MsgQianFenSync : public MsgVenueInner {
	public:
		MsgQianFenSync() {}
		virtual ~MsgQianFenSync() {}
		static const std::string TYPE;
		virtual const std::string& getType() const { return TYPE; }
		MSGPACK_DEFINE_MAP(playerId, timestamp, nonce, signature, venueId);
	};

	class MsgQianFenReady : public MsgVenueInner {
	public:
		MsgQianFenReady() {}
		virtual ~MsgQianFenReady() {}
		static const std::string TYPE;
		virtual const std::string& getType() const { return TYPE; }
		MSGPACK_DEFINE_MAP(playerId, timestamp, nonce, signature, venueId);
	};

	class MsgQianFenCallScore : public MsgVenueInner {
	public:
		MsgQianFenCallScore() : score(0) {}
		virtual ~MsgQianFenCallScore() {}
		static const std::string TYPE;
		virtual const std::string& getType() const { return TYPE; }
		int score;	// 叫分值
		MSGPACK_DEFINE_MAP(playerId, timestamp, nonce, signature, venueId, score);
	};

	class MsgQianFenPlay : public MsgVenueInner {
	public:
		MsgQianFenPlay() {}
		virtual ~MsgQianFenPlay() {}
		static const std::string TYPE;
		virtual const std::string& getType() const { return TYPE; }
		std::vector<int> cardIds;
		MSGPACK_DEFINE_MAP(playerId, timestamp, nonce, signature, venueId, cardIds);
	};

	class MsgQianFenSyncResp : public MsgBase {
	public:
		MsgQianFenSyncResp();
		virtual ~MsgQianFenSyncResp() {}
		static const std::string TYPE;
		virtual const std::string& getType() const { return TYPE; }
		MSG_PACK_IMPL
	public:
		int gameState; int mySeat; int currentPlayer; int roundNo; int banker; int playerCount;
		std::vector<int> myCards; int bankerSeat;
		MSGPACK_DEFINE_MAP(gameState, mySeat, currentPlayer, roundNo, banker, playerCount, myCards, bankerSeat);
	};

	class MsgQianFenDeal : public MsgBase {
	public:
		MsgQianFenDeal();
		virtual ~MsgQianFenDeal() {}
		static const std::string TYPE;
		virtual const std::string& getType() const { return TYPE; }
		MSG_PACK_IMPL
	public:
		std::vector<int> cards; int roundNo; int banker;
		MSGPACK_DEFINE_MAP(cards, roundNo, banker);
	};

	class MsgQianFenCallScoreNotify : public MsgBase {
	public:
		MsgQianFenCallScoreNotify();
		virtual ~MsgQianFenCallScoreNotify() {}
		static const std::string TYPE;
		virtual const std::string& getType() const { return TYPE; }
		MSG_PACK_IMPL
	public:
		int seat; int score; int nextSeat; int bankerSeat;
		MSGPACK_DEFINE_MAP(seat, score, nextSeat, bankerSeat);
	};

	class MsgQianFenPlayNotify : public MsgBase {
	public:
		MsgQianFenPlayNotify();
		virtual ~MsgQianFenPlayNotify() {}
		static const std::string TYPE;
		virtual const std::string& getType() const { return TYPE; }
		MSG_PACK_IMPL
	public:
		int seat; std::vector<int> cardIds; int nextPlayer;
		MSGPACK_DEFINE_MAP(seat, cardIds, nextPlayer);
	};

	class MsgQianFenRoundResult : public MsgBase {
	public:
		MsgQianFenRoundResult();
		virtual ~MsgQianFenRoundResult() {}
		static const std::string TYPE;
		virtual const std::string& getType() const { return TYPE; }
		MSG_PACK_IMPL
	public:
		int scores[4]; int64_t winGolds[4]; int roundScoreCards[4];
		MSGPACK_DEFINE_MAP(scores, winGolds, roundScoreCards);
	};

	class MsgQianFenFinalResult : public MsgBase {
	public:
		MsgQianFenFinalResult();
		virtual ~MsgQianFenFinalResult() {}
		static const std::string TYPE;
		virtual const std::string& getType() const { return TYPE; }
		MSG_PACK_IMPL
	public:
		int totalScores[4]; int64_t totalGolds[4]; std::string playerIds[4];
		int64_t roomFeeTotal;
		std::vector<std::string> roomFeePlayerIds;
		std::vector<int64_t> roomFeeAmounts;
		int64_t shuffleFeeTotal;
		std::vector<std::string> shuffleFeePlayerIds;
		std::vector<int64_t> shuffleFeeAmounts;
		MSGPACK_DEFINE_MAP(totalScores, totalGolds, playerIds,
			roomFeeTotal, roomFeePlayerIds, roomFeeAmounts,
			shuffleFeeTotal, shuffleFeePlayerIds, shuffleFeeAmounts);
	};
}

#endif
