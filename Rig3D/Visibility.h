#pragma once
#include "GraphicsMath/cgm.h"
#include "Rig3D/Parametric.h"
#include <vector>

namespace Rig3D
{
	struct Frustum
	{
		Plane<vec3f> front, back, left, right, bottom, top;
	};

	inline void ExtractNormalizedFrustumLH(Frustum* frustum, mat4f& viewProjectionMatrix)
	{
		float distance, magnitude;

		// Near
		
		vec3f front =
		{
			viewProjectionMatrix[0][2],
			viewProjectionMatrix[1][2],
			viewProjectionMatrix[2][2]
		};

		distance = viewProjectionMatrix[3][2];
		magnitude = cliqCity::graphicsMath::magnitude(front);

		frustum->front.normal = front / magnitude;
		frustum->front.distance = -distance / magnitude;

		// Far

		vec3f back =
		{
			viewProjectionMatrix[0][3] - viewProjectionMatrix[0][2],
			viewProjectionMatrix[1][3] - viewProjectionMatrix[1][2],
			viewProjectionMatrix[2][3] - viewProjectionMatrix[2][2]
		};

		distance = viewProjectionMatrix[3][3] - viewProjectionMatrix[3][2];
		magnitude = cliqCity::graphicsMath::magnitude(back);

		frustum->back.normal = back / magnitude;
		frustum->back.distance = -distance / magnitude;

		// Left / Right
		
		vec3f left = 
		{ 
			viewProjectionMatrix[0][3] + viewProjectionMatrix[0][0], 
			viewProjectionMatrix[1][3] + viewProjectionMatrix[1][0], 
			viewProjectionMatrix[2][3] + viewProjectionMatrix[2][0]
		};

		distance = viewProjectionMatrix[3][3] + viewProjectionMatrix[3][0];
		magnitude = cliqCity::graphicsMath::magnitude(left);

		left /= magnitude;
		distance /= magnitude;

		frustum->left.normal = left;
		frustum->left.distance = -distance;

		vec3f right =
		{
			viewProjectionMatrix[0][3] - viewProjectionMatrix[0][0],
			viewProjectionMatrix[1][3] - viewProjectionMatrix[1][0],
			viewProjectionMatrix[2][3] - viewProjectionMatrix[2][0]
		};

		distance = viewProjectionMatrix[3][3] - viewProjectionMatrix[3][0];
		magnitude = cliqCity::graphicsMath::magnitude(right);

		right /= magnitude;
		distance /= magnitude;

		frustum->right.normal = right;
		frustum->right.distance = -distance;

		// Top / Bottom

		vec3f bottom =
		{
			viewProjectionMatrix[0][3] + viewProjectionMatrix[0][1],
			viewProjectionMatrix[1][3] + viewProjectionMatrix[1][1],
			viewProjectionMatrix[2][3] + viewProjectionMatrix[2][1]
		};

		distance = viewProjectionMatrix[3][3] + viewProjectionMatrix[3][1];
		magnitude = cliqCity::graphicsMath::magnitude(bottom);

		bottom /= magnitude;
		distance /= magnitude;

		frustum->bottom.normal = bottom;
		frustum->bottom.distance = -distance;

		vec3f top =
		{
			viewProjectionMatrix[0][3] - viewProjectionMatrix[0][1],
			viewProjectionMatrix[1][3] - viewProjectionMatrix[1][1],
			viewProjectionMatrix[2][3] - viewProjectionMatrix[2][1]
		};

		distance = viewProjectionMatrix[3][3] - viewProjectionMatrix[3][1];
		magnitude = cliqCity::graphicsMath::magnitude(top);

		top /= magnitude;
		distance /= magnitude;

		frustum->top.normal = top;
		frustum->top.distance = -distance;
	}

	inline void Cull(const Frustum& frustum, Sphere<vec3f>* spheres, std::vector<uint32_t>& indices, const uint32_t& count)
	{

		float distance;
		const Plane<vec3f>* planes[6] =
		{
			&frustum.front,
			&frustum.back,
			&frustum.left,
			&frustum.right,
			&frustum.bottom,
			&frustum.top,
		};

		for (uint32_t i = 0; i < count; i++)
		{
			bool shouldCull = false;
			for (uint32_t p = 0; p < 6; p++)
			{
				distance = cliqCity::graphicsMath::dot(planes[p]->normal, spheres[i].origin) - (planes[p]->distance - spheres[i].radius);
				if (distance < 0)
				{
					shouldCull = true;
					break;
				}
			}

			if (!shouldCull) 
			{
				indices.push_back(i);
			}
		}
	}
}