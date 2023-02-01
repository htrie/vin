#include "FluidSystem.h" 
#include <vector>
#include <algorithm>
#include <fstream>
#include <iostream>
#include "Storage.h"
#include "AlgebraicMultigridSolver.h"

namespace Physics
{
	typedef Vector2<int> Vector2i;


	struct FluidNode
	{
		FluidNode()
		{
			velocity = Vector2f(0.0f, 0.0f);
			pressure = 0.0f;
			substance = 0.0f;
		}
		Vector2f velocity;
		float pressure;
		float substance;
	};

	FluidNode MaxNode(FluidNode left, FluidNode right)
	{
		FluidNode res;
		res.velocity.x = std::max(left.velocity.x, right.velocity.x);
		res.velocity.y = std::max(left.velocity.y, right.velocity.y);
		res.pressure = std::max(left.pressure, right.pressure);
		res.substance = std::max(left.substance, right.substance);
		return res;
	}
	FluidNode MinNode(FluidNode left, FluidNode right)
	{
		FluidNode res;
		res.velocity.x = std::min(left.velocity.x, right.velocity.x);
		res.velocity.y = std::min(left.velocity.y, right.velocity.y);
		res.pressure = std::min(left.pressure, right.pressure);
		res.substance = std::min(left.substance, right.substance);
		return res;
	}
	FluidNode ClampNode(FluidNode node, FluidNode minNone, FluidNode maxNode)
	{
		return MinNode(MaxNode(node, minNone), maxNode);
	}

	FluidNode operator +(const FluidNode &left, const FluidNode &right)
	{
		FluidNode res;
		res.velocity = left.velocity + right.velocity;
		res.pressure = left.pressure + right.pressure;
		res.substance = left.substance + right.substance;
		return res;
	}

	FluidNode operator *(const FluidNode &node, float val)
	{
		FluidNode res;
		res.velocity = node.velocity * val;
		res.pressure = node.pressure * val;
		res.substance = node.substance * val;
		return res;
	}

	struct PressureNode
	{
		float pressure;
		float substance;
	};

	PressureNode operator +(const PressureNode &left, const PressureNode &right)
	{
		PressureNode res;
		res.pressure = left.pressure + right.pressure;
		res.substance = left.substance + right.substance;
		return res;
	}

	PressureNode operator *(const PressureNode &node, float val)
	{
		PressureNode res;
		res.pressure = node.pressure * val;
		res.substance = node.substance * val;
		return res;
	}

	struct InternalNodeStaticData
	{
		float boundaryDist;
		float source;
		Vector2f velocity;

		InternalNodeStaticData()
		{
			boundaryDist = 0.0f;
			source = 0.0f;
			velocity = Vector2f(0.0f, 0.0f);
		}
		InternalNodeStaticData operator+(InternalNodeStaticData other) const
		{
			InternalNodeStaticData res;
			res.boundaryDist = this->boundaryDist + other.boundaryDist;
			res.source = this->source + other.source;
			res.velocity = this->velocity + other.velocity;
			return res;
		}
		InternalNodeStaticData operator*(float val) const
		{
			InternalNodeStaticData res;
			res.boundaryDist = this->boundaryDist * val;
			res.source = this->source * val;
			res.velocity = this->velocity * val;
			return res;
		}
	};

	struct FluidRegularGrid
	{
		typedef FluidNode Node;
		const Node GetNode(Vector2i nodeIndex) const
		{
			Node res;
			size_t index = pressure.GetIndex(nodeIndex);
			assert(index == substance.GetIndex(nodeIndex));
			assert(index == velocity.GetIndex(nodeIndex));
			res.pressure = pressure.GetNode(index);
			res.substance = substance.GetNode(index);
			res.velocity = velocity.GetNode(index);
			return res;
		}
		void SetNode(Vector2i nodeIndex, Node node)
		{
			size_t index = pressure.GetIndex(nodeIndex);
			assert(index == substance.GetIndex(nodeIndex));
			assert(index == velocity.GetIndex(nodeIndex));
			pressure.SetNode(index, node.pressure);
			substance.SetNode(index, node.substance);
			velocity.SetNode(index, node.velocity);
		}
		Vector2f GetPressureGradient(Vector2i nodeIndex)
		{
			return pressure.GetGradient(nodeIndex);
		}
		float GetVelocityDivergence(Vector2i nodeIndex)
		{
			return velocity.GetDivergence(nodeIndex);
		}
		void Resize(Vector2i size, Vector2f step)
		{
			this->size = size;
			this->step = step;
			this->invStep = Vector2f(1.0f / step.x, 1.0f / step.y);
			pressure.Resize(size, step);
			substance.Resize(size, step);
			velocity.Resize(size, step);
		}
		RegularStorage<float> pressure;
		RegularStorage<float> substance;
		RegularStorage<Vector2f> velocity;

		Vector2i size;
		Vector2f invStep;
		Vector2f step;
	};

	template<typename Storage>
	typename Storage::Node Interpolate(const Storage &storage, Vector2f pos)
	{
		Vector2f low = Vector2f(floor(pos.x), floor(pos.y));
		Vector2f ratio = pos - low;

		int lowx = int(low.x);
		int lowy = int(low.y);

		int highx = lowx + 1;
		int highy = lowy + 1;

		auto CorrectCoords = [&storage](int &x, int &y)
		{
			x = std::max(0, std::min(x, storage.size.x - 1));
			y = std::max(0, std::min(y, storage.size.y - 1));
		};
		/*auto CorrectCoords = [&storage](int &x, int &y)
		{
			x = (x + storage.size.x) % storage.size.x;
			y = (y + storage.size.y) % storage.size.y;
		};*/
		CorrectCoords(lowx, lowy);
		CorrectCoords(highx, highy);

		typename Storage::Node res =
			storage.GetNode(Vector2i(lowx, lowy)) * (1.0f - ratio.x) * (1.0f - ratio.y) +
			storage.GetNode(Vector2i(highx, lowy)) * (ratio.x) * (1.0f - ratio.y) +
			storage.GetNode(Vector2i(highx, highy)) * (ratio.x) * (ratio.y) +
			storage.GetNode(Vector2i(lowx, highy)) * (1.0f - ratio.x) * (ratio.y);

		return res;
	}
	struct StaggeredAlignedGrid
	{
		static Vector2f GetXVelocityNodeOffset()
		{
			return Vector2f(0.0f, 0.5f);
		}
		static Vector2f GetYVelocityNodeOffset()
		{
			return Vector2f(0.5f, 0.0f);
		}
		static Vector2f GetPressureNodeOffset()
		{
			return Vector2f(0.5f, 0.5f);
		}
		static Vector2f GetBoundaryDistNodeOffset()
		{
			return Vector2f(0.0f, 0.0f);
		}
		float GetPressureXGradient(Vector2i xVelocityNode)
		{
			return (pressure.GetNode(xVelocityNode) - pressure.GetNode(xVelocityNode + Vector2i(-1, 0))) * invStep.x;
		}
		float GetPressureYGradient(Vector2i yVelocityNode)
		{
			return (pressure.GetNode(yVelocityNode) - pressure.GetNode(yVelocityNode + Vector2i(0, -1))) * invStep.y;
		}
		Vector2f GetVelocityPressureNode(Vector2i pressureNode)
		{
			return Vector2f(
				(xVelocity.GetNode(pressureNode) + xVelocity.GetNode(pressureNode + Vector2i(1, 0))) * 0.5f,
				(yVelocity.GetNode(pressureNode) + yVelocity.GetNode(pressureNode + Vector2i(0, 1))) * 0.5f);
		}
		Vector2f GetVelocityXVelocityNode(Vector2i xVelocityNode)
		{
			return Vector2f(
				xVelocity.GetNode(xVelocityNode),
				(yVelocity.GetNode(xVelocityNode) + yVelocity.GetNode(xVelocityNode + Vector2i(-1, 0)) +
					yVelocity.GetNode(xVelocityNode + Vector2i(0, 1)) + yVelocity.GetNode(xVelocityNode + Vector2i(-1, 1))) * 0.25f);
		}
		Vector2f GetVelocityYVelocityNode(Vector2i yVelocityNode)
		{
			return Vector2f(
				(xVelocity.GetNode(yVelocityNode) + xVelocity.GetNode(yVelocityNode + Vector2i(0, -1)) +
					xVelocity.GetNode(yVelocityNode + Vector2i(1, 0)) + xVelocity.GetNode(yVelocityNode + Vector2i(1, -1))) * 0.25f,
				yVelocity.GetNode(yVelocityNode));
		}
		float GetVelocityDivergence(Vector2i pressureNode)
		{
			float xDivergence = (xVelocity.GetNode(pressureNode + Vector2i(1, 0)) - xVelocity.GetNode(pressureNode)) * invStep.x;
			float yDivergence = (yVelocity.GetNode(pressureNode + Vector2i(0, 1)) - yVelocity.GetNode(pressureNode)) * invStep.y;
			return
				(xVelocity.GetNode(pressureNode + Vector2i(1, 0)) - xVelocity.GetNode(pressureNode)) * invStep.x +
				(yVelocity.GetNode(pressureNode + Vector2i(0, 1)) - yVelocity.GetNode(pressureNode)) * invStep.y;
		}

		Vector2f GetScatteredVelocity(Vector2i nodeIndex)
		{
			return Vector2f(xVelocity.GetNode(nodeIndex), yVelocity.GetNode(nodeIndex));
		}
		void SetScatteredVelocity(Vector2i nodeIndex, Vector2f velocity)
		{
			xVelocity.SetNode(nodeIndex, velocity.x);
			yVelocity.SetNode(nodeIndex, velocity.y);
		}

		FluidNode GetScatteredNode(Vector2i nodeIndex) const
		{
			FluidNode res;
			res.pressure = pressure.GetNode(nodeIndex);
			res.velocity = Vector2f(xVelocity.GetNode(nodeIndex), yVelocity.GetNode(nodeIndex));
			res.substance = substance.GetNode(nodeIndex);
			return res;
		}

		void SetScatteredNode(Vector2i nodeIndex, FluidNode node)
		{
			pressure.SetNode(nodeIndex, node.pressure);
			xVelocity.SetNode(nodeIndex, node.velocity.x);
			yVelocity.SetNode(nodeIndex, node.velocity.y);
			substance.SetNode(nodeIndex, node.substance);
		}

		void Resize(Vector2i size, Vector2f areaSize)
		{
			this->size = size;
			this->step = Vector2f(areaSize.x / size.x, areaSize.y / size.y);
			this->invStep = Vector2f(1.0f / step.x, 1.0f / step.y);
			pressure.Resize(size, step);
			substance.Resize(size, step);
			xVelocity.Resize(size, step);
			yVelocity.Resize(size, step);
		}

		RegularStorage<float> pressure;
		RegularStorage<float> substance;
		RegularStorage<float> xVelocity;
		RegularStorage<float> yVelocity;
		Vector2i size;
		Vector2f invStep;
		Vector2f step;
	};

	struct StaggeredMisalignedGrid
	{
		static Vector2f GetXVelocityOffset()
		{
            return Vector2f();
		}
		static Vector2f GetYVelocityOffset()
		{
            return Vector2f();
		}
		static Vector2f GetPressureOffset()
		{
            return Vector2f();
		}
		float GetXPressureGradient(Vector2i xVelocityNode)
		{
            return 0;
		}
		float GetYPressureGradient(Vector2i yVelocityNode)
		{
            return 0;
		}
		Vector2f GetVelocityPressureNode(Vector2i pressureNode)
		{
            return Vector2f();
		}
		Vector2f GetVelocityXVelocityNode(Vector2i xVelocityNode)
		{
            return Vector2f();
		}
		Vector2f GetVelocityYVelocityNode(Vector2i yVelocityNode)
		{
            return Vector2f();
		}
		float GetVelocityDivergence(Vector2i pressureNode)
		{
            return 0;
		}
	};


	template<typename StaggeredGridType>
	struct StaggeredGridSolver
	{
		template<typename Func>
		void SetDynamicData(Func f)
		{
			Vector2i nodeIndex;
			for (nodeIndex.y = 0; nodeIndex.y < size.y; nodeIndex.y++)
			{
				for (nodeIndex.x = 0; nodeIndex.x < size.x; nodeIndex.x++)
				{
					FluidSystem::NodeDynamicData data = f(nodeIndex);

					FluidNode fluidNode;
					fluidNode.pressure = data.pressure;
					fluidNode.velocity = data.velocity;
					fluidNode.substance = data.substance;
					grid.SetScatteredNode(nodeIndex, fluidNode);
					nextGrid.SetScatteredNode(nodeIndex, fluidNode);
					tmpGrid.SetScatteredNode(nodeIndex, fluidNode);
				}
			}
		}

		void GetDynamicData(FluidSystem::NodeDynamicData *nodeData)
		{
			Vector2i nodeIndex;
			for (nodeIndex.y = 0; nodeIndex.y < size.y; nodeIndex.y++)
			{
				for (nodeIndex.x = 0; nodeIndex.x < size.x; nodeIndex.x++)
				{
					if (nodeIndex.x > 0 && nodeIndex.y > 0 && (nodeIndex.x < size.x - 1) && (nodeIndex.y < size.y - 1))
						nodeData[nodeIndex.x + nodeIndex.y * size.x].velocity = grid.GetVelocityPressureNode(nodeIndex);
					else
						nodeData[nodeIndex.x + nodeIndex.y * size.x].velocity = Vector2f(0.0f, 0.0f);
					nodeData[nodeIndex.x + nodeIndex.y * size.x].pressure = grid.pressure.GetNode(nodeIndex);
					nodeData[nodeIndex.x + nodeIndex.y * size.x].substance = grid.substance.GetNode(nodeIndex);
				}
			}
		}

		void GetStaticData(FluidSystem::NodeStaticData *nodeData)
		{
			Vector2i nodeIndex;
			for (nodeIndex.y = 0; nodeIndex.y < size.y; nodeIndex.y++)
			{
				for (nodeIndex.x = 0; nodeIndex.x < size.x; nodeIndex.x++)
				{
					auto data = staticData.GetNode(nodeIndex);
					nodeData[nodeIndex.x + nodeIndex.y * size.x].nodeType = (IsSolid(data.boundaryDist)) ? FluidSystem::NodeTypes::FixedVelocity : FluidSystem::NodeTypes::Free;
					nodeData[nodeIndex.x + nodeIndex.y * size.x].source = data.source;
				}
			}
		}

		template<typename Func>
		void SetStaticData(Func f)
		{
			Vector2i nodeIndex;
			for (nodeIndex.y = 0; nodeIndex.y < size.y; nodeIndex.y++)
			{
				for (nodeIndex.x = 0; nodeIndex.x < size.x; nodeIndex.x++)
				{
					FluidSystem::NodeStaticData data = f(nodeIndex);

					InternalNodeStaticData internalData;
					//internalData.boundaryDist = (nodeIndex.x > 4 && nodeIndex.y > 4) ? -1.0f : 1.0f;
					internalData.boundaryDist = (data.nodeType == FluidSystem::NodeTypes::FixedVelocity) ? 1.0f : -1.0f;
					//internalData.boundaryDist = -1.0f;
					internalData.source = data.source;
					if (nodeIndex.x > 0 && nodeIndex.y > 0 && (nodeIndex.x < size.x - 1) && (nodeIndex.y < size.y - 1))
						internalData.velocity = grid.GetVelocityPressureNode(nodeIndex);
					else
						internalData.velocity = Vector2f(0.0f, 0.0f);
					staticData.SetNode(nodeIndex, internalData);
				}
			}
			BuildBoundaryNodes2();

			SetupPressureMultigridSolver();
		}

		void SetupPressureMultigridSolver()
		{
			Vector2i offsets[9];
			offsets[0] = Vector2i(-1, 0);
			offsets[1] = Vector2i(1, 0);
			offsets[2] = Vector2i(0, -1);
			offsets[3] = Vector2i(0, 1);

			offsets[4] = Vector2i(1, 1);
			offsets[5] = Vector2i(-1, 1);
			offsets[6] = Vector2i(-1, -1);
			offsets[7] = Vector2i(1, -1);
			offsets[8] = Vector2i(0, 0);

			Vector2i nodeIndex;
			for (nodeIndex.y = 1; nodeIndex.y < size.y - 1; nodeIndex.y++)
			{
				for (nodeIndex.x = 1; nodeIndex.x < size.x - 1; nodeIndex.x++)
				{
					Pattern3x3<float> equationMatrix(0.0f);

					float stepMult = 1.0f / ((step.x * step.x + step.y * step.y) * 0.5f);
					if (IsSolid(GetPressureNodeBoundaryDist(nodeIndex)))
					{
						int freeOffsetsCount = 0;
						for (size_t offsetIndex = 0; offsetIndex < 4; offsetIndex++)
						{
							if (!IsSolid(GetPressureNodeBoundaryDist(nodeIndex + offsets[offsetIndex])))
							{
								freeOffsetsCount++;
								equationMatrix.SetNode(offsets[offsetIndex] + Vector2i(1, 1), 1.0f * stepMult);
							}
						}
						if (freeOffsetsCount > 0)
							equationMatrix.SetNode(Vector2i(1, 1), -float(freeOffsetsCount) * stepMult);
					}
					else
					{
						for (size_t offsetIndex = 0; offsetIndex < 4; offsetIndex++)
						{
							equationMatrix.SetNode(offsets[offsetIndex] + Vector2i(1, 1), 1.0f * stepMult);
						}
						equationMatrix.SetNode(Vector2i(1, 1), -4.0f * stepMult);
					}

					pressureMultigridSolver.SetEquationMatrix(nodeIndex, equationMatrix);
				}
			}
			pressureMultigridSolver.SetupLayers();
		}

		bool IsSolid(float boundaryDist) const
		{
			return boundaryDist > 0.99f;
		}

		void AdvectGrid(StaggeredGridType &srcGrid, StaggeredGridType &dstGrid, float timeOffset)
		{
			float sizeMult = float(size.x) / float(areaSize.x);
			Vector2i nodeIndex;
			//for (auto nodeIndex : internalNodes)
			//Vector2i nodeIndex;
			for (nodeIndex.y = 1; nodeIndex.y < size.y - 1; nodeIndex.y++)
			{
				for (nodeIndex.x = 1; nodeIndex.x < size.x - 1; nodeIndex.x++)
				{
					{
						Vector2f velocity = srcGrid.GetVelocityPressureNode(nodeIndex);
						Vector2f pos = Vector2f(float(nodeIndex.x), float(nodeIndex.y));
						dstGrid.pressure.SetNode(nodeIndex, Interpolate(srcGrid.pressure, pos - velocity * timeOffset * sizeMult));
						dstGrid.substance.SetNode(nodeIndex, Interpolate(srcGrid.substance, pos - velocity * timeOffset * sizeMult));
					}
					{
						Vector2f velocity = srcGrid.GetVelocityXVelocityNode(nodeIndex);
						Vector2f pos = Vector2f(float(nodeIndex.x), float(nodeIndex.y));
						dstGrid.xVelocity.SetNode(nodeIndex, Interpolate(srcGrid.xVelocity, pos - velocity * timeOffset * sizeMult));
					}
					{
						Vector2f velocity = srcGrid.GetVelocityYVelocityNode(nodeIndex);
						Vector2f pos = Vector2f(float(nodeIndex.x), float(nodeIndex.y));
						dstGrid.yVelocity.SetNode(nodeIndex, Interpolate(srcGrid.yVelocity, pos - velocity * timeOffset * sizeMult));
					}
				}
			}
		}

		void Advect(float dt)
		{
			Vector2i nodeIndex;

			AdvectGrid(grid, nextGrid, dt);

			std::swap(nextGrid.pressure, grid.pressure);
			std::swap(nextGrid.substance, grid.substance);
			std::swap(nextGrid.xVelocity, grid.xVelocity);
			std::swap(nextGrid.yVelocity, grid.yVelocity);

			/*AdvectGrid(nextGrid, tmpGrid, -dt);
			for (auto nodeIndex : internalNodes)
			{
				FluidNode backwardNode = tmpGrid.GetScatteredNode(nodeIndex);;
				FluidNode initNode = grid.GetScatteredNode(nodeIndex);
				FluidNode nextNode = nextGrid.GetScatteredNode(nodeIndex);

				FluidNode modifiedNode = initNode + (initNode + backwardNode * -1.0f) * 0.5f;

				FluidNode minNode = nextGrid.GetScatteredNode(nodeIndex);
				FluidNode maxNode = nextGrid.GetScatteredNode(nodeIndex);
				minNode = MinNode(minNode, nextGrid.GetScatteredNode(nodeIndex + Vector2i(1, 0)));
				maxNode = MaxNode(maxNode, nextGrid.GetScatteredNode(nodeIndex + Vector2i(1, 0)));
				minNode = MinNode(minNode, nextGrid.GetScatteredNode(nodeIndex + Vector2i(-1, 0)));
				maxNode = MaxNode(maxNode, nextGrid.GetScatteredNode(nodeIndex + Vector2i(-1, 0)));
				minNode = MinNode(minNode, nextGrid.GetScatteredNode(nodeIndex + Vector2i(0, 1)));
				maxNode = MaxNode(maxNode, nextGrid.GetScatteredNode(nodeIndex + Vector2i(0, 1)));
				minNode = MinNode(minNode, nextGrid.GetScatteredNode(nodeIndex + Vector2i(0, -1)));
				maxNode = MaxNode(maxNode, nextGrid.GetScatteredNode(nodeIndex + Vector2i(0, -1)));
				modifiedNode = ClampNode(modifiedNode, minNode, maxNode);

				tmpGrid.SetScatteredNode(nodeIndex, modifiedNode);
			}
			AdvectGrid(tmpGrid, grid, dt);*/

			SetBounds();
		}

		void IterateProjection(size_t iterationsCount)
		{
			Vector2i nodeIndex;
			for (nodeIndex.y = 1; nodeIndex.y < size.y - 1; nodeIndex.y++)
			{
				for (nodeIndex.x = 1; nodeIndex.x < size.x - 1; nodeIndex.x++)
				{
					pressureMultigridSolver.GetValues().SetNode(nodeIndex, grid.pressure.GetNode(nodeIndex));

					float rightSide;
					if (IsSolid(GetPressureNodeBoundaryDist(nodeIndex)))
						rightSide = 0.0f;
					else
						rightSide = (grid.GetVelocityDivergence(nodeIndex) - staticData.GetNode(nodeIndex).source);
					pressureMultigridSolver.GetRightSides().SetNode(nodeIndex, rightSide);
				}
			}

			pressureMultigridSolver.Solve(5);

			for (nodeIndex.y = 1; nodeIndex.y < size.y - 1; nodeIndex.y++)
			{
				for (nodeIndex.x = 1; nodeIndex.x < size.x - 1; nodeIndex.x++)
				{
					grid.pressure.SetNode(nodeIndex, pressureMultigridSolver.GetValues().GetNode(nodeIndex));
				}
			}

			ApplyPressureGradient();
		}
		void IterateDiffusion(float dt, size_t iterationsCount)
		{
			//gauss-seidel
			//https://www.duo.uio.no/bitstream/handle/10852/9752/Moastuen.pdf?sequence=1
			float viscosity = 1000.0f;// 0.00001f;
			float mult = (step.x * step.x + step.y * step.y) * 0.5f / (viscosity * dt);
			Vector2i nodeIndex;
			for (int i = 0; i < iterationsCount * 2; i++)
			{
				float relaxation = 1.0f + (1.0f - float(i / 2) / float(iterationsCount - 1)) * 0.5f;
				//for (auto nodeIndex : internalNodes)
				Vector2i nodeIndex;
				for (nodeIndex.y = 1; nodeIndex.y < size.y - 1; nodeIndex.y++)
				{
					for (nodeIndex.x = 1; nodeIndex.x < size.x - 1; nodeIndex.x++)
					{
						if (IsSolid(GetPressureNodeBoundaryDist(nodeIndex)))
							continue;
						if ((nodeIndex.x % 2) ^ (nodeIndex.y % 2) ^ (i % 2))
							continue;

						grid.SetScatteredVelocity(nodeIndex, (
							grid.GetScatteredVelocity(nodeIndex + Vector2i(-1, 0)) +
							grid.GetScatteredVelocity(nodeIndex + Vector2i(1, 0)) +
							grid.GetScatteredVelocity(nodeIndex + Vector2i(0, -1)) +
							grid.GetScatteredVelocity(nodeIndex + Vector2i(0, 1)) +
							grid.GetScatteredVelocity(nodeIndex) * mult) * (1.0f / (4.0f + mult)));
					}
				}
				SetBounds();
			}
		}

		void ApplyPressureGradient()
		{
			Vector2i nodeIndex;

			//#pragma omp parallel for
			//SetBounds();

			for (auto nodeIndex : internalXVelocityNodes)
			{
				float xVelocity = grid.xVelocity.GetNode(nodeIndex);
				xVelocity -= grid.GetPressureXGradient(nodeIndex); //
				grid.xVelocity.SetNode(nodeIndex, xVelocity);
			}

			for(auto nodeIndex : internalYVelocityNodes)
			{
				float yVelocity = grid.yVelocity.GetNode(nodeIndex);
				yVelocity -= grid.GetPressureYGradient(nodeIndex);
				grid.yVelocity.SetNode(nodeIndex, yVelocity);
			}
			/*for (auto nodeIndex : internalPressureNodes)
			{
				grid.pressure.SetNode(nodeIndex, grid.pressure.GetNode(nodeIndex) * 0.99f);
			}*/
			SetBounds();
		}

		void SetBounds(bool continuousDerivative = true)
		{
			for (auto boundaryNode : boundaryPressureNodes)
			{
				Vector2i nodeIndex = boundaryNode.nodeIndex;
				float sumWeight = 0.0f;
				float sumPressure = 0.0f;
				for (size_t offsetIndex = 0; offsetIndex < boundaryNode.freeOffsetsCount; offsetIndex++)
				{
					Vector2i offsetNodeIndex = nodeIndex + boundaryNode.freeOffsets[offsetIndex];
					float weight = 1.0f;
					sumPressure += grid.pressure.GetNode(offsetNodeIndex) * weight;
					sumWeight += weight;
				}
				//grid.pressure.SetNode(nodeIndex, sumPressure / sumWeight);

				grid.xVelocity.SetNode(nodeIndex, 0.0f);
				grid.xVelocity.SetNode(nodeIndex + Vector2i(1, 0), 0.0f);
				grid.yVelocity.SetNode(nodeIndex, 0.0f);
				grid.yVelocity.SetNode(nodeIndex + Vector2i(0, 1), 0.0f);
			}

			/*for (auto boundaryNode : boundaryXVelocityNodes)
			{
				Vector2i nodeIndex = boundaryNode.nodeIndex;
				float sumWeight = 0.0f;
				float sumVelocity = 0.0f;
				for (size_t offsetIndex = 0; offsetIndex < boundaryNode.freeOffsetsCount; offsetIndex++)
				{
					Vector2i offsetNodeIndex = nodeIndex + boundaryNode.freeOffsets[offsetIndex];
					float weight = 1.0f;
					sumVelocity += grid.xVelocity.GetNode(offsetNodeIndex) * weight;
					sumWeight += weight;
				}
				grid.xVelocity.SetNode(nodeIndex, 2.0f * 0.0f - sumVelocity / sumWeight);
			}
			for (auto boundaryNode : boundaryYVelocityNodes)
			{
				Vector2i nodeIndex = boundaryNode.nodeIndex;
				float sumWeight = 0.0f;
				float sumVelocity = 0.0f;
				for (size_t offsetIndex = 0; offsetIndex < boundaryNode.freeOffsetsCount; offsetIndex++)
				{
					Vector2i offsetNodeIndex = nodeIndex + boundaryNode.freeOffsets[offsetIndex];
					float weight = 1.0f;
					sumVelocity += grid.yVelocity.GetNode(offsetNodeIndex) * weight;
					sumWeight += weight;
				}
				grid.yVelocity.SetNode(nodeIndex, 2.0f * 0.0f - sumVelocity / sumWeight);
			}*/
		}
		
		float GetSubstance(Vector2i pos) const
		{
			return grid.substance.GetNode(pos);
		}
		Vector2<float> GetSubstanceGradient(Vector2i pos) const
		{
			return grid.substance.GetGradient(pos);
		}
		Vector2<float> GetPressureGradient(Vector2i pos) const
		{
			return grid.pressure.GetGradient(pos);
		}
		float GetPressure(Vector2i pos) const
		{
			return grid.pressure.GetNode(pos);
		}
		bool IsSolidNode(Vector2i pos) const
		{
			//return IsSolid(staticData.GetNode(pos).boundaryDist);
			return IsSolid(GetPressureNodeBoundaryDist(pos));
		}

		void Resize(Vector2i size, Vector2f areaSize)
		{
			this->size = size;
			this->areaSize = areaSize;
			this->step = Vector2f(areaSize.x / size.x, areaSize.y / size.y);
			this->invStep = Vector2f(1.0f / step.x, 1.0f / step.y);

			grid.Resize(size, areaSize);
			nextGrid.Resize(size, areaSize);
			tmpGrid.Resize(size, areaSize);

			staticData.Resize(size, areaSize);

			pressureMultigridSolver.Resize(size);
		}
	private:
		static Vector2f ToVector2f(Vector2i index)
		{
			return Vector2f(float(index.x), float(index.y));
		}
		float GetPressureNodeBoundaryDist(Vector2i pressureNodeIndex) const
		{
			return Interpolate(staticData, ToVector2f(pressureNodeIndex) + grid.GetPressureNodeOffset() - grid.GetBoundaryDistNodeOffset()).boundaryDist;
		}
		float GetXVelocityBoundaryDist(Vector2i xVelocityNodeIndex) const
		{
			return Interpolate(staticData, ToVector2f(xVelocityNodeIndex) + grid.GetXVelocityNodeOffset() - grid.GetBoundaryDistNodeOffset()).boundaryDist;
		}
		float GetYVelocityBoundaryDist(Vector2i yVelocityNodeIndex) const
		{
			return Interpolate(staticData, ToVector2f(yVelocityNodeIndex) + grid.GetYVelocityNodeOffset() - grid.GetBoundaryDistNodeOffset()).boundaryDist;
		}

		void BuildBoundaryNodes2()
		{
			internalPressureNodes.clear();
			internalXVelocityNodes.clear();
			internalYVelocityNodes.clear();

			Vector2i offsets[9];
			offsets[0] = Vector2i(-1, 0);
			offsets[1] = Vector2i(1, 0);
			offsets[2] = Vector2i(0, -1);
			offsets[3] = Vector2i(0, 1);

			offsets[4] = Vector2i(1, 1);
			offsets[5] = Vector2i(-1, 1);
			offsets[6] = Vector2i(-1, -1);
			offsets[7] = Vector2i(1, -1);
			offsets[8] = Vector2i(0, 0);

			Vector2i nodeIndex;
			for (nodeIndex.y = 1; nodeIndex.y < size.y - 1; nodeIndex.y++)
			{
				for (nodeIndex.x = 1; nodeIndex.x < size.x - 1; nodeIndex.x++)
				{
					bool isScatteredNodeInternal = false;
					if (IsSolid(GetPressureNodeBoundaryDist(nodeIndex)))
					{
						BoundaryPressureNode boundaryPressureNode;
						boundaryPressureNode.nodeIndex = nodeIndex;
						boundaryPressureNode.freeOffsetsCount = 0;
						for (size_t offsetIndex = 0; offsetIndex < 4; offsetIndex++)
						{
							if (!IsSolid(GetPressureNodeBoundaryDist(nodeIndex + offsets[offsetIndex])))
								boundaryPressureNode.freeOffsets[boundaryPressureNode.freeOffsetsCount++] = offsets[offsetIndex];
						}
						if (boundaryPressureNode.freeOffsetsCount > 0)
							boundaryPressureNodes.push_back(boundaryPressureNode);
					}
					else
					{
						internalPressureNodes.push_back(nodeIndex);
						isScatteredNodeInternal = true;
					}

					if (IsSolid(GetXVelocityBoundaryDist(nodeIndex)))
					{
						BoundaryVelocityNode xVelocityBoundaryNode;
						xVelocityBoundaryNode.nodeIndex = nodeIndex;
						xVelocityBoundaryNode.freeOffsetsCount = 0;
						for (size_t offsetIndex = 0; offsetIndex < 2; offsetIndex++)
						{
							if (!IsSolid(GetXVelocityBoundaryDist(nodeIndex + offsets[offsetIndex])))
								xVelocityBoundaryNode.freeOffsets[xVelocityBoundaryNode.freeOffsetsCount++] = offsets[offsetIndex];
						}
						if (xVelocityBoundaryNode.freeOffsetsCount > 0)
							boundaryXVelocityNodes.push_back(xVelocityBoundaryNode);
					}
					else
					{
						internalXVelocityNodes.push_back(nodeIndex);
						isScatteredNodeInternal = true;
					}

					if (IsSolid(GetYVelocityBoundaryDist(nodeIndex)))
					{
						BoundaryVelocityNode yVelocityBoundaryNode;
						yVelocityBoundaryNode.nodeIndex = nodeIndex;
						yVelocityBoundaryNode.freeOffsetsCount = 0;
						for (size_t offsetIndex = 2; offsetIndex < 4; offsetIndex++)
						{
							if (!IsSolid(GetYVelocityBoundaryDist(nodeIndex + offsets[offsetIndex])))
								yVelocityBoundaryNode.freeOffsets[yVelocityBoundaryNode.freeOffsetsCount++] = offsets[offsetIndex];
						}
						if (yVelocityBoundaryNode.freeOffsetsCount > 0)
							boundaryYVelocityNodes.push_back(yVelocityBoundaryNode);
					}
					else
					{
						internalYVelocityNodes.push_back(nodeIndex);
						isScatteredNodeInternal = true;
					}

					if (isScatteredNodeInternal)
						internalScatteredNodes.push_back(nodeIndex);
				}
			}
		}
		StaggeredGridType grid;
		StaggeredGridType nextGrid;
		StaggeredGridType tmpGrid;

		RegularStorage<InternalNodeStaticData> staticData;

		AlgebraicMultigridSolver<float> pressureMultigridSolver;

		Vector2i size;
		Vector2f step;
		Vector2f invStep;
		Vector2f areaSize;

		struct BoundaryPressureNode
		{
			Vector2i nodeIndex;
			Vector2i freeOffsets[4];
			size_t freeOffsetsCount;
		};
		std::vector<BoundaryPressureNode> boundaryPressureNodes;

		struct BoundaryVelocityNode
		{
			Vector2i nodeIndex;
			Vector2i freeOffsets[2];
			size_t freeOffsetsCount;
		};
		std::vector<BoundaryVelocityNode> boundaryXVelocityNodes;
		std::vector<BoundaryVelocityNode> boundaryYVelocityNodes;

		std::vector<Vector2i> internalScatteredNodes;
		std::vector<Vector2i> internalPressureNodes;
		std::vector<Vector2i> internalXVelocityNodes;
		std::vector<Vector2i> internalYVelocityNodes;
	};

  template<typename Solver>
  class ExperimentalSystem : public FluidSystem
  {
  public:
    ExperimentalSystem() : relaxationIterations(5) {}
    virtual ~ExperimentalSystem() {}
    void SetSolverParams(size_t relaxationIterations) override
    {
      this->relaxationIterations = relaxationIterations;
    }

    void SetSize(Vector2i size, Vector2f areaSize) override
    {
      this->size = size;
      this->areaSize = areaSize;

      solver.Resize(size, areaSize);
    }

    void SetDynamicData(const NodeDynamicData *srcNodeData) override
    {
      solver.SetDynamicData([&](Vector2i gridPos) -> NodeDynamicData
      {
        return srcNodeData[size.x * gridPos.y + gridPos.x];
      });
    }
    void GetDynamicData(NodeDynamicData *dstNodeData) override
    {
      solver.GetDynamicData(dstNodeData);
    }
    void SetStaticData(const NodeStaticData *srcNodeData) override
    {
      solver.SetStaticData([&](Vector2i gridPos) -> NodeStaticData
      {
        return srcNodeData[size.x * gridPos.y + gridPos.x];
      });
    }

    void Update(float dt) override
    {
      solver.Advect(dt);
      solver.IterateDiffusion(dt, 1);
      solver.IterateProjection(relaxationIterations);
    }
  private:
    size_t relaxationIterations;
    Solver solver;
    Vector2i size;
    Vector2f areaSize;
  };

	std::unique_ptr<FluidSystem> FluidSystem::Create()
	{
		using Grid = StaggeredAlignedGrid;
		using Solver = StaggeredGridSolver<Grid>;
		using System = ExperimentalSystem<Solver>;
		return std::unique_ptr<FluidSystem>(new System());
	}
}
