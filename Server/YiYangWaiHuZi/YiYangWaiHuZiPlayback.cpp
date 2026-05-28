// YiYangWaiHuZiPlayback.cpp

#include "YiYangWaiHuZiPlayback.h"

namespace NiuMa
{
	WaiHuZiPlaybackData::WaiHuZiPlaybackData() : roundNo(0), banker(0), playerCount(3) {
		for (int i = 0; i < 3; i++) { scores[i] = 0; winGolds[i] = 0.0; huXi[i] = 0; }
	}
	WaiHuZiPlaybackData::~WaiHuZiPlaybackData() {}
}
