#pragma once
#include <stdint.h>
#include <GraphicsMath/cgm.h>
// Reference: http://stackoverflow.com/a/364562

namespace Rig3D
{
	template<class T>
	class Traits
	{
	public:
		struct Alignment
		{
			uint8_t	a;
			T		t;
		};

		enum
		{
			AlignmentOf = sizeof(Alignment) - sizeof(T)
		};
	};



	inline size_t GetAlignment(size_t size)
	{
		switch (size)
		{
		case 2:
			return Traits<uint16_t>::AlignmentOf;
		case 4:
			return Traits<uint32_t>::AlignmentOf;
		case 8:
			return Traits<uint64_t>::AlignmentOf;
		case 12:
			return Traits<vec3f>::AlignmentOf;
		case 16:
			return Traits<vec4f>::AlignmentOf;
		case 64:
			return Traits<mat4f>::AlignmentOf;
		default:
			return Traits<uint8_t>::AlignmentOf;
		}
	}
}