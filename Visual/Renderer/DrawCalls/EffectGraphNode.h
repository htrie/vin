#pragma once

#include "Common/FileReader/FFXReader.h"
#include "Visual/Renderer/DrawCalls/EffectGraphParameter.h"

namespace Renderer
{
	namespace DrawCalls
	{

		enum class Stage : unsigned
		{
			VertexInit,
			Animation,
			LocalTransform,
			LocalTransform_Calc,
			LocalTransform_Final,
			WorldTransform_Init,
			WorldTransform,
			WorldTransform_Calc,
			WorldTransform_Final,
			ProjectionTransform,
			ProjectionTransform_Calc,
			ProjectionTransform_Final,
			VertexOutput,
			VertexOutput_Calc,
			VertexOutput_Final,
			VertexOutput_Pass,

			VertexToPixel,
			PixelInit,
			UVSetup,
			UVSetup_Calc,
			UVSetup_Final,
			Texturing_Init,
			Texturing,
			Texturing_Calc,
			Texturing_Final,
			PreLighting,
			PreLighting_Calc,
			PreLighting_Final,
			AlphaClip,
			Lighting,
			Lighting_Calc,
			Lighting_Final,
			CustomLighting,
			CustomLighting_Calc,
			CustomLighting_Final,
			LightingEnd,
			LightingEnd_Calc,
			LightingEnd_Final,
			PostLighting,
			PostLighting_Calc,
			PostLighting_Final,
			FogStage,
			FogStage_Calc,
			FogStage_Final,
			PixelOutput,
			PixelOutput_Calc,
			PixelOutput_Final,

			ComputeInit,

			ParticlesSetup,
			ParticlesLifetimeInit,
			ParticlesLifetime,
			ParticlesLifetimeCalc,
			ParticlesEmitInit,
			ParticlesEmit,
			ParticlesEmitCalc,
			ParticlesUpdateInit,
			ParticlesUpdate,
			ParticlesUpdateCalc,
			ParticlesPhysicInit,
			ParticlesPhysic,
			ParticlesPhysicCalc,
			ParticlesIntegrate,
			ParticlesPostIntegrate,
			ParticlesCollision,
			ParticlesCollisionCalc,
			ParticlesWrite,
			ParticlesWriteFinal,

			NumStage,

			// These constants are used to figure out which shader (VS/PS/CS) to generate based on used stages:
			VERTEX_STAGE_BEGIN = VertexInit,
			VERTEX_STAGE_END = VertexToPixel,
			PIXEL_STAGE_BEGIN = VertexToPixel,
			PIXEL_STAGE_END = ComputeInit,
			COMPUTE_STAGE_BEGIN = ComputeInit,
			COMPUTE_STAGE_END = NumStage,
		};

		enum class LinkType : unsigned
		{
			Input,
			Output,
			None
		};

		enum LightingModel
		{
			PhongMaterial,
			SpecGlossPbrMaterial,
			Anisotropy,
			NumLightingModel
		};

		using ShaderUsageType = FileReader::FFXReader::ShaderUsageType;
		using Cost = FileReader::FFXReader::Cost;
		using ShaderMainGroup = FileReader::FFXReader::ShaderMainGroup;
		using ShaderOptGroup = FileReader::FFXReader::ShaderOptGroup;
		using ShaderGroup = FileReader::FFXReader::ShaderGroup;
		using ShaderGroups = FileReader::FFXReader::ShaderGroups;

		extern const std::string StageNames[(unsigned)Stage::NumStage];
		extern const std::string LightingModelNames[(unsigned)LightingModel::NumLightingModel];
		extern const std::string LightingModelMacros[(unsigned)LightingModel::NumLightingModel];
		extern const std::string GroupNodeName;
		extern const std::string LoopCountInputName;

		namespace EffectGraphUtil
		{
			typedef Memory::FlatSet<Stage, Memory::Tag::Unknown> ShaderGroupStages;

			Stage GetStageFromString(const std::string& stage_string);
			LightingModel GetLightingModelFromString(const std::string& in_str);
			unsigned GetGroupIndexByStage(const unsigned in_group_index, const Stage stage);
			unsigned GetMaxGroupIndex();
			unsigned HashString(const std::string& name);
			unsigned MergeTypeId(const unsigned a, const unsigned b);
			ShaderGroupStages GetGroupStages(const ShaderGroup& group);
			bool IsStageEnabled(Stage stage, const ShaderGroups& groups);
			bool IsStageEnabled(Stage stage, const ShaderGroup& group);
			bool IsCompatible(ShaderMainGroup mainGroup, ShaderOptGroup optGroup);

			inline bool IsVertexStage(Stage s) { return s >= Stage::VERTEX_STAGE_BEGIN && s < Stage::VERTEX_STAGE_END; }
			inline bool IsPixelStage(Stage s) { return s >= Stage::PIXEL_STAGE_BEGIN && s < Stage::PIXEL_STAGE_END; }
			inline bool IsComputeStage(Stage s) { return s >= Stage::COMPUTE_STAGE_BEGIN && s < Stage::COMPUTE_STAGE_END; }

			std::string AddDynamicParameterPrefix(const std::string& name);
		}

		class EffectGraphMacros
		{
		public:
			typedef Memory::LockStringPool<char, 512, 64, Memory::Tag::EffectGraphNodes> StringPool;
			typedef Memory::VectorMap<StringPool::Handle, StringPool::Handle, Memory::Tag::EffectGraphNodes> Macros; // Needs to be sorted.

			void Clear();
			void Insert(const StringPool::Handle& macro, const StringPool::Handle& value);
			void Erase(const StringPool::Handle& macro);
			void Add(const std::string& macro, const std::string& value = std::string());
			void Remove(const std::string& macro);
			bool Exists(const std::string& macro) const;
			std::string Find(const std::string& macro) const;
			const Macros& GetMacros() const { return macros; }

		private:
			static StringPool macro_string_pool;
			Macros macros;
		};

		class EffectGraphNodeStatic
		{
		public:
			EffectGraphNodeStatic(const std::string& name);
			EffectGraphNodeStatic() {}

			struct NodeConnector
			{		
				GraphType type;
				bool loop = false;
				const std::string& Name() const { return name; }
				NodeConnector(const std::string& name, GraphType type) : name(name), type(type) {}
				NodeConnector(const std::string& name, const GraphType& type, const bool loop) : name(name), type(type), loop(loop) {}
			private:
				std::string name;
			};
			typedef Memory::Vector<NodeConnector, Memory::Tag::EffectGraphNodeLinks> Connectors;

			struct StageConnector
			{
				Stage stage;
				const std::string& Name() const { return name; }
				const std::string& ExtensionPoint() const { return ext_point; }
				StageConnector(const std::string& name, const std::string& ext_point, Stage stage) : name(name), ext_point(ext_point), stage(stage) {}
			private:
				std::string name;
				std::string ext_point;
			};

			unsigned GetTypeId()						const { return type_id; }
			const std::string& GetName()				const { return name; }
			const EffectGraphMacros& GetCustomMacros()	const { return custom_macros; }
			ShaderUsageType GetShaderUsage()			const { return shader_usage_type; }
			Stage GetDefaultStage()						const { return default_stage; }
			unsigned GetOutputTypeIndex()				const { return output_type_index; }
			unsigned GetMatchingNodeType()				const { return matching_node_type; }
			Cost GetCost()								const { return cost; }
			const ShaderGroup& GetShaderGroup()			const { return shader_group; }

			bool IsInputType()				const { return link_type == LinkType::Input; } // Is Read Extension Point
			bool IsOutputType()				const { return link_type == LinkType::Output; } // Is Write Node Extension Point
			bool IsEngineOnly()				const { return engine_only; }
			bool IsGroup()					const { return is_group; }
			bool IsOutputOnly()				const { return !IsGroup() && output_connectors.empty() && link_type == LinkType::None; }
			bool IsInputOrderIndependent()	const { return input_order_independent; }
			bool IsEnabledByLightingModel(LightingModel lighting_model)		const { return lighting_model_visibility.test(lighting_model); }
			
			const Connectors& GetInputConnectors()	const { return input_connectors; }
			const Connectors& GetOutputConnectors()	const { return output_connectors; }
			const Memory::Vector<StageConnector, Memory::Tag::EffectGraphNodeLinks>& GetStageConnectors()	const { return stage_connectors; }
			const Memory::Vector<EffectGraphParameterStatic, Memory::Tag::EffectGraphParameters>& GetParamInfos()	const { return param_infos; }
			Memory::Vector<EffectGraphParameterStatic, Memory::Tag::EffectGraphParameters>& GetParamInfos() { return param_infos; }
			const Memory::Vector<EffectGraphParameterDynamic, Memory::Tag::EffectGraphParameters>& GetDynamicParamInfos()	const { return dynamic_param_infos; }
			Memory::Vector<EffectGraphParameterDynamic, Memory::Tag::EffectGraphParameters>& GetDynamicParamInfos()	{ return dynamic_param_infos; }
			const Memory::Vector<EffectGraphParameterDynamic, Memory::Tag::EffectGraphParameters>& GetCustomDynamicParamInfos()	const { return custom_dynamic_param_infos; }
			Memory::Vector<EffectGraphParameterDynamic, Memory::Tag::EffectGraphParameters>& GetCustomDynamicParamInfos() { return custom_dynamic_param_infos; }

			void AddStageConnector(const std::string& name, const std::string& extension_point, DrawCalls::Stage stage) { stage_connectors.emplace_back(name, extension_point, stage); }
			void AddInputConnector(const std::string& name, GraphType type)	{ input_connectors.emplace_back(name, type); }
			void AddOutputConnector(const std::string& name, GraphType type)	{ output_connectors.emplace_back(name, type); }
			void SetInputOrderIndependent(bool order_independent)		{ input_order_independent = order_independent; }
			void SetCost(Cost value)							{ cost = value; }
			void SetEngineOnly(const bool value)				{ engine_only = value; }
			void SetShaderUsage(const ShaderUsageType usage)	{ shader_usage_type = usage; }
			void SetLinkType(const LinkType type)				{ link_type = type; }
			void SetMatchingNodeType(const unsigned type)		{ matching_node_type = type; }
			void SetOutputTypeIndex(const unsigned index)		{ output_type_index = index; }
			void SetDefaultStage(const Stage stage)				{ default_stage = stage; }
			void SetShaderGroup(const ShaderGroup& group)		{ shader_group = group; }
			void AddCustomMacro(const std::string& macro, const std::string& value = std::string())		{ custom_macros.Add(macro, value); }
			void SetLightingModelVisibility(const LightingModel lighting_model, bool reset = false)		{ if(reset) lighting_model_visibility.reset(); lighting_model_visibility.set(lighting_model); }		
			void AddParamInfo(
				const GraphType& type, const unsigned int& data_id, unsigned num_elements,
				const Memory::Vector<std::string, Memory::Tag::EffectGraphParameterInfos>& names,
				const ParamRanges& mins,
				const ParamRanges& maxs,
				const ParamRanges& defaults)
			{
				param_infos.emplace_back( type, data_id, num_elements, names, mins, maxs, defaults );
			}
			void AddDynamicParamInfo(const GraphType& type, const unsigned data_id, const unsigned dynamic_param_id)
			{
				dynamic_param_infos.emplace_back( type, data_id, dynamic_param_id );
			}
			void AddCustomDynamicParamInfo(const GraphType& type)
			{
				custom_dynamic_param_infos.emplace_back(type, 0, 0);
			}

		private:
			std::string name;
			unsigned type_id = 0;
			bool engine_only = false;
			bool input_order_independent = false;
			bool is_group = false;
			Stage default_stage = Stage::NumStage;
			LinkType link_type = LinkType::None;
			unsigned output_type_index = 0;
			unsigned matching_node_type = 0;
			EffectGraphMacros custom_macros;
			Cost cost = Cost::Expensive;
			ShaderUsageType shader_usage_type = ShaderUsageType::VertexPixel;
			ShaderGroup shader_group;
			Connectors input_connectors;
			Connectors output_connectors;
			Memory::Vector<StageConnector, Memory::Tag::EffectGraphNodeLinks> stage_connectors;
			Memory::Vector<EffectGraphParameterStatic, Memory::Tag::EffectGraphParameters> param_infos;
			Memory::Vector<EffectGraphParameterDynamic, Memory::Tag::EffectGraphParameters> dynamic_param_infos;
			Memory::Vector<EffectGraphParameterDynamic, Memory::Tag::EffectGraphParameters> custom_dynamic_param_infos;
			std::bitset<NumLightingModel> lighting_model_visibility = std::bitset<NumLightingModel>().set();
		};

		class EffectGraphNode
		{
		public:
			typedef Memory::LockStringPool<char, 1024, 64, Memory::Tag::EffectGraphNodeLinks> NameStringPool;
			typedef Memory::LockStringPool<char, 32, 32, Memory::Tag::EffectGraphNodeLinks> MaskStringPool;

			struct Signature
			{
				unsigned hash_id = 0;
				unsigned index = 0;
				Stage stage = Stage::NumStage;
				Signature() {}
				Signature(const unsigned type_id, const unsigned index, const Stage stage) : index(index), stage(stage)
				{
					hash_id = 0;
					hash_id = EffectGraphUtil::MergeTypeId(hash_id, type_id);
					hash_id = EffectGraphUtil::MergeTypeId(hash_id, index);
					hash_id = EffectGraphUtil::MergeTypeId(hash_id, static_cast<unsigned>(stage));
				}
			};

			struct ConnectorInfo
			{
				unsigned hash_id = 0;
				unsigned index = 0;
				MaskStringPool::Handle mask;

				ConnectorInfo(const unsigned index, const std::string& mask);
				ConnectorInfo() {}

				std::string Mask() const { return std::string(mask.data(), mask.size()); }

				inline bool operator == (const ConnectorInfo& other) const { return hash_id == other.hash_id; }
				inline bool operator != (const ConnectorInfo& other) const { return hash_id != other.hash_id; }
			};

			struct Link
			{
				ConnectorInfo input_info;
				ConnectorInfo output_info;
				EffectGraphNode* node = nullptr;

				Link() {}
				Link(const ConnectorInfo& input_info, const ConnectorInfo& output_info, EffectGraphNode* node) : input_info(input_info), output_info(output_info), node(node) {}
			};

			typedef Memory::SmallVector<Link, 2, Memory::Tag::EffectGraphNodeLinks> Links;
			typedef Memory::SmallVector<Memory::Object<EffectGraphParameter>, 2, Memory::Tag::EffectGraphParameters> ParameterList;
			typedef Memory::Vector<std::string, Memory::Tag::EffectGraphParameters> CustomDynamicNames;

			EffectGraphNode(const EffectGraphNodeStatic& base_node, const unsigned index, const Stage stage);
			EffectGraphNode(const EffectGraphNode& other) = delete;
			EffectGraphNode(); // Note: Needed for the FlatMap in EffectGraphInstance but this is never actually called
			~EffectGraphNode();

			template <typename F>
			void ProcessChildren(F func) const
			{
				Memory::VectorSet<unsigned, Memory::Tag::EffectGraphNodes> processed_nodes;
				processed_nodes.reserve(64);
				ProcessChildren(func, processed_nodes);
			}

			template <typename F, typename P>
			void ProcessChildren(F func, P& processed_nodes) const
			{
				if (processed_nodes.find(this->GetHashId()) != processed_nodes.end())
					return;

				for (const auto& a : input_links)
					a.node->ProcessChildren(func, processed_nodes);

				for (const auto& a : stage_links)
					a.node->ProcessChildren(func, processed_nodes);

				func(*this);

				processed_nodes.insert(this->GetHashId());
			}

			ParameterList::iterator begin();
			ParameterList::iterator end();
			ParameterList::const_iterator begin() const;
			ParameterList::const_iterator end() const;
			const ParameterList& GetParameters()			const	{ return parameters; }
			const EffectGraphNodeStatic& GetEffectNode()	const	{ return *static_ref; }
			const Links& GetInputLinks()					const	{ return input_links; }
			const Links& GetStageLinks()					const	{ return stage_links; }
			const Links& GetChildLinks()					const	{ return child_links; }

			Device::ShaderType GetPreferredShader() const;
			Stage GetStage()			const	{ return signature.stage; }
			unsigned GetHashId()		const	{ return signature.hash_id; }
			unsigned GetIndex()			const	{ return signature.index; }
			unsigned GetStageNumber()	const	{ return stage_number; }
			unsigned GetGraphIndex()	const	{ return graph_index; }
			unsigned GetGroupIndex()	const	{ return group_index; }
			unsigned GetTypeId()		const	{ return type_id; }	
			unsigned GetNumOutput()		const	{ return num_output_links; }
			bool IsMultiInput()			const	{ return num_output_links > 1; }
			bool IsMultiStage()			const	{ return is_multi_stage; }
			bool HasNoEffect()			const;
			bool IsCustomParameter()	const	{ return custom_parameter != 0; }
			unsigned GetCustomParameter() const { return custom_parameter; }
			const CustomDynamicNames& GetCustomDynamicNames() const { return custom_dynamic_names; }
			unsigned GetParentId() const { return parent_id; }
		
			void SetGraphIndex(uint16_t idx)				{ graph_index = idx; }
			void SetPreferredShader(Device::ShaderType t)	{ preferred_shader = t; }
			void SetTypeId(unsigned _type_id)				{ type_id = _type_id; }
			void SetMultiStage(bool value)					{ is_multi_stage = value; }
			void SetGroupIndex(unsigned _group_index)		{ group_index = _group_index; }
			void SetCustomParameter(const std::string& parameter) { custom_parameter = parameter.length() > 0 ? EffectGraphUtil::HashString(parameter) : 0; }
			void SetCustomParameter(unsigned parameter)		{ custom_parameter = parameter; }
			void SetCustomDynamicNames(const CustomDynamicNames& names) { custom_dynamic_names = names; }
			void SetParentId(const unsigned id) { parent_id = id; }
			bool CalculateTypeId(bool merge_pass = false);
			void SetSignature(const unsigned index, const Stage stage);
			void LoadParameters(const JsonValue& node_obj);	
			void AddInput(EffectGraphNode& input_node, const unsigned output_link, const std::string& output_mask, const unsigned input_link, const std::string& input_mask);
			void AddStageInput(EffectGraphNode& input_node, const unsigned output_link, const std::string& output_mask, const unsigned input_link, const std::string& input_mask);
			void AddChildInput(EffectGraphNode& input_node, const unsigned output_link, const std::string& output_mask, const unsigned input_link, const std::string& input_mask);

			// Dynamic connector slots (used for Groups only)
			void AddInputSlot(const std::string& name, const GraphType& type, const bool loop) { input_slots.emplace_back(name, type, loop); }
			void AddOutputSlot(const std::string& name, const GraphType& type, const bool loop) { output_slots.emplace_back(name, type, loop); }
			const EffectGraphNodeStatic::Connectors& GetInputSlots() const { return static_ref->IsGroup() ? input_slots : static_ref->GetInputConnectors(); }
			const EffectGraphNodeStatic::Connectors& GetOutputSlots () const { return static_ref->IsGroup() ? output_slots : static_ref->GetOutputConnectors(); }
			std::optional<unsigned> GetSlotIndexByName(const std::string& name, const bool is_input) const
			{
				unsigned index = 0;
				for (const auto& connector : (is_input) ? GetInputSlots() : GetOutputSlots())
				{
					if (connector.Name() == name)
						return index;
					++index;
				}
				return std::nullopt;
			}
			
			// GRAPHTOOLS_TODO: Consider for removal.	
			void SaveParameters(JsonValue& node_obj, JsonAllocator& allocator) const;		// Called inside EffectGraphInstance::Save() which is then called inside game components save functions which I can't easily remove for now
			std::string GetDebugName() const												// Used for displaying node labels on the graph editor and local variable name during hlsl generation
			{
				return GetEffectNode().GetName() + "_" + std::to_string(GetIndex());
			}

		protected:	
			const EffectGraphNodeStatic* static_ref = nullptr;
			ParameterList parameters;		
			Signature signature;
			Links input_links;
			Links stage_links;
			Links child_links; // used for Group nodes only
			Device::ShaderType preferred_shader = Device::ShaderType::NULL_SHADER;
			CustomDynamicNames custom_dynamic_names;
			unsigned type_id = 0;
			unsigned group_index = 0;
			unsigned stage_number = 0;
			unsigned num_output_links = 0;
			unsigned custom_parameter = 0;
			unsigned parent_id = -1;
			uint16_t graph_index = 0;
			bool is_multi_stage = false;

			// Dynamic connector slots (used for Group nodes only)
			EffectGraphNodeStatic::Connectors input_slots;
			EffectGraphNodeStatic::Connectors output_slots;
		};
	}
}