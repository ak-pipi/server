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
	namespace
	{
		bool is258(const MahjongTile::Tile& tile) {
			int n = static_cast<int>(tile.getNumber());
			return n == 2 || n == 5 || n == 8;
		}

		bool isNumberTile(const MahjongTile& mt) {
			int p = static_cast<int>(mt.getPattern()) - static_cast<int>(MahjongTile::Pattern::Tong);
			int n = static_cast<int>(mt.getNumber());
			return p >= 0 && p < 3 && n >= 1 && n <= 9;
		}

		bool all258NaturalTiles(const MahjongTileArray& tiles) {
			if (tiles.empty())
				return false;
			for (const MahjongTile& mt : tiles) {
				if (!isNumberTile(mt) || !is258(mt.getTile()))
					return false;
			}
			return true;
		}

		bool isNaturalChiChapter(MahjongTileArray tiles) {
			if (tiles.size() != 3)
				return false;
			std::sort(tiles.begin(), tiles.end(), [](const MahjongTile& a, const MahjongTile& b) {
				return a.getTile() < b.getTile();
			});
			for (const MahjongTile& mt : tiles) {
				if (!isNumberTile(mt))
					return false;
			}
			if (tiles[0].getPattern() != tiles[1].getPattern() ||
				tiles[1].getPattern() != tiles[2].getPattern())
				return false;

			int n0 = static_cast<int>(tiles[0].getNumber());
			int n1 = static_cast<int>(tiles[1].getNumber());
			int n2 = static_cast<int>(tiles[2].getNumber());
			return n1 == n0 + 1 && n2 == n1 + 1;
		}

		bool isNaturalSameTileChapter(const MahjongTileArray& tiles, size_t minSize) {
			if (tiles.size() < minSize)
				return false;
			MahjongTile::Tile tile = tiles[0].getTile();
			for (const MahjongTile& mt : tiles) {
				if (mt.getTile() != tile)
					return false;
			}
			return true;
		}

		bool isNaturalChapterWithoutWildcards(const MahjongChapter& chapter) {
			const MahjongTileArray& tiles = chapter.getAllTiles();
			switch (chapter.getType()) {
			case MahjongChapter::Type::Chi:
				return isNaturalChiChapter(tiles);
			case MahjongChapter::Type::Peng:
				return isNaturalSameTileChapter(tiles, 3);
			case MahjongChapter::Type::ZhiGang:
			case MahjongChapter::Type::JiaGang:
			case MahjongChapter::Type::AnGang:
				return isNaturalSameTileChapter(tiles, 3);
			default:
				return false;
			}
		}

		bool canFormNaturalMelds(int counts[3][9]) {
			for (int p = 0; p < 3; p++) {
				for (int n = 0; n < 9; n++) {
					if (counts[p][n] == 0)
						continue;
					if (counts[p][n] >= 3) {
						counts[p][n] -= 3;
						if (canFormNaturalMelds(counts)) {
							counts[p][n] += 3;
							return true;
						}
						counts[p][n] += 3;
					}
					if (n <= 6 && counts[p][n + 1] > 0 && counts[p][n + 2] > 0) {
						counts[p][n]--;
						counts[p][n + 1]--;
						counts[p][n + 2]--;
						if (canFormNaturalMelds(counts)) {
							counts[p][n]++;
							counts[p][n + 1]++;
							counts[p][n + 2]++;
							return true;
						}
						counts[p][n]++;
						counts[p][n + 1]++;
						counts[p][n + 2]++;
					}
					return false;
				}
			}
			return true;
		}

		bool canWinNatural(const MahjongTileArray& tiles, bool requireJiang258) {
			if (tiles.empty() || (tiles.size() % 3) != 2)
				return false;

			int counts[3][9] = { {0} };
			for (const MahjongTile& mt : tiles) {
				if (!isNumberTile(mt))
					return false;
				int p = static_cast<int>(mt.getPattern()) - static_cast<int>(MahjongTile::Pattern::Tong);
				int n = static_cast<int>(mt.getNumber()) - 1;
				counts[p][n]++;
			}

			for (int p = 0; p < 3; p++) {
				for (int n = 0; n < 9; n++) {
					if (counts[p][n] < 2)
						continue;
					MahjongTile::Tile jiang(
						static_cast<MahjongTile::Pattern>(static_cast<int>(MahjongTile::Pattern::Tong) + p),
						static_cast<MahjongTile::Number>(n + 1));
					if (requireJiang258 && !is258(jiang))
						continue;
					counts[p][n] -= 2;
					bool ok = canFormNaturalMelds(counts);
					counts[p][n] += 2;
					if (ok)
						return true;
				}
			}
			return false;
		}

		bool canWinKeZiNatural(const MahjongTileArray& tiles) {
			if (tiles.empty() || (tiles.size() % 3) != 2)
				return false;
			std::map<MahjongTile::Tile, int> counts;
			for (const MahjongTile& mt : tiles)
				counts[mt.getTile()]++;
			bool hasJiang = false;
			for (const auto& kv : counts) {
				int mod = kv.second % 3;
				if (mod == 0)
					continue;
				if (mod == 2 && !hasJiang) {
					hasJiang = true;
					continue;
				}
				return false;
			}
			return hasJiang;
		}

		bool canWinQiXiaoDuiNatural(const MahjongTileArray& tiles) {
			if (tiles.size() != 14)
				return false;
			std::map<MahjongTile::Tile, int> counts;
			for (const MahjongTile& mt : tiles)
				counts[mt.getTile()]++;
			int pairs = 0;
			for (const auto& kv : counts) {
				if (kv.second % 2 != 0)
					return false;
				pairs += kv.second / 2;
			}
			return pairs == 7;
		}

		bool allSameSuit(const MahjongTileArray& tiles) {
			bool found = false;
			MahjongTile::Pattern pat = MahjongTile::Pattern::Invalid;
			for (const MahjongTile& mt : tiles) {
				if (!isNumberTile(mt))
					return false;
				if (!found) {
					found = true;
					pat = mt.getPattern();
				} else if (mt.getPattern() != pat)
					return false;
			}
			return found;
		}

		bool hasHuStyleFlag(unsigned int value, MahjongGenre::HuStyle style) {
			unsigned int mask = static_cast<unsigned int>(style);
			return (value & mask) == mask;
		}

		bool allSameSuitNaturalTilesWithChapters(const MahjongTileArray& handTiles, const MahjongChapterArray& chapters) {
			bool found = false;
			MahjongTile::Pattern pat = MahjongTile::Pattern::Invalid;
			for (const MahjongTile& mt : handTiles) {
				if (!isNumberTile(mt))
					return false;
				if (!found) {
					found = true;
					pat = mt.getPattern();
				} else if (mt.getPattern() != pat)
					return false;
			}
			for (const MahjongChapter& chapter : chapters) {
				const MahjongTileArray& tiles = chapter.getAllTiles();
				for (const MahjongTile& mt : tiles) {
					if (!isNumberTile(mt))
						return false;
					if (!found) {
						found = true;
						pat = mt.getPattern();
					} else if (mt.getPattern() != pat)
						return false;
				}
			}
			return found;
		}

		bool all258NaturalTilesWithChapters(const MahjongTileArray& handTiles, const MahjongChapterArray& chapters) {
			if (handTiles.empty())
				return false;
			for (const MahjongTile& mt : handTiles) {
				if (!isNumberTile(mt) || !is258(mt.getTile()))
					return false;
			}
			for (const MahjongChapter& chapter : chapters) {
				const MahjongTileArray& tiles = chapter.getAllTiles();
				for (const MahjongTile& mt : tiles) {
					if (!isNumberTile(mt) || !is258(mt.getTile()))
						return false;
				}
			}
			return true;
		}

		bool chaptersAreNaturalKeZiOnly(const MahjongChapterArray& chapters) {
			for (const MahjongChapter& chapter : chapters) {
				if (chapter.getType() == MahjongChapter::Type::Chi)
					return false;
				if (!isNaturalSameTileChapter(chapter.getAllTiles(), 3))
					return false;
			}
			return true;
		}

		int countNaturalQuadTiles(const MahjongTileArray& tiles) {
			std::map<MahjongTile::Tile, int> counts;
			for (const MahjongTile& mt : tiles)
				counts[mt.getTile()]++;
			int quads = 0;
			for (const auto& kv : counts) {
				if (kv.second >= 4)
					quads++;
			}
			return quads;
		}

		bool naturalQiXiaoDuiMatchesHuStyle(const MahjongTileArray& handTiles,
			const MahjongChapterArray& chapters,
			unsigned int style) {
			if (!chapters.empty() || !canWinQiXiaoDuiNatural(handTiles))
				return false;
			int requiredQuads = 0;
			if (hasHuStyleFlag(style, MahjongGenre::HuStyle::QiXiaoDui3))
				requiredQuads = 3;
			else if (hasHuStyleFlag(style, MahjongGenre::HuStyle::QiXiaoDui2))
				requiredQuads = 2;
			else if (hasHuStyleFlag(style, MahjongGenre::HuStyle::QiXiaoDui1))
				requiredQuads = 1;
			return countNaturalQuadTiles(handTiles) >= requiredQuads;
		}

		bool canHuNaturallyWithoutWildcards(const MahjongTileArray& tiles) {
			if (canWinNatural(tiles, true))
				return true;
			if (canWinQiXiaoDuiNatural(tiles))
				return true;
			if (canWinKeZiNatural(tiles))
				return true;
			if (allSameSuit(tiles) && canWinNatural(tiles, false))
				return true;
			return false;
		}
	}

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
		 * 直接检查当前手牌（含所有牌）能否胡牌
		 * 用于摸牌后检查自摸：手牌已经包含摸到的牌，直接检查是否满足胡牌结构
		 */
			bool canHuWithHand(const MahjongTileArray& tiles, MahjongAvatar* pAvatar,
				const MahjongTile* forcedNaturalTile = nullptr) const {
			if (tiles.empty())
				return false;

			// 获取赖子信息
			MahjongTile::Tile laiziTile;
			laiziTile.setInvalid();
			if (pAvatar != nullptr) {
				TaoJiangMahjongAvatar* tjAvatar = dynamic_cast<TaoJiangMahjongAvatar*>(pAvatar);
				if (tjAvatar != nullptr)
					laiziTile = tjAvatar->getLaiZi();
			}

			if (canHuWithoutUsingLaiZiAsWildcard(tiles, pAvatar))
				return true;

			// 分离赖子和普通牌
			int laiziCount = 0;
			MahjongTileArray normalTiles;
			if (laiziTile.isValid()) {
				for (const auto& mt : tiles) {
					bool forcedNatural = forcedNaturalTile != nullptr && forcedNaturalTile->isValid() &&
						mt.getId() == forcedNaturalTile->getId();
					if (mt.getTile() == laiziTile && !forcedNatural)
						laiziCount++;
					else
						normalTiles.push_back(mt);
				}
			} else {
				normalTiles = tiles;
			}

			std::sort(normalTiles.begin(), normalTiles.end());

				// 检查平胡（桃江麻将规则：雀头必须是2/5/8）
				if (canWinWithWildcards(normalTiles, laiziCount, 0, true))
					return true;

				// 检查清一色：吃、碰、杠章也必须同花色；大胡不要求2/5/8将
				if (canHuQingYiSe(normalTiles, laiziCount, pAvatar, laiziTile))
					return true;

				// 检查碰碰胡：吃牌章不成立，其余碰杠章按刻子/杠处理
				if (canHuPengPeng(normalTiles, laiziCount, pAvatar))
					return true;

				// 检查七小对：必须无吃碰杠牌章
				if (canHuQiXiaoDui(normalTiles, laiziCount, pAvatar))
					return true;

				// 检查将将胡：手牌及碰杠章均为2/5/8，吃牌章不成立
				if (canHuJiangJiang(normalTiles, laiziCount, pAvatar, laiziTile))
					return true;

				return false;
			}

		bool canHuWithoutUsingLaiZiAsWildcard(const MahjongTileArray& tiles, MahjongAvatar* pAvatar) const {
			// 按赖子本身牌面尝试胡牌。硬庄场景下，赖子不应被先移出手牌。
			MahjongTileArray naturalTiles = tiles;
			std::sort(naturalTiles.begin(), naturalTiles.end(), [](const MahjongTile& a, const MahjongTile& b) {
				return a.getTile() < b.getTile();
			});
			if (canHuNaturallyWithoutWildcards(naturalTiles))
				return true;
			MahjongTile::Tile noLaiZiSkip;
			noLaiZiSkip.setInvalid();
			if (canWinWithWildcards(naturalTiles, 0, 0, true))
				return true;
			if (canHuQingYiSe(naturalTiles, 0, pAvatar, noLaiZiSkip))
				return true;
			if (canHuPengPeng(naturalTiles, 0, pAvatar))
				return true;
			if (canHuQiXiaoDui(naturalTiles, 0, pAvatar))
				return true;
			if (canHuJiangJiang(naturalTiles, 0, pAvatar, noLaiZiSkip))
				return true;
			return false;
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
					std::sort(testTiles.begin(), testTiles.end(),
						[](const MahjongTile& a, const MahjongTile& b) {
							return a.getTile() < b.getTile();
						});
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
					// 按牌面(pattern+number)排序，而不是按id排序
					std::sort(testTiles.begin(), testTiles.end(),
						[](const MahjongTile& a, const MahjongTile& b) {
							return a.getTile() < b.getTile();
						});

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

				// 检查清一色听牌：吃、碰、杠章一起参与花色判断，大胡不要求2/5/8将
				checkQingYiSeWithLaizi(normalTiles, laiziCount, laiziTile, allGotTiles, tps, tingSet, pAvatar);

				// 检查碰碰胡听牌：所有面子都是刻子（无顺子），雀头不需要258限制
				checkPengPengHuWithLaizi(normalTiles, laiziCount, laiziTile, allGotTiles, tps, tingSet, pAvatar);

			// 也检查七小对
				checkQiXiaoDuiWithLaizi(tiles, normalTiles, laiziCount, laiziTile, allGotTiles, tps, tingSet, pAvatar);

			checkJiangJiangHuWithLaizi(normalTiles, laiziCount, laiziTile, allGotTiles, tps, tingSet, pAvatar);

			// 听牌检测完成
		}

		private:
			bool isTileTaken(const MahjongTile::Tile& candidate, const MahjongTile::TileArray& allGotTiles) const {
				for (const auto& gt : allGotTiles) {
					if (gt == candidate)
						return true;
				}
				return false;
			}

			void addTingTile(const MahjongTile::Tile& candidate, MahjongGenre::HuStyle style,
				const MahjongTile::TileArray& allGotTiles, MahjongGenre::TingPaiArray& tps,
				std::set<MahjongTile::Tile>& tingSet) const {
				if (isTileTaken(candidate, allGotTiles))
					return;
				int styleValue = static_cast<int>(style);
				for (MahjongGenre::TingPai& tp : tps) {
					if (tp.tile == candidate) {
						tp.style |= styleValue;
						tingSet.insert(candidate);
						return;
					}
				}

				tingSet.insert(candidate);
				MahjongGenre::TingPai tp;
				tp.tile = candidate;
				tp.style = styleValue;
				tps.push_back(tp);
			}

			bool mergeSuit(MahjongTile::Pattern pat, MahjongTile::Pattern& suit, bool& found) const {
				if (!found) {
					found = true;
					suit = pat;
					return true;
				}
				return suit == pat;
			}

			bool allNumberTilesSameSuitWithChapters(const MahjongTileArray& normalTiles, MahjongAvatar* pAvatar,
				const MahjongTile::Tile& laiziTile) const {
				bool found = false;
				MahjongTile::Pattern suit = MahjongTile::Pattern::Invalid;
				for (const MahjongTile& mt : normalTiles) {
					if (!isNumberTile(mt))
						return false;
					if (!mergeSuit(mt.getPattern(), suit, found))
						return false;
				}
				if (pAvatar == nullptr)
					return true;

				const MahjongChapterArray& chapters = pAvatar->getChapters();
				for (const auto& ch : chapters) {
					const MahjongTileArray& chTiles = ch.getAllTiles();
					for (const MahjongTile& ct : chTiles) {
						if (laiziTile.isValid() && ct.getTile() == laiziTile)
							continue;
						if (!isNumberTile(ct))
							return false;
						if (!mergeSuit(ct.getPattern(), suit, found))
							return false;
					}
				}
				return true;
			}

			bool chaptersAllowKeZiOnly(MahjongAvatar* pAvatar) const {
				if (pAvatar == nullptr)
					return true;
				const MahjongChapterArray& chapters = pAvatar->getChapters();
				for (const auto& ch : chapters) {
					if (ch.getType() == MahjongChapter::Type::Chi)
						return false;
				}
				return true;
			}

			bool all258WithChapters(const MahjongTileArray& normalTiles, MahjongAvatar* pAvatar,
				const MahjongTile::Tile& laiziTile) const {
				for (const MahjongTile& mt : normalTiles) {
					if (laiziTile.isValid() && mt.getTile() == laiziTile)
						continue;
					if (!is258(mt.getTile()))
						return false;
				}
				if (pAvatar == nullptr)
					return true;

				const MahjongChapterArray& chapters = pAvatar->getChapters();
				for (const auto& ch : chapters) {
					if (ch.getType() == MahjongChapter::Type::Chi)
						return false;
					const MahjongTileArray& chTiles = ch.getAllTiles();
					for (const MahjongTile& ct : chTiles) {
						if (laiziTile.isValid() && ct.getTile() == laiziTile)
							continue;
						if (!is258(ct.getTile()))
							return false;
					}
				}
				return true;
			}

			bool canHuQingYiSe(MahjongTileArray normalTiles, int laiziCount, MahjongAvatar* pAvatar,
				const MahjongTile::Tile& laiziTile) const {
				if (!allNumberTilesSameSuitWithChapters(normalTiles, pAvatar, laiziTile))
					return false;
				std::sort(normalTiles.begin(), normalTiles.end(), [](const MahjongTile& a, const MahjongTile& b) {
					return a.getTile() < b.getTile();
				});
				return canWinWithWildcards(normalTiles, laiziCount, 0, false);
			}

			bool canHuPengPeng(MahjongTileArray normalTiles, int laiziCount, MahjongAvatar* pAvatar) const {
				if (!chaptersAllowKeZiOnly(pAvatar))
					return false;
				std::sort(normalTiles.begin(), normalTiles.end(), [](const MahjongTile& a, const MahjongTile& b) {
					return a.getTile() < b.getTile();
				});
				return canWinKeZiOnly(normalTiles, laiziCount, 0);
			}

			bool canHuJiangJiang(MahjongTileArray normalTiles, int laiziCount, MahjongAvatar* pAvatar,
				const MahjongTile::Tile& laiziTile) const {
				(void)laiziCount;
				return all258WithChapters(normalTiles, pAvatar, laiziTile);
			}

			void checkJiangJiangHuWithLaizi(const MahjongTileArray& normalTiles, int laiziCount,
				const MahjongTile::Tile& laiziTile, const MahjongTile::TileArray& allGotTiles,
				MahjongGenre::TingPaiArray& tps, std::set<MahjongTile::Tile>& tingSet,
				MahjongAvatar* pAvatar) const {
				for (int pi = 0; pi < 3; pi++) {
					MahjongTile::Pattern pat = static_cast<MahjongTile::Pattern>(
						static_cast<int>(MahjongTile::Pattern::Tong) + pi);
					for (int ni = 1; ni <= 9; ni++) {
						MahjongTile::Tile candidate(pat, static_cast<MahjongTile::Number>(ni));
						if (isTileTaken(candidate, allGotTiles))
							continue;

						bool candidateIsLaizi = laiziTile.isValid() && candidate == laiziTile;
						int wildCount = candidateIsLaizi ? (laiziCount + 1) : laiziCount;
						MahjongTileArray testTiles = normalTiles;
						if (!candidateIsLaizi) {
							MahjongTile candMt;
							candMt.setTile(candidate);
							testTiles.push_back(candMt);
						}
						if (canHuJiangJiang(testTiles, wildCount, pAvatar, laiziTile))
							addTingTile(candidate, MahjongGenre::HuStyle::JiangJiangHu, allGotTiles, tps, tingSet);
					}
				}
			}

			void checkQingYiSeWithLaizi(const MahjongTileArray& normalTiles, int laiziCount,
				const MahjongTile::Tile& laiziTile, const MahjongTile::TileArray& allGotTiles,
				MahjongGenre::TingPaiArray& tps, std::set<MahjongTile::Tile>& tingSet,
				MahjongAvatar* pAvatar) const {
				for (int pi = 0; pi < 3; pi++) {
					MahjongTile::Pattern pat = static_cast<MahjongTile::Pattern>(
						static_cast<int>(MahjongTile::Pattern::Tong) + pi);
					for (int ni = 1; ni <= 9; ni++) {
						MahjongTile::Tile candidate(pat, static_cast<MahjongTile::Number>(ni));
						if (isTileTaken(candidate, allGotTiles))
							continue;

						bool candidateIsLaizi = laiziTile.isValid() && candidate == laiziTile;
						int wildCount = candidateIsLaizi ? (laiziCount + 1) : laiziCount;
						MahjongTileArray testTiles = normalTiles;
						if (!candidateIsLaizi) {
							MahjongTile candMt;
							candMt.setTile(candidate);
							testTiles.push_back(candMt);
						}
						if (canHuQingYiSe(testTiles, wildCount, pAvatar, laiziTile))
							addTingTile(candidate, MahjongGenre::HuStyle::QingYiSe, allGotTiles, tps, tingSet);
					}
				}
			}

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
		 * 碰碰胡 + 赖子万能牌检查
		 * 碰碰胡要求：所有面子都是刻子（无顺子），雀头不需要258限制
		 */
		void checkPengPengHuWithLaizi(const MahjongTileArray& normalTiles, int laiziCount,
			const MahjongTile::Tile& laiziTile,
			const MahjongTile::TileArray& allGotTiles,
			MahjongGenre::TingPaiArray& tps,
			std::set<MahjongTile::Tile>& tingSet,
			MahjongAvatar* pAvatar) const {

			// 碰碰胡要求所有面子（含碰杠）都是刻子，不能有顺子（吃的章）
			if (pAvatar != nullptr) {
				const MahjongChapterArray& chapters = pAvatar->getChapters();
				for (const auto& ch : chapters) {
					if (ch.getType() == MahjongChapter::Type::Chi) {
						// 有吃的顺子，不可能是碰碰胡
						return;
					}
				}
			}

			for (int pi = 0; pi < 3; pi++) {
				MahjongTile::Pattern pat = static_cast<MahjongTile::Pattern>(
					static_cast<int>(MahjongTile::Pattern::Tong) + pi);
				for (int ni = 1; ni <= 9; ni++) {
					MahjongTile::Tile candidate(pat, static_cast<MahjongTile::Number>(ni));

					bool allTaken = false;
					for (const auto& gt : allGotTiles) {
						if (gt == candidate) { allTaken = true; break; }
					}
					if (allTaken) continue;

					bool candidateIsLaizi = laiziTile.isValid() && candidate == laiziTile;
					int wildCount = candidateIsLaizi ? (laiziCount + 1) : laiziCount;

					MahjongTileArray testTiles;
					if (candidateIsLaizi) {
						testTiles = normalTiles;
					} else {
						testTiles = normalTiles;
						MahjongTile candMt;
						candMt.setTile(candidate);
						testTiles.push_back(candMt);
					}
					std::sort(testTiles.begin(), testTiles.end(),
						[](const MahjongTile& a, const MahjongTile& b) {
							return a.getTile() < b.getTile();
						});

					// 碰碰胡：只允许刻子，不允许顺子，雀头不做258限制
					if (canWinKeZiOnly(testTiles, wildCount, 0))
						addTingTile(candidate, MahjongGenre::HuStyle::PengPengHu, allGotTiles, tps, tingSet);
				}
			}
		}

		/**
		 * 碰碰胡检测：只允许刻子（无顺子），雀头不做258限制
		 * 类似canWinWithWildcards，但canFormMelds改为canFormKeZiOnly
		 */
		bool canWinKeZiOnly(MahjongTileArray& tiles, int wildLeft, int depth) const {
			int size = static_cast<int>(tiles.size());
			if ((size + wildLeft) % 3 != 2)
				return false;

			return canWinKeZiOnlyWithJiang(tiles, wildLeft, depth);
		}

		bool canWinKeZiOnlyWithJiang(MahjongTileArray& tiles, int wildLeft, int depth) const {
			int size = static_cast<int>(tiles.size());
			if (size == 0 && wildLeft == 0)
				return true;
			if (size == 0 && wildLeft >= 0) {
				return (wildLeft >= 2 && (wildLeft - 2) % 3 == 0);
			}
			if (wildLeft < 0)
				return false;

			// 尝试普通对子做雀头（不限制258）
			for (int i = 0; i < size; i++) {
				if (i + 1 < size && tiles[i].getTile() == tiles[i + 1].getTile()) {
					MahjongTileArray remaining;
					for (int j = 0; j < size; j++) {
						if (j == i || j == i + 1) continue;
						remaining.push_back(tiles[j]);
					}
					if (canFormKeZiOnly(remaining, wildLeft, depth + 1))
						return true;
					while (i + 1 < size && tiles[i].getTile() == tiles[i + 1].getTile())
						i++;
				}
			}

			// 万能牌做雀头（2个万能）
			if (wildLeft >= 2) {
				if (canFormKeZiOnly(tiles, wildLeft - 2, depth + 1))
					return true;
			}

			// 1个万能 + 1张普通牌做雀头（不限制258）
			if (wildLeft >= 1) {
				for (int i = 0; i < size; i++) {
					MahjongTileArray remaining;
					for (int j = 0; j < size; j++) {
						if (j == i) continue;
						remaining.push_back(tiles[j]);
					}
					if (canFormKeZiOnly(remaining, wildLeft - 1, depth + 1))
						return true;
				}
			}

			return false;
		}

		/**
		 * 只允许刻子（无顺子）的面子消除
		 */
		bool canFormKeZiOnly(MahjongTileArray& tiles, int wildLeft, int depth) const {
			if (depth > 20) return false;

			int size = static_cast<int>(tiles.size());
			if ((size + wildLeft) % 3 != 0)
				return false;
			if (size == 0)
				return (wildLeft % 3 == 0);

			int counts[3][9] = { {0} };
			for (int i = 0; i < size; i++) {
				int p = static_cast<int>(tiles[i].getPattern()) - static_cast<int>(MahjongTile::Pattern::Tong);
				int n = static_cast<int>(tiles[i].getNumber()) - 1;
				if (p >= 0 && p < 3 && n >= 0 && n < 9)
					counts[p][n]++;
			}

			for (int p = 0; p < 3; p++) {
				for (int n = 0; n < 9; n++) {
					if (counts[p][n] > 0) {
						return canEliminateKeZiOnly(counts, wildLeft, p, n);
					}
				}
			}
			return (wildLeft % 3 == 0);
		}

		bool canEliminateKeZiOnly(int counts[3][9], int wildLeft, int pat, int num) const {
			while (pat < 3) {
				while (num < 9 && counts[pat][num] == 0)
					num++;
				if (num < 9) break;
				pat++;
				num = 0;
			}
			if (pat >= 3) {
				return (wildLeft % 3 == 0);
			}

			int c = counts[pat][num];

			// 刻子（天然3张）
			if (c >= 3) {
				counts[pat][num] -= 3;
				if (canEliminateKeZiOnly(counts, wildLeft, pat, num))
					return true;
				counts[pat][num] += 3;
			}

			// 万能补刻子
			if (wildLeft > 0) {
				int need = 3 - c;
				if (need > 0 && wildLeft >= need) {
					counts[pat][num] = 0;
					if (canEliminateKeZiOnly(counts, wildLeft - need, pat, num))
						return true;
					counts[pat][num] = c;
				}
			}

			return false;
		}

		/**
		 * 七小对 + 赖子万能牌检查
		 */
			bool canHuQiXiaoDui(const MahjongTileArray& normalTiles, int laiziCount, MahjongAvatar* pAvatar) const {
				if (pAvatar != nullptr && !pAvatar->getChapters().empty())
					return false;
				if (static_cast<int>(normalTiles.size()) + laiziCount != 14)
					return false;

				std::map<MahjongTile::Tile, int> tileCounts;
				for (const auto& mt : normalTiles)
					tileCounts[mt.getTile()]++;

				int singleCount = 0;
				for (const auto& kv : tileCounts) {
					if (kv.second % 2 != 0)
						singleCount++;
				}

				if (singleCount > laiziCount)
					return false;
				return ((laiziCount - singleCount) % 2) == 0;
			}

			MahjongGenre::HuStyle getQiXiaoDuiStyle(const MahjongTileArray& normalTiles, int laiziCount,
				MahjongAvatar* pAvatar) const {
				if (!canHuQiXiaoDui(normalTiles, laiziCount, pAvatar))
					return MahjongGenre::HuStyle::Invalid;

				int quadCount = 0;
				std::map<MahjongTile::Tile, int> tileCounts;
				for (const MahjongTile& mt : normalTiles)
					tileCounts[mt.getTile()]++;
				for (const auto& kv : tileCounts) {
					if (kv.second >= 4)
						quadCount++;
				}

				if (quadCount == 1)
					return MahjongGenre::HuStyle::QiXiaoDui1;
				if (quadCount == 2)
					return MahjongGenre::HuStyle::QiXiaoDui2;
				if (quadCount >= 3)
					return MahjongGenre::HuStyle::QiXiaoDui3;
				return MahjongGenre::HuStyle::QiXiaoDui;
			}

			void checkQiXiaoDuiWithLaizi(const MahjongTileArray& allTiles,
				const MahjongTileArray& normalTiles, int laiziCount,
				const MahjongTile::Tile& laiziTile,
				const MahjongTile::TileArray& allGotTiles,
				MahjongGenre::TingPaiArray& tps,
				std::set<MahjongTile::Tile>& tingSet,
				MahjongAvatar* pAvatar) const {

				(void)allTiles;
				if (pAvatar != nullptr && !pAvatar->getChapters().empty())
					return;
				if (static_cast<int>(normalTiles.size()) + laiziCount != 13)
					return;

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

					// 模拟摸到该牌后，用完整手牌七小对逻辑复算，避免听牌提示和胡牌按钮口径不一致。
					MahjongTileArray testTiles = normalTiles;
					if (!candidateIsLaizi) {
						MahjongTile candMt;
						candMt.setTile(candidate);
						testTiles.push_back(candMt);
					}
					MahjongGenre::HuStyle style = getQiXiaoDuiStyle(testTiles, wildCount, pAvatar);
					if (style != MahjongGenre::HuStyle::Invalid)
						addTingTile(candidate, style, allGotTiles, tps, tingSet);
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
			, _roomFee(0)
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
		, _gangRevealedCount(0)
		, _lastGangType(MahjongAction::Type::Invalid)
		, _lastGangTileId(MahjongTile::INVALID_ID)
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
				_totalWinGolds[i] = 0;
			}

		// 解析玩法配置
		if (!ruleConfig.empty())
			parseRuleConfig(ruleConfig);

		// 押金数额为底注的8倍
		setCashPledge(_diZhu * 8);
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
			if (root.isMember("room_fee") && root["room_fee"].isInt64())
				_roomFee = root["room_fee"].asInt64();
			else if (root.isMember("room_fee") && root["room_fee"].isInt())
				_roomFee = root["room_fee"].asInt();
			else if (root.isMember("room_fee_type") && root["room_fee_type"].isInt())
				_roomFee = root["room_fee_type"].asInt();
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

bool TaoJiangMahjongRoom::shouldAllowDianPaoForAvatar(MahjongAvatar* pAvatar, const MahjongTile& mt) const {
	return shouldAllowDianPaoForAvatar(pAvatar, mt, true);
}

bool TaoJiangMahjongRoom::shouldAllowDianPaoForAvatar(MahjongAvatar* pAvatar, const MahjongTile& mt, bool huTileAsWildcard) const {
	// 桃江麻将规则：平胡不能抓炮；硬庄、报听及其他任意大胡可以抓炮。
	TaoJiangMahjongAvatar* tjAvatar = dynamic_cast<TaoJiangMahjongAvatar*>(pAvatar);
	if (tjAvatar == nullptr)
		return true;

	bool detected = tjAvatar->detectHuStyle(false, mt, huTileAsWildcard);
	if (isYingZhuangHu(tjAvatar, mt, false))
		return true;
	if (!detected)
		return false;
	if (!hasDianPaoDaHu(tjAvatar, mt)) {
		InfoS << "桃江麻将: 平胡不允许抓炮, 玩家=" << pAvatar->getPlayerId();
		return false;
	}

	return true;
}

bool TaoJiangMahjongRoom::canCreateDianPaoOption(MahjongAvatar* pAvatar, const MahjongTile& mt, std::string& passed) const {
	if (pAvatar == nullptr || !canDianPao())
		return false;
	if (!pAvatar->canDianPao(mt, passed))
		return false;
	bool huTileAsWildcard = !isLaiZi(mt.getTile());
	if (!canHuWithCandidate(pAvatar, mt, false, huTileAsWildcard))
		return false;

	TaoJiangMahjongAvatar* tjAvatar = dynamic_cast<TaoJiangMahjongAvatar*>(pAvatar);
	if (tjAvatar != nullptr) {
		tjAvatar->detectHuStyle(false, mt, huTileAsWildcard);
		if (isYingZhuangHu(tjAvatar, mt, false))
			return true;
	}

	return shouldAllowDianPaoForAvatar(pAvatar, mt, huTileAsWildcard);
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

	bool TaoJiangMahjongRoom::fetchTile(bool bBack) {
		if (earlyTermination()) {
			noMoreTile();
			return false;
		}

		MahjongAvatar* pAvatar = dynamic_cast<MahjongAvatar*>(getAvatar(_actor).get());
		if (pAvatar == nullptr)
			return false;

		MahjongTile mt;
		bool bTest = false;
		const std::string& str = pAvatar->getNextTile();
		if (!str.empty()) {
			bTest = _dealer.fetchTile(mt, str);
			pAvatar->setNextTile(std::string(""));
		}
		if (!bTest) {
			if (bBack)
				_dealer.fetchTile1(mt);
			else
				_dealer.fetchTile(mt);
		}

			changeState(StateMachine::Fetched);

			// 先入手牌并清除过胡限制，再按当前完整手牌判断自摸。
			pAvatar->fetchTile(mt);
			MahjongTileArray tingBaseTiles;
			pAvatar->getTilesNoFetched(tingBaseTiles);
			_rule->checkTingPai(tingBaseTiles, pAvatar->getGangTiles(), pAvatar->getTingTiles(), pAvatar);

			bool canHu = canHuWithCandidate(pAvatar, mt, true);

			if (canHu) {
				int id = _acOpIdAlloc.askForId();
				if (id >= ACTION_OPTION_POOL_SIZE) {
					LOG_ERROR("逻辑错误，动作id大于动作选项池大小");
					return false;
				}
				_acOpPool[id].setType(MahjongAction::Type::ZiMo);
				_acOpPool[id].setId(id);
				_acOpPool[id].setPlayer(_actor);
				_acOpPool[id].setTileId1(mt.getId());
				_acOps1[0].push_back(id);
				pAvatar->addActionOption(id);
			}

			MahjongAction ma(MahjongAction::Type::Fetch, (static_cast<int>(_actors.size()) - 1), mt.getId());
			_actions.push_back(ma);

			notifyFetchTile(pAvatar, bBack);
			notifyTingTile(pAvatar);
			afterFetchChiPeng(pAvatar, mt.getId());

			return true;
	}

	void TaoJiangMahjongRoom::doHu() {
		int selectedHuTileId = MahjongTile::INVALID_ID;
		if (!_acOps2[0].empty())
			selectedHuTileId = _acOpPool[_acOps2[0].at(0)].getTileId1();

		clearActionOptions();
		notifyActionOptionsFinish();
		changeState(StateMachine::End);
		_hu = true;

		int nHuNums = 0;
		int banker = 0;
		bool bTemp = false;
		MahjongAvatar* pAvaFangPao = nullptr;
		MahjongAction::Type eType = MahjongAction::Type::Invalid;

		for (int i = 0; i < getMaxPlayerNums(); i++) {
			TaoJiangMahjongAvatar* avatar = dynamic_cast<TaoJiangMahjongAvatar*>(getAvatar(i).get());
			if (avatar == nullptr || !avatar->isHu())
				continue;

			nHuNums++;
			banker = i;
			avatar->addHuTimes();
			if (avatar->isMenQing())
				avatar->addHuWay(MahjongGenre::HuWay::MenQing);

			if (avatar->isZiMo()) {
				avatar->addZiMo();
				_huTileId = (selectedHuTileId != MahjongTile::INVALID_ID) ? selectedHuTileId : avatar->getFetchedTileId();
				if (avatar->getTileNums() == 2)
					avatar->addHuWay(MahjongGenre::HuWay::QuanQiuRen);
				if (_tilesLeft >= _dealer.getTileLeft())
					avatar->addHuWay(MahjongGenre::HuWay::HaiDiLaoYue);

				int gangNums = 0;
				if (!_actors.empty()) {
					const MahjongActor& ma = _actors.back();
					for (int j = ma.getStart(); j < static_cast<int>(_actions.size()); j++) {
						eType = _actions[j].getType();
						if (eType == MahjongAction::Type::ZhiGang ||
							eType == MahjongAction::Type::JiaGang ||
							eType == MahjongAction::Type::AnGang)
							gangNums++;
					}
				}
				if (gangNums == 1)
					avatar->addHuWay(MahjongGenre::HuWay::GangShangHua1);
				else if (gangNums == 2)
					avatar->addHuWay(MahjongGenre::HuWay::GangShangHua2);
				else if (gangNums == 3)
					avatar->addHuWay(MahjongGenre::HuWay::GangShangHua3);
				else if (gangNums >= 4)
					avatar->addHuWay(MahjongGenre::HuWay::GangShangHua4);
			} else {
				avatar->addJiePao();
				if (pAvaFangPao == nullptr) {
					pAvaFangPao = dynamic_cast<MahjongAvatar*>(getAvatar(_actor).get());
					if (pAvaFangPao != nullptr)
						pAvaFangPao->addFangPao();
				}
				if (pAvaFangPao != nullptr && pAvaFangPao->getTileNums() == 1)
					avatar->addHuWay(MahjongGenre::HuWay::QuanQiuPao);
				_huTileId = (selectedHuTileId != MahjongTile::INVALID_ID) ? selectedHuTileId : _playedTileId;
				if (_tilesLeft >= _dealer.getTileLeft())
					avatar->addHuWay(MahjongGenre::HuWay::HaiDiPao);
				if (_waitingQiangGang)
					avatar->addHuWay(MahjongGenre::HuWay::QiangGangHu1);
			}

			MahjongTile huTile;
			huTile.setId(_huTileId);
			_dealer.getTileById(huTile);

			if (_baoTinged[i])
				avatar->addHuWay(MahjongGenre::HuWay::BaoTing);

			bool yingZhuang = isYingZhuangHu(avatar, huTile, avatar->isZiMo());
			avatar->setYingZhuang(yingZhuang);

			if (_laiziEnabled) {
				if (avatar->isZiMo()) {
					int laiZiCount = countLaiZiForHu(avatar, huTile, true);
					if (laiZiCount >= 4)
						avatar->addHuWay(MahjongGenre::HuWay::TianTianHu);
					else if (laiZiCount >= 3)
						avatar->addHuWay(MahjongGenre::HuWay::TianHu);

					int mingZiCount = countMingZiInHand(avatar);
					if (mingZiCount >= 3)
						avatar->addHuWay(MahjongGenre::HuWay::DiHu);
				}
			}

			if (isHeiTianHu(avatar)) {
				avatar->addHuStyle(MahjongGenre::HuStyle::HeiTianHu);
				InfoS << "黑天胡检测成功, 座位=" << i;
			}
		}

		notifyHuTile();
		notifyShowTiles();
		calcHuScore();
		doJieSuan();
		bankerNextRound(nHuNums, banker);

		int lastActor = _actor;
		MahjongAction ma;
		ma.setTile(_huTileId);
		for (int i = 0; i < getMaxPlayerNums(); i++) {
			bTemp = true;
			MahjongAvatar* pAvatar = dynamic_cast<MahjongAvatar*>(getAvatar(i).get());
			if (pAvatar == nullptr)
				continue;
			if (pAvatar->isZiMo())
				ma.setType(MahjongAction::Type::ZiMo);
			else if (pAvatar->isDianPao())
				ma.setType(MahjongAction::Type::DianPao);
			else
				bTemp = false;
			if (!bTemp)
				continue;

			ma.setSlot(static_cast<int>(_actors.size()));
			updateCurrentActor(i);
			_actions.push_back(ma);
		}
		_actor = lastActor;
		afterHu();
	}

	void TaoJiangMahjongRoom::onBaoTing(const NetMessage::Ptr& netMsg) {
		if (!_baoTingEnabled)
			return;
		if (_roundState != StageState::Underway)
			return;

		TaoJiangMahjongAvatar* avatar = nullptr;
		for (int i = 0; i < getMaxPlayerNums(); i++) {
			GameAvatar::Ptr av = getAvatar(i);
			if (av && av->getSession() == netMsg->getSession()) {
				avatar = dynamic_cast<TaoJiangMahjongAvatar*>(av.get());
				break;
			}
		}
		if (avatar == nullptr)
			return;

		int seat = avatar->getSeat();

		// 庄家不能报听
		if (seat == _banker) {
			InfoS << "桃江麻将: 庄家不能报听, 玩家=" << avatar->getPlayerId();
			return;
		}

		// 只能在首轮摸牌前报听（没有打出过牌）
		if (avatar->getPlayedTileNums() > 0) {
			InfoS << "桃江麻将: 已出牌不能报听, 玩家=" << avatar->getPlayerId();
			return;
		}

		// 必须已听牌
		const MahjongGenre::TingPaiArray& tingTiles = avatar->getTingTiles();
		if (tingTiles.empty()) {
			InfoS << "桃江麻将: 未听牌不能报听, 玩家=" << avatar->getPlayerId();
			return;
		}

		_baoTinged[seat] = true;
		avatar->setBaoTinged(true);

		// 通知所有人报听
		MsgTJBaoTing msg;
		msg.seat = seat;
		sendMessageToAll(msg);

		InfoS << "桃江麻将: 玩家=" << avatar->getPlayerId() << ", 座位=" << seat << " 报听成功";
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

		// 初始化杠翻牌系统
		_gangRevealedCount = 0;
		_lastGangType = MahjongAction::Type::Invalid;
		_lastGangTileId = MahjongTile::INVALID_ID;

		// 重置开杠后标志
		for (int i = 0; i < getMaxPlayerNums(); i++) {
			auto ava = getAvatar(i);
			if (ava) {
				TaoJiangMahjongAvatar* tjAva = dynamic_cast<TaoJiangMahjongAvatar*>(ava.get());
				if (tjAva) tjAva->setAfterGang(false);
			}
		}
	}

	bool TaoJiangMahjongRoom::canShuffleCardsBeforeNextRound(const std::string& playerId,
		int& nextRoundNo,
		int& roundCount,
		std::string& errMsg) const {
		nextRoundNo = _roundNo + 1;
		roundCount = _roundCount;
		if (!hasAvatar(playerId)) {
			errMsg = "你不在当前房间内";
			return false;
		}
		if (_roundNo <= 0) {
			errMsg = "首局开始前不能洗牌";
			return false;
		}
		if (_roundCount > 0 && _roundNo >= _roundCount) {
			errMsg = "全部对局已结束，不能洗牌";
			return false;
		}
		if (_roundState != StageState::NotStarted) {
			errMsg = "下局开始前才能洗牌";
			return false;
		}
		return true;
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
		else if (msgType == MsgTJBaoTing::TYPE)
			onBaoTing(netMsg);
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
		if (_roundCount > 0 && _roundNo >= _roundCount)
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
		if (_roundCount > 0 && _roundNo >= _roundCount)
			return;
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
		if (_roundCount > 0 && _roundNo >= _roundCount)
			return;

		// 开始一局
		_roundState = StageState::Underway;
		_backupBanker = _banker;

		clean();
		_roundNo++;

		// 先洗牌并确定赖子（必须在发送 MsgTJStartRound 之前完成）
		_dealer.shuffle();
		determineLaiZi();

		// 将赖子信息设置到每个avatar，供听牌检测使用
		if (_laiziEnabled) {
			for (int i = 0; i < getMaxPlayerNums(); i++) {
				TaoJiangMahjongAvatar* av = dynamic_cast<TaoJiangMahjongAvatar*>(getAvatar(i).get());
				if (av != nullptr) {
					av->setLaiZi(_laiZi);
					av->setMingZi(_laiZiOriginal);
				}
			}
		} else {
			MahjongTile::Tile invalidTile;
			invalidTile.setInvalid();
			for (int i = 0; i < getMaxPlayerNums(); i++) {
				TaoJiangMahjongAvatar* av = dynamic_cast<TaoJiangMahjongAvatar*>(getAvatar(i).get());
				if (av != nullptr) {
					av->setLaiZi(invalidTile);
					av->setMingZi(invalidTile);
				}
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
		int score = 0;
		int scores[4] = { 0, 0, 0, 0 };
		double diZhu = _diZhu;
		TaoJiangMahjongAvatar* avatar1 = nullptr;
		TaoJiangMahjongAvatar* avatar2 = nullptr;
		for (int i = 0; i < getMaxPlayerNums(); i++) {
			avatar1 = dynamic_cast<TaoJiangMahjongAvatar*>(getAvatar(i).get());
			if (avatar1 == nullptr)
				continue;
			// 算胡分
			if (avatar1->isHu()) {
				int daHuCount = avatar1->getDaHuCount();
				score = calcBaseHuScore(daHuCount, avatar1->isZiMo(), avatar1->isYingZhuang());

				if (avatar1->isDianPao()) {
					// 点炮：放炮者一人承担（门子x1）
					if (_waitingQiangGang) {
						MahjongTile huTile;
						huTile.setId(_huTileId);
						if (_dealer.getTileById(huTile)) {
							int qiangGangScore = calcQiangGangHuScore(huTile);
							if (qiangGangScore > 0)
								score = qiangGangScore;
						}
					}
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
			// 桃江麻将没有杠分（规则7.3：没有杠分）
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

		// 结算前按本局实际应赔金额补足押金，避免 5 分局初始押金 250 截断 480 等正常结算。
		for (int i = 0; i < getMaxPlayerNums(); i++) {
			GameAvatar::Ptr ptr = getAvatar(i);
			avatar1 = dynamic_cast<TaoJiangMahjongAvatar*>(ptr.get());
			if (avatar1 == NULL)
				continue;

			avatar1->getLoseScores(loseScores);
			double requiredCapital = static_cast<double>(avatar1->getCashPledge());
			double owed = 0.0;
			for (int j = 0; j < 4; j++) {
				if (i != j && loseScores[j] > 0)
					owed += diZhu * loseScores[j];
			}
			if (owed > requiredCapital) {
				int64_t pledgeNeed = static_cast<int64_t>(std::ceil(owed));
				if (!deductCashPledge(ptr, pledgeNeed, false)) {
					InfoS << "桃江麻将结算押金不足，玩家=" << avatar1->getPlayerId()
						<< "，需要=" << pledgeNeed << "，当前押金=" << avatar1->getCashPledge();
				}
			}
		}

		// 使用DebtLiquidation清算多边债务
		bool test = false;
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
		const bool allRoundsFinished = (_roundCount > 0 && _roundNo >= _roundCount);

		MsgTJSettlement msg;
		msg.roundNo = _roundNo;
		msg.roundCount = _roundCount;
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
				_totalWinGolds[i] += msg.winGolds[i];
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
		if (allRoundsFinished) {
			std::vector<std::pair<std::string, int64_t>> netWins;
			for (int i = 0; i < getMaxPlayerNums(); i++) {
				avatar = dynamic_cast<TaoJiangMahjongAvatar*>(getAvatar(i).get());
				if (avatar == NULL)
					continue;
				netWins.emplace_back(avatar->getPlayerId(), _totalWinGolds[i]);
			}
			calcRoomFeeSettlementData(_roomFee, netWins,
				msg.roomFeeTotal, msg.roomFeePlayerIds, msg.roomFeeAmounts);
			getShuffleFeeSettlementData(msg.shuffleFeeTotal,
				msg.shuffleFeePlayerIds, msg.shuffleFeeAmounts);
		}
		for (int i = 0; i < getMaxPlayerNums(); i++) {
			avatar = dynamic_cast<TaoJiangMahjongAvatar*>(getAvatar(i).get());
			if (avatar == NULL)
				continue;
			msg.kick = _kicks[i] && !allRoundsFinished;
			msg.send(avatar->getSession());
		}
	}

	void TaoJiangMahjongRoom::afterHu() {
		saveRoundRecord();
		const bool allRoundsFinished = (_roundCount > 0 && _roundNo >= _roundCount);

		GameAvatar::Ptr avatar;
			for (int i = 0; i < getMaxPlayerNums(); i++) {
				avatar = getAvatar(i);
				if (!avatar)
					continue;
				if (_kicks[i] && !allRoundsFinished)
					kickAvatar(avatar);
				else
					avatar->setReady(false);
			}
			if (allRoundsFinished) {
				publishFinalRoomFee();
				gameOver();
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

		if (_roundNo > 0) {
			MsgTJSettlement settlement;
			settlement.kick = false;
			settlement.roundNo = _roundNo;
			settlement.roundCount = _roundCount;
			std::vector<std::pair<std::string, int64_t>> netWins;
			for (int i = 0; i < getMaxPlayerNums(); i++) {
				TaoJiangMahjongAvatar* avatar = dynamic_cast<TaoJiangMahjongAvatar*>(getAvatar(i).get());
				if (avatar == NULL)
					continue;
				settlement.winGolds[i] = static_cast<int>(_totalWinGolds[i]);
				netWins.emplace_back(avatar->getPlayerId(), _totalWinGolds[i]);
			}
			calcRoomFeeSettlementData(_roomFee, netWins,
				settlement.roomFeeTotal, settlement.roomFeePlayerIds, settlement.roomFeeAmounts);
			getShuffleFeeSettlementData(settlement.shuffleFeeTotal,
				settlement.shuffleFeePlayerIds, settlement.shuffleFeeAmounts);
			sendMessageToAll(settlement);
		}

		MsgDisband msg;
		sendMessageToAll(msg);

			_roundState = StageState::NotStarted;

			publishFinalRoomFee();
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

	void TaoJiangMahjongRoom::publishFinalRoomFee() {
		if (_roundNo <= 0)
			return;
		std::vector<std::pair<std::string, int64_t>> netWins;
		for (int i = 0; i < getMaxPlayerNums(); i++) {
			GameAvatar::Ptr avatar = getAvatar(i);
			if (!avatar)
				continue;
			netWins.emplace_back(avatar->getPlayerId(), _totalWinGolds[i]);
		}
		publishRoomFeeOnGameOver(_roomFee, netWins, "TaoJiangMahjong", "桃江麻将整场房费");
	}

	int TaoJiangMahjongRoom::countMingZiInHand(MahjongAvatar* pAvatar) const {
		if (pAvatar == nullptr || !_laiziEnabled)
			return 0;

		int count = 0;
		const MahjongTileArray& handTiles = pAvatar->getTiles();
		for (const auto& mt : handTiles) {
			if (isLaiZiOriginal(mt.getTile()))
				count++;
		}
		return count;
	}

	int TaoJiangMahjongRoom::countLaiZiInHand(MahjongAvatar* pAvatar) const {
		if (pAvatar == nullptr || !_laiziEnabled)
			return 0;

		int count = 0;
		const MahjongTileArray& handTiles = pAvatar->getTiles();
		for (const auto& mt : handTiles) {
			if (isLaiZi(mt.getTile()))
				count++;
		}
		return count;
	}

	int TaoJiangMahjongRoom::countLaiZiForHu(TaoJiangMahjongAvatar* avatar, const MahjongTile& huTile, bool includeHuTile) const {
		if (avatar == nullptr || !_laiziEnabled)
			return 0;

		int count = 0;
		bool huTileInHand = false;
		const MahjongTileArray& handTiles = avatar->getTiles();
		for (const MahjongTile& mt : handTiles) {
			if (isLaiZi(mt.getTile()))
				count++;
			if (huTile.isValid() && mt.getId() == huTile.getId())
				huTileInHand = true;
		}

		if (includeHuTile && huTile.isValid() && !huTileInHand && isLaiZi(huTile.getTile()))
			count++;
		return count;
	}

	bool TaoJiangMahjongRoom::isYingZhuangHu(TaoJiangMahjongAvatar* avatar, const MahjongTile& huTile, bool zimo) const {
		if (avatar == nullptr)
			return false;
		if (!_laiziEnabled)
			return true;

		MahjongTileArray handTiles = avatar->getTiles();
		if (!zimo) {
			handTiles.push_back(huTile);
		} else if (huTile.isValid()) {
			bool inHand = false;
			for (const MahjongTile& mt : handTiles) {
				if (mt.getId() == huTile.getId()) {
					inHand = true;
					break;
				}
			}
			if (!inHand)
				handTiles.push_back(huTile);
		}

		const MahjongChapterArray& chapters = avatar->getChapters();
		bool hasLaiZi = false;
		for (const MahjongTile& mt : handTiles) {
			if (isLaiZi(mt.getTile())) {
				hasLaiZi = true;
				break;
			}
		}
		for (const auto& ch : chapters) {
			const MahjongTileArray& lstTiles = ch.getAllTiles();
			for (const MahjongTile& mt : lstTiles) {
				if (isLaiZi(mt.getTile())) {
					hasLaiZi = true;
					break;
				}
			}
			if (hasLaiZi)
				break;
		}
		if (!hasLaiZi)
			return true;

		for (const auto& ch : chapters) {
			if (!isNaturalChapterWithoutWildcards(ch))
				return false;
		}

		std::sort(handTiles.begin(), handTiles.end(), [](const MahjongTile& a, const MahjongTile& b) {
			return a.getTile() < b.getTile();
		});
		if (!canHuNaturallyWithoutWildcards(handTiles))
			return false;

		unsigned int style = static_cast<unsigned int>(avatar->getHuStyle());
		if (hasHuStyleFlag(style, MahjongGenre::HuStyle::QingYiSe) &&
			!allSameSuitNaturalTilesWithChapters(handTiles, chapters))
			return false;
		if (hasHuStyleFlag(style, MahjongGenre::HuStyle::PengPengHu) &&
			(!chaptersAreNaturalKeZiOnly(chapters) || !canWinKeZiNatural(handTiles)))
			return false;
		if (hasHuStyleFlag(style, MahjongGenre::HuStyle::QiXiaoDui) &&
			!naturalQiXiaoDuiMatchesHuStyle(handTiles, chapters, style))
			return false;
		if (hasHuStyleFlag(style, MahjongGenre::HuStyle::JiangJiangHu) &&
			!all258NaturalTilesWithChapters(handTiles, chapters))
			return false;
		if (hasHuStyleFlag(style, MahjongGenre::HuStyle::HeiTianHu))
			return false;

		return true;
	}

	bool TaoJiangMahjongRoom::isHeiTianHu(TaoJiangMahjongAvatar* avatar) const {
		if (avatar == nullptr || !avatar->isZiMo())
			return false;
		if (avatar->getPlayedTileNums() != 0 || !noChiPengGang())
			return false;

		const MahjongTileArray& handTiles = avatar->getTiles();
		int counts[3][9] = { {0} };
		for (const MahjongTile& mt : handTiles) {
			if (!isNumberTile(mt))
				return false;
			if (_laiziEnabled && isLaiZi(mt.getTile()))
				return false;
			if (is258(mt.getTile()))
				return false;
			int p = static_cast<int>(mt.getPattern()) - static_cast<int>(MahjongTile::Pattern::Tong);
			int n = static_cast<int>(mt.getNumber()) - 1;
			counts[p][n]++;
			if (counts[p][n] >= 3)
				return false;
		}
		for (int p = 0; p < 3; p++) {
			for (int n = 0; n <= 6; n++) {
				if (counts[p][n] > 0 && counts[p][n + 1] > 0 && counts[p][n + 2] > 0)
					return false;
			}
		}
		return true;
	}

	bool TaoJiangMahjongRoom::hasDianPaoDaHu(TaoJiangMahjongAvatar* avatar, const MahjongTile& mt) const {
		if (avatar == nullptr)
			return false;
		int style = avatar->getHuStyle();
		bool hasDaHu = false;
		if ((style & static_cast<int>(MahjongGenre::HuStyle::QiXiaoDui)) == static_cast<int>(MahjongGenre::HuStyle::QiXiaoDui))
			hasDaHu = true;
		if ((style & static_cast<int>(MahjongGenre::HuStyle::PengPengHu)) == static_cast<int>(MahjongGenre::HuStyle::PengPengHu))
			hasDaHu = true;
		if ((style & static_cast<int>(MahjongGenre::HuStyle::JiangJiangHu)) == static_cast<int>(MahjongGenre::HuStyle::JiangJiangHu))
			hasDaHu = true;
		if ((style & static_cast<int>(MahjongGenre::HuStyle::QingYiSe)) == static_cast<int>(MahjongGenre::HuStyle::QingYiSe))
			hasDaHu = true;
		if ((style & static_cast<int>(MahjongGenre::HuStyle::HeiTianHu)) == static_cast<int>(MahjongGenre::HuStyle::HeiTianHu))
			hasDaHu = true;
		if (_baoTinged[avatar->getSeat()])
			hasDaHu = true;
		bool yingZhuang = isYingZhuangHu(avatar, mt, false);
		if (yingZhuang)
			hasDaHu = true;
		return hasDaHu;
	}

	void TaoJiangMahjongRoom::ensureTingTile(MahjongAvatar* avatar, const MahjongTile& mt, MahjongGenre::HuStyle style) const {
		if (avatar == nullptr || !mt.getTile().isValid())
			return;

		MahjongGenre::TingPaiArray& tingTiles = avatar->getTingTiles();
		for (const MahjongGenre::TingPai& tp : tingTiles) {
			if (mt.isSame(tp.tile))
				return;
		}
		tingTiles.push_back(MahjongGenre::TingPai(mt.getTile(), style));
	}

	bool TaoJiangMahjongRoom::canHuWithCandidate(MahjongAvatar* avatar, const MahjongTile& mt, bool tileAlreadyInHand,
		bool huTileAsWildcard) const {
		if (avatar == nullptr || !mt.getTile().isValid())
			return false;

		bool cachedCanHu = huTileAsWildcard ? avatar->canHu(mt) : false;
		TaoJiangMahjongRule* tjRule = dynamic_cast<TaoJiangMahjongRule*>(_rule.get());
		if (tjRule == nullptr)
			return cachedCanHu;

		MahjongTileArray testTiles = avatar->getTiles();
		if (!tileAlreadyInHand)
			testTiles.push_back(mt);

		const MahjongTile* forcedNaturalTile = huTileAsWildcard ? nullptr : &mt;
		bool currentCanHu = tjRule->canHuWithHand(testTiles, avatar, forcedNaturalTile);
		if (currentCanHu)
			ensureTingTile(avatar, mt, MahjongGenre::HuStyle::Invalid);
		return cachedCanHu || currentCanHu;
	}

	int TaoJiangMahjongRoom::calcBaseHuScore(int daHuCount, bool zimo, bool yingZhuang) const {
		int multiplier = 1;
		int baseMultiplier = zimo ? 3 : 2;
		if (daHuCount > 0)
			multiplier = baseMultiplier * daHuCount;
		int cap = (_maxScore > 0) ? _maxScore : 18;
		if (multiplier > cap)
			multiplier = cap;
		if (yingZhuang)
			multiplier *= 2;
		return 8 * multiplier;
	}

	int TaoJiangMahjongRoom::calcQiangGangHuScore(const MahjongTile& huTile) const {
		TaoJiangMahjongAvatar* gangPlayer = dynamic_cast<TaoJiangMahjongAvatar*>(getAvatar(_actor).get());
		if (gangPlayer == nullptr)
			return 0;

		int oldStyle = 0;
		int oldStyleEx = 0;
		int oldWay = 0;
		bool oldYingZhuang = false;
		gangPlayer->getHuCalcState(oldStyle, oldStyleEx, oldWay, oldYingZhuang);

		int score = 0;
		if (gangPlayer->detectHuStyle(true, huTile)) {
			gangPlayer->addHuWay(MahjongGenre::HuWay::GangShangHua1);
			gangPlayer->setYingZhuang(isYingZhuangHu(gangPlayer, huTile, true));
			score = calcBaseHuScore(gangPlayer->getDaHuCount(), true, gangPlayer->isYingZhuang());
		}

		gangPlayer->restoreHuCalcState(oldStyle, oldStyleEx, oldWay, oldYingZhuang);
		return score;
	}

	bool TaoJiangMahjongRoom::sameTingTiles(const MahjongGenre::TingPaiArray& a, const MahjongGenre::TingPaiArray& b) const {
		std::set<std::pair<int, int> > setA;
		std::set<std::pair<int, int> > setB;
		for (const MahjongGenre::TingPai& tp : a) {
			setA.insert(std::make_pair(static_cast<int>(tp.tile.getPattern()), static_cast<int>(tp.tile.getNumber())));
		}
		for (const MahjongGenre::TingPai& tp : b) {
			setB.insert(std::make_pair(static_cast<int>(tp.tile.getPattern()), static_cast<int>(tp.tile.getNumber())));
		}
		return setA == setB;
	}

	bool TaoJiangMahjongRoom::shouldKeepTingForGang(TaoJiangMahjongAvatar* avatar, const MahjongTile& gangTile, MahjongAction::Type gangType) const {
		if (avatar == nullptr)
			return false;

		const MahjongGenre::TingPaiArray& beforeTing = avatar->isBaoTinged() ? avatar->getBaoTingTiles() : avatar->getTingTiles();
		if (beforeTing.empty())
			return false;
		if (!avatar->isBaoTinged() && !avatar->isAfterGang())
			return true;

		int removeCount = 0;
		if (gangType == MahjongAction::Type::JiaGang)
			removeCount = 1;
		else if (gangType == MahjongAction::Type::ZhiGang)
			removeCount = 3;
		else if (gangType == MahjongAction::Type::AnGang)
			removeCount = 4;
		else
			return false;

		MahjongTileArray testTiles = avatar->getTiles();
		int removed = 0;
		MahjongTileArray::iterator it = testTiles.begin();
		while (it != testTiles.end() && removed < removeCount) {
			if (it->getTile() == gangTile.getTile()) {
				it = testTiles.erase(it);
				removed++;
			} else {
				++it;
			}
		}
		if (removed != removeCount)
			return false;

		MahjongTile::TileArray gangTiles = avatar->getGangTiles();
		gangTiles.push_back(gangTile.getTile());
		MahjongGenre::TingPaiArray afterTing;
		_rule->checkTingPai(testTiles, gangTiles, afterTing, avatar);
		return sameTingTiles(beforeTing, afterTing);
	}

	bool TaoJiangMahjongRoom::addQiangGangOptions(MahjongAvatar* gangPlayer, const MahjongTileArray& candidateTiles) {
		if (gangPlayer == nullptr || candidateTiles.empty())
			return false;

		bool hasQiangGang = false;
		for (int i = 0; i < getMaxPlayerNums(); i++) {
			if (i == _actor)
				continue;
			MahjongAvatar* opponent = dynamic_cast<MahjongAvatar*>(getAvatar(i).get());
			if (opponent == nullptr)
				continue;

			for (const MahjongTile& candidateTile : candidateTiles) {
				if (!canHuWithCandidate(opponent, candidateTile, false))
					continue;
				if (!shouldAllowDianPaoForAvatar(opponent, candidateTile))
					continue;

				hasQiangGang = true;
				_waitingQiangGang = true;
				int id = _acOpIdAlloc.askForId();
				if (id >= ACTION_OPTION_POOL_SIZE) {
					LOG_ERROR("逻辑错误，动作id大于动作选项池大小");
					continue;
				}
				_acOpPool[id].setType(MahjongAction::Type::DianPao);
				_acOpPool[id].setId(id);
				_acOpPool[id].setPlayer(i);
				_acOpPool[id].setTileId1(candidateTile.getId());
				_acOps1[0].push_back(id);
				opponent->addActionOption(id);
				notifyActionOptions(opponent);
			}
		}
		return hasQiangGang;
	}

	void TaoJiangMahjongRoom::finishGangRevealWithoutHu(MahjongAvatar* gangPlayer) {
		if (gangPlayer == nullptr)
			return;
		for (int k = 0; k < _gangRevealedCount; k++)
			gangPlayer->addPlayedTile(_gangRevealedTiles[k]);
		_gangRevealedCount = 0;
		_lastGangType = MahjongAction::Type::Invalid;
		_lastGangTileId = MahjongTile::INVALID_ID;

		TaoJiangMahjongAvatar* tjGangPlayer = dynamic_cast<TaoJiangMahjongAvatar*>(gangPlayer);
		if (tjGangPlayer)
			tjGangPlayer->setAfterGang(true);
		updateCurrentActor();
		fetchTile();
	}

	void TaoJiangMahjongRoom::afterFetchChiPeng(MahjongAvatar* pAvatar, int fetchedId) {
		if (pAvatar == nullptr)
			return;

		if (fetchedId < 0) {
			_rule->checkTingPai(pAvatar->getTiles(), pAvatar->getGangTiles(), pAvatar->getTingTiles(), pAvatar);
			notifyTingTile(pAvatar);
		}

		// 桃江麻将规则：没有听牌时不能开杠
		const MahjongGenre::TingPaiArray& tingTiles = pAvatar->getTingTiles();
		bool hasTing = !tingTiles.empty();
		TaoJiangMahjongAvatar* tjAvatar = dynamic_cast<TaoJiangMahjongAvatar*>(pAvatar);

		std::vector<int> lstTileIds;
		std::vector<int>::const_iterator it;
		int id = 0;
		MahjongTile gangTile;

		// 加杠：需要听牌才能加杠（开杠后仍可继续加杠，只要听牌不变）
		if (hasTing && pAvatar->canJiaGang(lstTileIds)) {
			it = lstTileIds.begin();
			while (it != lstTileIds.end()) {
				if (_delayJiaGang || (*it == fetchedId)) {
					gangTile.setId(*it);
					if (!_dealer.getTile(gangTile) || !shouldKeepTingForGang(tjAvatar, gangTile, MahjongAction::Type::JiaGang)) {
						++it;
						continue;
					}
					id = _acOpIdAlloc.askForId();
					if (id >= ACTION_OPTION_POOL_SIZE) {
						LOG_ERROR("逻辑错误，动作id大于动作选项池大小");
						return;
					}
					_acOpPool[id].setType(MahjongAction::Type::JiaGang);
					_acOpPool[id].setId(id);
					_acOpPool[id].setPlayer(_actor);
					_acOpPool[id].setTileId1(*it);
					_acOps1[1].push_back(id);
					pAvatar->addActionOption(id);
				}
				++it;
			}
		}

		// 暗杠：需要听牌才能暗杠（开杠后仍可继续暗杠，只要听牌不变）
		if (hasTing && pAvatar->canAnGang(lstTileIds)) {
			it = lstTileIds.begin();
			while (it != lstTileIds.end()) {
				gangTile.setId(*it);
				if (!_dealer.getTile(gangTile) || !shouldKeepTingForGang(tjAvatar, gangTile, MahjongAction::Type::AnGang)) {
					++it;
					continue;
				}
				id = _acOpIdAlloc.askForId();
				if (id >= ACTION_OPTION_POOL_SIZE) {
					LOG_ERROR("逻辑错误，动作id大于动作选项池大小");
					return;
				}
				_acOpPool[id].setType(MahjongAction::Type::AnGang);
				_acOpPool[id].setId(id);
				_acOpPool[id].setPlayer(_actor);
				_acOpPool[id].setTileId1(*it);
				_acOps1[1].push_back(id);
				pAvatar->addActionOption(id);
				++it;
			}
		}

		if (pAvatar->hasActionOption()) {
			// 有杠等选项 → 同时也添加出牌选项，通知客户端并进入等待动作选项状态
			id = _acOpIdAlloc.askForId();
			if (id >= ACTION_OPTION_POOL_SIZE) {
				LOG_ERROR("逻辑错误，动作id大于动作选项池大小");
				return;
			}
			_acOpPool[id].setType(MahjongAction::Type::Play);
			_acOpPool[id].setId(id);
			_acOpPool[id].setPlayer(_actor);
			pAvatar->addActionOption(id);

			notifyActionOptions(pAvatar);
			changeState(StateMachine::Action);
		} else {
			// 通知玩家出牌
			id = _acOpIdAlloc.askForId();
			if (id >= ACTION_OPTION_POOL_SIZE) {
				LOG_ERROR("逻辑错误，动作id大于动作选项池大小");
				return;
			}
			_acOpPool[id].setType(MahjongAction::Type::Play);
			_acOpPool[id].setId(id);
			_acOpPool[id].setPlayer(_actor);
			pAvatar->addActionOption(id);

			// 进入等待出牌状态
			changeState(StateMachine::Play);
			notifyActionOptions(pAvatar);
		}
	}

	bool TaoJiangMahjongRoom::executeGang() {
		// 等待其他玩家选择胡动作
		if (!_acOps1[0].empty())
			return false;
		if (_acOps2[1].empty())
			return false;

		const MahjongActionOption& acOp = _acOpPool[_acOps2[1].at(0)];
		MahjongAvatar* pAvatar = dynamic_cast<MahjongAvatar*>(getAvatar(acOp.getPlayer()).get());
		MahjongTile mt;
		MahjongAction ma;
		mt.setId(acOp.getTileId1());
		_dealer.getTile(mt);
		ma.setTile(acOp.getTileId1());
		MahjongAction::Type gangType = MahjongAction::Type::Invalid;
		TaoJiangMahjongAvatar* tjAvatar = dynamic_cast<TaoJiangMahjongAvatar*>(pAvatar);
		if (pAvatar == nullptr || tjAvatar == nullptr) {
			ErrorS << "逻辑错误，牌桌(Id: " << getId() << ")杠牌玩家不存在!";
			return false;
		}
		const MahjongGenre::TingPaiArray& beforeTing = tjAvatar->isBaoTinged() ? tjAvatar->getBaoTingTiles() : tjAvatar->getTingTiles();
		if (beforeTing.empty()) {
			InfoS << "桃江麻将: 未听牌不能开杠, 玩家=" << pAvatar->getPlayerId();
			return false;
		}
		if (!shouldKeepTingForGang(tjAvatar, mt, acOp.getType())) {
			InfoS << "桃江麻将: 杠后听口改变，拒绝杠牌, 玩家=" << pAvatar->getPlayerId();
			return false;
		}

		if (acOp.getType() == MahjongAction::Type::ZhiGang) {
			if (!pAvatar->doZhiGang(mt, static_cast<int>(_actions.size()), _actor)) {
				ErrorS << "逻辑错误，牌桌(Id: " << getId() << ")玩家(Id: " << pAvatar->getPlayerId() << ")直杠失败!";
				return false;
			}
			ma.setType(MahjongAction::Type::ZhiGang);
			ma.setSlot(static_cast<int>(_actors.size()));

			MahjongAvatar* pCurAva = dynamic_cast<MahjongAvatar*>(getAvatar(_actor).get());
			pCurAva->setPlayedTileAction(mt.getId(), MahjongAction::Type::ZhiGang, acOp.getPlayer());
			gangType = MahjongAction::Type::ZhiGang;
		}
		else if (acOp.getType() == MahjongAction::Type::JiaGang) {
			if (!pAvatar->doJiaGang(mt, static_cast<int>(_actions.size()))) {
				ErrorS << "逻辑错误，牌桌(Id: " << getId() << ")玩家(Id: " << pAvatar->getPlayerId() << ")加杠失败!";
				return false;
			}
			ma.setType(MahjongAction::Type::JiaGang);
			ma.setSlot(static_cast<int>(_actors.size() - 1));
			gangType = MahjongAction::Type::JiaGang;
		}
		else if (acOp.getType() == MahjongAction::Type::AnGang) {
			if (!pAvatar->doAnGang(mt, static_cast<int>(_actions.size()))) {
				ErrorS << "逻辑错误，牌桌(Id: " << getId() << ")玩家(Id: " << pAvatar->getPlayerId() << ")暗杠失败!";
				return false;
			}
			ma.setType(MahjongAction::Type::AnGang);
			ma.setSlot(static_cast<int>(_actors.size() - 1));
			gangType = MahjongAction::Type::AnGang;
		} else {
			ErrorS << "逻辑错误，牌桌(Id: " << getId() << ")玩家(Id: " << pAvatar->getPlayerId() << ")杠牌出错!";
			return false;
		}
		// 更新听牌信息
		_rule->checkTingPai(pAvatar->getTiles(), pAvatar->getGangTiles(), pAvatar->getTingTiles(), pAvatar);
		// 清空所有动作选项
		clearActionOptions();
		// 通知所有人动作选项结束
		notifyActionOptionsFinish();
		// 通知所有人玩家杠牌
		notifyGangTile(pAvatar);
		// 通知玩家听牌
		notifyTingTile(pAvatar);

		if (acOp.getType() == MahjongAction::Type::ZhiGang)
			updateCurrentActor(acOp.getPlayer());
		_actions.push_back(ma);

		// 保存杠类型，用于passActionOption中判断
		_lastGangType = gangType;
		_lastGangTileId = mt.getId();

		// 桃江麻将：杠后翻3张牌
		processGangReveal(pAvatar, gangType, mt);

		return true;
	}

	void TaoJiangMahjongRoom::processGangReveal(MahjongAvatar* gangPlayer, MahjongAction::Type gangType, const MahjongTile& gangTile) {
		if (gangPlayer == nullptr)
			return;

		// 从牌墙翻3张牌
		MahjongTile mt1, mt2, mt3;
		bool hasTile1 = _dealer.fetchTile(mt1);
		bool hasTile2 = _dealer.fetchTile(mt2);
		bool hasTile3 = _dealer.fetchTile(mt3);

		_gangRevealedCount = 0;
		if (hasTile1) _gangRevealedTiles[_gangRevealedCount++] = mt1;
		if (hasTile2) _gangRevealedTiles[_gangRevealedCount++] = mt2;
		if (hasTile3) _gangRevealedTiles[_gangRevealedCount++] = mt3;

		// 通知所有人翻出的牌
		notifyGangReveal(gangPlayer, mt1, mt2, mt3);

		// 检查开杠者能否用翻出的牌杠上花
		bool gangShangHua = false;
		for (int k = 0; k < _gangRevealedCount; k++) {
			MahjongTile& revealTile = _gangRevealedTiles[k];
			if (canHuWithCandidate(gangPlayer, revealTile, false, true)) {
				// 开杠者可以胡这张牌 → 杠上花
				int id = _acOpIdAlloc.askForId();
				if (id >= ACTION_OPTION_POOL_SIZE) {
					LOG_ERROR("逻辑错误，动作id大于动作选项池大小");
					gangShangHua = true; // 标记有杠上花可能
					continue;
				}
				_acOpPool[id].setType(MahjongAction::Type::ZiMo);
				_acOpPool[id].setId(id);
				_acOpPool[id].setPlayer(_actor);
				_acOpPool[id].setTileId1(revealTile.getId());
				_acOps1[0].push_back(id);
				gangPlayer->addActionOption(id);
				gangShangHua = true;
			}
		}

		if (gangShangHua) {
			// 开杠者有杠上花选项，等待选择
			notifyActionOptions(gangPlayer);
			changeState(StateMachine::Action);
		} else {
			MahjongTileArray candidates;
			if (gangType == MahjongAction::Type::ZhiGang || gangType == MahjongAction::Type::JiaGang)
				candidates.push_back(gangTile);
			for (int k = 0; k < _gangRevealedCount; k++)
				candidates.push_back(_gangRevealedTiles[k]);

			if (addQiangGangOptions(gangPlayer, candidates)) {
				// 有对手可以抢杠，进入等待动作选项状态
				changeState(StateMachine::Action);
			} else {
				finishGangRevealWithoutHu(gangPlayer);
			}
		}
	}

	void TaoJiangMahjongRoom::notifyGangReveal(MahjongAvatar* gangPlayer, const MahjongTile& mt1, const MahjongTile& mt2, const MahjongTile& mt3) {
		MsgTJGangReveal msg;
		msg.seat = gangPlayer->getSeat();
		msg.count = _gangRevealedCount;
		for (int k = 0; k < _gangRevealedCount; k++)
			msg.tiles.push_back(_gangRevealedTiles[k]);
		sendMessageToAll(msg);
	}

	void TaoJiangMahjongRoom::passActionOption(const std::string& playerId) {
		MahjongAvatar* pAvatar = dynamic_cast<MahjongAvatar*>(getAvatar(playerId).get());
		if ((pAvatar == nullptr) || !(pAvatar->hasActionOption()))
			return;

		// 处理过胡/过碰
		MahjongTile mt;
		const std::vector<int>& lstAcOps = pAvatar->getAllActionOptions();
		std::vector<int>::const_iterator it = lstAcOps.begin();
		while (it != lstAcOps.end()) {
			const MahjongActionOption& mao = _acOpPool[*it];
			if (mao.getType() == MahjongAction::Type::DianPao) {
				mt.setId(mao.getTileId1());
				if (_dealer.getTileById(mt))
					pAvatar->passDianPao(mt);
			}
			else if (mao.getType() == MahjongAction::Type::Peng) {
				mt.setId(mao.getTileId1());
				if (_dealer.getTileById(mt))
					pAvatar->passPeng(mt);
			}
			++it;
		}
		clearActionOptions(pAvatar);
		bool bRet = executeActionOptions();
		if (bRet)
			return;

		if (_waitingQiangGang) {
			// 继续等待其他玩家抢杠
			if (!_acOps1[0].empty())
				return;

			// 所有人放弃抢杠
			_waitingQiangGang = false;
			MahjongAvatar* gangPlayer = dynamic_cast<MahjongAvatar*>(getAvatar(_actor).get());
			finishGangRevealWithoutHu(gangPlayer);
		} else if (_gangRevealedCount > 0) {
			// 开杠者放弃了杠上花，检查对手能否抢杠
			MahjongAvatar* gangPlayer = dynamic_cast<MahjongAvatar*>(getAvatar(_actor).get());
			MahjongTileArray candidates;
			if (_lastGangType == MahjongAction::Type::ZhiGang || _lastGangType == MahjongAction::Type::JiaGang) {
				MahjongTile gangTile;
				gangTile.setId(_lastGangTileId);
				if (_dealer.getTileById(gangTile))
					candidates.push_back(gangTile);
			}
			for (int k = 0; k < _gangRevealedCount; k++)
				candidates.push_back(_gangRevealedTiles[k]);

			if (addQiangGangOptions(gangPlayer, candidates)) {
				changeState(StateMachine::Action);
			} else {
				finishGangRevealWithoutHu(gangPlayer);
			}
		} else {
			// 非杠翻牌场景，走基类逻辑
			bool bTest = false;
			for (unsigned int i = 0; i < 4; i++) {
				if (!_acOps1[i].empty()) {
					bTest = true;
					break;
				}
			}
			if (!bTest) {
				if (pAvatar->getSeat() == _actor) {
					// 通知出牌
					int id = _acOpIdAlloc.askForId();
					if (id >= ACTION_OPTION_POOL_SIZE) {
						LOG_ERROR("逻辑错误，动作id大于动作选项池大小");
						return;
					}
					_acOpPool[id].setType(MahjongAction::Type::Play);
					_acOpPool[id].setId(id);
					_acOpPool[id].setPlayer(_actor);
					pAvatar->addActionOption(id);
					notifyActionOptions(pAvatar);

					// 进入等待出牌状态
					changeState(StateMachine::Play);
				} else {
					if (!fetchAgainAfterPlay())
						updateCurrentActor();
					fetchTile();
				}
			} else {
				changeState(StateMachine::Action);
			}
		}
	}
}
