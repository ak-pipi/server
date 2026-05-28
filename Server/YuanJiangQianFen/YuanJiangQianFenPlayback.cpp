// YuanJiangQianFenPlayback.cpp
#include "YuanJiangQianFenPlayback.h"
namespace NiuMa {
	QianFenPlaybackData::QianFenPlaybackData() : roundNo(0), banker(0), playerCount(4) {
		for (int i = 0; i < 4; i++) { scores[i] = 0; winGolds[i] = 0; }
	}
	QianFenPlaybackData::~QianFenPlaybackData() {}
}
