#pragma once
#include <thread>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <atomic>
#include "Task.h"
#include "Memory\Memory\PoolAllocator.h"

#ifdef _WINDLL
#define RIG3D __declspec(dllexport)
#else
#define RIG3D __declspec(dllimport)
#endif

namespace cliqCity
{
	namespace multicore
	{
		typedef cliqCity::memory::PoolAllocator TaskPool;
		typedef std::atomic<uint32_t>			AtomicCounter;
		typedef std::condition_variable			Signal;
		typedef std::mutex						Mutex;
		typedef std::unique_lock<Mutex>			UniqueLock;
		typedef std::lock_guard<Mutex>			ScopedLock;
		typedef std::thread						Thread;
		typedef std::queue<Task*>				TaskQueue;

		class RIG3D TaskDispatcher
		{
		public:
			TaskDispatcher(Thread* threads, uint8_t threadCount, void* memory, size_t size);
			TaskDispatcher();
			~TaskDispatcher();

			void Start();
			void Pause();
			bool IsPaused();

			TaskID  AddTask(const TaskData& data, TaskKernel kernel);

			void Synchronize();
			void WaitForTask(const TaskID& taskID) const;
			bool IsTaskFinished(const TaskID& taskID) const;

		private:
			AtomicCounter	mTaskGeneration;
			Signal			mTaskSignal;
			Signal			mThreadSignal;
			Mutex			mMemoryLock;
			Mutex			mTaskQueueLock;
			Mutex			mThreadLock;
			TaskQueue		mTaskQueue;
			TaskPool		mAllocator;
			void*			mMemory;
			Thread*			mThreads;
			uint8_t			mThreadCount;
			uint8_t			mActiveThreadCount;
			bool			mIsPaused;

			TaskID	GetTaskID(Task* task) const;
			Task*	GetTask(const TaskID& taskID) const;

			Task*	WaitForAvailableTasks();
			Task*	AllocateTask();
			void	FreeTask(Task* task);
			void	QueueTask(Task* task);
			void	ExecuteTask(Task* task);
			void	ProcessTasks();
			void	JoinThreads();
		};
	}
}