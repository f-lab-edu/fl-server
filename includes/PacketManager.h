#pragma once

class UserManager;
class RoomManager;
class RedisManager;

class PacketManager
{
public:
	PacketManager() = default;
	~PacketManager() = default;

private:
	using PROCESS_RECV_PACKET_FUNCTION = void(PacketManager::*)(UINT32, UINT16, shared_ptr<char>);
	unordered_map<int, PROCESS_RECV_PACKET_FUNCTION> mRecvFunctionDictionary;

	shared_ptr<UserManager> mUserManager;

	function<void(int, shared_ptr<char>)> mSendMQDataFunc;

	bool mIsRunProcessThread = false;

	thread mProcessThread;

	mutex mLock;

	deque<UINT32> mInComingPacketUserIndex;

	deque<PacketInfo> mSystemPacketQueue;
};

