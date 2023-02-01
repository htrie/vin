#pragma once

#include "Visual/Device/State.h"
#include "Visual/Renderer/DrawCalls/DrawDataNames.h"

namespace Renderer::DrawCalls
{
	class Uniform
	{
		static constexpr size_t storage_size = std::max({
			sizeof(bool),
			sizeof(int),
			sizeof(unsigned),
			sizeof(float),
			sizeof(simd::vector4),
			sizeof(simd::matrix),
			sizeof(simd::Spline5)
		});

		Device::UniformInput::Type type = Device::UniformInput::Type::None;
		uint32_t hash = 0;
		std::array<uint8_t, storage_size> storage;

	public:
		Uniform() {}
		Uniform(bool value);
		Uniform(int value);
		Uniform(unsigned value);
		Uniform(float value);
		Uniform(const simd::vector2& value);
		Uniform(const simd::vector3& value);
		Uniform(const simd::vector4& value);
		Uniform(const simd::matrix& value);
		Uniform(const simd::Spline5& value);

		Uniform(const Device::UniformInput& input);

		Uniform& SetHash(const uint32_t hash) { this->hash = hash; return *this; }

		Device::UniformInput::Type GetType() const { return type; }
		uint32_t GetHash() const { return hash; }
		size_t GetSize() const;
		const uint8_t* GetData() const;

		Device::UniformInput ToInput(const uint64_t key) const;

		void Tweak(const Uniform& other);
	};

	class Uniforms
	{
		struct Entry
		{
			uint64_t key = 0;
			uint32_t hash = 0;
			uint16_t offset = 0;
			Device::UniformInput::Type type = Device::UniformInput::Type::None;

			Entry(uint64_t key, uint32_t hash, uint16_t offset, Device::UniformInput::Type type)
				: key(key), hash(hash), offset(offset), type(type) {}
		};

		Memory::SmallVector<Entry, 8, Memory::Tag::DrawCalls> dynamic_entries;
		Memory::SmallVector<Entry, 8, Memory::Tag::DrawCalls> static_entries;
		Memory::SmallVector<uint8_t, 256, Memory::Tag::DrawCalls> storage;

	public:
		void Clear();

		Uniforms& AddDynamic(const uint64_t key, const Uniform&& value);
		Uniforms& AddStatic(const uint64_t key, const Uniform&& value);
		Uniforms& Add(const Uniforms& uniforms);

		void Append(Device::UniformInputs& inputs) const;

		void Patch(const Uniforms& _uniforms);
		void Patch(uint64_t key, const Uniform&& value);

		void Tweak(unsigned hash, const Uniform& value);

		size_t GetSize() const { return dynamic_entries.size() + static_entries.size(); }

		unsigned Hash() const;
	};


	class Binding
	{
		Device::BindingInput::Type type = Device::BindingInput::Type::None;
		uint64_t id_hash = 0;
		::Texture::Handle texture_handle; // [TODO] Make bindings use tight storage (Device::Handle<Device::Resource> parent type?)
		Device::Handle<Device::Texture> texture;
		Device::Handle<Device::TexelBuffer> texel_buffer;
		Device::Handle<Device::ByteAddressBuffer> byte_address_buffer;
		Device::Handle<Device::StructuredBuffer> structured_buffer;

	public:
		Binding() {}
		Binding(uint64_t id_hash, ::Texture::Handle texture_handle);
		Binding(uint64_t id_hash, Device::Handle<Device::Texture> texture);
		Binding(uint64_t id_hash, Device::Handle<Device::TexelBuffer> texel_buffer);
		Binding(uint64_t id_hash, Device::Handle<Device::ByteAddressBuffer> byte_address_buffer);
		Binding(uint64_t id_hash, Device::Handle<Device::StructuredBuffer> structured_buffer);

		Device::BindingInput ToInput() const;

		uint32_t Hash() const;
	};

	struct Bindings : public Memory::SmallVector<Binding, 4, Memory::Tag::DrawCalls>
	{
		typedef Memory::SmallVector<Binding, 4, Memory::Tag::DrawCalls> Parent;
		using Parent::Parent; // Use constructors of SmallVector, so we can keep using initialiser-list based constructor/etc

		unsigned Hash() const;
	};

}