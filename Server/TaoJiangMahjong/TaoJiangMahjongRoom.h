// TaoJiangMahjongRoom.h

#ifndef _NIU_MA_TAOJIANG_MAHJONG_ROOM_H_
#define _NIU_MA_TAOJIANG_MAHJONG_ROOM_H_

#include "MahjongRoom.h"
#include "../GameDefines.h"
#include "MahjongTile.h"

#include <string>

namespace NiuMa
{
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
		 * 重写摸牌，在摸牌后更新听牌提示
		 */
		virtual bool fetchTile(bool bBack = false) override;

		/**
		 * 重写点炮判断：桃江麻将平胡不能抓炮，只有大胡才能吃炮
		 */
		virtual bool shouldAllowDianPaoForAvatar(MahjongAvatar* pAvatar, const MahjongTile& mt) const override;

		/**
		 * 重写胡牌检测，添加天天胡和地胡检测（桃江麻将特有规则）
		 */
		virtual void doHu() override;

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
		 * 房费类型，0-房主付费，1-AA付费
		 */
		int _roomFeeType;

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
	};
}

#endif // !_NIU_MA_TAOJIANG_MAHJONG_ROOM_H_
