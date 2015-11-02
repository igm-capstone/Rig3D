#include "TaskDispatcher.h"

using namespace cliqCity::multicore;

TaskDispatcher::TaskDispatcher(Thread* threads, uint8_t threadCount, void* memory, size_t size) :
	mTaskGeneration(0),
	mAllocator(memory, reinterpret_cast<char*>(memory) + size, sizeof(Task)),
	mMemory(memory),
	mThreads(threads),
	mThreadCount(threadCount),
	mActiveThreadCount(0),
	mIsPaused(true)
{

}

TaskDispatcher::TaskDispatcher() : TaskDispatcher(nullptr, 0, nullptr, 0)
{

}

TaskDispatcher::~TaskDispatcher()
{
	// Wait for all currently executing tasks. Threads should be in waiting state.
	while (!mTaskQueue.empty())
	{
		std::this_thread::yield();
	}

	// Pause queue so threads will exit upon completion.
	Pause();

	mThreads = nullptr;
}

void TaskDispatcher::Start()
{
	if (!mIsPaused)
	{
		return;
	}

	mIsPaused = false;
	for (int i = 0; i < mThreadCount; i++)
	{
		mThreads[i] = std::thread(&TaskDispatcher::ProcessTasks, this);
		mActiveThreadCount++;
	}
}

void TaskDispatcher::Pause()
{
	mIsPaused = true;

	UniqueLock lock(mThreadLock);
	while (mActiveThreadCount != 0)
	{
		// Wait until receiving exit signals from all threads
		mThreadSignal.wait(lock);

		// Prompt any waiting threads to exit.
		mTaskSignal.notify_all();
	}

	// Finally join threads.
	JoinThreads();
}

bool TaskDispatcher::IsPaused()
{
	return mIsPaused;
}

TaskID TaskDispatcher::AddTask(const TaskData& data, TaskKernel kernel)
{
	Task* task = AllocateTask();
	task->mData = data;
	task->mKernel = kernel;

	TaskID taskID = GetTaskID(task);

	QueueTask(task);

	return taskID;
}

void TaskDispatcher::Synchronize()
{
	if (mIsPaused)
	{
		return;
	}

	while (!mTaskQueue.empty())
	{
		std::this_thread::yield();
	}

	// Use pause to kill any threads still executing a task
	Pause();

	// Restart threads
	Start();
}

void TaskDispatcher::WaitForTask(const TaskID& taskID) const
{
	while (!IsTaskFinished(taskID))
	{
		// Could help with tasks if possible...
		std::this_thread::yield();
	}
}

bool TaskDispatcher::IsTaskFinished(const TaskID& taskID) const
{
	Task* task = GetTask(taskID);
	if (task->mGeneration != taskID.mGeneration)
	{
		return true;
	}
	else
	{
		// TO DO: Check dependent tasks.
	}

	return false;
}

inline TaskID TaskDispatcher::GetTaskID(Task* task) const
{
	return TaskID(task - reinterpret_cast<Task*>(mMemory), task->mGeneration);
}

inline Task* TaskDispatcher::GetTask(const TaskID& taskID) const
{
	return reinterpret_cast<Task*>(mMemory) + taskID.mOffset;
}

inline Task* TaskDispatcher::WaitForAvailableTasks()
{
	UniqueLock pendingLock(mTaskQueueLock);
	while (mTaskQueue.empty())
	{
		if (mIsPaused)
		{
			return nullptr;
		}

		mTaskSignal.wait(pendingLock);
	}

	Task* task = mTaskQueue.front();
	mTaskQueue.pop();

	return task;
}

inline Task* TaskDispatcher::AllocateTask()
{
	Task* task = nullptr;
	{
		ScopedLock lock(mMemoryLock);
		task = reinterpret_cast<Task*>(mAllocator.Allocate());
	}

	task->mGeneration = ++mTaskGeneration;
	return task;
}

inline void TaskDispatcher::FreeTask(Task* task)
{
	task->mGeneration = ++mTaskGeneration;
	{
		ScopedLock lock(mMemoryLock);
		mAllocator.Free(task);
	}
}

inline void TaskDispatcher::QueueTask(Task* task)
{
	{
		UniqueLock lock(mTaskQueueLock);
		mTaskQueue.push(task);
	}

	mTaskSignal.notify_one();	
}

inline void TaskDispatcher::ExecuteTask(Task* task)
{	
	(task->mKernel)(task->mData);
}

inline void TaskDispatcher::ProcessTasks()
{
	while (!mIsPaused)
	{
		Task* task = WaitForAvailableTasks();
		if (task)
		{
			ExecuteTask(task);
			FreeTask(task);
		}
	}

	UniqueLock lock(mThreadLock);
	mActiveThreadCount--;
	std::notify_all_at_thread_exit(mThreadSignal, std::move(lock));
}

inline void TaskDispatcher::JoinThreads()
{
	for (int i = 0; i < mThreadCount; i++)
	{
		if (mThreads[i].joinable())
		{
			mThreads[i].join();
		}
	}
}