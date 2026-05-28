// ChangShaMahjongRoomHandler.h
// 长沙麻将游戏房间消息处理器

#ifndef _NIU_MA_CHANGSHA_MAHJONG_ROOM_HANDLER_H_
#define _NIU_MA_CHANGSHA_MAHJONG_ROOM_HANDLER_H_

#include "Mahjong/MahjongRoomHandler.h"

namespace NiuMa
{
	class ChangShaMahjongRoomHandler : public MahjongRoomHandler
	{
	public:
		ChangShaMahjongRoomHandler(const MessageQueue::Ptr& queue = nullptr);
		virtual ~ChangShaMahjongRoomHandler();
	};
}

#endif // _NIU_MA_CHANGSHA_MAHJONG_ROOM_HANDLER_H_
