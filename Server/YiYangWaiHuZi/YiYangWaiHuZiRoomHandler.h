// YiYangWaiHuZiRoomHandler.h
#ifndef _NIU_MA_YIYANG_WAIHUZI_ROOM_HANDLER_H_
#define _NIU_MA_YIYANG_WAIHUZI_ROOM_HANDLER_H_

#include "Game/GameRoomHandler.h"

namespace NiuMa
{
	class YiYangWaiHuZiRoomHandler : public GameRoomHandler
	{
	public:
		YiYangWaiHuZiRoomHandler(const MessageQueue::Ptr& queue = nullptr);
		virtual ~YiYangWaiHuZiRoomHandler();
	};
}
#endif
