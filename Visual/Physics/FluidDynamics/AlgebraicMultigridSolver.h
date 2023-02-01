#pragma once

#include <vector>
#include <algorithm>
#include <fstream>
#include <iostream>

namespace Physics
{
	Pattern3x3<float> BuildProjectionMatrix()
	{
		Pattern3x3<float> res;
		Vector2i nodeIndex;
		for (nodeIndex.y = 0; nodeIndex.y < 3; nodeIndex.y++)
		{
			for (nodeIndex.x = 0; nodeIndex.x < 3; nodeIndex.x++)
			{
				float sum = 0.0f;
				Vector2i offset = nodeIndex - Vector2i(1, 1);
				float val = 0.0f;
				if (abs(offset.x) > 0 && abs(offset.y) > 0)
					val = 1.0f;
				if ((abs(offset.x) > 0) ^ (abs(offset.y) > 0))
					val = 2.0f;
				if (offset.x == 0 && offset.y == 0)
					val = 4.0f;
				res.SetNode(nodeIndex, val);
			}
		}
		return res;
	}

	template<typename NodeType>
	struct MultigridLayer
	{
		void Resize(Vector2i _size)
		{
			this->size = _size;
			values.Resize(size, Vector2f(1.0f, 1.0f));
			tmpValues.Resize(size, Vector2f(1.0f, 1.0f));
			rightSides.Resize(size, Vector2f(1.0f, 1.0f));
			rightSides.Fill(0.0f);
			residuals.Resize(size, Vector2f(1.0f, 1.0f));
			residuals.Fill(0.0f);
			equationMatrices.Resize(size, Vector2f(1.0f, 1.0f));
			equationMatrices.Fill(Pattern3x3<float>(0.0f));
		}
		RegularStorage<NodeType> &GetValues()
		{
			return values;
		}

		RegularStorage<NodeType> &GetRightSides()
		{
			return rightSides;
		}

		RegularStorage<Pattern3x3<float>> &GetEquationMatrices()
		{
			return equationMatrices;
		}

		NodeType GetNodeResidual(Vector2i nodeIndex)
		{
			return rightSides.GetNode(nodeIndex) - values.Dot(nodeIndex, equationMatrices.GetNode(nodeIndex), Vector2i(1, 1));
		}
		float ComputeResiduals()
		{
			float maxResidual = 0.0f;
			Vector2i nodeIndex;
			for (nodeIndex.y = 1; nodeIndex.y < size.y - 1; nodeIndex.y++)
			{
				for (nodeIndex.x = 1; nodeIndex.x < size.x - 1; nodeIndex.x++)
				{
					float nodeResidual = GetNodeResidual(nodeIndex);
					maxResidual = std::max(abs(nodeResidual), maxResidual);
					residuals.SetNode(nodeIndex, nodeResidual);
				}
			}
			return maxResidual;
		}


		float IterateGaussSeidel(size_t iterationsCount)
		{
			float maxDelta = 0.0f;
			for (size_t i = 0; i < iterationsCount * 2; i++)
			{
				Vector2i nodeIndex;
				for (nodeIndex.y = 1; nodeIndex.y < size.y - 1; nodeIndex.y++)
				{
					for (nodeIndex.x = 1; nodeIndex.x < size.x - 1; nodeIndex.x++)
					{
						if ((nodeIndex.x % 2) ^ (nodeIndex.y % 2) ^ (i % 2))
							continue;

						float err = GetNodeResidual(nodeIndex);

						Vector2i maxElemIndex = Vector2i(1, 1);
						Vector2i innerIndex;
						/*if (fabs(equationMatrix.GetNode(maxElemIndex)) < 1e-3f)
						{
							for (innerIndex.y = 0; innerIndex.y < 3; innerIndex.y++)
							{
								for (innerIndex.x = 0; innerIndex.x < 3; innerIndex.x++)
								{
									if (abs(equationMatrix.GetNode(innerIndex)) >= abs(equationMatrix.GetNode(maxElemIndex)))
									{
										maxElemIndex = innerIndex;
									}
								}
							}
						}*/
						//float divider = equationMatrix.GetNode(Vector2i(1, 1));
						float divider = equationMatrices.GetNode(nodeIndex).GetNode(maxElemIndex);

						if (fabs(divider) < 1e-7f)
							continue;

						Vector2i targetNode = nodeIndex + maxElemIndex - Vector2i(1, 1);
						float currValue = values.GetNode(targetNode);
						float newValue = currValue + err / divider;

						values.SetNode(targetNode, newValue);
					}
				}
			}
			return maxDelta;
		}

		enum struct ProductTypes
		{
			Equation,
			Identity
		};
		Pattern3x3<NodeType> GetGalerkinProduct(Vector2i nodeIndex, ProductTypes productType) const
		{
			StaticRegularStorage<Pattern3x3<float>, 7, 7> fullPatterns[2];
			auto *ping = &fullPatterns[0];
			auto *pong = &fullPatterns[1];

			ping->Fill(Pattern3x3<float>(0.0f));

			Vector2i innerIndex;
			for (innerIndex.y = 0; innerIndex.y < 3; innerIndex.y++)
			{
				for (innerIndex.x = 0; innerIndex.x < 3; innerIndex.x++)
				{
					Pattern3x3<float> pattern(0.0f);
					pattern.SetNode(innerIndex, 1.0f);
					ping->SetNode(innerIndex * 2 + Vector2i(1, 1), pattern);
				}
			}
			*pong = *ping;

			Pattern3x3<float> interpolatorMatrix = BuildProjectionMatrix();
			Pattern3x3<float> restrictionMatrix = BuildProjectionMatrix();

			auto test0 = ping->GetNode(Vector2i(3, 3));
			for (innerIndex.y = 1; innerIndex.y < 6; innerIndex.y++)
			{
				for (innerIndex.x = 1; innerIndex.x < 6; innerIndex.x++)
				{
					//Vector2i fineNodeIndex = nodeIndex + innerIndex - Vector2i(3, 3);
					Pattern3x3<float> product = ping->Dot(innerIndex, interpolatorMatrix, Vector2i(1, 1));
					pong->SetNode(innerIndex, product);
				}
			}
			std::swap(ping, pong);

			if (productType != ProductTypes::Identity)
			{
				for (innerIndex.y = 1; innerIndex.y < 6; innerIndex.y++)
				{
					for (innerIndex.x = 1; innerIndex.x < 6; innerIndex.x++)
					{
						Vector2i fineNodeIndex = nodeIndex + innerIndex - Vector2i(3, 3);
						Pattern3x3<float> product;
						if (fineNodeIndex.x < 0 || fineNodeIndex.y < 0 || fineNodeIndex.x >= size.x || fineNodeIndex.y >= size.y)
							product.Fill(0.0f);
						else
							product = ping->Dot(innerIndex, equationMatrices.GetNode(fineNodeIndex), Vector2i(1, 1));
						pong->SetNode(innerIndex, product);
					}
				}
				std::swap(ping, pong);
			}

			for (innerIndex.y = 1; innerIndex.y < 6; innerIndex.y++)
			{
				for (innerIndex.x = 1; innerIndex.x < 6; innerIndex.x++)
				{
					//Vector2i fineNodeIndex = nodeIndex + innerIndex - Vector2i(3, 3);
					Pattern3x3<float> product = ping->Dot(innerIndex, restrictionMatrix, Vector2i(1, 1));
					pong->SetNode(innerIndex, product);
				}
			}
			std::swap(ping, pong);

			return  ping->GetNode(Vector2i(3, 3));
		}
		Vector2i ToFineIndex(Vector2i coarseIndex)
		{
			return (coarseIndex - Vector2i(1, 1)) * 2 + Vector2i(1, 1);
			//return coarseIndex * 2;
		}
		Vector2i ToCoarseIndex(Vector2i fineIndex)
		{
			return Vector2i((fineIndex.x - 1) / 2 + 1, (fineIndex.y - 1) / 2 + 1);
			//return Vector2i(fineIndex.x / 2, fineIndex.y / 2);
		}
		void StaticDownsample(const MultigridLayer &fineSolver)
		{
			Vector2i nodeIndex;
			for (nodeIndex.y = 1; nodeIndex.y < size.y - 1; nodeIndex.y++)
			{
				for (nodeIndex.x = 1; nodeIndex.x < size.x - 1; nodeIndex.x++)
				{
					auto equationMatrix = fineSolver.GetGalerkinProduct(ToFineIndex(nodeIndex), ProductTypes::Equation);
					this->equationMatrices.SetNode(nodeIndex, equationMatrix);
				}
			}
		}

		void DynamicDownsample(const MultigridLayer &fineSolver)
		{
			Pattern3x3<float> restrictionMatrix = BuildProjectionMatrix();
			Vector2i nodeIndex;
			for (nodeIndex.y = 1; nodeIndex.y < size.y - 1; nodeIndex.y++)
			{
				for (nodeIndex.x = 1; nodeIndex.x < size.x - 1; nodeIndex.x++)
				{
					Vector2i fineNodeIndex = ToFineIndex(nodeIndex);

					float rightSide = fineSolver.residuals.Dot(fineNodeIndex, restrictionMatrix, Vector2i(1, 1));
					/*if (equationMatrices.GetNode(nodeIndex).IsNull())
						rightSide = 0;*/
					rightSides.SetNode(nodeIndex, rightSide);
					values.SetNode(nodeIndex, 0.0f);
				}
			}
		}

		void Upsample(const MultigridLayer &coarseSolver)
		{
			tmpValues.Fill(0.0f);
			Vector2i nodeIndex;
			for (nodeIndex.y = 1; nodeIndex.y < coarseSolver.size.y - 1; nodeIndex.y++)
			{
				for (nodeIndex.x = 1; nodeIndex.x < coarseSolver.size.x - 1; nodeIndex.x++)
				{

					{
						Vector2i fineNode = ToFineIndex(nodeIndex);

						auto matrix = coarseSolver.equationMatrices.GetNode(nodeIndex);

						tmpValues.SetNode(fineNode, matrix.IsNull() ? 0.0f : coarseSolver.values.GetNode(nodeIndex));
					}
				}
			}

			Pattern3x3<float> interpolationMatrix = BuildProjectionMatrix();
			for (nodeIndex.y = 1; nodeIndex.y < size.y - 1; nodeIndex.y++)
			{
				for (nodeIndex.x = 1; nodeIndex.x < size.x - 1; nodeIndex.x++)
				{
					float valueDelta = tmpValues.Dot(nodeIndex, interpolationMatrix, Vector2i(1, 1));

					values.SetNode(nodeIndex, values.GetNode(nodeIndex) + valueDelta);
				}
			}
		}
	private:

		RegularStorage<NodeType> values;
		RegularStorage<NodeType> tmpValues;
		RegularStorage<NodeType> rightSides;
		RegularStorage<NodeType> residuals;
		RegularStorage<Pattern3x3<float> > equationMatrices;
		Vector2i size;
	};

	template<typename NodeT>
	struct AlgebraicMultigridSolver
	{
		void Resize(Vector2i size)
		{
			layerSolvers.resize(5);

			Vector2i currResolution = size;
			for (auto &layerSolver : layerSolvers)
			{
				layerSolver.Resize(currResolution);
				currResolution.x = (currResolution.x + 2 - 2) / 2 + 2;
				currResolution.y = (currResolution.y + 2 - 2) / 2 + 2;
				/*currResolution.x = (currResolution.x + 1) / 2;
				currResolution.y = (currResolution.y + 1) / 2;*/
			}
		}

		void Solve(size_t iterationsCount)
		{
			for (size_t i = 1; i < layerSolvers.size(); i++)
			{
				//layerSolvers[i].DownsamplePoissonFrom(layerSolvers[i - 1]);

				float residual = layerSolvers[i - 1].ComputeResiduals();
				//std::cout << "level: " << i - 1 << " residual: " << residual << "\n";
				layerSolvers[i].DynamicDownsample(layerSolvers[i - 1]);
			}
			//std::cout << "\n";

			for (size_t i = 0; i < layerSolvers.size(); i++)
			{
				size_t layerIndex = layerSolvers.size() - 1 - i;
				//if(layerIndex == 2)
				//layerSolvers[layerIndex].IteratePressureSolver(iterationsCount, (layerIndex == 0));
				float maxDelta = 0.0f;
				//if (layerIndex == 3)
					maxDelta = layerSolvers[layerIndex].IterateGaussSeidel( static_cast< size_t >( iterationsCount * pow(4, layerIndex) ) );
					//maxDelta = layerSolvers[layerIndex].IterateGaussSeidel(2 * pow(4, layerIndex));


				float residual = layerSolvers[layerIndex].ComputeResiduals();
				//std::cout << "level: " << layerIndex << " residual after solvle: " << residual << " maxDelta: " << maxDelta << "\n";

				if (layerIndex > 0)
				{
					//layerSolvers[layerIndex - 1].UpsamplePoissonFrom(layerSolvers[layerIndex]);
					layerSolvers[layerIndex - 1].Upsample(layerSolvers[layerIndex]);
				}
			}
			//std::cout << "\n";
		}

		RegularStorage<NodeT> &GetValues()
		{
			return layerSolvers[0].GetValues();
		}
		RegularStorage<NodeT> &GetRightSides()
		{
			return layerSolvers[0].GetRightSides();
		}

		void SetEquationMatrix(Vector2i nodeIndex, const Pattern3x3<float> &equationMatrix)
		{
			layerSolvers[0].GetEquationMatrices().SetNode(nodeIndex, equationMatrix);
		}

		void SetupLayers()
		{
			for (size_t i = 1; i < layerSolvers.size(); i++)
			{
				layerSolvers[i].StaticDownsample(layerSolvers[i - 1]);
			}
		}
	private:
		std::vector<MultigridLayer<NodeT>> layerSolvers;
	};
}