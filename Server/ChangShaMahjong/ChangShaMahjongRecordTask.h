// ChangShaMahjongRecordTask.h
// 长沙麻将一局记录保存任务

#ifndef _NIU_MA_CHANGSHA_MAHJONG_RECORD_TASK_H_
#define _NIU_MA_CHANGSHA_MAHJONG_RECORD_TASK_H_

#include "MySql/MysqlQueryTask.h"

namespace NiuMa
{
	class ChangShaMahjongRecordTask : public MysqlQueryTask
	{
	public:
		ChangShaMahjongRecordTask();
		virtual ~ChangShaMahjongRecordTask();

	public:
		virtual QueryType buildQuery(std::string& sql) override;

	public:
		// 局号数
		int _roundNo;

		// 庄家座位号
		int _banker;

		// 全部玩家得分
		int _scores[4];

		// 全部玩家赢的金币数量
		double _winGolds[4];

		// 场地id
		std::string _venueId;

		// 全部玩家id
		std::string _playerIds[4];

		// 回放数据，MessagePack序列化后zlib压缩再Base64编码
		std::string _playback;

		// 随机种子hash
		std::string _randomSeedHash;
	};
}

#endif // _NIU_MA_CHANGSHA_MAHJONG_RECORD_TASK_H_
