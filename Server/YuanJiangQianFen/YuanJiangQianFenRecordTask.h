// YuanJiangQianFenRecordTask.h
#ifndef _NIU_MA_YUANJIANG_QIANFEN_RECORD_TASK_H_
#define _NIU_MA_YUANJIANG_QIANFEN_RECORD_TASK_H_
#include "MySql/MysqlQueryTask.h"
namespace NiuMa {
	class YuanJiangQianFenRecordTask : public MysqlQueryTask {
	public:
		YuanJiangQianFenRecordTask();
		virtual ~YuanJiangQianFenRecordTask();
		virtual QueryType buildQuery(std::string& sql) override;
	public:
		int _roundNo; int _banker; int _scores[4]; int64_t _winGolds[4];
		std::string _venueId; std::string _playerIds[4]; std::string _playback; std::string _randomSeedHash;
	};
}
#endif
