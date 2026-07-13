// HongZhongMahjongAvatar.cpp

#include "HongZhongMahjongAvatar.h"
#include "HongZhongMahjongRule.h"

#include <algorithm>

namespace NiuMa
{
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
		const int qiXiaoDui = static_cast<int>(MahjongGenre::HuStyle::QiXiaoDui);
		const int pengPengHu = static_cast<int>(MahjongGenre::HuStyle::PengPengHu);
		const int qingYiSe = static_cast<int>(MahjongGenre::HuStyle::QingYiSe);
		bool bigHu = ((_huStyle & qiXiaoDui) == qiXiaoDui) ||
			((_huStyle & pengPengHu) == pengPengHu) ||
			((_huStyle & qingYiSe) == qingYiSe);
		int score = bigHu ? 5 : 4;
		score *= std::max(1, _birdMultiplier);
		if (_huHongZhongCount == 0)
			score *= 2;
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
		HongZhongMahjongRule::HuAnalysis analysis = HongZhongMahjongRule::analyzeHu(tiles, _chapters);
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
