#pragma once

#include <vector>
#include <algorithm>
#include <fstream>
#include <iostream>
#include "../Maths/VectorMaths.h"

namespace Physics
{
	typedef Vector2<int> Vector2i;

	template<typename NodeT, size_t widthT, size_t heightT>
	struct StaticRegularStorage
	{
		using Node = NodeT;
		const static size_t width = widthT;
		const static size_t height = heightT;
		using SelfType = StaticRegularStorage<NodeT, widthT, heightT>;
		StaticRegularStorage() {}
		StaticRegularStorage(const NodeT val)
		{
			for (size_t index = 0; index < width * height; index++)
			{
				nodes[index] = val;
			}
		}
		Node GetNode(Vector2i nodeIndex) const
		{
			return nodes[nodeIndex.x + nodeIndex.y * widthT];
		}
		void SetNode(Vector2i nodeIndex, const Node &val)
		{
			nodes[nodeIndex.x + nodeIndex.y * width] = val;
		}

		SelfType &operator += (const SelfType &other)
		{
			Vector2i nodeIndex;
			for (nodeIndex.y = 0; nodeIndex.y < height; nodeIndex.y++)
			{
				for (nodeIndex.x = 0; nodeIndex.x < width; nodeIndex.x++)
				{
					SetNode(nodeIndex, GetNode(nodeIndex) + other.GetNode(nodeIndex));
				}
			}
			return *this;
		}

		SelfType &operator *= (const NodeT &val)
		{
			Vector2i nodeIndex;
			for (nodeIndex.y = 0; nodeIndex.y < height; nodeIndex.y++)
			{
				for (nodeIndex.x = 0; nodeIndex.x < width; nodeIndex.x++)
				{
					SetNode(nodeIndex, GetNode(nodeIndex) * val);
				}
			}
			return *this;
		}

		SelfType &Fill(const NodeT &value)
		{
			Vector2i nodeIndex;
			for (nodeIndex.y = 0; nodeIndex.y < height; nodeIndex.y++)
			{
				for (nodeIndex.x = 0; nodeIndex.x < width; nodeIndex.x++)
				{
					SetNode(nodeIndex, value);
				}
			}
			return *this;
		}

		template<typename OtherStorage>
		NodeT Dot(Vector2i nodeIndex, const OtherStorage &pattern, Vector2i patternCenter) const
		{
			NodeT res = NodeT(0.0f);
			Vector2i matrixIndex;
			Vector2i startIndex = nodeIndex - patternCenter;
			Vector2i endIndex = nodeIndex - patternCenter + Vector2i(pattern.width, pattern.height);
			for (matrixIndex.y = std::max(0, startIndex.y); matrixIndex.y < std::min<int>(endIndex.y, height); matrixIndex.y++)
			{
				for (matrixIndex.x = std::max(0, startIndex.x); matrixIndex.x < std::min<int>(endIndex.x, width); matrixIndex.x++)
				{
					Vector2i patternIndex = matrixIndex - nodeIndex + patternCenter;
					res += (GetNode(matrixIndex) *= pattern.GetNode(patternIndex));
				}
			}
			return res;
		}

		template<typename OtherStorage, typename MultType>
		void MulAdd(Vector2i nodeIndex, const OtherStorage &pattern, Vector2i patternCenter, const MultType &mult)
		{
			Vector2i matrixIndex;
			Vector2i startIndex = nodeIndex - patternCenter;
			Vector2i endIndex = nodeIndex - patternCenter + Vector2i(pattern.width, pattern.height);
			for (matrixIndex.y = std::max(0, startIndex.y); matrixIndex.y < std::min<int>(endIndex.y, height); matrixIndex.y++)
			{
				for (matrixIndex.x = std::max(0, startIndex.x); matrixIndex.x < std::min<int>(endIndex.x, width); matrixIndex.x++)
				{
					Vector2i patternIndex = matrixIndex - nodeIndex + patternCenter;
					NodeT curr = GetNode(matrixIndex);
					NodeT add = mult;
					add.MulScalar(pattern.GetNode(patternIndex));
					curr += add;
					SetNode(matrixIndex, curr);
				}
			}
		}

		template<typename OtherStorage>
		void Mul(const OtherStorage &pattern)
		{
			Vector2i nodeIndex;
			for (nodeIndex.y = 0; nodeIndex.y < height; nodeIndex.y++)
			{
				for (nodeIndex.x = 0; nodeIndex.x < width; nodeIndex.x++)
				{
					SetNode(nodeIndex, GetNode(nodeIndex) * pattern.GetNode(nodeIndex));
				}
			}
		}

		template<typename Scalar>
		void MulScalar(const Scalar &scalar)
		{
			Vector2i nodeIndex;
			for (nodeIndex.y = 0; nodeIndex.y < height; nodeIndex.y++)
			{
				for (nodeIndex.x = 0; nodeIndex.x < width; nodeIndex.x++)
				{
					SetNode(nodeIndex, GetNode(nodeIndex) * scalar);
				}
			}
		}

		bool IsNull()
		{
			float sum = 0.0f;
			Vector2i nodeIndex;
			for (nodeIndex.y = 0; nodeIndex.y < height; nodeIndex.y++)
			{
				for (nodeIndex.x = 0; nodeIndex.x < width; nodeIndex.x++)
				{
					sum += abs(GetNode(nodeIndex));
				}
			}
			return sum < 1e-7f;
		}

		Node nodes[width * height];
	};

	template<typename NodeT>
	struct RegularStorage
	{
		typedef NodeT Node;
		const Node GetNode(Vector2i nodeIndex) const
		{
			return nodes[GetIndex(nodeIndex)];
		}
		const Node GetNode(size_t nodeIndex) const
		{
			return nodes[nodeIndex];
		}
		void SetNode(Vector2i nodeIndex, Node node)
		{
			nodes[GetIndex(nodeIndex)] = node;
		}
		void SetNode(size_t nodeIndex, Node node)
		{
			nodes[nodeIndex] = node;
		}

		Vector2<Node> GetGradient(Vector2i nodeIndex) const
		{
			return Vector2<Node>(
				(GetNode(nodeIndex + Vector2i(1, 0)) - GetNode(nodeIndex + Vector2i(-1, 0))) * 0.5f * invStep.x,
				(GetNode(nodeIndex + Vector2i(0, 1)) - GetNode(nodeIndex + Vector2i(0, -1))) * 0.5f * invStep.y);
		}
		Vector2<Node> GetAbsGradient(Vector2i nodeIndex) const
		{
			return Vector2<Node>(
				(abs(GetNode(nodeIndex + Vector2i(1, 0))) - abs(GetNode(nodeIndex + Vector2i(-1, 0)))) * 0.5f * invStep.x,
				(abs(GetNode(nodeIndex + Vector2i(0, 1))) - abs(GetNode(nodeIndex + Vector2i(0, -1)))) * 0.5f * invStep.y);
		}
		float GetDivergence(Vector2i nodeIndex) const
		{
			return
				(GetNode(nodeIndex + Vector2i(1, 0)).x - GetNode(nodeIndex + Vector2i(-1, 0)).x) * 0.5f * invStep.x +
				(GetNode(nodeIndex + Vector2i(0, 1)).y - GetNode(nodeIndex + Vector2i(0, -1)).y) * 0.5f * invStep.y;
		}
		float GetVorticity(Vector2i nodeIndex) const
		{
			return
				(GetNode(nodeIndex + Vector2i(1, 0)).y - GetNode(nodeIndex + Vector2i(-1, 0)).y) * 0.5f * invStep.x -
				(GetNode(nodeIndex + Vector2i(0, 1)).x - GetNode(nodeIndex + Vector2i(0, -1)).x) * 0.5f * invStep.y;
		}
		void Resize(Vector2i size, Vector2f step)
		{
			this->size = size;
			this->step = step;
			this->invStep = Vector2f(1.0f / step.x, 1.0f / step.y);
			nodes.resize(size.x * size.y);
		}
		size_t GetIndex(Vector2i nodeIndex) const
		{
			return nodeIndex.x + size.x * nodeIndex.y;
		}


		void Fill(const NodeT &value)
		{
			Vector2i nodeIndex;
			for (nodeIndex.y = 0; nodeIndex.y < size.y; nodeIndex.y++)
			{
				for (nodeIndex.x = 0; nodeIndex.x < size.x; nodeIndex.x++)
				{
					SetNode(nodeIndex, value);
				}
			}
		}

		template<typename OtherStorage>
		NodeT Dot(Vector2i nodeIndex, const OtherStorage &pattern, Vector2i patternCenter) const
		{
			NodeT res = NodeT(0.0f);
			Vector2i matrixIndex;
			Vector2i startIndex = nodeIndex - patternCenter;
			Vector2i endIndex = nodeIndex - patternCenter + Vector2i(pattern.width, pattern.height);
			for (matrixIndex.y = std::max(0, startIndex.y); matrixIndex.y < std::min<int>(endIndex.y, size.y); matrixIndex.y++)
			{
				for (matrixIndex.x = std::max(0, startIndex.x); matrixIndex.x < std::min<int>(endIndex.x, size.x); matrixIndex.x++)
				{
					Vector2i patternIndex = matrixIndex - nodeIndex + patternCenter;
					NodeT val = GetNode(matrixIndex);
					res += (val *= pattern.GetNode(patternIndex));
				}
			}
			return res;
		}
		std::vector<Node> nodes;
		Vector2i size;
		Vector2f step;
		Vector2f invStep;
	};



	template<typename NodeT>
	using Pattern3x3 = StaticRegularStorage<NodeT, 3, 3>;
}