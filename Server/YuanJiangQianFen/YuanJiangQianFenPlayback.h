// YuanJiangQianFenPlayback.h
#ifndef _NIU_MA_YUANJIANG_QIANFEN_PLAYBACK_H_
#define _NIU_MA_YUANJIANG_QIANFEN_PLAYBACK_H_
#include <string>
#include <vector>
#include "msgpack/msgpack.hpp"
namespace NiuMa {
	struct QianFenStep {
		int action; int seat; std::vector<int> cardIds; int64_t timestamp;
		MSGPACK_DEFINE_MAP(action, seat, cardIds, timestamp);
	};
	class QianFenPlaybackData {
	public:
		QianFenPlaybackData();
		virtual ~QianFenPlaybackData();
		std::string venueId; int roundNo; int banker; int playerCount;
		std::string playerIds[4]; std::vector<int> initCards[4]; std::string randomSeedHash;
		std::vector<QianFenStep> steps; int scores[4]; int64_t winGolds[4]; std::string settlement;
		MSGPACK_DEFINE_MAP(venueId, roundNo, banker, playerCount, playerIds, initCards, randomSeedHash, steps, scores, winGolds, settlement);
	};
}
#endif
