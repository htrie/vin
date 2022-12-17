#pragma once

#include "Visual/Device/Constants.h"
#include "Visual/Mesh/VertexLayout.h"

namespace Device
{

	enum class DeclType : uint8_t
	{
		FLOAT1,
		FLOAT2,
		FLOAT3,
		FLOAT4,
		D3DCOLOR,
		UBYTE4,
		SHORT2,
		SHORT4,
		UBYTE4N,
		BYTE4N,
		SHORT2N,
		SHORT4N,
		USHORT2N,	
		USHORT4N,
		UDEC3,
		DEC3N,
		FLOAT16_2,
		FLOAT16_4,
	};

	enum class DeclUsage : uint8_t
	{
		POSITION,
		BLENDWEIGHT,
		BLENDINDICES,
		NORMAL,
		PSIZE,
		TEXCOORD,
		TANGENT,
		BINORMAL,
		TESSFACTOR,
		POSITIONT,
		COLOR,
		FOG,
		DEPTH,
		SAMPLE,
		UNUSED,
	};

	struct VertexElement
	{
		WORD Stream;
		WORD Offset;
		DeclType Type;
		DeclUsage Usage;
		BYTE UsageIndex;
	};

#if defined(DEBUG)
	typedef std::vector<VertexElement> VertexElements;
#else
	typedef Memory::SmallVector<VertexElement, 8, Memory::Tag::Device> VertexElements;
#endif


	class VertexDeclaration
	{
		static inline std::atomic_uint32_t monotonic_id = { 1 };
		uint32_t id = monotonic_id++;

		VertexElements elements;
		uint32_t elements_hash = 0;
		size_t stride = 0;

	public:
		VertexDeclaration() {}
		VertexDeclaration(const Memory::Span<const VertexElement>& vertex_elements);

		const VertexElements& GetElements() const { return elements; }
		uint32_t GetElementsHash() const { return elements_hash; }
		size_t GetStride() const { return stride; }

		const VertexElement* FindElement(DeclUsage usage, unsigned usage_index) const;

		static UINT GetVertexStride(const VertexElement* pDecl, DWORD Stream);

		static unsigned GetTypeSize(DeclType decl_type);

		uint32_t GetID() const { return id; }
	};

	Device::VertexElements SetupVertexLayoutElements(const Mesh::VertexLayout& layout);

}
