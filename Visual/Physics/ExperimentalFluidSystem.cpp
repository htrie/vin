#include "FluidSystem.h" 

namespace Physics
{
	typedef Vector2<int> Vector2i;

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
		std::vector<Node> nodes;
		Vector2i size;
		Vector2f step;
		Vector2f invStep;
	};

	struct FluidNode
	{
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

	struct NodeStaticData
	{
		bool isStatic;
		bool isBoundary;
		float source;
		Vector2f velocity;
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
		CorrectCoords(lowx, lowy);
		CorrectCoords(highx, highy);

		typename Storage::Node res =
			storage.GetNode(Vector2i(lowx, lowy)) * (1.0f - ratio.x) * (1.0f - ratio.y) +
			storage.GetNode(Vector2i(highx, lowy)) * (ratio.x) * (1.0f - ratio.y) +
			storage.GetNode(Vector2i(highx, highy)) * (ratio.x) * (ratio.y) +
			storage.GetNode(Vector2i(lowx, highy)) * (1.0f - ratio.x) * (ratio.y);

		return res;
	}

	struct RegularGridSolver
	{
	public:
		template<typename Func>
		void SetDynamicData(Func f)
		{
			Vector2i nodeIndex;
			for (nodeIndex.y = 0; nodeIndex.y < size.y; nodeIndex.y++)
			{
				for (nodeIndex.x = 0; nodeIndex.x < size.x; nodeIndex.x++)
				{
					Vector2f pos = Vector2f(float(nodeIndex.x), float(nodeIndex.y));
					FluidSystem::NodeDynamicData data = f(pos);

					FluidNode fluidNode;
					fluidNode.pressure = data.pressure;
					fluidNode.velocity = data.velocity;
					fluidNode.substance = data.substance;
					grid.SetNode(nodeIndex, fluidNode);
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
					Vector2f pos = Vector2f(float(nodeIndex.x), float(nodeIndex.y));

					nodeData[nodeIndex.x + nodeIndex.y * size.x].velocity = grid.velocity.GetNode(nodeIndex);
					nodeData[nodeIndex.x + nodeIndex.y * size.x].pressure = grid.pressure.GetNode(nodeIndex);
					nodeData[nodeIndex.x + nodeIndex.y * size.x].substance = grid.substance.GetNode(nodeIndex);
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
					Vector2f pos = Vector2f(float(nodeIndex.x), float(nodeIndex.y));
					FluidSystem::NodeStaticData data = f(pos);

					NodeStaticData internalData;
					internalData.isBoundary = (data.nodeType == FluidSystem::NodeTypes::FixedVelocity || data.nodeType == FluidSystem::NodeTypes::SoftVelocity);
					internalData.source = data.source;
					internalData.velocity = grid.velocity.GetNode(nodeIndex);
					internalData.isStatic = false;
					staticData.SetNode(nodeIndex, internalData);
				}
			}
			BuildBoundaryNodes();
		}
		void SetBounds()
		{
			for (auto boundaryNode : boundaryNodes)
			{
				Vector2i nodeIndex = boundaryNode.nodeIndex;
				auto centerData = staticData.GetNode(nodeIndex);

				grid.velocity.SetNode(nodeIndex, centerData.velocity);
				Vector2i normalOffset = boundaryNode.normalOffset;

				/*if (normalOffset.x != 0 || normalOffset.y != 0)
				{
					Vector2f freeVelocity = grid.velocity.GetNode(nodeIndex + normalOffset);
					Vector2f boundaryVelocity = grid.velocity.GetNode(nodeIndex);
					Vector2f resVelocity = Vector2f(2.0f * centerData.velocity.x - freeVelocity.x, 2.0f * centerData.velocity.y - freeVelocity.y);
					grid.velocity.SetNode(nodeIndex, resVelocity);
					grid.pressure.SetNode(nodeIndex, grid.pressure.GetNode(nodeIndex + normalOffset));
				}*/

				if (normalOffset.x != 0)
				{
					Vector2f freeVelocity = grid.velocity.GetNode(nodeIndex + Vector2i(normalOffset.x, 0));
					Vector2f boundaryVelocity = grid.velocity.GetNode(nodeIndex);
					//Vector2f resVelocity = Vector2f(2.0f * centerData.velocity.x - freeVelocity.x, boundaryVelocity.y);
					Vector2f resVelocity = Vector2f(2.0f * centerData.velocity.x - freeVelocity.x, 2.0f * centerData.velocity.y - freeVelocity.y);
					grid.velocity.SetNode(nodeIndex, resVelocity);
					grid.pressure.SetNode(nodeIndex, grid.pressure.GetNode(nodeIndex + Vector2i(normalOffset.x, 0)));
				}
				if (normalOffset.y != 0)
				{
					Vector2f freeVelocity = grid.velocity.GetNode(nodeIndex + Vector2i(0, normalOffset.y));
					Vector2f boundaryVelocity = grid.velocity.GetNode(nodeIndex);
					//Vector2f resVelocity = Vector2f(boundaryVelocity.x, 2.0f * centerData.velocity.y - freeVelocity.y);
					Vector2f resVelocity = Vector2f(2.0f * centerData.velocity.x - freeVelocity.x, 2.0f * centerData.velocity.y - freeVelocity.y);
					grid.velocity.SetNode(nodeIndex, resVelocity);
					grid.pressure.SetNode(nodeIndex, grid.pressure.GetNode(nodeIndex + Vector2i(0, normalOffset.y)));
				}
			}
		}
		void Advect(float dt)
		{
			Vector2i nodeIndex;
			/*#pragma omp parallel for
			for (nodeIndex.y = 0; nodeIndex.y < size.y; nodeIndex.y++)
			{
			for (nodeIndex.x = 0; nodeIndex.x < size.x / 2; nodeIndex.x++)
			{
			Vector2f velocity = grid.velocity.GetNode(nodeIndex);
			Vector2f pos = Vector2f(float(nodeIndex.x), float(nodeIndex.y));
			nextGrid.SetNode(nodeIndex, Interpolate(grid, pos - velocity * dt));

			//Vector2f velocity = grid.velocity.GetNode(nodeIndex);
			//Vector2f pos = Vector2f(float(nodeIndex.x), float(nodeIndex.y));
			//Vector2f halfPosVelocity = Interpolate(grid.velocity, pos - velocity * dt * 0.5f);
			//nextGrid.SetNode(nodeIndex, Interpolate(grid, pos - halfPosVelocity * dt));
			}
			}*/
			float sizeMult = float(size.x) / float(areaSize.x);//assumes square grid

			for (auto nodeIndex : internalNodes)
			{
				Vector2f velocity = grid.velocity.GetNode(nodeIndex);
				Vector2f pos = Vector2f(float(nodeIndex.x), float(nodeIndex.y));
				nextGrid.SetNode(nodeIndex, Interpolate(grid, pos - velocity * sizeMult * dt));
			}

			std::swap(nextGrid, grid);
			return;
			/*#pragma omp parallel for
			for (nodeIndex.y = 0; nodeIndex.y < size.y; nodeIndex.y++)
			{
			for (nodeIndex.x = size.x / 2; nodeIndex.x < size.x; nodeIndex.x++)
			{
			Vector2f velocity = nextGrid.velocity.GetNode(nodeIndex);
			Vector2f pos = Vector2f(float(nodeIndex.x), float(nodeIndex.y));
			nextGrid.SetNode(nodeIndex, Interpolate(grid, pos - velocity * dt));

			//Vector2f velocity = grid.velocity.GetNode(nodeIndex);
			//Vector2f pos = Vector2f(float(nodeIndex.x), float(nodeIndex.y));
			//Vector2f halfPosVelocity = Interpolate(grid.velocity, pos - velocity * dt * 0.5f);
			//nextGrid.SetNode(nodeIndex, Interpolate(grid, pos - halfPosVelocity * dt));
			}
			}*/
			//https://users.cg.tuwien.ac.at/zsolnai/wp/wp-content/uploads/2014/01/zsolnai_msc_thesis.pdf
/*#pragma omp parallel for
			for (nodeIndex.y = 1; nodeIndex.y < size.y - 1; nodeIndex.y++)
			{
				for (nodeIndex.x = 0; nodeIndex.x < size.x - 1; nodeIndex.x++)
				{

					if (nodeIndex.x < size.x / 2)
					{
						//grid.SetNode(nodeIndex, nextGrid.GetNode(nodeIndex));

						Vector2f velocity = nextGrid.velocity.GetNode(nodeIndex);
						Vector2f pos = Vector2f(float(nodeIndex.x), float(nodeIndex.y));
						FluidNode backwardNode = Interpolate(nextGrid, pos + velocity * dt);
						FluidNode initNode = grid.GetNode(nodeIndex);
						FluidNode nextNode = nextGrid.GetNode(nodeIndex);

						FluidNode modifiedNode = initNode + (initNode + backwardNode * -1.0f) * 0.5f;

						FluidNode minNode = nextGrid.GetNode(nodeIndex);
						FluidNode maxNode = nextGrid.GetNode(nodeIndex);
						minNode = MinNode(minNode, nextGrid.GetNode(nodeIndex + Vector2i(1, 0)));
						maxNode = MaxNode(maxNode, nextGrid.GetNode(nodeIndex + Vector2i(1, 0)));
						minNode = MinNode(minNode, nextGrid.GetNode(nodeIndex + Vector2i(-1, 0)));
						maxNode = MaxNode(maxNode, nextGrid.GetNode(nodeIndex + Vector2i(-1, 0)));
						minNode = MinNode(minNode, nextGrid.GetNode(nodeIndex + Vector2i(0, 1)));
						maxNode = MaxNode(maxNode, nextGrid.GetNode(nodeIndex + Vector2i(0, 1)));
						minNode = MinNode(minNode, nextGrid.GetNode(nodeIndex + Vector2i(0, -1)));
						maxNode = MaxNode(maxNode, nextGrid.GetNode(nodeIndex + Vector2i(0, -1)));
						//modifiedNode = ClampNode(modifiedNode, minNode, maxNode);


						tmpGrid.SetNode(nodeIndex, modifiedNode);
					}
					else
					{
						Vector2f velocity = nextGrid.velocity.GetNode(nodeIndex);
						Vector2f pos = Vector2f(float(nodeIndex.x), float(nodeIndex.y));
						FluidNode backwardNode = Interpolate(nextGrid, pos + velocity * dt);
						FluidNode initNode = grid.GetNode(nodeIndex);
						FluidNode nextNode = nextGrid.GetNode(nodeIndex);

						FluidNode modifiedNode = nextNode + (initNode + backwardNode * -1.0f) * 0.5f;

						FluidNode minNode = nextGrid.GetNode(nodeIndex);
						FluidNode maxNode = nextGrid.GetNode(nodeIndex);
						minNode = MinNode(minNode, nextGrid.GetNode(nodeIndex + Vector2i(1, 0)));
						maxNode = MaxNode(maxNode, nextGrid.GetNode(nodeIndex + Vector2i(1, 0)));
						minNode = MinNode(minNode, nextGrid.GetNode(nodeIndex + Vector2i(-1, 0)));
						maxNode = MaxNode(maxNode, nextGrid.GetNode(nodeIndex + Vector2i(-1, 0)));
						minNode = MinNode(minNode, nextGrid.GetNode(nodeIndex + Vector2i(0, 1)));
						maxNode = MaxNode(maxNode, nextGrid.GetNode(nodeIndex + Vector2i(0, 1)));
						minNode = MinNode(minNode, nextGrid.GetNode(nodeIndex + Vector2i(0, -1)));
						maxNode = MaxNode(maxNode, nextGrid.GetNode(nodeIndex + Vector2i(0, -1)));
						//modifiedNode = ClampNode(modifiedNode, minNode, maxNode);


						grid.SetNode(nodeIndex, modifiedNode);
					}
				}
			}

#pragma omp parallel for
			for (nodeIndex.y = 1; nodeIndex.y < size.y - 1; nodeIndex.y++)
			{
				for (nodeIndex.x = 0; nodeIndex.x < size.x - 1; nodeIndex.x++)
				{

					if (nodeIndex.x < size.x / 2)
					{
						Vector2f velocity = tmpGrid.velocity.GetNode(nodeIndex);
						Vector2f pos = Vector2f(float(nodeIndex.x), float(nodeIndex.y));
						grid.SetNode(nodeIndex, Interpolate(tmpGrid, pos - velocity * dt));
						//grid.SetNode(nodeIndex, nextGrid.GetNode(nodeIndex));
					}
					else
					{
						grid.SetNode(nodeIndex, nextGrid.GetNode(nodeIndex));
					}
				}
			}*/
			/*#pragma omp parallel for
			for (nodeIndex.y = 1; nodeIndex.y < size.y - 1; nodeIndex.y++)
			{
			for (nodeIndex.x = 0; nodeIndex.x < size.x - 1; nodeIndex.x++)
			{
			Vector2f velocity = nextGrid.velocity.GetNode(nodeIndex);
			Vector2f pos = Vector2f(float(nodeIndex.x), float(nodeIndex.y));
			FluidNode backwardNode = Interpolate(nextGrid, pos + velocity * dt);
			FluidNode initNode = grid.GetNode(nodeIndex);

			if (nodeIndex.x < size.x / 2)
			{
			FluidNode modifiedNode = initNode + (initNode + backwardNode * -1.0f) * 0.5f;

			FluidNode minNode = grid.GetNode(nodeIndex);
			FluidNode maxNode = grid.GetNode(nodeIndex);
			minNode = MinNode(minNode, grid.GetNode(nodeIndex + Vector2i(1, 0)));
			maxNode = MaxNode(maxNode, grid.GetNode(nodeIndex + Vector2i(1, 0)));
			minNode = MinNode(minNode, grid.GetNode(nodeIndex + Vector2i(-1, 0)));
			maxNode = MaxNode(maxNode, grid.GetNode(nodeIndex + Vector2i(-1, 0)));
			minNode = MinNode(minNode, grid.GetNode(nodeIndex + Vector2i(0, 1)));
			maxNode = MaxNode(maxNode, grid.GetNode(nodeIndex + Vector2i(0, 1)));
			minNode = MinNode(minNode, grid.GetNode(nodeIndex + Vector2i(0, -1)));
			maxNode = MaxNode(maxNode, grid.GetNode(nodeIndex + Vector2i(0, -1)));
			//modifiedNode = ClampNode(modifiedNode, minNode, maxNode);


			tmpGrid.SetNode(nodeIndex, modifiedNode);
			}
			else
			{

			FluidNode modifiedNode = nextGrid.GetNode(nodeIndex) + (initNode + backwardNode * -1.0f) * 0.5f;
			FluidNode minNode = nextGrid.GetNode(nodeIndex);
			FluidNode maxNode = nextGrid.GetNode(nodeIndex);
			minNode = MinNode(minNode, nextGrid.GetNode(nodeIndex + Vector2i(1, 0)));
			maxNode = MaxNode(maxNode, nextGrid.GetNode(nodeIndex + Vector2i(1, 0)));
			minNode = MinNode(minNode, nextGrid.GetNode(nodeIndex + Vector2i(-1, 0)));
			maxNode = MaxNode(maxNode, nextGrid.GetNode(nodeIndex + Vector2i(-1, 0)));
			minNode = MinNode(minNode, nextGrid.GetNode(nodeIndex + Vector2i(0, 1)));
			maxNode = MaxNode(maxNode, nextGrid.GetNode(nodeIndex + Vector2i(0, 1)));
			minNode = MinNode(minNode, nextGrid.GetNode(nodeIndex + Vector2i(0, -1)));
			maxNode = MaxNode(maxNode, nextGrid.GetNode(nodeIndex + Vector2i(0, -1)));
			//modifiedNode = ClampNode(modifiedNode, minNode, maxNode);
			tmpGrid.SetNode(nodeIndex, modifiedNode);
			}

			//Vector2f velocity = grid.velocity.GetNode(nodeIndex);
			//Vector2f pos = Vector2f(float(nodeIndex.x), float(nodeIndex.y));
			//Vector2f halfPosVelocity = Interpolate(grid.velocity, pos - velocity * dt * 0.5f);
			//nextGrid.SetNode(nodeIndex, Interpolate(grid, pos - halfPosVelocity * dt));
			}
			}
			#pragma omp parallel for
			for (nodeIndex.y = 0; nodeIndex.y < size.y; nodeIndex.y++)
			{
			for (nodeIndex.x = 0; nodeIndex.x < size.x; nodeIndex.x++)
			{
			if (nodeIndex.x < size.x / 2)
			{
			Vector2f velocity = tmpGrid.velocity.GetNode(nodeIndex);
			Vector2f pos = Vector2f(float(nodeIndex.x), float(nodeIndex.y));
			nextGrid.SetNode(nodeIndex, Interpolate(tmpGrid, pos - velocity * dt));
			}
			else
			{
			nextGrid.SetNode(nodeIndex, tmpGrid.GetNode(nodeIndex));
			}

			//Vector2f velocity = grid.velocity.GetNode(nodeIndex);
			//Vector2f pos = Vector2f(float(nodeIndex.x), float(nodeIndex.y));
			//Vector2f halfPosVelocity = Interpolate(grid.velocity, pos - velocity * dt * 0.5f);
			//nextGrid.SetNode(nodeIndex, Interpolate(grid, pos - halfPosVelocity * dt));
			}
			}
			std::swap(nextGrid, grid);*/
			SetBounds();
		}
		/*void ConfineVorticity(float dt)
		{
			//https://www.duo.uio.no/bitstream/handle/10852/9752/Moastuen.pdf?sequence=1
			Vector2i nodeIndex;
#pragma omp parallel for
			for (nodeIndex.y = 1; nodeIndex.y < size.y - 1; nodeIndex.y++)
			{
				for (nodeIndex.x = 1; nodeIndex.x < size.x - 1; nodeIndex.x++)
				{
					velocityVorticity.SetNode(nodeIndex, grid.velocity.GetVorticity(nodeIndex));
				}
			}

#pragma omp parallel for
			for (nodeIndex.y = 1; nodeIndex.y < size.y - 1; nodeIndex.y++)
			{
				for (nodeIndex.x = size.x / 2; nodeIndex.x < size.x - 1; nodeIndex.x++)
				{
					Vector2f vorticityGradient = velocityVorticity.GetAbsGradient(nodeIndex);
					Vector2f normGradient = vorticityGradient * (1.0f / (vorticityGradient.Len() + 1e-5f));
					Vector2f force = Vector2f(-normGradient.y, normGradient.x) * velocityVorticity.GetNode(nodeIndex) * dt;

					Vector2f velocity = grid.velocity.GetNode(nodeIndex);
					velocity += force * dt * -5.0f;
					grid.velocity.SetNode(nodeIndex, velocity);

					//velocityVorticity.SetNode(nodeIndex, grid.velocity.GetVorticity(nodeIndex));
				}
			}
		}*/
		void Diffuse(float dt)
		{
			/*float viscosity = 1e-6f;
			Vector2i nodeIndex;

			int iterationsCount = 4;

			//gauss-seidel
			//https://www.duo.uio.no/bitstream/handle/10852/9752/Moastuen.pdf?sequence=1
			float mult = step.x * step.x / (viscosity * dt);
			for (int i = 0; i < iterationsCount * 2; i++)
			{
#pragma omp parallel for
				for (nodeIndex.y = 1; nodeIndex.y < size.y - 1; nodeIndex.y++)
				{
					int offset = (i + nodeIndex.y) % 2;
					for (nodeIndex.x = 1; nodeIndex.x < (size.x - 2) / 2; nodeIndex.x++)
					{
						Vector2i localIndex = Vector2i(nodeIndex.x * 2 + offset, nodeIndex.y);

						grid.SetNode(localIndex, (
							grid.GetNode(localIndex + Vector2i(-1, 0)) +
							grid.GetNode(localIndex + Vector2i(1, 0)) +
							grid.GetNode(localIndex + Vector2i(0, -1)) +
							grid.GetNode(localIndex + Vector2i(0, 1)) +
							grid.GetNode(localIndex) * mult) * (1.0f / (4.0f + mult)));
					}
				}
				SetBounds();
			}*/
		}
		void Project(size_t iterationsCount)
		{
			Vector2i nodeIndex;
			for (auto nodeIndex : internalNodes)
			{
				velocityDivergence.SetNode(nodeIndex, grid.GetVelocityDivergence(nodeIndex));
			}
			/*for (int i = 0; i < iterationsCount; i++)
			{
				#pragma omp parallel for
				for (nodeIndex.y = 1; nodeIndex.y < size.y - 1; nodeIndex.y++)
				{
					for (nodeIndex.x = 1; nodeIndex.x < size.x - 1; nodeIndex.x++)
					{
						nextGrid.pressure.SetNode(nodeIndex, (
						grid.pressure.GetNode(nodeIndex + Vector2i(-1,  0)) +
						grid.pressure.GetNode(nodeIndex + Vector2i( 1,  0)) +
						grid.pressure.GetNode(nodeIndex + Vector2i( 0, -1)) +
						grid.pressure.GetNode(nodeIndex + Vector2i( 0,  1)) +
						-step.x * step.x * (velocityDivergence.GetNode(nodeIndex) - staticData.GetNode(nodeIndex).source)) / 4.0f );
					}
				}
				std::swap(grid.pressure, nextGrid.pressure);
				SetBounds();
			}*/

			//gauss-seidel
			//https://www.duo.uio.no/bitstream/handle/10852/9752/Moastuen.pdf?sequence=1
			for (int i = 0; i < iterationsCount * 2; i++)
			{
				float relaxation = 1.0f + (1.0f - float(i / 2) / float(iterationsCount - 1)) * 0.5f;
				for (auto nodeIndex : internalNodes)
				{
					if ((nodeIndex.x % 2) ^ (nodeIndex.y % 2) ^ (i % 2))
						continue;
					float currPressure = grid.pressure.GetNode(nodeIndex);
					float newPressure = (
						grid.pressure.GetNode(nodeIndex + Vector2i(-1, 0)) +
						grid.pressure.GetNode(nodeIndex + Vector2i(1, 0)) +
						grid.pressure.GetNode(nodeIndex + Vector2i(0, -1)) +
						grid.pressure.GetNode(nodeIndex + Vector2i(0, 1)) +
						-step.x * step.x * (velocityDivergence.GetNode(nodeIndex) - staticData.GetNode(nodeIndex).source)) / 4.0f;
					grid.pressure.SetNode(nodeIndex, currPressure + relaxation * (newPressure - currPressure));
				}
				SetBounds();
			}

			for (auto nodeIndex : internalNodes)
			{
				Vector2f velocity = grid.velocity.GetNode(nodeIndex);
				velocity -= grid.GetPressureGradient(nodeIndex);
				grid.velocity.SetNode(nodeIndex, velocity);
			}
			SetBounds();
		}

		void Resize(Vector2i size, Vector2f areaSize)
		{
			this->size = size;
			this->areaSize = areaSize;
			this->step = Vector2f(areaSize.x / float(size.x), areaSize.y / float(size.y));
			this->invStep = Vector2f(1.0f / step.x, 1.0f / step.y);
			grid.Resize(size, step);
			nextGrid.Resize(size, step);
			tmpGrid.Resize(size, step);
			staticData.Resize(size, step);
			velocityDivergence.Resize(size, step);
			velocityVorticity.Resize(size, step);
		}
	private:
		void BuildBoundaryNodes()
		{
			boundaryNodes.clear();
			internalNodes.clear();
			Vector2i nodeIndex;
			for (nodeIndex.y = 1; nodeIndex.y < size.y - 1; nodeIndex.y++)
			{
				for (nodeIndex.x = 1; nodeIndex.x < size.x - 1; nodeIndex.x++)
				{
					Vector2i normalOffset = Vector2i(0, 0);
					auto centerData = staticData.GetNode(nodeIndex);

					if (!centerData.isBoundary)
					{
						internalNodes.push_back(nodeIndex);
						continue;
					}

					grid.velocity.SetNode(nodeIndex, centerData.velocity);
					if (!staticData.GetNode(nodeIndex + Vector2i(-1, 0)).isBoundary)
						normalOffset.x = -1;
					if (!staticData.GetNode(nodeIndex + Vector2i(1, 0)).isBoundary)
						normalOffset.x = 1;
					if (!staticData.GetNode(nodeIndex + Vector2i(0, -1)).isBoundary)
						normalOffset.y = -1;
					if (!staticData.GetNode(nodeIndex + Vector2i(0, 1)).isBoundary)
						normalOffset.y = 1;

					if (normalOffset.x != 0 || normalOffset.y != 0)
					{
						BoundaryNode boundaryNode;
						boundaryNode.nodeIndex = nodeIndex;
						boundaryNode.normalOffset = normalOffset;
						boundaryNodes.push_back(boundaryNode);
					}

					/*if (normalOffset.x != 0)
					{
					Vector2f freeVelocity = grid.velocity.GetNode(nodeIndex + Vector2i(normalOffset.x, 0));
					Vector2f boundaryVelocity = grid.velocity.GetNode(nodeIndex);
					//Vector2f resVelocity = Vector2f(2.0f * centerData.velocity.x - freeVelocity.x, boundaryVelocity.y);
					Vector2f resVelocity = Vector2f(2.0f * centerData.velocity.x - freeVelocity.x, 2.0f * centerData.velocity.y - freeVelocity.y);
					grid.velocity.SetNode(nodeIndex, resVelocity);
					grid.pressure.SetNode(nodeIndex, grid.pressure.GetNode(nodeIndex + Vector2i(normalOffset.x, 0)));
					}
					if (normalOffset.y != 0)
					{
					Vector2f freeVelocity = grid.velocity.GetNode(nodeIndex + Vector2i(0, normalOffset.y));
					Vector2f boundaryVelocity = grid.velocity.GetNode(nodeIndex);
					//Vector2f resVelocity = Vector2f(boundaryVelocity.x, 2.0f * centerData.velocity.y - freeVelocity.y);
					Vector2f resVelocity = Vector2f(2.0f * centerData.velocity.x - freeVelocity.x, 2.0f * centerData.velocity.y - freeVelocity.y);
					grid.velocity.SetNode(nodeIndex, resVelocity);
					grid.pressure.SetNode(nodeIndex, grid.pressure.GetNode(nodeIndex + Vector2i(0, normalOffset.y)));
					}*/
				}
			}
		}
		FluidRegularGrid grid;
		FluidRegularGrid nextGrid;
		FluidRegularGrid tmpGrid;

		RegularStorage<NodeStaticData> staticData;
		RegularStorage<float> velocityDivergence;
		RegularStorage<float> velocityVorticity;
		Vector2i size;
		Vector2f step;
		Vector2f invStep;
		Vector2f areaSize;
		struct BoundaryNode
		{
			Vector2i normalOffset;
			Vector2i nodeIndex;
		};
		std::vector<BoundaryNode> boundaryNodes;
		std::vector<Vector2i> internalNodes;
	};

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

		FluidNode GetScatteredNode(Vector2i nodeIndex)
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
			return Vector2f( 0.0f, 0.0f );
		}
		static Vector2f GetYVelocityOffset()
		{
			return Vector2f( 0.0f, 0.0f );
		}
		static Vector2f GetPressureOffset()
		{
			return Vector2f( 0.0f, 0.0f );
		}
		float GetXPressureGradient(Vector2i xVelocityNode)
		{
			return 0.0f;
		}
		float GetYPressureGradient(Vector2i yVelocityNode)
		{
			return 0.0f;
		}
		Vector2f GetVelocityPressureNode(Vector2i pressureNode)
		{
			return Vector2f( 0.0f, 0.0f );
		}
		Vector2f GetVelocityXVelocityNode(Vector2i xVelocityNode)
		{
			return Vector2f( 0.0f, 0.0f );
		}
		Vector2f GetVelocityYVelocityNode(Vector2i yVelocityNode)
		{
			return Vector2f( 0.0f, 0.0f );
		}
		float GetVelocityDivergence(Vector2i pressureNode)
		{
			return 0.0f;
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
					Vector2f pos = Vector2f(float(nodeIndex.x), float(nodeIndex.y));
					FluidSystem::NodeDynamicData data = f(pos);

					FluidNode fluidNode;
					fluidNode.pressure = data.pressure;
					fluidNode.velocity = data.velocity;
					fluidNode.substance = data.substance;
					grid.SetScatteredNode(nodeIndex, fluidNode);
					nextGrid.SetScatteredNode(nodeIndex, fluidNode);
					tmpGrid.SetScatteredNode(nodeIndex, fluidNode);
					velocityDivergence.SetNode(nodeIndex, 0.0f);
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
					Vector2f pos = Vector2f(float(nodeIndex.x), float(nodeIndex.y));

					if (nodeIndex.x > 0 && nodeIndex.y > 0 && (nodeIndex.x < size.x - 1) && (nodeIndex.y < size.y - 1))
						nodeData[nodeIndex.x + nodeIndex.y * size.x].velocity = grid.GetVelocityPressureNode(nodeIndex);
					else
						nodeData[nodeIndex.x + nodeIndex.y * size.x].velocity = Vector2f(0.0f, 0.0f);
					nodeData[nodeIndex.x + nodeIndex.y * size.x].pressure = grid.pressure.GetNode(nodeIndex);
					nodeData[nodeIndex.x + nodeIndex.y * size.x].substance = grid.substance.GetNode(nodeIndex);
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
					Vector2f pos = Vector2f(float(nodeIndex.x), float(nodeIndex.y));
					FluidSystem::NodeStaticData data = f(pos);

					NodeStaticData internalData;
					internalData.isBoundary = (data.nodeType == FluidSystem::NodeTypes::FixedVelocity || data.nodeType == FluidSystem::NodeTypes::SoftVelocity);
					internalData.source = data.source;
					if (nodeIndex.x > 0 && nodeIndex.y > 0 && (nodeIndex.x < size.x - 1) && (nodeIndex.y < size.y - 1))
						internalData.velocity = grid.GetVelocityPressureNode(nodeIndex);
					else
						internalData.velocity = Vector2f(0.0f, 0.0f);
					internalData.isStatic = false;
					staticData.SetNode(nodeIndex, internalData);
				}
			}
			BuildBoundaryNodes();
		}

		void AdvectGrid(StaggeredGridType &srcGrid, StaggeredGridType &dstGrid, float timeOffset)
		{
			float sizeMult = float(size.x) / float(areaSize.x);
			Vector2i nodeIndex;
			for (auto nodeIndex : internalNodes)
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
		void Diffuse(float dt, size_t iterationsCount)
		{
			//gauss-seidel
			//https://www.duo.uio.no/bitstream/handle/10852/9752/Moastuen.pdf?sequence=1
			float viscosity = 1000.0f;// 0.00001f;
			float mult = step.x * step.x / (viscosity * dt);
			Vector2i nodeIndex;
			for (int i = 0; i < iterationsCount * 2; i++)
			{
				float relaxation = 1.0f + (1.0f - float(i / 2) / float(iterationsCount - 1)) * 0.5f;
				for (auto nodeIndex : internalNodes)
				{
					if ((nodeIndex.x % 2) ^ (nodeIndex.y % 2) ^ (i % 2))
						continue;

					grid.SetScatteredVelocity(nodeIndex, (
						grid.GetScatteredVelocity(nodeIndex + Vector2i(-1, 0)) +
						grid.GetScatteredVelocity(nodeIndex + Vector2i(1, 0)) +
						grid.GetScatteredVelocity(nodeIndex + Vector2i(0, -1)) +
						grid.GetScatteredVelocity(nodeIndex + Vector2i(0, 1)) +
						grid.GetScatteredVelocity(nodeIndex) * mult) * (1.0f / (4.0f + mult)));
				}
				SetBounds();
			}

		}
		void Project(size_t iterationsCount)
		{
			Vector2i nodeIndex;
			//#pragma omp parallel for

			for (auto nodeIndex : internalNodes)
			{
				velocityDivergence.SetNode(nodeIndex, grid.GetVelocityDivergence(nodeIndex));
			}
			//gauss-seidel
			//https://www.duo.uio.no/bitstream/handle/10852/9752/Moastuen.pdf?sequence=1
			for (int i = 0; i < iterationsCount * 2; i++)
			{
				float relaxation = 1.0f + (1.0f - float(i / 2) / float(iterationsCount - 1)) * 0.5f;
				for (auto nodeIndex : internalNodes)
				{
					if ((nodeIndex.x % 2) ^ (nodeIndex.y % 2) ^ (i % 2))
						continue;
					//Vector2i localIndex = Vector2i(nodeIndex.x * 2 + offset, nodeIndex.y);
					float currPressure = grid.pressure.GetNode(nodeIndex);
					float newPressure = (
						grid.pressure.GetNode(nodeIndex + Vector2i(-1, 0)) +
						grid.pressure.GetNode(nodeIndex + Vector2i(1, 0)) +
						grid.pressure.GetNode(nodeIndex + Vector2i(0, -1)) +
						grid.pressure.GetNode(nodeIndex + Vector2i(0, 1)) +
						-step.x * step.x * (velocityDivergence.GetNode(nodeIndex) - staticData.GetNode(nodeIndex).source)) / 4.0f;
					grid.pressure.SetNode(nodeIndex, currPressure + relaxation * (newPressure - currPressure));
				}
				SetBounds();
			}

			//#pragma omp parallel for
			//SetBounds();

			for (auto nodeIndex : internalNodes)
			{
				float xVelocity = grid.xVelocity.GetNode(nodeIndex);
				xVelocity -= grid.GetPressureXGradient(nodeIndex);
				grid.xVelocity.SetNode(nodeIndex, xVelocity);

				float yVelocity = grid.yVelocity.GetNode(nodeIndex);
				yVelocity -= grid.GetPressureYGradient(nodeIndex);
				grid.yVelocity.SetNode(nodeIndex, yVelocity);
			}
			SetBounds();
		}

		void SetBounds()
		{
			for (auto boundaryNode : boundaryNodes)
			{
				Vector2i nodeIndex = boundaryNode.nodeIndex;
				auto centerData = staticData.GetNode(nodeIndex);
				if (!centerData.isBoundary)
					continue;

				Vector2i normalOffset = boundaryNode.normalOffset;

				
				//boundary goes through cell center
				/*if (normalOffset.x != 0)
				{
					Vector2i freeIndex = nodeIndex + Vector2i(normalOffset.x == 1 ? 1 : 0, 0);
					Vector2i boundaryIndex = nodeIndex + Vector2i(normalOffset.x == 1 ? 0 : 1, 0);
					float resVelocity = 2.0f * centerData.velocity.x - grid.xVelocity.GetNode(freeIndex);
					grid.xVelocity.SetNode(boundaryIndex, resVelocity);
					grid.yVelocity.SetNode(nodeIndex, centerData.velocity.y);
					grid.yVelocity.SetNode(nodeIndex + Vector2i(0, 1), centerData.velocity.y);
					grid.pressure.SetNode(nodeIndex + Vector2i(-normalOffset.x, 0), grid.pressure.GetNode(nodeIndex));
				}

				if (normalOffset.y != 0)
				{
					Vector2i freeIndex = nodeIndex + Vector2i(0, normalOffset.y == 1 ? 1 : 0);
					Vector2i boundaryIndex = nodeIndex + Vector2i(0, normalOffset.y == 1 ? 0 : 1);
					float resVelocity = 2.0f * centerData.velocity.y - grid.yVelocity.GetNode(freeIndex);
					grid.yVelocity.SetNode(boundaryIndex, resVelocity);
					grid.xVelocity.SetNode(nodeIndex, centerData.velocity.x);
					grid.xVelocity.SetNode(nodeIndex + Vector2i(1, 0), centerData.velocity.x);
					grid.pressure.SetNode(nodeIndex + Vector2i(0, -normalOffset.y), grid.pressure.GetNode(nodeIndex));
				}*/

				grid.xVelocity.SetNode(nodeIndex, centerData.velocity.x);
				grid.xVelocity.SetNode(nodeIndex + Vector2i(1, 0), centerData.velocity.x);
				grid.yVelocity.SetNode(nodeIndex, centerData.velocity.y);
				grid.yVelocity.SetNode(nodeIndex + Vector2i(0, 1), centerData.velocity.y);

				if (normalOffset.x != 0)
				{
					grid.pressure.SetNode(nodeIndex, grid.pressure.GetNode(nodeIndex + Vector2i(normalOffset.x, 0)));
				}

				if (normalOffset.y != 0)
				{
					grid.pressure.SetNode(nodeIndex, grid.pressure.GetNode(nodeIndex + Vector2i(0, normalOffset.y)));
				}
			}
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
			velocityDivergence.Resize(size, areaSize);
			velocityVorticity.Resize(size, areaSize);
		}

	private:
		void BuildBoundaryNodes()
		{
			boundaryNodes.clear();
			internalNodes.clear();
			Vector2i nodeIndex;
			for (nodeIndex.y = 1; nodeIndex.y < size.y - 1; nodeIndex.y++)
			{
				for (nodeIndex.x = 1; nodeIndex.x < size.x - 1; nodeIndex.x++)
				{
					Vector2i normalOffset = Vector2i(0, 0);
					auto centerData = staticData.GetNode(nodeIndex);

					if (!centerData.isBoundary)
					{
						internalNodes.push_back(nodeIndex);
						continue;
					}

					if (!staticData.GetNode(nodeIndex + Vector2i(-1, 0)).isBoundary)
						normalOffset.x = -1;
					if (!staticData.GetNode(nodeIndex + Vector2i(1, 0)).isBoundary)
						normalOffset.x = 1;
					if (!staticData.GetNode(nodeIndex + Vector2i(0, -1)).isBoundary)
						normalOffset.y = -1;
					if (!staticData.GetNode(nodeIndex + Vector2i(0, 1)).isBoundary)
						normalOffset.y = 1;

					if (normalOffset.x != 0 || normalOffset.y != 0)
					{
						BoundaryNode boundaryNode;
						boundaryNode.nodeIndex = nodeIndex;
						boundaryNode.normalOffset = normalOffset;
						boundaryNodes.push_back(boundaryNode);
					}
				}
			}
		}
		StaggeredGridType grid;
		StaggeredGridType nextGrid;
		StaggeredGridType tmpGrid;

		RegularStorage<NodeStaticData> staticData;
		RegularStorage<float> velocityDivergence;
		RegularStorage<float> velocityVorticity;
		Vector2i size;
		Vector2f step;
		Vector2f invStep;
		Vector2f areaSize;
		struct BoundaryNode
		{
			Vector2i normalOffset;
			Vector2i nodeIndex;
		};
		std::vector<BoundaryNode> boundaryNodes;
		std::vector<Vector2i> internalNodes;
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
			solver.SetDynamicData([&](Vector2f gridPos) -> NodeDynamicData
			{
				return srcNodeData[size.x * int(gridPos.y + 0.5f) + int(gridPos.x + 0.5f)];
			});
		}
		void GetDynamicData(NodeDynamicData *dstNodeData) override
		{
			solver.GetDynamicData(dstNodeData);
		}
		void SetStaticData(const NodeStaticData *srcNodeData) override
		{
			solver.SetStaticData([&](Vector2f gridPos) -> NodeStaticData
			{
				return srcNodeData[size.x * int(gridPos.y + 0.5f) + int(gridPos.x + 0.5f)];
			});
		}
		virtual void Update(float dt)
		{
			solver.Advect(dt);
			solver.Diffuse(dt, 1);
			solver.Project(relaxationIterations);
		}
	private:
		size_t relaxationIterations;
		Solver solver;
		Vector2i size;
		Vector2f areaSize;
	};
	std::unique_ptr<FluidSystem> FluidSystem::Create()
	{
		//return std::make_unique<ExperimentalSystem<RegularGridSolver> >();
		return std::make_unique<ExperimentalSystem<StaggeredGridSolver<StaggeredAlignedGrid>>>();
	}
}