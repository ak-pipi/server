// YiYangWaiHuZiMessages.h
// 益阳歪胡子网络消息定义

#ifndef _NIU_MA_YIYANG_WAIHUZI_MESSAGES_H_
#define _NIU_MA_YIYANG_WAIHUZI_MESSAGES_H_

#include "Game/GameMessages.h"

namespace NiuMa
{
	class YiYangWaiHuZiMessages
	{
	private:
		YiYangWaiHuZiMessages() {}
	public:
		virtual ~YiYangWaiHuZiMessages() {}
		static void registMessages();
	};

	class MsgWaiHuZiSync : public MsgVenueInner {
	public:
		MsgWaiHuZiSync() {}
		virtual ~MsgWaiHuZiSync() {}
		static const std::string TYPE;
		virtual const std::string& getType() const { return TYPE; }
		MSGPACK_DEFINE_MAP(playerId, timestamp, nonce, signature, venueId);
	};

	class MsgWaiHuZiReady : public MsgVenueInner {
	public:
		MsgWaiHuZiReady() {}
		virtual ~MsgWaiHuZiReady() {}
		static const std::string TYPE;
		virtual const std::string& getType() const { return TYPE; }
		MSGPACK_DEFINE_MAP(playerId, timestamp, nonce, signature, venueId);
	};

	class MsgWaiHuZiDiscard : public MsgVenueInner {
	public:
		MsgWaiHuZiDiscard() {}
		virtual ~MsgWaiHuZiDiscard() {}
		static const std::string TYPE;
		virtual const std::string& getType() const { return TYPE; }
		int cardId;
		MSGPACK_DEFINE_MAP(playerId, timestamp, nonce, signature, venueId, cardId);
	};

	class MsgWaiHuZiAction : public MsgVenueInner {
	public:
		MsgWaiHuZiAction() : action(0) {}
		virtual ~MsgWaiHuZiAction() {}
		static const std::string TYPE;
		virtual const std::string& getType() const { return TYPE; }
		int action;	// WaiHuZiAction枚举
		std::vector<int> cardIds; // 吃牌的牌ID
		MSGPACK_DEFINE_MAP(playerId, timestamp, nonce, signature, venueId, action, cardIds);
	};

	class MsgWaiHuZiSyncResp : public MsgBase {
	public:
		MsgWaiHuZiSyncResp();
		virtual ~MsgWaiHuZiSyncResp() {}
		static const std::string TYPE;
		virtual const std::string& getType() const { return TYPE; }
		MSG_PACK_IMPL
	public:
		int gameState;
		int mySeat;
		int currentPlayer;
		int roundNo;
		int banker;
		int playerCount;
		std::vector<int> myCards;
		int lastDiscardId;
		int lastDiscardSeat;
		MSGPACK_DEFINE_MAP(gameState, mySeat, currentPlayer, roundNo, banker, playerCount, myCards, lastDiscardId, lastDiscardSeat);
	};

	class MsgWaiHuZiDeal : public MsgBase {
	public:
		MsgWaiHuZiDeal();
		virtual ~MsgWaiHuZiDeal() {}
		static const std::string TYPE;
		virtual const std::string& getType() const { return TYPE; }
		MSG_PACK_IMPL
	public:
		std::vector<int> cards;
		int firstPlayer;
		int roundNo;
		int banker;
		MSGPACK_DEFINE_MAP(cards, firstPlayer, roundNo, banker);
	};

	class MsgWaiHuZiDrawNotify : public MsgBase {
	public:
		MsgWaiHuZiDrawNotify();
		virtual ~MsgWaiHuZiDrawNotify() {}
		static const std::string TYPE;
		virtual const std::string& getType() const { return TYPE; }
		MSG_PACK_IMPL
	public:
		int seat;
		int cardId;		// 只有摸牌者自己知道，其他人=0
		int remainCount;
		MSGPACK_DEFINE_MAP(seat, cardId, remainCount);
	};

	class MsgWaiHuZiDiscardNotify : public MsgBase {
	public:
		MsgWaiHuZiDiscardNotify();
		virtual ~MsgWaiHuZiDiscardNotify() {}
		static const std::string TYPE;
		virtual const std::string& getType() const { return TYPE; }
		MSG_PACK_IMPL
	public:
		int seat;
		int cardId;
		int nextPlayer;
		MSGPACK_DEFINE_MAP(seat, cardId, nextPlayer);
	};

	class MsgWaiHuZiActionNotify : public MsgBase {
	public:
		MsgWaiHuZiActionNotify();
		virtual ~MsgWaiHuZiActionNotify() {}
		static const std::string TYPE;
		virtual const std::string& getType() const { return TYPE; }
		MSG_PACK_IMPL
	public:
		int seat;
		int action;
		std::vector<int> cardIds;
		int nextPlayer;
		MSGPACK_DEFINE_MAP(seat, action, cardIds, nextPlayer);
	};

	class MsgWaiHuZiSettlement : public MsgBase {
	public:
		MsgWaiHuZiSettlement();
		virtual ~MsgWaiHuZiSettlement() {}
		static const std::string TYPE;
		virtual const std::string& getType() const { return TYPE; }
		MSG_PACK_IMPL
	public:
		int huSeat;
		int huXi;
		int scores[3];
		double winGolds[3];
		MSGPACK_DEFINE_MAP(huSeat, huXi, scores, winGolds);
	};
}

#endif // _NIU_MA_YIYANG_WAIHUZI_MESSAGES_H_
