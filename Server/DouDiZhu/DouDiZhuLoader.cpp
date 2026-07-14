// DouDiZhuLoader.cpp

#include "Base/Log.h"
#include "DouDiZhuLoader.h"
#include "DouDiZhuRoom.h"
#include "../GameDefines.h"
#include "MySql/MysqlPool.h"

#include <mysql/jdbc.h>

namespace NiuMa
{
	DouDiZhuLoader::DouDiZhuLoader()
		: VenueLoader(static_cast<int>(GameType::DouDiZhu))
	{}

	DouDiZhuLoader::~DouDiZhuLoader() {}

	Venue::Ptr DouDiZhuLoader::load(const std::string& id) {
		class LoadTask : public MysqlQueryTask
		{
		public:
			LoadTask(const std::string& id)
				: _venueId(id)
				, _level(0)
				, _districtId(0)
			{}

			virtual QueryType buildQuery(std::string& sql) override {
				sql = "select t.`number`, t.`level`, t.`rule_config`, v.`district_id` "
					"from `game_doudizhu` t left join `venue` v on t.`venue_id` = v.`id` "
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
			std::string _venueId;
			std::string _number;
			int _level;
			std::string _ruleConfig;
			int _districtId;
		};

		std::shared_ptr<LoadTask> task = std::make_shared<LoadTask>(id);
		MysqlPool::getSingleton().syncQuery(task);
		if (!task->getSucceed()) {
			ErrorS << "加载斗地主游戏(Id: " << id << ")失败";
			return nullptr;
		}
		std::shared_ptr<DouDiZhuGameRule> rule = std::make_shared<DouDiZhuGameRule>();
		rule->initialise();
		return std::make_shared<DouDiZhuRoom>(
			rule, id, task->_number, task->_level, task->_ruleConfig, task->_districtId);
	}
}
