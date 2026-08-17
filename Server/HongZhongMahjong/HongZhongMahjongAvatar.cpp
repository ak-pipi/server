// HongZhongMahjongAvatar.cpp

#include "HongZhongMahjongAvatar.h"
#include "HongZhongMahjongRule.h"

#include <algorithm>

namespace NiuMa
{
	namespace
	{
		bool hasFlag(int mask, int flag) {
			return (mask & flag) == flag;
		}

		bool hasGangShangHua(int huWay) {
			return hasFlag(huWay, static_cast<int>(MahjongGenre::HuWay::GangShangHua1)) ||
				hasFlag(huWay, static_cast<int>(MahjongGenre::HuWay::GangShangHua2)) ||
				hasFlag(huWay, static_cast<int>(MahjongGenre::HuWay::GangShangHua3)) ||
				hasFlag(huWay, static_cast<int>(MahjongGenre::HuWay::GangShangHua4));
		}

		bool hasHongZhongBonusHu(int huStyle, int huWay) {
			return hasFlag(huStyle, static_cast<int>(MahjongGenre::HuStyle::QiXiaoDui)) ||
				hasFlag(huStyle, static_cast<int>(MahjongGenre::HuStyle::PengPengHu)) ||
				hasGangShangHua(huWay);
		}
	}

	HongZhongMahjongAvatar::HongZhongMahjongAvatar(const std::string& playerId, int seat, bool bRobot)
		: MahjongAvatar(playerId, seat, bRobot)
		, _winGold(0.0)
		, _huHongZhongCount(0)
		, _birdMultiplier(1)
	{
		for (int i = 0; i < 4; i++)
			_loseScores[i] = 0;
	}

	HongZhongMahjongAvatar::~HongZhongMahjongAvatar() {}

	void HongZhongMahjongAvatar::clear() {
		MahjongAvatar::clear();

		for (int i = 0; i < 4; i++)
			_loseScores[i] = 0;
		_winGold = 0.0;
		_huHongZhongCount = 0;
		_birdMultiplier = 1;
	}

	int HongZhongMahjongAvatar::calcHuScore() const {
		const int fixedHuScore = 2;
		const int fixedBaseScore = 2;
		const bool bonusHu = hasHongZhongBonusHu(_huStyle, _huWay);
		const int huScore = fixedHuScore + (bonusHu ? 1 : 0);
		const int birdScore = std::max(1, _birdMultiplier);

		int score = (huScore + birdScore) * fixedBaseScore;
		if (_huHongZhongCount == 0)
			score *= 2;
		if (bonusHu)
			score += 1;
		return score;
	}

	bool HongZhongMahjongAvatar::detectHuStyle(bool bZiMo, const MahjongTile& mt) {
		MahjongTileArray tiles = _handTiles;
		if (!bZiMo)
			tiles.push_back(mt);
		else {
			bool found = false;
			for (const MahjongTile& tile : tiles) {
				if (tile.getId() == mt.getId()) {
					found = true;
					break;
				}
			}
			if (!found)
				tiles.push_back(mt);
		}
		std::sort(tiles.begin(), tiles.end());
		int fixedHongZhongCount = (!bZiMo && HongZhongMahjongRule::isHongZhong(mt)) ? 1 : 0;
		HongZhongMahjongRule::HuAnalysis analysis = HongZhongMahjongRule::analyzeHu(tiles, _chapters, fixedHongZhongCount);
		if (!analysis.hu)
			return false;
		_huStyle = analysis.style;
		_huHongZhongCount = analysis.hongZhongCount;
		return true;
	}

	void HongZhongMahjongAvatar::addLoseScore(int seat, int s) {
		if (seat < 0 || seat > 3)
			return;
		_loseScores[seat] += s;
	}

	void HongZhongMahjongAvatar::getLoseScores(int loseScores[4]) const {
		for (int i = 0; i < 4; i++)
			loseScores[i] = _loseScores[i];
	}

	void HongZhongMahjongAvatar::setWinGold(double g) {
		_winGold = g;
	}

	double HongZhongMahjongAvatar::getWinGold() const {
		return _winGold;
	}

	void HongZhongMahjongAvatar::setHuContext(int style, int hongZhongCount) {
		_huStyle = style;
		_huHongZhongCount = hongZhongCount;
	}

	void HongZhongMahjongAvatar::setBirdMultiplier(int multiplier) {
		_birdMultiplier = std::max(1, multiplier);
	}

	int HongZhongMahjongAvatar::getHuHongZhongCount() const {
		return _huHongZhongCount;
	}

	int HongZhongMahjongAvatar::getBirdMultiplier() const {
		return _birdMultiplier;
	}
}
