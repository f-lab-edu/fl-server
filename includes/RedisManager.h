#pragma once

#include "CRedisConn.h"

#include "RedisTask.h"

// docker pull redis:8.8-m03-alpine3.23
// port 6379
// volume redis-data:/data

struct RedisTask;

class RedisManager
{
public:
	RedisManager() = default;
	~RedisManager() = default;

	bool Run(string ip_, UINT16 port_, const UINT32 threadCount_);

	void End();

	void PushTask(RedisTask task_);

	RedisTask TakeResponseTask();

private:
	bool Connect(string ip_, UINT16 port_);

	void TaskProcessThread();

	RedisTask TakeRequestTask();

	void PushResponse(RedisTask task_);

private:
	RedisCpp::CRedisConn mConn;

	atomic<bool> mIsTaskRun = false;
	vector<thread> mTaskThreads;

	mutex mReqLock;
	deque<RedisTask> mRequestTask;

	mutex mResLock;
	deque<RedisTask> mResponseTask;
};

