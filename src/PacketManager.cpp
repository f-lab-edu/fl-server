#include "pch.h"

#include "PacketManager.h"
#include "UserManager.h"
#include "User.h"

#include "RedisManager.h"
#include "RedisLogin.h"
#include "RedisTask.h"

void PacketManager::Init(const UINT32 maxClient_)
{
	mRecvFunctionDictionary = unordered_map<int, PROCESS_RECV_PACKET_FUNCTION>();

	mRecvFunctionDictionary[PACKET_ID::SYS_USER_CONNECT] = &PacketManager::ProcessUserConnect;
	mRecvFunctionDictionary[PACKET_ID::SYS_USER_DISCONNECT] = &PacketManager::ProcessUserDisConnect;

	mRecvFunctionDictionary[PACKET_ID::LOGIN_REQUEST] = &PacketManager::ProcessLogin;
	mRecvFunctionDictionary[RedisTaskID::RESPONSE_LOGIN] = &PacketManager::ProcessLoginDBResult;

	CreateComponent(maxClient_);

	mRedisManager = make_shared<RedisManager>();
}

bool PacketManager::Run()
{
	if (mRedisManager->Run("127.0.0.1", 6379, 1) == false)
		return false;

	mIsRunProcessThread = true;
	mProcessThread = thread([this]() { ProcessPacket(); });

	return true;
}

void PacketManager::End()
{
	mRedisManager->End();

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

		if (auto task = mRedisManager->TakeResponseTask(); task.TaskID != RedisTaskID::INVALID)
		{
			isIdle = false;

			shared_ptr<char[]> dataPtr(task.pData, static_cast<char*>(task.pData.get()));
			ProcessRecvPacket(task.UserIndex, task.TaskID, task.DataSize, dataPtr);
			task.Release();
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
		RedisLoginReq dbReq = {};
		CopyMemory(dbReq.UserID, pLoginReqPacket->UserID, (MAX_USER_ID_LEN) + 1);
		CopyMemory(dbReq.UserPW, pLoginReqPacket->UserPW, (MAX_USER_PW_LEN) + 1);

		RedisTask task = {};
		task.UserIndex = clientIndex_;
		task.TaskID = RedisTaskID::REQUEST_LOGIN;
		task.DataSize = sizeof(RedisLoginReq);
		task.pData = make_shared<RedisLoginReq>(dbReq);
		mRedisManager->PushTask(task);
	}
	else
	{
		loginResPacket.Result = (UINT16)ERROR_CODE::LOGIN_USER_ALREADY;
		SendPacketFunc(clientIndex_, sizeof(LOGIN_RESPONSE_PACKET), MakePacketBuffer(loginResPacket));
		return;
	}
}

void PacketManager::ProcessLoginDBResult(UINT32 clientIndex_, UINT16 packetSize_, shared_ptr<char[]> pPacket_)
{
	spdlog::info("ProcessLoginDBResult. UserIndex: {}", clientIndex_);

	auto pBody = reinterpret_cast<RedisLoginRes*>(pPacket_.get());

	if (pBody->Result == ERROR_CODE::NONE)
	{
		// 로그인 완료 처리
	}

	LOGIN_RESPONSE_PACKET loginResPacket = {};
	loginResPacket.PacketId = PACKET_ID::LOGIN_RESPONSE;
	loginResPacket.PacketLength = sizeof(LOGIN_RESPONSE_PACKET);
	loginResPacket.Result = pBody->Result;
	SendPacketFunc(clientIndex_, sizeof(LOGIN_RESPONSE_PACKET), MakePacketBuffer(loginResPacket));
}
