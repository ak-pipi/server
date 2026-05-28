// YuanJiangQianFenLoader.cpp
#include "Base/Log.h"
#include "YuanJiangQianFenLoader.h"
#include "YuanJiangQianFenRoom.h"
#include "../GameDefines.h"
#include "MySql/MysqlPool.h"
#include <mysql/jdbc.h>

namespace NiuMa
{
	YuanJiangQianFenLoader::YuanJiangQianFenLoader()
		: VenueLoader(static_cast<int>(GameType::YuanJiangQianFen)) {}
	YuanJiangQianFenLoader::~YuanJiangQianFenLoader() {}

	Venue::Ptr YuanJiangQianFenLoader::load(const std::string& id) {
		class LoadTask : public MysqlQueryTask {
		public:
			LoadTask(const std::string& id) : _venueId(id), _level(0) {}
			virtual ~LoadTask() {}
			virtual QueryType buildQuery(std::string& sql) override {
				sql = "select `number`, `level`, `rule_config` from `game_yuanjiang_qianfen` where `venue_id` = \"" + _venueId + "\"";
				return QueryType::Select;
			}
			virtual int fetchResult(sql::ResultSet* res) override {
				int rows = 0;
				while (res->next()) {
					_number = res->getString("number"); _level = res->getInt("level"); _ruleConfig = res->getString("rule_config"); rows++;
				}
				return rows;
			}
		public:
			const std::string _venueId; std::string _number; int _level; std::string _ruleConfig;
		};
		auto task = std::make_shared<LoadTask>(id);
		MysqlPool::getSingleton().syncQuery(task);
		if (!task->getSucceed()) { ErrorS << "加载沅江千分游戏(Id: " << id << ")失败"; return nullptr; }
		return std::make_shared<YuanJiangQianFenRoom>(id, task->_number, task->_level, task->_ruleConfig);
	}
}
