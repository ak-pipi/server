// TaoJiangMahjongMessages.h

#ifndef _NIU_MA_TAOJIANG_MAHJONG_MESSAGES_H_
#define _NIU_MA_TAOJIANG_MAHJONG_MESSAGES_H_

#include "MahjongMessages.h"
#include "MahjongSettlement.h"

namespace NiuMa
{
	class TaoJiangMahjongMessages
	{
	private:
		TaoJiangMahjongMessages() {}

	public:
		virtual ~TaoJiangMahjongMessages() {}

		static void registMessages();
	};

	/**
	 * 请求同步桃江麻将游戏数据消息
	 * 客户端->服务器
	 */
	class MsgTJSync : public MsgVenueInner {
	public:
		MsgTJSync() {}
		virtual ~MsgTJSync() {}

		static const std::string TYPE;

		virtual const std::string& getType() const {
			return TYPE;
		}

		MSGPACK_DEFINE_MAP(playerId, timestamp, nonce, signature, venueId);
	};

	/**
	 * 响应同步桃江麻将游戏数据消息
	 * 服务器->客户端
	 */
	class MsgTJSyncResp : public MsgBase
	{
	public:
		MsgTJSyncResp();
		virtual ~MsgTJSyncResp();

		static const std::string TYPE;

		virtual const std::string& getType() const {
			return TYPE;
		}

		MSG_PACK_IMPL

	public:
		// 房间编号
		std::string number;

		// 玩家自己的金币数量
		int64_t gold;

		// 玩家自己的钻石数量
		int64_t diamond;

		// 底注
		int diZhu;

		// 是否可吃
		bool chi;

		// 是否可点炮
		bool dianPao;

		// 当前是否摸起一张牌未打出
		bool hasFetch;

		// 玩家自己的座位号
		int seat;

		// 当前局状态
		int roundState;

		// 解散状态
		int disbandState;

		// 庄家座位号
		int banker;

		int roundNo;

		int roundCount;

		// 牌池剩余牌数量
		int leftTiles;

		// 所有玩家的手牌数量
		int handTileNums[4];

		// 最新摸上的牌
		MahjongTile fetchTile;

		// 自己手牌
		MahjongTileArray handTiles;

		// 所有玩家打出的牌
		MahjongTileArray playedTiles[4];

		// 所有玩家的牌章
		MahjongChapterArray chapters[4];

		// 赖子系统
		bool laiziEnabled;
		MahjongTile wangPai;          // 王牌（赖子/万能牌，客户端展示用）
		MahjongTile mingZi;           // 明子（骰子翻出的牌）
		int dicePoint;              // 骰子点数

		// 报听信息
		bool baoTingEnabled;
		bool baoTinged[4];          // 各玩家是否已报听

		MSGPACK_DEFINE_MAP(number, gold, diamond, diZhu, chi, dianPao, hasFetch,
			seat, roundState, disbandState, banker, roundNo, roundCount, leftTiles, handTileNums,
			fetchTile, handTiles, playedTiles, chapters,
			laiziEnabled, wangPai, mingZi, dicePoint,
			baoTingEnabled, baoTinged);
	};

	/**
	 * 开始新一局消息
	 * 服务器->客户端
	 */
	class MsgTJStartRound : public MsgBase {
	public:
		MsgTJStartRound();
		virtual ~MsgTJStartRound();

		static const std::string TYPE;

		virtual const std::string& getType() const {
			return TYPE;
		}

		MSG_PACK_IMPL

	public:
		// 庄家座位号
		int banker;

		int roundNo;

		int roundCount;

		// 赖子系统
		bool laiziEnabled;
		MahjongTile wangPai;          // 王牌（赖子/万能牌）
		MahjongTile mingZi;           // 明子（骰子翻出的牌）
		int dicePoint;

		MSGPACK_DEFINE_MAP(banker, roundNo, roundCount,
			laiziEnabled, wangPai, mingZi, dicePoint);
	};

	/**
	 * 结算数据消息
	 * 服务器->客户端
	 */
	class MsgTJSettlement : public MsgBase
	{
	public:
		MsgTJSettlement();
		virtual ~MsgTJSettlement();

		static const std::string TYPE;

		virtual const std::string& getType() const {
			return TYPE;
		}

		MSG_PACK_IMPL

	public:
		// 收到消息的玩家是否因金币不足被踢出
		bool kick;

		// 结算后玩家的金币数
		int64_t golds[4];

		// 所有玩家的本局获利的金币数量
		int winGolds[4];

		// 所有玩家是否为硬庄（赖子未做万能牌）
		bool yingZhuang[4];

		// 结算数据
		MahjongSettlement data;

		MSGPACK_DEFINE_MAP(kick, golds, winGolds, yingZhuang, data);
	};

	/**
	 * 通知解散投票消息
	 * 服务器->客户端
	 */
	class MsgTJDisbandVote : public MsgBase
	{
	public:
		MsgTJDisbandVote();
		virtual ~MsgTJDisbandVote();

		static const std::string TYPE;

		virtual const std::string& getType() const {
			return TYPE;
		}

		MSG_PACK_IMPL

	public:
		// 发起解散者座位号
		int disbander;

		// 等待投票已经过了多久，秒
		int elapsed;

		// 各玩家的选择，0-未选择、1-同意、2-反对
		int choices[4];

		MSGPACK_DEFINE_MAP(disbander, elapsed, choices);
	};
}

#endif // !_NIU_MA_TAOJIANG_MAHJONG_MESSAGES_H_
