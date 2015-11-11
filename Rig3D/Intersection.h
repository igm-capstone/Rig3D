#pragma once

#pragma once
#include "Parametric.h"
#include <cmath>
#include <algorithm>
#include <float.h>
#include "GraphicsMath\cgm.h"

template<class Vector>
int IntersectRayPlane(const Ray<Vector>& ray, const Plane<Vector>& plane, Vector& poi, float& t)
{
	float denominator = cliqCity::graphicsMath::dot(ray.normal, plane.normal);
	
	// Ray is parallel to plane and there is no intersection
	if (denominator == 0.0f)
	{
		return 0;
	}

	float numerator = plane.distance - cliqCity::graphicsMath::dot(ray.origin, plane.normal);

	t = numerator / denominator;
	poi = ray.origin + ray.normal * t;

	return 1;
}

template<class Vector>
int IntersectRaySphere(const Ray<Vector>& ray, const Sphere<Vector>& sphere, Vector& poi, float& t)
{
	// R(t) = P + tD, where t >= 0
	// S(t) = (X - C) * (X - C) = r^2, where X is a point on the surface of the sphere
	// Substitute R(t) for X to find the value of t for which the ray intersects the sphere

	// Let m = Ray.origin - sphere.origin
	Vector m = ray.origin - sphere.origin;

	// Let b = projection of ray direction onto vector from rayOrigin to sphereOrigin
	float b = cliqCity::graphicsMath::dot(m, ray.normal);

	// Let c = difference between distance from rayOrigin to sphereOrigin and sphereRadius
	float c = cliqCity::graphicsMath::dot(m, m) - sphere.radius * sphere.radius;

	// If c > 0 rayOrigin is outside of sphere and if b > 0.0f ray is pointing away from sphere.
	if (c > 0.0f && b > 0.0f)
	{
		return 0;
	}

	float discriminant = b * b - c;

	// If discriminant is negative the ray misses the sphere
	if (discriminant < 0.0f)
	{
		return 0;
	}

	t = -b - sqrt(discriminant);
	poi = ray.origin + t * ray.normal;

	return 1;
}

template<class Vector>
int IntersectRayAABB(const Ray<Vector>& ray, const AABB<Vector>& aabb, Vector& poi, float& t)
{
	int numElements = sizeof(Vector) / sizeof(float);

	float tMin = 0.0f;
	float tMax = FLT_MAX;

	for (int i = 0; i < numElements; i++)
	{
		float aabbMin = aabb.origin[i] - aabb.radius;
		float aabbMax = aabb.origin[i] + aabb.radius;

		if (abs(ray.direction[i]) < FLT_EPSILON)
		{
			// Ray is parallel to slab. Check if origin is contained by plane
			if (ray.origin[i] < aabbMin || ray.origin[i] > aabbMax)
			{
				return 0;
			}
		}
		else
		{
			float ood = 1.0f / ray.direction[i];
			float t1 = (aabbMin - ray.origin[i]) * ood;
			float t2 = (aabbMax - ray.origin[i]) * ood;

			if (t1 > t2)
			{
				float temp = t2;
				t2 = t1;
				t1 = temp;
			}

			tMin = max(tMin, t1);
			tMax = min(tMax, t2);

			if (tMin > tMax)
			{
				return 0;
			}
		}
	}

	poi = ray.origin + ray.normal * tMin;
	t = tMin;
	return 1;
}


template<class Vector>
int IntersectSpherePlane(const Sphere<Vector>& sphere, const Plane<Vector>& plane)
{
	// Evaluating the plane equation (N * X) = d for a point gives signed distance from plane
	float distance = cliqCity::graphicsMath::dot(plane.normal, sphere.origin) - plane.distance;

	// If distance is less than radius we have intersection
	return (abs(distance) <= sphere.radius);
}

template<class Vector>
int IntersectDynamicSpherePlane(const Sphere<Vector>& sphere, const Vector& velocity, const Plane<Vector>& plane, Vector& poi, float& t)
{
	float distance = cliqCity::graphicsMath::dot(plane.normal, sphere.origin) - plane.distance;
	
	// Check if sphere is all ready overlapping
	if (abs(distance) <= sphere.radius)
	{
		t	= 0.0f;
		poi = sphere.origin;
		return 1;
	}
	else
	{
		float denominator = cliqCity::graphicsMath::dot(plane.normal, velocity);
		
		// Check if sphere is moving parallel to (0.0f), or away from (< 0.0f) the plane 
		if (denominator * distance >= 0.0f)
		{
			return 0;
		}
		else
		{
			// Spere is moving towards the plane

			float r	= (distance > 0.0f) ? sphere.radius : -sphere.radius;
			t		= (r - distance) / denominator;
			poi		= sphere.origin + t * velocity - r * plane.normal;
			return 1;
		}
	}
}

template<class Vector>
int IntersectSphereSphere(const Sphere<Vector>& s0, const Sphere<Vector>& s1)
{
	// Spheres are intersecting if distance is less than the sum of radii
	Vector distance = s0.origin - s1.origin;
	float distanceSquared = cliqCity::graphicsMath::dot(distance, distance);
	float sumRadii = (s0.radius + s1.radius);
	return (distanceSquared <= (sumRadii * sumRadii));
}

template<class Vector>
int IntersectDynamicSphereSphere(const Sphere<Vector>& s0, const Vector v0, const Sphere<Vector>& s1, const Vector& v1, Vector& poi, float& t)
{
	Vector relativeVelocity = v1 - v0;
	Vector distance			= s1.origin - s0.origin;
	float sumRadii			= s0.radius + s1.radius;
	float c = cliqCity::graphicsMath::dot(distance, distance) - sumRadii * sumRadii;
	if (c < 0.0f)
	{
		t = 0.0f;
		poi = s0.origin;
		return 1;
	}

	float a = cliqCity::graphicsMath::dot(relativeVelocity, relativeVelocity);
	if (a < FLT_EPSILON)
	{
		return 0;
	}

	float b = cliqCity::graphicsMath::dot(relativeVelocity, distance);
	if (b >= 0.0f)
	{
		return 0;
	}

	float d = b * b - a * c;
	if (d < 0.0f)
	{
		return 0;
	}

	t = (-b - sqrt(d)) / a;
	poi = s0.origin + t * v0; // Probably wrong
	return 1;
}