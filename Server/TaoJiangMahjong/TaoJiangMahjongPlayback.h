// TaoJiangMahjongPlayback.h

#ifndef _NIU_MA_TAOJIANG_MAHJONG_PLAYBACK_H_
#define _NIU_MA_TAOJIANG_MAHJONG_PLAYBACK_H_

#include "MahjongSettlement.h"
#include "MahjongPlayback.h"

namespace NiuMa
{
	class TaoJiangMahjongPlaybackData : public MahjongPlaybackData
	{
	public:
		TaoJiangMahjongPlaybackData();
		virtual ~TaoJiangMahjongPlaybackData();

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

#endif // !_NIU_MA_TAOJIANG_MAHJONG_PLAYBACK_H_
