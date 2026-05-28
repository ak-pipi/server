// YiYangWaiHuZiRecordTask.h
#ifndef _NIU_MA_YIYANG_WAIHUZI_RECORD_TASK_H_
#define _NIU_MA_YIYANG_WAIHUZI_RECORD_TASK_H_

#include "MySql/MysqlQueryTask.h"

namespace NiuMa
{
	class YiYangWaiHuZiRecordTask : public MysqlQueryTask {
	public:
		YiYangWaiHuZiRecordTask();
		virtual ~YiYangWaiHuZiRecordTask();
		virtual QueryType buildQuery(std::string& sql) override;
	public:
		int _roundNo;
		int _banker;
		int _scores[3];
		double _winGolds[3];
		int _huXi[3];
		std::string _venueId;
		std::string _playerIds[3];
		std::string _playback;
		std::string _randomSeedHash;
	};
}
#endif
