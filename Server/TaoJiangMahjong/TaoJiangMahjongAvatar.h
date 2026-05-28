// TaoJiangMahjongAvatar.h

#ifndef _NIU_MA_TAOJIANG_MAHJONG_AVATAR_H_
#define _NIU_MA_TAOJIANG_MAHJONG_AVATAR_H_

#include "MahjongAvatar.h"

namespace NiuMa
{
	class TaoJiangMahjongAvatar : public MahjongAvatar {
	public:
		TaoJiangMahjongAvatar(const std::string& playerId, int seat, bool bRobot);
		virtual ~TaoJiangMahjongAvatar();

	public:
		virtual void clear() override;
		virtual int calcHuScore() const override;

	public:
		void addLoseScore(int seat, int s);
		void getLoseScores(int loseScores[4]) const;
		void setWinGold(double g);
		double getWinGold() const;

	private:
		/**
		 * 一局中玩家需要赔付给其他玩家的赔分
		 */
		int _loseScores[4];

		/**
		 * 一局结算之后玩家赢得(或输)的金币数量
		 */
		double _winGold;
	};
}

#endif // !_NIU_MA_TAOJIANG_MAHJONG_AVATAR_H_
