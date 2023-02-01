#include <set>

#include "Common/Utility/StringManipulation.h"
#include "Common/Utility/MurmurHash2.h"
#include "Common/Utility/Counters.h"

#include "Visual/Renderer/EffectGraphSystem.h"

#include "EffectGraphNodeFactory.h"
#include "EffectGraphNode.h"

namespace Renderer
{
	namespace DrawCalls
	{
		EffectGraphNodeStatic null_static_ref;

		const std::string StageNames[(unsigned)Stage::NumStage]
		{
			"VertexInit",
			"Animation",
			"LocalTransform",
			"LocalTransform_Calc",
			"LocalTransform_Final",
			"WorldTransform_Init",
			"WorldTransform",
			"WorldTransform_Calc",
			"WorldTransform_Final",
			"ProjectionTransform",
			"ProjectionTransform_Calc",
			"ProjectionTransform_Final",
			"VertexOutput",
			"VertexOutput_Calc",
			"VertexOutput_Final",
			"VertexOutput_Pass",
			"VertexToPixel",
			"PixelInit",
			"UVSetup",
			"UVSetup_Calc",
			"UVSetup_Final",
			"Texturing_Init",
			"Texturing",
			"Texturing_Calc",
			"Texturing_Final",
			"PreLighting",
			"PreLighting_Calc",
			"PreLighting_Final",
			"AlphaClip",
			"Lighting",
			"Lighting_Calc",
			"Lighting_Final",
			"CustomLighting",
			"CustomLighting_Calc",
			"CustomLighting_Final",
			"LightingEnd",
			"LightingEnd_Calc",
			"LightingEnd_Final",
			"PostLighting",
			"PostLighting_Calc",
			"PostLighting_Final",
			"FogStage",
			"FogStage_Calc",
			"FogStage_Final",
			"PixelOutput",
			"PixelOutput_Calc",
			"PixelOutput_Final",
			"ComputeInit",
			"ParticlesSetup",
			"ParticlesLifetimeInit",
			"ParticlesLifetime",
			"ParticlesLifetimeCalc",
			"ParticlesEmitInit",
			"ParticlesEmit",
			"ParticlesEmitCalc",
			"ParticlesUpdateInit",
			"ParticlesUpdate",
			"ParticlesUpdateCalc",
			"ParticlesPhysicInit",
			"ParticlesPhysic",
			"ParticlesPhysicCalc",
			"ParticlesIntegrate",
			"ParticlesPostIntegrate",
			"ParticlesCollision",
			"ParticlesCollisionCalc",
			"ParticlesWrite",
			"ParticlesWriteFinal",
		};

		const std::string LightingModelNames[(unsigned)LightingModel::NumLightingModel] =
		{
			"PhongMaterial",
			"SpecGlossPbrMaterial",
			"Anisotropy"
		};

		const std::string LightingModelMacros[(unsigned)LightingModel::NumLightingModel] =
		{
			"",
			"GGX",
			"ANISOTROPY"
		};

		const std::string GroupNodeName = "<Group>";
		const std::string LoopCountInputName = "<loop_count>";

		namespace EffectGraphUtil
		{
			namespace
			{
				class GroupStages
				{
				private:
					Memory::BitSet<size_t(Stage::NumStage)> stages;

				public:
					constexpr GroupStages() { }

					// Adds a single stage to the group
					constexpr void Push(Stage s) 
					{ 
						if (s < Stage::NumStage)
							stages.set(size_t(s));
					}

					// Adds all stages in the specified range from (first, last( to the group
					constexpr void PushRange(Stage first, Stage last)
					{
						const auto first_idx = std::min(size_t(first), size_t(Stage::NumStage));
						const auto last_idx = std::min(size_t(last), size_t(Stage::NumStage));
						for (auto a = first_idx; a < last_idx; a++)
							stages.set(a);
					}

					// Adds all stages in the specified range from (first, last) to the group
					constexpr void PushRangeInclusive(Stage first, Stage last)
					{
						PushRange(first, Stage(unsigned(last) + 1));
					}

					constexpr bool HasStage(Stage s) const { return s < Stage::NumStage && stages.test(size_t(s)); }

					void Patch(ShaderGroupStages& res) const
					{
						for (size_t a = 0; a < size_t(Stage::NumStage); a++)
							if (stages.test(a))
								res.insert(Stage(a));
					}
				};

				class ShaderMainGroups : public Memory::BitSet<magic_enum::enum_count<ShaderMainGroup>()>
				{
				public:
					constexpr ShaderMainGroups() noexcept = default;
					constexpr ShaderMainGroups(const ShaderMainGroups& o) noexcept : BitSet(o) {}
					constexpr ShaderMainGroups(ShaderMainGroup g) noexcept : ShaderMainGroups() { BitSet::set(size_t(g)); }

					constexpr ShaderMainGroups& operator=(const ShaderMainGroups& o) noexcept { BitSet::operator=(o); return *this; }
					constexpr bool operator[](ShaderMainGroup g) const noexcept { return BitSet::test(size_t(g)); }
					constexpr bool test(ShaderMainGroup g) const noexcept { return BitSet::test(size_t(g)); }
					constexpr ShaderMainGroups& set() noexcept { BitSet::set(); return *this; }
					constexpr ShaderMainGroups& set(ShaderMainGroup g, bool value = true) noexcept { BitSet::set(size_t(g), value); return *this; }
					constexpr ShaderMainGroups& reset() noexcept { BitSet::reset(); return *this; }
					constexpr ShaderMainGroups& reset(ShaderMainGroup g) noexcept { BitSet::reset(size_t(g)); return *this; }
					constexpr ShaderMainGroups& flip() noexcept { BitSet::flip(); return *this; }
					constexpr ShaderMainGroups& flip(ShaderMainGroup g) noexcept { BitSet::flip(size_t(g)); return *this; }

					constexpr ShaderMainGroups& operator&=(const ShaderMainGroups& o) noexcept { BitSet::operator&=(o); return *this; }
					constexpr ShaderMainGroups& operator|=(const ShaderMainGroups& o) noexcept { BitSet::operator|=(o); return *this; }
					constexpr ShaderMainGroups& operator^=(const ShaderMainGroups& o) noexcept { BitSet::operator^=(o); return *this; }

					constexpr ShaderMainGroups operator&(const ShaderMainGroups& o) const noexcept { return ShaderMainGroups(*this) &= o; }
					constexpr ShaderMainGroups operator|(const ShaderMainGroups& o) const noexcept { return ShaderMainGroups(*this) |= o; }
					constexpr ShaderMainGroups operator^(const ShaderMainGroups& o) const noexcept { return ShaderMainGroups(*this) ^= o; }
					constexpr ShaderMainGroups operator~() const noexcept { return ShaderMainGroups(*this).flip(); }
				};

				// Helper class to map from a Shader Group to a list of Stages (Only used in tools, declared here to be close to the Stage enum declaration)
				class ShaderGroupDesc
				{
				private:
					std::array<GroupStages, size_t(magic_enum::enum_count<ShaderMainGroup>())> groups = {}; // Groups for each ShaderMainGroup
					std::array<ShaderMainGroups, size_t(Stage::NumStage)> stages = {};
					std::array<ShaderMainGroups, size_t(magic_enum::enum_count<ShaderOptGroup>())> compatible_groups = {};

					constexpr GroupStages* Modify(ShaderMainGroup group)
					{
						if (auto a = magic_enum::enum_index(group))
							return &groups[*a];

						return nullptr;
					}

					constexpr void AddOpt(ShaderMainGroup g, ShaderOptGroup group)
					{
						if (auto a = magic_enum::enum_index(group))
							compatible_groups[*a] |= g;
					}

					constexpr void Finalize()
					{
						for (auto& a : stages)
							a.reset();

						for (const auto& b : magic_enum::enum_values<ShaderMainGroup>())
							if (auto g = Modify(b))
								for (size_t a = 0; a < size_t(Stage::NumStage); a++)
									if (g->HasStage(Stage(a)))
										stages[a].set(b);
					}

					ShaderGroupStages GetGroup(const ShaderMainGroups& group) const
					{
						ShaderGroupStages r;

						for (const auto& b : magic_enum::enum_values<ShaderMainGroup>())
							if (group.test(b))
								if (auto a = magic_enum::enum_index(b))
									groups[*a].Patch(r);
						
						return r;
					}

					constexpr void SetupMaterial()
					{
						if (auto group = Modify(ShaderMainGroup::Material))
						{
							group->PushRangeInclusive(Stage::VertexInit, Stage::VertexOutput_Pass);
							group->Push(Stage::VertexToPixel);
							group->PushRangeInclusive(Stage::PixelInit, Stage::PixelOutput_Final);
						}

						AddOpt(ShaderMainGroup::Material, ShaderOptGroup::Temporary);
						AddOpt(ShaderMainGroup::Material, ShaderOptGroup::ParticlesRender);
						AddOpt(ShaderMainGroup::Material, ShaderOptGroup::Trail);
						AddOpt(ShaderMainGroup::Material, ShaderOptGroup::TrailUVs);
					}

					constexpr void SetupParticlesUpdate()
					{
						if (auto group = Modify(ShaderMainGroup::ParticlesUpdate))
							group->PushRangeInclusive(Stage::ParticlesSetup, Stage::ParticlesWriteFinal);

						AddOpt(ShaderMainGroup::ParticlesUpdate, ShaderOptGroup::Temporary);
						AddOpt(ShaderMainGroup::ParticlesUpdate, ShaderOptGroup::ParticlesPhysics);
					}

				public:
					constexpr ShaderGroupDesc()
					{
						SetupMaterial();
						SetupParticlesUpdate();
						Finalize();
					}

					ShaderGroupStages GetGroup(const ShaderGroup& group) const
					{
						if (auto g = std::get_if<ShaderMainGroup>(&group))
							return GetGroup(ShaderMainGroups(*g));
						else if (auto g = std::get_if<ShaderOptGroup>(&group))
							if (auto a = magic_enum::enum_index(*g))
								return GetGroup(compatible_groups[*a]);

						return ShaderGroupStages();
					}

					constexpr bool IsEnabled(Stage stage, const ShaderGroup& group) const
					{
						if (auto g = std::get_if<ShaderMainGroup>(&group))
							return (stages[size_t(stage)] & *g) != ShaderMainGroups();
						else if (auto g = std::get_if<ShaderOptGroup>(&group))
							if (auto a = magic_enum::enum_index(*g))
								return (stages[size_t(stage)] & compatible_groups[*a]) != ShaderMainGroups();
						
						return false;
					}

					constexpr bool IsEnabled(Stage stage, const ShaderGroups& groups) const
					{
						return stage < Stage::NumStage && (stages[size_t(stage)] & groups.MainGroup) != ShaderMainGroups();
					}

					constexpr bool IsCompatible(ShaderMainGroup mainGroup, ShaderOptGroup optGroup) const
					{
						if (auto a = magic_enum::enum_index(optGroup))
							return compatible_groups[*a].test(mainGroup);

						return false;
					}
				};

				constexpr ShaderGroupDesc ShaderGroupInfo;
			}

			Stage GetStageFromString(const std::string& stage_string)
			{
				for(unsigned i = 0; i < (unsigned)Stage::NumStage; ++i)
					if(stage_string == StageNames[i])
						return (Stage)i;
				return Stage::NumStage;
			}

			LightingModel GetLightingModelFromString(const std::string& in_str)
			{
				for(unsigned i = 0; i < (unsigned)LightingModel::NumLightingModel; ++i)
					if(in_str == LightingModelNames[i])
						return (LightingModel)i;
				return LightingModel::NumLightingModel;
			}

			unsigned MAX_GROUP_INDEX = 4;
			unsigned GetGroupIndexByStage(const unsigned in_group_index, const Stage stage)
			{
				unsigned group_index = in_group_index;
				if (stage < Stage::UVSetup || stage > Stage::Texturing_Init)
					group_index = 0;
				return group_index;
			}

			unsigned GetMaxGroupIndex()
			{
				return MAX_GROUP_INDEX;
			}

			unsigned HashString(const std::string& name)
			{
				return MurmurHash2(name.c_str(), (int)name.length(), 0x34322);
			}

			unsigned MergeTypeId(const unsigned a, const unsigned b)
			{
				unsigned ab[2] = {a, b};
				return MurmurHash2(ab, sizeof(ab), 0x34322);
			}

			ShaderGroupStages GetGroupStages(const ShaderGroup& group)
			{
				return ShaderGroupInfo.GetGroup(group);
			}

			bool IsStageEnabled(Stage stage, const ShaderGroups& groups)
			{
				return ShaderGroupInfo.IsEnabled(stage, groups);
			}

			bool IsStageEnabled(Stage stage, const ShaderGroup& group)
			{
				return ShaderGroupInfo.IsEnabled(stage, group);
			}

			bool IsCompatible(ShaderMainGroup mainGroup, ShaderOptGroup optGroup)
			{
				return ShaderGroupInfo.IsCompatible(mainGroup, optGroup);
			}

			std::string AddDynamicParameterPrefix(const std::string& name)
			{
				return "custom_" + name;
			}
		}

		EffectGraphNodeStatic::EffectGraphNodeStatic(const std::string& name) : name(name), type_id(EffectGraphUtil::HashString(name))
		{
			is_group = (name == GroupNodeName);
			lighting_model_visibility.set();
		}

		EffectGraphMacros::StringPool EffectGraphMacros::macro_string_pool("EffectGraphMacro");

		void EffectGraphMacros::Clear()
		{
			macros.clear();
		}

		void EffectGraphMacros::Insert(const StringPool::Handle& macro_handle, const StringPool::Handle& value_handle)
		{
			macros[macro_handle] = value_handle;
		}

		void EffectGraphMacros::Erase(const StringPool::Handle& macro_handle)
		{
			macros.erase(macro_handle);
		}

		void EffectGraphMacros::Add(const std::string& macro, const std::string& value)
		{
			const auto macro_handle = macro_string_pool.grab(macro.data(), static_cast< unsigned >( macro.size() ) );
			const auto value_handle = macro_string_pool.grab(value.data(), static_cast< unsigned >( value.size() ) );
			Insert(macro_handle, value_handle);
		}

		void EffectGraphMacros::Remove(const std::string& macro)
		{
			for (auto& pair : macros)
			{
				if (pair.first.equals(macro.data(), static_cast< unsigned >( macro.size() ) ))
				{
					Erase(pair.first);
					return;
				}
			}
		}

		bool EffectGraphMacros::Exists(const std::string& macro) const
		{
			for (auto& pair : macros)
			{
				if (pair.first.equals(macro.data(), static_cast< unsigned >( macro.size() ) ))
					return true;
			}
			return false;
		}

		std::string EffectGraphMacros::Find(const std::string& macro) const
		{
			assert(Exists(macro));
			for (auto& pair : macros)
			{
				if (pair.first.equals(macro.data(), static_cast< unsigned >( macro.size() ) ))
					return std::string(pair.second.data(), pair.second.size());
			}
			return std::string();
		}


		//COUNTER_ONLY(auto& egn_counter = Counters::Add(L"EffectGraphNode");)

		EffectGraphNode::MaskStringPool connector_mask_string_pool("ConnectorInfoMask");

		EffectGraphNode::EffectGraphNode::ConnectorInfo::ConnectorInfo(const unsigned index, const std::string& mask)
			: index(index)
		{
			hash_id = EffectGraphUtil::MergeTypeId(index, EffectGraphUtil::HashString(mask));
			this->mask = connector_mask_string_pool.grab(mask.data(), static_cast< unsigned >(mask.size()));
		}

		EffectGraphNode::EffectGraphNode(const EffectGraphNodeStatic& base_node, const unsigned index, const Stage stage)
			: static_ref(&base_node)
		{
			PROFILE_ONLY(Memory::StackTag stack(Memory::Tag::EffectGraphNodes));

			SetSignature(index, stage);

			parameters.reserve(base_node.GetParamInfos().size());
			for (auto itr = base_node.GetParamInfos().begin(); itr != base_node.GetParamInfos().end(); ++itr)
			{
				auto& param_static = *itr;
				parameters.emplace_back(::EffectGraph::System::Get().CreateParameter(param_static));
			}

			//COUNTER_ONLY(Counters::Increment(egn_counter);)
		}

		EffectGraphNode::EffectGraphNode() : static_ref(&null_static_ref)
		{
		}

		EffectGraphNode::~EffectGraphNode()
		{
			//COUNTER_ONLY(Counters::Decrement(egn_counter);)
		}

		EffectGraphNode::ParameterList::iterator EffectGraphNode::begin()
		{
			return parameters.begin();
		}

		EffectGraphNode::ParameterList::iterator EffectGraphNode::end()
		{
			return parameters.end();
		}

		EffectGraphNode::ParameterList::const_iterator EffectGraphNode::begin() const
		{ 
			return parameters.begin(); 
		}

		EffectGraphNode::ParameterList::const_iterator EffectGraphNode::end() const
		{ 
			return parameters.end();
		}

		bool EffectGraphNode::HasNoEffect() const
		{
			// used for output type nodes, to quickly check whether this has any effect on the shader (and so can skip processing further)
			if(GetEffectNode().IsOutputOnly())
				return false;
			else if(input_links.empty())
				return true;
			else
			{
				// check if all its input links are connected to it's corresponding input type
				for(auto link_itr = input_links.begin(); link_itr != input_links.end(); ++link_itr)
				{
					auto input_node = link_itr->node;
					if(!input_node->static_ref->IsInputType() || input_node->GetStageNumber() != GetStageNumber())
						return false;
				}
			}
			return true;
		}

		void AddLinkInternal(EffectGraphNode::Links& links, EffectGraphNode& input_node, const unsigned output_link, const std::string& output_mask, const unsigned input_link, const std::string& input_mask)
		{
			const auto& input_connector_mask = EffectGraphNode::ConnectorInfo(input_link, input_mask);
			EffectGraphNode::Link* link = nullptr;
			for (auto& _link : links)
			{
				if (_link.input_info == input_connector_mask)
				{
					link = &_link;
					break;
				}
			}

			const auto& output_connector_mask = EffectGraphNode::ConnectorInfo(output_link, output_mask);
			if (link)
			{
				link->output_info = output_connector_mask;
				link->node = &input_node;
			}
			else
			{
				links.emplace_back(input_connector_mask, output_connector_mask, &input_node);
				std::sort(links.begin(), links.end(), [&](const EffectGraphNode::Link& a, const EffectGraphNode::Link& b)
				{
					const auto index_a = a.input_info.index;
					const auto index_b = b.input_info.index;
					if (index_a == index_b)
						return a.input_info.Mask() < b.input_info.Mask();
					return index_a < index_b;
				});
			}
		}

		void EffectGraphNode::AddInput(EffectGraphNode& input_node, const unsigned output_link, const std::string& output_mask, const unsigned input_link, const std::string& input_mask)
		{
			AddLinkInternal(input_links, input_node, output_link, output_mask, input_link, input_mask);
			++input_node.num_output_links;
		}

		void EffectGraphNode::AddStageInput(EffectGraphNode& input_node, const unsigned output_link, const std::string& output_mask, const unsigned input_link, const std::string& input_mask)
		{
			AddLinkInternal(stage_links, input_node, output_link, output_mask, input_link, input_mask);
			++input_node.num_output_links;
		}

		void EffectGraphNode::AddChildInput(EffectGraphNode& input_node, const unsigned output_link, const std::string& output_mask, const unsigned input_link, const std::string& input_mask)
		{
			AddLinkInternal(child_links, input_node, output_link, output_mask, input_link, input_mask);
			++input_node.num_output_links;
		}

		Device::ShaderType EffectGraphNode::GetPreferredShader() const
		{
			if (static_ref)
			{
				switch (static_ref->GetShaderUsage())
				{
					case ShaderUsageType::Compute:		return Device::ShaderType::COMPUTE_SHADER;	break;
					case ShaderUsageType::Pixel:		return Device::ShaderType::PIXEL_SHADER;	break;
					case ShaderUsageType::Vertex:		return Device::ShaderType::VERTEX_SHADER;	break;
					case ShaderUsageType::VertexPixel:
						if (preferred_shader != Device::ShaderType::VERTEX_SHADER)
							return Device::ShaderType::PIXEL_SHADER;

						break;
					default:
						break;
				}
			}

			return preferred_shader;
		}

		void EffectGraphNode::LoadParameters(const JsonValue& node_obj)
		{
			const auto& parameters_obj = node_obj[L"parameters"];
			if (!parameters_obj.IsNull())
			{
				auto param_info = begin();
				for (const auto& param : parameters_obj.GetArray())
				{
					if (param_info == end())
						break;
					(*param_info)->FillFromData(param);
					++param_info;
				}			
			}
		}

		void EffectGraphNode::SaveParameters(JsonValue& node_obj, JsonAllocator& allocator) const
		{
			JsonValue param_array(rapidjson::kArrayType);
			for (const auto& param : *this)
			{
				JsonValue param_obj(rapidjson::kObjectType);
				param->SaveData(param_obj, allocator);
				param_array.PushBack(param_obj, allocator);
			}

			if (!param_array.Empty())
				node_obj.AddMember(L"parameters", param_array, allocator);
		}

		bool EffectGraphNode::CalculateTypeId(bool merge_pass /*= false*/)
		{
			if(GetTypeId() != 0)
				return false;

			unsigned node_id = 0;
			if(!GetInputSlots().empty() || static_ref->IsInputType())
			{				
				for(auto input_link = input_links.begin(); input_link != input_links.end(); ++input_link)
				{
					node_id = EffectGraphUtil::MergeTypeId(node_id, input_link->input_info.hash_id);
					node_id = EffectGraphUtil::MergeTypeId(node_id, input_link->output_info.hash_id);
					const auto* input_node = input_link->node;
					node_id = EffectGraphUtil::MergeTypeId(node_id, input_node->GetTypeId());
				}
			}

			// Samplers needs to be taken into account now
			for (const auto& param : *this)
			{
				const GraphType type = param->GetType();
				if (type == GraphType::Sampler)
				{
					const auto sampler_index = *param->GetParameter().AsInt();
					node_id = EffectGraphUtil::MergeTypeId(node_id, sampler_index);
				}
			}
			
			// Need to take into account the current custom names
			for (const auto& custom_name : custom_dynamic_names)
				if (!custom_name.empty())
					node_id = EffectGraphUtil::MergeTypeId(node_id, EffectGraphUtil::HashString(EffectGraphUtil::AddDynamicParameterPrefix(custom_name)));

			// During merge pass we use the instance hash id instead of the node type's hash because we want to generate a new shader when
			// a merge happens but the result is basically the same as not having to merge it at all (usually when overwriting an output) 
			// but we want the constants to have correct indexing. We do this only during merge pass because the nodes indices are already sorted. 
			// None-merge pass means we are calculating type id by individual graphs and is used for drawcall hashes.
			if(merge_pass)
			{
				assert(GetHashId() != 0);
				node_id = EffectGraphUtil::MergeTypeId(node_id, GetHashId());
			}
			else
			{
				if (static_ref->IsInputType() || static_ref->IsOutputType())
					node_id = EffectGraphUtil::MergeTypeId(node_id, EffectGraphUtil::HashString(StageNames[(unsigned)GetStage()]));
				node_id = EffectGraphUtil::MergeTypeId(node_id, static_ref->GetTypeId());
			}

			SetTypeId(node_id);

			return true;
		}
		
		void EffectGraphNode::SetSignature(const unsigned index, const Stage stage)
		{
			signature = Signature(static_ref->GetTypeId(), index, stage);
			
			if (static_ref->IsInputType() || static_ref->IsOutputType())
				stage_number = (unsigned)signature.stage * GetEffectGraphNodeFactory().GetNumOutputTypes() + (unsigned)static_ref->GetOutputTypeIndex();
			else if(static_ref->IsOutputOnly())
				stage_number = (unsigned)Stage::NumStage * GetEffectGraphNodeFactory().GetNumOutputTypes() + static_ref->GetTypeId();
			else
				stage_number = (unsigned)Stage::NumStage * GetEffectGraphNodeFactory().GetNumOutputTypes();
		}
	}
}