// TaoJiangMahjongLoader.cpp

#include "Base/Log.h"
#include "TaoJiangMahjongLoader.h"
#include "TaoJiangMahjongRoom.h"
#include "../GameDefines.h"
#include "MySql/MysqlPool.h"

#include <mysql/jdbc.h>

namespace NiuMa
{
	TaoJiangMahjongLoader::TaoJiangMahjongLoader()
		: VenueLoader(static_cast<int>(GameType::TaoJiangMahjong))
	{}

	TaoJiangMahjongLoader::~TaoJiangMahjongLoader() {}

	Venue::Ptr TaoJiangMahjongLoader::load(const std::string& id) {
		class LoadTask : public MysqlQueryTask
		{
		public:
			LoadTask(const std::string& id)
				: _venueId(id)
				, _level(0)
			{}

			virtual ~LoadTask() {}

		public:
			virtual QueryType buildQuery(std::string& sql) override {
				sql = "select `number`, `level`, `rule_config` from `game_taojiang_mahjong` where `venue_id` = \"" + _venueId + "\"";
				return QueryType::Select;
			}

			virtual int fetchResult(sql::ResultSet* res) override {
				int rows = 0;
				while (res->next()) {
					_number = res->getString("number");
					_level = res->getInt("level");
					_ruleConfig = res->getString("rule_config");
					rows++;
				}
				return rows;
			}

		public:
			const std::string _venueId;

			// 房间编号
			std::string _number;

			// 等级
			int _level;

			// 玩法配置JSON
			std::string _ruleConfig;
		};
		std::shared_ptr<LoadTask> task = std::make_shared<LoadTask>(id);
		MysqlPool::getSingleton().syncQuery(task);
		if (!task->getSucceed()) {
			ErrorS << "加载桃江麻将游戏(Id: " << id << ")失败";
			return nullptr;
		}
		std::shared_ptr<TaoJiangMahjongRoom> room = std::make_shared<TaoJiangMahjongRoom>(
			id, task->_number, task->_level, task->_ruleConfig);
		return room;
	}
}
