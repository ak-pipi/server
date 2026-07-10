// DouDiZhuRoomHandler.h

#ifndef _NIU_MA_DOU_DI_ZHU_ROOM_HANDLER_H_
#define _NIU_MA_DOU_DI_ZHU_ROOM_HANDLER_H_

#include "Game/GameRoomHandler.h"

namespace NiuMa
{
	class DouDiZhuRoomHandler : public GameRoomHandler
	{
	public:
		DouDiZhuRoomHandler(const MessageQueue::Ptr& queue = nullptr);
		virtual ~DouDiZhuRoomHandler();
	};
}

#endif
