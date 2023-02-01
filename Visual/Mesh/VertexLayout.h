#pragma once

#include "Common/FileReader/Vector4dByte.h"

namespace Mesh
{

	// Defines vertex layout based on flags stored in the mesh header
	// Used by fixed, skinned and terrain tile meshes
	class VertexLayout
	{
	public:
		static constexpr uint32_t FLAG_HAS_UV2 = 1 << 0;
		static constexpr uint32_t FLAG_HAS_COLOR = 1 << 1;
		static constexpr uint32_t FLAG_HAS_BONE_WEIGHT = 1 << 2;
		static constexpr uint32_t FLAG_HAS_UV1 = 1 << 3;
		static constexpr uint32_t FLAG_HAS_NORMAL = 1 << 4;
		static constexpr uint32_t FLAG_HAS_TANGENT = 1 << 5;
		static constexpr uint32_t FLAG_DEFAULT = FLAG_HAS_TANGENT | FLAG_HAS_NORMAL | FLAG_HAS_UV1;
		
		explicit VertexLayout(uint32_t flags)
			: vertex_size(sizeof(simd::vector3))
			, normal_offset(-1)
			, tangent_offset(-1)
			, uv1_offset(-1)
			, uv2_offset(-1)
			, color_offset(-1)
			, bone_index_offset(-1)
			, bone_weight_offset(-1)
		{
			if (flags & FLAG_HAS_NORMAL)
			{
				normal_offset = vertex_size;
				vertex_size += sizeof(Vector4dByte);
			}

			if (flags & FLAG_HAS_TANGENT)
			{
				tangent_offset = vertex_size;
				vertex_size += sizeof(Vector4dByte);
			}

			if (flags & FLAG_HAS_UV1)
			{
				uv1_offset = vertex_size;
				vertex_size += sizeof(simd::vector2_half);
			}

			if (flags & FLAG_HAS_BONE_WEIGHT)
			{
				bone_index_offset = vertex_size;
				vertex_size += sizeof(Vector4dByte);

				bone_weight_offset = vertex_size;
				vertex_size += sizeof(Vector4dByte);
			}

			if (flags & FLAG_HAS_UV2)
			{
				uv2_offset = vertex_size;
				vertex_size += sizeof(simd::vector2_half);
			}

			if (flags & FLAG_HAS_COLOR)
			{
				color_offset = vertex_size;
				vertex_size += sizeof(Vector4dByte);
			}
		}

		const simd::vector3& GetPosition(uint32_t index, const uint8_t* data) const { return reinterpret_cast<const simd::vector3&>(data[index * vertex_size + position_offset]); };
		simd::vector3& GetPosition(uint32_t index, uint8_t* data) const { return reinterpret_cast<simd::vector3&>(data[index * vertex_size + position_offset]); };
		const Vector4dByte& GetNormal(uint32_t index, const uint8_t* data) const { assert(normal_offset != -1); return reinterpret_cast<const Vector4dByte&>(data[index * vertex_size + normal_offset]); };
		Vector4dByte& GetNormal(uint32_t index, uint8_t* data) const { assert(normal_offset != -1); return reinterpret_cast<Vector4dByte&>(data[index * vertex_size + normal_offset]); };
		const Vector4dByte& GetTangent(uint32_t index, const uint8_t* data) const { assert(tangent_offset != -1); return reinterpret_cast<const Vector4dByte&>(data[index * vertex_size + tangent_offset]); };
		Vector4dByte& GetTangent(uint32_t index, uint8_t* data) const { assert(tangent_offset != -1); return reinterpret_cast<Vector4dByte&>(data[index * vertex_size + tangent_offset]); };
		const simd::vector2_half& GetUV1(uint32_t index, const uint8_t* data) const { assert(uv1_offset != -1); return reinterpret_cast<const simd::vector2_half&>(data[index * vertex_size + uv1_offset]); };
		simd::vector2_half& GetUV1(uint32_t index, uint8_t* data) const { assert(uv1_offset != -1); return reinterpret_cast<simd::vector2_half&>(data[index * vertex_size + uv1_offset]); };
		const simd::vector2_half& GetUV2(uint32_t index, const uint8_t* data) const { assert(uv2_offset != -1); return reinterpret_cast<const simd::vector2_half&>(data[index * vertex_size + uv2_offset]); };
		simd::vector2_half& GetUV2(uint32_t index, uint8_t* data) const { assert(uv2_offset != -1); return reinterpret_cast<simd::vector2_half&>(data[index * vertex_size + uv2_offset]); };
		const Vector4dByte& GetColor(uint32_t index, const uint8_t* data) const { assert(color_offset != -1); return reinterpret_cast<const Vector4dByte&>(data[index * vertex_size + color_offset]); };
		Vector4dByte& GetColor(uint32_t index, uint8_t* data) const { assert(color_offset != -1); return reinterpret_cast<Vector4dByte&>(data[index * vertex_size + color_offset]); };
		const Vector4dByte& GetBoneIndex(uint32_t index, const uint8_t* data) const { assert(bone_index_offset != -1); return reinterpret_cast<const Vector4dByte&>(data[index * vertex_size + bone_index_offset]); };
		Vector4dByte& GetBoneIndex(uint32_t index, uint8_t* data) const { assert(bone_index_offset != -1); return reinterpret_cast<Vector4dByte&>(data[index * vertex_size + bone_index_offset]); };
		const Vector4dByte& GetBoneWeight(uint32_t index, const uint8_t* data) const { assert(bone_weight_offset != -1); return reinterpret_cast<const Vector4dByte&>(data[index * vertex_size + bone_weight_offset]); };
		Vector4dByte& GetBoneWeight(uint32_t index, uint8_t* data) const { assert(bone_weight_offset != -1); return reinterpret_cast<Vector4dByte&>(data[index * vertex_size + bone_weight_offset]); };

		constexpr uint32_t GetPositionOffset() const { return position_offset; }
		uint32_t GetNormalOffset() const { return normal_offset; }
		uint32_t GetTangentOffset() const { return tangent_offset; }
		uint32_t GetUV1Offset() const { return uv1_offset; }
		uint32_t GetUV2Offset() const { return uv2_offset; }
		uint32_t GetColorOffset() const { return color_offset; }
		uint32_t GetBoneIndexOffset() const { return bone_index_offset; }
		uint32_t GetBoneWeightOffset() const { return bone_weight_offset; }

		bool HasNormal() const { return normal_offset != -1; }
		bool HasTangent() const { return tangent_offset != -1; }
		bool HasUV1() const { return uv1_offset != -1; }
		bool HasUV2() const { return uv2_offset != -1; }
		bool HasColor() const { return color_offset != -1; }
		bool HasBoneWeight() const { return bone_weight_offset != -1; }

		uint32_t VertexSize() const { return vertex_size; }

	private:
		constexpr static uint32_t position_offset = 0;
		uint32_t normal_offset;
		uint32_t tangent_offset;
		uint32_t uv1_offset;
		uint32_t bone_index_offset;
		uint32_t bone_weight_offset;
		uint32_t uv2_offset;
		uint32_t color_offset;
		uint32_t vertex_size;
	};

}