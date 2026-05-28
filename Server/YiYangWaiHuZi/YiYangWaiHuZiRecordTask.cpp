// YiYangWaiHuZiRecordTask.cpp

#include "YiYangWaiHuZiRecordTask.h"
#include <sstream>

namespace NiuMa
{
	YiYangWaiHuZiRecordTask::YiYangWaiHuZiRecordTask() : _roundNo(0), _banker(0) {
		for (int i = 0; i < 3; i++) { _scores[i] = 0; _winGolds[i] = 0.0; _huXi[i] = 0; }
	}
	YiYangWaiHuZiRecordTask::~YiYangWaiHuZiRecordTask() {}

	MysqlQueryTask::QueryType YiYangWaiHuZiRecordTask::buildQuery(std::string& sql) {
		std::stringstream ss;
		ss << "insert into `game_yiyang_waihuzi_record`(`venue_id`, `round_no`, `banker`, "
			<< "`player_id0`, `score0`, `wingold0`, `huxi0`, "
			<< "`player_id1`, `score1`, `wingold1`, `huxi1`, "
			<< "`player_id2`, `score2`, `wingold2`, `huxi2`, "
			<< "`random_seed_hash`, `playback`) values(\""
			<< _venueId << "\", " << _roundNo << ", " << _banker << ", \""
			<< _playerIds[0] << "\", " << _scores[0] << ", " << _winGolds[0] << ", " << _huXi[0] << ", \""
			<< _playerIds[1] << "\", " << _scores[1] << ", " << _winGolds[1] << ", " << _huXi[1] << ", \""
			<< _playerIds[2] << "\", " << _scores[2] << ", " << _winGolds[2] << ", " << _huXi[2] << ", \""
			<< _randomSeedHash << "\", \"" << _playback << "\")";
		sql = ss.str();
		return QueryType::Insert;
	}
}
