// Resources: http://blog.molecular-matters.com/2012/04/12/building-a-load-balanced-task-scheduler-part-2-task-model-relationships/

#pragma once
#include <stdint.h>

#ifdef _WINDLL
#define RIG3D __declspec(dllexport)
#else
#define RIG3D __declspec(dllimport)
#endif

namespace cliqCity
{
	namespace multicore
	{
		struct RIG3D TaskID
		{
			uint32_t mOffset;
			uint32_t mGeneration;

			TaskID(uint32_t offset, uint32_t generation) : mOffset(offset), mGeneration(generation) {};
			TaskID() : TaskID(0, 0) {};
		};

		struct RIG3D TaskData
		{
			void* mKernelData;

			union
			{
				struct Stream
				{
					void* in[4];
					void* out[4];
				} mStream;
			};
			
			TaskData() : mKernelData(nullptr) {};
		};
		
		typedef void(*TaskKernel)(const TaskData&);
		
		class RIG3D Task
		{
		public:
			char		mAlias[sizeof(void*)];
			uint32_t	mGeneration;
			TaskData	mData;
			TaskKernel	mKernel;

			Task() : mGeneration(0), mKernel(nullptr) {};
			~Task() {};
		};
	}
}

namespace std
{
	template<>
	struct RIG3D hash<cliqCity::multicore::TaskID>
	{
		size_t operator()(const cliqCity::multicore::TaskID& taskID) const
		{
			hash<uint32_t> hashFunction;
			return hashFunction(taskID.mOffset + taskID.mGeneration);
		}
	};

	template<>
	struct equal_to<cliqCity::multicore::TaskID>
	{
		bool operator()(const cliqCity::multicore::TaskID& lhs, const cliqCity::multicore::TaskID& rhs) const
		{
			return (lhs.mOffset == rhs.mOffset && lhs.mGeneration == rhs.mGeneration);
		}
	};
}


