#pragma once
#define _USE_MATH_DEFINES
#include <cmath>
#include <vector>
#include <math.h>

namespace Rig3D
{
	namespace Geometry
	{
		template <class Vertex, class Index>
		void Plane(std::vector<Vertex>& vertices, std::vector<Index>& indices, float width, float depth, uint32_t vertexWidth, uint32_t vertexDepth)
		{
			vertices.reserve(sizeof(Vertex) * vertexWidth * vertexDepth);
			indices.reserve(sizeof(Index) * (vertexWidth - 1) * (vertexDepth - 1) * 6);

			float widthStep = width / static_cast<float>(vertexWidth - 1);
			float depthStep = depth / static_cast<float>(vertexDepth - 1);

			float halfWidth = width * 0.5f;
			float halfDepth = depth * 0.5f;

			for (uint32_t z = 0; z < vertexDepth; z++)
			{
				for (uint32_t x = 0; x < vertexWidth; x++)
				{
					Vertex v0;
					v0.Position = { x * widthStep - halfWidth, 0.0f, z * depthStep - halfDepth };
					v0.Normal	= { 0.0f, 1.0f, 0.0f };
					v0.UV = { static_cast<float>(x) / (vertexWidth - 1), 1.0f - (static_cast<float>(z) / (vertexDepth - 1)) };
					vertices.push_back(v0);
				}
			}

			for (uint32_t z = 0; z < vertexDepth - 1; z++)
			{
				for (uint32_t x = 0; x < vertexWidth - 1; x++)
				{
					indices.push_back((z * vertexWidth) + x);
					indices.push_back(((z + 1) * vertexWidth) + x);
					indices.push_back(((z + 1) * vertexWidth) + x + 1);
					indices.push_back(((z + 1) * vertexWidth) + x + 1);
					indices.push_back((z * vertexWidth) + x + 1);
					indices.push_back((z * vertexWidth) + x);
				}
			}
		}

		template <class Vertex, class Index>
		static void Sphere(std::vector<Vertex>& vertices, std::vector<Index>& indices, uint32_t azimuthSubdivisions, uint32_t polarSubdivisions, float radius)
		{
			uint32_t index = 0;
			float pi = static_cast<float>(M_PI);

			for (uint32_t t = 0; t < polarSubdivisions; t++)
			{
				float theta0 = (static_cast<float>(t) / polarSubdivisions) * pi;
				float theta1 = (static_cast<float>(t + 1) / polarSubdivisions) * pi;

				for (uint32_t p = 0; p < azimuthSubdivisions; p++)
				{
					float phi0 = (static_cast<float>(p) / azimuthSubdivisions) * pi * 2.0f;
					float phi1 = (static_cast<float>(p + 1) / azimuthSubdivisions) * pi * 2.0f;


					float sinTheta0 = sin(theta0);
					float cosTheta0 = cos(theta0);
					float sinPhi0 = sin(phi0);
					float cosPhi0 = cos(phi0);

					float sinTheta1 = sin(theta1);
					float cosTheta1 = cos(theta1);
					float sinPhi1 = sin(phi1);
					float cosPhi1 = cos(phi1);

					vec3f n0 = { sinTheta0 * cosPhi0, cosTheta0, sinTheta0 * sinPhi0 };
					vec3f n1 = { sinTheta0 * cosPhi1, cosTheta0, sinTheta0 * sinPhi1 };
					vec3f n2 = { sinTheta1 * cosPhi1, cosTheta1, sinTheta1 * sinPhi1 };
					vec3f n3 = { sinTheta1 * cosPhi0, cosTheta1, sinTheta1 * sinPhi0 };

					Vertex v0, v1, v2, v3;
					v0.Position = radius * n0;
					v1.Position = radius * n1;
					v2.Position = radius * n2;
					v3.Position = radius * n3;

					v0.Normal = n0;
					v1.Normal = n1;
					v2.Normal = n2;
					v3.Normal = n3;

					v0.UV = { 0.5f + (atan2(n0.z, n0.x) / (2.0f * pi)), 0.5f - (asin(n0.y) / pi) };
					v0.UV = { 0.5f + (atan2(n1.z, n1.x) / (2.0f * pi)), 0.5f - (asin(n1.y) / pi) };
					v0.UV = { 0.5f + (atan2(n2.z, n2.x) / (2.0f * pi)), 0.5f - (asin(n2.y) / pi) };
					v0.UV = { 0.5f + (atan2(n3.z, n3.x) / (2.0f * pi)), 0.5f - (asin(n3.y) / pi) };

					if (t == 0)
					{
						vertices.push_back(v3);
						vertices.push_back(v2);
						vertices.push_back(v0);

						indices.push_back(index++);
						indices.push_back(index++);
						indices.push_back(index++);
					}
					else if (t + 1 == polarSubdivisions)
					{
						vertices.push_back(v1);
						vertices.push_back(v0);
						vertices.push_back(v2);

						indices.push_back(index++);
						indices.push_back(index++);
						indices.push_back(index++);
					}
					else
					{
						vertices.push_back(v3);
						vertices.push_back(v1);
						vertices.push_back(v0);

						vertices.push_back(v3);
						vertices.push_back(v2);
						vertices.push_back(v1);

						indices.push_back(index++);
						indices.push_back(index++);
						indices.push_back(index++);

						indices.push_back(index++);
						indices.push_back(index++);
						indices.push_back(index++);
					}
				}
			}
		}
	}
}