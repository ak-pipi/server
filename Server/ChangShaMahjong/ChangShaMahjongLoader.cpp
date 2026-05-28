// ChangShaMahjongLoader.cpp

#include "Base/Log.h"
#include "ChangShaMahjongLoader.h"
#include "ChangShaMahjongRoom.h"
#include "../GameDefines.h"
#include "MySql/MysqlPool.h"

#include <mysql/jdbc.h>

namespace NiuMa
{
	ChangShaMahjongLoader::ChangShaMahjongLoader()
		: VenueLoader(static_cast<int>(GameType::ChangShaMahjong))
	{}

	ChangShaMahjongLoader::~ChangShaMahjongLoader() {}

	Venue::Ptr ChangShaMahjongLoader::load(const std::string& id) {
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
				sql = "select `number`, `level`, `rule_config` from `game_changsha_mahjong` where `venue_id` = \"" + _venueId + "\"";
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
			std::string _number;
			int _level;
			std::string _ruleConfig;
		};
		std::shared_ptr<LoadTask> task = std::make_shared<LoadTask>(id);
		MysqlPool::getSingleton().syncQuery(task);
		if (!task->getSucceed()) {
			ErrorS << "加载长沙麻将游戏(Id: " << id << ")失败";
			return nullptr;
		}
		std::shared_ptr<ChangShaMahjongRoom> room = std::make_shared<ChangShaMahjongRoom>(
			id, task->_number, task->_level, task->_ruleConfig);
		return room;
	}
}
