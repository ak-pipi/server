// HongZhongMahjongPlayback.cpp

#include "HongZhongMahjongPlayback.h"

namespace NiuMa
{
	HongZhongMahjongPlaybackData::HongZhongMahjongPlaybackData()
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
		randomSeedHash.clear();
	}
}
