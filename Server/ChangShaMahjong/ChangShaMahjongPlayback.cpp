// ChangShaMahjongPlayback.cpp

#include "ChangShaMahjongPlayback.h"

namespace NiuMa
{
	ChangShaMahjongPlaybackData::ChangShaMahjongPlaybackData()
		: MahjongPlaybackData()
		, qiShouHuSeat(-1)
		, qiShouHuType(0)
		, birdMultiple(1)
	{
		for (int i = 0; i < 4; i++) {
			scores[i] = 0;
			winGolds[i] = 0.0;
		}
	}

	ChangShaMahjongPlaybackData::~ChangShaMahjongPlaybackData() {}
}
