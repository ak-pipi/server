// ChangShaMahjongMessages.h
// 长沙麻将网络消息定义

#ifndef _NIU_MA_CHANGSHA_MAHJONG_MESSAGES_H_
#define _NIU_MA_CHANGSHA_MAHJONG_MESSAGES_H_

#include "Mahjong/MahjongMessages.h"
#include "../StandardMahjong/StandardMahjongMessages.h"

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
	class MsgChangShaSync : public MsgMahjongSync {
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
	 * 玩家准备消息
	 * 客户端->服务器
	 */
	class MsgChangShaReady : public MsgMahjongSync {
	public:
		MsgChangShaReady() {}
		virtual ~MsgChangShaReady() {}

		static const std::string TYPE;

		virtual const std::string& getType() const {
			return TYPE;
		}

		MSGPACK_DEFINE_MAP(playerId, timestamp, nonce, signature, venueId);
	};

	/**
	 * 解散投票请求
	 * 客户端->服务器
	 */
	class MsgChangShaDisband : public MsgMahjongSync {
	public:
		MsgChangShaDisband() {}
		virtual ~MsgChangShaDisband() {}

		static const std::string TYPE;

		virtual const std::string& getType() const {
			return TYPE;
		}

		// 1=发起解散，2=同意，3=拒绝
		int choice;

		MSGPACK_DEFINE_MAP(playerId, timestamp, nonce, signature, venueId, choice);
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
