// ChangShaMahjongAvatar.cpp

#include "ChangShaMahjongAvatar.h"

namespace NiuMa
{
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

	int ChangShaMahjongAvatar::calcHuScore() const {
		// 基础胡牌分数
		int baseScore = 1;

		// 自摸翻倍
		// 清一色x2
		// 字一色x4
		// 碰碰胡x2
		// 七小对x4
		// 杠上花x2
		// 海底捞月x2
		// 天胡x4
		// 地胡x4
		// 这里返回基础分，具体翻倍在Room中根据牌型计算
		return baseScore;
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
