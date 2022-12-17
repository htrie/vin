#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <rapidjson/error/en.h>

#include "Common/File/InputFileStream.h"
#include "Common/Utility/StringManipulation.h"
#include "Common/FileReader/GraphInstanceParser.h"

#include "Visual/Utility/GraphInstanceCollection.h"
#include "Visual/Utility/TextureMetadata.h" // For conversion purposes only
#include "Visual/Renderer/EffectGraphSystem.h"

#include "EffectGraphNodeFactory.h"
#include "EffectGraphInstance.h"


namespace Renderer
{
	namespace DrawCalls
	{

		void GetParentCustomParameters(CustomParameterMap& custom_parameter_map, const EffectGraphHandle& graph)
		{
			auto func = [&](const unsigned index, const unsigned group_index)
			{
				const auto& output_node = graph->GetNode(index);
				output_node.ProcessChildren([&](const EffectGraphNode* node)
					{
						const auto& custom_parameter = node->GetCustomParameter();
						if (custom_parameter && custom_parameter_map.find(custom_parameter) == custom_parameter_map.end())
							custom_parameter_map.insert(std::make_pair(custom_parameter, node));
					});
			};
			custom_parameter_map.clear();
			graph->ProcessOutputNodes(func);
			graph->ProcessOutputOnlyNodes(func);
		}


		InstanceParameter::InstanceParameter(const EffectGraphNodeStatic& base_node)
		{
			PROFILE_ONLY(Memory::StackTag stack(Memory::Tag::EffectGraphParameters));
			parameters.reserve(base_node.GetParamInfos().size());
			for (auto itr = base_node.GetParamInfos().begin(); itr != base_node.GetParamInfos().end(); ++itr)
			{
				auto& param_static = *itr;
				parameters.emplace_back(::EffectGraph::System::Get().CreateParameter(param_static));
			}
		}

		void InstanceParameter::LoadParameter(const JsonValue& node_obj)
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

		void InstanceParameter::SaveParameter(JsonValue& node_obj, JsonAllocator& allocator) const
		{
			JsonValue param_array(rapidjson::kArrayType);
			for (auto& param_info : *this)
			{
				JsonValue param_obj(rapidjson::kObjectType);
				param_info->SaveData(param_obj, allocator);
				param_array.PushBack(param_obj, allocator);
			}

			if (!param_array.Empty())
				node_obj.AddMember(L"parameters", param_array, allocator);
		}

		class InstanceDesc::Parser : public FileReader::GraphInstanceParser
		{
		private:
			InstanceDesc* parent;

		public:
			Parser(InstanceDesc* parent) : parent(parent) {}

			bool SetData(const Data& data) override final
			{
				if(!data.parent_filename.empty())
					parent->graph_filename = data.parent_filename;

				if (data.alpha_ref)
				{
					parent->alpha_ref_overriden = true;
					parent->alpha_ref = simd::vector4(1.0f, *data.alpha_ref, 0.001f, 1.0f);
				}

				CustomParameterMap custom_parameter_map;
				const auto graph = ::EffectGraph::System::Get().FindGraph(parent->GetGraphFilename());
				GetParentCustomParameters(custom_parameter_map, graph);

				bool has_deprecated_parameter = false;
				parent->parameters.reserve(data.custom_parameters.size());

				for (auto& custom_params : data.custom_parameters)
				{
					const auto custom_param_name = EffectGraphUtil::HashString(WstringToString(custom_params.name));
					if (auto found_node = custom_parameter_map.find(custom_param_name); found_node != custom_parameter_map.end())
					{
						parent->parameters.insert(std::make_pair(custom_param_name, InstanceParameter(found_node->second->GetEffectNode())));
						parent->parameters[custom_param_name].LoadParameter(custom_params.data);
					}
					else
						has_deprecated_parameter = true;
				}

				return has_deprecated_parameter;
			}
		};


		InstanceDesc::InstanceDesc(std::wstring_view graph_filename)
			: graph_filename(graph_filename)
			//: tweak_id(++monotonic_id) // NOT HERE (no inputs to distinguish).
		{
		}

		InstanceDesc::InstanceDesc(std::wstring_view parent_filename, std::wstring_view buffer)
			: tweak_id(++monotonic_id)
		{
			PROFILE_ONLY(Memory::StackTag stack(Memory::Tag::EffectGraphInstances));
			Parser::Parse(buffer, Parser(this), parent_filename);
		}

		InstanceDesc::InstanceDesc(std::wstring_view parent_filename, rapidjson::GenericValue<rapidjson::UTF16LE<>>& instance_obj)
			: tweak_id(++monotonic_id)
		{
			PROFILE_ONLY(Memory::StackTag stack(Memory::Tag::EffectGraphInstances));
			Parser::Parse(instance_obj, Parser(this), parent_filename);
		}

		void InstanceDesc::SaveToStream(std::wostream& stream) const
		{
			using namespace rapidjson;
			JsonDocument doc;
			doc.SetObject();
			auto& allocator = doc.GetAllocator();

			Save(doc, allocator);

			GenericStringBuffer<JsonEncoding> buffer;
			Writer<GenericStringBuffer<JsonEncoding>, JsonEncoding, JsonEncoding> writer(buffer);
			doc.Accept(writer);
			stream << buffer.GetString();
		}

		void InstanceDesc::Save(JsonValue& parent_obj, JsonAllocator& allocator) const
		{
#if defined(DEVELOPMENT_CONFIGURATION) || defined(TESTING_CONFIGURATION)
			Utility::GraphInstanceCollection::OnSaveGraphInstance(this, parent_obj, allocator);
#endif
		}


		unsigned GetGraphId(std::wstring_view graph_filename)
		{
			return MurmurHash2(graph_filename.data(), int(graph_filename.length() * sizeof(wchar_t)), 0x34322);
		}

		unsigned GetInstanceId(std::wstring_view graph_filename, unsigned tweak_id)
		{
			using namespace Renderer::DrawCalls::EffectGraphUtil;
			unsigned result = 0;
			result = MergeTypeId(result, GetGraphId(graph_filename));
			result = MergeTypeId(result, tweak_id);
			return result;
		}

		unsigned GetUniformHash(std::wstring_view graph_filename, unsigned tweak_id, unsigned drawdata_id, unsigned drawdata_index)
		{
			using namespace Renderer::DrawCalls::EffectGraphUtil;
			unsigned result = 0;
			result = GetInstanceId(graph_filename, tweak_id);
			result = MergeTypeId(result, drawdata_id);
			result = MergeTypeId(result, drawdata_index);
			return result;
		}


		uint64_t BuildUniformKey(DrawDataNames::DrawDataIdentifiers id, uint8_t index)
		{
			return Device::IdHash(id, index);
		}

		DrawCalls::Uniform BuildUniform(const Device::UniformInput& uniform_input)
		{
			switch (uniform_input.type)
			{
			case Device::UniformInput::Type::Bool: return *(bool*)uniform_input.value;
			case Device::UniformInput::Type::Int: return *(int*)uniform_input.value;
			case Device::UniformInput::Type::UInt: return *(unsigned*)uniform_input.value;
			case Device::UniformInput::Type::Float: return *(float*)uniform_input.value;
			case Device::UniformInput::Type::Vector2: return *(simd::vector2*)uniform_input.value;
			case Device::UniformInput::Type::Vector3: return *(simd::vector3*)uniform_input.value;
			case Device::UniformInput::Type::Vector4: return *(simd::vector4*)uniform_input.value;
			case Device::UniformInput::Type::Matrix: return *(simd::matrix*)uniform_input.value;
			case Device::UniformInput::Type::Spline5: return *(simd::Spline5*)uniform_input.value;
			case Device::UniformInput::Type::None:
			default: throw std::runtime_error("Invalid uniform input type");
			}
		}


		GraphDesc::GraphDesc(const wchar_t* graph_filename)
			: graph_filename(graph_filename)
		{}

		GraphDesc::GraphDesc(std::wstring_view graph_filename)
			: graph_filename(graph_filename)
		{}

		GraphDesc::GraphDesc(const std::shared_ptr<InstanceDesc>& instance_desc)
			: graph_filename(instance_desc->graph_filename)
			, instance_desc(instance_desc)
		{}


		class EffectGraphInstance
		{
		public:
			EffectGraphInstance() {}
			EffectGraphInstance(const GraphDesc& graph_desc);

			const EffectGraphUniformInputs* GetUniformInputsByOutput(unsigned output_type) const;
			const BindingInputInfos* GetBindingInputsByOutput(unsigned output_type) const;

		protected:
			Memory::FlatMap<unsigned, EffectGraphUniformInputs, Memory::Tag::EffectGraphInstanceInfos> uniform_inputs;
			Memory::FlatMap<unsigned, BindingInputInfos, Memory::Tag::EffectGraphInstanceInfos> binding_inputs;
		};

		EffectGraphInstance::EffectGraphInstance(const GraphDesc& graph_desc)
		{
			PROFILE_ONLY(Memory::StackTag stack(Memory::Tag::EffectGraphInstances));

			const auto graph = ::EffectGraph::System::Get().FindGraph(graph_desc.GetGraphFilename());

			Memory::VectorSet<const EffectGraphNode*, Memory::Tag::EffectGraphNodes> processed_nodes;
			auto func = [&](const unsigned index, const unsigned group_index)
			{
				const auto& head = graph->GetNode(index);
				const unsigned stage = head.GetStageNumber();

				head.ProcessChildren([&](const EffectGraphNode* node)
				{
					if(processed_nodes.find(node) != processed_nodes.end())
						return;

					bool use_default = true;
					const auto& custom_parameter = node->GetCustomParameter();
					if(graph_desc.instance_desc && !graph_desc.instance_desc->parameters.empty() && custom_parameter)
					{
						auto found = graph_desc.instance_desc->parameters.find(custom_parameter);
						if(found != graph_desc.instance_desc->parameters.end())
						{
							auto& params = found->second;
							for(const auto& param : params)
							{
								if (auto uniform_input_info = param->GetUniformInputInfo(); uniform_input_info.id != DrawDataNames::DrawDataIdentifiers::None)
								{
									const auto uniform_hash = GetUniformHash(graph_desc.GetGraphFilename(), graph_desc.GetTweakId(), uniform_input_info.id, node->GetIndex());
									uniform_inputs[stage][uniform_input_info.id].push_back({ uniform_input_info.input.SetHash(uniform_hash), uniform_input_info.id, uniform_input_info.index });
								}
								if (auto binding_input_info = param->GetBindingInputInfo(); binding_input_info.id != DrawDataNames::DrawDataIdentifiers::None)
								{
									binding_inputs[stage].push_back(binding_input_info);
								}
							}
							use_default = false;
						}
					}
					
					if(use_default)
					{
						for(const auto& param : *node)
						{
							if (auto uniform_input_info = param->GetUniformInputInfo(); uniform_input_info.id != DrawDataNames::DrawDataIdentifiers::None)
							{
								const auto uniform_hash = GetUniformHash(graph_desc.GetGraphFilename(), graph_desc.GetTweakId(), uniform_input_info.id, node->GetIndex());
								uniform_inputs[stage][uniform_input_info.id].push_back({ uniform_input_info.input.SetHash(uniform_hash), uniform_input_info.id, uniform_input_info.index });
							}
							if (auto binding_input_info = param->GetBindingInputInfo(); binding_input_info.id != DrawDataNames::DrawDataIdentifiers::None)
							{
								binding_inputs[stage].push_back(binding_input_info);
							}
						}
					}

					processed_nodes.insert(node);
				});
			};

			// if alphatest threshold is changed, bind it as well (Note: we just bind it on stage index 0)
			if (graph->IsAlphaRefOverriden())
			{
				auto uniform_input_info = graph->GetAlphaRefUniformInput();
				if (graph_desc.instance_desc && graph_desc.instance_desc->alpha_ref_overriden)
					uniform_input_info = { ::Device::UniformInput(0).SetVector(&graph_desc.instance_desc->alpha_ref), DrawDataNames::AlphaTesting, 0 };
					
				const auto uniform_hash = GetUniformHash(graph_desc.GetGraphFilename(), 0, uniform_input_info.id, 0);
				uniform_inputs[0][uniform_input_info.id].push_back({ uniform_input_info.input.SetHash(uniform_hash), uniform_input_info.id, uniform_input_info.index });
				const auto input_hash = MurmurHash2(uniform_input_info.input.value, static_cast<int>(uniform_input_info.input.GetSize()), 0x34322);
			}

			// bind drawdata from output nodes
			graph->ProcessOutputNodes(func);

			// bind drawdata from vertex output only nodes
			graph->ProcessOutputOnlyNodes(func);
		}

		const EffectGraphUniformInputs* EffectGraphInstance::GetUniformInputsByOutput(unsigned output_type) const
		{
			auto found = uniform_inputs.find((unsigned)output_type);
			if(found != uniform_inputs.end())
				return &found->second;
			return nullptr;
		}

		const BindingInputInfos* EffectGraphInstance::GetBindingInputsByOutput(unsigned output_type) const
		{
			auto found = binding_inputs.find((unsigned)output_type);
			if (found != binding_inputs.end())
				return &found->second;
			return nullptr;
		}


		struct TempInputs
		{
			Memory::Vector<EffectGraphInstance, Memory::Tag::DrawCalls> instances;

			Memory::Map<InputOutputKey, Memory::SmallVector<const EffectGraphUniformInputs*, 16, Memory::Tag::DrawCalls>, Memory::Tag::DrawCalls> uniforms;
			Memory::Map<InputOutputKey, Memory::SmallVector<const BindingInputInfos*, 16, Memory::Tag::DrawCalls>, Memory::Tag::DrawCalls> bindings;
		};

		TempInputs SetupInputs(const EffectGraphDescs& graph_descs, EffectOrder::Value& effect_order)
		{
			PROFILE;

			TempInputs result;

			LightingModel final_lighting_model = LightingModel::PhongMaterial;
			for (auto& it : graph_descs)
			{
				const auto graph = ::EffectGraph::System::Get().FindGraph(it.second.GetGraphFilename());

				if (graph->IsLightingModelOverriden())
					final_lighting_model = graph->GetOverridenLightingModel();

				auto effect_graph_order = graph->GetEffectOrder();
				if (effect_graph_order != Renderer::DrawCalls::EffectOrder::Default)
					effect_order = effect_graph_order;
			}

			auto& all_uniform_inputs = result.uniforms;
			auto& all_binding_inputs = result.bindings;

			for (auto& it : graph_descs)
			{
				unsigned group_index = it.first;

				const auto graph = ::EffectGraph::System::Get().FindGraph(it.second.GetGraphFilename());

				result.instances.emplace_back(it.second);
				const auto& instance = result.instances.back();

				// Bind the graph's alphatest threshold if it has a value. If the next graph also overrides the alphatest threshold then just use that value. Basically use the value that was applied last.
				if (auto* uniform_inputs = instance.GetUniformInputsByOutput(0))
				{
					if (all_uniform_inputs.find(InputOutputKey(0, 0)) == all_uniform_inputs.end())
						all_uniform_inputs[InputOutputKey(0, 0)].push_back(uniform_inputs);
					else
						all_uniform_inputs[InputOutputKey(0, 0)][0] = uniform_inputs;
				}

				graph->ProcessOutputNodes([&](const unsigned index, const unsigned group)
					{
						const auto& output_node = graph->GetNode(index);
						if (output_node.HasNoEffect() || !output_node.GetEffectNode().IsEnabledByLightingModel(final_lighting_model))
							return;

						group_index = EffectGraphUtil::GetGroupIndexByStage(group_index, output_node.GetStage());

						const unsigned output_stage = output_node.GetStageNumber();
						if (auto* draw_data_list = instance.GetUniformInputsByOutput(output_stage))
							all_uniform_inputs[InputOutputKey(group_index, output_stage)].push_back(draw_data_list);
						if (auto* binding_inputs = instance.GetBindingInputsByOutput(output_stage))
							all_binding_inputs[InputOutputKey(group_index, output_stage)].push_back(binding_inputs);
					});

				graph->ProcessOutputOnlyNodes([&](const unsigned index, const unsigned group)
					{
						const auto& output_node = graph->GetNode(index);
						const unsigned output_stage = output_node.GetStageNumber();
						if (auto* draw_data_list = instance.GetUniformInputsByOutput(output_stage))
							all_uniform_inputs[InputOutputKey(0, output_stage)].push_back(draw_data_list);
						if (auto* binding_inputs = instance.GetBindingInputsByOutput(output_stage))
							all_binding_inputs[InputOutputKey(0, output_stage)].push_back(binding_inputs);
					});
			}

			return result;
		}

		void ArrangeInputs(const TempInputs& temp_inputs, UniformInputInfos& out_uniform_inputs, BindingInputInfos& out_binding_inputs)
		{
			struct ZeroedIndex
			{
				unsigned index = 0;
			};
			Memory::Map<DrawDataNames::DrawDataIdentifiers, ZeroedIndex, Memory::Tag::DrawCalls> indices;

			for (auto map_itr : temp_inputs.uniforms)
			{
				for (auto* vec_itr : map_itr.second)
				{
					for (auto itr : *vec_itr)
					{
						for (auto& uniform_input : itr.second)
						{
							out_uniform_inputs.push_back(uniform_input);
							out_uniform_inputs.back().index = indices[uniform_input.id].index++;
						}
					}
				}
			}

			for (auto vec_iter : temp_inputs.bindings)
			{
				for (auto iter : vec_iter.second)
				{
					for (auto& texture_input : *iter)
					{
						out_binding_inputs.push_back(texture_input);
						out_binding_inputs.back().index = indices[texture_input.id].index++;
					}
				}
			}
		}

		Inputs GatherInputs(const EffectGraphDescs& graph_descs)
		{
			Inputs inputs;
			ArrangeInputs(SetupInputs(graph_descs, inputs.effect_order), inputs.uniform_inputs_infos, inputs.binding_inputs_infos);
			return inputs;
		}

	}
}