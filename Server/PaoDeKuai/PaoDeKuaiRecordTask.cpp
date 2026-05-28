// PaoDeKuaiRecordTask.cpp

#include "PaoDeKuaiRecordTask.h"
#include "Base/Log.h"

#include <sstream>

namespace NiuMa
{
	PaoDeKuaiRecordTask::PaoDeKuaiRecordTask()
		: _roundNo(0)
		, _banker(0)
	{
		for (int i = 0; i < 2; i++) {
			_scores[i] = 0;
			_winGolds[i] = 0;
		}
	}

	PaoDeKuaiRecordTask::~PaoDeKuaiRecordTask() {}

	MysqlQueryTask::QueryType PaoDeKuaiRecordTask::buildQuery(std::string& sql) {
		std::stringstream ss;
		ss << "insert into `game_paodekuai_record`(`venue_id`, `round_no`, `banker`, "
			<< "`player_id0`, `score0`, `wingold0`, "
			<< "`player_id1`, `score1`, `wingold1`, "
			<< "`random_seed_hash`, `playback`) values(\""
			<< _venueId << "\", " << _roundNo << ", " << _banker << ", \""
			<< _playerIds[0] << "\", " << _scores[0] << ", " << _winGolds[0] << ", \""
			<< _playerIds[1] << "\", " << _scores[1] << ", " << _winGolds[1] << ", \""
			<< _randomSeedHash << "\", \""
			<< _playback << "\")";
		sql = ss.str();
		return QueryType::Insert;
	}
}
