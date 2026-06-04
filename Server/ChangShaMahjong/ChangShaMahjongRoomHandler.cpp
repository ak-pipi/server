// ChangShaMahjongRoomHandler.cpp

#include "ChangShaMahjongRoomHandler.h"
#include "../GameDefines.h"

namespace NiuMa
{
	ChangShaMahjongRoomHandler::ChangShaMahjongRoomHandler(const MessageQueue::Ptr& queue)
		: MahjongRoomHandler(queue)
	{
		addGameType(static_cast<int>(GameType::ChangShaMahjong));
	}

	ChangShaMahjongRoomHandler::~ChangShaMahjongRoomHandler() {}
}
