// PaoDeKuaiRoomHandler.cpp

#include "PaoDeKuaiRoomHandler.h"
#include "../GameDefines.h"

namespace NiuMa
{
	PaoDeKuaiRoomHandler::PaoDeKuaiRoomHandler(const MessageQueue::Ptr& queue)
		: GameRoomHandler(queue)
	{
		addGameType(static_cast<int>(GameType::PaoDeKuai));
	}

	PaoDeKuaiRoomHandler::~PaoDeKuaiRoomHandler() {}
}
