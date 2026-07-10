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

		/**
		 * 重写直杠检测：桃江麻将规则，没有听牌时不能直杠
		 */
		virtual bool canZhiGang(const MahjongTile& mt) const override;

		/**
		 * 重写碰牌检测：桃江麻将规则，开杠后不能碰
		 */
		virtual bool canPeng(const MahjongTile& mt, std::string& passed) const override;

		/**
		 * 重写吃牌检测：桃江麻将规则，开杠后不能吃
		 */
		virtual bool canChi(const MahjongTile& mt, std::vector<std::pair<int, int> >& lstPairs) const override;

		/**
		 * 重写胡牌检测：报听后只能胡报听时锁定的牌
		 */
		virtual bool canHu(const MahjongTile& mt) const override;

		/**
		 * 报听后只能打出本轮摸到的牌，不能换听
		 */
		virtual bool playTile(int id) override;

		/**
		 * 重写吃/碰执行：允许赖子参与吃碰组合
		 */
		virtual bool doChi(const MahjongTile& mt, int id1, int id2, int actionId, int player) override;
		virtual bool doPeng(const MahjongTile& mt, int actionId, int player) override;

	public:
		/**
		 * 设置开杠后标志（开杠后不能吃碰杠）
		 */
		void setAfterGang(bool v);
		bool isAfterGang() const;
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
		 * 设置明子牌（骰子翻出的牌）
		 */
		void setMingZi(const MahjongTile::Tile& tile);
		const MahjongTile::Tile& getMingZi() const;

	/**
		 * 设置是否已报听
		 */
		void setBaoTinged(bool b);
		bool isBaoTinged() const;
		const MahjongGenre::TingPaiArray& getBaoTingTiles() const;

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

		/**
		 * 临时牌型计算状态快照/恢复（用于抢杠胡按开杠者杠上花计分）
		 */
		void getHuCalcState(int& style, int& styleEx, int& way, bool& yingZhuang) const;
		void restoreHuCalcState(int style, int styleEx, int way, bool yingZhuang);

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
		 * 报听时锁定的听牌列表
		 */
		MahjongGenre::TingPaiArray _baoTingTiles;

		/**
		 * 开杠后标志，true=不能吃碰杠（只能摸什么打什么）
		 */
		bool _afterGang;

		/**
		 * 是否为硬庄（胡牌时所有赖子均作为本身牌使用，未做万能牌）
		 */
		bool _yingZhuang;

		/**
		 * 赖子牌（万能牌），用于听牌/胡牌检测时的替代逻辑
		 */
		MahjongTile::Tile _laiZi;

		/**
		 * 明子牌，用于地胡检测
		 */
		MahjongTile::Tile _mingZi;
	};
}

#endif // !_NIU_MA_TAOJIANG_MAHJONG_AVATAR_H_
