
#ifdef ENABLE_BINK_VIDEO

#include <algorithm>
#include <mutex>
#include <vector>
#include <thread>

#include "Common/Utility/StringManipulation.h"
#include "Common/Utility/Http/HttpStream.h"
#include "Common/Utility/Logger/Logger.h"
#include "Common/Utility/Profiler/ScopedProfiler.h"

#include "BinkStreamer.h"

class BinkStreamer::Impl
{
public:
	static BinkStreamer::Impl *GetInstance()
	{
		static BinkStreamer::Impl instance;
		return &instance;
	}

	HBINK GetHandle() { return bink; }

	bool Begin()
	{
		PROFILE;

		if( http_stream )
			return false;

		http_stream = std::make_unique< HttpStream >();
		BinkSetOSFileCallbacks( FileOpenCallback, FileReadCallback, FileSeekCallback, FileCloseCallback );

		return true;
	}

	void End()
	{
		PROFILE;

		if( http_stream )
		{
			Close();
			http_stream.reset();
			BinkSetOSFileCallbacks( nullptr, nullptr, nullptr, nullptr );
		}
	}

	bool Open( char const *url, U32 open_flags )
	{
		PROFILE;

		if( bink != nullptr )
			return false;

		streaming_start_time = std::chrono::steady_clock::now();
		streaming_speed = 0;
		streaming_cancelled = false;

		video_length_millisec = 0;
		video_size_bytes = 0;

		bink = BinkOpen( url, open_flags );

		if( bink == nullptr )
		{
			LOG_WARN( "BinkOpen: " << BinkGetError() );
			Close();
			return false;
		}

		video_length_millisec = 1000 / ( bink->FrameRate / bink->FrameRateDiv ) * bink->Frames;
		video_size_bytes = (unsigned)bink->Size;

		return true;
	}

	void Close()
	{
		PROFILE;

		if( http_stream )
		{
			http_stream->Close();

			buffers.clear();
			read_pos = 0;
			received_size = 0;

			if( bink != nullptr )
			{
				BinkClose( bink );
				bink = nullptr;
			}
		}
	}

private:
	void Seek(size_t pos)
	{
		std::unique_lock<std::mutex> lock(buffer_mutex);
		read_pos = std::min(pos, received_size);
	}

	void Append(const char* data, size_t size)
	{
		PROFILE;

		std::vector<char> tmp(size);
		memcpy(tmp.data(), data, size);

		std::unique_lock<std::mutex> lock(buffer_mutex);
		buffers.emplace_back(std::move(tmp));
		received_size += size;
	}

	bool Wait(size_t bytes)
	{
		PROFILE;

		while (received_size - read_pos < bytes)
		{
			if (http_stream->GetState() != HttpStream::State::Streaming)
				return false;

			std::this_thread::sleep_for(std::chrono::milliseconds(HttpStream::wait_interval));
		}
		return true;
	}

	void ReadPartial(const std::vector<char>& buffer, size_t& pos, size_t& remaining, char*& dest)
	{
		const auto offset = std::max(pos, read_pos) - pos;
		const auto size = std::min(buffer.size() - offset, remaining);
		memcpy(dest, &buffer.data()[offset], size);
		pos += buffer.size();
		remaining -= size;
		dest += size;
	}

	size_t Read(char* dest, size_t bytes)
	{
		PROFILE;

		std::unique_lock<std::mutex> lock(buffer_mutex);

		size_t pos = 0;
		size_t remaining = bytes;

		for (auto& buffer : buffers)
		{
			if (pos + buffer.size() < read_pos) // TODO: Find first buffer faster.
			{
				pos += buffer.size();
				continue;
			}

			ReadPartial(buffer, pos, remaining, dest);

			if (remaining == 0)
				break;
		}

		read_pos += bytes;
		return bytes;
	}

	/*
	 * Description
	 * overrides the low - level IO functions in the default IO system.
	 *
	 * Parameters
	 * user_data[out] a two element array you can save information into( usually the file handle ).
	 *
	 * filename[in] filename to open.
	 *
	 * Return value
	 * return[out] Returns 1 if successful, 0 if error.
	 *
	 * Discussion
	 * The default file IO system will call this function when opening a file.You can write whatever data you need at user_data[0] and user_data[1].
	 */
#if BinkMajorVersion >= 2022
	static S32 RADLINK FileOpenCallback( UINTa *user_data, char const *url, U64 *optional_out_size)
#else
	static S32 RADLINK FileOpenCallback(UINTa *user_data, char const *url )
#endif
	{
		auto read = [&]( const char *data, size_t size )
		{
			GetInstance()->Append(data, size);
		};

		auto done = [&]( long status_code )
		{
			LOG_INFO( "BinkStreamer::done(): HTTP status is " << status_code );
		};

		return GetInstance()->http_stream->Open( url, std::move( read ), std::move( done ) ) ? 1 : 0;
	}

	/*
	 * Description
	 * overrides the low-level IO functions in the default IO system.
	 *
	 * Parameters
	 * user_data [out] the same two element array passed to BINKFILEOPEN.
	 *
	 * dest [out] the memory address to write the data too.
	 *
	 * bytes [in] the number of bytes to read.
	 *
	 * Return value
	 * return [out] Returns the number of bytes actually written.
	 *
	 * Discussion
	 * The default file IO system will call this function when reading data. You can read or write whatever data you need at user_data[0] and user_data[1]
	 * (it is the same buffer passed to open, so you will usually reference the file handle at user_data[0]).
	 */
	static U64 RADLINK FileReadCallback( UINTa *user_data, void *dest, U64 bytes )
	{
		if (!GetInstance()->Wait(bytes))
			return 0;

		return GetInstance()->Read((char*)dest, bytes);
	}

	/*
	 * Description
	 * overrides the low-level IO functions in the default IO system.
	 *
	 * Parameters
	 * user_data [out] specifies what information to return.
	 *
	 * pos [in] filename to open.
	 *
	 * Return value
	 * return [out]
	 *
	 * Discussion
	 * The default file IO system will call this function when seeking within the file. You can read or write whatever data you need at user_data[0] and user_data[1]
	 * (it is the same buffer passed to open, so you will usually reference the file handle at user_data[0]).
	 */
	static void RADLINK FileSeekCallback( UINTa *user_data, U64 pos )
	{
		GetInstance()->Seek((size_t)pos);
	}

	/*
	 * Description
	 * overrides the low-level IO functions in the default IO system.
	 *
	 * Parameters
	 * user_data [out] specifies what information to return.
	 *
	 * Return value
	 * return [out]
	 *
	 * Discussion
	 * The default file IO system will call this function when close the file. You can read or write whatever data you need at user_data[0] and user_data[1]
	 * (it is the same buffer passed to open, so you will usually reference the file handle at user_data[0]).
	 */
	static void RADLINK FileCloseCallback( UINTa *user_data )
	{
		// do nothing, Close() and End() will take care about cleanup
	}

	HBINK bink = nullptr;

	std::unique_ptr< HttpStream > http_stream;

	std::chrono::steady_clock::time_point streaming_start_time;
	std::atomic< double > streaming_speed;
	std::atomic< bool > streaming_cancelled;

	std::atomic< unsigned > video_length_millisec;
	std::atomic< unsigned > video_size_bytes;

	std::vector<std::vector< char >> buffers;
	std::mutex buffer_mutex;
	size_t received_size = 0;
	size_t read_pos = 0;
};

BinkStreamer::BinkStreamer()
{
}

BinkStreamer::~BinkStreamer()
{
	End();
}

bool BinkStreamer::Begin()
{
	return Impl::GetInstance()->Begin();
}

void BinkStreamer::End()
{
	return Impl::GetInstance()->End();
}

HBINK BinkStreamer::BinkStreamer::GetHandle()
{
	return Impl::GetInstance()->GetHandle();
}

bool BinkStreamer::Open( const char *url, U32 open_flags )
{
	return Impl::GetInstance()->Open( url, open_flags );
}

bool BinkStreamer::Open( const wchar_t *url, U32 open_flags )
{
	return Open( WstringToString( url ).c_str(), open_flags );
}

void BinkStreamer::Close()
{
	return Impl::GetInstance()->Close();
}

#endif
