// PaoDeKuaiPlayback.h
// 跑得快回放数据

#ifndef _NIU_MA_PAODEKUAI_PLAYBACK_H_
#define _NIU_MA_PAODEKUAI_PLAYBACK_H_

#include "PokerCard.h"

#include <string>
#include <vector>

#include "msgpack/msgpack.hpp"

namespace NiuMa
{
	/**
	 * 跑得快一步操作记录
	 */
	struct PaoDeKuaiStep
	{
		// 操作类型：0=出牌，1=过牌
		int action;

		// 玩家座位号
		int seat;

		// 出牌的牌ID列表
		std::vector<int> cardIds;

		// 时间戳
		int64_t timestamp;

		MSGPACK_DEFINE_MAP(action, seat, cardIds, timestamp);
	};

	/**
	 * 跑得快回放数据
	 */
	class PaoDeKuaiPlaybackData
	{
	public:
		PaoDeKuaiPlaybackData();
		virtual ~PaoDeKuaiPlaybackData();

	public:
		// 场地ID
		std::string venueId;

		// 局号
		int roundNo;

		// 庄家座位号
		int banker;

		// 玩家数
		int playerCount;

		// 各玩家ID
		std::string playerIds[2];

		// 各玩家初始手牌ID
		std::vector<int> initCards[2];

		// 随机种子hash
		std::string randomSeedHash;

		// 操作步骤
		std::vector<PaoDeKuaiStep> steps;

		// 各玩家得分
		int scores[2];

		// 各玩家赢的金币
		int64_t winGolds[2];

		// 积分显示倍率
		int scoreScale;

		// 结算信息
		std::string settlement;

		MSGPACK_DEFINE_MAP(venueId, roundNo, banker, playerCount, playerIds, initCards,
			randomSeedHash, steps, scores, winGolds, scoreScale, settlement);
	};
}

#endif // _NIU_MA_PAODEKUAI_PLAYBACK_H_
