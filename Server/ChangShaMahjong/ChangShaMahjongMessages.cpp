// ChangShaMahjongMessages.cpp

#include "ChangShaMahjongMessages.h"
#include "Message/MessageManager.h"
#include "Message/MsgCreator.h"

namespace NiuMa
{
	const std::string MsgChangShaSync::TYPE("MsgChangShaSync");
	const std::string MsgChangShaSyncResp::TYPE("MsgChangShaSyncResp");
	const std::string MsgChangShaStartRound::TYPE("MsgChangShaStartRound");
	const std::string MsgChangShaSettlement::TYPE("MsgChangShaSettlement");
	const std::string MsgChangShaQiShouHu::TYPE("MsgChangShaQiShouHu");
	const std::string MsgChangShaBird::TYPE("MsgChangShaBird");
	const std::string MsgChangShaDisbandVote::TYPE("MsgChangShaDisbandVote");

	MsgChangShaSyncResp::MsgChangShaSyncResp()
		: gold(0)
		, diamond(0)
		, diZhu(0)
		, chi(false)
		, dianPao(false)
		, hasFetch(false)
		, seat(0)
		, roundState(0)
		, disbandState(0)
		, banker(0)
		, playerCount(4)
		, roundNo(0)
		, roundCount(0)
		, leftTiles(0)
		, qiShouHuSeat(-1)
		, qiShouHuType(0)
		, qiShouHuScore(0)
		, birdMultiple(1)
	{
		for (int i = 0; i < 4; i++)
			handTileNums[i] = 0;
	}

	MsgChangShaSyncResp::~MsgChangShaSyncResp() {}

	MsgChangShaStartRound::MsgChangShaStartRound()
		: banker(0)
		, playerCount(4)
		, roundNo(0)
		, roundCount(0)
		, birdCount(0)
		, zhongNiaoEnabled(false)
		, require258Jiang(true)
	{}

	MsgChangShaStartRound::~MsgChangShaStartRound() {}

	MsgChangShaSettlement::MsgChangShaSettlement()
		: kick(false)
		, birdMultiple(1)
		, qiShouHuSeat(-1)
		, qiShouHuType(0)
		, qiShouHuScore(0)
		, roomFeeTotal(0)
		, shuffleFeeTotal(0)
	{
		for (int i = 0; i < 4; i++) {
			golds[i] = 0LL;
			winGolds[i] = 0;
		}
	}

	MsgChangShaSettlement::~MsgChangShaSettlement() {}

	MsgChangShaQiShouHu::MsgChangShaQiShouHu()
		: seat(-1)
		, huType(0)
		, score(0)
	{}

	MsgChangShaBird::MsgChangShaBird()
		: multiple(1)
	{}

	MsgChangShaDisbandVote::MsgChangShaDisbandVote()
		: disbander(-1)
		, remainTime(0)
	{
		for (int i = 0; i < 4; i++)
			choices[i] = 0;
	}

	void ChangShaMahjongMessages::registMessages() {
		IMsgCreator::Ptr creator = IMsgCreator::Ptr(new MsgCreator<MsgChangShaSync>());
		MessageManager::getSingleton().registCreator(MsgChangShaSync::TYPE, creator);
	}
}
