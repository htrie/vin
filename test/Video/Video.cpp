
#ifdef ENABLE_BINK_VIDEO

#include <thread>

#include "Common/File/Pack.h"
#include "Common/File/FileSystem.h"
#include "Common/File/InputFileStream.h"
#include "Common/File/Exceptions.h"
#include "Common/File/PathHelper.h"
#include "Common/File/StorageSystem.h"
#include "Common/Utility/StringManipulation.h"
#include "Common/Utility/Logger/Logger.h"
#include "Common/Utility/Profiler/ScopedProfiler.h"

#include "Visual/Device/binktextures.h"
#include "Visual/Video/BinkStreamer.h"

#include "Video.h"

#include "Bink/bink.h"


namespace Video
{
	namespace Video_NS
	{
		BINKSHADERS* video_shaders{ nullptr };

		constexpr U32 kVideoThreadMaxCount = 2;

		std::array<U32, kVideoThreadMaxCount> video_threads{};
		S32 video_thread_count{ 0 };
	}

	static File::Cache<Memory::Tag::Bink> file_cache;


	class BinkReader
	{
#if BinkMajorVersion >= 2022
		static S32 RADLINK Open(UINTa* user_data, char const* name, U64 *optional_out_size)
#else
		static S32 RADLINK Open(UINTa* user_data, char const* name )
#endif
		{
			try
			{
				// [TODO] Stream from input file source to avoid opening lag when reading from bundles.
				if (auto file = Storage::System::Get().Open(StringToWstring(name), File::Locations::All, false))
				{
					const auto size = file->GetFileSize();
					*user_data = file_cache.Add(std::move(file));
					return size > 0 ? 1 : 0; // Returns 1 on success.
				}
				else
				{
					LOG_WARN(L"[VIDEO] Failed to open file \"" << StringToWstring(name));
				}
			}
			catch (const std::exception& e)
			{
				LOG_WARN(L"[VIDEO] Failed to open file \"" << StringToWstring(name) << L"\" (" << StringToWstring(e.what()) << L")");
			}
			return 0;
		}

		static U64 RADLINK Read(UINTa* user_data, void* dest, U64 bytes)
		{
			if (auto* file = file_cache.Find(*user_data))
				return file->Read(dest, (unsigned)bytes);
			return 0;
		}

		static void RADLINK Seek(UINTa* user_data, U64 pos)
		{
			if (auto* file = file_cache.Find(*user_data))
				file->Seek((unsigned)pos);
		}

		static void RADLINK Close(UINTa* user_data)
		{
			file_cache.Remove(*user_data);
		}

		HBINK bink = nullptr;

	public:
		BinkReader(const std::wstring& filename, U32 open_flags)
		{
			BinkSetOSFileCallbacks(Open, Read, Seek, Close); // Need to specify every time since BinkStreamer may override this. // [TODO] Globally define callbacks and do switch there.
			bink = BinkOpen(WstringToString(filename).c_str(), open_flags);
		}

		~BinkReader()
		{
			BinkClose(bink);
		}

		HBINK GetHandle() const { return bink; }
	};


	struct Video
	{
		BINK* bink = nullptr;
		BINKTEXTURES* bink_textures = nullptr;
		std::unique_ptr< BinkReader > reader; // From file.
		std::unique_ptr< BinkStreamer > streamer; // From network.

		std::wstring filename;
		std::wstring url;

		float left = 0.0f;
		float top = 0.0f;
		float right = 1.0f;
		float bottom = 1.0f;

		float volume = 1.0f;

		bool loop = false;
		bool frame_ready = false;

		BinkBlendModes blendmode{ BinkBlendModes::Unspecified };

		unsigned int frame_goto = 0;
	};

	void* VideoAlloc(U64 bytes) { return Memory::Allocate(Memory::Tag::Bink, bytes); }
	void VideoFree(void* ptr) { Memory::Free(ptr); }


	void VideoSetHooks()
	{
		BinkSetMemory(VideoAlloc, VideoFree);
	}

	void VideoStartThreads()
	{
		const unsigned int cpu_count = std::min( std::thread::hardware_concurrency(), (unsigned int)Video_NS::kVideoThreadMaxCount );
		for(unsigned i = 0 ; i < cpu_count ; ++i)
		{
			const S32 res = BinkStartAsyncThread( i, 0 );
			if (res == 0)
			{
				LOG_WARN("[VIDEO] Failed to start Bink thread");
			}

			Video_NS::video_threads[i] = i;
		}
		Video_NS::video_thread_count = (S32)cpu_count;
	}

	void VideoJoinThreads()
	{
		BinkRequestStopAsyncThreadsMulti( Video_NS::video_threads.data(), Video_NS::video_thread_count );
		BinkWaitStopAsyncThreadsMulti( Video_NS::video_threads.data(), Video_NS::video_thread_count );
	}

	void VideoSetSoundSystem()
	{
		const S32 res = BinkInitSound();
		if (res == 0)
		{
			LOG_WARN("[VIDEO] Failed to set Bink sound system");
		}
	}

	void VideoInit()
	{
		VideoSetHooks();
		VideoStartThreads();
		VideoSetSoundSystem();
	}

	void VideoQuit()
	{
		VideoJoinThreads();
	}

	void VideoCreateShaders(Device::IDevice* device)
	{
		Video_NS::video_shaders = Create_Bink_shaders(device);
		if ( Video_NS::video_shaders == nullptr)
		{
			LOG_WARN( "[VIDEO] Failed to create Bink shaders" );
		}
	}

	void VideoDestroyShaders()
	{
		if ( Video_NS::video_shaders != nullptr)
		{
			Free_Bink_shaders( Video_NS::video_shaders );
			Video_NS::video_shaders = nullptr;
		}
	}


	Video* VideoCreate( const std::wstring_view filename, const std::wstring_view url )
	{
		auto video = new Video();
		video->bink = nullptr;
		video->bink_textures = nullptr;
		video->filename = filename;
		video->url = url;
		video->left = 0.0f;
		video->top = 0.0f;
		video->right = 1.0f;
		video->bottom = 1.0f;
		video->volume = 1.0f;
		video->loop = false;
		video->frame_ready = false;
		return video;
	}

	void VideoDestroy( Video* const video )
	{
		delete video;
	}

	void VideoOpen( Video* const video )
	{
		PROFILE;

		if( !video->filename.empty() )
		{
			auto reader = std::make_unique<BinkReader>(video->filename, BINKNOFRAMEBUFFERS | BINKALPHA);
			if (auto bink = reader->GetHandle())
			{
				video->bink = bink;
				video->reader = std::move(reader);
			}
			else
			{
				LOG_WARN("[VIDEO] Failed to open Bink file \"" << video->filename << "\"");
			}
		}
		else if( !video->url.empty() )
		{
			auto streamer = std::make_unique< BinkStreamer >();
			streamer->Begin();
			if( streamer->Open( video->url.c_str(), BINKNOFRAMEBUFFERS | BINKALPHA ) )
			{
				video->bink = streamer->GetHandle();
				video->streamer = std::move( streamer );
			}
			else
			{
				LOG_WARN("[VIDEO] Failed to open Bink stream \"" << video->url << "\"");
			}
		}
	}

	void VideoClose( Video* const video )
	{
		PROFILE;

		video->bink = nullptr;

		if (video->reader)
			video->reader.reset();

		if (video->streamer)
			video->streamer.reset();
	}

	void VideoStartNextFrame( Video* const video )
	{
		if (!video->bink)
			return;

		if (!video->bink_textures)
			return;

		Start_Bink_texture_update(video->bink_textures);

		BinkDoFrameAsyncMulti(video->bink, Video_NS::video_threads.data(), Video_NS::video_thread_count);
	}

	void VideoCreateTextures( Video* const video )
	{
		if (!video->bink)
			return;

		if( !Video_NS::video_shaders )
			return;

		video->bink_textures = Create_Bink_textures( Video_NS::video_shaders, video->bink, 0 );
		if (video->bink_textures == nullptr)
		{
			LOG_WARN("[VIDEO] Failed to create Bink textures");
		}
		else
		{
			Set_Bink_blend_mode( video->bink_textures, video->blendmode );
		}

		VideoStartNextFrame(video);
	}

	void VideoDestroyTextures( Video* const video )
	{
		if (!video->bink)
			return;

		if (!video->bink_textures)
			return;

		BinkDoFrameAsyncWait(video->bink, -1);

		Free_Bink_textures(video->bink_textures);
		video->bink_textures = nullptr;
	}

	void VideoBeforeReset( Video* const video )
	{
		if (!video->bink)
			return;

		if (!video->bink_textures)
			return;

		BinkDoFrameAsyncWait(video->bink, -1);

		Before_Reset_Bink_textures(video->bink_textures);
	}

	void VideoAfterReset( Video* const video )
	{
		if (!video->bink_textures)
			return;

		After_Reset_Bink_textures(video->bink_textures);
	}

	void VideoUpdate( Video* const video )
	{
		if (!video->bink)
			return;

		if (!video->bink_textures)
			return;

		if (BinkWait(video->bink))
			return;

		if (!BinkDoFrameAsyncWait(video->bink, 1000))
			return;

		if( video->frame_goto != 0 )
		{
			PROFILE_NAMED("[ BinkGoto ]");
			BinkGoto(video->bink, video->frame_goto, 0 );
			video->frame_goto = 0;
		}

		video->frame_ready = true;

		while (BinkShouldSkip(video->bink))
		{
			Finish_Bink_texture_update(video->bink_textures);
			BinkNextFrame(video->bink);
			VideoStartNextFrame(video);
			BinkDoFrameAsyncWait(video->bink, -1);
		}

		Finish_Bink_texture_update(video->bink_textures);

		if (video->bink->FrameNum == video->bink->Frames && !video->loop)
			return;

		BinkNextFrame(video->bink);

		VideoStartNextFrame(video);
	}

	void VideoRender( Video* const video )
	{
		if (!video->bink_textures)
			return;

		VideoUpdate(video);

		if( !video->frame_ready || !Video_NS::video_shaders )
			return;
		
		Draw_Bink_textures( video->bink_textures, Video_NS::video_shaders );
	}

	void VideoSetPosition( Video* const video, const float left, const float top, const float right, const float bottom)
	{
		video->left = left;
		video->top = top;
		video->right = right;
		video->bottom = bottom;

		if (!video->bink_textures)
			return;

		Set_Bink_draw_position(video->bink_textures, video->left, video->top, video->right, video->bottom);
	}

	void VideoSetLoop( Video* const video, const bool loop )
	{
		video->loop = loop;

		if (!video->bink)
			return;

		BinkSetWillLoop(video->bink, video->loop ? 1 : 0);
	}

	void VideoSetVolume( Video* const video, const float volume )
	{
		video->volume = volume;

		if (!video->bink)
			return;

		BinkSetVolume(video->bink, 0, (S32)(volume * 32768.0f));
	}

	void VideoSetAlpha( Video* const video, const float alpha)
	{
		if (video && video->bink && video->bink_textures)
			Set_Bink_alpha_settings(video->bink_textures, alpha, false);
	}

	void VideoSetColourMultiply( Video* const video, const float r, const float g, const float b )
	{
		if( video && video->bink && video->bink_textures )
			Set_Bink_colour_multiply( video->bink_textures, r, g, b );
	}

	void VideoSetAdditiveBlendMode( Video* const video )
	{
		if( video )
		{
			video->blendmode = BinkBlendModes::AdditiveBlend;

			if( video->bink && video->bink_textures )
				Set_Bink_blend_mode( video->bink_textures, video->blendmode );
		}
	}

	void VideoResetBlendMode( Video* const video )
	{
		if( video )
		{
			video->blendmode = BinkBlendModes::Unspecified;

			if( video->bink && video->bink_textures )
				Set_Bink_blend_mode( video->bink_textures, video->blendmode );
		}
	}

	float VideoGetAspectRatio( Video* const video )
	{
		if (video && video->bink)
			return video->bink->Width / static_cast< float >(video->bink->Height);
		return 0;
	}

	bool VideoIsPlaying( Video* const video )
	{
		return video && video->bink && video->bink->FrameNum < video->bink->Frames;
	}

	unsigned int VideoGetNextFrame( Video* const video )
	{
		if( video && video->bink )
			return video->bink->FrameNum;

		return 0;
	}

	void VideoSetNextFrame( Video* const video, const unsigned int frame )
	{
		if( video && video->bink )
			video->frame_goto = frame;
	}

	float VideoGetPlaceByPercent( Video* const video )
	{
		if( video && video->bink )
			return static_cast< float >(video->bink->FrameNum) / static_cast< float >(video->bink->Frames);

		return 0.0f;
	}

	void VideoSetPlaceByPercent( Video* const video, const float percent )
	{
		if( video && video->bink )
		{
			const auto frame = static_cast< U32 >( percent * static_cast< float >(video->bink->Frames) ) + 1u;
			if(video->bink->FrameNum != frame)
				video->frame_goto = frame;
		}
	}

	bool VideoIsStreaming( Video* const video )
	{
		return video->streamer != nullptr;
	}

}

#endif
