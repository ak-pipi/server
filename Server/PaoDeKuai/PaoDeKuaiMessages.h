// PaoDeKuaiMessages.h
// 跑得快网络消息定义

#ifndef _NIU_MA_PAODEKUAI_MESSAGES_H_
#define _NIU_MA_PAODEKUAI_MESSAGES_H_

#include "Game/GameMessages.h"
#include "PokerGenre.h"

namespace NiuMa
{
	class PaoDeKuaiMessages
	{
	private:
		PaoDeKuaiMessages() {}

	public:
		virtual ~PaoDeKuaiMessages() {}

		static void registMessages();
	};

	/**
	 * 请求同步跑得快游戏数据消息
	 * 客户端->服务器
	 */
	class MsgPaoDeKuaiSync : public MsgVenueInner {
	public:
		MsgPaoDeKuaiSync() {}
		virtual ~MsgPaoDeKuaiSync() {}

		static const std::string TYPE;

		virtual const std::string& getType() const {
			return TYPE;
		}

		MSGPACK_DEFINE_MAP(playerId, timestamp, nonce, signature, venueId);
	};

	/**
	 * 响应同步跑得快游戏数据消息
	 * 服务器->客户端
	 */
	class MsgPaoDeKuaiSyncResp : public MsgBase {
	public:
		MsgPaoDeKuaiSyncResp();
		virtual ~MsgPaoDeKuaiSyncResp() {}

		static const std::string TYPE;

		virtual const std::string& getType() const {
			return TYPE;
		}

		MSG_PACK_IMPL

	public:
		// 游戏状态
		int gameState;

		// 当前出牌玩家的座位号
		int currentPlayer;

		// 自己的座位号
		int mySeat;

		// 自己的手牌
		std::vector<int> myCards;

		// 桌面上最新的出牌
		std::vector<int> lastPlayCards;

		// 桌面上最新出牌的座位号
		int lastPlaySeat;

		// 桌面上最新出牌的牌型
		int lastPlayGenre;

		// 是否为首出（无人压牌）
		bool isFirstPlay;

		// 局号
		int roundNo;

		// 庄家座位号
		int banker;

		// 玩家数量
		int playerCount;

		// 房间号
		std::string number;

		// 房间等级
		int level;

		// 底注
		int baseScore;

		// 总局数
		int roundCount;

		// 炸弹数量
		int bombCount;

		// 当前倍数
		int multiplier;

		// 各玩家剩余手牌数量
		int remainCounts[2];

		MSGPACK_DEFINE_MAP(gameState, currentPlayer, mySeat, myCards, lastPlayCards,
			lastPlaySeat, lastPlayGenre, isFirstPlay, roundNo, banker, playerCount,
			number, level, baseScore, roundCount, bombCount, multiplier, remainCounts);
	};

	/**
	 * 玩家准备消息
	 * 客户端->服务器
	 */
	class MsgPaoDeKuaiReady : public MsgVenueInner {
	public:
		MsgPaoDeKuaiReady() {}
		virtual ~MsgPaoDeKuaiReady() {}

		static const std::string TYPE;

		virtual const std::string& getType() const {
			return TYPE;
		}

		MSGPACK_DEFINE_MAP(playerId, timestamp, nonce, signature, venueId);
	};

	/**
	 * 通知游戏开始/发牌消息
	 * 服务器->客户端
	 */
	class MsgPaoDeKuaiDeal : public MsgBase {
	public:
		MsgPaoDeKuaiDeal();
		virtual ~MsgPaoDeKuaiDeal() {}

		static const std::string TYPE;

		virtual const std::string& getType() const {
			return TYPE;
		}

		MSG_PACK_IMPL

	public:
		// 手牌ID列表
		std::vector<int> cards;

		// 首出玩家座位号
		int firstPlayer;

		// 局号
		int roundNo;

		// 庄家座位号
		int banker;

		// 总局数
		int roundCount;

		// 底注
		int baseScore;

		MSGPACK_DEFINE_MAP(cards, firstPlayer, roundNo, banker, roundCount, baseScore);
	};

	/**
	 * 玩家出牌消息
	 * 客户端->服务器
	 */
	class MsgPaoDeKuaiPlay : public MsgVenueInner {
	public:
		MsgPaoDeKuaiPlay() {}
		virtual ~MsgPaoDeKuaiPlay() {}

		static const std::string TYPE;

		virtual const std::string& getType() const {
			return TYPE;
		}

	public:
		// 出牌的牌ID列表
		std::vector<int> cardIds;

		MSGPACK_DEFINE_MAP(playerId, timestamp, nonce, signature, venueId, cardIds);
	};

	/**
	 * 通知玩家出牌消息
	 * 服务器->客户端
	 */
	class MsgPaoDeKuaiPlayNotify : public MsgBase {
	public:
		MsgPaoDeKuaiPlayNotify();
		virtual ~MsgPaoDeKuaiPlayNotify() {}

		static const std::string TYPE;

		virtual const std::string& getType() const {
			return TYPE;
		}

		MSG_PACK_IMPL

	public:
		// 出牌玩家座位号
		int seat;

		// 出牌的牌ID列表（空表示过牌）
		std::vector<int> cardIds;

		// 牌型
		int genre;

		// 下一个出牌玩家座位号
		int nextPlayer;

		// 出牌玩家剩余手牌数量
		int remainCount;

		// 当前倍数
		int multiplier;

		MSGPACK_DEFINE_MAP(seat, cardIds, genre, nextPlayer, remainCount, multiplier);
	};

	/**
	 * 出牌失败消息
	 * 服务器->客户端
	 */
	class MsgPaoDeKuaiPlayFailed : public MsgBase {
	public:
		MsgPaoDeKuaiPlayFailed() {}
		virtual ~MsgPaoDeKuaiPlayFailed() {}

		static const std::string TYPE;

		virtual const std::string& getType() const {
			return TYPE;
		}

		MSG_PACK_IMPL

	public:
		std::string errMsg;

		MSGPACK_DEFINE_MAP(errMsg);
	};

	/**
	 * 通知一局结算消息
	 * 服务器->客户端
	 */
	class MsgPaoDeKuaiSettlement : public MsgBase {
	public:
		MsgPaoDeKuaiSettlement();
		virtual ~MsgPaoDeKuaiSettlement() {}

		static const std::string TYPE;

		virtual const std::string& getType() const {
			return TYPE;
		}

		MSG_PACK_IMPL

	public:
		// 赢家座位号
		int winnerSeat;

		// 各玩家得分
		int scores[2];

		// 各玩家赢的金币
		int64_t winGolds[2];

		// 各玩家剩余手牌
		std::vector<int> remainCards[2];

		// 底注
		int baseScore;

		// 炸弹数量
		int bombCount;

		// 结算倍数
		int multiplier;

		// 是否关门/春天
		bool spring;

		MSGPACK_DEFINE_MAP(winnerSeat, scores, winGolds, remainCards,
			baseScore, bombCount, multiplier, spring);
	};
}

#endif // _NIU_MA_PAODEKUAI_MESSAGES_H_
