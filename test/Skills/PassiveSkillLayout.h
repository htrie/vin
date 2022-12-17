#pragma once

#include "ClientInstanceCommon/Skills/PassiveSkillGraph.h"

namespace Skills
{

	///Class for loading the layout of passive skills for the passive skill screen
	class PassiveSkillLayout
	{
		///A group of skills that circle a center location
		struct Orbit
		{
			Orbit( const simd::vector2 &location ) : location( location ) { }
			const simd::vector2 location; ///< Location of the center of an orbital
		};

		///A single ring around an orbit that contains a fixed number of passives
		struct Orbital
		{
			Orbital( const Orbit& orbit, const unsigned size, const float offset ) : orbit( orbit ), size( size ), offset( offset ) { }
			const Orbit& orbit;
			const unsigned size;
			const float offset;
		};

		///A single passive in the graph
		struct Passive
		{
			Passive( const PassiveSkillHandle& handle, const Orbital& orbital, const unsigned index ) : handle( handle ), orbital( orbital ), index( index ) { }
			const PassiveSkillHandle handle;
			const Orbital& orbital;
			const unsigned index;
		};

		typedef std::list< Orbit > Orbits_t;
		typedef std::list< Orbital > Orbitals_t;
		typedef std::unordered_map< PassiveSkillHandle, Passive > Passives_t;

	public:
		///A curved edge connecting two passives
		struct CurvedEdgeLocation
		{
			PassiveSkillHandle first, second;
			simd::vector2 center_position; ///< The center of the circle of the arc
			float start, end;
		};

		///A straight edge connecting two passives
		struct StraightEdgeLocation
		{
			PassiveSkillHandle first, second;
			simd::vector2 start, end;
		};

		///A passive plus a location
		struct PassiveLocation
		{
			PassiveSkillHandle passive;
			simd::vector2 center_position;
		};

		class PassiveLocationIterator : public std::iterator< std::forward_iterator_tag, const PassiveLocation >
		{
		public:
			PassiveLocationIterator( const Passives_t::const_iterator wrapped_iter, const Passives_t::const_iterator end );

			///Deref
			const PassiveLocation& operator*() const { return passive_location; }
			///Forward
			void operator++();

			///Comparison
			bool operator==( const PassiveLocationIterator& other ) { return other.iter == iter; }
			bool operator!=( const PassiveLocationIterator& other ) { return other.iter != iter; }
		private:
			void CalculatePassiveLocation( );
			PassiveLocation passive_location;
			Passives_t::const_iterator iter;
			Passives_t::const_iterator end;
		};

		class PassiveLocationRange
		{
		public:
			PassiveLocationRange( const Passives_t& passives ) : passives( passives ) { }
			PassiveLocationIterator begin() const { return PassiveLocationIterator( passives.begin(), passives.end() ); }
			PassiveLocationIterator end() const { return PassiveLocationIterator( passives.end(), passives.end() ); }
			typedef PassiveLocationIterator const_iterator;
		private:
			const Passives_t& passives;
		};

	public:
		PassiveSkillLayout( const std::wstring& filename, const Resource::Handle< PassiveSkillGraph >& passive_skill_graph );

		const std::vector< CurvedEdgeLocation >& CurvedEdgeLocations( ) const;
		const std::vector< StraightEdgeLocation >& StraightEdgeLocations( ) const;
		PassiveLocationRange PassiveLocations( ) const { return PassiveLocationRange( passives ); }

	private:
		Orbits_t orbits;
		Orbitals_t orbitals;
		Passives_t passives;
	};

}
