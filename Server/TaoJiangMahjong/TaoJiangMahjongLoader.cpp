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
				, _districtId(0)
			{}

			virtual ~LoadTask() {}

		public:
			virtual QueryType buildQuery(std::string& sql) override {
				// 联合 venue 表查询 district_id，用于区域匹配房间的 Redis 维护
				sql = "select t.`number`, t.`level`, t.`rule_config`, v.`district_id` "
					"from `game_taojiang_mahjong` t left join `venue` v on t.`venue_id` = v.`id` "
					"where t.`venue_id` = \"" + _venueId + "\"";
				return QueryType::Select;
			}

			virtual int fetchResult(sql::ResultSet* res) override {
				int rows = 0;
				while (res->next()) {
					_number = res->getString("number");
					_level = res->getInt("level");
					_ruleConfig = res->getString("rule_config");
					// district_id 可能为 NULL（好友房），NULL 时返回0
					auto did = res->getInt64("district_id");
					_districtId = (did == 0 || res->wasNull()) ? 0 : static_cast<int>(did);
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

			// 区域ID（0表示好友房，不参与区域匹配）
			int _districtId;
		};
		std::shared_ptr<LoadTask> task = std::make_shared<LoadTask>(id);
		MysqlPool::getSingleton().syncQuery(task);
		if (!task->getSucceed()) {
			ErrorS << "加载桃江麻将游戏(Id: " << id << ")失败";
			return nullptr;
		}
		std::shared_ptr<TaoJiangMahjongRoom> room = std::make_shared<TaoJiangMahjongRoom>(
			id, task->_number, task->_level, task->_ruleConfig, task->_districtId);
		return room;
	}
}
