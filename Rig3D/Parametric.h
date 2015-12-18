#pragma once
#include "GraphicsMath/cgm.h"

namespace Rig3D
{
	template<class Vector>
	struct Ray
	{
		Vector origin;
		Vector normal;
	};

	template<class Vector>
	struct Line
	{
		Vector origin;
		Vector end;
	};

	template<class Vector>
	struct AABB
	{
		Vector origin;
		Vector halfSize;
	};

	template<class Vector>
	struct Sphere
	{
		Vector	origin;
		float	radius;
	};

	template<class Vector>
	struct Plane
	{
		Vector normal;
		float  distance;
	};
}

typedef Rig3D::AABB<vec2f>		BoxCollider2D;
typedef Rig3D::AABB<vec3f>		BoxCollider;
typedef Rig3D::Sphere<vec2f>	CircleCollider;
typedef Rig3D::Sphere<vec3f>	SphereCollider;