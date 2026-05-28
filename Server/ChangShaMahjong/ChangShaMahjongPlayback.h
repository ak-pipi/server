// ChangShaMahjongPlayback.h
// 长沙麻将回放数据

#ifndef _NIU_MA_CHANGSHA_MAHJONG_PLAYBACK_H_
#define _NIU_MA_CHANGSHA_MAHJONG_PLAYBACK_H_

#include "Mahjong/MahjongPlayback.h"

#include <string>
#include <vector>

#include "msgpack/msgpack.hpp"

namespace NiuMa
{
	/**
	 * 长沙麻将回放数据（扩展基础回放数据）
	 */
	class ChangShaMahjongPlaybackData : public MahjongPlaybackData
	{
	public:
		ChangShaMahjongPlaybackData();
		virtual ~ChangShaMahjongPlaybackData();

	public:
		// 各玩家得分
		int scores[4];

		// 各玩家赢的金币
		double winGolds[4];

		// 结算信息
		std::string settlement;

		// 起手胡座位和类型（-1表示无）
		int qiShouHuSeat;
		int qiShouHuType;

		// 中鸟数据
		std::vector<int> birdTiles;
		std::vector<int> hitSeats;
		int birdMultiple;

		// 随机种子hash
		std::string randomSeedHash;

		MSGPACK_DEFINE_MAP(dealedTiles, chapters, actions, actors,
			scores, winGolds, settlement,
			qiShouHuSeat, qiShouHuType, birdTiles, hitSeats, birdMultiple,
			randomSeedHash);
	};
}

#endif // _NIU_MA_CHANGSHA_MAHJONG_PLAYBACK_H_
