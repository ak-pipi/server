// PaoDeKuaiPlayback.cpp

#include "PaoDeKuaiPlayback.h"

namespace NiuMa
{
	PaoDeKuaiPlaybackData::PaoDeKuaiPlaybackData()
		: roundNo(0)
		, banker(0)
		, playerCount(2)
	{
		for (int i = 0; i < 2; i++) {
			scores[i] = 0;
			winGolds[i] = 0;
		}
	}

	PaoDeKuaiPlaybackData::~PaoDeKuaiPlaybackData() {}
}
