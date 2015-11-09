#pragma once
#include "Rig3D/rig_defines.h"
#include <d3d11.h>

namespace Rig3D
{
	inline D3D11_USAGE GD3D11_Usage_Map(const GPUMemoryUsage& usage)
	{
		switch (usage)
		{
		case GPU_MEMORY_USAGE_STATIC:
			return D3D11_USAGE_IMMUTABLE;
		case GPU_MEMORY_USAGE_DYNAMIC:
			return D3D11_USAGE_DYNAMIC;
		case GPU_MEMORY_USAGE_COPY:
			return D3D11_USAGE_STAGING;
		default:
			return D3D11_USAGE_DEFAULT;
			;
		}
	}

	inline D3D11_PRIMITIVE_TOPOLOGY GD3D11_Primitive_Type_Map(const GPUPrimitiveType& primitiveType)
	{
		switch (primitiveType)
		{
		case GPU_PRIMITIVE_TYPE_POINT:
			return D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
		case GPU_PRIMITIVE_TYPE_LINE:
			return D3D_PRIMITIVE_TOPOLOGY_LINELIST;
		case GPU_PRIMITIVE_TYPE_TRIANGLE:
			return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		default:
			return D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
			;
		}
	}


}