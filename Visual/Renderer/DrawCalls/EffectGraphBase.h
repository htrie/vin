#pragma once
#include <rapidjson/document.h>
#include <rapidjson/error/en.h>
#include "Common/Resource/Handle.h"
#include "Common/Utility/Counters.h"
#include "Common/FileReader/BlendMode.h"
#include "Common/FileReader/FXGRAPHReader.h"
#include "Common/FileReader/MATReader.h"
#include "Common/Utility/Logger/Logger.h"
#include "Visual/Utility/DXHelperFunctions.h"
#include "Visual/Utility/TextureMetadata.h"
#include "Visual/Renderer/DynamicParameterManager.h"
#include "Visual/Renderer/DrawCalls/EffectGraphNode.h"
#include "Visual/Renderer/DrawCalls/EffectOrder.h"
#include "Visual/Device/State.h"

namespace Renderer
{
	namespace DrawCalls
	{
		const unsigned GRAPH_VERSION = 3;

		struct InputOutputKey
		{
			unsigned group;
			unsigned stage;
			InputOutputKey(unsigned group, unsigned stage) : group(group), stage(stage) {}
			inline bool operator == (const InputOutputKey& other) const { return group == other.group && stage == other.stage; }
			inline bool operator != (const InputOutputKey& other) const { return group != other.group || stage != other.stage; }
			inline bool operator < (const InputOutputKey& other) const { return group < other.group || (group == other.group && stage < other.stage); }
		};

		struct EffectGraphStateOverwrite
		{
			template<typename T> class Overwriteable
			{
			private:
				T value;
				bool overwritten = false;

			public:
				bool Overwrite(T& dst) const
				{
					if (overwritten)
						dst = value;
					
					return overwritten;
				}

				unsigned int Hash() const
				{
					if (overwritten)
						return MurmurHash2(&value, sizeof(value), 0x34322);

					return 0;
				}

				bool IsOverwritten() const { return overwritten; }
				const T& Get() const { return value; }
				operator T() const { return value; }

				void Merge(const Overwriteable<T>& o) { overwritten = o.Overwrite(value) || overwritten; }
				Overwriteable<T>& operator= (const T& v){ value = v; overwritten = true; return *this; }
				void Reset() { overwritten = false; }

				friend std::istream& operator>>(std::istream& in, Overwriteable<T>& c) { in >> c.value; c.overwritten = true; return in; }
			};

			struct
			{
				Overwriteable<float> depth_bias;
				Overwriteable<float> slope_scale;
				Overwriteable<Device::FillMode> fill_mode;
				Overwriteable<Device::CullMode> cull_mode;
			} rasterizer;
			struct
			{
				Overwriteable<bool> depth_test_enable;
				Overwriteable<bool> depth_write_enable;
				Overwriteable<Device::CompareMode> compare_mode;
				Overwriteable<Device::StencilState> stencil_state;
				bool disable_depth_write_if_blending = false;
			} depth_stencil;
			struct
			{
				Overwriteable<Device::BlendMode> color_src;
			} blend;

			void Reset();
			unsigned int Hash() const;
			void Merge(const EffectGraphStateOverwrite& other);
			void Overwrite(Device::RasterizerState& state) const;
			void Overwrite(Device::DepthStencilState& state, BlendMode blend_mode) const;
			void Overwrite(Device::BlendTargetState& state) const;
			void Overwrite(Device::BlendState& state) const;
		};
		
		typedef Memory::VectorMap<InputOutputKey, Memory::Vector<unsigned, Memory::Tag::EffectGraphs>, Memory::Tag::EffectGraphs> EffectGraphNodeBuckets;
		typedef Memory::VectorMap<InputOutputKey, unsigned, Memory::Tag::EffectGraphs> ConstEffectGraphNodes; // Needs to be sorted.
		typedef Memory::LockStringPool<char, 8*1024, 256, Memory::Tag::EffectGraphs> EffectGraphFilenameStringPool;
		typedef Memory::SmallVector<std::pair<EffectGraphFilenameStringPool::Handle, unsigned>, 8, Memory::Tag::EffectGraphs> EffectGraphFilenames;

		template <typename NodeType>
		class EffectGraphBase
		{
		protected:
			class Reader;

		public:
			EffectGraphBase();
			EffectGraphBase(const EffectGraphBase& other) = delete;
			virtual ~EffectGraphBase();

			const EffectGraphStateOverwrite&	GetStateOverwrites()		const { return state_overwrites; }
			const EffectGraphMacros&			GetCustomMacros()			const { return custom_macros; }
			const simd::vector4&				GetAlphaRef()				const { return alpha_ref; }	
			UniformInputInfo					GetAlphaRefUniformInput()	const { return { Device::UniformInput(0).SetVector(&alpha_ref), DrawDataNames::AlphaTesting, 0 }; }
			EffectOrder::Value					GetEffectOrder()			const { return effect_order; }
			LightingModel						GetOverridenLightingModel()	const { return overriden_lighting_model; }
			BlendMode							GetOverridenBlendMode()		const { return overriden_blend_mode; }
			unsigned							GetTypeId()					const { return type_id; }
			const auto&							GetShaderGroups()			const { return shader_group; }
			
			bool IsLightingModelOverriden()	const { return lighting_model_overriden; }	
			bool IsBlendModeOverriden()		const { return blend_mode_overriden; }
			bool IsAlphaRefOverriden()		const { return alpha_ref_overriden; }
			bool IsLightingDisabled()		const { return !!(flags & DisableLightingFlag); }
			bool IsConstant()				const { return !!(flags & Constant); }
			bool IsIgnoreConstant()			const { return !!(flags & IgnoreConstant); }
			bool IsUseStartTime()			const { return !!(flags & UseStartTimeFlag); }
			bool HasCustomParameters()		const { return !!(flags & HasParameters); }
			bool IsEmpty()					const { return nodes.empty(); }
					
			template <typename F>
			void ProcessInputNodes(F func) const
			{
				for (auto itr = input_nodes.begin(); itr != input_nodes.end(); ++itr)
					func(itr->second, itr->first.group);
			}

			template <typename F>
			void ProcessOutputNodes(F func) const
			{
				for (auto itr = output_nodes.begin(); itr != output_nodes.end(); ++itr)
					func(itr->second, itr->first.group);
			}

			template <typename F>
			void ProcessOutputOnlyNodes(F func) const
			{
				for (auto itr = output_only_nodes.begin(); itr != output_only_nodes.end(); ++itr)
					func(itr->second, 0);
			}

			template <typename F>
			void ProcessDynamicParams(F func) const
			{
				for (auto itr = dynamic_params.begin(); itr != dynamic_params.end(); ++itr)
					func(itr->second.info);
			}

			template <typename F>
			void ProcessCustomDynamicParams(F func) const
			{
				for (const auto& param : custom_dynamic_params)
					func(param.id);
			}

			template <typename F>
			void ProcessAllNodes(F func) const
			{
				for (const auto& node : nodes)
					if(node)
						func(*node);
			}

			void CreateEffectGraphFromBuffer(std::wstring& buffer);
			void CreateEffectGraph(const JsonValue& graph_obj);
			void CalculateTypeId();
			const NodeType& GetNode(const unsigned index) const { return *nodes[index].get(); }

			// TODO: Consider these for removal. Used mostly for tools but on Client, this is used by GraphReloader
			void ResetGraph();

		protected:
			virtual Memory::Object<NodeType> CreateEffectGraphNode(const std::string& type_name, const unsigned index, const Stage stage) = 0;
			void OnAddNode(const unsigned node_index, const unsigned group_index = 0);

			unsigned version = 0;
			unsigned type_id = 0;
			EffectGraphStateOverwrite state_overwrites;
			Memory::Vector<Memory::Object<NodeType>, Memory::Tag::EffectGraphNodes> nodes;
			ConstEffectGraphNodes input_nodes;
			ConstEffectGraphNodes output_nodes;
			FileReader::FFXReader::ShaderGroups shader_group = FileReader::FFXReader::ShaderMainGroup::Material;
			Memory::VectorMap<unsigned, unsigned, Memory::Tag::EffectGraphs> output_only_nodes;

			enum Flags
			{
				DisableLightingFlag = 1,
				Constant = 8,
				IgnoreConstant = 16,
				UseStartTimeFlag = 32,
				HasParameters = 64,
			};
			unsigned short flags = 0;
			bool lighting_model_overriden = false;
			bool blend_mode_overriden = false;
			bool alpha_ref_overriden = false;
			simd::vector4 alpha_ref = simd::vector4(1.0f, 0.5f, 0.001f, 1.0f);
			BlendMode overriden_blend_mode = BlendMode::Opaque;
			LightingModel overriden_lighting_model = LightingModel::PhongMaterial;
			EffectOrder::Value effect_order = EffectOrder::Default;
			EffectGraphMacros custom_macros;	
			
			struct GraphDynamicParamInfo
			{
				const DynamicParameterInfo* info = nullptr;
				unsigned count = 1;
				GraphDynamicParamInfo(const DynamicParameterInfo* info) : info(info) {}
			};
			Memory::FlatMap<unsigned, GraphDynamicParamInfo, Memory::Tag::DrawCalls> dynamic_params;

			struct CustomGraphDynamicParamInfo
			{
				const unsigned id = 0;
				unsigned count = 1;
				CustomGraphDynamicParamInfo(const unsigned id) : id(id) {}
			};
			Memory::Vector<CustomGraphDynamicParamInfo, Memory::Tag::DrawCalls> custom_dynamic_params;

			// TODO: Consider for removal. 
			std::wstring filename;
			EffectGraphFilenames filenames;

		protected:
			class Reader : public FileReader::FXGRAPHReader
			{
			private:
				EffectGraphBase* parent;

			public:
				Reader(EffectGraphBase* parent) : parent(parent) {}

				void SetOverwrites(Flags flags, const Overwrites& overwrites) override final
				{
					parent->state_overwrites.Reset();

					unsigned uflags = unsigned(flags);
					if ((uflags & unsigned(Flags::DisableLighting)) != 0)
						parent->flags |= DisableLightingFlag;

					if ((uflags & unsigned(Flags::Constant)) != 0)
						parent->flags |= Constant;

					if ((uflags & unsigned(Flags::IgnoreConstant)) != 0)
						parent->flags |= IgnoreConstant;

					if ((uflags & unsigned(Flags::UseStartTime)) != 0)
						parent->flags |= UseStartTimeFlag;

					if (overwrites.alpha_ref)
					{
						parent->alpha_ref_overriden = true;
						parent->alpha_ref = simd::vector4(1.0f, *overwrites.alpha_ref, 0.001f, 1.0f);
					}

					if (overwrites.blend_mode)
					{
						parent->blend_mode_overriden = true;
						parent->overriden_blend_mode = *overwrites.blend_mode;
					}

					if (overwrites.effect_order)
					{
						parent->effect_order = (EffectOrder::Value)*overwrites.effect_order;
					}

					if (overwrites.lighting_model)
					{
						parent->lighting_model_overriden = true;
						auto lighting_model = EffectGraphUtil::GetLightingModelFromString(WstringToString(*overwrites.lighting_model));
						if (lighting_model == LightingModel::NumLightingModel)
							throw Resource::Exception(parent->filename) << L"Invalid lighting model set for effect graph";

						parent->overriden_lighting_model = lighting_model;
					}

					if (overwrites.rasterizer.cull_mode)
					{
						auto cull_mode = Utility::StringToCullMode(WstringToString(*overwrites.rasterizer.cull_mode));
						if (cull_mode == Device::CullMode::NumCullMode)
							throw Resource::Exception(parent->filename) << L"render_state_overwrite.cull_mode has an invalid value";

						parent->state_overwrites.rasterizer.cull_mode = cull_mode;
					}

					if (overwrites.rasterizer.fill_mode)
					{
						auto fill_mode = Utility::StringToFillMode(WstringToString(*overwrites.rasterizer.fill_mode));
						if (fill_mode == Device::FillMode::NumFillMode)
							throw Resource::Exception(parent->filename) << L"render_state_overwrite.fill_mode has an invalid value";

						parent->state_overwrites.rasterizer.fill_mode = fill_mode;
					}

					if (overwrites.rasterizer.depth_bias)
					{
						parent->state_overwrites.rasterizer.depth_bias = *overwrites.rasterizer.depth_bias;
					}

					if (overwrites.rasterizer.slope_scale)
					{
						parent->state_overwrites.rasterizer.slope_scale = *overwrites.rasterizer.slope_scale;
					}

					if (overwrites.depth_stencil.depth_test_enable)
					{
						parent->state_overwrites.depth_stencil.depth_test_enable = *overwrites.depth_stencil.depth_test_enable;
					}

					if (overwrites.depth_stencil.depth_write_enable)
					{
						parent->state_overwrites.depth_stencil.depth_write_enable = *overwrites.depth_stencil.depth_write_enable;
					}

					if (overwrites.depth_stencil.compare_mode)
					{
						auto compare_mode = Utility::StringToCompareMode(WstringToString(*overwrites.depth_stencil.compare_mode));
						if (compare_mode == Device::CompareMode::NumCompareMode)
							throw Resource::Exception(parent->filename) << L"depth_stencil.compare_mode has an invalid value";

						parent->state_overwrites.depth_stencil.compare_mode = compare_mode;
					}

					if (overwrites.depth_stencil.disable_depth_write_if_blending)
					{
						parent->state_overwrites.depth_stencil.disable_depth_write_if_blending = *overwrites.depth_stencil.disable_depth_write_if_blending;
					}

					if (overwrites.depth_stencil.stencil)
					{
						if (overwrites.depth_stencil.stencil->enabled)
						{
							Device::StencilState new_stencil;
							new_stencil.enabled = true;
							new_stencil.ref = overwrites.depth_stencil.stencil->ref;
							new_stencil.compare_mode = Utility::StringToCompareMode(WstringToString(overwrites.depth_stencil.stencil->compare));
							if (new_stencil.compare_mode == Device::CompareMode::NumCompareMode)
								throw Resource::Exception(parent->filename) << L"render_state_overwrite.stencil.compare has an invalid value";

							new_stencil.pass_op = Utility::StringToStencilOp(WstringToString(overwrites.depth_stencil.stencil->pass));
							if (new_stencil.pass_op == Device::StencilOp::NumStencilOp)
								throw Resource::Exception(parent->filename) << L"render_state_overwrite.stencil.pass has an invalid value";

							new_stencil.z_fail_op = Utility::StringToStencilOp(WstringToString(overwrites.depth_stencil.stencil->z_fail));
							if (new_stencil.z_fail_op == Device::StencilOp::NumStencilOp)
								throw Resource::Exception(parent->filename) << L"render_state_overwrite.stencil.z_fail has an invalid value";

							parent->state_overwrites.depth_stencil.stencil_state = new_stencil;
						}
						else
						{
							parent->state_overwrites.depth_stencil.stencil_state = Device::DisableStencilState();
						}
					}

					if (overwrites.blend.src_color_blend)
					{
						auto blend_mode = Utility::StringToDeviceBlendMode(WstringToString(*overwrites.blend.src_color_blend));
						if (blend_mode == Device::BlendMode::NumBlendMode)
							throw Resource::Exception(parent->filename) << L"render_state_overwrite.src_color_blend has an invalid value";

						parent->state_overwrites.blend.color_src = blend_mode;
					}
				}

				void SetGraph(const Nodes& nodes, const Links& links) override final
				{
					Memory::VectorMap<unsigned, unsigned, Memory::Tag::EffectGraphNodes> nodes_map;
					parent->nodes.reserve(nodes.size());
					nodes_map.reserve(nodes.size());

					const auto GetStage = [this](const std::string& stage) -> std::optional<Stage>
					{
						const auto res = stage.empty() ? Stage::NumStage : EffectGraphUtil::GetStageFromString(stage);
						if (res == Stage::NumStage || EffectGraphUtil::IsStageEnabled(res, parent->shader_group))
							return res;

						return std::nullopt;
					};

					const auto IsNodeEnabled = [this](NodeType* node) -> bool
					{
						if (node->GetEffectNode().IsInputType() || node->GetEffectNode().IsOutputType())
						{
							const auto current_groups = parent->shader_group;
							const auto node_group = node->GetEffectNode().GetShaderGroup();
							if (auto g = std::get_if<FileReader::FFXReader::ShaderMainGroup>(&node_group); g && *g != current_groups.MainGroup)
								return false;
							else if (auto g = std::get_if<FileReader::FFXReader::ShaderOptGroup>(&node_group); g && !current_groups.test(*g))
								return false;

							if (!EffectGraphUtil::IsStageEnabled(node->GetStage(), node_group))
								return false;
						}

						return true;
					};

					for (const auto& node : nodes)
					{
						if (const auto stage = GetStage(node.stage))
						{
							const auto new_node_index = static_cast<unsigned>(parent->nodes.size());
							parent->nodes.push_back(parent->CreateEffectGraphNode(node.type, node.index, *stage));
							auto& new_node = parent->nodes.back();
							if (!new_node)
							{
								parent->nodes.pop_back();
								continue;
							}
							
							new_node->LoadParameters(node.parameters);
							if (!node.custom_parameter.empty())
							{
								parent->flags |= HasParameters;
								new_node->SetCustomParameter(node.custom_parameter);
							}

							new_node->SetCustomDynamicNames(node.custom_dynamic_names);

							new_node->SetPreferredShader(node.ui_position.y < FileReader::FXGRAPHReader::PIXEL_VERTEX_Y_SPLIT ? Device::ShaderType::VERTEX_SHADER : Device::ShaderType::PIXEL_SHADER);

							new_node->SetParentId(node.parent_id);

							for (const auto& slot : node.input_slots)
							{
								new_node->AddInputSlot(slot.name, slot.type, slot.loop);
							}

							for (const auto& slot : node.output_slots)
							{
								new_node->AddOutputSlot(slot.name, slot.type, slot.loop);
							}

							nodes_map.insert(std::make_pair(new_node->GetHashId(), new_node_index));

							if (!IsNodeEnabled(&*new_node))
								continue;
							
							parent->OnAddNode(new_node_index);
						}
					}

					const auto GetLinkEndPoint = [&](const FXGRAPHReader::LinkEndPoint& pt) -> std::optional<unsigned>
					{
						if (const auto stage = GetStage(pt.stage))
						{
							const auto type_id = EffectGraphUtil::HashString(pt.type);
							auto found = nodes_map.find(EffectGraphNode::Signature(type_id, pt.index, *stage).hash_id);
							if (found == nodes_map.end())
							{
								LOG_WARN(L"Failed establishing links for node " << StringToWstring(pt.type) << L" at index " << pt.index << " in " << parent->filename);
								return std::nullopt;
							}

							return found->second;
						}

						return std::nullopt;
					};

					for (const auto& link : links)
					{
						Memory::Object<NodeType>* src_node = nullptr;
						Memory::Object<NodeType>* dst_node = nullptr;
						if (auto src_node_index = GetLinkEndPoint(link.src))
							src_node = &parent->nodes[*src_node_index];

						if(auto dst_node_index = GetLinkEndPoint(link.dst))
							dst_node = &parent->nodes[*dst_node_index];

						if (!src_node || !dst_node)
							continue;

						if (!IsNodeEnabled(src_node->get()) || !IsNodeEnabled(dst_node->get()))
							continue;
		
						if (link.child_link)
						{
							const auto is_group = (*src_node)->GetEffectNode().IsGroup();
							const auto src_link_index = is_group ? (*src_node)->GetSlotIndexByName(link.src.variable, true) : (*src_node)->GetSlotIndexByName(link.src.variable, false);
							const auto dst_link_index = (*dst_node)->GetSlotIndexByName(link.dst.variable, false);
							if (src_link_index && dst_link_index)
								(*dst_node)->AddChildInput(**src_node, *src_link_index, link.src.swizzle, *dst_link_index, link.dst.swizzle);
						}
						else
						{
							const auto is_internal_group_link = (*src_node)->GetEffectNode().IsGroup() && (*dst_node)->GetParentId() == (*src_node)->GetIndex();
							const auto src_link_index = is_internal_group_link ? (*src_node)->GetSlotIndexByName(link.src.variable, true) : (*src_node)->GetSlotIndexByName(link.src.variable, false);
							const auto dst_link_index = (*dst_node)->GetSlotIndexByName(link.dst.variable, true);
							if (src_link_index && dst_link_index)
								(*dst_node)->AddInput(**src_node, *src_link_index, link.src.swizzle, *dst_link_index, link.dst.swizzle);
						}
					}
				}

				void SetShaderGroups(const FileReader::FFXReader::ShaderGroups& group) override 
				{
					parent->shader_group = group;
				}
			};

			class MatReader : public FileReader::MATReader
			{
			private:
				EffectGraphBase* parent;

			public:
				MatReader(EffectGraphBase* parent) : parent(parent) {}

				void SetDefaultGraph(std::wstring_view graph_name, const rapidjson::GenericValue<rapidjson::UTF16LE<>>& data) override final
				{
					parent->CreateEffectGraph(data);
				}
			};
		};

		COUNTER_ONLY(auto& eg_counter = Counters::Add(L"EffectGraph");)

		template<typename NodeType>
		EffectGraphBase<NodeType>::EffectGraphBase()
		{
			COUNTER_ONLY(Counters::Increment(eg_counter);)
		}

		template<typename NodeType>
		EffectGraphBase<NodeType>::~EffectGraphBase()
		{
			COUNTER_ONLY(Counters::Decrement(eg_counter);)
		}

		template<typename NodeType>
		void EffectGraphBase<NodeType>::CreateEffectGraphFromBuffer(std::wstring& buffer)
		{
			Reader::ReadBuffer(buffer, Reader(this), filename);
			CalculateTypeId();
		}

		template<typename NodeType>
		void EffectGraphBase<NodeType>::CreateEffectGraph(const JsonValue& graph_obj)
		{
			Reader::Read(graph_obj, Reader(this), filename);
			CalculateTypeId();
		}

		template<typename NodeType>
		void EffectGraphBase<NodeType>::CalculateTypeId()
		{
			type_id = 0;

			// lighting model
			if (lighting_model_overriden)
				type_id = EffectGraphUtil::MergeTypeId(type_id, (unsigned)overriden_lighting_model);

			// blend mode
			if (blend_mode_overriden)
				type_id = EffectGraphUtil::MergeTypeId(type_id, (unsigned)overriden_blend_mode);

			// graph property flags
			type_id = EffectGraphUtil::MergeTypeId(type_id, flags);

			// include state overrides
			type_id = EffectGraphUtil::MergeTypeId(type_id, state_overwrites.Hash());

			unsigned num_nodes = 0;
			auto func = [&](const unsigned index, const unsigned group_index)
			{
				const auto& head = GetNode(index);
				if (head.HasNoEffect() || !head.GetEffectNode().IsEnabledByLightingModel(lighting_model_overriden ? overriden_lighting_model : LightingModel::PhongMaterial))
					return;

				head.ProcessChildren([&](const EffectGraphNode& node)
				{
					if (const_cast<EffectGraphNode&>(node).CalculateTypeId())
						++num_nodes;
				});
				type_id = EffectGraphUtil::MergeTypeId(type_id, head.GetTypeId());
			};

			ProcessOutputNodes(func);
			ProcessOutputOnlyNodes(func);

			type_id = EffectGraphUtil::MergeTypeId(type_id, num_nodes);
		}

		template<typename NodeType>
		void EffectGraphBase<NodeType>::ResetGraph()
		{
			// reset flags/properties
			flags = 0;
			lighting_model_overriden = false;
			overriden_lighting_model = LightingModel::PhongMaterial;
			blend_mode_overriden = false;
			overriden_blend_mode = BlendMode::Opaque;
			effect_order = EffectOrder::Default;
			shader_group = FileReader::FFXReader::ShaderMainGroup::Material;
			alpha_ref_overriden = false;
			alpha_ref = simd::vector4(1.0f, 0.5f, 0.001f, 1.0f);

			// reset nodes
			nodes.clear();
			input_nodes.clear();
			output_nodes.clear();
			output_only_nodes.clear();
			custom_macros.Clear();
			dynamic_params.clear();
			custom_dynamic_params.clear();
		}

		template<typename NodeType>
		void EffectGraphBase<NodeType>::OnAddNode(const unsigned node_index, const unsigned group_index /*=0*/)
		{
			const auto& node = nodes[node_index];
			for (auto itr = node->GetEffectNode().GetCustomMacros().GetMacros().begin(); itr != node->GetEffectNode().GetCustomMacros().GetMacros().end(); ++itr)
				custom_macros.Insert(itr->first, itr->second);

			const unsigned stage_number = node->GetStageNumber();
			if (node->GetEffectNode().IsInputType() && node->GetInputLinks().empty())
				input_nodes.insert(std::make_pair(InputOutputKey(group_index, stage_number), node_index));
			else if (node->GetEffectNode().IsOutputType())
				output_nodes.insert(std::make_pair(InputOutputKey(group_index, stage_number), node_index));
			else if (node->GetEffectNode().IsOutputOnly())
				output_only_nodes.insert(std::make_pair(stage_number, node_index));

			if (!node->GetEffectNode().GetDynamicParamInfos().empty())
			{
				const auto dynamic_param_id = node->GetEffectNode().GetDynamicParamInfos().begin()->dynamic_param_id;  // TODO: This should be a loop? 
				auto dynamic_param_info = GetDynamicParameterManager().GetParamInfo(dynamic_param_id);
				auto found = dynamic_params.find(dynamic_param_id);
				if (found == dynamic_params.end())
					dynamic_params.insert({ dynamic_param_id, GraphDynamicParamInfo(dynamic_param_info) });
				else
					++found->second.count;
			}

			auto names_itr = node->GetCustomDynamicNames().begin();
			for(const auto& custom_info : node->GetEffectNode().GetCustomDynamicParamInfos())
			{
				std::string name;
				if (names_itr != node->GetCustomDynamicNames().end())
				{
					name = *names_itr;
					++names_itr;
				}

				if (!name.empty())
				{
					const auto data_id = Device::HashStringExpr(EffectGraphUtil::AddDynamicParameterPrefix(name).c_str());
					auto found = std::find_if(custom_dynamic_params.begin(), custom_dynamic_params.end(), [&](const auto& param)
					{
						return param.id == data_id;
					});
					if (found == custom_dynamic_params.end())
						custom_dynamic_params.push_back(CustomGraphDynamicParamInfo(data_id));
					else
						++found->count;
				}
			}
		}
	}
}