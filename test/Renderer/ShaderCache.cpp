#include <array>
#include <map>
#include <iomanip>
#include <fstream>

#include "Common/File/FileSystem.h"
#include "Common/File/InputFileStream.h"
#include "Common/File/SHA-256.h"
#include "Common/File/StorageSystem.h"
#include "Common/Utility/Logger/Logger.h"
#include "Common/Utility/BinaryFind.h"
#include "Common/Utility/Profiler/ScopedProfiler.h"
#include "Common/Utility/StringManipulation.h"
#include "Common/Utility/OsAbstraction.h"
#include "Common/Job/JobSystem.h"

#include "Visual/Device/Compiler.h"
#include "Visual/Profiler/JobProfile.h"

#include "Oodle/Oodle.h"

#include "ShaderCache.h"


namespace Renderer
{
	namespace
	{
		enum
		{
			Version = 10,
			MaxShaderCacheSizeInBytes = 64 * 1024 * 1024,
			MaxShaderUncompressedSize = std::numeric_limits<uint32_t>::max(),
		};

		const auto Compressor = OodleLZ_Compressor::OodleLZ_Compressor_Hydra;
		const auto SeekTableFlags = OodleLZSeekTable_Flags::OodleLZSeekTable_Flags_None;
	#if defined(PRODUCTION_CONFIGURATION)
		const auto CompressionLevel = OodleLZ_CompressionLevel::OodleLZ_CompressionLevel_Optimal;
	#else
		const auto CompressionLevel = OodleLZ_CompressionLevel::OodleLZ_CompressionLevel_Normal;
	#endif
		const auto SeekChunkSize = OODLELZ_BLOCK_LEN;

		std::wstring GetIndexFilename(const std::wstring& dirname)
		{
			return dirname + L"/index.dat";
		}

		std::wstring GetShaderFilename(const std::wstring& dirname, const unsigned file_index)
		{
			std::wstringstream shader_filename;
			shader_filename << dirname << L"/" << file_index << L".dat";
			return shader_filename.str();
		}

		void CalculateDeduplicationHash(const unsigned char* data, const size_t size, std::array< char, 32 >& hash_out)
		{
			File::SHA256::Hash((unsigned char*)data, (unsigned int)size, (unsigned char*)hash_out.data());
		}

#if !defined( CONSOLE ) && !defined(__APPLE__) && !defined(ANDROID)
		void WriteShaderCacheFile(const std::wstring& filename, const Memory::Vector<unsigned char, Memory::Tag::ShaderCache>& input) // TODO: Add Archive class.
		{
			auto options = *OodleLZ_CompressOptions_GetDefault(Compressor, CompressionLevel);
			options.seekChunkReset = true;
			options.seekChunkLen = SeekChunkSize;

			Memory::Vector<char, Memory::Tag::ShaderCache> output(OodleLZ_GetCompressedBufferSizeNeeded(input.size()));
			SINTa result = OodleLZ_Compress(Compressor, (uint8_t*)input.data(), input.size(), (uint8_t*)output.data(), CompressionLevel, &options);
			if (result == OODLELZ_FAILED)
				throw std::runtime_error("[SHADER] Oodle: Failed to compress");
			output.resize(result); // Fit.

			std::ofstream output_file(filename, std::ios::out | std::ios::binary);
			const uint32_t input_size = (uint32_t)input.size();
			const uint32_t output_size = (uint32_t)output.size();
			output_file.write((char*)&input_size, sizeof(uint32_t));
			output_file.write((char*)&output_size, sizeof(uint32_t));
			output_file.write((char*)output.data(), output.size());
		}
#endif

		void ReadShaderCacheFile(const std::wstring& filename, ShaderCache::Data* table, ShaderCache::Data* data)
		{
			PROFILE;

			File::InputFileStream stream(filename);
			auto* input = (unsigned char*)stream.GetFilePointer();
			
			const auto uncompressed_size = *(uint32_t*)input; input += sizeof(uint32_t);
			const auto compressed_size = *(uint32_t*)input; input += sizeof(uint32_t);
			const auto file_size = stream.GetFileSize();
			if (compressed_size + (2 * sizeof(uint32_t)) > file_size)
			{
				data->clear();
				table->clear();
				LOG_CRIT(L"Shader Cache File '" << filename << L"' is corrupted");
				return;
			}

			ShaderCache::Data compressed_data(compressed_size);
			memcpy(compressed_data.data(), input, compressed_size);
			input += compressed_size;

			ShaderCache::Data seek_table(OodleLZ_GetSeekTableMemorySizeNeeded(OodleLZ_GetNumSeekChunks(uncompressed_size, SeekChunkSize), SeekTableFlags));
			const bool res = OodleLZ_FillSeekTable((OodleLZ_SeekTable*)seek_table.data(), SeekTableFlags, SeekChunkSize, nullptr, uncompressed_size, (uint8_t*)compressed_data.data(), compressed_data.size());
			if (!res)
				throw std::runtime_error("[SHADER] Oodle: Failed to fill seek table");

			*table = std::move(seek_table);
			*data = std::move(compressed_data);
		}

		Device::ShaderData UncompressPartial(const OodleLZ_SeekTable* seek_table, const ShaderCache::Data& data, size_t offset, size_t size)
		{
			PROFILE;

			assert(size > 0);

			const unsigned block_start_index = OodleLZ_FindSeekEntry(offset, seek_table);
			const unsigned block_end_index = OodleLZ_FindSeekEntry(offset + size - 1, seek_table);
			const size_t block_start_pos = (size_t)OodleLZ_GetSeekEntryPackedPos(block_start_index, seek_table);
			const size_t blocks_max_size = (1 + block_end_index - block_start_index) * SeekChunkSize;
			const size_t blocks_size = std::min(blocks_max_size, (size_t)seek_table->totalRawLen - block_start_index * SeekChunkSize);

			Device::ShaderData tmp(blocks_size);
			const auto res = OodleLZ_Decompress((uint8_t*)data.data() + block_start_pos, blocks_max_size, (uint8_t*)tmp.data(), tmp.size(), OodleLZ_FuzzSafe_No);
			if (res == OODLELZ_FAILED)
				throw std::runtime_error("[SHADER] Oodle: Failed to decompress");

			Device::ShaderData out(size);
			const size_t local_offset = offset % SeekChunkSize;
			memcpy(out.data(), tmp.data() + local_offset, size);
			return out;
		}

		bool first_time = true;
	}


#pragma pack( push, 1 )
	struct ShaderCache::Header
	{
		uint32_t version = Version;
		uint32_t num_shaders = 0;
		uint32_t num_shader_files = 0;
	};

	struct ShaderCache::Entry
	{
		uint8_t cache_key[ ShaderCache::CacheKeyLength ];
		uint32_t offset;
		uint32_t size;
		uint16_t file_index;

		static_assert(std::numeric_limits< decltype(size) >::max() >= MaxShaderUncompressedSize, "Uncompressed size variable not big enough to hold the max shader size");
	};
#pragma pack( pop )

	void ShaderCache::EmptyHeader()
	{
		static Header empty_header;
		header = &empty_header;
		entries = NULL;
	}

	ShaderCache::ShaderCache(const std::wstring& dirname, bool async)
	{
		PROFILE;

#if defined(MOBILE)
		// Do not load shader cache on Mobile for now to save as much memory as we can.
		// We base gather all PoE2 shaders, and that's about 200MB at the moment.
		EmptyHeader();
		return;
#endif

		try
		{
			File::InputFileStream stream(GetIndexFilename(dirname));
			index_data.reset(new unsigned char[stream.GetFileSize()]);
			memcpy(index_data.get(), stream.GetFilePointer(), stream.GetFileSize());

			header = (const Header*)index_data.get();
			if( header->version != Version )
			{
				LOG_WARN("[SHADER] Failed to load cache. Header version mismatch (found " << header->version << L", expected " << Version << L") ");
				EmptyHeader();
				return;
			}

			entries = (const Entry*)(index_data.get() + sizeof(Header));
		}
		catch ( const File::FileNotFound& exception )
		{
			LOG_WARN("[SHADER] Failed to load cache. File not found: " << exception.what());
			EmptyHeader();
		}
		catch ( const std::bad_alloc& exception)
		{
			LOG_WARN("[SHADER] Failed to load cache. Bad alloc: " << exception.what());
			EmptyHeader();
		}

		tables.resize(header->num_shader_files);
		datas.resize(header->num_shader_files);

		if (header->num_shader_files > 0)
		{
			if (async)
			{
				//TODO: This implicit "change priority after first time" is not great, maybe remove it?
				const auto priority = first_time ? Job2::Type::Medium : Job2::Type::High;

				//TODO: Why don't we kick one job per cache file, when we're already kicking jobs anyway?
				job_count++;
				Job2::System::Get().Add(priority, { Memory::Tag::ShaderCache, Job2::Profile::LoadShaders, [=]()
				{
					ReadShaderCacheFiles(dirname);
					job_count--;
				}});

				//TODO: Remove (This wait is not really needed anymore)
				if (priority == Job2::Type::High) // Wait for jobs after the first time.
				{
					while (job_count > 0)
						Job2::System::Get().RunOnce(Job2::Type::High);
				}
			}
			else
				ReadShaderCacheFiles(dirname);
		}

		first_time = false;
	}

	ShaderCache::~ShaderCache() 
	{
		while (job_count > 0) {}
	}

	void ShaderCache::ReadShaderCacheFiles(const std::wstring& dirname)
	{
		for( unsigned i = 0; i < header->num_shader_files; ++i )
		{
			const auto& filename = GetShaderFilename(dirname, i);
			ReadShaderCacheFile(filename, &tables[i], &datas[i]);
		}
	}

	namespace
	{
		unsigned char HexDigit( const wchar_t in )
		{
			return ( in < 'a' ? in - L'0' : in - L'a' + 10 );
		}
	}

	Device::ShaderData ShaderCache::GetCachedShader( const std::wstring& cache_key ) const
	{
		PROFILE;

		uint8_t lookup_cache_key[CacheKeyLength];
		for( unsigned i = 0; i < 32; ++i )
			lookup_cache_key[i] = (HexDigit(cache_key[i * 2]) << 4) | HexDigit(cache_key[i * 2 + 1]);

		const auto entry = GetCachedEntry(lookup_cache_key);
		if( !entry )
			return Device::ShaderData();

		return DecompressShader(*entry);
	}

	Device::ShaderData ShaderCache::DecompressShader(const Entry& entry) const
	{
		PROFILE;

		while (job_count > 0) // Wait for load operations
			Job2::System::Get().RunOnce(Job2::Type::High);

		const auto& table = tables[entry.file_index];
		const auto& data = datas[entry.file_index];
		if (table.empty() || data.empty())
			return Device::ShaderData();

		return UncompressPartial((OodleLZ_SeekTable*)table.data(), data, entry.offset, entry.size);
	}

	const ShaderCache::Entry* ShaderCache::GetCachedEntry(const uint8_t(&lookup_cache_key)[CacheKeyLength]) const
	{
		struct Compare
		{
			bool operator()(const uint8_t(&cache_key)[CacheKeyLength], const Entry entry) const
			{
				return memcmp(cache_key, entry.cache_key, ShaderCache::CacheKeyLength) < 0;
			}

			bool operator()(const Entry entry, const uint8_t(&cache_key)[CacheKeyLength]) const
			{
				return memcmp(entry.cache_key, cache_key, ShaderCache::CacheKeyLength) < 0;
			}
		};

		const auto entry = Utility::binary_find(entries, entries + header->num_shaders, lookup_cache_key, Compare());
		if( entry == entries + header->num_shaders )
			return nullptr;
		return entry;
	}

#if !defined( CONSOLE ) && !defined(__APPLE__) && !defined(ANDROID)
	void ShaderCache::BuildShaderCacheFromDirectory( const std::wstring& directory, const std::wstring& output_directory, const bool create_diff, ShaderCompiler::Statistics* statistics)
	{
		std::unique_ptr< ShaderCache > old_shader_cache;
		unsigned file_index = 0;
		if (create_diff)
		{
			std::wcout << L"Loading old shader cache " << output_directory << std::endl;
			old_shader_cache = std::make_unique< ShaderCache >(output_directory, false);
			file_index = old_shader_cache->header->num_shader_files;
		}

		std::wcout << L"Finding all directory cache shaders " << std::endl;
		std::vector< std::wstring > files;
		FileSystem::FindAllFiles(directory, files);

		//Ignore sentinel file:
		if (const auto sentinel = std::find(files.begin(), files.end(), directory + L"/version.txt"); sentinel != files.end())
			files.erase(sentinel);

		Memory::Vector< Entry, Memory::Tag::ShaderCache > entries;
		entries.reserve(files.size());

		Memory::FlatMap<uint64_t, std::pair<size_t, size_t>, Memory::Tag::ShaderCache> existing_offsets;
		Memory::Vector<unsigned char, Memory::Tag::ShaderCache> merged;
		merged.reserve(MaxShaderCacheSizeInBytes);

		size_t duplicates = 0;
		size_t num_reused = 0;

		const auto start_index = file_index;
		
		FileSystem::CreateDirectoryChain(output_directory);
		std::wcout << L"Packing shaders " << std::endl;

		std::atomic< unsigned > running_count = { 0 };
		const unsigned max_running = 16; // Higher job count increases the memory consumption quite a bit, so don't make this too big

		const auto WaitForJobs = [&](bool final_call)
		{
			const auto running_limit = final_call ? 0 : max_running;

			while (!Job2::System::Get().Try(Job2::Fence::All))
			{
				if (running_count.load() > running_limit)
				{
					Job2::System::Get().RunOnce(Job2::Type::High);
					continue;
				}
				else
					break;
			}
		};

		const auto FlushCacheFile = [&]()
		{
			if (merged.empty())
				return;

			WaitForJobs(false);

			running_count++;
			Job2::System::Get().Add(Job2::Type::High, { Memory::Tag::Unknown, Job2::Profile::ShaderCompilation, [merged, file_index, &output_directory, &running_count]()
			{
				const auto filename = GetShaderFilename(output_directory, file_index);
				WriteShaderCacheFile(filename + L".tmp", merged);
				running_count--;
			}});

			merged.clear();
			file_index++;
		};

		for (const auto& file : files)
		{
			const auto pos = file.find_last_of(L'/');
			if (pos == std::wstring::npos)
			{
				LOG_WARN(L"Skip shader (bad filename: " << file << L")");
				continue;
			}

			const auto shader_name = file.substr(pos + 1);
			if (shader_name.length() != 64)
			{
				LOG_WARN(L"Skip shader (bad filename: " << file << L")");
				continue;
			}

			if (!std::all_of(shader_name.begin(), shader_name.end(), [](const wchar_t c) { return (c >= L'0' && c <= '9') || (c >= L'a' && c <= 'f'); }))
			{
				LOG_WARN(L"Skip shader (bad filename: " << file << L")");
				continue;
			}

			auto& entry = entries.emplace_back();
			for (unsigned i = 0; i < 32; ++i)
				entry.cache_key[i] = (HexDigit(shader_name[i * 2]) << 4) | HexDigit(shader_name[i * 2 + 1]);

			File::InputFileStream stream(file);
			entry.size = (uint32_t)stream.GetFileSize();
			if (stream.GetFileSize() > MaxShaderUncompressedSize)
				throw std::runtime_error("Shader is too large. This may be because shaders are bigger in debug mode. Try Release");

			const auto hash = MurmurHash2_64(stream.GetFilePointer(), (int)stream.GetFileSize(), 0x5EED5EED);

			//If the shader is in the old shader cache, then we don't compress the data, it's already in the old cache, we just note the existing offset / file_index
			if (old_shader_cache)
			{
				if (const auto* existing = old_shader_cache->GetCachedEntry(entry.cache_key))
				{
					const auto shader_data = old_shader_cache->DecompressShader(*existing);
					if (!shader_data.empty())
					{
						const auto old_hash = MurmurHash2_64(shader_data.data(), (int)shader_data.size(), 0x5EED5EED);

						if (old_hash == hash)
						{
							if (!existing_offsets.insert(std::make_pair(hash, std::make_pair(existing->offset, existing->file_index))).second)
								duplicates++;

							entry = *existing;
							num_reused++;
							continue;
						}
					}
				}
			}

			// Deduplicate
			if (const auto existing = existing_offsets.find(hash); existing != existing_offsets.end())
			{
				if (statistics)
					statistics->RemoveLogger(shader_name);

				entry.offset = uint32_t(existing->second.first);
				entry.file_index = uint32_t(existing->second.second);
				duplicates++;
				continue;
			}

			if (merged.size() + entry.size > MaxShaderCacheSizeInBytes)
				FlushCacheFile();

			entry.offset = uint32_t(merged.size());
			entry.file_index = file_index;
			existing_offsets.insert(std::make_pair(hash, std::make_pair(entry.offset, entry.file_index)));

			merged.resize(merged.size() + entry.size);
			memcpy(merged.data() + entry.offset, stream.GetFilePointer(), std::min(size_t(entry.size), stream.GetFileSize()));
		}

		FlushCacheFile();
		WaitForJobs(true);

		if (statistics != nullptr)
			statistics->Print();

		std::wcout << L"Caculated shader table. " << files.size() << " total files. " << existing_offsets.size() << " unique and " << duplicates << " duplicates.";
		if (create_diff)
			std::wcout << L" " << num_reused << L" shaders were reused from the old cache.";

		std::wcout << std::endl;

		// Sort entries, as we will use binary search during runtime for finding entries
		std::sort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b)
		{
			return std::memcmp(a.cache_key, b.cache_key, ShaderCache::CacheKeyLength) < 0;
		});

		//Write out new index file last.
		{
			std::ofstream output(GetIndexFilename(output_directory) + L".tmp", std::ios::out | std::ios::binary);
			if (!output)
				throw std::runtime_error("Unable to open index file for writing");

			//Write header
			Header header;
			header.version = Version;
			header.num_shaders = (int)entries.size();
			header.num_shader_files = file_index;
			output.write((char*)&header, sizeof(Header));

			//Write table
			for (const auto& entry : entries)
				output.write((const char*)&entry, sizeof(entry));
		}

		// Clear old index/dict first.
		FileSystem::DeleteFile(GetIndexFilename(output_directory));

		//Everything is written successfully, rename to the actual filenames
		for (unsigned a = start_index; a < file_index; a++)
		{
			const auto filename = GetShaderFilename(output_directory, a);
			if (!FileSystem::RenameFile(filename + L".tmp", filename))
				throw std::runtime_error("Failed to rename cache file");
		}

		const auto index_file = GetIndexFilename(output_directory);
		if (!FileSystem::RenameFile(index_file + L".tmp", index_file))
			throw std::runtime_error("Failed to rename index file");

		//Delete any remaining tmp files
		std::vector< std::wstring > tmp_files;
		FileSystem::FindFiles(output_directory, L"*.tmp", false, false, tmp_files);
		for (const auto& tmp_file : tmp_files)
			FileSystem::DeleteFile(tmp_file);
	}

	void ShaderCache::ValidateShaderCache( const std::wstring& output_directory )
	{
		std::wcout << L"Validating " << output_directory << std::endl;

		try
		{
			ShaderCache shader_cache(output_directory, false);

			for (unsigned i = 0; i < shader_cache.header->num_shaders; ++i)
			{
				shader_cache.DecompressShader(shader_cache.entries[i]);

				if (!(i % 1000))
					std::wcout << L"Validated " << i << " shaders" << std::endl;
			}

			std::wcout << L"Validated " << shader_cache.header->num_shaders << " shaders" << std::endl;
			std::wcout << L"Success";
		}
		catch (std::exception& e)
		{
			std::wcout << L"Error: " << e.what();
		}
	}
#endif

}
