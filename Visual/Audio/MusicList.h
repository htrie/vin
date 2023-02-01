#pragma once
#include <vector>
#include <string>
#include "Visual/Audio/SoundSystem.h"
#include "Common/Loaders/CEMusic.h"

namespace Audio
{
	class MusicList 
	{
	public:
		static MusicList* GetInstance();

		Loaders::Music_Row_Handle GetCurrentTrack() const { return current_track < playlist.size() ? playlist[ current_track ] : Loaders::Music_Row_Handle(); }

		void PlayRandomHideoutMusic();
		void PlayCreditsMusic();

		//fade out the music and play this track, then resume the music afterwards
		void PlayInterlude( const std::wstring& track_file_name, const float fade_time = 2.5f );

		void ChangeTrack();

		///Only call this during the close of the client
		void StopAll();
		void FadeOutToSilence();

		using Playlist = Memory::Vector< Loaders::Music_Row_Handle, Memory::Tag::Sound >;
		const Playlist& GetCurrentPlaylist() const { return playlist; }
		void SetTrack( const Loaders::Music_Row_Handle& music, const bool let_interlude_complete = false );
		void SetPlaylist( Playlist playlist );

		// Note: MusicList needs to be updated every frame now as there's no more SoundStreamEventListener for checking if music is finished
		void FrameMove( const float elapsed_time );

	private:
		void CacheLastStreamPosition();
		
		Playlist playlist;
		unsigned current_track = -1;

		bool playing_interlude = false;
		bool fade_in_next_track = false;
		bool cache_music_times = true;

		uint64_t current_stream = 0;
		uint64_t last_stream = 0;
		std::wstring current_path, last_path;
		Memory::Vector< std::pair< std::wstring, int >, Memory::Tag::Sound > cached_music_times;
	};
}
