// YiYangWaiHuZiRoomHandler.cpp

#include "YiYangWaiHuZiRoomHandler.h"
#include "../GameDefines.h"

namespace NiuMa
{
	YiYangWaiHuZiRoomHandler::YiYangWaiHuZiRoomHandler(const MessageQueue::Ptr& queue)
		: GameRoomHandler(queue)
	{
		addGameType(static_cast<int>(GameType::YiYangWaiHuZi));
	}
	YiYangWaiHuZiRoomHandler::~YiYangWaiHuZiRoomHandler() {}
}
