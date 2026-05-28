// ChangShaMahjongAvatar.h
// 长沙麻将玩家替身

#ifndef _NIU_MA_CHANGSHA_MAHJONG_AVATAR_H_
#define _NIU_MA_CHANGSHA_MAHJONG_AVATAR_H_

#include "MahjongAvatar.h"

namespace NiuMa
{
	class ChangShaMahjongAvatar : public MahjongAvatar {
	public:
		ChangShaMahjongAvatar(const std::string& playerId, int seat, bool bRobot);
		virtual ~ChangShaMahjongAvatar();

	public:
		virtual void clear() override;
		virtual int calcHuScore() const override;

	public:
		void addLoseScore(int seat, int s);
		void getLoseScores(int loseScores[4]) const;
		void setWinGold(double g);
		double getWinGold() const;

		// 设置/获取起手胡类型（0=无，1=缺一色，2=板板胡，3=大四喜，4=六六顺，5=节节高，6=三同，7=一枝花）
		void setQiShouHuType(int type) { _qiShouHuType = type; }
		int getQiShouHuType() const { return _qiShouHuType; }

		// 起手胡得分
		void setQiShouHuScore(int score) { _qiShouHuScore = score; }
		int getQiShouHuScore() const { return _qiShouHuScore; }

		// 中鸟数量
		void setBirdCount(int count) { _birdCount = count; }
		int getBirdCount() const { return _birdCount; }

	private:
		// 一局中玩家需要赔付给其他玩家的赔分
		int _loseScores[4];

		// 一局结算之后玩家赢得(或输)的金币数量
		double _winGold;

		// 起手胡类型
		int _qiShouHuType;

		// 起手胡得分
		int _qiShouHuScore;

		// 中鸟数量
		int _birdCount;
	};
}

#endif // _NIU_MA_CHANGSHA_MAHJONG_AVATAR_H_
