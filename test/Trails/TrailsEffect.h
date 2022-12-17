#pragma once

#include "Common/Resource/Handle.h"
#include "Common/Resource/ChildHandle.h"
#include "TrailsTemplate.h"

namespace Trails
{
	struct TrailsEffect;

	typedef Resource::Handle< TrailsEffect > TrailsEffectHandle;
	typedef Resource::ChildHandle< TrailsEffect, TrailsTemplate > TrailsTemplateHandle;

	struct TrailsEffect
	{
	private:
		class Reader;
	public:
		TrailsEffect();
		TrailsEffect( const std::wstring &filename, Resource::Allocator& );

		unsigned GetNumTemplates( ) const { return (unsigned)templates.size(); } // get number of child objects

		friend TrailsTemplateHandle GetTemplate( const TrailsEffectHandle& effect, unsigned index ) { return TrailsTemplateHandle( effect, &effect->templates[ index ] ); }

		void AddTemplate( ); // add a new empty template
		void RemoveTemplate( unsigned index ); // remove i-th template

		void Save( const std::wstring& filename ) const;

		void Reload(const std::wstring& filename); //Tools only
	private:
		std::vector< TrailsTemplate > templates;
	};
}
