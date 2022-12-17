#pragma once

namespace Device::ReflectionD3D
{
	struct ResourceInfo {
		uint64_t id_hash;
		uint32_t binding_point;
		uint32_t size;
		D3D_SHADER_INPUT_TYPE type;
		D3D_SRV_DIMENSION dimension;

		ResourceInfo() {}
		void Read(std::istringstream& stream);
		void Write(std::ostringstream& stream);
	};

	struct UniformInfo {
		uint64_t id_hash;
		uint32_t slot;
		uint32_t offset;
		uint32_t size;

		UniformInfo() {}
		void Read(std::istringstream& stream);
		void Write(std::ostringstream& stream);
	};

	struct ConstantInfo
	{
		uint32_t name_hash;
		uint8_t buffer_index;
		uint32_t offset;
		uint32_t size;
		D3D_SRV_DIMENSION type;

		ConstantInfo() {}

		void Read(std::istringstream& stream);
		void Write(std::ostringstream& stream);
	};

	void ReadString(std::istringstream& stream, std::string& out_string)
	{
		uint8_t len = 0;
		stream.read((char*)&len, sizeof(uint8_t));
		char* temp_string = new char[len];
		stream.read((char*)temp_string, len);
		out_string = std::string(temp_string, len);
		delete[] temp_string;
	}

	void WriteString(std::ostringstream& stream, const std::string in_string)
	{
		uint8_t len = (uint8_t)(in_string.length()); // can be just 8 bits, types/semantics/names shoudn't be longer than 255 characters
		assert(len < 256);
		stream.write((char*)&len, sizeof(uint8_t));
		stream.write((char*)in_string.c_str(), len);
	}

	struct ReflectionInfo
	{
		Memory::VectorMap<std::string, Device::ShaderParameter, Memory::Tag::ShaderMetadata> output_variables;
		Memory::VectorMap<uint64_t, ResourceInfo, Memory::Tag::ShaderMetadata> resource_infos;
		Memory::VectorMap<uint64_t, UniformInfo, Memory::Tag::ShaderMetadata> uniform_infos;
		uint32_t instruction_count = 0;
		UINT group_size_x = 0;
		UINT group_size_y = 0;
		UINT group_size_z = 0;

		void Write(std::ostringstream& stream)
		{
			// write the current version
			stream.write((char*)&Compiler::BytecodeVersionD3D12, sizeof(uint8_t));

			uint32_t group_size[3] = { (uint32_t)group_size_x, (uint32_t)group_size_y, (uint32_t)group_size_z };
			stream.write((char*)&group_size[0], sizeof(uint32_t));
			stream.write((char*)&group_size[1], sizeof(uint32_t));
			stream.write((char*)&group_size[2], sizeof(uint32_t));

			uint8_t size = (uint8_t)output_variables.size();
			stream.write((char*)&size, sizeof(uint8_t)); // can be just 8 bits, input semantics can only reach 16
			for (const auto& pair : output_variables)
			{
				WriteString(stream, pair.second.type);
				WriteString(stream, pair.second.name);
				WriteString(stream, pair.second.semantic);
			}

			uint16_t resource_size = (uint16_t)resource_infos.size();	// can be just 16 bits, don't think there can be more than 65536 constants
			stream.write((char*)&resource_size, sizeof(uint16_t));
			for (auto& rsc : resource_infos)
				rsc.second.Write(stream);

			uint16_t uniform_size = (uint16_t)uniform_infos.size();	// can be just 16 bits, don't think there can be more than 65536 constants
			stream.write((char*)&uniform_size, sizeof(uint16_t));
			for (auto& uniform : uniform_infos)
				uniform.second.Write(stream);

			stream.write((char*)&instruction_count, sizeof(uint32_t));
		}
	};

	void ResourceInfo::Read(std::istringstream& stream)
	{
		stream.read((char*)&id_hash, sizeof(uint64_t));
		stream.read((char*)&binding_point, sizeof(uint32_t));
		stream.read((char*)&size, sizeof(uint32_t));
		uint32_t _type;
		stream.read((char*)&_type, sizeof(uint32_t));
		type = (D3D_SHADER_INPUT_TYPE)_type;

		uint32_t _dimension;
		stream.read((char*)&_dimension, sizeof(uint32_t));
		dimension = (D3D_SRV_DIMENSION)_dimension;
	}

	void ResourceInfo::Write(std::ostringstream& stream)
	{
		stream.write((char*)&id_hash, sizeof(uint64_t));
		stream.write((char*)&binding_point, sizeof(uint32_t));
		stream.write((char*)&size, sizeof(uint32_t));

		uint32_t _type = (uint32_t)type;
		stream.write((char*)&_type, sizeof(uint32_t));

		uint32_t _dimension = (uint32_t)dimension;
		stream.write((char*)&_dimension, sizeof(uint32_t));
	}

	void UniformInfo::Read(std::istringstream& stream)
	{
		stream.read((char*)&id_hash, sizeof(uint64_t));
		stream.read((char*)&slot, sizeof(uint32_t));
		stream.read((char*)&offset, sizeof(uint32_t));
		stream.read((char*)&size, sizeof(uint32_t));
	}

	void UniformInfo::Write(std::ostringstream& stream)
	{
		stream.write((char*)&id_hash, sizeof(uint64_t));
		stream.write((char*)&slot, sizeof(uint32_t));
		stream.write((char*)&offset, sizeof(uint32_t));
		stream.write((char*)&size, sizeof(uint32_t));
	}

	void ConstantInfo::Read(std::istringstream& stream)
	{
		stream.read((char*)&name_hash, sizeof(uint32_t));
		stream.read((char*)&buffer_index, sizeof(uint8_t)); // can be just 8 bits, since max of this is just 15
		stream.read((char*)&offset, sizeof(uint32_t));
		stream.read((char*)&size, sizeof(uint32_t));
		uint8_t temp_type = 0;
		stream.read((char*)&temp_type, sizeof(uint8_t));
		type = (D3D_SRV_DIMENSION)temp_type;
	}

	void ConstantInfo::Write(std::ostringstream& stream)
	{
		stream.write((char*)&name_hash, sizeof(uint32_t));
		stream.write((char*)&buffer_index, sizeof(uint8_t)); // can be just 8 bits, since max of this is just 15
		stream.write((char*)&offset, sizeof(uint32_t));
		stream.write((char*)&size, sizeof(uint32_t));
		stream.write((char*)&type, sizeof(uint8_t));
	}

}