// ChangShaMahjongMessages.h
// 长沙麻将网络消息定义

#ifndef _NIU_MA_CHANGSHA_MAHJONG_MESSAGES_H_
#define _NIU_MA_CHANGSHA_MAHJONG_MESSAGES_H_

#include "Mahjong/MahjongMessages.h"
#include "Mahjong/MahjongSettlement.h"

namespace NiuMa
{
	class ChangShaMahjongMessages
	{
	private:
		ChangShaMahjongMessages() {}

	public:
		virtual ~ChangShaMahjongMessages() {}

		static void registMessages();
	};

	/**
	 * 请求同步长沙麻将数据
	 * 客户端->服务器
	 */
	class MsgChangShaSync : public MsgVenueInner {
	public:
		MsgChangShaSync() {}
		virtual ~MsgChangShaSync() {}

		static const std::string TYPE;

		virtual const std::string& getType() const {
			return TYPE;
		}

		MSGPACK_DEFINE_MAP(playerId, timestamp, nonce, signature, venueId);
	};

	/**
	 * 响应同步长沙麻将游戏数据
	 * 服务器->客户端
	 */
	class MsgChangShaSyncResp : public MsgBase
	{
	public:
		MsgChangShaSyncResp();
		virtual ~MsgChangShaSyncResp();

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

		int qiShouHuSeat;
		int qiShouHuType;
		int qiShouHuScore;
		std::vector<int> birdTiles;
		std::vector<int> hitSeats;
		int birdMultiple;

		MSGPACK_DEFINE_MAP(number, gold, diamond, diZhu, chi, dianPao, hasFetch,
			seat, roundState, disbandState, banker, playerCount, roundNo, roundCount, leftTiles, handTileNums,
			fetchTile, handTiles, playedTiles, chapters,
			qiShouHuSeat, qiShouHuType, qiShouHuScore, birdTiles, hitSeats, birdMultiple);
	};

	/**
	 * 开始新一局
	 * 服务器->客户端
	 */
	class MsgChangShaStartRound : public MsgBase {
	public:
		MsgChangShaStartRound();
		virtual ~MsgChangShaStartRound();

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
		int birdCount;
		bool zhongNiaoEnabled;
		bool require258Jiang;

		MSGPACK_DEFINE_MAP(banker, playerCount, roundNo, roundCount,
			birdCount, zhongNiaoEnabled, require258Jiang);
	};

	/**
	 * 结算数据
	 * 服务器->客户端
	 */
	class MsgChangShaSettlement : public MsgBase
	{
	public:
		MsgChangShaSettlement();
		virtual ~MsgChangShaSettlement();

		static const std::string TYPE;

		virtual const std::string& getType() const {
			return TYPE;
		}

		MSG_PACK_IMPL

	public:
		bool kick;
		int64_t golds[4];
		int winGolds[4];
		std::vector<int> birdTiles;
		std::vector<int> hitSeats;
		int birdMultiple;
		int qiShouHuSeat;
		int qiShouHuType;
		int qiShouHuScore;
		MahjongSettlement data;

		MSGPACK_DEFINE_MAP(kick, golds, winGolds, birdTiles, hitSeats, birdMultiple,
			qiShouHuSeat, qiShouHuType, qiShouHuScore, data);
	};

	/**
	 * 通知起手胡消息
	 * 服务器->客户端
	 */
	class MsgChangShaQiShouHu : public MsgBase {
	public:
		MsgChangShaQiShouHu();
		virtual ~MsgChangShaQiShouHu() {}

		static const std::string TYPE;

		virtual const std::string& getType() const {
			return TYPE;
		}

		MSG_PACK_IMPL

	public:
		// 起手胡玩家座位号
		int seat;

		// 起手胡类型
		int huType;

		// 得分
		int score;

		MSGPACK_DEFINE_MAP(seat, huType, score);
	};

	/**
	 * 通知中鸟结果消息
	 * 服务器->客户端
	 */
	class MsgChangShaBird : public MsgBase {
	public:
		MsgChangShaBird();
		virtual ~MsgChangShaBird() {}

		static const std::string TYPE;

		virtual const std::string& getType() const {
			return TYPE;
		}

		MSG_PACK_IMPL

	public:
		// 鸟牌列表（牌面值）
		std::vector<int> birdTiles;

		// 命中座位列表
		std::vector<int> hitSeats;

		// 倍数
		int multiple;

		MSGPACK_DEFINE_MAP(birdTiles, hitSeats, multiple);
	};

	/**
	 * 通知解散投票
	 * 服务器->客户端
	 */
	class MsgChangShaDisbandVote : public MsgBase {
	public:
		MsgChangShaDisbandVote();
		virtual ~MsgChangShaDisbandVote() {}

		static const std::string TYPE;

		virtual const std::string& getType() const {
			return TYPE;
		}

		MSG_PACK_IMPL

	public:
		// 发起者座位号
		int disbander;

		// 剩余投票时间(秒)
		int remainTime;

		// 各玩家投票结果：0=未投票，1=同意，2=拒绝
		int choices[4];

		MSGPACK_DEFINE_MAP(disbander, remainTime, choices);
	};
}

#endif // _NIU_MA_CHANGSHA_MAHJONG_MESSAGES_H_
