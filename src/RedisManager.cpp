#include "pch.h"

#include "CRedisConn.h"

#include "RedisManager.h"
#include "RedisLogin.h"

RedisManager::RedisManager() = default;

RedisManager::~RedisManager() = default;

bool RedisManager::Run(string ip_, UINT16 port_, const UINT32 threadCount_)
{
	mConn = make_unique<RedisCpp::CRedisConn>();
	if (Connect(ip_, port_) == false)
	{
		spdlog::error("Redis connect failed\n");
		return false;
	}

	mIsTaskRun = true;

	for (UINT32 i = 0; i < threadCount_; i++)
		mTaskThreads.emplace_back([this]() { TaskProcessThread(); });

	spdlog::info("Redis running\n");

	return true;
}

void RedisManager::End()
{
	mIsTaskRun = false;

	for (auto& thread : mTaskThreads)
	{
		if (thread.joinable())
			thread.join();
	}
}

void RedisManager::PushTask(RedisTask task_)
{
	lock_guard<mutex> guard(mReqLock);
	mRequestTask.push_back(task_);
}

RedisTask RedisManager::TakeResponseTask()
{
	lock_guard<mutex> guard(mResLock);

	if (mResponseTask.empty())
		return RedisTask();

	auto task = mResponseTask.front();
	mResponseTask.pop_front();

	return task;
}

bool RedisManager::Connect(string ip_, UINT16 port_)
{
	if (mConn->connect(ip_, port_) == false)
	{
		spdlog::error("redis connect error : {}\n", mConn->getErrorStr());
		return false;
	}
	else
	{
		spdlog::info("redis connect success\n");
	}

	return true;
}

void RedisManager::TaskProcessThread()
{
	spdlog::info("redis task process thread run\n");

	while (mIsTaskRun)
	{
		bool isIdle = true;

		if (auto task = TakeRequestTask(); task.TaskID != RedisTaskID::INVALID)
		{
			isIdle = false;
			if (task.TaskID == RedisTaskID::REQUEST_LOGIN)
			{
				auto pRequest = static_pointer_cast<RedisLoginReq>(task.pData);

				RedisLoginRes bodyData = {};
				bodyData.Result = ERROR_CODE::LOGIN_USER_INVALID_PW;

				string value;
				if (mConn->get(pRequest->UserID, value))
				{
					bodyData.Result = ERROR_CODE::NONE;

					if (value.compare(pRequest->UserPW) == 0)
						bodyData.Result = ERROR_CODE::NONE;
				}

				RedisTask resTask = {};
				resTask.UserIndex = task.UserIndex;
				resTask.TaskID = RedisTaskID::RESPONSE_LOGIN;
				resTask.DataSize = sizeof(RedisLoginRes);
				auto pRes = make_shared<RedisLoginRes>(bodyData);
				resTask.pData = pRes;

				PushResponse(resTask);
			}

			task.Release();
		}

		if (isIdle)
		{
			this_thread::sleep_for(chrono::milliseconds(1));
		}
	}

	spdlog::info("Redis thread finished\n");
}

RedisTask RedisManager::TakeRequestTask()
{
	lock_guard<mutex> guard(mReqLock);

	if (mRequestTask.empty())
		return RedisTask();

	auto task = mRequestTask.front();
	mRequestTask.pop_front();

	return task;
}

void RedisManager::PushResponse(RedisTask task_)
{
	lock_guard<mutex> guard(mResLock);
	mResponseTask.push_back(task_);
}
