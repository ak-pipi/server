// HongZhongMahjongAvatar.cpp

#include "HongZhongMahjongAvatar.h"

namespace NiuMa
{
	HongZhongMahjongAvatar::HongZhongMahjongAvatar(const std::string& playerId, int seat, bool bRobot)
		: MahjongAvatar(playerId, seat, bRobot)
		, _winGold(0.0)
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
	}

	int HongZhongMahjongAvatar::calcHuScore() const {
		int score = 1;	// 基础分

		// 碰碰胡
		if ((_huStyle & static_cast<int>(MahjongGenre::HuStyle::PengPengHu)) == static_cast<int>(MahjongGenre::HuStyle::PengPengHu))
			score = 2;
		// 七小对
		else if ((_huStyle & static_cast<int>(MahjongGenre::HuStyle::QiXiaoDui3)) == static_cast<int>(MahjongGenre::HuStyle::QiXiaoDui3))
			score = 16;
		else if ((_huStyle & static_cast<int>(MahjongGenre::HuStyle::QiXiaoDui2)) == static_cast<int>(MahjongGenre::HuStyle::QiXiaoDui2))
			score = 12;
		else if ((_huStyle & static_cast<int>(MahjongGenre::HuStyle::QiXiaoDui1)) == static_cast<int>(MahjongGenre::HuStyle::QiXiaoDui1))
			score = 8;
		else if ((_huStyle & static_cast<int>(MahjongGenre::HuStyle::QiXiaoDui)) == static_cast<int>(MahjongGenre::HuStyle::QiXiaoDui))
			score = 4;
		// 单吊
		else if ((_huStyle & static_cast<int>(MahjongGenre::HuStyle::DanDiao)) == static_cast<int>(MahjongGenre::HuStyle::DanDiao))
			score = 2;
		// 边张
		else if ((_huStyle & static_cast<int>(MahjongGenre::HuStyle::BianZhang)) == static_cast<int>(MahjongGenre::HuStyle::BianZhang))
			score = 2;
		// 卡张
		else if ((_huStyle & static_cast<int>(MahjongGenre::HuStyle::KaZhang)) == static_cast<int>(MahjongGenre::HuStyle::KaZhang))
			score = 2;
		// 十三幺
		else if ((_huStyle & static_cast<int>(MahjongGenre::HuStyle::ShiSanYao)) == static_cast<int>(MahjongGenre::HuStyle::ShiSanYao))
			score = 8;
		// 平胡
		else if ((_huStyle & static_cast<int>(MahjongGenre::HuStyle::PingHu)) == static_cast<int>(MahjongGenre::HuStyle::PingHu))
			score = 1;

		// 清一色翻2倍
		if ((_huStyle & static_cast<int>(MahjongGenre::HuStyle::QingYiSe)) == static_cast<int>(MahjongGenre::HuStyle::QingYiSe))
			score *= 2;

		// 字一色翻4倍
		if ((_huStyle & static_cast<int>(MahjongGenre::HuStyle::ZiYiSe)) == static_cast<int>(MahjongGenre::HuStyle::ZiYiSe))
			score *= 4;

		// 门清翻2倍
		if ((_huWay & static_cast<int>(MahjongGenre::HuWay::MenQing)) == static_cast<int>(MahjongGenre::HuWay::MenQing))
			score *= 2;

		// 自摸翻2倍(红中麻将自摸翻倍)
		if ((_huWay & static_cast<int>(MahjongGenre::HuWay::ZiMo)) == static_cast<int>(MahjongGenre::HuWay::ZiMo))
			score *= 2;

		// 杠上花翻2倍
		if ((_huWay & static_cast<int>(MahjongGenre::HuWay::GangShangHua1)) == static_cast<int>(MahjongGenre::HuWay::GangShangHua1))
			score *= 2;
		// 杠上炮翻2倍
		if ((_huWay & static_cast<int>(MahjongGenre::HuWay::GangShangPao1)) == static_cast<int>(MahjongGenre::HuWay::GangShangPao1))
			score *= 2;
		// 抢杠胡翻2倍
		if ((_huWay & static_cast<int>(MahjongGenre::HuWay::QiangGangHu1)) == static_cast<int>(MahjongGenre::HuWay::QiangGangHu1))
			score *= 2;
		// 海底捞月翻2倍
		if ((_huWay & static_cast<int>(MahjongGenre::HuWay::HaiDiLaoYue)) == static_cast<int>(MahjongGenre::HuWay::HaiDiLaoYue))
			score *= 2;
		// 海底炮翻2倍
		if ((_huWay & static_cast<int>(MahjongGenre::HuWay::HaiDiPao)) == static_cast<int>(MahjongGenre::HuWay::HaiDiPao))
			score *= 2;
		// 天胡翻4倍
		if ((_huWay & static_cast<int>(MahjongGenre::HuWay::TianHu)) == static_cast<int>(MahjongGenre::HuWay::TianHu))
			score *= 4;
		// 地胡翻4倍
		if ((_huWay & static_cast<int>(MahjongGenre::HuWay::DiHu)) == static_cast<int>(MahjongGenre::HuWay::DiHu))
			score *= 4;

		return score;
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
}
