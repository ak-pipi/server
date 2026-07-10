// DouDiZhuAvatar.h

#ifndef _NIU_MA_DOU_DI_ZHU_AVATAR_H_
#define _NIU_MA_DOU_DI_ZHU_AVATAR_H_

#include "PokerAvatar.h"
#include "DouDiZhuGameRule.h"

namespace NiuMa
{
	class DouDiZhuAvatar : public PokerAvatar
	{
	public:
		DouDiZhuAvatar(const PokerRule::Ptr& rule, const std::string& playerId, int seat, bool robot);
		virtual ~DouDiZhuAvatar();

	public:
		void setReady(bool ready) { _ready = ready; }
		bool isReady() const { return _ready; }
		void setLandlord(bool landlord) { _landlord = landlord; }
		bool isLandlord() const { return _landlord; }
		void setRoundScore(int score) { _roundScore = score; }
		int getRoundScore() const { return _roundScore; }
		void setTotalScore(int score) { _totalScore = score; }
		int getTotalScore() const { return _totalScore; }
		void setWinGold(int64_t gold) { _winGold = gold; }
		int64_t getWinGold() const { return _winGold; }
		bool isFinished() const { return _cards.empty(); }
		virtual void clear() override;

	protected:
		virtual void combineAllGenres() override;
		virtual void candidateCombinationsImpl(int situation = 0) override;
		virtual void candidateCombinationsImpl(const PokerGenre& pg, int situation = 0) override;

	private:
		void combineSamePoint(int order);

	private:
		bool _ready;
		bool _landlord;
		int _roundScore;
		int _totalScore;
		int64_t _winGold;
	};
}

#endif
