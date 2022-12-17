#pragma once

#include <string>
#include <memory>
#include <vector>

#include "Visual/Device/Shader.h"
#include "Visual/Renderer/ShaderCompilerStatistics.h"

namespace Renderer
{

	class ShaderCache
	{
	public:
		ShaderCache( const std::wstring& dirname, bool async );
		~ShaderCache();

		///Gets the data from a cached shader
		Device::ShaderData GetCachedShader( const std::wstring& cache_key ) const;

#ifndef CONSOLE
		static void BuildShaderCacheFromDirectory( const std::wstring& directory, const std::wstring& output_directory, const bool create_diff, ShaderCompiler::Statistics* statistics = nullptr);
		static void ValidateShaderCache( const std::wstring& output_directory );
#endif
		void ReadShaderCacheFiles(const std::wstring& dirname);

		using Data = Memory::Vector<char, Memory::Tag::ShaderCache>;

	private:
		enum
		{
			CacheKeyLength = 32,
		};

		struct Entry;
		struct Header;

		const Entry* GetCachedEntry( const uint8_t (&lookup_cache_key)[ CacheKeyLength ] ) const;
		Device::ShaderData DecompressShader(const Entry& entry) const;

		std::atomic_uint job_count = {0};

		std::vector< char > dictionary;

		std::unique_ptr< unsigned char[] > index_data;
		
		const Header* header;
		const Entry* entries;
		std::vector<Data> tables;
		std::vector<Data> datas;

		void EmptyHeader();
	};

	
}
