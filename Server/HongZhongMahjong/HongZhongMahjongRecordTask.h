// HongZhongMahjongRecordTask.h

#include "MySql/MysqlQueryTask.h"

namespace NiuMa
{
	/**
	 * 红中麻将一局记录保存任务
	 */
	class HongZhongMahjongRecordTask : public MysqlQueryTask
	{
	public:
		HongZhongMahjongRecordTask();
		virtual ~HongZhongMahjongRecordTask();

	public:
		virtual QueryType buildQuery(std::string& sql) override;

	public:
		int _roundNo;
		int _banker;
		int _scores[4];
		int _winGolds[4];
		std::string _venueId;
		std::string _playerIds[4];
		std::string _playback;
		std::string _randomSeedHash;
	};
}
