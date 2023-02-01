
namespace Device
{

	unsigned VertexDeclaration::GetTypeSize(DeclType decl_type)
	{
		switch (decl_type)
		{
		case DeclType::FLOAT1:		return 4;
		case DeclType::FLOAT2:		return 4 * 2;
		case DeclType::FLOAT3:		return 4 * 3;
		case DeclType::FLOAT4:		return 4 * 4;
		case DeclType::D3DCOLOR:	return 4;
		case DeclType::UBYTE4:		return 1 * 4;
		case DeclType::SHORT2:		return 2 * 2;
		case DeclType::SHORT4:		return 2 * 4;
		case DeclType::UBYTE4N:		return 1 * 4;
		case DeclType::BYTE4N:		return 1 * 4;
		case DeclType::SHORT2N:		return 2 * 2;
		case DeclType::SHORT4N:		return 2 * 4;
		case DeclType::USHORT2N:	return 2 * 2;
		case DeclType::USHORT4N:	return 2 * 4;
		case DeclType::UDEC3:		return 4;
		case DeclType::DEC3N:		return 4;
		case DeclType::FLOAT16_2:	return 2 * 2;
		case DeclType::FLOAT16_4:	return 2 * 4;
		default:					throw std::runtime_error("Unsupported vertex declaration type");
		}
	}

	VertexDeclaration::VertexDeclaration(const Memory::Span<const VertexElement>& vertex_elements)
		: elements(vertex_elements.begin(), vertex_elements.end())
	{
		for (auto& element : elements)
		{
			assert(element.Stream < 2);
			stride = element.Offset + GetTypeSize(element.Type);
		}

		elements_hash = MurmurHash2(elements.data(), (int)(sizeof(VertexElement) * elements.size()), 0xc58f1a7b);
	}

	const VertexElement* VertexDeclaration::FindElement(DeclUsage usage, unsigned usage_index) const
	{
		for (auto& element : elements)
		{
			if ((element.Usage == usage) && (element.UsageIndex == usage_index))
				return &element;
		}
		return nullptr;
	}

	Device::VertexElements SetupVertexLayoutElements(const Mesh::VertexLayout& layout)
	{
		assert(layout.HasNormal() && layout.HasTangent() && layout.HasUV1()); // Don't have mesh files without uv1/tbn yet

		Device::VertexElements vertex_elements;

		vertex_elements.push_back({ 0, (WORD)layout.GetPositionOffset(), Device::DeclType::FLOAT3, Device::DeclUsage::POSITION, 0 });
		vertex_elements.push_back({ 0, (WORD)layout.GetNormalOffset(), Device::DeclType::BYTE4N, Device::DeclUsage::NORMAL, 0 });
		vertex_elements.push_back({ 0, (WORD)layout.GetTangentOffset(), Device::DeclType::BYTE4N, Device::DeclUsage::TANGENT, 0 });
		vertex_elements.push_back({ 0, (WORD)layout.GetUV1Offset(), Device::DeclType::FLOAT16_2, Device::DeclUsage::TEXCOORD, 0 });

		if (layout.HasBoneWeight())
		{
			vertex_elements.push_back({ 0, (WORD)layout.GetBoneIndexOffset(), Device::DeclType::UBYTE4, Device::DeclUsage::BLENDINDICES, 0 });
			vertex_elements.push_back({ 0, (WORD)layout.GetBoneWeightOffset(), Device::DeclType::UBYTE4N, Device::DeclUsage::BLENDWEIGHT, 0 });
		}

		if (layout.HasUV2()) vertex_elements.push_back({ 0, (WORD)layout.GetUV2Offset(), Device::DeclType::FLOAT16_2, Device::DeclUsage::COLOR, 3 });
		if (layout.HasColor()) vertex_elements.push_back({ 0, (WORD)layout.GetColorOffset(), Device::DeclType::UBYTE4N, Device::DeclUsage::COLOR, 0 });

		return vertex_elements;
	}

}
