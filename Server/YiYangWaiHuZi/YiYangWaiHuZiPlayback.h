// YiYangWaiHuZiPlayback.h
#ifndef _NIU_MA_YIYANG_WAIHUZI_PLAYBACK_H_
#define _NIU_MA_YIYANG_WAIHUZI_PLAYBACK_H_

#include <string>
#include <vector>
#include "msgpack/msgpack.hpp"

namespace NiuMa
{
	struct WaiHuZiStep {
		int action;
		int seat;
		std::vector<int> cardIds;
		int64_t timestamp;
		MSGPACK_DEFINE_MAP(action, seat, cardIds, timestamp);
	};

	class WaiHuZiPlaybackData {
	public:
		WaiHuZiPlaybackData();
		virtual ~WaiHuZiPlaybackData();
	public:
		std::string venueId;
		int roundNo;
		int banker;
		int playerCount;
		std::string playerIds[3];
		std::vector<int> initCards[3];
		std::string randomSeedHash;
		std::vector<WaiHuZiStep> steps;
		int scores[3];
		double winGolds[3];
		int huXi[3];
		std::string settlement;
		MSGPACK_DEFINE_MAP(venueId, roundNo, banker, playerCount, playerIds, initCards, randomSeedHash, steps, scores, winGolds, huXi, settlement);
	};
}
#endif
