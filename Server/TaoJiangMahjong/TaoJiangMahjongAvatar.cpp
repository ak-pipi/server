// TaoJiangMahjongAvatar.cpp

#include "TaoJiangMahjongAvatar.h"

namespace NiuMa
{
	TaoJiangMahjongAvatar::TaoJiangMahjongAvatar(const std::string& playerId, int seat, bool bRobot)
		: MahjongAvatar(playerId, seat, bRobot)
		, _winGold(0.0)
	{
		for (int i = 0; i < 4; i++)
			_loseScores[i] = 0;
	}

	TaoJiangMahjongAvatar::~TaoJiangMahjongAvatar() {}

	void TaoJiangMahjongAvatar::clear() {
		MahjongAvatar::clear();

		for (int i = 0; i < 4; i++)
			_loseScores[i] = 0;
		_winGold = 0.0;
	}

	int TaoJiangMahjongAvatar::calcHuScore() const {
		int score = 0;
		// 桃江麻将2人玩法，算分规则

		// 算平胡分
		bool pingHu = true;
		if ((_huStyle & static_cast<int>(MahjongGenre::HuStyle::PengPengHu)) == static_cast<int>(MahjongGenre::HuStyle::PengPengHu)) {
			// 碰碰胡
			score = 2;
		}
		else if ((_huStyle & static_cast<int>(MahjongGenre::HuStyle::DanDiao)) == static_cast<int>(MahjongGenre::HuStyle::DanDiao)) {
			// 单吊
			score = 2;
		}
		else if ((_huStyle & static_cast<int>(MahjongGenre::HuStyle::BianZhang)) == static_cast<int>(MahjongGenre::HuStyle::BianZhang))
			score = 2;
		else if ((_huStyle & static_cast<int>(MahjongGenre::HuStyle::KaZhang)) == static_cast<int>(MahjongGenre::HuStyle::KaZhang))
			score = 2;
		else if ((_huStyle & static_cast<int>(MahjongGenre::HuStyle::PingHu)) == static_cast<int>(MahjongGenre::HuStyle::PingHu))
			score = 1;
		else
			pingHu = false;

		// 算七小对分
		if ((_huStyle & static_cast<int>(MahjongGenre::HuStyle::QiXiaoDui3)) == static_cast<int>(MahjongGenre::HuStyle::QiXiaoDui3))
			score = 16;
		else if ((_huStyle & static_cast<int>(MahjongGenre::HuStyle::QiXiaoDui2)) == static_cast<int>(MahjongGenre::HuStyle::QiXiaoDui2))
			score = 12;
		else if ((_huStyle & static_cast<int>(MahjongGenre::HuStyle::QiXiaoDui1)) == static_cast<int>(MahjongGenre::HuStyle::QiXiaoDui1))
			score = 8;
		else if ((_huStyle & static_cast<int>(MahjongGenre::HuStyle::QiXiaoDui)) == static_cast<int>(MahjongGenre::HuStyle::QiXiaoDui))
			score = 4;

		// 门清翻2倍
		bool menQing = false;
		if ((_huWay & static_cast<int>(MahjongGenre::HuWay::MenQing)) == static_cast<int>(MahjongGenre::HuWay::MenQing)) {
			if (((_huWay & static_cast<int>(MahjongGenre::HuWay::TianHu)) != static_cast<int>(MahjongGenre::HuWay::TianHu)) &&
				((_huWay & static_cast<int>(MahjongGenre::HuWay::DiHu)) != static_cast<int>(MahjongGenre::HuWay::DiHu)) && pingHu)
				menQing = true;
		}
		if (menQing)
			score *= 2;

		// 清一色翻2倍
		if ((_huStyle & static_cast<int>(MahjongGenre::HuStyle::QingYiSe)) == static_cast<int>(MahjongGenre::HuStyle::QingYiSe))
			score *= 2;

		// 杠上花翻倍
		if ((_huWay & static_cast<int>(MahjongGenre::HuWay::GangShangHua1)) == static_cast<int>(MahjongGenre::HuWay::GangShangHua1))
			score *= 2;
		// 杠上炮翻倍
		if ((_huWay & static_cast<int>(MahjongGenre::HuWay::GangShangPao1)) == static_cast<int>(MahjongGenre::HuWay::GangShangPao1))
			score *= 2;
		// 抢杠胡翻倍
		if ((_huWay & static_cast<int>(MahjongGenre::HuWay::QiangGangHu1)) == static_cast<int>(MahjongGenre::HuWay::QiangGangHu1))
			score *= 2;
		// 海底捞月翻2倍
		if ((_huWay & static_cast<int>(MahjongGenre::HuWay::HaiDiLaoYue)) == static_cast<int>(MahjongGenre::HuWay::HaiDiLaoYue))
			score *= 2;
		// 海底炮翻2倍
		if ((_huWay & static_cast<int>(MahjongGenre::HuWay::HaiDiPao)) == static_cast<int>(MahjongGenre::HuWay::HaiDiPao))
			score *= 2;
		// 天胡翻10倍
		if ((_huWay & static_cast<int>(MahjongGenre::HuWay::TianHu)) == static_cast<int>(MahjongGenre::HuWay::TianHu))
			score *= 10;
		// 地胡翻5倍
		if ((_huWay & static_cast<int>(MahjongGenre::HuWay::DiHu)) == static_cast<int>(MahjongGenre::HuWay::DiHu))
			score *= 5;

		return score;
	}

	void TaoJiangMahjongAvatar::addLoseScore(int seat, int s) {
		if (seat < 0 || seat > 3)
			return;
		_loseScores[seat] += s;
	}

	void TaoJiangMahjongAvatar::getLoseScores(int loseScores[4]) const {
		for (int i = 0; i < 4; i++)
			loseScores[i] = _loseScores[i];
	}

	void TaoJiangMahjongAvatar::setWinGold(double g) {
		_winGold = g;
	}

	double TaoJiangMahjongAvatar::getWinGold() const {
		return _winGold;
	}
}
