// ChangShaMahjongRoomHandler.cpp

#include "ChangShaMahjongRoomHandler.h"
#include "ChangShaMahjongMessages.h"
#include "../GameDefines.h"

namespace NiuMa
{
	ChangShaMahjongRoomHandler::ChangShaMahjongRoomHandler(const MessageQueue::Ptr& queue)
		: MahjongRoomHandler(queue)
	{
		addMessage(MsgChangShaSync::TYPE);
		addGameType(static_cast<int>(GameType::ChangShaMahjong));
	}

	ChangShaMahjongRoomHandler::~ChangShaMahjongRoomHandler() {}
}
