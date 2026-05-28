// HongZhongMahjongRoomHandler.h

#ifndef _NIU_MA_HONGZHONG_MAHJONG_ROOM_HANDLER_H_
#define _NIU_MA_HONGZHONG_MAHJONG_ROOM_HANDLER_H_

#include "MahjongRoomHandler.h"

namespace NiuMa
{
	class HongZhongMahjongRoomHandler : public MahjongRoomHandler
	{
	public:
		HongZhongMahjongRoomHandler(const MessageQueue::Ptr& queue = nullptr);
		virtual ~HongZhongMahjongRoomHandler();
	};
}

#endif // !_NIU_MA_HONGZHONG_MAHJONG_ROOM_HANDLER_H_
