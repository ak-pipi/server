// TaoJiangMahjongRoom.cpp

#include "Base/BaseUtils.h"
#include "Base/Log.h"
#include "Game/GetCapitalTask.h"
#include "MySql/MysqlPool.h"
#include "Venue/VenueInnerHandler.h"
#include "TaoJiangMahjongAvatar.h"
#include "TaoJiangMahjongRoom.h"
#include "TaoJiangMahjongMessages.h"
#include "TaoJiangMahjongPlayback.h"
#include "TaoJiangMahjongRecordTask.h"
#include "Game/GameMessages.h"
#include "Game/DebtLiquidation.h"
#include "Game/ReplayUtils.h"
#include "Player/PlayerManager.h"
#include "Constant/RedisKeys.h"
#include "Redis/RedisPool.h"
#include "jsoncpp/include/json/json.h"

#include <sstream>
#include <cmath>
#include <map>
#include <set>
#include <zlib.h>

#include <mysql/jdbc.h>

namespace NiuMa
{
	/**
	 * 桃江麻将规则：无字牌（东南西北中发白），108张牌
	 * 支持赖子万能牌
	 */
	class TaoJiangMahjongRule : public MahjongRule
	{
	public:
		bool hasZiPai() const override { return false; }

		// 桃江麻将将牌必须是2、5、8数字的牌（清一色时不需要2/5/8将牌）
		bool isValidJiangTile(const MahjongTile::Tile& tile) const override {
			// 清一色不需要限制将牌
			return true;
		}

		/**
		 * 重写听牌检测，支持赖子万能牌
		 * 算法：对手中每个赖子牌，尝试替换为所有可能的牌面，
		 * 检查替换后的手牌是否能胡牌
		 */
		void checkTingPai(const MahjongTileArray& tiles, const MahjongTile::TileArray& allGotTiles,
			MahjongGenre::TingPaiArray& tps, MahjongAvatar* pAvatar) const override {
			tps.clear();
			if (tiles.size() > 13)
				return;

			// 从avatar获取赖子牌信息
			MahjongTile::Tile laiziTile;
			laiziTile.setInvalid();
			if (pAvatar != nullptr) {
				// 尝试从 TaoJiangMahjongAvatar 获取赖子
				TaoJiangMahjongAvatar* tjAvatar = dynamic_cast<TaoJiangMahjongAvatar*>(pAvatar);
				if (tjAvatar != nullptr) {
					laiziTile = tjAvatar->getLaiZi();
				}
			}

			// 统计手牌中的赖子数量
			int laiziCount = 0;
			if (laiziTile.isValid()) {
				for (const auto& mt : tiles) {
					if (mt.getTile() == laiziTile)
						laiziCount++;
				}
			}

			// 构建不含赖子的手牌（赖子作为万能牌处理）
			MahjongTileArray normalTiles;
			for (const auto& mt : tiles) {
				if (mt.getTile() != laiziTile) {
					normalTiles.push_back(mt);
				}
			}

		// 遍历所有可能的牌面（筒/条/万 × 1~9 = 27种），
		// 模拟摸到该牌后，加上赖子万能牌能否胡牌
		std::set<MahjongTile::Tile> tingSet; // 去重
		for (int pi = 0; pi < 3; pi++) {
			MahjongTile::Pattern pat = static_cast<MahjongTile::Pattern>(
				static_cast<int>(MahjongTile::Pattern::Tong) + pi);
			for (int ni = 1; ni <= 9; ni++) {
				MahjongTile::Tile candidate(pat, static_cast<MahjongTile::Number>(ni));

				// 关键：如果候选牌本身就是赖子牌，摸到它后它也作为万能牌使用
				// 此时万能牌数量 = laiziCount + 1（手中已有赖子 + 新摸到的赖子）
				bool candidateIsLaizi = laiziTile.isValid() && candidate == laiziTile;

				if (candidateIsLaizi) {
					// 摸到赖子牌：normalTiles不变，万能牌数量+1
					// 桃江麻将平胡规则：雀头必须是2、5、8点数
					MahjongTileArray testTiles = normalTiles;
					std::sort(testTiles.begin(), testTiles.end());
					if (canWinWithWildcards(testTiles, laiziCount + 1, 0, true)) {
						bool allTaken = false;
						for (const auto& gt : allGotTiles) {
							if (gt == candidate) {
								allTaken = true;
								break;
							}
						}
						if (!allTaken && tingSet.find(candidate) == tingSet.end()) {
							tingSet.insert(candidate);
							MahjongGenre::TingPai tp;
							tp.tile = candidate;
							tp.style = static_cast<int>(MahjongGenre::HuStyle::PingHu);
							tps.push_back(tp);
						}
					}
				} else {
					// 摸到普通牌：normalTiles + candidate，万能牌数量=laiziCount
					// 桃江麻将平胡规则：雀头必须是2、5、8点数
					MahjongTileArray testTiles = normalTiles;
					MahjongTile candMt;
					candMt.setTile(candidate);
					testTiles.push_back(candMt);
					std::sort(testTiles.begin(), testTiles.end());

					if (canWinWithWildcards(testTiles, laiziCount, 0, true)) {
						bool allTaken = false;
						for (const auto& gt : allGotTiles) {
							if (gt == candidate) {
								allTaken = true;
								break;
							}
						}
						if (!allTaken && tingSet.find(candidate) == tingSet.end()) {
							tingSet.insert(candidate);
							MahjongGenre::TingPai tp;
							tp.tile = candidate;
							tp.style = static_cast<int>(MahjongGenre::HuStyle::PingHu);
							tps.push_back(tp);
						}
					}
				}
			}
		}

			// 也检查七小对
			checkQiXiaoDuiWithLaizi(tiles, normalTiles, laiziCount, laiziTile, allGotTiles, tps, tingSet);

			// 检查将将胡听牌：全部为2/5/8的牌即可胡，不要求面子结构
			// 赖子也算2/5/8（赖子做万能牌时可以当任意2/5/8使用）
			// 需要同时检查碰杠中的牌是否也是2/5/8
			{
				// 检查碰杠中的牌是否全是2/5/8
				bool chaptersAll258 = true;
				if (pAvatar != nullptr) {
					const MahjongChapterArray& chapters = pAvatar->getChapters();
					for (const auto& ch : chapters) {
						const MahjongTileArray& chTiles = ch.getAllTiles();
						for (const auto& ct : chTiles) {
							MahjongTile::Number num = ct.getNumber();
							if (num != MahjongTile::Number::Er &&
								num != MahjongTile::Number::Wu &&
								num != MahjongTile::Number::Ba) {
								chaptersAll258 = false;
								break;
							}
						}
						if (!chaptersAll258) break;
					}
				} else {
					chaptersAll258 = true;
				}

				if (chaptersAll258) {
					// 手牌中非赖子牌必须是2/5/8
					bool normalAll258 = true;
					for (const auto& mt : normalTiles) {
						MahjongTile::Number num = mt.getNumber();
						if (num != MahjongTile::Number::Er &&
							num != MahjongTile::Number::Wu &&
							num != MahjongTile::Number::Ba) {
							normalAll258 = false;
							break;
						}
					}

					if (normalAll258) {
						// 所有手牌（含赖子）和碰杠牌都是2/5/8，可以听将将胡
						// 摸到任意2/5/8的牌即可胡（总共14张全2/5/8）
						// 总牌数 = normalTiles.size() + laiziCount = 13
						// 摸一张2/5/8后 = 14张全2/5/8
						for (int pi = 0; pi < 3; pi++) {
							MahjongTile::Pattern pat = static_cast<MahjongTile::Pattern>(
								static_cast<int>(MahjongTile::Pattern::Tong) + pi);
							for (int ni_idx = 0; ni_idx < 3; ni_idx++) {
								MahjongTile::Number nums258[3] = {
									MahjongTile::Number::Er,
									MahjongTile::Number::Wu,
									MahjongTile::Number::Ba
								};
								MahjongTile::Tile candidate(pat, nums258[ni_idx]);

								// 检查是否已被全部拿完
								bool allTaken = false;
								for (const auto& gt : allGotTiles) {
									if (gt == candidate) { allTaken = true; break; }
								}
								if (allTaken) continue;

								if (tingSet.find(candidate) == tingSet.end()) {
									tingSet.insert(candidate);
									MahjongGenre::TingPai tp;
									tp.tile = candidate;
									tp.style = static_cast<int>(MahjongGenre::HuStyle::JiangJiangHu);
									tps.push_back(tp);
								}
							}
						}
					}
				}
			}

			// 调试日志：输出听牌结果
			{
				std::string handStr;
				MahjongRule::getTileArrayString(tiles, handStr);
				std::string laiziStr;
				laiziTile.toString(laiziStr);
				std::stringstream ss;
				ss << "[TaoJiangMahjong] checkTingPai: 玩家Id=" << (pAvatar ? pAvatar->getPlayerId() : "null")
					<< ", 手牌=" << handStr
					<< ", 赖子=" << (laiziTile.isValid() ? laiziStr : "无")
					<< ", 赖子数量=" << laiziCount
					<< ", 听牌数=" << tps.size();
				for (const auto& tp : tps) {
					std::string tStr;
					tp.tile.toString(tStr);
					ss << " [" << tStr << "]";
				}
				LOG_INFO(ss.str());
			}
		}

	private:
		/**
		 * 递归检查手牌加万能牌是否能胡牌
		 * tiles: 当前手牌（不含赖子），size应为 (14 - wildcardsUsed - 3*depth*?)
		 * wildLeft: 剩余可用的万能牌数量
		 * depth: 递归深度（防止无限递归）
		 * requireJiang258: 是否要求雀头必须是2/5/8点数（桃江麻将平胡规则）
		 *
		 * 胡牌型：n个面子(刻子/顺子) + 1个雀头(对子)
		 * 总牌数 = 3n + 2
		 */
		bool canWinWithWildcards(MahjongTileArray& tiles, int wildLeft, int depth, bool requireJiang258 = false) const {
			int size = static_cast<int>(tiles.size());
			// 手牌+万能牌总数应该是 3n+2（14张胡牌型）
			if ((size + wildLeft) % 3 != 2)
				return false;

			return canWinWithJiang(tiles, wildLeft, depth, requireJiang258);
		}

		/**
		 * 尝试选雀头后检查剩余牌能否全部组成面子
		 * requireJiang258: 是否要求雀头必须是2、5、8点数（桃江麻将平胡规则）
		 */
		bool canWinWithJiang(MahjongTileArray& tiles, int wildLeft, int depth, bool requireJiang258 = false) const {
			int size = static_cast<int>(tiles.size());
			if (size == 0 && wildLeft == 0)
				return true;
			if (size == 0 && wildLeft >= 0) {
				// 无剩余普通牌，万能牌需要组成2张雀头 + 3n个面子
				// wildLeft 必须能组成 3n+2 的结构
				return (wildLeft >= 2 && (wildLeft - 2) % 3 == 0);
			}
			if (wildLeft < 0)
				return false;

			// 辅助函数：判断一张牌的点数是否是2、5或8
			auto is258 = [](const MahjongTile::Tile& t) -> bool {
				int n = static_cast<int>(t.getNumber());
				return n == 2 || n == 5 || n == 8;
			};

			// 尝试每种可能的雀头
			for (int i = 0; i < size; i++) {
				// 找到第一个不小于tiles[i]的牌
				if (i + 1 < size && tiles[i].getTile() == tiles[i + 1].getTile()) {
					// 普通对子做雀头
					MahjongTile::Tile jiang = tiles[i].getTile();

					// 平胡规则：雀头必须是2、5、8点数
					if (requireJiang258 && !is258(jiang)) {
						// 跳过相同牌
						while (i + 1 < size && tiles[i].getTile() == tiles[i + 1].getTile())
							i++;
						continue;
					}

					// 移除两张
					MahjongTileArray remaining;
					for (int j = 0; j < size; j++) {
						if (j == i || j == i + 1) continue;
						remaining.push_back(tiles[j]);
					}
					if (canFormMelds(remaining, wildLeft, depth + 1))
						return true;
					// 跳过相同牌
					while (i + 1 < size && tiles[i].getTile() == tiles[i + 1].getTile())
						i++;
				}
			}

			// 尝试用万能牌做雀头（需要2个万能牌）
			// 万能牌可以当任意牌使用，包括2/5/8，所以不受requireJiang258限制
			if (wildLeft >= 2) {
				if (canFormMelds(tiles, wildLeft - 2, depth + 1))
					return true;
			}

			// 尝试用1个万能牌 + 1张普通牌做雀头（万能牌当该牌使用）
			// 这是赖子做万能牌时的常见场景：如手牌有1张5万+1个赖子，赖子当5万组成55万雀头
			if (wildLeft >= 1) {
				for (int i = 0; i < size; i++) {
					MahjongTile::Tile jiang = tiles[i].getTile();
					// 平胡规则：雀头必须是2、5、8点数
					if (requireJiang258 && !is258(jiang))
						continue;

					// 用1个赖子 + tiles[i] 做雀头，移除这1张普通牌
					MahjongTileArray remaining;
					for (int j = 0; j < size; j++) {
						if (j == i) continue;
						remaining.push_back(tiles[j]);
					}
					if (canFormMelds(remaining, wildLeft - 1, depth + 1))
						return true;
				}
			}

			return false;
		}

		/**
		 * 检查牌能否全部组成面子（刻子或顺子），可以使用万能牌补齐
		 * 使用按花色统计 + 递归回溯，避免贪心法的遗漏问题
		 */
		bool canFormMelds(MahjongTileArray& tiles, int wildLeft, int depth) const {
			if (depth > 20) // 安全阀
				return false;

			int size = static_cast<int>(tiles.size());
			// size + wildLeft 应该是3的倍数
			if ((size + wildLeft) % 3 != 0)
				return false;
			if (size == 0)
				return (wildLeft % 3 == 0);

			// 统计每种牌的数量（按花色+点数）
			int counts[3][9] = { {0} }; // [花色0~2][点数0~8]
			for (int i = 0; i < size; i++) {
				int p = static_cast<int>(tiles[i].getPattern()) - static_cast<int>(MahjongTile::Pattern::Tong);
				int n = static_cast<int>(tiles[i].getNumber()) - 1;
				if (p >= 0 && p < 3 && n >= 0 && n < 9)
					counts[p][n]++;
			}

			// 从第一个花色开始，找到第一个有牌的位置，进行回溯消除
			for (int p = 0; p < 3; p++) {
				for (int n = 0; n < 9; n++) {
					if (counts[p][n] > 0) {
						return canEliminateMelds(counts, wildLeft, p, n);
					}
				}
			}
			return (wildLeft % 3 == 0);
		}

		/**
		 * 递归消除面子：从 (pat, num) 位置开始，尝试所有可能的消除方式
		 * counts: 每种牌的剩余数量 [花色0~2][点数0~8]
		 *
		 * 当前牌 (pat, num) 必须被消耗，方式包括：
		 * 1. 刻子：(num, num, num)，万能补缺
		 * 2. 顺子 (num, num+1, num+2)：当前牌做第一张
		 * 3. 顺子 (num-1, num, num+1)：当前牌做第二张
		 * 4. 顺子 (num-2, num-1, num)：当前牌做第三张
		 * 以上顺子中万能可以补任意缺失位
		 */
		bool canEliminateMelds(int counts[3][9], int wildLeft, int pat, int num) const {
			// 找下一个有牌的位置
			while (pat < 3) {
				while (num < 9 && counts[pat][num] == 0)
					num++;
				if (num < 9)
					break;
				pat++;
				num = 0;
			}
			if (pat >= 3) {
				// 所有牌已消除完，剩余万能牌必须是3的倍数
				return (wildLeft % 3 == 0);
			}

			int c = counts[pat][num];

			// === 1. 尝试刻子 ===
			if (c >= 3) {
				counts[pat][num] -= 3;
				if (canEliminateMelds(counts, wildLeft, pat, num))
					return true;
				counts[pat][num] += 3;
			}

			// === 2. 顺子 (num, num+1, num+2)，当前牌做第一张 ===
			if (num <= 6) {
				int c2 = counts[pat][num + 1];
				int c3 = counts[pat][num + 2];
				// 计算需要万能补的位数（不含第一张，第一张已有 counts[pat][num] >= 1）
				int need2 = (c2 > 0) ? 0 : 1;
				int need3 = (c3 > 0) ? 0 : 1;
				int needTotal = need2 + need3;
				if (wildLeft >= needTotal && needTotal <= 2) {
					counts[pat][num]--;
					if (c2 > 0) counts[pat][num + 1]--;
					if (c3 > 0) counts[pat][num + 2]--;
					if (canEliminateMelds(counts, wildLeft - needTotal, pat, num))
						return true;
					counts[pat][num]++;
					if (c2 > 0) counts[pat][num + 1]++;
					if (c3 > 0) counts[pat][num + 2]++;
				}
			}

			// === 3. 顺子 (num-1, num, num+1)，当前牌做第二张 ===
			if (num >= 1 && num <= 7) {
				int c1 = counts[pat][num - 1];
				int c3 = counts[pat][num + 1];
				int need1 = (c1 > 0) ? 0 : 1;
				int need3 = (c3 > 0) ? 0 : 1;
				int needTotal = need1 + need3;
				if (wildLeft >= needTotal && needTotal <= 2) {
					counts[pat][num]--;
					if (c1 > 0) counts[pat][num - 1]--;
					if (c3 > 0) counts[pat][num + 1]--;
					if (canEliminateMelds(counts, wildLeft - needTotal, pat, num))
						return true;
					counts[pat][num]++;
					if (c1 > 0) counts[pat][num - 1]++;
					if (c3 > 0) counts[pat][num + 1]++;
				}
			}

			// === 4. 顺子 (num-2, num-1, num)，当前牌做第三张 ===
			if (num >= 2) {
				int c1 = counts[pat][num - 2];
				int c2 = counts[pat][num - 1];
				int need1 = (c1 > 0) ? 0 : 1;
				int need2 = (c2 > 0) ? 0 : 1;
				int needTotal = need1 + need2;
				if (wildLeft >= needTotal && needTotal <= 2) {
					counts[pat][num]--;
					if (c1 > 0) counts[pat][num - 2]--;
					if (c2 > 0) counts[pat][num - 1]--;
					if (canEliminateMelds(counts, wildLeft - needTotal, pat, num))
						return true;
					counts[pat][num]++;
					if (c1 > 0) counts[pat][num - 2]++;
					if (c2 > 0) counts[pat][num - 1]++;
				}
			}

			// === 5. 万能牌补刻子 ===
			if (wildLeft > 0) {
				int need = 3 - c;
				if (need > 0 && wildLeft >= need) {
					counts[pat][num] = 0;
					if (canEliminateMelds(counts, wildLeft - need, pat, num))
						return true;
					counts[pat][num] = c;
				}
			}

			return false;
		}

		/**
		 * 七小对 + 赖子万能牌检查
		 */
		void checkQiXiaoDuiWithLaizi(const MahjongTileArray& allTiles,
			const MahjongTileArray& normalTiles, int laiziCount,
			const MahjongTile::Tile& laiziTile,
			const MahjongTile::TileArray& allGotTiles,
			MahjongGenre::TingPaiArray& tps,
			std::set<MahjongTile::Tile>& tingSet) const {

			// 七小对需要14张牌
			// 普通牌 + 赖子 = 13张，摸一张后 = 14张
			// 七小对：7个对子
			// 用万能牌补齐不够的对子数
			int normalCount = static_cast<int>(normalTiles.size()); // 不含赖子的牌数

			// 计算普通牌中已有的对子数
			std::map<MahjongTile::Tile, int> tileCounts;
			for (const auto& mt : normalTiles) {
				tileCounts[mt.getTile()]++;
			}
			int pairCount = 0;
			int singleCount = 0;
			int tripleCount = 0;
			for (const auto& kv : tileCounts) {
				if (kv.second >= 2) pairCount += kv.second / 2;
				if (kv.second % 2 == 1) singleCount++;
			}

			// 每个赖子可以补一个对子的一半，也可以自己配对
			// 需要总共7个对子
			// 已有 pairCount 个对子，singleCount 个单牌
			// laiziCount 个赖子：赖子自身也可以配对 (laiziCount / 2) 个对子
			// 剩余赖子可以配单牌或补对子

			// 遍历候选摸牌
			for (int pi = 0; pi < 3; pi++) {
				MahjongTile::Pattern pat = static_cast<MahjongTile::Pattern>(
					static_cast<int>(MahjongTile::Pattern::Tong) + pi);
				for (int ni = 1; ni <= 9; ni++) {
					MahjongTile::Tile candidate(pat, static_cast<MahjongTile::Number>(ni));

					// 检查是否已被全部拿完
					bool allTaken = false;
					for (const auto& gt : allGotTiles) {
						if (gt == candidate) { allTaken = true; break; }
					}
					if (allTaken) continue;

					// 如果候选牌是赖子，它作为万能牌使用，万能牌数量+1
					bool candidateIsLaizi = laiziTile.isValid() && candidate == laiziTile;
					int wildCount = candidateIsLaizi ? (laiziCount + 1) : laiziCount;

					// 模拟摸到该牌后的对子情况
					auto tc = tileCounts;
					if (!candidateIsLaizi) {
						tc[candidate]++;
					}
					int pc = 0, sc = 0;
					for (const auto& kv : tc) {
						pc += kv.second / 2;
						if (kv.second % 2 == 1) sc++;
					}

					// 赖子对子：wildCount / 2
					int laiziPairs = wildCount / 2;
					int laiziSingles = wildCount % 2;

					// 每个赖子单可以配一个普通单牌成对子
					int pairedWithWild = std::min(laiziSingles, sc);
					int wildRemain = laiziSingles - pairedWithWild; // >= 0
					int normalRemain = sc - pairedWithWild;         // >= 0

					int totalPairs = pc + laiziPairs + pairedWithWild + wildRemain / 2;

					if (normalRemain == 0 && totalPairs >= 7 && tingSet.find(candidate) == tingSet.end()) {
						tingSet.insert(candidate);
						MahjongGenre::TingPai tp;
						tp.tile = candidate;
						tp.style = static_cast<int>(MahjongGenre::HuStyle::QiXiaoDui);
						tps.push_back(tp);
					}
				}
			}
		}
	};

	TaoJiangMahjongRoom::TaoJiangMahjongRoom(const std::string& venueId, const std::string& number, int level, const std::string& ruleConfig, int districtId)
		: MahjongRoom(std::make_shared<TaoJiangMahjongRule>(), venueId, static_cast<int>(GameType::TaoJiangMahjong), 2)
		, _number(number)
		, _level(level)
		, _districtId(districtId)
		, _roundState(StageState::NotStarted)
		, _disbandState(StageState::NotStarted)
		, _roundNo(0)
		, _backupBanker(0)
		, _disbander(0)
		, _disbandTick(0)
		, _diZhu(1)
		, _maxScore(0)
		, _roomFeeType(0)
		, _roundCount(8)
		, _allowChi(true)
		, _allowPeng(true)
		, _allowGang(true)
		, _allowZiMo(true)
		, _allowDianPao(true)
		, _laiziEnabled(true)
		, _hongzhongEnabled(false)
		, _bankerRule(0)
		, _dissolveVote(true)
		, _laiZiOriginal(MahjongTile::Tile(MahjongTile::Pattern::Invalid, MahjongTile::Number::Invalid))
		, _laiZi(MahjongTile::Tile(MahjongTile::Pattern::Invalid, MahjongTile::Number::Invalid))
		, _dicePoint(0)
		, _baoTingEnabled(true)
	{
		_anGangVisible = true;
		_chi = _allowChi;
		_dianPao = _allowDianPao;

		for (int i = 0; i < 6; i++)
			_distances[i] = -1;
		for (int i = 0; i < 4; i++) {
			_disbandChoices[i] = 0;
			_kicks[i] = false;
			_baoTinged[i] = false;
		}

		// 解析玩法配置
		if (!ruleConfig.empty())
			parseRuleConfig(ruleConfig);

		// 押金数额为底注的50倍
		setCashPledge(_diZhu * 50);
	}

	TaoJiangMahjongRoom::~TaoJiangMahjongRoom()
	{}

	void TaoJiangMahjongRoom::parseRuleConfig(const std::string& ruleConfig) {
		Json::Reader reader;
		Json::Value root;
		if (!reader.parse(ruleConfig, root))
			return;

		if (root.isMember("base_score") && root["base_score"].isInt())
			_diZhu = root["base_score"].asInt();
		if (root.isMember("max_score") && root["max_score"].isInt())
			_maxScore = root["max_score"].asInt();
		if (root.isMember("room_fee_type") && root["room_fee_type"].isInt())
			_roomFeeType = root["room_fee_type"].asInt();
		if (root.isMember("round_count") && root["round_count"].isInt())
			_roundCount = root["round_count"].asInt();
		if (root.isMember("allow_chi") && root["allow_chi"].isBool())
			_allowChi = root["allow_chi"].asBool();
		if (root.isMember("allow_peng") && root["allow_peng"].isBool())
			_allowPeng = root["allow_peng"].asBool();
		if (root.isMember("allow_gang") && root["allow_gang"].isBool())
			_allowGang = root["allow_gang"].asBool();
		if (root.isMember("allow_zimo") && root["allow_zimo"].isBool())
			_allowZiMo = root["allow_zimo"].asBool();
		if (root.isMember("allow_dianpao") && root["allow_dianpao"].isBool())
			_allowDianPao = root["allow_dianpao"].asBool();
		if (root.isMember("laizi_enabled") && root["laizi_enabled"].isBool())
			_laiziEnabled = root["laizi_enabled"].asBool();
		if (root.isMember("hongzhong_enabled") && root["hongzhong_enabled"].isBool())
			_hongzhongEnabled = root["hongzhong_enabled"].asBool();
		if (root.isMember("banker_rule") && root["banker_rule"].isInt())
			_bankerRule = root["banker_rule"].asInt();
		if (root.isMember("dissolve_vote") && root["dissolve_vote"].isBool())
			_dissolveVote = root["dissolve_vote"].asBool();
		if (root.isMember("bao_ting_enabled") && root["bao_ting_enabled"].isBool())
			_baoTingEnabled = root["bao_ting_enabled"].asBool();

		// 同步到MahjongRoom基类字段
		_chi = _allowChi;
		_dianPao = _allowDianPao;
	}

	void TaoJiangMahjongRoom::determineLaiZi() {
		if (!_laiziEnabled) {
			_laiZiOriginal.setInvalid();
			_laiZi.setInvalid();
			_dicePoint = 0;
			return;
		}

		// 掷骰子确定赖子
		_dicePoint = BaseUtils::randInt(1, 7);

		// 随机选择明子：筒/条/万(3种花色) × 1~9(9个点数) = 27种
		// 桃江麻将只有数牌，没有字牌
		int patternIdx = BaseUtils::randInt(0, 3); // 0=筒, 1=条, 2=万
		int numberIdx = BaseUtils::randInt(1, 10);   // 1~9
		MahjongTile::Pattern pattern = static_cast<MahjongTile::Pattern>(
			static_cast<int>(MahjongTile::Pattern::Tong) + patternIdx);
		MahjongTile::Number number = static_cast<MahjongTile::Number>(numberIdx);

		_laiZiOriginal = MahjongTile::Tile(pattern, number);

		// 赖子 = 明子点数+1的同花色牌
		// 如果明子是9，则赖子为同花色的1（循环）
		int newNum = numberIdx + 1;
		if (newNum > 9)
			newNum = 1;

		_laiZi = MahjongTile::Tile(pattern, static_cast<MahjongTile::Number>(newNum));

		std::string laiZiStr, originalStr;
		_laiZi.toString(laiZiStr);
		_laiZiOriginal.toString(originalStr);
		InfoS << "桃江麻将赖子确定: 骰子=" << _dicePoint << ", 翻牌=" << originalStr << ", 赖子=" << laiZiStr;
	}

	bool TaoJiangMahjongRoom::isLaiZi(const MahjongTile::Tile& tile) const {
		if (!_laiziEnabled)
			return false;
		return tile == _laiZi;
	}

	bool TaoJiangMahjongRoom::isLaiZiOriginal(const MahjongTile::Tile& tile) const {
		if (!_laiziEnabled)
			return false;
		return tile == _laiZiOriginal;
	}

	bool TaoJiangMahjongRoom::fetchTile(bool bBack) {
	// 调用基类摸牌逻辑
	bool ret = MahjongRoom::fetchTile(bBack);
	if (!ret)
		return false;

	// 摸牌后重新计算并通知听牌信息（修复：自摸时需要更新听牌提示）
	MahjongAvatar* pAvatar = dynamic_cast<MahjongAvatar*>(getAvatar(_actor).get());
	if (pAvatar != nullptr) {
		_rule->checkTingPai(pAvatar->getTiles(), pAvatar->getGangTiles(), pAvatar->getTingTiles(), pAvatar);
		notifyTingTile(pAvatar);
	}

	return true;
}

bool TaoJiangMahjongRoom::shouldAllowDianPaoForAvatar(MahjongAvatar* pAvatar, const MahjongTile& mt) const {
	// 桃江麻将规则：平胡不能抓炮（只能自摸），只有大胡才能点炮
	TaoJiangMahjongAvatar* tjAvatar = dynamic_cast<TaoJiangMahjongAvatar*>(pAvatar);
	if (tjAvatar == nullptr)
		return true;

	// 临时检测胡牌型来判断是否为大胡
	// detectHuStyle 会设置 _huStyle 标记位
	tjAvatar->detectHuStyle(false, mt);

	// 判断是否有实质性大胡牌型（不含报听、硬庄等非牌型因素）
	// 大胡牌型包括：七小对(含豪七对)、碰碰胡、将将胡、黑天胡、清一色、天胡/天天胡、杠上花
	const int style = tjAvatar->getHuStyle();
	const int way = tjAvatar->getHuWay();

	bool hasRealDaHu = false;

	// 牌型类大胡
	if ((style & static_cast<int>(MahjongGenre::HuStyle::QiXiaoDui)) != 0) hasRealDaHu = true;   // 七小对/豪七对
	if ((style & static_cast<int>(MahjongGenre::HuStyle::PengPengHu)) != 0) hasRealDaHu = true;   // 碰碰胡
	if ((style & static_cast<int>(MahjongGenre::HuStyle::JiangJiangHu)) != 0) hasRealDaHu = true;  // 将将胡
	if ((style & static_cast<int>(MahjongGenre::HuStyle::HeiTianHu)) != 0) hasRealDaHu = true;     // 黑天胡
	if ((style & static_cast<int>(MahjongGenre::HuStyle::QingYiSe)) != 0) hasRealDaHu = true;      // 清一色

	// 胡牌方式类大胡
	if ((way & static_cast<int>(MahjongGenre::HuWay::TianTianHu)) != 0) hasRealDaHu = true;       // 天天胡
	else if ((way & static_cast<int>(MahjongGenre::HuWay::TianHu)) != 0) hasRealDaHu = true;       // 天胡
	if ((way & static_cast<int>(MahjongGenre::HuWay::GangShangHua1)) != 0) hasRealDaHu = true;     // 杠上花

	// 注意：报听(_baoTinged) 不算大胡牌型，仅增加番数
	// 报听的平胡仍然是平胡 → 不能抓炮

	if (!hasRealDaHu) {
		std::string tileStr;
		mt.getTile().toString(tileStr);
		InfoS << "桃江麻将: 平胡不允许抓炮, 玩家=" << pAvatar->getPlayerId() << ", 牌=" << tileStr;
		return false;
	}

	return true;
}

void TaoJiangMahjongRoom::dealTiles() {
		// 洗牌和确定赖子已在 startRound() 中完成

		// 发牌，每人13张
		MahjongAvatar* pAvatar = nullptr;
		MahjongTile mt;
		for (int i = 0; i < getMaxPlayerNums(); i++) {
			pAvatar = dynamic_cast<MahjongAvatar*>(getAvatar(i).get());
			if (pAvatar == nullptr)
				continue;
			MahjongTileArray& lstTiles = pAvatar->getTiles();
			lstTiles.clear();
			for (unsigned int j = 0; j < 13; j++) {
				_dealer.fetchTile(mt);
				lstTiles.push_back(mt);
			}
			pAvatar->sortTiles();
			pAvatar->backupDealedTiles();
			_rule->checkTingPai(pAvatar->getTiles(), pAvatar->getGangTiles(), pAvatar->getTingTiles(), pAvatar);
		}

		notifyDealTiles();
		for (int i = 0; i < getMaxPlayerNums(); i++) {
			pAvatar = dynamic_cast<MahjongAvatar*>(getAvatar(i).get());
			if (pAvatar == nullptr)
				continue;
			notifyTingTile(pAvatar);
		}

		// 每人发13张牌，发完之后庄家摸第一张牌
		updateCurrentActor(_banker);
		fetchTile();
	}

	void TaoJiangMahjongRoom::doHu() {
		// 调用基类doHu完成基本胡牌处理
		MahjongRoom::doHu();

		// 桃江麻将扩展：检测天天胡
		// 天天胡 = 天胡 + 赖子本身牌做胡牌
		for (int i = 0; i < getMaxPlayerNums(); i++) {
			TaoJiangMahjongAvatar* avatar = dynamic_cast<TaoJiangMahjongAvatar*>(getAvatar(i).get());
			if (avatar == nullptr || !avatar->isHu())
				continue;

			// 检测天胡
			int huWay = avatar->getHuWay();
			bool isTianHu = (huWay & static_cast<int>(MahjongGenre::HuWay::TianHu)) == static_cast<int>(MahjongGenre::HuWay::TianHu);
			if (isTianHu && _laiziEnabled) {
				// 检查是否为天天胡：胡的牌是明子
				MahjongTile huTile;
				huTile.setId(_huTileId);
				MahjongDealer::getTileById(huTile);
				if (isLaiZiOriginal(huTile.getTile())) {
					avatar->addHuWay(MahjongGenre::HuWay::TianTianHu);
				}
			}

			// 检测地胡（桃江麻将：拥有三张明子，摸完牌到轮次即可胡）
			// 基类已处理DiHu检测，此处无需额外操作

			// 检测报听标记
			if (_baoTinged[i])
				avatar->addHuWay(MahjongGenre::HuWay::BaoTing);

			// 检测硬庄（赖子未做万能牌使用）
			if (_laiziEnabled) {
				unsigned int jiangJiangHuFlag = static_cast<unsigned int>(MahjongGenre::HuStyle::JiangJiangHu);
				bool isJiangJiangHu = (avatar->getHuStyle() & jiangJiangHuFlag) == jiangJiangHuFlag;

				if (isJiangJiangHu) {
					// 将将胡牌型：赖子本身是2/5/8就算硬庄（将将胡全是2/5/8，赖子当本身用）
					bool hasLaiZiNon258 = false;
					MahjongTileArray allTiles = avatar->getTiles();
					MahjongChapterArray chapters = avatar->getChapters();
					for (const auto& ch : chapters) {
						const MahjongTileArray& lstTiles = ch.getAllTiles();
						allTiles.insert(allTiles.end(), lstTiles.begin(), lstTiles.end());
					}
					for (const auto& t : allTiles) {
						if (isLaiZi(t.getTile())) {
							MahjongTile::Number laiziNum = _laiZi.getNumber();
							if (laiziNum != MahjongTile::Number::Er &&
								laiziNum != MahjongTile::Number::Wu &&
								laiziNum != MahjongTile::Number::Ba) {
								hasLaiZiNon258 = true;
								break;
							}
						}
					}
					avatar->setYingZhuang(!hasLaiZiNon258);
				} else {
					// 其他牌型：手牌+碰杠中没有赖子牌才算硬庄（赖子全部按本身牌使用）
					bool hasLaiZi = false;
					MahjongTileArray allTiles = avatar->getTiles();
					MahjongChapterArray chapters = avatar->getChapters();
					for (const auto& ch : chapters) {
						const MahjongTileArray& lstTiles = ch.getAllTiles();
						allTiles.insert(allTiles.end(), lstTiles.begin(), lstTiles.end());
					}
					for (const auto& t : allTiles) {
						if (isLaiZi(t.getTile())) {
							hasLaiZi = true;
							break;
						}
					}
					avatar->setYingZhuang(!hasLaiZi);
				}
			}
			else {
				// 没有赖子系统时，所有胡牌都是硬庄
				avatar->setYingZhuang(true);
			}

			// 检测黑天胡：仅限第一轮摸牌
			// 条件：14张牌中没有刻子（三张）、没有顺子、没有2/5/8将牌、没有赖子做万能牌
			// 第一轮判定：没有打过牌且没有吃碰杠
			if (avatar->getPlayedTileNums() == 0 && noChiPengGang()) {
				const MahjongTileArray& handTiles = avatar->getTiles();
				const MahjongChapterArray& chapters = avatar->getChapters();
				bool hasHeiTianHu = true;

				// 必须没有牌章（无吃碰杠，手牌即为全部14张）
				if (!chapters.empty()) {
					hasHeiTianHu = false;
				}

				if (hasHeiTianHu) {
					// 检查1：没有赖子做万能牌（即硬庄）
					if (_laiziEnabled && !avatar->isYingZhuang()) {
						hasHeiTianHu = false;
					}

					// 检查2：没有刻子（没有牌出现3次及以上）
					if (hasHeiTianHu) {
						std::map<std::string, int> tileCount;
						for (MahjongTileArray::const_iterator it = handTiles.begin(); it != handTiles.end(); ++it) {
							std::string key;
							it->getTile().toString(key);
							tileCount[key]++;
						}
						for (std::map<std::string, int>::const_iterator it = tileCount.begin(); it != tileCount.end(); ++it) {
							if (it->second >= 3) {
								hasHeiTianHu = false;
								break;
							}
						}
					}

					// 检查3：没有顺子（没有三张同花色连续数字的牌）
					if (hasHeiTianHu) {
						// 按花色分组统计数字
						std::map<int, std::vector<int>> patternNums;
						for (MahjongTileArray::const_iterator it = handTiles.begin(); it != handTiles.end(); ++it) {
							int pat = static_cast<int>(it->getPattern());
							int num = static_cast<int>(it->getNumber());
							patternNums[pat].push_back(num);
						}
						for (std::map<int, std::vector<int>>::const_iterator it = patternNums.begin(); it != patternNums.end(); ++it) {
							const std::vector<int>& nums = it->second;
							// 标记存在的数字
							bool exists[10] = {false};
							for (size_t k = 0; k < nums.size(); k++) {
								if (nums[k] >= 1 && nums[k] <= 9)
									exists[nums[k]] = true;
							}
							// 检查是否有连续三个数字
							for (int n = 1; n <= 7; n++) {
								if (exists[n] && exists[n + 1] && exists[n + 2]) {
									hasHeiTianHu = false;
									break;
								}
							}
							if (!hasHeiTianHu) break;
						}
					}

					// 检查4：将牌不是2/5/8
					if (hasHeiTianHu) {
						// 找到将牌（出现2次的牌中选一张作为将牌）
						// 简化：检查所有出现2次的牌，如果存在一张不是2/5/8的，则可以用作将牌
						// 如果所有对子都是2/5/8，则不满足黑天胡条件
						std::map<std::string, int> tileCount2;
						for (MahjongTileArray::const_iterator it = handTiles.begin(); it != handTiles.end(); ++it) {
							std::string key;
							it->getTile().toString(key);
							tileCount2[key]++;
						}
						bool hasNon258Pair = false;
						for (std::map<std::string, int>::const_iterator it = tileCount2.begin(); it != tileCount2.end(); ++it) {
							if (it->second == 2) {
								// 检查这张牌的数字是否是2/5/8
								// 遍历手牌找到对应牌获取数字
								for (MahjongTileArray::const_iterator ht = handTiles.begin(); ht != handTiles.end(); ++ht) {
									std::string tKey;
									ht->getTile().toString(tKey);
									if (tKey == it->first) {
										MahjongTile::Number tileNum = ht->getNumber();
										if (tileNum != MahjongTile::Number::Er &&
											tileNum != MahjongTile::Number::Wu &&
											tileNum != MahjongTile::Number::Ba) {
											hasNon258Pair = true;
										}
										break;
									}
								}
							}
						}
						if (!hasNon258Pair) {
							hasHeiTianHu = false;
						}
					}
				}

				if (hasHeiTianHu) {
					avatar->addHuStyle(MahjongGenre::HuStyle::HeiTianHu);
					InfoS << "黑天胡检测成功, 座位=" << i;
				}
			}
		}

		// 重新计算胡牌分数
		calcHuScore();
	}

	void TaoJiangMahjongRoom::onBaoTing(const NetMessage::Ptr& netMsg) {
		if (!_baoTingEnabled)
			return;
		if (_roundState != StageState::Underway)
			return;

		// 庄家不能报听
		// 这里暂时通过消息处理，后续可以添加更精确的消息定义
		// 报听条件：没开始摸牌前已听牌
		// 当前简化实现：玩家发送报听请求，检查是否已听牌
	}

	GameAvatar::Ptr TaoJiangMahjongRoom::createAvatar(const std::string& playerId, int seat, bool robot) const {
		return std::make_shared<TaoJiangMahjongAvatar>(playerId, seat, robot);
	}

	void TaoJiangMahjongRoom::onAvatarJoined(int seat, const std::string& playerId) {
		MahjongRoom::onAvatarJoined(seat, playerId);
		// 好友房不参与区域匹配，无需维护 Redis 未满列表
		if (_districtId == 0)
			return;
		// 更新区域内场地的玩家数量
		int districtId = getDistrictId();
		std::string redisKey = RedisKeys::DISTRICT_NOT_FULL_VENUES + std::to_string(districtId);
		if (isFull())
			RedisPool::getSingleton().hdel(redisKey, getId());
		else {
			int count = getAvatarCount();
			RedisPool::getSingleton().hset(redisKey, getId(), count);
		}
	}

	void TaoJiangMahjongRoom::onAvatarLeaved(int seat, const std::string& playerId) {
		clearDistances(seat, _distances);
		int count = getAvatarCount();
		if (count == 0)
			gameOver();
		// 好友房不参与区域匹配，无需维护 Redis 未满列表
		if (_districtId != 0) {
			// 更新区域内场地的玩家数量
			int districtId = getDistrictId();
			std::string redisKey = RedisKeys::DISTRICT_NOT_FULL_VENUES + std::to_string(districtId);
			RedisPool::getSingleton().hset(redisKey, getId(), count);

			// 记录玩家的进入场地轨迹（用于区域匹配时避免重复进入刚离开的场地）
			redisKey = RedisKeys::DISTRICT_PLAYER_TRACK;
			std::string::size_type pos = redisKey.find("{0}");
			redisKey.replace(pos, 3, std::to_string(districtId));
			pos = redisKey.find("{1}");
			redisKey.replace(pos, 3, playerId);
			RedisPool::getSingleton().hset(redisKey, getId(), BaseUtils::getCurrentMillisecond());
		}
	}

	int TaoJiangMahjongRoom::getDistrictId() const {
		return _districtId;
	}

	bool TaoJiangMahjongRoom::checkEnter(const std::string& playerId, std::string& errMsg, bool robot) const {
		if (_roundState == StageState::Underway) {
			errMsg = "游戏正在进行中，不能进入房间";
			return false;
		}
		return true;
	}

	int TaoJiangMahjongRoom::checkLeave(const std::string& playerId, std::string& errMsg) const {
		if (_roundState == StageState::Underway) {
			errMsg = "游戏正在进行中，不能离开房间";
			return 1;
		}
		return 0;
	}

	void TaoJiangMahjongRoom::getAvatarExtraInfo(const GameAvatar::Ptr& avatar, std::string& base64) const {
		std::shared_ptr<GetCapitalTask> task = std::make_shared<GetCapitalTask>(avatar->getPlayerId());
		MysqlPool::getSingleton().syncQuery(task);
		int64_t gold = avatar->getCashPledge();
		int64_t diamond = 0LL;
		if (task->getSucceed() && task->getRows() > 0) {
			gold += task->getGold();
			diamond = task->getDiamond();
		}
		Json::Value tmp(Json::objectValue);
		tmp["gold"] = static_cast<Json::Int64>(gold);
		tmp["diamond"] = static_cast<Json::Int64>(diamond);
		if (!avatar->isOffline()) {
			Session::Ptr session = avatar->getSession();
			if (session)
				tmp["ip"] = session->getRemoteIp();
		}
		std::string json = tmp.toStyledString();
		BaseUtils::encodeBase64(base64, json.data(), static_cast<int>(json.size()));
	}

	void TaoJiangMahjongRoom::clean() {
		MahjongRoom::clean();

		for (int i = 0; i < 4; i++) {
			_kicks[i] = false;
			_baoTinged[i] = false;
		}
	}

	bool TaoJiangMahjongRoom::onMessage(const NetMessage::Ptr& netMsg) {
		if (MahjongRoom::onMessage(netMsg))
			return true;

		bool ret = true;
		const std::string& msgType = netMsg->getType();
		if (msgType == MsgTJSync::TYPE)
			onSyncMahjong(netMsg);
		else if (msgType == MsgPlayerReady::TYPE)
			onPlayerReady(netMsg);
		else if (msgType == MsgDisbandRequest::TYPE)
			onDisbandRequest(netMsg);
		else if (msgType == MsgDisbandChoose::TYPE)
			onDisbandChoose(netMsg);
		else
			ret = false;

		return ret;
	}

	void TaoJiangMahjongRoom::onTimer() {
		if (_disbandState == StageState::Underway) {
			time_t nowTick = BaseUtils::getCurrentMillisecond();
			int deltaTicks = static_cast<int>(nowTick - _disbandTick);
			if (deltaTicks > 300000) {
				// 超过300秒(5分钟)不选择默认同意解散
				for (int i = 0; i < getMaxPlayerNums(); i++) {
					if (_disbandChoices[i] == 0)
						doDisbandChoose(i, 1);
				}
			}
		}
	}

	void TaoJiangMahjongRoom::onSyncMahjong(const NetMessage::Ptr& netMsg) {
		MsgTJSync* inst = dynamic_cast<MsgTJSync*>(netMsg->getMessage().get());
		if (inst == nullptr)
			return;
		TaoJiangMahjongAvatar* avatar = dynamic_cast<TaoJiangMahjongAvatar*>(getAvatar(inst->getPlayerId()).get());
		if (avatar == nullptr)
			return;
		std::shared_ptr<GetCapitalTask> task = std::make_shared<GetCapitalTask>(inst->getPlayerId());
		MysqlPool::getSingleton().syncQuery(task);
		if (!task->getSucceed() || (task->getRows() < 1)) {
			ErrorS << "查询玩家资产失败，场地Id: " << getId() << ", 玩家Id: " << inst->getPlayerId();
			return;
		}
		MsgTJSyncResp msg;
		msg.number = _number;
		msg.gold = task->getGold();
		msg.diamond = task->getDiamond();
		msg.diZhu = _diZhu;
		msg.chi = _allowChi;
		msg.dianPao = _allowDianPao;
		msg.seat = avatar->getSeat();
		msg.roundState = static_cast<int>(_roundState);
		msg.disbandState = static_cast<int>(_disbandState);
		msg.banker = _banker;
		msg.roundNo = _roundNo;
		msg.roundCount = _roundCount;
		msg.leftTiles = _dealer.getTileLeft();
		// 赖子系统
		msg.laiziEnabled = _laiziEnabled;
		if (_laiziEnabled) {
			msg.wangPai.setTile(_laiZi);
			msg.mingZi.setTile(_laiZiOriginal);
			msg.dicePoint = _dicePoint;
		}
		// 报听信息
		msg.baoTingEnabled = _baoTingEnabled;
		for (int i = 0; i < getMaxPlayerNums(); i++)
			msg.baoTinged[i] = _baoTinged[i];

		TaoJiangMahjongAvatar* tmpAvatar = NULL;
		if (_roundState == StageState::Underway) {
			for (int i = 0; i < getMaxPlayerNums(); i++) {
				tmpAvatar = dynamic_cast<TaoJiangMahjongAvatar*>(getAvatar(i).get());
				if (tmpAvatar == NULL)
					continue;
				if (i == avatar->getSeat()) {
					msg.handTiles = tmpAvatar->getTiles();
					if ((i == _actor) && !_actions.empty()) {
						const MahjongAction& ma = _actions.back();
						if (ma.getType() == MahjongAction::Type::Fetch) {
							int tileId = tmpAvatar->getFetchedTileId();
							msg.hasFetch = true;
							msg.fetchTile.setId(tileId);
							_dealer.getTileById(msg.fetchTile);
						}
					}
				}
				msg.handTileNums[i] = tmpAvatar->getTileNums();
				tmpAvatar->getPlayedTilesNoAction(msg.playedTiles[i]);
				msg.chapters[i] = tmpAvatar->getChapters();
			}
		}
		msg.send(netMsg->getSession());
		sendAvatars(netMsg->getSession());
		if (_roundState == StageState::Underway) {
			notifyActorUpdated(inst->getPlayerId());
			if ((_state == StateMachine::Action) || (_state == StateMachine::Play))
				notifyWaitingAction(inst->getPlayerId());
			if (avatar->hasActionOption())
				notifyActionOptions(avatar);
			notifyTingTile(avatar);
			if (_disbandState == StageState::Underway)
				notifyDisbandVote(inst->getPlayerId());
		}
	}

	void TaoJiangMahjongRoom::onPlayerReady(const NetMessage::Ptr& netMsg) {
		if (_roundState != StageState::NotStarted)
			return;
		MsgPlayerReady* inst = dynamic_cast<MsgPlayerReady*>(netMsg->getMessage().get());
		if (inst == nullptr)
			return;
		TaoJiangMahjongAvatar* avatar = dynamic_cast<TaoJiangMahjongAvatar*>(getAvatar(inst->getPlayerId()).get());
		if (avatar == nullptr)
			return;
		avatar->setReady(true);
		MsgPlayerReadyResp msg;
		msg.playerId = avatar->getPlayerId();
		msg.seat = avatar->getSeat();
		sendMessageToAll(msg);
		if (isFull() && isAllReady()) {
			// 所有人都已经准备好，开始一局
			startRound();
		}
	}

	void TaoJiangMahjongRoom::startRound() {
		// 开始一局
		_roundState = StageState::Underway;
		_backupBanker = _banker;

		clean();

		if (_roundNo == 0) {
			class GetMaxRoundNoTask : public MysqlQueryTask {
			public:
				GetMaxRoundNoTask(const std::string& venueId)
					: _venueId(venueId)
					, _maxRoundNo(0)
				{}

				virtual ~GetMaxRoundNoTask() {}

			public:
				virtual QueryType buildQuery(std::string& sql) override {
					std::stringstream ss;
					ss << "select max(`round_no`) from `game_taojiang_mahjong_record` where `venue_id` = \"" << _venueId << "\"";
					sql = ss.str();
					return QueryType::Select;
				}

				virtual int fetchResult(sql::ResultSet* res) override {
					int rows = 0;
					while (res->next()) {
						_maxRoundNo = res->getInt(1);
						rows++;
					}
					return rows;
				}

			public:
				const std::string _venueId;
				int _maxRoundNo;
			};
			std::shared_ptr<GetMaxRoundNoTask> task = std::make_shared<GetMaxRoundNoTask>(getId());
			MysqlPool::getSingleton().syncQuery(task);
			if (task->getSucceed() && task->getRows() > 0)
				_roundNo = task->_maxRoundNo;
		}
		_roundNo++;

		// 先洗牌并确定赖子（必须在发送 MsgTJStartRound 之前完成）
		_dealer.shuffle();
		determineLaiZi();

		// 将赖子信息设置到每个avatar，供听牌检测使用
		if (_laiziEnabled) {
			for (int i = 0; i < getMaxPlayerNums(); i++) {
				TaoJiangMahjongAvatar* av = dynamic_cast<TaoJiangMahjongAvatar*>(getAvatar(i).get());
				if (av != nullptr)
					av->setLaiZi(_laiZi);
			}
		}

		MsgTJStartRound msg;
		msg.banker = _banker;
		msg.roundNo = _roundNo;
		msg.roundCount = _roundCount;
		// 赖子系统
		msg.laiziEnabled = _laiziEnabled;
		if (_laiziEnabled) {
			msg.wangPai.setTile(_laiZi);
			msg.mingZi.setTile(_laiZiOriginal);
			msg.dicePoint = _dicePoint;
		}
		std::ostringstream os;
		os << "桃江麻将牌桌(Id:" << getId() << ")开局，各玩家Id: ";
		TaoJiangMahjongAvatar* avatar = nullptr;
		for (int i = 0; i < getMaxPlayerNums(); i++) {
			avatar = dynamic_cast<TaoJiangMahjongAvatar*>(getAvatar(i).get());
			if (avatar == nullptr)
				continue;
			if (i > 0)
				os << "、";
			os << avatar->getPlayerId();
			msg.send(avatar->getSession());
		}
		LOG_INFO(os.str());

		dealTiles();
	}

	double* TaoJiangMahjongRoom::getDistances() {
		return _distances;
	}

	void TaoJiangMahjongRoom::getDistances(std::vector<int>& distances) const {
		// 两人模式只有一对距离
		distances.push_back(static_cast<int>(_distances[0]));
	}

	int TaoJiangMahjongRoom::getDistanceIndex(int seat1, int seat2) const {
		// 两人模式只有一对: 座位0-1
		if ((seat1 == 0 && seat2 == 1) || (seat1 == 1 && seat2 == 0))
			return 0;
		return -1;
	}

	void TaoJiangMahjongRoom::calcHuScore() const {
		// 流局不算分
		if (!_hu)
			return;
		int seat = 0;
		int score = 0;
		int scores[4] = { 0, 0, 0, 0 };
		TaoJiangMahjongAvatar* avatar1 = nullptr;
		TaoJiangMahjongAvatar* avatar2 = nullptr;
		MahjongChapter::Type chapterType = MahjongChapter::Type::Invalid;
		MahjongChapterArray::const_iterator it;
		for (int i = 0; i < getMaxPlayerNums(); i++) {
			avatar1 = dynamic_cast<TaoJiangMahjongAvatar*>(getAvatar(i).get());
			if (avatar1 == nullptr)
				continue;
			// 算胡分
			if (avatar1->isHu()) {
				int daHuCount = avatar1->getDaHuCount();
				bool isZiMo = avatar1->isZiMo();

				// 计算倍率：自摸每大胡×3，放炮每大胡×2
				int multiplier = 1;
				for (int k = 0; k < daHuCount; k++) {
					multiplier *= (isZiMo ? 3 : 2);
				}

				// 硬庄额外×2，纳入封顶计算
				if (avatar1->isYingZhuang())
					multiplier *= 2;

				// 18倍封顶
				if (multiplier > 18)
					multiplier = 18;

				// 最终得分 = 8底分 × 倍率
				score = 8 * multiplier;

				if (avatar1->isDianPao()) {
					// 点炮：放炮者一人承担（门子x1）
					avatar2 = dynamic_cast<TaoJiangMahjongAvatar*>(getAvatar(_actor).get());
					if (avatar2 != nullptr) {
						avatar1->addLoseScore(_actor, -score);
						avatar2->addLoseScore(i, score);
					}
				}
				else {
					// 自摸：其余玩家各承担一份（门子x3，但2人游戏只有1个对手）
					for (int j = 0; j < getMaxPlayerNums(); j++) {
						if (i == j)
							continue;
						avatar2 = dynamic_cast<TaoJiangMahjongAvatar*>(getAvatar(j).get());
						if (avatar2 == nullptr)
							continue;
						// 自摸计法：每个其他玩家各赔一份
						avatar1->addLoseScore(j, -score);
						avatar2->addLoseScore(i, score);
					}
				}
			}
			// 算杠分
			const MahjongChapterArray& lstChapters = avatar1->getChapters();
			it = lstChapters.begin();
			while (it != lstChapters.end()) {
				if (it->isVetoed()) {
					++it;
					continue;
				}
				chapterType = it->getType();
				if (chapterType == MahjongChapter::Type::ZhiGang) {
					seat = it->getTargetPlayer();
					avatar2 = dynamic_cast<TaoJiangMahjongAvatar*>(getAvatar(seat).get());
					if (avatar2 != nullptr) {
						avatar1->addLoseScore(seat, -1);
						avatar2->addLoseScore(i, 1);
					}
				}
				else if (chapterType == MahjongChapter::Type::JiaGang) {
					for (int j = 0; j < getMaxPlayerNums(); j++) {
						if (i == j)
							continue;
						avatar2 = dynamic_cast<TaoJiangMahjongAvatar*>(getAvatar(j).get());
						if (avatar2 == nullptr)
							continue;
						avatar1->addLoseScore(j, -1);
						avatar2->addLoseScore(i, 1);
					}
				}
				else if (chapterType == MahjongChapter::Type::AnGang) {
					for (int j = 0; j < getMaxPlayerNums(); j++) {
						if (i == j)
							continue;
						avatar2 = dynamic_cast<TaoJiangMahjongAvatar*>(getAvatar(j).get());
						if (avatar2 == nullptr)
							continue;
						avatar1->addLoseScore(j, -2);
						avatar2->addLoseScore(i, 2);
					}
				}
				++it;
			}
		}
		int loseScores[4] = { 0 };
		for (int i = 0; i < getMaxPlayerNums(); i++) {
			avatar1 = dynamic_cast<TaoJiangMahjongAvatar*>(getAvatar(i).get());
			if (avatar1 == NULL)
				continue;
			avatar1->getLoseScores(loseScores);
			for (int j = 0; j < 4; j++) {
				if (i != j)
					scores[j] += loseScores[j];
			}
		}
		for (int i = 0; i < getMaxPlayerNums(); i++) {
			avatar1 = dynamic_cast<TaoJiangMahjongAvatar*>(getAvatar(i).get());
			if (avatar1 != NULL)
				avatar1->setScore(scores[i]);
		}
		// 使用DebtLiquidation清算多边债务
		bool test = false;
		double diZhu = _diZhu;
		double capital = 0.0f;
		DebtNode* node = NULL;
		std::unordered_map<int, DebtNode*> debtNet;
		for (int i = 0; i < getMaxPlayerNums(); i++) {
			avatar1 = dynamic_cast<TaoJiangMahjongAvatar*>(getAvatar(i).get());
			if (avatar1 == NULL)
				continue;
			test = false;
			avatar1->getLoseScores(loseScores);
			for (int j = 0; j < 4; j++) {
				if ((i == j) || (loseScores[j] == 0))
					continue;
				test = true;
				break;
			}
			if (!test)
				continue;
			capital = static_cast<double>(avatar1->getCashPledge());
			node = new DebtNode(i, capital);
			debtNet.insert(std::make_pair(i, node));
		}
		std::unordered_map<int, DebtNode*>::const_iterator it1 = debtNet.begin();
		std::unordered_map<int, DebtNode*>::const_iterator it2;
		while (it1 != debtNet.end()) {
			node = it1->second;
			avatar1 = dynamic_cast<TaoJiangMahjongAvatar*>(getAvatar(it1->first).get());
			avatar1->getLoseScores(loseScores);
			for (int j = 0; j < 4; j++) {
				if (((it1->first) == j) || (loseScores[j] == 0))
					continue;
				it2 = debtNet.find(j);
				if (it2 != debtNet.end())
					node->tally((it2->second), diZhu * loseScores[j]);
			}
			++it1;
		}
		std::string logDebt;
		DebtLiquidation dl;
		dl.printDebtNet(debtNet, logDebt);
		if (!dl(debtNet)) {
			dl.releaseDebtNet(debtNet);
			ErrorS << "清算结果不正确，原始债务网：" << logDebt;
			return;
		}
		test = false;
		it1 = debtNet.begin();
		while (it1 != debtNet.end()) {
			node = it1->second;
			if (node->getCapital() < 0.0) {
				test = true;
				break;
			}
			++it1;
		}
		if (test) {
			dl.releaseDebtNet(debtNet);
			ErrorS << "清算之后存在负数结果，原始债务网：" << logDebt;
			return;
		}
		double winGold = 0.0;
		it1 = debtNet.begin();
		while (it1 != debtNet.end()) {
			avatar1 = dynamic_cast<TaoJiangMahjongAvatar*>(getAvatar(it1->first).get());
			node = it1->second;
			capital = node->getCapital();
			winGold = capital - static_cast<double>(avatar1->getCashPledge());
			avatar1->setWinGold(winGold);
			++it1;
		}
		dl.releaseDebtNet(debtNet);
	}

	void TaoJiangMahjongRoom::doJieSuan() {
		_roundState = StageState::NotStarted;

		MsgTJSettlement msg;
		getSettlementData(&(msg.data));

		// 填充硬庄信息
		for (int i = 0; i < getMaxPlayerNums(); i++) {
			TaoJiangMahjongAvatar* av = dynamic_cast<TaoJiangMahjongAvatar*>(getAvatar(i).get());
			msg.yingZhuang[i] = (av != NULL && av->isYingZhuang());
		}

		double delta = 0.0;
		int64_t tmp = 0LL;
		int64_t cashPledge = 0LL;
		int64_t goldNeed = getCashPledge();
		bool test = true;
		GameAvatar::Ptr ptr;
		TaoJiangMahjongAvatar* avatar = NULL;
		std::shared_ptr<GetCapitalTask> task;
		for (int i = 0; i < getMaxPlayerNums(); i++) {
			ptr = getAvatar(i);
			avatar = dynamic_cast<TaoJiangMahjongAvatar*>(ptr.get());
			if (avatar == NULL)
				continue;
			delta = avatar->getWinGold();
			// 四舍五入
			delta = floor(delta + 0.5);
			avatar->setWinGold(delta);
			msg.winGolds[i] = static_cast<int>(delta);
			cashPledge = avatar->getCashPledge();
			cashPledge += msg.winGolds[i];
			test = true;
			if (msg.winGolds[i] != 0) {
				if (cashPledge < goldNeed) {
					test = deductCashPledge(ptr);
				}
				else {
					updateCashPledge(avatar->getPlayerId(), cashPledge);
				}
			}
			task = std::make_shared<GetCapitalTask>(avatar->getPlayerId());
			MysqlPool::getSingleton().syncQuery(task);
			if (task->getSucceed() && task->getRows() > 0)
				msg.golds[i] = task->getGold() + avatar->getCashPledge();
			if (!test)
				_kicks[i] = true;
		}
		for (int i = 0; i < getMaxPlayerNums(); i++) {
			avatar = dynamic_cast<TaoJiangMahjongAvatar*>(getAvatar(i).get());
			if (avatar == NULL)
				continue;
			msg.kick = _kicks[i];
			msg.send(avatar->getSession());
		}
	}

	void TaoJiangMahjongRoom::afterHu() {
		saveRoundRecord();

		if (_roundCount > 0 && _roundNo >= _roundCount) {
			disbandRoom();
			return;
		}

		GameAvatar::Ptr avatar;
		for (int i = 0; i < getMaxPlayerNums(); i++) {
			avatar = getAvatar(i);
			if (!avatar)
				continue;
			if (_kicks[i])
				kickAvatar(avatar);
			else
				avatar->setReady(false);
		}
	}

	void TaoJiangMahjongRoom::onDisbandRequest(const NetMessage::Ptr& netMsg) {
		if (!_dissolveVote)
			return;
		if (_disbandState == StageState::Underway)
			return;
		MsgDisbandRequest* inst = dynamic_cast<MsgDisbandRequest*>(netMsg->getMessage().get());
		if (inst == nullptr)
			return;
		TaoJiangMahjongAvatar* avatar = dynamic_cast<TaoJiangMahjongAvatar*>(getAvatar(inst->getPlayerId()).get());
		if (avatar == NULL)
			return;
		_disbander = avatar->getSeat();
		for (int i = 0; i < getMaxPlayerNums(); i++) {
			if (i == _disbander)
				_disbandChoices[i] = 1;
			else
				_disbandChoices[i] = 0;
		}
		_disbandState = StageState::Underway;
		_disbandTick = BaseUtils::getCurrentMillisecond();
		notifyDisbandVote(std::string(""));
	}

	void TaoJiangMahjongRoom::notifyDisbandVote(const std::string& playerId) {
		MsgTJDisbandVote msg;
		msg.disbander = _disbander;
		time_t nowTick = BaseUtils::getCurrentMillisecond();
		msg.elapsed = static_cast<int>((nowTick - _disbandTick) / 1000LL);

		for (int i = 0; i < getMaxPlayerNums(); i++)
			msg.choices[i] = _disbandChoices[i];
		if (playerId.empty())
			sendMessageToAll(msg);
		else
			sendMessage(msg, playerId);
	}

	void TaoJiangMahjongRoom::onDisbandChoose(const NetMessage::Ptr& netMsg) {
		MsgDisbandChoose* inst = dynamic_cast<MsgDisbandChoose*>(netMsg->getMessage().get());
		if (inst == nullptr)
			return;
		if ((inst->choice != 1) && (inst->choice != 2))
			return;
		TaoJiangMahjongAvatar* avatar = dynamic_cast<TaoJiangMahjongAvatar*>(getAvatar(inst->getPlayerId()).get());
		if (avatar != NULL)
			doDisbandChoose(avatar->getSeat(), inst->choice);
	}

	void TaoJiangMahjongRoom::doDisbandChoose(int seat, int choice) {
		if (seat < 0 || seat >= getMaxPlayerNums())
			return;
		int half = (getMaxPlayerNums() >> 1);
		int nums1 = 0;
		int nums2 = 0;
		_disbandChoices[seat] = choice;
		for (int i = 0; i < getMaxPlayerNums(); i++) {
			if (_disbandChoices[i] == 1)
				nums1++;
			else if (_disbandChoices[i] == 2)
				nums2++;
		}
		MsgDisbandChoice msg;
		msg.seat = seat;
		msg.choice = choice;
		sendMessageToAll(msg);

		if (nums1 > half)
			disbandRoom();
		else if (nums2 >= half)
			disbandObsolete();
	}

	void TaoJiangMahjongRoom::disbandRoom() {
		_disbandState = StageState::Finished;

		MsgDisband msg;
		sendMessageToAll(msg);

		_roundState = StageState::NotStarted;

		kickAllAvatars();

		gameOver();
	}

	void TaoJiangMahjongRoom::disbandObsolete() {
		_disbandState = StageState::NotStarted;

		MsgDisbandObsolete msg;
		sendMessageToAll(msg);
	}

	void TaoJiangMahjongRoom::saveRoundRecord() {
		std::shared_ptr<TaoJiangMahjongRecordTask> task = std::make_shared<TaoJiangMahjongRecordTask>();
		task->_venueId = getId();
		task->_roundNo = _roundNo;
		task->_banker = _backupBanker;
		TaoJiangMahjongPlaybackData data;
		getSettlementData(&(data.settlement));
		getPlaybackData(data);
		TaoJiangMahjongAvatar* avatar = nullptr;
		for (int i = 0; i < getMaxPlayerNums(); i++) {
			avatar = dynamic_cast<TaoJiangMahjongAvatar*>(getAvatar(i).get());
			if (avatar == nullptr)
				continue;
			task->_playerIds[i] = avatar->getPlayerId();
			task->_scores[i] = avatar->getScore();
			task->_winGolds[i] = static_cast<int>(avatar->getWinGold());
			data.winGolds[i] = task->_winGolds[i];
		}
		// 生成随机种子hash
		task->_randomSeedHash = ReplayUtils::generateSeedHash(getId(), _roundNo, _backupBanker);
		data.randomSeedHash = task->_randomSeedHash;

		msgpack::sbuffer sbuf;
		msgpack::pack(sbuf, data);
		uLongf srcLen = static_cast<uLongf>(sbuf.size());
		std::string base64;
		if (srcLen > 0) {
			uLongf dstLen = srcLen + 100;
			unsigned char* dstBuf = new unsigned char[dstLen];
			int ret = compress(dstBuf, &dstLen, reinterpret_cast<const unsigned char*>(sbuf.data()), srcLen);
			if (ret != Z_OK)
				LOG_ERROR("桃江麻将牌局回放数据压缩失败");
			else {
				if (!BaseUtils::encodeBase64(base64, reinterpret_cast<const char*>(dstBuf), static_cast<int>(dstLen))) {
					LOG_ERROR("桃江麻将牌局回放数据打包Base64失败");
				}
			}
			delete[] dstBuf;
		}
		task->_playback = base64;
		// 异步保存到数据库
		MysqlPool::getSingleton().asyncQuery(task);
	}
}
