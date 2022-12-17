#pragma once
#include "../Maths/VectorMaths.h"

namespace Physics
{
	struct FluidSystem
	{
	protected:
		FluidSystem() {}
	public:
		static std::unique_ptr<FluidSystem> Create();
		virtual ~FluidSystem() {}

		struct NodeDynamicData
		{
			Vector2f velocity;
			float pressure;
			float substance;
		};

		virtual void SetSolverParams(size_t relaxationIterations) = 0;
		virtual void SetSize(Vector2<int> size, Vector2f areaSize) = 0;

		enum struct NodeTypes
		{
			Free,
			FixedVelocity,
			FixedPressure,
			SoftVelocity,
			SoftPressure
		};
		struct NodeStaticData
		{
			NodeTypes nodeType;
			float source;
		};
		virtual void SetDynamicData(const NodeDynamicData *srcNodeData) = 0;
		virtual void GetDynamicData(NodeDynamicData *dstNodeData) = 0;
		virtual void SetStaticData(const NodeStaticData *srcNodeData) = 0;


		virtual void Update(float dt) = 0;
	};
}