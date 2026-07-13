// PaoDeKuaiLoader.cpp

#include "Base/Log.h"
#include "PaoDeKuaiLoader.h"
#include "PaoDeKuaiRoom.h"
#include "../GameDefines.h"
#include "MySql/MysqlPool.h"

#include <mysql/jdbc.h>

namespace NiuMa
{
	PaoDeKuaiLoader::PaoDeKuaiLoader()
		: VenueLoader(static_cast<int>(GameType::PaoDeKuai))
	{}

	PaoDeKuaiLoader::~PaoDeKuaiLoader() {}

	Venue::Ptr PaoDeKuaiLoader::load(const std::string& id) {
		class LoadTask : public MysqlQueryTask
		{
		public:
			LoadTask(const std::string& id)
				: _venueId(id)
				, _level(0)
				, _districtId(0)
			{}

			virtual ~LoadTask() {}

		public:
			virtual QueryType buildQuery(std::string& sql) override {
				sql = "select t.`number`, t.`level`, t.`rule_config`, v.`district_id` "
					"from `game_paodekuai` t left join `venue` v on t.`venue_id` = v.`id` "
					"where t.`venue_id` = \"" + _venueId + "\"";
				return QueryType::Select;
			}

			virtual int fetchResult(sql::ResultSet* res) override {
				int rows = 0;
				while (res->next()) {
					_number = res->getString("number");
					_level = res->getInt("level");
					_ruleConfig = res->getString("rule_config");
					auto did = res->getInt64("district_id");
					_districtId = (did == 0 || res->wasNull()) ? 0 : static_cast<int>(did);
					rows++;
				}
				return rows;
			}

		public:
			const std::string _venueId;
			std::string _number;
			int _level;
			std::string _ruleConfig;
			int _districtId;
		};
		std::shared_ptr<LoadTask> task = std::make_shared<LoadTask>(id);
		MysqlPool::getSingleton().syncQuery(task);
		if (!task->getSucceed()) {
			ErrorS << "加载跑得快游戏(Id: " << id << ")失败";
			return nullptr;
		}
		std::shared_ptr<PaoDeKuaiRule> rule = std::make_shared<PaoDeKuaiRule>();
		rule->initialise();
		std::shared_ptr<PaoDeKuaiRoom> room = std::make_shared<PaoDeKuaiRoom>(
			rule, id, task->_number, task->_level, task->_ruleConfig, task->_districtId);
		return room;
	}
}
