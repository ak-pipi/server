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
	{
		_rule = std::make_shared<DouDiZhuGameRule>();
		_rule->initialise();
	}

	DouDiZhuLoader::~DouDiZhuLoader() {}

	Venue::Ptr DouDiZhuLoader::load(const std::string& id) {
		class LoadTask : public MysqlQueryTask
		{
		public:
			LoadTask(const std::string& id)
				: _venueId(id)
				, _level(0)
			{}

			virtual QueryType buildQuery(std::string& sql) override {
				sql = "select `number`, `level`, `rule_config` from `game_doudizhu` where `venue_id` = \"" + _venueId + "\"";
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
			std::string _venueId;
			std::string _number;
			int _level;
			std::string _ruleConfig;
		};

		std::shared_ptr<LoadTask> task = std::make_shared<LoadTask>(id);
		MysqlPool::getSingleton().syncQuery(task);
		if (!task->getSucceed()) {
			ErrorS << "加载斗地主游戏(Id: " << id << ")失败";
			return nullptr;
		}
		return std::make_shared<DouDiZhuRoom>(_rule, id, task->_number, task->_level, task->_ruleConfig);
	}
}
