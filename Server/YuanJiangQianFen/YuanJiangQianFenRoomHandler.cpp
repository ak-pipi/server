// YuanJiangQianFenRoomHandler.cpp
#include "YuanJiangQianFenRoomHandler.h"
#include "../GameDefines.h"
namespace NiuMa {
	YuanJiangQianFenRoomHandler::YuanJiangQianFenRoomHandler(const MessageQueue::Ptr& queue)
		: GameRoomHandler(queue)
	{
		addGameType(static_cast<int>(GameType::YuanJiangQianFen));
	}
	YuanJiangQianFenRoomHandler::~YuanJiangQianFenRoomHandler() {}
}
