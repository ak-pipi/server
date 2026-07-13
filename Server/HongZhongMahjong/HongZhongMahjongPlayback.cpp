// HongZhongMahjongPlayback.cpp

#include "HongZhongMahjongPlayback.h"

namespace NiuMa
{
	HongZhongMahjongPlaybackData::HongZhongMahjongPlaybackData()
		: birdMultiplier(1)
	{
		for (int i = 0; i < 4; i++)
			winGolds[i] = 0;
	}

	HongZhongMahjongPlaybackData::~HongZhongMahjongPlaybackData() {}

	void HongZhongMahjongPlaybackData::initialize() {
		MahjongPlaybackData::initialize();
		for (int i = 0; i < 4; i++)
			winGolds[i] = 0;
		settlement.initialize();
		birdTile = MahjongTile();
		birdMultiplier = 1;
		randomSeedHash.clear();
	}
}
