#include "PassiveSkillLayout.h"

#include "Common/File/InputFileStream.h"
#include "Common/Utility/StringManipulation.h"

namespace Skills
{

	PassiveSkillLayout::PassiveSkillLayout( const std::wstring& filename, const Resource::Handle< PassiveSkillGraph >& passive_skill_graph )
	{
		File::InputFileStream stream( filename, ".psg" );

		std::wstring token;
		while( stream >> token )
		{
			if( token != L"orbit" )
				throw Resource::Exception( filename ) << L"Expected orbit or end of file";

			int orbit_x, orbit_y;
			if( !StreamData( stream, orbit_x ) || !StreamData( stream, orbit_y ) )
				throw Resource::Exception( filename ) << L"Expected integer co-ordinate.";

			orbits.push_back( Orbit( simd::vector2( float( orbit_x ), float( orbit_y ) ) ) );

			if( !ReadExpected( stream, L"{" ) )
				throw Resource::Exception( filename ) << L"Expected {";

			while( stream >> token )
			{
				if( token == L"}" )
					break;

				if( token != L"orbital" )
					throw Resource::Exception( filename ) << L"Expected } or orbital";

				int offset;
				if( !StreamData(stream, offset) )
					throw Resource::Exception( filename ) << L"Expected integer offset in degrees";

				if( !ReadExpected(stream, L"{" ))
					throw Resource::Exception( filename ) << L"Expected {";

				std::vector< PassiveSkillHandle > passive_handles;

				while( stream >> token )
				{
					if( token == L"}" )
						break;

					if( token == L"EMPTY" )
					{
						passive_handles.push_back( PassiveSkillHandle() );
					}
					else
					{
						const PassiveSkillHandle passive;// = GetSkill( passive_skill_graph->GetSkillList(), token );
						if( passive.IsNull() )
							throw Resource::Exception( filename ) << L"Skill " << token << L" does not exist.";
						passive_handles.push_back( passive );
					}

				}

				const float offset_radians = ( offset / 360.0f ) * PI2;
				orbitals.push_back( Orbital( orbits.back(), static_cast< unsigned >( passives.size() ), offset_radians ) );

				for( size_t i = 0; i < passives.size(); ++i )
				{
					if( !passive_handles[ i ].IsNull() )
						passives.insert( std::make_pair( passive_handles[ i ], Passive( passive_handles[ i ], orbitals.back(), static_cast< unsigned >( i ) ) ) );
				}
			}

		}
	}

	void PassiveSkillLayout::PassiveLocationIterator::operator++()
	{
		++iter;
		CalculatePassiveLocation();
	}

	PassiveSkillLayout::PassiveLocationIterator::PassiveLocationIterator( const Passives_t::const_iterator wrapped_iter, const Passives_t::const_iterator end  )
		: iter( wrapped_iter ), end( end )
	{
		CalculatePassiveLocation();
	}


	namespace
	{
		const float NumSkillsToRadii[ ] = 
		{
			0.0f, //Not supported
			0.0f, //1 skill sits in the center
			0.0f, //Not supported
			20.0f,
			40.0f
		};
	}

	void PassiveSkillLayout::PassiveLocationIterator::CalculatePassiveLocation()
	{
		if( iter == end )
		{
			passive_location.passive.Release();
		}
		else
		{
			const Passive& passive = iter->second;

			const float radius = NumSkillsToRadii[ passive.orbital.size ];
			const float angle = passive.index * ( PI2 / passive.orbital.size ) + passive.orbital.offset;

			passive_location.passive = passive.handle;
			passive_location.center_position = passive.orbital.orbit.location + simd::vector2( radius * sinf( angle ), radius * cosf( angle ) );
		}
	}
}

