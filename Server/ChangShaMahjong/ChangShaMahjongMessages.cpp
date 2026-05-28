// ChangShaMahjongMessages.cpp

#include "ChangShaMahjongMessages.h"
#include "Message/MessageManager.h"
#include "Message/MsgCreator.h"

namespace NiuMa
{
	const std::string MsgChangShaSync::TYPE = "ChangSha.Sync";
	const std::string MsgChangShaReady::TYPE = "ChangSha.Ready";
	const std::string MsgChangShaDisband::TYPE = "ChangSha.Disband";
	const std::string MsgChangShaQiShouHu::TYPE = "ChangSha.QiShouHu";
	const std::string MsgChangShaBird::TYPE = "ChangSha.Bird";
	const std::string MsgChangShaDisbandVote::TYPE = "ChangSha.DisbandVote";

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
		IMsgCreator::Ptr creator1 = IMsgCreator::Ptr(new MsgCreator<MsgChangShaSync>());
		MessageManager::getSingleton().registCreator(MsgChangShaSync::TYPE, creator1);
		IMsgCreator::Ptr creator2 = IMsgCreator::Ptr(new MsgCreator<MsgChangShaReady>());
		MessageManager::getSingleton().registCreator(MsgChangShaReady::TYPE, creator2);
		IMsgCreator::Ptr creator3 = IMsgCreator::Ptr(new MsgCreator<MsgChangShaDisband>());
		MessageManager::getSingleton().registCreator(MsgChangShaDisband::TYPE, creator3);
	}
}
