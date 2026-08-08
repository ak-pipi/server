// HongZhongMahjongRule.h

#ifndef _NIU_MA_HONGZHONG_MAHJONG_RULE_H_
#define _NIU_MA_HONGZHONG_MAHJONG_RULE_H_

#include "MahjongRule.h"
#include "MahjongChapter.h"

namespace NiuMa
{
	class HongZhongMahjongRule : public MahjongRule
	{
	public:
		struct HuAnalysis
		{
			bool hu;
			int style;
			int hongZhongCount;

			HuAnalysis()
				: hu(false)
				, style(0)
				, hongZhongCount(0)
			{}
		};

	public:
		HongZhongMahjongRule();
		virtual ~HongZhongMahjongRule();

	public:
		virtual bool hasZiPai() const override;
		virtual void checkTingPai(const MahjongTileArray& tiles, const MahjongTile::TileArray& allGotTiles,
			MahjongGenre::TingPaiArray& tps, MahjongAvatar* pAvatar) const override;

	public:
		static bool isHongZhong(const MahjongTile::Tile& tile);
		static bool isHongZhong(const MahjongTile& tile);
		static void getHongZhongTile(MahjongTile& tile);
		static int countHongZhong(const MahjongTileArray& tiles);
		static int countHongZhong(const MahjongChapterArray& chapters);
		static HuAnalysis analyzeHu(const MahjongTileArray& handTiles, const MahjongChapterArray& chapters);
		static HuAnalysis analyzeHu(const MahjongTileArray& handTiles, const MahjongChapterArray& chapters, int fixedHongZhongCount);

	private:
		static bool isNumberTile(const MahjongTile& tile);
		static bool canPingHu(const MahjongTileArray& tiles, int fixedHongZhongCount = 0);
		static bool canQiXiaoDui(const MahjongTileArray& tiles, int fixedHongZhongCount = 0);
		static bool canPengPengHu(const MahjongTileArray& tiles, int fixedHongZhongCount = 0);
		static bool canFormMelds(int counts[3][9], int wildLeft);
		static bool canFormMeldsWithFixedHongZhong(int counts[3][9], int wildLeft, int fixedHongZhongCount);
		static bool canFormKeZiOnly(const int counts[3][9], int wildLeft);
		static bool canFormKeZiOnlyWithFixedHongZhong(const int counts[3][9], int wildLeft, int fixedHongZhongCount);
		static bool allSameSuit(const MahjongTileArray& handTiles, const MahjongChapterArray& chapters, int fixedHongZhongCount = 0);
		static bool allChaptersKeZi(const MahjongChapterArray& chapters);
	};
}

#endif // !_NIU_MA_HONGZHONG_MAHJONG_RULE_H_
