#pragma once
#include <string>
#include <vector>
#include <unordered_set>
#include <map>
#include "Common/Transform.h"
#include "Memory/Memory/Memory.h"

#ifdef _WINDLL
#define RIG3D __declspec(dllexport)
#else
#define RIG3D __declspec(dllimport)
#endif

namespace Rig3D
{
	struct SceneGraphNode;
	
	class RIG3D SceneGraph
	{
	private:
		std::map<Transform*, SceneGraphNode*> map;
		PoolAllocator mAllocator;

		SceneGraphNode* GetNode(Transform* transform);
	public:

		SceneGraph(void* memoryBuffer, size_t size);
		~SceneGraph();

		inline void Add(std::string name, Transform* transform);
		inline void Add(std::string name, Transform* transform, Transform* parent);
		inline void Remove(Transform* transform, bool recursive = true);
		void SetParent(Transform* transform, Transform* parent);
		void GetChildren(Transform* transform, std::vector<Transform*> result);
		Transform* Find(std::string name);
	};

	struct SceneGraphNode
	{
		std::string name;
		Transform* transform;
		SceneGraphNode* parent;
		std::unordered_set<SceneGraphNode*> children;

		SceneGraphNode(std::string name, Transform* transform)
			: name(name), transform(transform), parent(nullptr) {}

		SceneGraphNode(const SceneGraphNode& other) :
			name(other.name),
			transform(other.transform),
			parent(other.parent),
			children(other.children)
		{}

		void SetParent(SceneGraphNode* newParent)
		{
			// clear previous parent dependences
			if (parent != nullptr)
			{
				parent->children.erase(this);
			}

			// update parent
			parent = newParent;

			// set new parent dependences
			if (parent != nullptr)
			{
				transform->SetParent(parent->transform);
				parent->children.insert(this);
			}
			else
			{
				transform->SetParent(nullptr);
			}
		}
	};

}