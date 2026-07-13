// HongZhongMahjongRule.cpp

#include "HongZhongMahjongRule.h"
#include "MahjongAvatar.h"
#include "MahjongDealer.h"

#include <algorithm>
#include <map>

namespace NiuMa
{
	namespace
	{
		void copyCounts(int dst[3][9], const int src[3][9]) {
			for (int p = 0; p < 3; p++) {
				for (int n = 0; n < 9; n++)
					dst[p][n] = src[p][n];
			}
		}

		bool tileUsedUp(const MahjongTileArray& tiles, const MahjongTile::Tile& tile) {
			int count = 0;
			for (const MahjongTile& mt : tiles) {
				if (mt.getTile() == tile)
					count++;
			}
			return count >= 4;
		}

		void appendTingTile(MahjongGenre::TingPaiArray& tps, const MahjongTile::Tile& tile, int style) {
			for (MahjongGenre::TingPai& tp : tps) {
				if (tp.tile == tile) {
					tp.style |= style;
					return;
				}
			}
			MahjongGenre::TingPai tp;
			tp.tile = tile;
			tp.style = style;
			tps.push_back(tp);
		}
	}

	HongZhongMahjongRule::HongZhongMahjongRule()
		: MahjongRule()
	{}

	HongZhongMahjongRule::~HongZhongMahjongRule() {}

	bool HongZhongMahjongRule::hasZiPai() const {
		return false;
	}

	bool HongZhongMahjongRule::isHongZhong(const MahjongTile::Tile& tile) {
		return tile.getPattern() == MahjongTile::Pattern::Zhong;
	}

	bool HongZhongMahjongRule::isHongZhong(const MahjongTile& tile) {
		return isHongZhong(tile.getTile());
	}

	void HongZhongMahjongRule::getHongZhongTile(MahjongTile& tile) {
		tile.setTile(MahjongTile::Tile(MahjongTile::Pattern::Zhong));
		MahjongDealer::getIdByTile(tile);
	}

	int HongZhongMahjongRule::countHongZhong(const MahjongTileArray& tiles) {
		int count = 0;
		for (const MahjongTile& mt : tiles) {
			if (isHongZhong(mt))
				count++;
		}
		return count;
	}

	int HongZhongMahjongRule::countHongZhong(const MahjongChapterArray& chapters) {
		int count = 0;
		for (const MahjongChapter& chapter : chapters) {
			const MahjongTileArray& tiles = chapter.getAllTiles();
			for (const MahjongTile& mt : tiles) {
				if (isHongZhong(mt))
					count++;
			}
		}
		return count;
	}

	bool HongZhongMahjongRule::isNumberTile(const MahjongTile& tile) {
		int p = static_cast<int>(tile.getPattern()) - static_cast<int>(MahjongTile::Pattern::Tong);
		int n = static_cast<int>(tile.getNumber());
		return p >= 0 && p < 3 && n >= 1 && n <= 9;
	}

	void HongZhongMahjongRule::checkTingPai(const MahjongTileArray& tiles, const MahjongTile::TileArray&,
		MahjongGenre::TingPaiArray& tps, MahjongAvatar* pAvatar) const {
		tps.clear();
		if (tiles.empty() || (tiles.size() % 3) != 1)
			return;

		MahjongChapterArray emptyChapters;
		const MahjongChapterArray* chapters = &emptyChapters;
		if (pAvatar != nullptr)
			chapters = &(pAvatar->getChapters());

		MahjongTileArray testTiles;
		testTiles.reserve(tiles.size() + 1);
		static const MahjongTile::Pattern patterns[3] = {
			MahjongTile::Pattern::Tong,
			MahjongTile::Pattern::Tiao,
			MahjongTile::Pattern::Wan
		};

		for (MahjongTile::Pattern pattern : patterns) {
			for (int n = static_cast<int>(MahjongTile::Number::Yi);
				n <= static_cast<int>(MahjongTile::Number::Jiu); n++) {
				MahjongTile candidate;
				candidate.setTile(MahjongTile::Tile(pattern, static_cast<MahjongTile::Number>(n)));
				MahjongDealer::getIdByTile(candidate);
				if (tileUsedUp(tiles, candidate.getTile()))
					continue;

				testTiles = tiles;
				testTiles.push_back(candidate);
				std::sort(testTiles.begin(), testTiles.end());
				HuAnalysis analysis = analyzeHu(testTiles, *chapters);
				if (analysis.hu)
					appendTingTile(tps, candidate.getTile(), analysis.style);
			}
		}

		MahjongTile hongZhong;
		getHongZhongTile(hongZhong);
		if (!tileUsedUp(tiles, hongZhong.getTile())) {
			testTiles = tiles;
			testTiles.push_back(hongZhong);
			std::sort(testTiles.begin(), testTiles.end());
			HuAnalysis analysis = analyzeHu(testTiles, *chapters);
			if (analysis.hu)
				appendTingTile(tps, hongZhong.getTile(), analysis.style);
		}
	}

	HongZhongMahjongRule::HuAnalysis HongZhongMahjongRule::analyzeHu(const MahjongTileArray& handTiles,
		const MahjongChapterArray& chapters) {
		HuAnalysis result;
		if (handTiles.empty() || (handTiles.size() % 3) != 2)
			return result;

		for (const MahjongChapter& chapter : chapters) {
			if (chapter.getType() == MahjongChapter::Type::Chi)
				return result;
		}

		bool pingHu = canPingHu(handTiles);
		bool qiXiaoDui = chapters.empty() && canQiXiaoDui(handTiles);
		bool pengPengHu = allChaptersKeZi(chapters) && canPengPengHu(handTiles);
		bool qingYiSe = allSameSuit(handTiles, chapters) && (pingHu || qiXiaoDui || pengPengHu);
		if (!pingHu && !qiXiaoDui && !pengPengHu && !qingYiSe)
			return result;

		result.hu = true;
		result.hongZhongCount = countHongZhong(handTiles) + countHongZhong(chapters);
		if (pingHu)
			result.style |= static_cast<int>(MahjongGenre::HuStyle::PingHu);
		if (pengPengHu)
			result.style |= static_cast<int>(MahjongGenre::HuStyle::PengPengHu);
		if (qiXiaoDui)
			result.style |= static_cast<int>(MahjongGenre::HuStyle::QiXiaoDui);
		if (qingYiSe)
			result.style |= static_cast<int>(MahjongGenre::HuStyle::QingYiSe);
		if (result.style == 0)
			result.style = static_cast<int>(MahjongGenre::HuStyle::PingHu);
		return result;
	}

	bool HongZhongMahjongRule::canPingHu(const MahjongTileArray& tiles) {
		if (tiles.empty() || (tiles.size() % 3) != 2)
			return false;

		int counts[3][9] = { {0} };
		int wild = 0;
		for (const MahjongTile& mt : tiles) {
			if (isHongZhong(mt)) {
				wild++;
				continue;
			}
			if (!isNumberTile(mt))
				return false;
			int p = static_cast<int>(mt.getPattern()) - static_cast<int>(MahjongTile::Pattern::Tong);
			int n = static_cast<int>(mt.getNumber()) - 1;
			counts[p][n]++;
		}

		for (int p = 0; p < 3; p++) {
			for (int n = 0; n < 9; n++) {
				if (counts[p][n] >= 2) {
					int tmp[3][9];
					copyCounts(tmp, counts);
					tmp[p][n] -= 2;
					if (canFormMelds(tmp, wild))
						return true;
				}
				if (counts[p][n] >= 1 && wild >= 1) {
					int tmp[3][9];
					copyCounts(tmp, counts);
					tmp[p][n] -= 1;
					if (canFormMelds(tmp, wild - 1))
						return true;
				}
			}
		}
		if (wild >= 2) {
			int tmp[3][9];
			copyCounts(tmp, counts);
			if (canFormMelds(tmp, wild - 2))
				return true;
		}
		return false;
	}

	bool HongZhongMahjongRule::canQiXiaoDui(const MahjongTileArray& tiles) {
		if (tiles.size() != 14)
			return false;

		std::map<MahjongTile::Tile, int> counts;
		int wild = 0;
		for (const MahjongTile& mt : tiles) {
			if (isHongZhong(mt)) {
				wild++;
				continue;
			}
			if (!isNumberTile(mt))
				return false;
			counts[mt.getTile()]++;
		}

		int needWild = 0;
		for (const auto& kv : counts) {
			if ((kv.second % 2) != 0)
				needWild++;
		}
		if (needWild > wild)
			return false;
		return ((wild - needWild) % 2) == 0;
	}

	bool HongZhongMahjongRule::canPengPengHu(const MahjongTileArray& tiles) {
		if (tiles.empty() || (tiles.size() % 3) != 2)
			return false;

		int counts[3][9] = { {0} };
		int wild = 0;
		for (const MahjongTile& mt : tiles) {
			if (isHongZhong(mt)) {
				wild++;
				continue;
			}
			if (!isNumberTile(mt))
				return false;
			int p = static_cast<int>(mt.getPattern()) - static_cast<int>(MahjongTile::Pattern::Tong);
			int n = static_cast<int>(mt.getNumber()) - 1;
			counts[p][n]++;
		}

		for (int p = 0; p < 3; p++) {
			for (int n = 0; n < 9; n++) {
				if (counts[p][n] >= 2) {
					int tmp[3][9];
					copyCounts(tmp, counts);
					tmp[p][n] -= 2;
					if (canFormKeZiOnly(tmp, wild))
						return true;
				}
				if (counts[p][n] >= 1 && wild >= 1) {
					int tmp[3][9];
					copyCounts(tmp, counts);
					tmp[p][n] -= 1;
					if (canFormKeZiOnly(tmp, wild - 1))
						return true;
				}
			}
		}
		if (wild >= 2) {
			int tmp[3][9];
			copyCounts(tmp, counts);
			if (canFormKeZiOnly(tmp, wild - 2))
				return true;
		}
		return false;
	}

	bool HongZhongMahjongRule::canFormMelds(int counts[3][9], int wildLeft) {
		for (int p = 0; p < 3; p++) {
			for (int n = 0; n < 9; n++) {
				if (counts[p][n] <= 0)
					continue;

				if (counts[p][n] >= 3) {
					counts[p][n] -= 3;
					if (canFormMelds(counts, wildLeft)) {
						counts[p][n] += 3;
						return true;
					}
					counts[p][n] += 3;
				}

				int need = 3 - counts[p][n];
				if (counts[p][n] < 3 && need <= wildLeft) {
					int used = counts[p][n];
					counts[p][n] = 0;
					if (canFormMelds(counts, wildLeft - need)) {
						counts[p][n] = used;
						return true;
					}
					counts[p][n] = used;
				}

				if (n <= 6) {
					int needSeq = 0;
					int used[3] = { 0, 0, 0 };
					for (int k = 0; k < 3; k++) {
						if (counts[p][n + k] > 0)
							used[k] = 1;
						else
							needSeq++;
					}
					if (needSeq <= wildLeft) {
						for (int k = 0; k < 3; k++)
							counts[p][n + k] -= used[k];
						if (canFormMelds(counts, wildLeft - needSeq)) {
							for (int k = 0; k < 3; k++)
								counts[p][n + k] += used[k];
							return true;
						}
						for (int k = 0; k < 3; k++)
							counts[p][n + k] += used[k];
					}
				}
				return false;
			}
		}
		return (wildLeft % 3) == 0;
	}

	bool HongZhongMahjongRule::canFormKeZiOnly(const int counts[3][9], int wildLeft) {
		int needWild = 0;
		for (int p = 0; p < 3; p++) {
			for (int n = 0; n < 9; n++) {
				int mod = counts[p][n] % 3;
				if (mod > 0)
					needWild += 3 - mod;
			}
		}
		if (needWild > wildLeft)
			return false;
		return ((wildLeft - needWild) % 3) == 0;
	}

	bool HongZhongMahjongRule::allSameSuit(const MahjongTileArray& handTiles, const MahjongChapterArray& chapters) {
		bool found = false;
		MahjongTile::Pattern suit = MahjongTile::Pattern::Invalid;
		auto testTile = [&](const MahjongTile& mt) -> bool {
			if (isHongZhong(mt))
				return true;
			if (!isNumberTile(mt))
				return false;
			if (!found) {
				found = true;
				suit = mt.getPattern();
				return true;
			}
			return suit == mt.getPattern();
		};

		for (const MahjongTile& mt : handTiles) {
			if (!testTile(mt))
				return false;
		}
		for (const MahjongChapter& chapter : chapters) {
			const MahjongTileArray& tiles = chapter.getAllTiles();
			for (const MahjongTile& mt : tiles) {
				if (!testTile(mt))
					return false;
			}
		}
		return found;
	}

	bool HongZhongMahjongRule::allChaptersKeZi(const MahjongChapterArray& chapters) {
		for (const MahjongChapter& chapter : chapters) {
			MahjongChapter::Type type = chapter.getType();
			if (type == MahjongChapter::Type::Chi)
				return false;
		}
		return true;
	}
}
