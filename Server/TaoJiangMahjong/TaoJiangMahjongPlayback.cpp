// TaoJiangMahjongPlayback.cpp

#include "TaoJiangMahjongPlayback.h"

namespace NiuMa
{
	TaoJiangMahjongPlaybackData::TaoJiangMahjongPlaybackData()
	{
		for (int i = 0; i < 4; i++)
			winGolds[i] = 0;
	}

	TaoJiangMahjongPlaybackData::~TaoJiangMahjongPlaybackData() {}

	void TaoJiangMahjongPlaybackData::initialize() {
		MahjongPlaybackData::initialize();
		for (int i = 0; i < 4; i++)
			winGolds[i] = 0;
		settlement.initialize();
		randomSeedHash.clear();
	}
}
