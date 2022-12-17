#include "Common/Utility/MurmurHash2.h"
#include "Common/Utility/Logger/Logger.h"

#include "Visual/Device/Buffer.h"

#include "Uniform.h"

namespace Renderer::DrawCalls
{
	namespace
	{
		unsigned MergeHash(const unsigned a, const unsigned b)
		{
			unsigned ab[2] = { a, b };
			return MurmurHash2(ab, sizeof(ab), 0x34322);
		}

		static size_t DetermineSize(Device::UniformInput::Type type)
		{
			switch (type)
			{
				case Device::UniformInput::Type::Bool: return sizeof(bool);
				case Device::UniformInput::Type::Int: return sizeof(int);
				case Device::UniformInput::Type::UInt: return sizeof(unsigned);
				case Device::UniformInput::Type::Float: return sizeof(float);
				case Device::UniformInput::Type::Vector2: return sizeof(simd::vector2);
				case Device::UniformInput::Type::Vector3: return sizeof(simd::vector3);
				case Device::UniformInput::Type::Vector4: return sizeof(simd::vector4);
				case Device::UniformInput::Type::Matrix: return sizeof(simd::matrix);
				case Device::UniformInput::Type::Spline5: return sizeof(simd::Spline5);
				default: throw std::runtime_error("Invalid uniform type");
			}
		}

		static Device::UniformInput BuildInput(const Device::UniformInput::Type type, const uint64_t key, const uint8_t* value)
		{
			switch (type)
			{
				case Device::UniformInput::Type::Bool: return Device::UniformInput(key).SetBool((bool*)value);
				case Device::UniformInput::Type::Int: return Device::UniformInput(key).SetInt((int*)value);
				case Device::UniformInput::Type::UInt: return Device::UniformInput(key).SetUInt((unsigned*)value);
				case Device::UniformInput::Type::Float: return Device::UniformInput(key).SetFloat((float*)value);
				case Device::UniformInput::Type::Vector2: return Device::UniformInput(key).SetVector((simd::vector2*)value);
				case Device::UniformInput::Type::Vector3: return Device::UniformInput(key).SetVector((simd::vector3*)value);
				case Device::UniformInput::Type::Vector4: return Device::UniformInput(key).SetVector((simd::vector4*)value);
				case Device::UniformInput::Type::Matrix: return Device::UniformInput(key).SetMatrix((simd::matrix*)value);
				case Device::UniformInput::Type::Spline5: return Device::UniformInput(key).SetSpline5((simd::Spline5*)value);
				default: throw std::runtime_error("Invalid uniform type");
			}
		}

	}

	Uniform::Uniform(bool value)
		: type(Device::UniformInput::Type::Bool)
	{
		*(bool*)storage.data() = value;
	}

	Uniform::Uniform(int value)
		: type(Device::UniformInput::Type::Int)
	{
		*(int*)storage.data() = value;
	}

	Uniform::Uniform(unsigned value)
		: type(Device::UniformInput::Type::UInt)
	{
		*(unsigned*)storage.data() = value;
	}

	Uniform::Uniform(float value)
		: type(Device::UniformInput::Type::Float)
	{
		*(float*)storage.data() = value;
	}

	Uniform::Uniform(const simd::vector2& value)
		: type(Device::UniformInput::Type::Vector2)
	{
		*(simd::vector2*)storage.data() = value;
	}

	Uniform::Uniform(const simd::vector3& value)
		: type(Device::UniformInput::Type::Vector3)
	{
		*(simd::vector3*)storage.data() = value;
	}

	Uniform::Uniform(const simd::vector4& value)
		: type(Device::UniformInput::Type::Vector4)
	{
		*(simd::vector4*)storage.data() = value;
	}

	Uniform::Uniform(const simd::matrix& value)
		: type(Device::UniformInput::Type::Matrix)
	{
		*(simd::matrix*)storage.data() = value;
	}

	Uniform::Uniform(const simd::Spline5& value)
		: type(Device::UniformInput::Type::Spline5)
	{
		*(simd::Spline5*)storage.data() = value;
	}

	Uniform::Uniform(const Device::UniformInput& input)
		: type(input.type)
	{
		memcpy(storage.data(), input.value, input.GetSize());
	}

	size_t Uniform::GetSize() const
	{
		return DetermineSize(type);
	}

	const uint8_t* Uniform::GetData() const
	{
		switch (type)
		{
			case Device::UniformInput::Type::Bool: return storage.data();
			case Device::UniformInput::Type::Int: return storage.data();
			case Device::UniformInput::Type::UInt: return storage.data();
			case Device::UniformInput::Type::Float: return storage.data();
			case Device::UniformInput::Type::Vector2: return storage.data();
			case Device::UniformInput::Type::Vector3: return storage.data();
			case Device::UniformInput::Type::Vector4: return storage.data();
			case Device::UniformInput::Type::Matrix: return storage.data();
			case Device::UniformInput::Type::Spline5: return storage.data();
			default: throw std::runtime_error("Invalid uniform type");
		}
	}

	Device::UniformInput Uniform::ToInput(const uint64_t key) const
	{
		return BuildInput(type, key, storage.data());
	}

	void Uniform::Tweak(const Uniform& other)
	{
		storage = other.storage;
	}


	void Uniforms::Clear()
	{
		entries.clear();
		storage.clear();
	}

	Uniforms& Uniforms::Add(const uint64_t key, const Uniform&& value)
	{
		const auto offset = (uint16_t)storage.size();
		entries.emplace_back(key, value.GetHash(), offset, value.GetType());
		storage.resize(storage.size() + value.GetSize());
		assert(storage.size() < std::numeric_limits<uint16_t>::max());
		memcpy(&storage[offset], value.GetData(), value.GetSize());
		return *this;
	}

	Uniforms& Uniforms::Add(const Uniforms& uniforms)
	{
		const auto offset = (uint16_t)storage.size();
		entries.reserve(entries.size() + uniforms.entries.size());
		storage.resize(storage.size() + uniforms.storage.size());
		assert(storage.size() < std::numeric_limits<uint16_t>::max());

		for (const auto& entry : uniforms.entries)
			entries.emplace_back(entry.key, entry.hash, offset + entry.offset, entry.type);
		if (uniforms.storage.size() > 0)
			memcpy(&storage[offset], uniforms.storage.data(), uniforms.storage.size());

		return *this;
	}

	void Uniforms::Append(Device::UniformInputs& inputs) const
	{
		for (unsigned i = 0; i < entries.size(); ++i)
		{
			inputs.push_back(BuildInput(entries[i].type, entries[i].key, &storage[entries[i].offset]));
		}
	}

	void Uniforms::Patch(const Uniforms& _uniforms)
	{
		for (unsigned i = 0; i < _uniforms.entries.size(); ++i)
		{
			for (unsigned j = 0; j < entries.size(); ++j)
			{
				if (entries[j].key == _uniforms.entries[i].key)
				{
					memcpy(&storage[entries[j].offset], &_uniforms.storage[_uniforms.entries[i].offset], DetermineSize(entries[j].type));
					break;
				}
			}
		}
	}

	void Uniforms::Patch(uint64_t key, const Uniform&& value)
	{
		for (unsigned j = 0; j < entries.size(); ++j)
		{
			if (entries[j].key == key)
			{
				memcpy(&storage[entries[j].offset], value.GetData(), value.GetSize());
				break;
			}
		}
	}

	void Uniforms::Tweak(unsigned hash, const Uniform& value)
	{
		for (unsigned j = 0; j < entries.size(); ++j)
		{
			if (entries[j].hash == hash)
			{
				memcpy(&storage[entries[j].offset], value.GetData(), value.GetSize());
				break;
			}
		}
	}

	unsigned Uniforms::Hash() const
	{
		unsigned hash = 0;

		for (unsigned j = 0; j < entries.size(); ++j)
		{
			hash = MergeHash(hash, (unsigned)(0xffffffff & entries[j].key));
			hash = MergeHash(hash, (unsigned)((0xffffffff00000000 & entries[j].key) >> 32));
		}
		hash = MergeHash(hash, MurmurHash2(storage.data(), (int)storage.size(), 0x34322));

		return hash;
	}


	Binding::Binding(uint64_t id_hash, ::Texture::Handle texture_handle)
		: type(Device::BindingInput::Type::Texture)
		, id_hash(id_hash)
		, texture_handle(texture_handle)
	{
	}

	Binding::Binding(uint64_t id_hash, Device::Handle<Device::Texture> texture)
		: type(Device::BindingInput::Type::Texture)
		, id_hash(id_hash)
		, texture(texture)
	{
	}

	Binding::Binding(uint64_t id_hash, Device::Handle<Device::TexelBuffer> texel_buffer)
		: type(Device::BindingInput::Type::TexelBuffer)
		, id_hash(id_hash)
		, texel_buffer(texel_buffer)
	{
	}

	Binding::Binding(uint64_t id_hash, Device::Handle<Device::ByteAddressBuffer> byte_address_buffer)
		: type(Device::BindingInput::Type::ByteAddressBuffer)
		, id_hash(id_hash)
		, byte_address_buffer(byte_address_buffer)
	{
	}

	Binding::Binding(uint64_t id_hash, Device::Handle<Device::StructuredBuffer> structured_buffer)
		: type(Device::BindingInput::Type::StructuredBuffer)
		, id_hash(id_hash)
		, structured_buffer(structured_buffer)
	{
	}

	Device::BindingInput Binding::ToInput() const
	{
		switch (type)
		{
		case Device::BindingInput::Type::Texture:
		{
			if (texture)
				return Device::BindingInput(id_hash).SetTexture(texture);
			return Device::BindingInput(id_hash).SetTextureResource(texture_handle);
		}
		case Device::BindingInput::Type::TexelBuffer: return Device::BindingInput(id_hash).SetTexelBuffer(texel_buffer);
		case Device::BindingInput::Type::ByteAddressBuffer: return Device::BindingInput(id_hash).SetByteAddressBuffer(byte_address_buffer);
		case Device::BindingInput::Type::StructuredBuffer: return Device::BindingInput(id_hash).SetStructuredBuffer(structured_buffer);
		default: throw std::runtime_error("Invalid binding input type");
		}

	}

	uint32_t Binding::Hash() const
	{
		uint32_t hash = 0;
		assert(id_hash);
		hash = MergeHash(hash, (unsigned)(0xffffffff & id_hash));
		hash = MergeHash(hash, (unsigned)((0xffffffff00000000 & id_hash) >> 32));
		hash = MergeHash(hash, (unsigned)type);
		switch (type)
		{
		case Device::BindingInput::Type::Texture:
		{
			if (texture)
				hash = MergeHash(hash, (unsigned)texture->GetID());
			else
				hash = MergeHash(hash, (unsigned)texture_handle.GetHash());
			break;
		}
		case Device::BindingInput::Type::TexelBuffer: hash = MergeHash(hash, (unsigned)texel_buffer->GetID()); break;
		case Device::BindingInput::Type::ByteAddressBuffer: hash = MergeHash(hash, (unsigned)byte_address_buffer->GetID()); break;
		case Device::BindingInput::Type::StructuredBuffer: hash = MergeHash(hash, (unsigned)structured_buffer->GetID()); break;
		default: throw std::runtime_error("Invalid binding input type");
		}
		return hash;
	}

	unsigned Bindings::Hash() const
	{
		uint32_t hash = 0;
		for (const auto& a : *this)
			hash = MergeHash(hash, a.Hash());
		return hash;
	}

}