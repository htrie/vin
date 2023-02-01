#include <sstream>

#include "Common/Utility/StringManipulation.h"
#include "Common/File/InputFileStream.h"
#include "Common/Utility/Logger/Logger.h"

#include "Visual/Utility/GraphInstanceCollection.h"
#include "Visual/Renderer/EffectGraphSystem.h"

#include "EffectGraphNodeFactory.h"
#include "EffectGraphParameter.h"
#include "EffectGraph.h"

namespace Renderer
{
    namespace DrawCalls
	{	
		EffectGraphFilenameStringPool filename_string_pool("EffectGraphFilename");

		EffectGraph::EffectGraph(const std::wstring& _filename)
        {
			PROFILE_ONLY(Memory::StackTag stack(Memory::Tag::EffectGraphs));

			filename = _filename;

			try
			{
				if (EndsWith(filename, L".fxgraph"))
				{
					Reader::Read(filename, Reader(this));
				}
				else if (EndsWith(filename, L".matgraph"))
				{
					auto material = filename;
					RemoveSubstring(material, L".matgraph");
					if (File::Exists(material)) // TODO: conversion code, remove once conversion is done.  Only time material would not exist here is when it's a generated material during conversion, in which case main graph would just be blank graph.
						MatReader::Read(material, MatReader(this));

					if (blend_mode_overriden && overriden_blend_mode == BlendMode::Opaque) // Default blend mode is already opaque. // [TODO] Remove directly in the data and remove this hack.
					{
						// Need to avoid the opaque override so we can have blend mode override in engine graphs.
						blend_mode_overriden = false;
					}
				}
				CalculateTypeId();
			}
			catch (const std::exception& e)
			{
				throw Resource::Exception(filename) << e.what();
			}

#if defined(DEVELOPMENT_CONFIGURATION) || defined(TESTING_CONFIGURATION)
			Utility::GraphCollection::OnAddGraph(this);
#endif
		}
        
		EffectGraph::EffectGraph()
        {
#if defined(DEVELOPMENT_CONFIGURATION) || defined(TESTING_CONFIGURATION)
			Utility::GraphCollection::OnAddGraph(this);
#endif
        }

        EffectGraph::~EffectGraph()
        {
#if defined(DEVELOPMENT_CONFIGURATION) || defined(TESTING_CONFIGURATION)
			Utility::GraphCollection::OnRemoveGraph(this);
#endif
		}

		Memory::Object<EffectGraphNode> EffectGraph::CreateEffectGraphNode(const std::string& type_name, const unsigned index, const Stage stage)
		{
			return GetEffectGraphNodeFactory().CreateEffectGraphNode(type_name, index, stage);
		}

		void EffectGraph::MergeEffectGraphInternal(unsigned group_index, const EffectGraphHandle& other, EffectGraphNodeBuckets& sorted_nodes_by_output, uint16_t graph_index)
		{
			PROFILE;

			PROFILE_ONLY(Memory::StackTag stack(Memory::Tag::EffectGraphs));

			std::string error_msg;
			ReplacementsMap replacements;
			replacements.reserve(other->nodes.size());
			Memory::FlatSet<unsigned, Memory::Tag::EffectGraphNodes> processed_nodes;
			processed_nodes.reserve(other->nodes.size());
			Memory::Vector<const EffectGraphNode*, Memory::Tag::EffectGraphNodes> processed_group_nodes;

			type_id = EffectGraphUtil::MergeTypeId(type_id, other->type_id);

			CopyFlags(other);
     
			ConstEffectGraphNodes prev_heads;
			prev_heads.reserve(output_nodes.size());
            for (auto itr = output_nodes.begin(); itr != output_nodes.end(); ++itr)
				prev_heads.insert(std::make_pair(itr->first, itr->second));
            
			const auto func_head = [&](const unsigned index, const unsigned temp)
            {
				const auto& head = other->GetNode(index);
				if (head.HasNoEffect() || !head.GetEffectNode().IsEnabledByLightingModel(overriden_lighting_model))
                    return;
                
				const auto output_stage = head.GetStageNumber();
				unsigned parent_group_id = -1;

				const auto func_node = [&](const EffectGraphNode& node)
				{
					if (CopyNode(node, head, group_index, graph_index, parent_group_id, prev_heads, replacements))
					{
						const auto new_node_index = static_cast<unsigned>(nodes.size()) - 1;
						sorted_nodes_by_output[InputOutputKey(group_index, output_stage)].push_back(new_node_index);

						if (node.GetEffectNode().IsGroup())
							processed_group_nodes.push_back(&node);
					}
				};

				head.ProcessChildren(func_node, processed_nodes);

				for (const auto& group_node : processed_group_nodes)
				{
					auto& new_group_node = FindReplacement(*group_node, replacements);
					
					parent_group_id = new_group_node->GetIndex();
					for (auto& link : group_node->GetChildLinks())
						link.node->ProcessChildren(func_node, processed_nodes);
					
					CopyInputs(new_group_node, *group_node, replacements, true);
				}
            };
            
            other->ProcessOutputNodes(func_head);
            other->ProcessOutputOnlyNodes(func_head);
            
			const auto filename = WstringToString(other->filename);
			filenames.emplace_back(std::pair(filename_string_pool.grab(filename.data(), (unsigned)filename.size()), group_index));
        }
     
		Memory::Object<EffectGraphNode>& EffectGraph::FindReplacement(const EffectGraphNode& src, const ReplacementsMap& replacements)
		{
			const auto instance_id = src.GetHashId();
			const auto found = replacements.find(instance_id);
			assert(found != replacements.end());
			return nodes[found->second];
		}

		void EffectGraph::CopyFlags(const EffectGraphHandle& other)
		{
			flags = flags | other->flags;
			state_overwrites.Merge(other->state_overwrites);

			if (other->IsBlendModeOverriden())
			{
				overriden_blend_mode = other->GetOverridenBlendMode();
				blend_mode_overriden = true;
			}

			for (auto itr = other->GetCustomMacros().GetMacros().begin(); itr != other->GetCustomMacros().GetMacros().end(); ++itr)
				custom_macros.Insert(itr->first, itr->second);
		}

		void EffectGraph::CopyInputs(Memory::Object<EffectGraphNode>& dest, const EffectGraphNode& src, const ReplacementsMap& replacements, const bool child_input)
		{
			const auto& links = child_input ? src.GetChildLinks() : src.GetInputLinks();
			for (auto itr = links.begin(); itr != links.end(); ++itr)
			{
				const auto& input_node = FindReplacement(*itr->node, replacements);
				const auto output_index = itr->output_info.index;
				const auto output_mask = itr->output_info.Mask();
				const auto input_index = itr->input_info.index;
				const auto input_mask = itr->input_info.Mask();
				if (child_input)
					dest->AddChildInput(*input_node, output_index, output_mask, input_index, input_mask);
				else
					dest->AddInput(*input_node, output_index, output_mask, input_index, input_mask);
			}
		}

		bool EffectGraph::CopyNode(const EffectGraphNode& src, const EffectGraphNode& head, const unsigned group, const uint16_t graph_index, const unsigned parent_id, ConstEffectGraphNodes prev_heads, ReplacementsMap& replacements)
		{
			const auto instance_id = src.GetHashId();
			const auto& static_ref = src.GetEffectNode();
			Memory::Object<EffectGraphNode>* relink_output = nullptr;

			if (replacements.find(instance_id) != replacements.end())
				return false;

			auto group_index = EffectGraphUtil::GetGroupIndexByStage(group, head.GetStage());

			if (static_ref.IsInputType())
			{
				// Retain the input node's group index in case it's parent group index is in a different stage.
				group_index = EffectGraphUtil::GetGroupIndexByStage(group, src.GetStage());

				// Check if we need to setup for relink from previous output. Otherwise, check if the input node needs 
				// to be replaced by a previous input node of the same type/stage which should happen only when there's 
				// no previous output of the same type/stage.
				if (auto prev_head = prev_heads.find(InputOutputKey(group_index, src.GetStageNumber())); prev_head != prev_heads.end()) // Relink node from previous output
				{
					relink_output = &nodes[prev_head->second];
				}
				else if (auto prev_input = input_nodes.find(InputOutputKey(group_index, src.GetStageNumber())); prev_input != input_nodes.end()) // Input node already exist
				{
					replacements.insert(std::make_pair(instance_id, prev_input->second));
					return false;
				}
			}

			nodes.emplace_back(GetEffectGraphNodeFactory().CreateEffectGraphNode(static_ref.GetTypeId(), 0, src.GetStage()));
			auto& new_node = nodes.back();
			auto new_node_index = static_cast<unsigned>(nodes.size() - 1);
			new_node->SetGroupIndex(group_index);
			new_node->SetPreferredShader(src.GetPreferredShader());
			new_node->SetGraphIndex(graph_index);
			new_node->SetParentId(parent_id);
			new_node->SetCustomParameter(src.GetCustomParameter());
			replacements.insert(std::make_pair(instance_id, new_node_index));

			CopyInputs(new_node, src, replacements, false);
			if (relink_output)
				new_node->AddInput(**relink_output, 0, std::string(), 0, std::string());

			// Copy parameters as it's needed for inlining uniforms during shader generation
			auto curr_param = new_node->begin();
			for (auto param = src.begin(); param != src.end(); ++param, ++curr_param)
				(*curr_param)->Copy(*(*param));

			// Transfer the custom dynamic name values over as it's needed for shader generation
			new_node->SetCustomDynamicNames(src.GetCustomDynamicNames());

			// Transfer the dynamic input/output slots over for group nodes as it's needed for shader generation
			if (src.GetEffectNode().IsGroup())
			{
				for (const auto& slot : src.GetInputSlots())
					new_node->AddInputSlot(slot.Name(), slot.type, slot.loop);
				for (const auto& slot : src.GetOutputSlots())
					new_node->AddOutputSlot(slot.Name(), slot.type, slot.loop);
			}

			OnAddNode(new_node_index, new_node->GetGroupIndex());
			return true;
		}

		void EffectGraph::SortNodeIndicesByStage(const EffectGraphNodeBuckets& sorted_nodes_by_output)
        {
			PROFILE;

			// Reindex the nodes with the indices according to the order of the output slots they are connected to
			Memory::FlatMap<unsigned, unsigned, Memory::Tag::EffectGraphNodes> indices_by_nodetype;
			Memory::FlatMap<unsigned, unsigned, Memory::Tag::EffectGraphNodes> indices_by_paramtype;
            
			auto get_index = [&](Memory::FlatMap<unsigned, unsigned, Memory::Tag::EffectGraphNodes>& input_map, unsigned type_id) -> unsigned
            {
                unsigned index;
                auto found_index = input_map.find(type_id);
                if (found_index == input_map.end())
                {
                    input_map[type_id] = 0;
                    index = 0;
                }
                else
                {
                    ++found_index->second;
                    index = found_index->second;
                }
                return index;
            };
            
            for (auto itr = sorted_nodes_by_output.begin(); itr != sorted_nodes_by_output.end(); ++itr)
            {
				for (auto& node_index : itr->second)
                {
					auto& node = nodes[node_index];
					unsigned index = get_index(indices_by_nodetype, node->GetEffectNode().GetTypeId());
					node->SetSignature(index, node->GetStage());
                    
					for (const auto& param : *node)
                    {
						//if (is_uniform)
						{
							unsigned index = get_index(indices_by_paramtype, param->GetDataId());
							param->SetIndex(index);
						}
						/*else
							(*param_info)->SetIndex(-1);*/
                    }
                }
            }
        }
        
		void EffectGraph::ConnectStageInputs()
		{
			PROFILE;

			for (auto& node : nodes)
			{
				const auto& static_ref = node->GetEffectNode();
				for (size_t a = 0; a < static_ref.GetStageConnectors().size(); a++)
				{
					const auto& con = static_ref.GetStageConnectors()[a];
					Memory::Object<EffectGraphNode>* target = nullptr;
					unsigned target_stage = 0;
					for (const auto& input : input_nodes)
					{
						if (!nodes[input.second]->GetEffectNode().IsInputType())
							continue;

						if (&node == &nodes[input.second])
							continue;

						if (input.first.group != 0 && input.first.group != node->GetGroupIndex())
							continue;

						const auto stage_number = nodes[input.second]->GetStageNumber();
						if (target_stage > stage_number)
							continue;

						if (target_stage == stage_number && target && input.first.group != node->GetGroupIndex())
							continue;

						if (nodes[input.second]->GetStage() <= con.stage && nodes[input.second]->GetEffectNode().GetName() == con.ExtensionPoint())
						{
							target = &nodes[input.second];
							target_stage = stage_number;
						}
					}

					for (const auto& output : output_nodes)
					{
						if (!nodes[output.second]->GetEffectNode().IsOutputType())
							continue;

						if (&node == &nodes[output.second])
							continue;

						if (GetEffectGraphNodeFactory().Find(nodes[output.second]->GetEffectNode().GetMatchingNodeType()).GetName() != con.ExtensionPoint())
							continue;

						const auto stage_number = nodes[output.second]->GetStageNumber();
						if (target_stage > stage_number)
							continue;

						if (target_stage == stage_number && target && output.first.group != node->GetGroupIndex())
							continue;

						if (nodes[output.second]->GetStage() <= con.stage)
						{
							target = &nodes[output.second];
							target_stage = stage_number;
						}
					}

					if (target)
					{
						if ((*target)->GetEffectNode().IsInputType())
						{
							node->AddStageInput(*(*target), 0, "", unsigned(a), "");
						}
						else if((*target)->GetEffectNode().IsOutputType())
						{
							for (const auto& b : (*target)->GetInputLinks())
								node->AddStageInput(*b.node, b.output_info.index, b.output_info.Mask(), unsigned(a), b.input_info.Mask());
						}
					}
				}
			}
		}

		void EffectGraph::MergeEffectGraph(const EffectGraphGroups& groups)
		{
			PROFILE;

			Memory::FlatMap<size_t, EffectGraphHandle, Memory::Tag::EffectGraphs> graphs;
			for (const auto& it : groups)
			{
				graphs[it.second.hash()] = ::EffectGraph::System::Get().FindGraph(it.second.to_view());
			}

			shader_group.reset();
			for (const auto& it : groups)
			{
				shader_group |= graphs[it.second.hash()]->GetShaderGroups();
			}

			// get the final lighting model to be used
			for (auto& it : groups)
			{
				auto& graph = graphs[it.second.hash()];
				if (graph->IsLightingModelOverriden())
				{
					lighting_model_overriden = true;
					overriden_lighting_model = graph->GetOverridenLightingModel();
				}
            }
            
			EffectGraphNodeBuckets sorted_nodes_by_output;

			// reserve number of nodes
			size_t total_nodes = 0;
			for (auto& it : groups)
			{
				auto& graph = graphs[it.second.hash()];
				total_nodes += static_cast<unsigned>(graph->nodes.size());
			}
			nodes.reserve(total_nodes);

			// merge the graph from the instances
			uint16_t graph_index = 0;
			for (auto& it : groups)
			{
				unsigned group_index = it.first;
				auto& graph = graphs[it.second.hash()];
				MergeEffectGraphInternal(group_index, graph, sorted_nodes_by_output, graph_index++);
			}
            
			// Connect stage inputs after all graphs have been merged in
			ConnectStageInputs();

            // rename the nodes with the indices according to the order of the output slots they are connected to
			SortNodeIndicesByStage(sorted_nodes_by_output);
            
            // finalize the type id and multi-stage flag based from the graph setup
            CalculateTypeIdAndMultiStageFlag();
        }
        
		void EffectGraph::GetFinalShaderFragments(EffectGraphNodeBuckets& shader_fragments, EffectGraphMacros& macros, bool is_fullbright) const
        {
            //Get the factors that change the final fragments
            bool is_lighting_disabled = IsLightingDisabled() || is_fullbright;
            
            //Iterate thru the stage nodes in the combined graph
            {
                //Setup the lighting model macros
                std::string lighting_model_macro;
                if (IsLightingModelOverriden())
                    lighting_model_macro = LightingModelMacros[(unsigned)GetOverridenLightingModel()];
                if (!lighting_model_macro.empty())
				{
					macros.Add(lighting_model_macro, "");
				}
                
                //Add the macros from the combined graph
				for (auto macro = GetCustomMacros().GetMacros().begin(); macro != GetCustomMacros().GetMacros().end(); ++macro)
					macros.Insert(macro->first, macro->second);
                
                //Add the has lighting macro
                if (!is_lighting_disabled)
					macros.Add("HAS_LIGHTING", "");
                
                //Finally, setup the lighting and shadow fragments. Sort the stage nodes according the group index and stages. Input nodes should always go before the output nodes in the same group and stage.
				auto func = [&](const unsigned stage_node_index, const unsigned group_index)
                {
					const auto& stage_node = nodes[stage_node_index];
					Stage stage = stage_node->GetStage();
                    
					if (stage_node->GetEffectNode().IsInputType())
						shader_fragments[InputOutputKey(group_index, (unsigned)stage)].push_back(stage_node_index);
                    else
                    {
                        bool skip = is_lighting_disabled && stage >= Stage::Lighting && stage <= Stage::LightingEnd_Final;
                        if (!skip)
							shader_fragments[InputOutputKey(group_index, (unsigned)stage)].push_back(stage_node_index);
                    }
                };

                ProcessInputNodes(func);
                ProcessOutputNodes(func);
				ProcessOutputOnlyNodes([&](const unsigned node_index, const unsigned group_index)
				{
					const auto& node = nodes[node_index];
					switch (node->GetPreferredShader())
					{
						case Device::ShaderType::COMPUTE_SHADER: 
							shader_fragments[InputOutputKey(group_index, unsigned(Stage::COMPUTE_STAGE_END) - 1)].push_back(node_index);
							break;
						case Device::ShaderType::VERTEX_SHADER:
							shader_fragments[InputOutputKey(group_index, unsigned(Stage::VERTEX_STAGE_END) - 1)].push_back(node_index);
							break;
						case Device::ShaderType::PIXEL_SHADER:
							shader_fragments[InputOutputKey(group_index, unsigned(Stage::PIXEL_STAGE_END) - 1)].push_back(node_index);
							break;
						default:
							shader_fragments[InputOutputKey(group_index, unsigned(Stage::NumStage))].push_back(node_index);
							break;
					}
				});
            }
        }
        
		std::string EffectGraph::GetName() const
		{
			std::stringstream res_name;
			for (const auto& it : GetFilenames())
			{
				const auto filename_handle = it.first;
				const auto filename = std::string(filename_handle.data(), filename_handle.size());
				auto start = filename.find_last_of("\\/") + 1;
				auto length = filename.rfind('.') - start;
				auto name = filename.substr(start, length);
				if (EndsWith(name, ".mat"))
					name = filename.substr(start, name.rfind('.'));
				std::replace(name.begin(), name.end(), ' ', '_');
				res_name << name << "_";
			}
			return res_name.str();
		}

		void EffectGraph::CalculateTypeIdAndMultiStageFlag()
		{
			PROFILE;

			type_id = 0;

			// lighting model
			if(lighting_model_overriden)
				type_id = EffectGraphUtil::MergeTypeId(type_id, (unsigned)overriden_lighting_model);

			// blend mode
			if (blend_mode_overriden)
				type_id = EffectGraphUtil::MergeTypeId(type_id, (unsigned)overriden_blend_mode);

            // graph property flags
            type_id = EffectGraphUtil::MergeTypeId(type_id, flags);
            
			// include state overrides
			type_id = EffectGraphUtil::MergeTypeId(type_id, state_overwrites.Hash());

			Memory::VectorMap<unsigned, unsigned, Memory::Tag::EffectGraphNodes> processed_nodes;
			processed_nodes.reserve(nodes.size());
			auto func = [&](const unsigned index, const unsigned group_index)
            {
				const auto& head = GetNode(index);
				const unsigned head_stage = head.GetStageNumber();
				head.ProcessChildren([&](const EffectGraphNode& node)
					{
						const_cast<EffectGraphNode&>(node).CalculateTypeId(true);
                                          
						// Determine if a node is a multi-stage node i.e. a node that is being used on different output stages.  
						// This flag is only used during shader text generation in the fragment linker.
						auto processed_node = processed_nodes.find(node.GetHashId());
						if (processed_node != processed_nodes.end())
						{
							if (node.IsMultiInput() && processed_node->second != head_stage)
								const_cast<EffectGraphNode&>(node).SetMultiStage(true);
						}
						else
							processed_nodes[node.GetHashId()] = head_stage;
					});
				type_id = EffectGraphUtil::MergeTypeId(type_id, head.GetTypeId());
			};

			ProcessOutputNodes(func);
			ProcessOutputOnlyNodes(func);
		}

		// NODEGRAPH_TODO: only used for trails (remove and do this the proper way later)
		bool EffectGraph::HasTbnNormals() const
		{
			unsigned group = 0;
			unsigned stage_number = (unsigned)Stage::Texturing_Init * GetEffectGraphNodeFactory().GetNumOutputTypes() + GetEffectGraphNodeFactory().GetTbnNormalIndex();
			InputOutputKey key(group, stage_number);		
			auto found = output_nodes.find(key);
			return (found != output_nodes.end());
		}	
    }
}
