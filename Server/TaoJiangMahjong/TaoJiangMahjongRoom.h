// TaoJiangMahjongRoom.h

#ifndef _NIU_MA_TAOJIANG_MAHJONG_ROOM_H_
#define _NIU_MA_TAOJIANG_MAHJONG_ROOM_H_

#include "MahjongRoom.h"
#include "../GameDefines.h"
#include "MahjongTile.h"

#include <string>

namespace NiuMa
{
	class TaoJiangMahjongAvatar;

	/**
	 * 桃江麻将游戏房
	 * 地方2人麻将，支持好友房、匹配房、练习房
	 * 玩法配置通过ruleConfig JSON动态加载
	 */
	class TaoJiangMahjongRoom : public MahjongRoom
	{
	public:
		TaoJiangMahjongRoom(const std::string& venueId, const std::string& number, int level, const std::string& ruleConfig, int districtId = 0);
		virtual ~TaoJiangMahjongRoom();

	public:
		virtual bool onMessage(const NetMessage::Ptr& netMsg) override;
		virtual void onTimer() override;

	// 重写
	protected:
		virtual GameAvatar::Ptr createAvatar(const std::string& playerId, int seat, bool robot) const override;
		virtual bool checkEnter(const std::string& playerId, std::string& errMsg, bool robot = false) const override;
		virtual int checkLeave(const std::string& playerId, std::string& errMsg) const override;
		virtual void getAvatarExtraInfo(const GameAvatar::Ptr& avatar, std::string& base64) const override;
		virtual void onAvatarJoined(int seat, const std::string& playerId) override;
		virtual void onAvatarLeaved(int seat, const std::string& playerId) override;
		virtual void clean() override;
		virtual double* getDistances() override;
		virtual void getDistances(std::vector<int>& distances) const override;
		virtual int getDistanceIndex(int seat1, int seat2) const override;
		virtual void calcHuScore() const override;
		virtual void doJieSuan() override;
		virtual void afterHu() override;

		/**
		 * 重写杠动作：桃江麻将杠后翻3张牌
		 */
		virtual bool executeGang() override;

		/**
		 * 重写放弃动作选项：桃江麻将杠后流程
		 */
		virtual void passActionOption(const std::string& playerId) override;

		/**
		 * 重写摸牌/吃/碰后出杠：桃江麻将实时判断杠后是否仍听牌。
		 */
		virtual void afterFetchChiPeng(MahjongAvatar* pAvatar, int fetchedId = -1) override;
		virtual bool canShuffleCardsBeforeNextRound(const std::string& playerId,
			int& nextRoundNo,
			int& roundCount,
			std::string& errMsg) const override;

	private:
		/**
		 * 解析玩法配置JSON
		 */
		void parseRuleConfig(const std::string& ruleConfig);

		/**
		 * 玩家请求同步游戏数据
		 */
		void onSyncMahjong(const NetMessage::Ptr& netMsg);

		/**
		 * 玩家就绪
		 */
		void onPlayerReady(const NetMessage::Ptr& netMsg);

		/**
		 * 请求解散游戏
		 */
		void onDisbandRequest(const NetMessage::Ptr& netMsg);

		/**
		 * 玩家解散选择消息
		 */
		void onDisbandChoose(const NetMessage::Ptr& netMsg);

		/**
		 * 开始新一局
		 */
		void startRound();

		/**
		 * 玩家请求报听
		 */
		void onBaoTing(const NetMessage::Ptr& netMsg);

		/**
		 * 开局确定赖子（骰子翻出明子，明子点数+1同花色为赖子）
		 */
		void determineLaiZi();

		/**
		 * 检测某张牌是否为赖子
		 */
		bool isLaiZi(const MahjongTile::Tile& tile) const;

		/**
		 * 检测某张牌是否为明子（骰子翻出来的那张牌）
		 */
		bool isLaiZiOriginal(const MahjongTile::Tile& tile) const;

		/**
		 * 重写发牌，在发牌前确定赖子
		 */
		virtual void dealTiles() override;

		/**
		 * 重写摸牌，补充赖子万能牌的直接胡牌兜底检测
		 */
		virtual bool fetchTile(bool bBack = false) override;

		/**
		 * 重写点炮判断：桃江麻将平胡不能抓炮，只有大胡才能吃炮
		 */
		virtual bool shouldAllowDianPaoForAvatar(MahjongAvatar* pAvatar, const MahjongTile& mt) const override;

		/**
		 * 重写点炮动作创建：硬庄平胡允许抓炮，需绕过通用 canHu 缓存的旧口径。
		 */
		virtual bool canCreateDianPaoOption(MahjongAvatar* pAvatar, const MahjongTile& mt, std::string& passed) const override;

		/**
		 * 重写直杠动作创建：必须已听，且开杠四张牌移出后听口不变。
		 */
		virtual bool canCreateZhiGangOption(MahjongAvatar* pAvatar, const MahjongTile& mt) const override;

		/**
		 * 重写胡牌检测，添加天胡/天天胡/地胡检测（桃江麻将特有规则）
		 */
		virtual void doHu() override;

		/**
		 * 统计玩家手牌中明子牌的数量（用于天胡/天天胡/地胡检测）
		 */
		int countMingZiInHand(MahjongAvatar* pAvatar) const;

		/**
		 * 统计玩家当前手牌中赖子牌的数量
		 */
		int countLaiZiInHand(MahjongAvatar* pAvatar) const;

		/**
		 * 判断胡牌是否为硬庄：没有赖子，或赖子均按本身牌面自然成胡且满足硬庄基础将牌约束
		 */
		bool isYingZhuangHu(TaoJiangMahjongAvatar* avatar, const MahjongTile& huTile, bool zimo) const;

		/**
		 * 黑天胡：首轮自摸、无刻子、无自然顺子、无任意2/5/8、无赖子
		 */
		bool isHeiTianHu(TaoJiangMahjongAvatar* avatar) const;

		/**
		 * 统计胡牌时赖子数量：手牌 + 当前胡牌牌，牌章不计入。
		 */
		int countLaiZiForHu(TaoJiangMahjongAvatar* avatar, const MahjongTile& huTile, bool includeHuTile) const;

		/**
		 * 统计胡牌时明子数量：仅统计真实手牌；自摸/杠上花的胡牌张计入，抓炮张不计入。
		 */
		int countMingZiForHu(TaoJiangMahjongAvatar* avatar, const MahjongTile& huTile, bool includeHuTile) const;

		/**
		 * 校验桃江胡牌时真实手牌张数：自摸/杠上花为 3n+2，抓炮为 3n+1。
		 */
		bool isValidHuHandTileCount(TaoJiangMahjongAvatar* avatar, const MahjongTile& huTile, bool zimo) const;

		/**
		 * 判断当前真实手牌加胡牌张是否能构成常规胡牌结构；抓炮张只参与胡牌结构，不参与特殊牌数量。
		 */
		bool canFormRegularHuForSpecial(TaoJiangMahjongAvatar* avatar, const MahjongTile& huTile, bool zimo) const;

		/**
		 * 按桃江规则估算某次胡牌是否具备可点炮的大胡
		 */
		bool hasDianPaoDaHu(TaoJiangMahjongAvatar* avatar, const MahjongTile& mt) const;
		bool shouldAllowDianPaoForAvatar(MahjongAvatar* pAvatar, const MahjongTile& mt, bool huTileAsWildcard) const;

		/**
		 * 按当前手牌实时判断某张候选牌是否可胡，并补齐听牌缓存
		 */
		bool canHuWithCandidate(MahjongAvatar* avatar, const MahjongTile& mt, bool tileAlreadyInHand,
			bool huTileAsWildcard = true) const;

		/**
		 * 确保候选胡牌进入听牌缓存，避免多轮换听后动作生成依赖旧缓存
		 */
		void ensureTingTile(MahjongAvatar* avatar, const MahjongTile& mt, MahjongGenre::HuStyle style) const;
		void ensureTingTile(MahjongAvatar* avatar, const MahjongTile& mt, int style) const;

		/**
		 * 根据大胡数量、胡牌方式和硬庄计算单份胡分（不含台桌分，结算时统一乘）
		 */
		int calcBaseHuScore(int daHuCount, bool zimo, bool yingZhuang) const;

		void addTaoJiangSpecialHuWays(TaoJiangMahjongAvatar* avatar, const MahjongTile& huTile, bool zimo, bool hasRegularHu) const;

		/**
		 * 抢杠胡按开杠者可杠上花的分数计分
		 */
		int calcQiangGangHuScore(const MahjongTile& huTile) const;

		/**
		 * 开杠后必须仍然听牌；报听后开杠还必须保持报听听口不变。
		 */
		bool shouldKeepTingForGang(TaoJiangMahjongAvatar* avatar, const MahjongTile& gangTile, MahjongAction::Type gangType) const;

		/**
		 * 比较两组听牌牌面是否一致
		 */
		bool sameTingTiles(const MahjongGenre::TingPaiArray& a, const MahjongGenre::TingPaiArray& b) const;

		/**
		 * 明杠后先检查杠牌本身能否被抢；暗杠不检查。
		 */
		bool addImmediateQiangGangOptions(MahjongAvatar* gangPlayer,
			MahjongAction::Type gangType,
			const MahjongTile& gangTile);

		/**
		 * 杠后翻3张牌处理：开杠者优先胡，否则对手可胡翻牌，都不胡则进弃牌池
		 */
		void processGangReveal(MahjongAvatar* gangPlayer);

		/**
		 * 为对手添加抢杠/抢翻牌胡动作
		 */
		bool addQiangGangOptions(MahjongAvatar* gangPlayer,
			const MahjongTileArray& gangTileCandidates,
			const MahjongTileArray& revealedCandidates);

		/**
		 * 杠后无人胡时，翻牌进入弃牌池并切到下家摸牌；开杠者进入锁定状态。
		 */
		void finishGangRevealWithoutHu(MahjongAvatar* gangPlayer);

		/**
		 * 通知杠翻出的3张牌
		 */
		void notifyGangReveal(MahjongAvatar* gangPlayer, const MahjongTile& mt1, const MahjongTile& mt2, const MahjongTile& mt3);

		/**
		 * 通知发起解散投票
		 * @param playerId 通知指定玩家，为空则通知全部
		 */
		void notifyDisbandVote(const std::string& playerId);

		/**
		 * 玩家做解散选择
		 * @param seat 玩家座位号
		 * @param choice 解散选择，1-同意、2-反对
		 */
		void doDisbandChoose(int seat, int choice);

		/**
		 * 解散游戏
		 */
		void disbandRoom();

		/**
		 * 取消解散
		 */
		void disbandObsolete();

		/**
		 * 保存一局游戏记录
		 */
		void saveRoundRecord();

		void publishFinalRoomFee();

		/**
		 * 获取区域ID
		 * @return 区域ID，0表示好友房
		 */
		int getDistrictId() const;

	private:
		/**
		 * 房间编号，用于手动输入进入房间
		 */
		const std::string _number;

		/**
		 * 等级
		 */
		const int _level;

		/**
		 * 区域ID（0表示好友房，不参与区域匹配）
		 */
		const int _districtId;

		/**
		 * 牌局状态
		 */
		StageState _roundState;

		/**
		 * 解散状态
		 */
		StageState _disbandState;

		/**
		 * 局号数，每局递增
		 */
		int _roundNo;

		/**
		 * 备份本局的庄家位置(在保存牌局记录时使用)
		 */
		int _backupBanker;

		/**
		 * 解散房间的玩家索引
		 */
		int _disbander;

		/**
		 * 房间进入投票解散状态的时间
		 */
		time_t _disbandTick;

		/**
		 * 解散投票，0-未选择、1-同意、2-反对
		 */
		int _disbandChoices[4];

		/**
		 * 玩家之间的距离
		 */
		double _distances[6];

		/**
		 * 本局结束后被踢出房间的玩家
		 */
		bool _kicks[4];

		// ---- 以下为玩法配置项(从ruleConfig JSON解析而来) ----

		/**
		 * 底注
		 */
		int _diZhu;

		/**
		 * 封顶分数，0表示不封顶
		 */
		int _maxScore;

		/**
		 * 整场总房费金额
		 */
		int64_t _roomFee;

		/**
		 * 总局数
		 */
		int _roundCount;

		/**
		 * 是否允许吃牌
		 */
		bool _allowChi;

		/**
		 * 是否允许碰牌
		 */
		bool _allowPeng;

		/**
		 * 是否允许杠牌
		 */
		bool _allowGang;

		/**
		 * 是否允许自摸
		 */
		bool _allowZiMo;

		/**
		 * 是否允许点炮
		 */
		bool _allowDianPao;

		/**
		 * 是否启用赖子
		 */
		bool _laiziEnabled;

		/**
		 * 是否启用红中
		 */
		bool _hongzhongEnabled;

		/**
		 * 庄家规则，0-轮庄，1-赢家坐庄，2-输家坐庄
		 */
		int _bankerRule;

		/**
		 * 是否允许投票解散
		 */
		bool _dissolveVote;

		/**
		 * 整场累计净输赢，用于房费承担判断
		 */
		int64_t _totalWinGolds[4];

		// ---- 赖子系统 ----

		/**
		 * 明子（骰子翻出来的那张牌，本身不是赖子）
		 */
		MahjongTile::Tile _laiZiOriginal;

		/**
		 * 赖子（明子点数+1的同花色牌，可做万能牌使用）
		 */
		MahjongTile::Tile _laiZi;

		/**
		 * 骰子点数
		 */
		int _dicePoint;

		// ---- 报听系统 ----

		/**
		 * 各玩家是否已报听
		 */
		bool _baoTinged[4];

		/**
		 * 报听是否可用（true=开启报听功能）
		 */
		bool _baoTingEnabled;

		// ---- 杠后翻牌系统 ----

		/**
		 * 杠后翻出的3张牌
		 */
		MahjongTile _gangRevealedTiles[3];

		/**
		 * 杠后翻出的牌数量（0~3）
		 */
		int _gangRevealedCount;

		/**
		 * 当前杠操作的类型（用于passActionOption判断）
		 */
		MahjongAction::Type _lastGangType;

		/**
		 * 当前杠操作的牌id（用于明杠本身牌可抢）
		 */
		int _lastGangTileId;

	};
}

#endif // !_NIU_MA_TAOJIANG_MAHJONG_ROOM_H_
