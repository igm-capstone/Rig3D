#pragma once
#include "Rig3D\rig_defines.h"
#include "Rig3D\Graphics\DirectX11\DX11Mesh.h"
#include "GraphicsMath\cgm.h"
#include <vector>
#include <map>
#include <fstream>

namespace Rig3D
{
	class IRenderer;
	class IMesh;
	
#pragma region OBJResource
	template<class Vertex>
	class OBJBasicResource
	{
	public:
		std::vector<Vertex>		mVertices;
		std::vector<uint16_t>	mIndices;

		uint32_t mVertexCount;
		uint32_t mIndexCount;

		const char* mFilename;

		OBJBasicResource(const char* filename) : mVertexCount(0), mIndexCount(0), mFilename(filename)
		{

		}

		OBJBasicResource() : OBJBasicResource(nullptr)
		{

		}

		~OBJBasicResource()
		{

		}

		bool Load()
		{
			mVertices.clear();
			mIndices.clear();

			// File input object
			std::ifstream obj(mFilename);

			// Check for successful open
			if (!obj.is_open())
			{
				return false;
			}

			// Variables used while reading the file
			std::vector<vec3f> positions;     // Positions from the file
			std::vector<vec3f> normals;       // Normals from the file
			std::vector<vec2f> uvs;           // UVs from the file
			unsigned int triangleCounter = 0;    // Count of triangles/mIndices
			char chars[100];                     // String for line reading

												 // Still good?
			while (obj.good())
			{
				// Get the line (100 characters should be more than enough)
				obj.getline(chars, 100);

				// Check the type of line
				if (chars[0] == 'v' && chars[1] == 'n')
				{
					// Read the 3 numbers directly into an XMFLOAT3
					vec3f norm;
					sscanf_s(
						chars,
						"vn %f %f %f",
						&norm.x, &norm.y, &norm.z);

					// Add to the list of normals
					normals.push_back(norm);
				}
				else if (chars[0] == 'v' && chars[1] == 't')
				{
					// Read the 2 numbers directly into an XMFLOAT2
					vec2f uv;
					sscanf_s(
						chars,
						"vt %f %f",
						&uv.x, &uv.y);

					// Add to the list of uv's
					uvs.push_back(uv);
				}
				else if (chars[0] == 'v')
				{
					// Read the 3 numbers directly into an XMFLOAT3
					vec3f pos;
					sscanf_s(
						chars,
						"v %f %f %f",
						&pos.x, &pos.y, &pos.z);

					// Add to the positions
					positions.push_back(pos);
				}
				else if (chars[0] == 'f')
				{
					// Read the 9 face indices into an array
					unsigned int i[9];
					sscanf_s(
						chars,
						"f %d/%d/%d %d/%d/%d %d/%d/%d",
						&i[0], &i[1], &i[2],
						&i[3], &i[4], &i[5],
						&i[6], &i[7], &i[8]);

					// - Create the mVertices by looking up
					//    corresponding data from vectors
					// - OBJ File indices are 1-based, so
					//    they need to be adusted
					Vertex v1;
					v1.Position = positions[i[0] - 1];
					v1.UV = uvs[i[1] - 1];
					v1.Normal = normals[i[2] - 1];

					Vertex v2;
					v2.Position = positions[i[3] - 1];
					v2.UV = uvs[i[4] - 1];
					v2.Normal = normals[i[5] - 1];

					Vertex v3;
					v3.Position = positions[i[6] - 1];
					v3.UV = uvs[i[7] - 1];
					v3.Normal = normals[i[8] - 1];

					// Add the vertices to the vector
					mVertices.push_back(v1);
					mVertices.push_back(v2);
					mVertices.push_back(v3);

					// Add three more indices
					mIndices.push_back(triangleCounter++);
					mIndices.push_back(triangleCounter++);
					mIndices.push_back(triangleCounter++);
				}
			}

			// Close
			obj.close();

			return true;
		}
	};

	template<class Vertex>
	class OBJResource
	{
	public:
		std::vector<Vertex>		mVertices;
		std::vector<uint16_t>	mIndices;

		uint32_t mVertexCount;
		uint32_t mIndexCount;

		const char* mFilename;

		OBJResource(const char* filename) : mVertexCount(0), mIndexCount(0), mFilename(filename)
		{

		}

		OBJResource() : OBJResource(nullptr)
		{

		}

		~OBJResource()
		{

		}

		// Expects Vertex {position: float3, uv: float2, normal: float3, tangent: float4}
		bool Load()
		{
			mVertices.clear();
			mIndices.clear();

			// File input object
			std::ifstream obj(mFilename);

			// Check for successful open
			if (!obj.is_open())
			{
				return false;
			}

			// Variables used while reading the file
			std::vector<vec3f> positions;     // Positions from the file
			std::vector<vec3f> normals;       // Normals from the file
			std::vector<vec2f> uvs;           // UVs from the file
			std::map<int, std::vector<vec3f>> sharedTangentMap;
			std::map<int, std::vector<vec3f>> sharedBitangentMap;
			unsigned int triangleCounter = 0;    // Count of triangles/mIndices
			char chars[100];                     // String for line reading

												 // Still good?
			while (obj.good())
			{
				// Get the line (100 characters should be more than enough)
				obj.getline(chars, 100);

				// Check the type of line
				if (chars[0] == 'v' && chars[1] == 'n')
				{
					// Read the 3 numbers directly into an XMFLOAT3
					vec3f norm;
					sscanf_s(
						chars,
						"vn %f %f %f",
						&norm.x, &norm.y, &norm.z);

					// Add to the list of normals
					normals.push_back(norm);
				}
				else if (chars[0] == 'v' && chars[1] == 't')
				{
					// Read the 2 numbers directly into an XMFLOAT2
					vec2f uv;
					sscanf_s(
						chars,
						"vt %f %f",
						&uv.x, &uv.y);

					// Add to the list of uv's
					uvs.push_back(uv);
				}
				else if (chars[0] == 'v')
				{
					// Read the 3 numbers directly into an XMFLOAT3
					vec3f pos;
					sscanf_s(
						chars,
						"v %f %f %f",
						&pos.x, &pos.y, &pos.z);

					// Add to the positions
					positions.push_back(pos);
				}
				else if (chars[0] == 'f')
				{
					// Read the 9 face indices into an array
					unsigned int i[9];
					sscanf_s(
						chars,
						"f %d/%d/%d %d/%d/%d %d/%d/%d",
						&i[0], &i[1], &i[2],
						&i[3], &i[4], &i[5],
						&i[6], &i[7], &i[8]);

					// - Create the mVertices by looking up
					//    corresponding data from vectors
					// - OBJ File indices are 1-based, so
					//    they need to be adusted
					Vertex v1;
					v1.Position = positions[i[0] - 1];
					v1.UV = uvs[i[1] - 1];
					v1.Normal = normals[i[2] - 1];

					Vertex v2;
					v2.Position = positions[i[3] - 1];
					v2.UV = uvs[i[4] - 1];
					v2.Normal = normals[i[5] - 1];

					Vertex v3;
					v3.Position = positions[i[6] - 1];
					v3.UV = uvs[i[7] - 1];
					v3.Normal = normals[i[8] - 1];

					// Add the vertices to the vector
					mVertices.push_back(v1);
					mVertices.push_back(v2);
					mVertices.push_back(v3);

					// Add three more indices
					mIndices.push_back(triangleCounter++);
					mIndices.push_back(triangleCounter++);
					mIndices.push_back(triangleCounter++);

					float x1 = v2.Position.x - v1.Position.x;
					float x2 = v3.Position.x - v1.Position.x;
					float y1 = v2.Position.y - v1.Position.y;
					float y2 = v3.Position.y - v1.Position.y;
					float z1 = v2.Position.z - v1.Position.z;
					float z2 = v3.Position.z - v1.Position.z;

					float s1 = v2.UV.x - v1.UV.x;
					float s2 = v3.UV.x - v1.UV.x;
					float t1 = v2.UV.y - v1.UV.y;
					float t2 = v3.UV.y - v1.UV.y;

					float r = 1.0f / ((s1 * t2) - (s2 * t1));
					vec3f tangent = { (((t2 * x1) - (t1 * x2)) * r), (((t2 * y1) - (t1 * y2)) * r), (((t2 * z1) - (t1 * z2)) * r) };
					vec3f bitangent = { (((s2 * x1) - (s1 * x2)) * r), (((s2 * y1) - (s1 * y2)) * r), (((s2 * z1) - (s1 * z2)) * r) };

					for (int j = 3; j >= 0; j--) {
						int index = triangleCounter - j;
						if (sharedTangentMap.find(index) == sharedTangentMap.end()) {
							std::vector<vec3f> tangents = { tangent };
							std::vector<vec3f> bitangents = { bitangent };
							sharedTangentMap.insert({ index, tangents });
							sharedBitangentMap.insert({ index, bitangents });
						}
						else {
							sharedTangentMap.at(index).push_back(tangent);
							sharedBitangentMap.at(index).push_back(bitangent);
						}
					}
				}
			}

			for (unsigned int i = 0; i < mVertices.size(); i++)
			{
				std::vector<vec3f>& faceTangents = sharedTangentMap.at(i);
				std::vector<vec3f>& faceBitangents = sharedBitangentMap.at(i);
				vec3f vertexTangent = { 0.0f, 0.0f, 0.0f };
				vec3f vertexBitangent = { 0.0f, 0.0f, 0.0f };
				vec3f& vertexNormal = mVertices[i].Normal;

				for (unsigned int j = 0; j < faceTangents.size(); j++) {
					vertexTangent += vec4f(faceTangents[j]);
					vertexBitangent += vec4f(faceBitangents[j]);
				}

				vertexBitangent /= (float)faceBitangents.size();
				vertexTangent = cliqCity::graphicsMath::normalize(vertexTangent / (float)faceTangents.size());
				vertexTangent = cliqCity::graphicsMath::normalize((vertexTangent - vertexNormal * cliqCity::graphicsMath::dot(vertexNormal, vertexTangent)));
				mVertices[i].Tangent = vec4f(vertexTangent, 0.0f);
				mVertices[i].Tangent.w = cliqCity::graphicsMath::dot(cliqCity::graphicsMath::cross(vertexNormal, vertexTangent), vertexBitangent);
				mVertices[i].Tangent.w = (mVertices[i].Tangent.w < 0.0f) ? -1.0f : 1.0f;
			}

			// Close
			obj.close();
			
			return true;
		}
	};
#pragma endregion

	template<class Allocator>
	class MeshLibrary
	{
	public:
		Allocator*	mAllocator;

		MeshLibrary(Allocator* allocator);
		MeshLibrary();
		~MeshLibrary();

		void SetAllocator(Allocator* allocator);
		void NewMesh(IMesh** mesh, IRenderer* renderer);

		template<template<typename> class Resource, class Vertex>
		void LoadMesh(IMesh** mesh, IRenderer* renderer, Resource<Vertex>& resource);
	};

	template<class Allocator>
	MeshLibrary<Allocator>::MeshLibrary(Allocator* allocator) : mAllocator(allocator)
	{

	}

	template<class Allocator>
	MeshLibrary<Allocator>::MeshLibrary()
	{

	}

	template<class Allocator>
	MeshLibrary<Allocator>::~MeshLibrary()
	{
		mAllocator = nullptr;
	}

	template<class Allocator>
	void MeshLibrary<Allocator>::SetAllocator(Allocator* allocator)
	{
		mAllocator = allocator;
	}

	template<class Allocator>
	void MeshLibrary<Allocator>::NewMesh(IMesh** mesh, IRenderer* renderer)
	{
		(renderer->GetGraphicsAPI() == GRAPHICS_API_DIRECTX11) ? RIG_NEW(DX11Mesh, mAllocator, *mesh)() : RIG_NEW(DX11Mesh, mAllocator, *mesh)();
	}

	template<class Allocator>
	template<template<typename> class Resource, class Vertex>
	void MeshLibrary<Allocator>::LoadMesh(IMesh** mesh, IRenderer* renderer, Resource<Vertex>& resource)
	{
		resource.Load();

		(renderer->GetGraphicsAPI() == GRAPHICS_API_DIRECTX11) ? RIG_NEW(DX11Mesh, mAllocator, *mesh)() : RIG_NEW(DX11Mesh, mAllocator, *mesh)();
		renderer->VSetMeshVertexBufferData(*mesh, &resource.mVertices[0], sizeof(Vertex) * resource.mVertices.size(), sizeof(Vertex), GPU_MEMORY_USAGE_STATIC);
		renderer->VSetMeshIndexBufferData(*mesh, &resource.mIndices[0], resource.mIndices.size(), GPU_MEMORY_USAGE_STATIC);
	}
}


