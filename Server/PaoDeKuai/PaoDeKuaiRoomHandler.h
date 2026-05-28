// PaoDeKuaiRoomHandler.h
// 跑得快游戏房间消息处理器

#ifndef _NIU_MA_PAODEKUAI_ROOM_HANDLER_H_
#define _NIU_MA_PAODEKUAI_ROOM_HANDLER_H_

#include "Game/GameRoomHandler.h"

namespace NiuMa
{
	class PaoDeKuaiRoomHandler : public GameRoomHandler
	{
	public:
		PaoDeKuaiRoomHandler(const MessageQueue::Ptr& queue = nullptr);
		virtual ~PaoDeKuaiRoomHandler();
	};
}

#endif // _NIU_MA_PAODEKUAI_ROOM_HANDLER_H_
