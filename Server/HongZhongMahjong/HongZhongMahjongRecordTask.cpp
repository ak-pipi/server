// HongZhongMahjongRecordTask.cpp

#include "HongZhongMahjongRecordTask.h"
#include "Base/Log.h"

#include <sstream>

namespace NiuMa
{
	HongZhongMahjongRecordTask::HongZhongMahjongRecordTask()
		: _roundNo(0)
		, _banker(0)
	{
		for (int i = 0; i < 4; i++) {
			_scores[i] = 0;
			_winGolds[i] = 0;
		}
	}

	HongZhongMahjongRecordTask::~HongZhongMahjongRecordTask() {}

	MysqlQueryTask::QueryType HongZhongMahjongRecordTask::buildQuery(std::string& sql) {
		std::stringstream ss;
		ss << "insert into `game_hongzhong_mahjong_record`(`venue_id`, `round_no`, `banker`, "
			<< "`player_id0`, `score0`, `wingold0`, "
			<< "`player_id1`, `score1`, `wingold1`, "
			<< "`player_id2`, `score2`, `wingold2`, "
			<< "`player_id3`, `score3`, `wingold3`, "
			<< "`random_seed_hash`, `playback`, `time`) values(\""
			<< _venueId << "\", " << _roundNo << ", " << _banker << ", \""
			<< _playerIds[0] << "\", " << _scores[0] << ", " << _winGolds[0] << ", \""
			<< _playerIds[1] << "\", " << _scores[1] << ", " << _winGolds[1] << ", \""
			<< _playerIds[2] << "\", " << _scores[2] << ", " << _winGolds[2] << ", \""
			<< _playerIds[3] << "\", " << _scores[3] << ", " << _winGolds[3] << ", \""
			<< _randomSeedHash << "\", \""
			<< _playback << "\", now())";
		sql = ss.str();
		return QueryType::Insert;
	}
}
