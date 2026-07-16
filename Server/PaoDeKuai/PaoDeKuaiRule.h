// PaoDeKuaiRule.h
// 跑得快游戏规则

#ifndef _NIU_MA_PAODEKUAI_RULE_H_
#define _NIU_MA_PAODEKUAI_RULE_H_

#include "PokerRule.h"

namespace NiuMa
{
	/**
	 * 跑得快牌型定义
	 */
	enum class PaoDeKuaiGenre : int
	{
		Invalid = 0,		// 无效
		Single = 1,			// 单张
		Pair = 2,			// 对子
		Triple = 3,			// 三条
		TripleOne = 4,		// 三带一
		TriplePair = 5,		// 三带二
		Straight = 6,		// 顺子（5+张连续）
		StraightPair = 7,	// 连对（2+对连续）
		Plane = 8,			// 飞机不带
		PlaneOne = 9,		// 飞机带单
		PlanePair = 10,		// 飞机带对
		Bomb = 11,			// 炸弹
		Rocket = 12			// 王炸（双王）
	};

	/**
	 * 跑得快规则类
	 * 2人跑得快：一副牌（去掉大小王、3张2、3张A、1张K），每人15张
	 * 牌值大小：2 > A > K > Q > J > 10 > 9 > 8 > 7 > 6 > 5 > 4 > 3
	 * 跑得快中2为最大牌，A为第二大，不按花色压牌
	 */
	class PaoDeKuaiRule : public PokerRule
	{
	public:
		PaoDeKuaiRule();
		virtual ~PaoDeKuaiRule();

	public:
		virtual void initialise() override;
		virtual int getPackNums() const override;
		virtual int getPointNums() const override;
		virtual void sortPointOrder() override;
		virtual void sortSuitOrder() override;
		virtual bool isDisapprovedCard(const PokerCard& c) const override;
		virtual int predicateCardGenre(PokerGenre& pcg) const override;
		virtual int getGenreCardNums(int genre) const override;
		virtual bool isValidGenre(int genre) const override;
		virtual int compareGenre(const PokerGenre& pcg1, const PokerGenre& pcg2) const override;
		virtual bool straightExcluded(const PokerCard& c) const override;
		virtual bool straightPairExcluded(const PokerCard& c) const override;
		virtual bool straightTripleExcluded(const PokerCard& c) const override;
		virtual bool butterflyExcluded(const PokerCard& c) const override;
		virtual bool straight(const CardArray& cards) const override;
		virtual bool straightPair(const CardArray& cards) const override;

	public:
		// 获取每人的发牌数
		int getCardCount() const { return _cardCount; }

		// 获取玩家数
		int getPlayerCount() const { return _playerCount; }

		// 获取底注
		int getBaseScore() const { return _baseScore; }

		// 获取总局数，0表示不限局数
		int getRoundCount() const { return _roundCount; }

		// 炸弹是否翻倍
		bool getBombDouble() const { return _bombDouble; }

		// 炸弹额外分数
		int getBombScore() const { return _bombScore; }

		// 是否允许过牌
		bool getAllowPass() const { return _allowPass; }

		// 自动出牌超时（毫秒）
		int getAutoPlayTimeout() const { return _autoPlayTimeout; }

		// 封顶分数
		int getMaxRoundScore() const { return _maxRoundScore; }

		// 兼容旧首出配置字段；当前固定为false
		bool getMustIncludeSpade3() const { return _mustIncludeSpade3; }

		// 输家一张未出是否翻倍
		bool getSpringDouble() const { return _springDouble; }

		// 有牌能大过上家时是否禁止过牌
		bool getForcePlayIfCanBeat() const { return _forcePlayIfCanBeat; }

		// 从JSON配置中加载
		void loadConfig(const std::string& ruleConfig);

	private:
		// 每人发牌数
		int _cardCount;

		// 玩家数
		int _playerCount;

		// 底注
		int _baseScore;

		// 总局数，0表示不限局数
		int _roundCount;

		// 炸弹翻倍
		bool _bombDouble;

		// 炸弹额外分数
		int _bombScore;

		// 允许过牌
		bool _allowPass;

		// 自动出牌超时（毫秒）
		int _autoPlayTimeout;

		// 封顶分数
		int _maxRoundScore;

		// 兼容旧配置字段；当前固定为false
		bool _mustIncludeSpade3;

		// 输家一张未出翻倍
		bool _springDouble;

		// 有大必出
		bool _forcePlayIfCanBeat;
	};
}

#endif // _NIU_MA_PAODEKUAI_RULE_H_
