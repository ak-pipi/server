// ChangShaMahjongAvatar.cpp

#include "ChangShaMahjongAvatar.h"

#include <algorithm>

namespace NiuMa
{
	namespace
	{
		bool hasHuStyle(unsigned int value, MahjongGenre::HuStyle style) {
			unsigned int mask = static_cast<unsigned int>(style);
			return (value & mask) == mask;
		}

		bool hasHuWay(unsigned int value, MahjongGenre::HuWay way) {
			unsigned int mask = static_cast<unsigned int>(way);
			return (value & mask) == mask;
		}

		bool isNumber258(const MahjongTile& mt) {
			int pattern = static_cast<int>(mt.getPattern());
			int number = static_cast<int>(mt.getNumber());
			return pattern >= static_cast<int>(MahjongTile::Pattern::Tong) &&
				pattern <= static_cast<int>(MahjongTile::Pattern::Wan) &&
				(number == 2 || number == 5 || number == 8);
		}
	}

	ChangShaMahjongAvatar::ChangShaMahjongAvatar(const std::string& playerId, int seat, bool bRobot)
		: MahjongAvatar(playerId, seat, bRobot)
		, _winGold(0.0)
		, _qiShouHuType(0)
		, _qiShouHuScore(0)
		, _birdCount(0)
	{
		for (int i = 0; i < 4; i++)
			_loseScores[i] = 0;
	}

	ChangShaMahjongAvatar::~ChangShaMahjongAvatar() {}

	void ChangShaMahjongAvatar::clear() {
		MahjongAvatar::clear();
		_winGold = 0.0;
		_qiShouHuType = 0;
		_qiShouHuScore = 0;
		_birdCount = 0;
		for (int i = 0; i < 4; i++)
			_loseScores[i] = 0;
	}

	bool ChangShaMahjongAvatar::detectHuStyle(bool bZiMo, const MahjongTile& mt) {
		if (!MahjongAvatar::detectHuStyle(bZiMo, mt))
			return false;

		MahjongTileArray tiles;
		tiles.reserve(_handTiles.size() + _chapters.size() * 4 + 1);
		tiles = _handTiles;
		if (!bZiMo)
			tiles.push_back(mt);
		for (const MahjongChapter& chapter : _chapters) {
			const MahjongTileArray& chapterTiles = chapter.getAllTiles();
			tiles.insert(tiles.end(), chapterTiles.begin(), chapterTiles.end());
		}

		if (!tiles.empty() && std::all_of(tiles.begin(), tiles.end(), isNumber258))
			_huStyle |= static_cast<int>(MahjongGenre::HuStyle::JiangJiangHu);
		return true;
	}

	int ChangShaMahjongAvatar::calcHuScore() const {
		int score = isDianPao() ? 1 : 2;
		unsigned int style = getHuStyle();
		unsigned int way = getHuWay();

		if (hasHuStyle(style, MahjongGenre::HuStyle::QiXiaoDui3))
			score *= 16;
		else if (hasHuStyle(style, MahjongGenre::HuStyle::QiXiaoDui2))
			score *= 8;
		else if (hasHuStyle(style, MahjongGenre::HuStyle::QiXiaoDui1))
			score *= 4;
		else if (hasHuStyle(style, MahjongGenre::HuStyle::QiXiaoDui))
			score *= 2;

		if (hasHuStyle(style, MahjongGenre::HuStyle::PengPengHu))
			score *= 2;
		if (hasHuStyle(style, MahjongGenre::HuStyle::JiangJiangHu))
			score *= 2;
		if (hasHuStyle(style, MahjongGenre::HuStyle::QingYiSe))
			score *= 2;

		if (hasHuWay(way, MahjongGenre::HuWay::TianHu) ||
			hasHuWay(way, MahjongGenre::HuWay::DiHu) ||
			hasHuWay(way, MahjongGenre::HuWay::RenHu))
			score *= 4;
		if (hasHuWay(way, MahjongGenre::HuWay::GangShangHua1) ||
			hasHuWay(way, MahjongGenre::HuWay::GangShangHua2) ||
			hasHuWay(way, MahjongGenre::HuWay::GangShangHua3) ||
			hasHuWay(way, MahjongGenre::HuWay::GangShangHua4) ||
			hasHuWay(way, MahjongGenre::HuWay::GangShangPao1) ||
			hasHuWay(way, MahjongGenre::HuWay::GangShangPao2) ||
			hasHuWay(way, MahjongGenre::HuWay::GangShangPao3) ||
			hasHuWay(way, MahjongGenre::HuWay::GangShangPao4) ||
			hasHuWay(way, MahjongGenre::HuWay::QiangGangHu1) ||
			hasHuWay(way, MahjongGenre::HuWay::QiangGangHu2) ||
			hasHuWay(way, MahjongGenre::HuWay::QiangGangHu3) ||
			hasHuWay(way, MahjongGenre::HuWay::QiangGangHu4))
			score *= 2;
		if (hasHuWay(way, MahjongGenre::HuWay::HaiDiLaoYue) ||
			hasHuWay(way, MahjongGenre::HuWay::HaiDiPao))
			score *= 2;

		return score > 0 ? score : 1;
	}

	void ChangShaMahjongAvatar::addLoseScore(int seat, int s) {
		if (seat >= 0 && seat < 4)
			_loseScores[seat] += s;
	}

	void ChangShaMahjongAvatar::getLoseScores(int loseScores[4]) const {
		for (int i = 0; i < 4; i++)
			loseScores[i] = _loseScores[i];
	}

	void ChangShaMahjongAvatar::setWinGold(double g) {
		_winGold = g;
	}

	double ChangShaMahjongAvatar::getWinGold() const {
		return _winGold;
	}
}
