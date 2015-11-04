#pragma once
#include "Memory/Memory/LinearAllocator.h"
#include <new>

namespace Rig3D
{
	class IShader
	{
	public:
		IShader();
		virtual ~IShader();

		virtual void VLoad(const char* filename) = 0;
	};
}
