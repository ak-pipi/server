// TaoJiangMahjongAvatar.h

#ifndef _NIU_MA_TAOJIANG_MAHJONG_AVATAR_H_
#define _NIU_MA_TAOJIANG_MAHJONG_AVATAR_H_

#include "MahjongAvatar.h"

#include <string>

namespace NiuMa
{
	class TaoJiangMahjongAvatar : public MahjongAvatar {
	public:
		TaoJiangMahjongAvatar(const std::string& playerId, int seat, bool bRobot);
		virtual ~TaoJiangMahjongAvatar();

	public:
		virtual void clear() override;
		virtual int calcHuScore() const override;
		virtual bool detectHuStyle(bool bZiMo, const MahjongTile& mt) override;

	public:
		void addLoseScore(int seat, int s);
		void getLoseScores(int loseScores[4]) const;
		void setWinGold(double g);
		double getWinGold() const;

		/**
		 * 设置赖子牌（万能牌）
		 */
		void setLaiZi(const MahjongTile::Tile& tile);
		const MahjongTile::Tile& getLaiZi() const;

	/**
		 * 设置是否已报听
		 */
		void setBaoTinged(bool b);
		bool isBaoTinged() const;

		/**
		 * 设置是否为硬庄（胡牌时所有赖子均作为本身牌使用，未做万能牌）
		 */
		void setYingZhuang(bool b);
		bool isYingZhuang() const;

		/**
		 * 添加胡牌样式标记（供Room调用）
		 */
		void addHuStyle(MahjongGenre::HuStyle eStyle);

		/**
		 * 获取大胡数量（用于结算倍率计算）
		 */
		int getDaHuCount() const;

		/**
		 * 获取胡牌类型名称（用于回放和展示）
		 */
		std::string getHuTypeName() const;

	private:
		/**
		 * 一局中玩家需要赔付给其他玩家的赔分
		 */
		int _loseScores[4];

		/**
		 * 一局结算之后玩家赢得(或输)的金币数量
		 */
		double _winGold;

		/**
		 * 是否已报听
		 */
		bool _baoTinged;

		/**
		 * 是否为硬庄（胡牌时所有赖子均作为本身牌使用，未做万能牌）
		 */
		bool _yingZhuang;

		/**
		 * 赖子牌（万能牌），用于听牌/胡牌检测时的替代逻辑
		 */
		MahjongTile::Tile _laiZi;
	};
}

#endif // !_NIU_MA_TAOJIANG_MAHJONG_AVATAR_H_
