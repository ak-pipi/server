// HongZhongMahjongMessages.h

#ifndef _NIU_MA_HONGZHONG_MAHJONG_MESSAGES_H_
#define _NIU_MA_HONGZHONG_MAHJONG_MESSAGES_H_

#include "MahjongMessages.h"
#include "MahjongSettlement.h"

namespace NiuMa
{
	class HongZhongMahjongMessages
	{
	private:
		HongZhongMahjongMessages() {}

	public:
		virtual ~HongZhongMahjongMessages() {}

		static void registMessages();
	};

	/**
	 * 请求同步红中麻将游戏数据消息
	 * 客户端->服务器
	 */
	class MsgHZSync : public MsgVenueInner {
	public:
		MsgHZSync() {}
		virtual ~MsgHZSync() {}

		static const std::string TYPE;

		virtual const std::string& getType() const {
			return TYPE;
		}

		MSGPACK_DEFINE_MAP(playerId, timestamp, nonce, signature, venueId);
	};

	/**
	 * 响应同步红中麻将游戏数据消息
	 * 服务器->客户端
	 */
	class MsgHZSyncResp : public MsgBase
	{
	public:
		MsgHZSyncResp();
		virtual ~MsgHZSyncResp();

		static const std::string TYPE;

		virtual const std::string& getType() const {
			return TYPE;
		}

		MSG_PACK_IMPL

	public:
		std::string number;
		int64_t gold;
		int64_t diamond;
		int diZhu;
		bool chi;
		bool dianPao;
		bool hasFetch;
		int seat;
			int roundState;
			int disbandState;
			int banker;
			int playerCount;
			int roundNo;
			int roundCount;
			int leftTiles;
			int handTileNums[4];
			MahjongTile fetchTile;
			MahjongTileArray handTiles;
		MahjongTileArray playedTiles[4];
		MahjongChapterArray chapters[4];

			MSGPACK_DEFINE_MAP(number, gold, diamond, diZhu, chi, dianPao, hasFetch,
				seat, roundState, disbandState, banker, playerCount, roundNo, roundCount, leftTiles, handTileNums,
				fetchTile, handTiles, playedTiles, chapters);
		};

	/**
	 * 开始新一局消息
	 * 服务器->客户端
	 */
	class MsgHZStartRound : public MsgBase {
	public:
		MsgHZStartRound();
		virtual ~MsgHZStartRound();

		static const std::string TYPE;

		virtual const std::string& getType() const {
			return TYPE;
		}

		MSG_PACK_IMPL

		public:
			int banker;
			int playerCount;
			int roundNo;
			int roundCount;

			MSGPACK_DEFINE_MAP(banker, playerCount, roundNo, roundCount);
		};

	/**
	 * 结算数据消息
	 * 服务器->客户端
	 */
	class MsgHZSettlement : public MsgBase
	{
	public:
		MsgHZSettlement();
		virtual ~MsgHZSettlement();

		static const std::string TYPE;

		virtual const std::string& getType() const {
			return TYPE;
		}

		MSG_PACK_IMPL

	public:
		bool kick;
			int64_t golds[4];
			int winGolds[4];
			int huScores[4];
			int gangScores[4];
			MahjongTile birdTile;
			int birdMultiplier;
			MahjongSettlement data;
			int64_t roomFeeTotal;
			std::vector<std::string> roomFeePlayerIds;
			std::vector<int64_t> roomFeeAmounts;
			int64_t shuffleFeeTotal;
			std::vector<std::string> shuffleFeePlayerIds;
			std::vector<int64_t> shuffleFeeAmounts;

			MSGPACK_DEFINE_MAP(kick, golds, winGolds, huScores, gangScores, birdTile, birdMultiplier, data,
				roomFeeTotal, roomFeePlayerIds, roomFeeAmounts,
				shuffleFeeTotal, shuffleFeePlayerIds, shuffleFeeAmounts);
		};

	/**
	 * 通知解散投票消息
	 * 服务器->客户端
	 */
	class MsgHZDisbandVote : public MsgBase
	{
	public:
		MsgHZDisbandVote();
		virtual ~MsgHZDisbandVote();

		static const std::string TYPE;

		virtual const std::string& getType() const {
			return TYPE;
		}

		MSG_PACK_IMPL

	public:
		int disbander;
		int elapsed;
		int choices[4];

		MSGPACK_DEFINE_MAP(disbander, elapsed, choices);
	};
}

#endif // !_NIU_MA_HONGZHONG_MAHJONG_MESSAGES_H_
