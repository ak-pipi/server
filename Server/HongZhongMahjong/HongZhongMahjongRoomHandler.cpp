// HongZhongMahjongRoomHandler.cpp

#include "HongZhongMahjongRoomHandler.h"
#include "HongZhongMahjongMessages.h"
#include "../GameDefines.h"

namespace NiuMa
{
	HongZhongMahjongRoomHandler::HongZhongMahjongRoomHandler(const MessageQueue::Ptr& queue)
		: MahjongRoomHandler(queue)
	{
		addMessage(MsgHZSync::TYPE);
		addGameType(static_cast<int>(GameType::HongZhongMahjong));
	}

	HongZhongMahjongRoomHandler::~HongZhongMahjongRoomHandler() {}
}
