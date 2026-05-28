// PaoDeKuaiRecordTask.h
// 跑得快一局记录保存任务

#ifndef _NIU_MA_PAODEKUAI_RECORD_TASK_H_
#define _NIU_MA_PAODEKUAI_RECORD_TASK_H_

#include "MySql/MysqlQueryTask.h"

namespace NiuMa
{
	class PaoDeKuaiRecordTask : public MysqlQueryTask
	{
	public:
		PaoDeKuaiRecordTask();
		virtual ~PaoDeKuaiRecordTask();

	public:
		virtual QueryType buildQuery(std::string& sql) override;

	public:
		// 局号
		int _roundNo;

		// 庄家座位号
		int _banker;

		// 全部玩家得分
		int _scores[2];

		// 全部玩家赢的金币数量
		int64_t _winGolds[2];

		// 场地id
		std::string _venueId;

		// 全部玩家id
		std::string _playerIds[2];

		// 回放数据，MessagePack序列化后zlib压缩再Base64编码
		std::string _playback;

		// 随机种子hash
		std::string _randomSeedHash;
	};
}

#endif // _NIU_MA_PAODEKUAI_RECORD_TASK_H_
