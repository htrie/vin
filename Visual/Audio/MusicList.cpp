#include "MusicList.h"

#include "Visual/Audio/SoundSystem.h"
#include "Common/Utility/PairFind.h"
#include "Common/Utility/ContainerOperations.h"
#include "Common/Utility/StaticData.h"

namespace Audio
{
	constexpr size_t cache_size = 4;
	constexpr auto random_music_hash = CompileTime::ConfigEditorMurmurHash2Short( L"Random" );
	int prev_time = 0;

	uint64_t Play(const Audio::Desc& desc)
	{
		const auto id = Audio::System::Get().Play(desc);
		prev_time = 0;
		return id;
	}

	static bool IsSameTrack( const Loaders::Music_Row_Handle& a, const Loaders::Music_Row_Handle& b )
	{
		//OK to compare pointers, because identical strings within the same table are deduplicated
		return a->Getfile() == b->Getfile();
	}

	MusicList* MusicList::GetInstance()
	{
		static Utility::StaticDataPtr< MusicList > instance;
		return instance.get();
	}

	void MusicList::SetPlaylist( Playlist playlist )
	{
		//null rows not allowed
		RemoveAll( playlist, Loaders::Music_Row_Handle() );

		//adjacent duplicate tracks not allowed
		{
			auto dupe = std::adjacent_find( playlist.begin(), playlist.end(), IsSameTrack );
			while( dupe != playlist.end() )
			{
				dupe = playlist.erase( dupe );
				dupe = std::adjacent_find( dupe, playlist.end(), IsSameTrack );
			}
		}

		current_track = -1;
		cache_music_times = playlist.size() <= 1;
		this->playlist = std::move( playlist );
		ChangeTrack();
	}

	void MusicList::PlayCreditsMusic()
	{
		const Loaders::MusicTable table;
		Playlist music;
		for( const auto& row : table )
		{
			if( row->GetCreditsOrder() )
				music.push_back( row );
		}
		Sort( music, []( const Loaders::Music_Row_Handle& a, const Loaders::Music_Row_Handle& b )
		{
			return a->GetCreditsOrder() < b->GetCreditsOrder();
		} );
		SetPlaylist( music );
	}

	void MusicList::PlayRandomHideoutMusic()
	{
		const Loaders::MusicTable table;
		Playlist random_tracks;
		random_tracks.reserve( table.GetNumRows() / 2 );

		for( const auto& row : table )
		{
			if( row->GetAllowRandom() && row->GetAllowInHideout() )
				random_tracks.push_back( row );
		}

		//remove duplicate tracks
		Sort( random_tracks, []( const Loaders::Music_Row_Handle& a, const Loaders::Music_Row_Handle& b )
		{
			return a->Getfile() < b->Getfile();
		} );
		random_tracks.erase( std::unique( random_tracks.begin(), random_tracks.end(), IsSameTrack ), random_tracks.end() );

		//randomise
		JumbleVector( random_tracks );
		SetPlaylist( std::move( random_tracks ) );
	}

	void MusicList::SetTrack( const Loaders::Music_Row_Handle& music, const bool let_interlude_complete )
	{
		if( music.IsValid() && music->GetHash() == random_music_hash )
		{
			PlayRandomHideoutMusic();
			return;
		}

		cache_music_times = true;
		playlist.clear();
		current_track = -1;

		if( music.IsValid() )
			playlist.push_back( music );

		if( !playing_interlude || !let_interlude_complete )
			ChangeTrack();
	}

	void MusicList::PlayInterlude( const std::wstring& track_file_name, const float fade_time )
	{
		//if last_stream is in use, it will be cut off here
		if( current_stream )
		{
			last_stream = current_stream;
			last_path = current_path;

			CacheLastStreamPosition();

			Audio::System::Get().Stop( current_stream );
			current_stream = 0;
		}

		if( !track_file_name.empty() )
		{
			current_path = track_file_name;
			Audio::Desc desc = Audio::Desc( current_path ).SetType( Audio::SoundType::Music );
			if( last_stream )
				desc.SetDelay( fade_time );
			current_stream = Play(desc);
			playing_interlude = true;
		}

		fade_in_next_track = true;
	}

	void MusicList::ChangeTrack()
	{
		if( playlist.empty() )
			current_track = -1;
		else
			current_track = ( current_track + 1 ) % (unsigned)playlist.size();

		if( current_stream )
		{
			//Don't replay track if it's just the same as the current track
			if( current_track != -1 && current_path == playlist[ current_track ]->Getfile() )
				return;

			last_stream = current_stream;
			last_path = current_path;

			CacheLastStreamPosition();

			Audio::System::Get().Stop( current_stream );
			current_stream = 0;
		}

		if( playlist.empty() )
			return;

		//Resume from where we left off if we remember this track
		int start_pos = 0;
		if( cache_music_times )
		{
			const auto cached_time = Utility::find_pair_first( cached_music_times.begin(), cached_music_times.end(), playlist[ current_track ]->Getfile() );
			if( cached_time != cached_music_times.end() )
			{
				start_pos = cached_time->second;
				cached_music_times.erase( cached_time );
			}
		}

		current_path = playlist[ current_track ]->Getfile();
		if (!current_path.empty())
			current_stream = Play(Audio::Desc(current_path).SetType(Audio::SoundType::Music).SetStartTime(start_pos));
		playing_interlude = false;
	}

	void MusicList::FrameMove( const float elapsed_time )
	{
		if( current_stream )
		{
			const auto info = Audio::System::Get().GetInfo(current_stream);
			const auto restarted = prev_time > info.GetTime();
			prev_time = info.GetTime();
			if (info.IsFinished() || restarted)
				ChangeTrack();
		}
	}

	void MusicList::StopAll()
	{
		if( current_stream )
			Audio::System::Get().Stop( current_stream );
		if( last_stream )
			Audio::System::Get().Stop( last_stream );
		current_stream = 0;
		last_stream = 0;
		playing_interlude = false;
	}

	void MusicList::FadeOutToSilence()
	{
		if( current_stream )
			Audio::System::Get().Stop( current_stream );
		playlist.clear();
		current_track = -1;
		playing_interlude = false;
	}

	void MusicList::CacheLastStreamPosition()
	{
		if( !last_stream || !cache_music_times )
			return;

		const auto existing = Utility::find_pair_first( cached_music_times.begin(), cached_music_times.end(), last_path );
		if( existing != cached_music_times.end() )
			cached_music_times.erase( existing );

		const auto last_time = Audio::System::Get().GetInfo( last_stream ).GetTime();
		cached_music_times.push_back( std::make_pair( last_path, last_time ) );
		if( cached_music_times.size() > cache_size )
			cached_music_times.erase( cached_music_times.begin() );
	}

}  //End Namespace Audio