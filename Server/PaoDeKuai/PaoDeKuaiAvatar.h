// PaoDeKuaiAvatar.h
// 跑得快玩家替身

#ifndef _NIU_MA_PAODEKUAI_AVATAR_H_
#define _NIU_MA_PAODEKUAI_AVATAR_H_

#include "PokerAvatar.h"
#include "PaoDeKuaiRule.h"

namespace NiuMa
{
	class PaoDeKuaiAvatar : public PokerAvatar
	{
	public:
		PaoDeKuaiAvatar(const PokerRule::Ptr& rule, const std::string& playerId, int seat, bool robot);
		virtual ~PaoDeKuaiAvatar();

	public:
		// 获取座位号
		int getSeat() const { return _seat; }

		// 设置/获取当局得分
		void setRoundScore(int score) { _roundScore = score; }
		int getRoundScore() const { return _roundScore; }

		// 设置/获取累计得分
		void setTotalScore(int score) { _totalScore = score; }
		int getTotalScore() const { return _totalScore; }

		// 设置/获取赢的金币
		void setWinGold(int64_t gold) { _winGold = gold; }
		int64_t getWinGold() const { return _winGold; }

		// 是否已出完牌
		bool isFinished() const { return _cards.empty(); }

		// 设置/获取是否准备
		void setReady(bool ready) { _ready = ready; }
		bool isReady() const { return _ready; }

		// 设置/获取是否托管
		void setTrusteeship(bool t) { _trusteeship = t; }
		bool isTrusteeship() const { return _trusteeship; }

		// 清理
		virtual void clear() override;

	protected:
		virtual void combineAllGenres() override;
		virtual void candidateCombinationsImpl(int situation = 0) override;
		virtual void candidateCombinationsImpl(const PokerGenre& pg, int situation = 0) override;

	private:
		/**
		 * 生成指定牌值的单张/对子/三条/炸弹组合
		 * @param order 牌值在顺序表中的位置
		 */
		void combineSinglePair(int order);
		void combineStraight();
		void combineStraightPair();
		void combineTriple();
		void combinePlane();

	private:
		// 当局得分
		int _roundScore;

		// 累计得分
		int _totalScore;

		// 赢的金币
		int64_t _winGold;

		// 是否准备
		bool _ready;

		// 是否托管
		bool _trusteeship;
	};
}

#endif // _NIU_MA_PAODEKUAI_AVATAR_H_
