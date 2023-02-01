#include "Common/Utility/Logger/Logger.h"

#include "Video.h"
#include "VideoSystem.h"


namespace Video
{
	// duplicate forward declaration from Video.h in case ENABLE_BINK_VIDEO not defined
	struct Video;

	class Impl
	{
		static constexpr uint64_t null_video_id = std::numeric_limits<uint64_t>::max();

#ifdef ENABLE_BINK_VIDEO
		std::map< uint64_t, Video* > videos;
		Memory::Mutex videos_mutex;

		std::atomic< uint64_t > next_id = { 0u };
		bool enable_video = false;
#endif // #ifdef ENABLE_BINK_VIDEO

		uint64_t FindStreamingVideoId()
		{
#ifdef ENABLE_BINK_VIDEO
			Memory::Lock lock( videos_mutex );

			for (const auto& [id, video] : videos)
			{
				if (VideoIsStreaming(video))
				{
					return id;
				}
			}
#endif // #ifdef ENABLE_BINK_VIDEO
			return null_video_id;
		}

		Video* Find(const uint64_t id)
		{
#ifdef ENABLE_BINK_VIDEO
			const auto it = videos.find(id);
			return (it != std::end(videos)) ? it->second : nullptr;
#else
			return nullptr;
#endif // #ifdef ENABLE_BINK_VIDEO
		}

		typedef void (*ProcessCallback)(Video*);
		void Process(const ProcessCallback callback)
		{
#ifdef ENABLE_BINK_VIDEO
			for (auto iter = videos.begin(); iter != videos.end(); ++iter)
			{
				callback(iter->second);
			}
#endif // #ifdef ENABLE_BINK_VIDEO
		}

	public:
		Impl()
		{
		}

		void Init(bool enable_video)
		{
#ifdef ENABLE_BINK_VIDEO
			LOG_INFO(L"[VIDEO] Enable: " << (enable_video ? L"ON" : L"OFF"));
			this->enable_video = enable_video;

			if (enable_video)
				VideoInit();
#endif // #ifdef ENABLE_BINK_VIDEO
		}

		void Swap()
		{
		}

		void GarbageCollect()
		{
		}

		void Clear()
		{
#ifdef ENABLE_BINK_VIDEO
			Process(VideoClose);
			Process(VideoDestroy);

			if (enable_video)
				VideoQuit();
#endif // #ifdef ENABLE_BINK_VIDEO
		}

		void OnCreateDevice(Device::IDevice* device)
		{
#ifdef ENABLE_BINK_VIDEO
			if (enable_video)
				VideoCreateShaders(device);

			Process(VideoCreateTextures);
#endif // #ifdef ENABLE_BINK_VIDEO
		}

		void OnResetDevice(Device::IDevice* device)
		{
#ifdef ENABLE_BINK_VIDEO
			Process(VideoAfterReset);
#endif // #ifdef ENABLE_BINK_VIDEO
		}

		void OnLostDevice()
		{
#ifdef ENABLE_BINK_VIDEO
			Process(VideoBeforeReset);
#endif // #ifdef ENABLE_BINK_VIDEO
		}

		void OnDestroyDevice()
		{
#ifdef ENABLE_BINK_VIDEO
			Process(VideoDestroyTextures);

			if (enable_video)
				VideoDestroyShaders();
#endif // #ifdef ENABLE_BINK_VIDEO
		}

		uint64_t Add(const std::wstring_view filename, const std::wstring_view url)
		{
#ifdef ENABLE_BINK_VIDEO
			if (!enable_video)
				return 0;

			// NOTE: We must stop the currently playing streaming video before playing any other video because of the fact that a streaming video hijacks the IO functions (see BinkStreamer implementation.)
			// TODO: This system should not be responsible for removing streaming videos. To refactor out at somepoint
			if( const auto streaming_video_id = FindStreamingVideoId(); streaming_video_id != null_video_id )
			{
				Remove( streaming_video_id );
			}

			Memory::Lock lock(videos_mutex);

			Video* video = VideoCreate(filename, url);
			VideoOpen(video);
			VideoCreateTextures(video);

			const auto id = ++next_id;
			videos.insert(std::pair< uint64_t, Video* >(id, video));

			return id;
#else
			return 0;
#endif // #ifdef ENABLE_BINK_VIDEO
		}

		void Remove(const uint64_t id)
		{
#ifdef ENABLE_BINK_VIDEO
			Memory::Lock lock(videos_mutex);

			const auto iter = videos.find(id);
			if (iter == videos.end())
				return;

			Video* video = iter->second;
			videos.erase(iter);

			VideoDestroyTextures(video);
			VideoClose(video);
			VideoDestroy(video);
#endif // #ifdef ENABLE_BINK_VIDEO
		}

		void Render(const uint64_t id)
		{
#ifdef ENABLE_BINK_VIDEO
			Memory::Lock lock(videos_mutex);
			if (auto* const video = Find(id))
				VideoRender(video);
#endif // #ifdef ENABLE_BINK_VIDEO
		}

		void SetPosition(const uint64_t id, const float left, const float top, const float right, const float bottom)
		{
#ifdef ENABLE_BINK_VIDEO
			Memory::Lock lock(videos_mutex);
			if (auto* const video = Find(id))
				VideoSetPosition(video, left, top, right, bottom);
#endif // #ifdef ENABLE_BINK_VIDEO
		}

		void SetLoop(const uint64_t id, const bool loop)
		{
#ifdef ENABLE_BINK_VIDEO
			Memory::Lock lock(videos_mutex);
			if (auto* const video = Find(id))
				VideoSetLoop(video, loop);
#endif // #ifdef ENABLE_BINK_VIDEO
		}

		void SetVolume(const uint64_t id, const float volume)
		{
#ifdef ENABLE_BINK_VIDEO
			Memory::Lock lock(videos_mutex);
			if (auto* const video = Find(id))
				VideoSetVolume(video, volume);
#endif // #ifdef ENABLE_BINK_VIDEO
		}

		void SetAlpha(const uint64_t id, const float alpha)
		{
#ifdef ENABLE_BINK_VIDEO
			Memory::Lock lock(videos_mutex);
			if (auto* const video = Find(id))
				VideoSetAlpha(video, alpha);
#endif // #ifdef ENABLE_BINK_VIDEO
		}

		void SetColourMultiply( const uint64_t id, const float r, const float g, const float b )
		{
#ifdef ENABLE_BINK_VIDEO
			Memory::Lock lock( videos_mutex );
			if( auto* const video = Find( id ) )
				VideoSetColourMultiply( video, r, g, b );
#endif // #ifdef ENABLE_BINK_VIDEO
		}

		void SetAdditiveBlendMode(const uint64_t id)
		{
#ifdef ENABLE_BINK_VIDEO
			Memory::Lock lock(videos_mutex);
			if (auto* const video = Find(id))
				VideoSetAdditiveBlendMode(video);
#endif // #ifdef ENABLE_BINK_VIDEO
		}

		void ResetBlendMode(const uint64_t id)
		{
#ifdef ENABLE_BINK_VIDEO
			Memory::Lock lock(videos_mutex);
			if (auto* const video = Find(id))
				VideoResetBlendMode(video);
#endif // #ifdef ENABLE_BINK_VIDEO
		}

		float GetAspectRatio(const uint64_t id)
		{
#ifdef ENABLE_BINK_VIDEO
			Memory::Lock lock(videos_mutex);
			auto* const video = Find(id);
			return video ? VideoGetAspectRatio(video) : 0.f;
#else
			return 0.0f;
#endif // #ifdef ENABLE_BINK_VIDEO
		}

		bool IsPlaying(const uint64_t id)
		{
#ifdef ENABLE_BINK_VIDEO
			Memory::Lock lock(videos_mutex);
			auto* const video = Find(id);
			return video && VideoIsPlaying(video);
#else
			return false;
#endif // #ifdef ENABLE_BINK_VIDEO
		}

		unsigned int GetNextFrame(const uint64_t id)
		{
#ifdef ENABLE_BINK_VIDEO
			Memory::Lock lock(videos_mutex);
			auto* const video = Find(id);
			return video ? VideoGetNextFrame(video) : 0;
#else
			return 0;
#endif // #ifdef ENABLE_BINK_VIDEO
		}

		void SetNextFrame(const uint64_t id, const unsigned int frame)
		{
#ifdef ENABLE_BINK_VIDEO
			Memory::Lock lock(videos_mutex);
			if (auto* const video = Find(id))
				VideoSetNextFrame(video, frame);
#endif // #ifdef ENABLE_BINK_VIDEO
		}

		float GetPlaceByPercent(const uint64_t id)
		{
#ifdef ENABLE_BINK_VIDEO
			Memory::Lock lock(videos_mutex);
			auto* const video = Find(id);
			return video ? VideoGetPlaceByPercent(video) : 0.f;
#else
			return 0.0f;
#endif // #ifdef ENABLE_BINK_VIDEO
		}

		void SetPlaceByPercent(const uint64_t id, const float percent)
		{
#ifdef ENABLE_BINK_VIDEO
			Memory::Lock lock(videos_mutex);
			if (auto* const video = Find(id))
				VideoSetPlaceByPercent(video, percent);
#endif // #ifdef ENABLE_BINK_VIDEO
		}
	};


	System::System() : ImplSystem()
	{
	}

	System& System::Get()
	{
		static System instance;
		return instance;
	}

	void System::Init(bool enable_video)
	{
		impl->Init(enable_video);
	}

	void System::Swap() { impl->Swap(); }
	void System::GarbageCollect() { impl->GarbageCollect(); }
	void System::Clear() { impl->Clear(); }

	void System::OnCreateDevice(Device::IDevice* device) { impl->OnCreateDevice(device); }
	void System::OnResetDevice(Device::IDevice* device) { impl->OnResetDevice(device); }
	void System::OnLostDevice() { impl->OnLostDevice(); }
	void System::OnDestroyDevice() { impl->OnDestroyDevice(); }

	uint64_t System::Add(std::wstring_view filename, std::wstring_view url) { return impl->Add(filename, url); }
	void System::Remove(uint64_t id) { impl->Remove(id); }

	void System::Render(const uint64_t id) { impl->Render(id); }

	void System::SetPosition(uint64_t id, float left, float top, float right, float bottom) { impl->SetPosition(id, left, top, right, bottom); }
	void System::SetLoop(uint64_t id, bool loop) { impl->SetLoop(id, loop); }
	void System::SetVolume(uint64_t id, float volume) { impl->SetVolume(id, volume); }
	void System::SetAlpha(uint64_t id, float alpha) { impl->SetAlpha(id, alpha); }
	void System::SetColourMultiply(uint64_t id, float r, float g, float b) { impl->SetColourMultiply(id, r, g, b); }

	void System::SetAdditiveBlendMode(uint64_t id) { impl->SetAdditiveBlendMode(id); }
	void System::ResetBlendMode(uint64_t id) { impl->ResetBlendMode(id); }

	float System::GetAspectRatio(uint64_t id) { return impl->GetAspectRatio(id); }
	bool System::IsPlaying(uint64_t id) { return impl->IsPlaying(id); }

	unsigned int System::GetNextFrame(uint64_t id) { return impl->GetNextFrame(id); }
	void System::SetNextFrame(uint64_t id, unsigned int frame) { impl->SetNextFrame(id, frame); }

	float System::GetPlaceByPercent(uint64_t id) { return impl->GetPlaceByPercent(id); }
	void System::SetPlaceByPercent(uint64_t id, float percent) { impl->SetPlaceByPercent(id, percent); }

}
