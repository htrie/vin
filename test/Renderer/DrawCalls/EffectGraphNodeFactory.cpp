#include "Common/File/InputFileStream.h"
#include "Common/Utility/ContainerOperations.h"
#include "Common/Utility/Counters.h"
#include "Common/FileReader/FFXReader.h"

#include "Visual/Renderer/FragmentLinker.h"
#include "Visual/Renderer/DynamicParameterManager.h"
#include "Visual/Renderer/DrawCalls/DrawDataNames.h"
#include "Visual/Renderer/EffectGraphSystem.h"
#include "Visual/Device/Shader.h"

#include "EffectGraphNodeFactory.h"

namespace Renderer
{
	namespace DrawCalls
	{
		namespace
		{
			EffectGraphNodeStatic null_static_node;
			Memory::Vector<unsigned, Memory::Tag::EffectGraphNodes> output_types;

			uint32_t GetDrawDataIdentifierFromName(const std::string& name, bool dynamic = false)
			{
				//  we store names with "__" as all graph uniforms use "__(index)" post string
				std::string final_name = name;
				if (!dynamic)
					final_name += "__";
				return Device::HashStringExpr(final_name.c_str());
			}

			class Reader : public FileReader::FFXReader
			{
			private:
				EffectGraphNodeFactory::GraphNodeMap& static_graph_nodes;
				EffectGraphNodeFactory::DefaultNodes& default_graph_nodes;

				template<typename T>
				static bool ExtractUniformValues(std::stringstream& stream, Memory::Vector<T, Memory::Tag::EffectGraphParameterInfos>& in_values)
				{
					std::stringstream temp_stream;
					std::string params;
					if (ExtractString(stream, params))
					{
						temp_stream.str(params);
						T val;
						while (temp_stream >> val)
							in_values.push_back(val);
						return true;
					}
					return false;
				}

				static bool ExtractUniformCustomRange(std::stringstream& stream, bool& out)
				{
					std::string colon, custom_range;
					stream >> std::ws;
					wchar_t token = stream.peek();
					if (token == ':')
					{
						stream.get();
						stream >> custom_range;
						out = true;
						if (stream.fail() || custom_range != "custom_range")
							return false;
					}

					return true;
				}

				struct UniformData
				{
					Memory::Vector<std::string, Memory::Tag::EffectGraphParameterInfos> names;
					ParamRanges mins;
					ParamRanges maxs;
					ParamRanges defaults;
					unsigned data_id = 0;
					unsigned num_elements;
					GraphType graph_type;
				};

				static UniformData ParseUniformData(const Uniform& uniform, const std::string& macro, const std::wstring& node_name)
				{
					UniformData data;
					auto stream = std::stringstream(uniform.properties);

					data.graph_type = uniform.type;
					if (data.graph_type == GraphType::Invalid)
						throw Resource::Exception() << L"Invalid type for an input in fragment";

					if (data.graph_type == GraphType::Sampler)
					{
						data.data_id = 0;
						data.num_elements = 1;
					}
					else
					{
						data.data_id = (unsigned)GetDrawDataIdentifierFromName(uniform.name);

						if (data.graph_type == GraphType::Texture || data.graph_type == GraphType::Texture3d || data.graph_type == GraphType::TextureCube)
						{
							data.num_elements = 1;
						}
						else if (data.graph_type == GraphType::Spline5)
						{
							if (!ExtractUniformValues(stream, data.names))
								throw Resource::Exception() << L"Unexpected end of file reading uniform names for fragment " << node_name << L".";

							data.mins.resize(1, -1.0f);
							data.maxs.resize(1, -1.0f);
							bool dummy;
							if (!ExtractUniformCustomRange(stream, dummy))
								throw Resource::Exception() << L"Unexpected end of file reading uniform custom range for fragment " << node_name << L".";

							if (data.names.size() != 1)
								throw Resource::Exception() << L"Type and number of elements for a uniform in fragment " << node_name << L" is inconsistent.";

							data.num_elements = (unsigned)data.names.size();
						}
						else if (data.graph_type == GraphType::SplineColour)
						{
							if (!ExtractUniformValues(stream, data.names))
								throw Resource::Exception() << L"Unexpected end of file reading uniform names for fragment " << node_name << L".";

							if (data.names.size() != 1)
								throw Resource::Exception() << L"Type and number of elements for a uniform in fragment " << node_name << L" is inconsistent.";

							data.num_elements = (unsigned)data.names.size();
						}
						else
						{
							if (!ExtractUniformValues(stream, data.names))
								throw Resource::Exception() << L"Unexpected end of file reading uniform names for fragment " << node_name << L".";

							if (!ExtractUniformValues(stream, data.mins))
								throw Resource::Exception() << L"Unexpected end of file reading uniform mins for fragment " << node_name << L".";

							if (!ExtractUniformValues(stream, data.maxs))
								throw Resource::Exception() << L"Unexpected end of file reading uniform maxs for fragment " << node_name << L".";

							if (!ExtractUniformValues(stream, data.defaults))
								throw Resource::Exception() << L"Unexpected end of file reading uniform defaults for fragment " << node_name << L".";

							bool dummy;
							if (!ExtractUniformCustomRange(stream, dummy))
								throw Resource::Exception() << L"Unexpected end of file reading uniform custom range for fragment " << node_name << L".";

							data.num_elements = (unsigned)data.names.size();

							if (data.num_elements != data.mins.size() || data.num_elements != data.maxs.size() || data.num_elements != data.defaults.size())
								throw Resource::Exception() << L"Inconsistent number of elements for a uniform in fragment " << node_name << L".";

							if (data.num_elements > 4)
								throw Resource::Exception() << L"Invalid number of elements for a uniform in fragment " << node_name << L". Max is 4.";

							if (data.graph_type == GraphType::Float3 && data.num_elements > 3)
								throw Resource::Exception() << L"Type and number of elements for a uniform in fragment " << node_name << L" is inconsistent.";

							if (data.graph_type == GraphType::Float2 && data.num_elements > 2)
								throw Resource::Exception() << L"Type and number of elements for a uniform in fragment " << node_name << L" is inconsistent.";

							if ((data.graph_type == GraphType::Float || data.graph_type == GraphType::Int || data.graph_type == GraphType::UInt) && data.num_elements > 1)
								throw Resource::Exception() << L"Type and number of elements for a uniform in fragment " << node_name << L" is inconsistent.";
						}
					}

					return data;
				}

				class ReaderFragment : public FileReader::FFXReader::Fragment
				{
				private:
					EffectGraphNodeFactory::GraphNodeMap& static_graph_nodes;
					EffectGraphNodeStatic node;
					bool reset_lighting_model = true;

				public:
					ReaderFragment(EffectGraphNodeFactory::GraphNodeMap& static_graph_nodes, const std::string& name) : static_graph_nodes(static_graph_nodes), node(name) {}
					~ReaderFragment()
					{
						static_graph_nodes[node.GetTypeId()] = std::move(node);
					}

					void AddConnection(const Connection& connection, const std::string& macro = "") override
					{
						switch (connection.direction)
						{
							case Connection::Direction::In:
							{
								if (!connection.semantic.empty())
									break;

								GraphType graph_type = GetGraphType(connection.type);
								if (graph_type == GraphType::Invalid)
									throw Resource::Exception() << L"Invalid type for an input in fragment";

								node.AddInputConnector(connection.name, graph_type);
								break;
							}
							case Connection::Direction::Out:
							{
								if (!connection.semantic.empty())
									break;

								GraphType graph_type = GetGraphType(connection.type);
								if (graph_type == GraphType::Invalid)
									throw Resource::Exception() << L"Invalid type for an output in fragment";

								node.AddOutputConnector(connection.name, graph_type);
								break;
							}
							case Connection::Direction::Inout:
							{
								if (!connection.semantic.empty())
									break;

								GraphType graph_type = GetGraphType(connection.type);
								if (graph_type == GraphType::Invalid)
									throw Resource::Exception() << L"Invalid type for an inout in fragment";

								node.AddInputConnector(connection.name, graph_type);
								node.AddOutputConnector(connection.name, graph_type);
								break;
							}
							case Connection::Direction::Dynamic:
							{
								GraphType graph_type = GetGraphType(connection.type);
								if (graph_type == GraphType::Invalid)
									throw Resource::Exception() << L"Invalid type for an dynamic in fragment";

								int data_id = GetDrawDataIdentifierFromName(connection.name, true);
								node.AddDynamicParamInfo(graph_type, data_id, 0);
								break;
							}
							case Connection::Direction::Custom:
							{
								GraphType graph_type = GetGraphType(connection.type);
								if (graph_type == GraphType::Invalid)
									throw Resource::Exception() << L"Invalid type for a custom dynamic in fragment";
								node.AddCustomDynamicParamInfo(graph_type);
								break;
							}
							case Connection::Direction::Stage:
							{
								const auto stage_pos = connection.semantic.find_first_of('.');
								if (stage_pos == connection.semantic.npos || stage_pos == 0 || stage_pos + 1 == connection.semantic.length())
									throw Resource::Exception() << L"Invalid stage semantic in fragment";

								GraphType graph_type = GetGraphType(connection.type);
								if (graph_type == GraphType::Invalid)
									throw Resource::Exception() << L"Invalid type for an dynamic in fragment";

								const auto stage = EffectGraphUtil::GetStageFromString(connection.semantic.substr(0, stage_pos));
								if (stage == Stage::NumStage)
									throw Resource::Exception() << L"Unknown stage '" << StringToWstring(connection.semantic.substr(0, stage_pos)) << L"' in fragment";

								const auto ext_point = std::string(ExtensionPoint::ReadNodePrefix) + connection.semantic.substr(stage_pos + 1);
								node.AddStageConnector(connection.name, ext_point, stage);
								break;
							}
							default:
								break;
						}
					}

					void AddUniform(const Uniform& uniform, const std::string& macro = "") override
					{
						const auto node_name = StringToWstring(node.GetName());
						const auto data = ParseUniformData(uniform, macro, node_name);
						node.AddParamInfo(data.graph_type, data.data_id, data.num_elements, data.names, data.mins, data.maxs, data.defaults);
					}

					void AddMacro(const std::string& name, const std::string& value = "") override
					{
						node.AddCustomMacro(name, value);
					}

					void SetLightingModel(LightingModel model) override
					{
						switch (model)
						{
							case LightingModel::PhongMaterial:
								node.SetLightingModelVisibility(DrawCalls::LightingModel::PhongMaterial, reset_lighting_model);
								break;
							case LightingModel::SpecGlossPbrMaterial:
								node.SetLightingModelVisibility(DrawCalls::LightingModel::SpecGlossPbrMaterial, reset_lighting_model);
								break;
							case LightingModel::Anisotropy:
								node.SetLightingModelVisibility(DrawCalls::LightingModel::Anisotropy, reset_lighting_model);
								break;
							default:
								throw Resource::Exception() << L"Invalid lighting model";
						}

						reset_lighting_model = false;
					}

					void SetCommutative(bool value) override
					{
						node.SetInputOrderIndependent(value);
					}

					void SetMetadata(const Metadata& data) override
					{
						node.SetShaderUsage(data.usage);
						node.SetCost(data.cost);
						node.SetEngineOnly(data.engineonly);
					}
				};

				class ReaderExtensionPoint : public FileReader::FFXReader::ExtensionPoint
				{
				private:
					EffectGraphNodeFactory::GraphNodeMap& static_graph_nodes;
					EffectGraphNodeFactory::DefaultNodes& default_graph_nodes;
					EffectGraphNodeStatic input_node;
					EffectGraphNodeStatic output_node;
					bool reset_lighting_model = true;
					bool has_usage = false;
					bool has_stage = false;

					void CheckUsageStage()
					{
						if (has_usage && has_stage)
						{
							if (input_node.GetShaderUsage() == ShaderUsageType::Any)
								return;

							if (input_node.GetShaderUsage() == ShaderUsageType::Vertex || input_node.GetShaderUsage() == ShaderUsageType::VertexPixel)
							{
								if (EffectGraphUtil::IsVertexStage(input_node.GetDefaultStage()))
									return;
							}

							if (input_node.GetShaderUsage() == ShaderUsageType::Pixel || input_node.GetShaderUsage() == ShaderUsageType::VertexPixel)
							{
								if (EffectGraphUtil::IsPixelStage(input_node.GetDefaultStage()))
									return;
							}

							if (input_node.GetShaderUsage() == ShaderUsageType::Compute)
							{
								if (EffectGraphUtil::IsComputeStage(input_node.GetDefaultStage()))
									return;
							}

							throw Resource::Exception() << L"Default Stage is not inside the declared shader usage range";
						}
					}

				public:
					ReaderExtensionPoint(EffectGraphNodeFactory::GraphNodeMap& static_graph_nodes, EffectGraphNodeFactory::DefaultNodes& default_graph_nodes, const std::string& name)
						: static_graph_nodes(static_graph_nodes)
						, default_graph_nodes(default_graph_nodes)
						, output_node(std::string(ReadNodePrefix) + name)
						, input_node(std::string(WriteNodePrefix) + name)
					{
						input_node.SetLinkType(DrawCalls::LinkType::Output);
						output_node.SetLinkType(DrawCalls::LinkType::Input);
						output_types.push_back(input_node.GetTypeId());

						input_node.SetMatchingNodeType(EffectGraphUtil::HashString(output_node.GetName()));
						output_node.SetMatchingNodeType(EffectGraphUtil::HashString(input_node.GetName()));
					}

					~ReaderExtensionPoint()
					{
						if (has_usage != has_stage)
						{
							if (has_usage)
							{
								switch (input_node.GetShaderUsage())
								{
									case ShaderUsageType::Vertex:
										input_node.SetDefaultStage(Stage::VERTEX_STAGE_BEGIN);
										output_node.SetDefaultStage(Stage::VERTEX_STAGE_BEGIN);
										break;
									case ShaderUsageType::Pixel:
										input_node.SetDefaultStage(Stage::PIXEL_STAGE_BEGIN);
										output_node.SetDefaultStage(Stage::PIXEL_STAGE_BEGIN);
										break;
									case ShaderUsageType::Compute:
										input_node.SetDefaultStage(Stage::COMPUTE_STAGE_BEGIN);
										output_node.SetDefaultStage(Stage::COMPUTE_STAGE_BEGIN);
										break;
									default:
										break;
								}
							}
							else
							{
								if (EffectGraphUtil::IsVertexStage(input_node.GetDefaultStage()))
								{
									input_node.SetShaderUsage(ShaderUsageType::Vertex);
									output_node.SetShaderUsage(ShaderUsageType::Vertex);
								}
								else if (EffectGraphUtil::IsPixelStage(input_node.GetDefaultStage()))
								{
									input_node.SetShaderUsage(ShaderUsageType::Pixel);
									output_node.SetShaderUsage(ShaderUsageType::Pixel);
								}
								else if (EffectGraphUtil::IsComputeStage(input_node.GetDefaultStage()))
								{
									input_node.SetShaderUsage(ShaderUsageType::Compute);
									output_node.SetShaderUsage(ShaderUsageType::Compute);
								}
							}
						}

						static_graph_nodes[output_node.GetTypeId()] = std::move(output_node);
						static_graph_nodes[input_node.GetTypeId()] = std::move(input_node);
					}

					void SetType(GraphType graph_type) override
					{
						input_node.AddInputConnector(std::string(WriteConnection), graph_type);
						output_node.AddOutputConnector(std::string(ReadConnection), graph_type);
					}

					void AddConnection(const Connection& connection, const std::string& macro = "") override
					{
						switch (connection.direction)
						{
							case Connection::Direction::Dynamic:
							{
								GraphType graph_type = GetGraphType(connection.type);
								if (graph_type == GraphType::Invalid)
									throw Resource::Exception() << L"Invalid type for an dynamic in fragment";

								int data_id = GetDrawDataIdentifierFromName(connection.name, true);
								input_node.AddDynamicParamInfo(graph_type, data_id, 0);
								output_node.AddDynamicParamInfo(graph_type, data_id, 0);
								break;
							}
							default:
								break;
						}
					}

					void AddUniform(const Uniform& uniform, const std::string& macro = "") override
					{
						const auto node_name = StringToWstring(output_node.GetName());
						const auto data = ParseUniformData(uniform, macro, node_name);
						input_node.AddParamInfo(data.graph_type, data.data_id, data.num_elements, data.names, data.mins, data.maxs, data.defaults);
						output_node.AddParamInfo(data.graph_type, data.data_id, data.num_elements, data.names, data.mins, data.maxs, data.defaults);
					}

					void AddMacro(const std::string& name, const std::string& value = "") override
					{
						input_node.AddCustomMacro(name, value);
						output_node.AddCustomMacro(name, value);
					}

					void SetLightingModel(LightingModel model) override
					{
						switch (model)
						{
							case LightingModel::PhongMaterial:
								input_node.SetLightingModelVisibility(DrawCalls::LightingModel::PhongMaterial, reset_lighting_model);
								output_node.SetLightingModelVisibility(DrawCalls::LightingModel::PhongMaterial, reset_lighting_model);
								break;
							case LightingModel::SpecGlossPbrMaterial:
								input_node.SetLightingModelVisibility(DrawCalls::LightingModel::SpecGlossPbrMaterial, reset_lighting_model);
								output_node.SetLightingModelVisibility(DrawCalls::LightingModel::SpecGlossPbrMaterial, reset_lighting_model);
								break;
							case LightingModel::Anisotropy:
								input_node.SetLightingModelVisibility(DrawCalls::LightingModel::Anisotropy, reset_lighting_model);
								output_node.SetLightingModelVisibility(DrawCalls::LightingModel::Anisotropy, reset_lighting_model);
								break;
							default:
								throw Resource::Exception() << L"Invalid lighting model";
						}

						reset_lighting_model = false;
					}

					void SetDefaultStage(const std::string& stage) override
					{
						input_node.SetDefaultStage(EffectGraphUtil::GetStageFromString(stage));
						output_node.SetDefaultStage(EffectGraphUtil::GetStageFromString(stage));

						has_stage = true;
						CheckUsageStage();
					}

					void SetShaderGroup(const ShaderGroup& group) override
					{
						input_node.SetShaderGroup(group);
						output_node.SetShaderGroup(group);
					}

					void SetDefaultNode(bool is_default) override
					{
						if (is_default)
							default_graph_nodes.push_back(input_node.GetTypeId());
					}

					void SetMetadata(const Metadata& data) override
					{
						input_node.SetShaderUsage(data.usage);
						output_node.SetShaderUsage(data.usage);

						input_node.SetCost(data.cost);
						output_node.SetCost(data.cost);

						input_node.SetEngineOnly(data.engineonly);
						output_node.SetEngineOnly(data.engineonly);

						has_usage = true;
						CheckUsageStage();
					}
				};

				class ReaderDeclaration : public FileReader::FFXReader::Declaration
				{
				private:
					EffectGraphNodeFactory::GraphNodeMap& static_graph_nodes;

				public:
					ReaderDeclaration(EffectGraphNodeFactory::GraphNodeMap& static_graph_nodes, const std::string& name) : static_graph_nodes(static_graph_nodes) {}
				};

			public:
				Reader(EffectGraphNodeFactory::GraphNodeMap& static_graph_nodes, EffectGraphNodeFactory::DefaultNodes& default_graph_nodes) : static_graph_nodes(static_graph_nodes), default_graph_nodes(default_graph_nodes) {}

				std::unique_ptr<Fragment> CreateFragment(const std::string& name) override { return std::make_unique<ReaderFragment>(static_graph_nodes, name); }
				std::unique_ptr<ExtensionPoint> CreateExtensionPoint(const std::string& name) override { return std::make_unique<ReaderExtensionPoint>(static_graph_nodes, default_graph_nodes, name); }
				std::unique_ptr<Declaration> CreateDeclaration(const std::string& name) { return std::make_unique<ReaderDeclaration>(static_graph_nodes, name); }
			};
		}

		

		EffectGraphNodeFactory::EffectGraphNodeFactory()
		{
			PROFILE_ONLY(Memory::StackTag stack(Memory::Tag::EffectGraphNodes));

			// add group node
			const auto type_id = EffectGraphUtil::HashString(GroupNodeName);
			static_graph_nodes[type_id] = EffectGraphNodeStatic(GroupNodeName);
			static_graph_nodes[type_id].SetShaderUsage(ShaderUsageType::Any);

			// Add new node files here
			ReadFile(L"Shaders/Renderer/Nodes/InputOutput.ffx");
			ReadFile(L"Shaders/Renderer/Nodes/UtilityNodes.ffx");
			ReadFile(L"Shaders/Renderer/Nodes/GpuParticles.ffx");
			ReadFile(L"Shaders/Renderer/Nodes/InternalOnly.ffx");
			ReadFile(L"Shaders/Renderer/Nodes/Legacy.ffx");
			ReadFile(L"Shaders/Renderer/Nodes/Effects.ffx");
			ReadFile(L"Shaders/Renderer/Nodes/Debug.ffx");
			ReadFile(L"Shaders/Renderer/Nodes/DynamicNodes.ffx");

			// Set the output index for the output/input nodes (i.e AlbedoColor, TbnNormal etc)
			for (size_t index = 0; index < output_types.size(); ++index)
			{
				auto& type = Find(output_types[index]);
				type.SetOutputTypeIndex(static_cast<unsigned>(index));
				if (auto input_type_id = type.GetMatchingNodeType())
				{
					auto& input_node_type = Find(input_type_id);
					input_node_type.SetOutputTypeIndex(type.GetOutputTypeIndex());
				}

				// TODO: Remove once trails lighting cleanup is done
				if (type.GetName() == "TbnNormal") 
					tbn_normal_output_index = type.GetOutputTypeIndex();
			}

			// Initialize the dynamice parameter nodes
			CreateDynamicParameterNodes();
		}

		unsigned EffectGraphNodeFactory::GetNumOutputTypes() const
		{
			return static_cast<unsigned>(output_types.size());
		}

		Memory::Object<EffectGraphNode> EffectGraphNodeFactory::CreateEffectGraphNode(const std::string& type_name, const unsigned index, const Stage stage)
		{
			const auto type_id = EffectGraphUtil::HashString(type_name);
			try
			{
				return CreateEffectGraphNode(type_id, index, stage);
			}
			catch (const FragmentFileParseError&)
			{
				throw FragmentFileParseError(L"", L"Failed to create effect graph node \"" + StringToWstring(type_name) + L"\". Node type does not exist");
			}
		}

		Memory::Object<EffectGraphNode> EffectGraphNodeFactory::CreateEffectGraphNode(const unsigned type_id, const unsigned index, const Stage stage)
		{
			auto static_graph_node = static_graph_nodes.find(type_id);
			if (static_graph_node == std::end(static_graph_nodes))
				throw FragmentFileParseError(L"", L"Failed to create effect graph node. Node type does not exist");
			return ::EffectGraph::System::Get().CreateNode(static_graph_node->second, index, stage);
		}

		EffectGraphNodeStatic& EffectGraphNodeFactory::Find(const unsigned type_id)
		{
			const auto static_graph_node = static_graph_nodes.find(type_id);
			if (static_graph_node == std::end(static_graph_nodes))
				return null_static_node;
			return static_graph_node->second;
		}

		void EffectGraphNodeFactory::ReadFile(const std::wstring& file)
		{
			try {
				auto reader = Reader(static_graph_nodes, default_graph_nodes);
				Reader::Read(file, reader);
			} catch (const Resource::Exception& e)
			{
				throw FragmentFileParseError(file, StringToWstring(e.what()));
			}
		}

		void EffectGraphNodeFactory::CreateDynamicParameterNodes()
		{
			auto& dynamic_param_manager = GetDynamicParameterManager();
			for( auto itr = dynamic_param_manager.begin(); itr != dynamic_param_manager.end(); ++itr )
			{
				const auto& name = itr->second.name;
				const auto type_id = EffectGraphUtil::HashString( name );

				auto found = Container::FindIf( static_graph_nodes, [type_id]( const std::pair< unsigned, EffectGraphNodeStatic >& node )
				{
					return node.second.GetTypeId() == type_id;
				} );

				if( found != end() )
				{
					itr->second.data_id = found->second.GetDynamicParamInfos().begin()->data_id;
					found->second.GetDynamicParamInfos().begin()->dynamic_param_id = itr->first;

					if( itr->second.data_type != found->second.GetDynamicParamInfos().begin()->type )
						throw FragmentFileParseError( L"DynamicNodes.ffx", L"Dynamic Node: " + StringToWstring( itr->second.name ) + L" has differring types between registration and DynamicNodes.ffx" );
				}
				else
				{
					unsigned data_id = GetDrawDataIdentifierFromName( name, true );
					itr->second.data_id = data_id;
					auto new_node = EffectGraphNodeStatic( name );
					static_graph_nodes[type_id] = std::move( new_node );
					static_graph_nodes[type_id].AddOutputConnector( "output", itr->second.data_type);
					static_graph_nodes[type_id].AddDynamicParamInfo( itr->second.data_type, data_id, itr->first );
				}
			}
		}

		EffectGraphNodeFactory& GetEffectGraphNodeFactory()
		{
			static EffectGraphNodeFactory factory;
			return factory;
		}
	}
}