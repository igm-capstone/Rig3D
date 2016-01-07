#include "SceneGraph.h"
#include <list>

using namespace Rig3D;


SceneGraphNode* SceneGraph::GetNode(Transform* transform)
{
	auto node = map.find(transform);
	if (node == map.end())
	{
		return nullptr;
	}

	return node->second;
}

SceneGraph::SceneGraph(void* memoryBuffer, size_t size)
	: mAllocator(memoryBuffer, reinterpret_cast<char*>(memoryBuffer) + size, sizeof(SceneGraphNode)) { }


SceneGraph::~SceneGraph() { }


void SceneGraph::Add(std::string name, Transform* transform)
{
	auto node = map[transform];
	if (node == nullptr)
	{
		node = new (mAllocator.Allocate()) SceneGraphNode(name, transform);
		map[transform] = node;
	}
}


void SceneGraph::Add(std::string name, Transform* transform, Transform* parent)
{
	auto node = map[transform];
	if (node == nullptr)
	{
		node = new (mAllocator.Allocate()) SceneGraphNode(name, transform);
		map[transform] = node;
	}

	auto parentNode = GetNode(parent);
	node->SetParent(parentNode);
}


void SceneGraph::Remove(Transform* transform, bool recursive)
{
	auto node = GetNode(transform);
	if (node == nullptr)
	{
		return;
	}

	node->SetParent(nullptr);
	if (recursive)
	{
		auto list = std::list<SceneGraphNode*>();

		list.push_back(node);

		// what should be done with the memory allocated for all those transforms???
		auto it = list.begin();
		while (it != list.end())
		{
			auto current = *it;

			for (auto child : current->children)
			{
				list.push_back(child);
			}

			current->SetParent(nullptr);
			map.erase(current->transform);

			++it;
		}

		it = list.begin();
		while (it != list.end())
		{
			mAllocator.Free(*it);
			++it;
		}
	}
	else // remove and keep children
	{
		for (auto child : node->children)
		{
			child->SetParent(nullptr);
		}

		delete node;
	}
}


void SceneGraph::SetParent(Transform* transform, Transform* parent)
{
	auto node = GetNode(transform);
	auto parentNode = GetNode(parent);

	node->SetParent(parentNode);
}


void SceneGraph::GetChildren(Transform* transform, std::vector<Transform*> result)
{
	auto node = GetNode(transform);
	result.reserve(node->children.size());

	for (auto child : node->children)
	{
		result.push_back(child->transform);
	}
}


Transform* SceneGraph::Find(std::string name)
{
	// for later ;)

	return nullptr;
}