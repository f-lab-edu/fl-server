#include "pch.h"

#include "PacketManager.h"
#include "UserManager.h"
#include "User.h"

void PacketManager::Init(const UINT32 maxClient_)
{
	mRecvFunctionDictionary = unordered_map<int, PROCESS_RECV_PACKET_FUNCTION>();

	mRecvFunctionDictionary[(int)PACKET_ID::SYS_USER_CONNECT] = &PacketManager::ProcessUserConnect;
	mRecvFunctionDictionary[(int)PACKET_ID::SYS_USER_DISCONNECT] = &PacketManager::ProcessUserDisConnect;

	mRecvFunctionDictionary[(int)PACKET_ID::LOGIN_REQUEST] = &PacketManager::ProcessLogin;

	CreateComponent(maxClient_);
}

bool PacketManager::Run()
{
	mIsRunProcessThread = true;
	mProcessThread = thread([this]() { ProcessPacket(); });

	return true;
}

void PacketManager::End()
{
	mIsRunProcessThread = false;

	if (mProcessThread.joinable())
		mProcessThread.join();
}

void PacketManager::ReceivePacketData(const UINT32 clientIndex_, const UINT32 size_, shared_ptr<char[]> pData_)
{
	auto pUser = mUserManager->GetUserByConnIdx(clientIndex_);
	pUser->SetPacketData(size_, pData_);

	EnqueuePacketData(clientIndex_);
}

void PacketManager::PushSystemPacket(PacketInfo packet_)
{
	lock_guard<mutex> guard(mLock);
	mSystemPacketQueue.push_back(packet_);
}

void PacketManager::CreateComponent(const UINT32 maxClient_)
{
	mUserManager = make_shared<UserManager>();
	mUserManager->Init(maxClient_);
}

void PacketManager::ClearConnectionInfo(INT32 clientIndex_)
{
	auto pReqUser = mUserManager->GetUserByConnIdx(clientIndex_);

	if (pReqUser->GetDomainState() != DOMAIN_STATE::NONE)
		mUserManager->DeleteUserInfo(pReqUser);
}

void PacketManager::EnqueuePacketData(const UINT32 clientIndex_)
{
	lock_guard<mutex> guard(mLock);
	mInComingPacketUserIndex.push_back(clientIndex_);
}

PacketInfo PacketManager::DequePacketData()
{
	UINT32 userIndex = 0;

	{
		lock_guard<mutex> guard(mLock);
		if (mInComingPacketUserIndex.empty())
			return PacketInfo();
	}

	userIndex = mInComingPacketUserIndex.front();
	mInComingPacketUserIndex.pop_front();

	auto pUser = mUserManager->GetUserByConnIdx(userIndex);
	auto packetData = pUser->GetPacket();
	packetData.ClientIndex = userIndex;

	return packetData;
}

PacketInfo PacketManager::DequeSystemPacketData()
{
	lock_guard<mutex> guard(mLock);
	if (mSystemPacketQueue.empty())
		return PacketInfo();

	auto packetData = mSystemPacketQueue.front();
	mSystemPacketQueue.pop_front();

	return packetData;
}

void PacketManager::ProcessPacket()
{
	while (mIsRunProcessThread)
	{
		bool isIdle = true;

		if (auto packetData = DequePacketData(); packetData.PacketId > (UINT16)PACKET_ID::SYS_END)
		{
			isIdle = false;
			ProcessRecvPacket(packetData.ClientIndex, packetData.PacketId, packetData.DataSize, packetData.pDataPtr);
		}

		if (auto packetData = DequeSystemPacketData(); packetData.PacketId != 0)
		{
			isIdle = false;
			ProcessRecvPacket(packetData.ClientIndex, packetData.PacketId, packetData.DataSize, packetData.pDataPtr);
		}

		if (isIdle)
		{
			this_thread::sleep_for(chrono::milliseconds(1));
		}
	}
}

void PacketManager::ProcessRecvPacket(const UINT32 clientIndex_, const UINT16 packetId_, const UINT16 packetSize_, shared_ptr<char[]> pPacket_)
{
	auto iter = mRecvFunctionDictionary.find(packetId_);
	if (iter != mRecvFunctionDictionary.end())
		(this->*(iter->second))(clientIndex_, packetSize_, pPacket_);
}

void PacketManager::ProcessUserConnect(UINT32 clientIndex_, UINT16 packetSize_, shared_ptr<char[]> pPacket_)
{
	printf("[ProcessUserConnect] clientIndex : %d\n", clientIndex_);
	auto pUser = mUserManager->GetUserByConnIdx(clientIndex_);
	pUser->Clear();
}

void PacketManager::ProcessUserDisConnect(UINT32 clientIndex_, UINT16 packetSize_, shared_ptr<char[]> pPacket_)
{
	printf("[ProcessUserDisConnect] clientIndex : %d\n", clientIndex_);
	ClearConnectionInfo(clientIndex_);
}

void PacketManager::ProcessLogin(UINT32 clientIndex_, UINT16 packetSize_, shared_ptr<char[]> pPacket_)
{
	if (LOGIN_REQUEST_PACKET_SIZE != packetSize_)
		return;

	auto pLoginReqPacket = reinterpret_cast<LOGIN_REQUEST_PACKET*>(pPacket_.get());

	printf("requested user id = %s\n", pLoginReqPacket->UserID);

	LOGIN_RESPONSE_PACKET loginResPacket = {};
	loginResPacket.PacketId = (UINT16)PACKET_ID::LOGIN_RESPONSE;
	loginResPacket.PacketLength = sizeof(LOGIN_RESPONSE_PACKET);

	if (mUserManager->GetCurrentUserCnt() >= mUserManager->GetMaxUserCnt())
	{
		loginResPacket.Result = (UINT16)ERROR_CODE::LOGIN_USER_USED_ALL_OBJ;
		SendPacketFunc(clientIndex_, sizeof(LOGIN_RESPONSE_PACKET), MakePacketBuffer(loginResPacket));
		return;
	}

	if (mUserManager->FindUserIndexByID(pLoginReqPacket->UserID) == -1)
	{
		loginResPacket.Result = (UINT16)ERROR_CODE::NONE;
		SendPacketFunc(clientIndex_, sizeof(LOGIN_RESPONSE_PACKET), MakePacketBuffer(loginResPacket));
	}
	else
	{
		loginResPacket.Result = (UINT16)ERROR_CODE::LOGIN_USER_ALREADY;
		SendPacketFunc(clientIndex_, sizeof(LOGIN_RESPONSE_PACKET), MakePacketBuffer(loginResPacket));
		return;
	}
}
