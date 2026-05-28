// HongZhongMahjongPlayback.h

#ifndef _NIU_MA_HONGZHONG_MAHJONG_PLAYBACK_H_
#define _NIU_MA_HONGZHONG_MAHJONG_PLAYBACK_H_

#include "MahjongSettlement.h"
#include "MahjongPlayback.h"

namespace NiuMa
{
	class HongZhongMahjongPlaybackData : public MahjongPlaybackData
	{
	public:
		HongZhongMahjongPlaybackData();
		virtual ~HongZhongMahjongPlaybackData();

	public:
		virtual void initialize();

	public:
		// 所有玩家的本局获利的金币数量
		int winGolds[4];

		// 结算数据
		MahjongSettlement settlement;

		// 随机种子hash（合规随机算法审计）
		std::string randomSeedHash;

		MSGPACK_DEFINE_MAP(dealedTiles, chapters, actions, actors, winGolds, settlement, randomSeedHash);
	};
}

#endif // !_NIU_MA_HONGZHONG_MAHJONG_PLAYBACK_H_
