#include "magic_enum/magic_enum.hpp"

#include "Common/File/InputFileStream.h"
#include "Common/File/PathHelper.h"
#include "Common/Utility/StringManipulation.h"
#include "Common/Utility/Counters.h"
#include "Common/Loaders/CEMiscEffectPacks.h"
#include "Common/FileReader/EPKReader.h"
#include "Common/Objects/Type.h"
#include "Common/Utility/Logger/Logger.h"

#include "Visual/Renderer/EffectGraphSystem.h"
#include "Visual/Renderer/ShaderSystem.h"
#include "Visual/Renderer/RendererSubsystem.h"
#include "Visual/Animation/Components/AnimatedRenderComponent.h"
#include "Visual/Renderer/DrawCalls/EffectGraphInstance.h"

#include "EffectPack.h"

#include <rapidjson/prettywriter.h>

namespace Visual
{
	void ReadRenderPasses(const std::wstring& buffer, MaterialPasses& out_passes, Objects::Origin origin /*= Objects::Origin::Client*/)
	{
		rapidjson::GenericDocument<rapidjson::UTF16LE<>> doc;
		doc.Parse<rapidjson::kParseTrailingCommasFlag>(buffer.c_str());
		if (doc.HasParseError() && doc.GetParseError() != rapidjson::kParseErrorDocumentEmpty)
			throw Resource::Exception() << StringToWstring(std::string(rapidjson::GetParseError_En(doc.GetParseError())));

		unsigned second_pass_id = 1;
		for (auto& pass_obj : doc[L"passes"].GetArray())
		{
			Visual::MaterialPass new_pass;

			if (!pass_obj[L"is_main"].IsNull())
			{
				if (!pass_obj[L"is_main"].GetBool())
					new_pass.index = second_pass_id++;
			}
			if (!pass_obj[L"type"].IsNull())
			{
				std::wstring apply_mode_str = pass_obj[L"type"].GetString();
				auto value = magic_enum::enum_cast<Visual::ApplyMode>(WstringToString(apply_mode_str));
				if (value.has_value())
					new_pass.apply_mode = value.value();
			}
			if (!pass_obj[L"apply_on_children"].IsNull())
			{
				new_pass.apply_on_children = pass_obj[L"apply_on_children"].GetBool();
			}
			if (!pass_obj[L"filename"].IsNull())
			{
				new_pass.material = MaterialHandle(pass_obj[L"filename"].GetString());
			}

			out_passes.push_back(new_pass);
	#ifndef PRODUCTION_CONFIGURATION
			out_passes.back().origin = origin;
	#endif
		}
	}

	std::wstring AddIndent(const std::wstring& str, size_t indent = 1)
	{
		if (str.empty() || indent == 0)
			return str;
		std::wstringstream s;
		for (size_t a = 0; a < str.length(); a++)
		{
			s << str[a];
			if (str[a] == L'\n' && a + 1 < str.length())
				for (size_t a = 0; a < indent; a++)
					s << L'\t';
		}
		return s.str();
	}

	void SaveRenderPasses(std::wostream& stream, const Visual::MaterialPasses& material_passes, size_t indent /*= 1*/, Objects::Origin origin /*= Objects::Origin::Client*/)
	{
		using namespace rapidjson;
		using JsonEncoding = UTF16LE<>;
		using JsonDocument = GenericDocument<JsonEncoding>;
		using JsonAllocator = JsonDocument::AllocatorType;
		using JsonValue = GenericValue<JsonEncoding>;

		JsonDocument doc;
		doc.SetObject();
		auto& allocator = doc.GetAllocator();

		JsonValue pass_array(kArrayType);
		for (const auto& material_pass : material_passes)
		{
	#ifndef PRODUCTION_CONFIGURATION
			if (material_pass.origin != origin)
				continue;
	#endif

			JsonValue pass_obj(kObjectType);

			pass_obj.AddMember(L"is_main", JsonValue().SetBool(material_pass.index == 0), allocator);

			auto apply_mode_str = StringToWstring(magic_enum::enum_name(material_pass.apply_mode));
			pass_obj.AddMember(L"type", JsonValue().SetString(apply_mode_str.c_str(), (int)apply_mode_str.length(), allocator), allocator);

			const std::wstring filename = material_pass.material.GetFilename().c_str();
			pass_obj.AddMember(L"filename", JsonValue().SetString(filename.c_str(), (int)filename.length(), allocator), allocator);

			pass_obj.AddMember(L"apply_on_children", JsonValue().SetBool(material_pass.apply_on_children), allocator);

			pass_array.PushBack(pass_obj, allocator);
		}

		if (!pass_array.Empty())
			doc.AddMember(L"passes", pass_array, allocator);

		GenericStringBuffer<JsonEncoding> buffer;
		PrettyWriter<GenericStringBuffer<JsonEncoding>, JsonEncoding, JsonEncoding> writer(buffer);
		doc.Accept(writer);
		stream << AddIndent(buffer.GetString(), indent);
	}

	// TODO: conversion code, remove once conversion is done
	void ConvertGraphs(const GraphPasses& graph_passes, const std::wstring_view src_filename, MaterialPasses& material_passes, const ApplyMode apply_mode)
	{
		for (const auto& graph_pass : graph_passes)
		{
			const auto type = PathHelper::GetExtension(src_filename).substr(1);
			std::wstring material_filename = PathHelper::RemoveExtension(src_filename) + L"_" + type + L"_pass" + std::to_wstring(graph_pass.first) + L".mat";
			Visual::MaterialPass new_pass;
			new_pass.material = MaterialHandle(MaterialCreator(material_filename, graph_pass.second));
			new_pass.index = graph_pass.first;
			new_pass.apply_mode = Visual::ApplyMode::Append;
			material_passes.push_back(new_pass);
		}
	}

	typedef Memory::FlatMap<unsigned, MaterialHandle, Memory::Tag::EffectGraphInstances> MatOverrides;
	void ConvertMaterialOverrides(const MatOverrides& mat_overrides, MaterialPasses& material_passes)
	{
		for (const auto& mat_override : mat_overrides)
		{
			const auto pass_index = mat_override.first;

			MaterialHandle material;
			for (auto& pass : material_passes)
			{
				if (pass.index == mat_override.first)
				{
					material = pass.material;
					break;
				}
			}

			if (material.IsNull())
			{
				Visual::MaterialPass new_pass;
				new_pass.material = mat_override.second;
				new_pass.index = pass_index;
				new_pass.apply_mode = Visual::ApplyMode::StandAlone;
				material_passes.push_back(new_pass);
			}
		}
	}


	COUNTER_ONLY(auto& ep_counter = Counters::Add(L"EffectPack");)

	// Implementation of epk reader (called via virtual functions from class in epkreader.h)
	class EffectPack::Reader : public FileReader::EPKReader
	{
	private:
		EffectPack* parent;
		const std::wstring& src_filename;

		// TODO: conversion code, remove once conversion is done
		GraphPasses graphs;
		MatOverrides material_overrides;

	public:
		Reader(EffectPack* parent, const std::wstring& filename) : parent(parent), src_filename(filename) {}
		~Reader()
		{
			// TODO: conversion code, remove once conversion is done
			ConvertGraphs(graphs, src_filename, parent->material_passes, Visual::ApplyMode::Append);
			ConvertMaterialOverrides(material_overrides, parent->material_passes);
		}

		void AddGraphInstance(const std::wstring& buffer, unsigned pass_index) override final
		{
			// TODO: conversion code, remove once conversion is done
			auto desc = std::make_shared<Renderer::DrawCalls::InstanceDesc>(src_filename, buffer);
			graphs[pass_index].emplace_back(desc);
		}

		void AddMaterialOverride(const std::wstring& filename, unsigned pass_index) override final
		{
			// TODO: conversion code, remove once conversion is done
			material_overrides[pass_index] = MaterialHandle(filename);
		}

		void AddTexture(const std::wstring& buffer) override final
		{
			// TODO: conversion code, remove once conversion is done
			TextureExport::LoadFromBuffer(src_filename, buffer, parent->textures);
		}

		void SetMaterialPass(const std::wstring & buffer) override final
		{
			Visual::ReadRenderPasses(buffer, parent->material_passes);
		}

		void AddTrailEffect(const std::wstring& filename, const Bone& bone, const bool ignore_errors ) override final
		{
			if( auto name = std::get_if<std::string>( &bone ) )
				parent->AddTrail( Trails::TrailsEffectHandle( filename ), *name, ignore_errors );
		}

		void AddParticleEffect(const std::wstring& filename, const Bone& bone, const bool ignore_errors ) override final
		{
			if( auto name = std::get_if<std::string>( &bone ) )
				parent->AddParticle( Particles::ParticleEffectHandle( filename ), *name, ignore_errors );
		}

		void AddAttachedObject(const std::wstring& filename, const Bone& bone, const bool ignore_errors ) override final
		{
			parent->AddAttachedObject( Animation::AnimatedObjectTypeHandle( filename ), bone, ignore_errors );
		}

		void AddAttachedObjects(const AttachmentInfo& info) override final
		{
			if( info.files.empty() )
			{
				throw Resource::Exception( src_filename ) << L"EffectPack::Reader::AddAttachedObjects failed as there were no attached object files listed";
				return;
			}

			AttachObjectsExInfo new_info;
			new_info.attached_objects.reserve(info.files.size());
			for( const auto& filename : info.files )
			{
				Animation::AnimatedObjectTypeHandle type;
				const size_t pos = filename.rfind( L'.' );
				if( pos == filename.npos )
					return;
				const std::wstring extension = filename.substr( pos + 1 );
				if( extension == L"sm" )
					type = Animation::CreateAnimatedObjectTypeFromSm( filename );
				else if( extension == L"fmt" || extension == L"ao" )
					type = Animation::CreateAnimatedObjectTypeFromFmtOrAo( filename );

				if( type.IsNull() )
					throw Resource::Exception( src_filename ) << L"EffectPack::Reader::AddAttachedObjects failed as \"" << filename << L"\" is not a valid attached object file type";

				new_info.attached_objects.push_back( type );
			}

			new_info.min_spawn = info.min_spawn;
			new_info.max_spawn = info.max_spawn;
			new_info.min_scale = info.min_scale;
			new_info.max_scale = info.max_scale;
			new_info.min_rotation = info.min_rotation;
			new_info.max_rotation = info.max_rotation;
			new_info.include_aux = info.include_aux;
			new_info.multi_attach = info.multi_attach;
			new_info.ignore_errors = info.ignore_errors;
			parent->AddAttachedObjectEx( std::move( new_info ), info.bone );
		}

		void AddChildAttachedObject( const std::wstring& filename, const std::wstring& args ) override final
		{
			ChildAttachedObject info;
			info.type = filename;
			std::stringstream ss( WstringToString( args ) );
			std::string token;
			while( ss >> token )
			{
				if( token == "from_bone" )
				{
					if( !ExtractString( ss, info.from.bone_name ) )
						throw Resource::Exception( src_filename ) << L"EffectPack::Reader::AddChildAttachedObject failed to parse from_bone";
				}
				else if( token == "from_child_bone" )
				{
					if( !ExtractString( ss, info.from.child_bone_name ) )
						throw Resource::Exception( src_filename ) << L"EffectPack::Reader::AddChildAttachedObject failed to parse from_child_bone";
				}
				else if( token == "from_bone_group_index" )
				{
					int from_bone_group_index = 0;
					ss >> from_bone_group_index;
					if( ss.fail() )
						throw Resource::Exception( src_filename ) << L"EffectPack::Reader::AddChildAttachedObject failed to parse from_bone_group_index";
					info.from.index_within_bone_group = from_bone_group_index;
				}
				else if( token == "from_child_bone_group_index" )
				{
					int from_child_bone_group_index = 0;
					ss >> from_child_bone_group_index;
					if( ss.fail() )
						throw Resource::Exception( src_filename ) << L"EffectPack::Reader::AddChildAttachedObject failed to parse from_child_bone_group_index";
					info.from.index_within_child_bone_group = from_child_bone_group_index;
				}
				else if( token == "to_bone" )
				{
					if( !ExtractString( ss, info.to.bone_name ) )
						throw Resource::Exception( src_filename ) << L"EffectPack::Reader::AddChildAttachedObject failed to parse to_bone";
				}
				else if( token == "to_child_bone" )
				{
					if( !ExtractString( ss, info.to.child_bone_name ) )
						throw Resource::Exception( src_filename ) << L"EffectPack::Reader::AddChildAttachedObject failed to parse to_child_bone";
				}
				else if( token == "to_bone_group_index" )
				{
					int to_bone_group_index = 0;
					ss >> to_bone_group_index;
					if( ss.fail() )
						throw Resource::Exception( src_filename ) << L"EffectPack::Reader::AddChildAttachedObject failed to parse to_bone_group_index";
					info.to.index_within_bone_group = to_bone_group_index;
				}
				else if( token == "to_child_bone_group_index" )
				{
					int to_child_bone_group_index = 0;
					ss >> to_child_bone_group_index;
					if( ss.fail() )
						throw Resource::Exception( src_filename ) << L"EffectPack::Reader::AddChildAttachedObject failed to parse to_child_bone_group_index";
					info.to.index_within_child_bone_group = to_child_bone_group_index;
				}
				else
					throw Resource::Exception( src_filename ) << L"EffectPack::Reader::AddChildAttachedObject failed to parse token \"" << StringToWstring( token ) << L"\"";
			}

			parent->attached_child_objects.push_back( std::move( info ) );
		}

		void AddMiscEffectPack(const std::wstring& effect_name, MiscEffectType type, float delay) override final
		{
			const auto misc_effect_packs = Loaders::MiscEffectPacks::GetTable();
			const auto row = misc_effect_packs->GetDataRowByKey(effect_name.c_str());

			if (row.IsNull())
				throw Resource::Exception(src_filename) << L"MiscEffectPack (" << StringToWstring(std::string(magic_enum::enum_name(type))) << L"): Invalid MiscEffectPack '" << effect_name << L"'";

			if (type == MiscEffectType::OnEnd)
			{
				parent->play_epk_on_end_handle.Release();
				parent->play_epk_on_end_player_handle.Release();

				parent->play_epk_on_end = Loaders::GetDataRowEnum(row);
				if (row->GetEffectPack() && *row->GetEffectPack())
					parent->play_epk_on_end_handle = row->GetEffectPack();

				if (row->GetEffectPackPlayer() && *row->GetEffectPackPlayer())
					parent->play_epk_on_end_player_handle = row->GetEffectPackPlayer();
			}
			else
			{
				parent->play_start_epk_after_delay = type == MiscEffectType::AfterDelay ? delay : 0.0f;

				parent->play_epk_on_begin_handle.Release();
				parent->play_epk_on_begin_player_handle.Release();

				parent->play_epk_on_begin = Loaders::GetDataRowEnum(row);
				if (row->GetEffectPack() && *row->GetEffectPack())
					parent->play_epk_on_begin_handle = row->GetEffectPack();

				if (row->GetEffectPackPlayer() && *row->GetEffectPackPlayer())
					parent->play_epk_on_begin_player_handle = row->GetEffectPackPlayer();
			}
		}

		void SetParentOnlyEffects(bool parent_only) override final
		{
			parent->parent_only_effects = parent_only;
		}
	};

	// Effect pack implementations
	EffectPack::EffectPack( const std::wstring& filename, Resource::Allocator& ) : id(Objects::Type::GetTypeNameHash(filename.c_str()))
	{
		COUNTER_ONLY(Counters::Increment(ep_counter);)

		PROFILE_ONLY(Memory::StackTag tag(Memory::Tag::Effects));

		Reader::Read(filename, Reader(this, filename));

		Warmup();
	}

	EffectPack::~EffectPack()
	{
		COUNTER_ONLY(Counters::Decrement(ep_counter);)
	}

	Animation::Components::GatherOut GatherGraphs(
		MaterialHandle material,
		const bool has_uv_alpha,
		const uint32_t mesh_flags)
	{
		Animation::Components::GatherOut result;
		Animation::Components::GatherBaseGraphs(result, material, Animation::Components::EffectsDrawCalls::EffectsDrawCallMaterialInfos(), has_uv_alpha, mesh_flags);
		return result;
	}

	void EffectPack::Warmup(const uint32_t mesh_flags, const bool has_uv_alpha)
	{
		const Mesh::VertexLayout layout(mesh_flags);
		const auto vertex_elements = Device::SetupVertexLayoutElements(layout);

		for(const auto& pass : material_passes)
		{
			if (pass.apply_mode != ApplyMode::StandAlone)
				continue;

			auto shader_base = Shader::Base();

			const auto gather = GatherGraphs(pass.material, has_uv_alpha, mesh_flags);

			Renderer::DrawCalls::Filenames graph_filenames;
			for (const auto& graph_desc : gather.graph_descs)
				graph_filenames.push_back(std::wstring(graph_desc.GetGraphFilename()));
			shader_base.AddEffectGraphs(graph_filenames, 0);

			shader_base.SetBlendMode(gather.blend_mode);

			Shader::System::Get().Warmup(shader_base);

			auto renderer_base = Renderer::Base()
				.SetShaderBase(shader_base)
				.SetVertexElements(vertex_elements);
			Renderer::System::Get().Warmup(renderer_base);
		}
	}

	void EffectPack::Warmup()
	{
		const auto default_mesh_flags = Mesh::VertexLayout::FLAG_HAS_NORMAL | Mesh::VertexLayout::FLAG_HAS_TANGENT | Mesh::VertexLayout::FLAG_HAS_UV1;

		Warmup(default_mesh_flags, false);  // Fixed mesh shaders consists of about 40% of the ao shaders after some test run
		//Warmup(default_mesh_flags | Mesh::VertexLayout::FLAG_HAS_UV2, false);
		//Warmup(default_mesh_flags | Mesh::VertexLayout::FLAG_HAS_COLOR, false);
		//Warmup(default_mesh_flags | Mesh::VertexLayout::FLAG_HAS_UV2 | Mesh::VertexLayout::FLAG_HAS_COLOR, false);
		Warmup(default_mesh_flags | Mesh::VertexLayout::FLAG_HAS_BONE_WEIGHT, false);  // Skinned mesh without uvalpha shaders consists of about 47% of the ao shaders after some test run
		//Warmup(default_mesh_flags | Mesh::VertexLayout::FLAG_HAS_BONE_WEIGHT | Mesh::VertexLayout::FLAG_HAS_UV2, false);
		//Warmup(default_mesh_flags | Mesh::VertexLayout::FLAG_HAS_BONE_WEIGHT | Mesh::VertexLayout::FLAG_HAS_COLOR, false);
		//Warmup(default_mesh_flags | Mesh::VertexLayout::FLAG_HAS_BONE_WEIGHT | Mesh::VertexLayout::FLAG_HAS_UV2 | Mesh::VertexLayout::FLAG_HAS_COLOR, false);
		Warmup(default_mesh_flags | Mesh::VertexLayout::FLAG_HAS_BONE_WEIGHT, true); // Skinned mesh with uvalpha shaders consists of about 13% of the ao shaders after some test run
		//Warmup(default_mesh_flags | Mesh::VertexLayout::FLAG_HAS_BONE_WEIGHT | Mesh::VertexLayout::FLAG_HAS_UV2, true);
		//Warmup(default_mesh_flags | Mesh::VertexLayout::FLAG_HAS_BONE_WEIGHT | Mesh::VertexLayout::FLAG_HAS_COLOR, true);
		//Warmup(default_mesh_flags | Mesh::VertexLayout::FLAG_HAS_BONE_WEIGHT | Mesh::VertexLayout::FLAG_HAS_UV2 | Mesh::VertexLayout::FLAG_HAS_COLOR, true);
	}

	void EffectPack::AddParticle( Particles::ParticleEffectHandle particle, const Bone& bone, const bool ignore_errors )
	{
		particles.emplace_back();
		particles.back().handle = particle;
		particles.back().SetBoneGroup( bone );
		particles.back().ignore_errors = ignore_errors;
	}

	void EffectPack::RemoveParticle( const unsigned index )
	{
		particles.erase( particles.begin() + index );
	}

	void EffectPack::AddAttachedObject( Animation::AnimatedObjectTypeHandle attached_object, const Visual::Bone& bone, const bool ignore_errors )
	{
		attached_objects.emplace_back();
		attached_objects.back().handle = attached_object;
		attached_objects.back().name_is_bone_group = false;
		attached_objects.back().SetBoneGroup( bone );
		attached_objects.back().ignore_errors = ignore_errors;
	}

	void EffectPack::RemoveAttachedObject( const unsigned index )
	{
		attached_objects.erase( attached_objects.begin() + index );
	}

	void EffectPack::AddAttachedObjectEx( Visual::AttachObjectsExInfo&& attached_object, const Visual::Bone& bone )
	{
		attached_objects_ex.push_back( std::move( attached_object ) );
		attached_objects_ex.back().SetBoneGroup( bone );
	}

	void EffectPack::RemoveAttachedObjectEx( const unsigned index )
	{
		attached_objects_ex.erase( attached_objects_ex.begin() + index );
	}

	void EffectPack::AddTrail( Trails::TrailsEffectHandle trail, const Visual::Bone& bone, const bool ignore_errors )
	{
		trail_effects.emplace_back();
		trail_effects.back().handle = trail;
		trail_effects.back().SetBoneGroup( bone );
		trail_effects.back().ignore_errors = ignore_errors;
	}

	void EffectPack::RemoveTrail( const unsigned index )
	{
		trail_effects.erase( trail_effects.begin() + index );
	}

	MaterialPass EffectPack::GetMaterialPass(const unsigned pass_index) const
	{
		for (const auto& material_pass : material_passes)
		{
			if (material_pass.index == pass_index)
				return material_pass;
		}
		return MaterialPass();
	}

	void EffectPack::AddMaterialPass(MaterialHandle material, const unsigned pass_index, ApplyMode apply_mode, const bool apply_on_children)
	{
		if (material.IsNull())
			RemoveMaterialPass(pass_index);
		else
		{
			for (auto& material_pass : material_passes)
			{
				if (material_pass.index == pass_index)
				{
					material_pass.material = material;
					material_pass.apply_mode = apply_mode;
					material_pass.apply_on_children = apply_on_children;
					return;
				}
			}

			material_passes.push_back({ material, pass_index, apply_mode, apply_on_children });
		}
	}

	void EffectPack::RemoveMaterialPass(const unsigned pass_index)
	{
		material_passes.erase(std::remove_if(material_passes.begin(), material_passes.end(), [&](const auto& material_pass)
		{
			return material_pass.index == pass_index;
		}));
	}

	void EffectBase::SetBoneGroup( const Bone& bone )
	{
		if( auto name = std::get_if<std::string>( &bone ) )
		{
			if( BeginsWith( *name, "bone-" ) )
			{
				bone_name = name->substr( 5 );
				name_is_bone_group = false;
			}
			else if( BeginsWith( *name, "group-" ) )
			{
				bone_name = name->substr( 6 );
				name_is_bone_group = true;
			}
			else
			{
				bone_name = *name;
			}
		}
		else if( auto index = std::get_if<int>( &bone ) )
		{
			bone_index = *index;
		}
	}
}
