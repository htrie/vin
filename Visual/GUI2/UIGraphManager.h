#pragma once

#include <functional>
#include <optional>
#include "UIParameter.h"

struct ImDrawList;
struct ImVec2;

namespace Device
{
	namespace GUI
	{
		typedef std::function<bool()> UIGraphRenderCallback;

		struct UIGraphStyle
		{
			simd::color colors[magic_enum::enum_count<GraphType>()];

			UIGraphStyle();
		};

		class UINode
		{
		public:
			friend class UIGraphManager;

			struct UIConnectorInfo
			{
				std::string name;
				GraphType type;
				bool loop = false;
				std::vector<std::string> masks;
				Memory::FlatMap<std::string, simd::vector2, Memory::Tag::UI> cached_positions;

				UIConnectorInfo(const std::string& name, const GraphType& type, const bool loop);
				UIConnectorInfo(const UIConnectorInfo& other);
				void operator = (const UIConnectorInfo& other);
				float OnRender(const UIGraphStyle& style, ImDrawList* draw_list, const float pos_x, const float pos_y, const bool is_input, UINode* current_node, UINode** src_node, UINode** dst_node, std::string& selected_slot);
				void AddMask(const std::string& mask);
				void RemoveMask(const std::string& mask);
				void ClearMasks() { masks.clear(); }
				std::vector<std::string> AddMasksFromFormat(std::string format, std::string previous_mask);
				const std::vector<std::string>& GetMasks() const { return masks; }
				float CalculateWidth();
			};

			UINode(const std::string& type_id, unsigned group = 0, const ImColor& title_color = ImColor(0, 0, 0, 0))
				: type_id(type_id)
				, group(group)
				, data(-1)
				, position(0.f, 0.f)
				, node_width(0.f)
				, parameter_start_x(0.f)
				, is_extension(false)
				, node_color(60, 60, 60, 255)
				, depth(0)
				, title_color(title_color)
			{}
			UINode(const std::string& type_id, const ImColor& title_color) : UINode(type_id, 0, title_color) {}
			UINode(const UINode& in_node);
			virtual ~UINode() {}

			void SetData(unsigned in_data);
			void SetGroup(unsigned in_group);
			void SetRenderCallback(const UIGraphRenderCallback& callback);
			virtual void AddInputSlot(const std::string& name, const GraphType& type, const bool loop);
			virtual void AddOutputSlot(const std::string& name, const GraphType& type, const bool loop);
			void AddParameter(
				const Memory::Vector<std::string, Memory::Tag::EffectGraphParameterInfos>& param_names,
				const Renderer::DrawCalls::ParamRanges& param_mins,
				const Renderer::DrawCalls::ParamRanges& param_maxs,
				GraphType type, const Renderer::DrawCalls::JsonValue& data, void* param_ptr, unsigned index);
			void AddCustomDynamicParameter(const std::string& value, const unsigned index);
			void SetInstanceName(const std::string& _instance_name);
			void SetCustomParameter(const std::string& _custom_param);
			virtual void SetUIPosition(const simd::vector2& _position);
			virtual void SetUISize(const simd::vector2& _size);
			virtual bool SetUIResize(const simd::vector2& delta) { return false; }
			virtual void SavePositionAndSize();
			virtual void RevertPositionAndSize();
			void RevertResize() {  }
			void SetNodeColor(const simd::vector4& color) { node_color = color; }
			void SetDepth(int _depth) { depth = _depth; }
			const std::string& GetTypeId() const;
			const std::string& GetInstanceName() const;
			
			unsigned GetData();
			const simd::vector2& GetUIPosition() const;
			const simd::vector2& GetUISize() const;
			const std::vector<std::unique_ptr<UIParameter>>& GetParameters() const { return parameters; }
			bool IsExtensionPoint() const { return is_extension; }
			bool IsGroupRegion() const { return is_group_region; } // Consider have a ui node type enum instead
			bool HasCustomRanges() const { return has_custom_ranges; }
			const std::vector<UIConnectorInfo>& GetInputSlots() const { return input_slots; }
			const std::vector<UIConnectorInfo>& GetOutputSlots() const { return output_slots; }

			struct NodeRect
			{
				simd::vector2 min;
				simd::vector2 max;
			};
			NodeRect GetNodeRect() const
			{
				return node_rect;
			}

			void SetParentId(unsigned in_parent) { parent_id = in_parent; }
			void SetParentOffset(const simd::vector2& offset) { parent_offset = offset; }
			void SetDescription(const std::string& text) { description = text; }
			unsigned GetParentId() const { return parent_id; }
			const simd::vector2& GetParentOffset() const { return parent_offset; }
			const simd::vector2& GetTransformPos() const { return transform_pos; }
			const std::string& GetDescription() const { return description; }

		protected:
			// NODEGRAPH_TODO : This function definitely needs refactoring
			virtual void OnRender(const UIGraphStyle& style, ImDrawList* draw_list, std::vector<UINode*>& selected_nodes, UINode** input_node, UINode** output_node, std::string& selected_node_input, std::string& selected_node_output, UIParameter** changed_parameter, bool& node_active, bool& on_group_changed, bool refresh_selection);
			virtual void CalculateNodeWidth();
			void CalculateInitialNodeRect();
			void SetTransformPos(const simd::vector2& _transform_pos);
			bool GetInputSlotPos(const std::string& input_slot, ImVec2& pos) const;
			bool GetOutputSlotPos(const std::string& output_slot, ImVec2& pos) const;
			unsigned int GetInputSlotColor(const UIGraphStyle& style, const std::string& input_slot) const;
			unsigned int GetOutputSlotColor(const UIGraphStyle& style, const std::string& output_slot) const;
			GraphType GetInputSlotType(const std::string& input_slot) const;
			int GetInputSlotIndex(const std::string& input_slot) const;
			int GetOutputSlotIndex(const std::string& input_slot) const;
			simd::vector2 GetNodeNamePos() const;
			virtual bool IntersectsBox(const ImVec2& min, const ImVec2& max) const;
			static void GetConnectorNameAndMaskHelper(const std::string& connector_str, std::string& out_connector_name, std::string& out_connector_mask);
			static GraphType GetMaskTypeHelper(const std::string& mask, const GraphType& default_type);
			
			UIGraphRenderCallback render_callback;
			unsigned data;
			std::string type_id;
			unsigned group;  // Note: this is with regards to Vertex, Pixel, Compute
			std::vector<UIConnectorInfo> input_slots;
			std::vector<UIConnectorInfo> output_slots;
			std::vector<std::unique_ptr<UIParameter>> parameters;
			std::string instance_name;
			std::string custom_parameter;
			simd::vector4 node_color;
			simd::vector2 position;
			simd::vector2 transform_pos;
			simd::vector2 size = 0.f;
			simd::vector2 saved_position = 0.f;
			simd::vector2 saved_parent_offset = 0.f;
			simd::vector2 saved_size = 0.f;
			ImColor title_color;
			float node_width;
			float node_content_width;
			float parameter_start_x;
			bool is_extension = false;
			bool is_group_region = false;  // Consider have a ui node type enum instead
			int depth;
			bool highlight_flow = false;
			bool is_highlighted_prev = false;
			bool is_highlighted_after = false;
			bool is_highlighted_selected = false;
			bool has_custom_ranges = false;
			NodeRect node_rect;

			// for grouping nodes into boxes
			unsigned parent_id = -1; 
			simd::vector2 parent_offset = 0.f;
			std::string description;
		};

		struct LinkEndPoint
		{
			UINode* node = nullptr;
			std::string slot;  // stores both the slot name and mask in name.mask format
		};
		typedef Memory::Vector<LinkEndPoint, Memory::Tag::EffectGraphNodeLinks> LinkEndPoints;

		struct UINodeLink
		{
			LinkEndPoint input;		// The right side of the link (left side slots of the node ui)
			LinkEndPoint output;	// The left side of the link (right side slots of the node ui)
			bool is_group_edge_link;	// To signify if this is a link connected to the edges of a group region from the inside

			UINodeLink() {}
			UINodeLink(UINode* input_node, UINode* output_node, const std::string& input_slot, const std::string& output_slot, const bool is_group_edge_link)
			{
				input = { input_node, input_slot };
				output = { output_node, output_slot };
				this->is_group_edge_link = is_group_edge_link;
			}
		};

		class UIExtensionPoint : public UINode
		{
		public:
			friend class UIGraphManager;
			UIExtensionPoint(const unsigned index, const unsigned group);

			void SetOutputParameter(const UIParameter* parameter);
			void SetInputParameter(const UIParameter* parameter);
			std::string GetOutputSlotNameByIndex(int index) const { return output_slots.empty() ? "" : output_slots[index].name; }
			std::string GetInputSlotNameByIndex(int index) const { return input_slots.empty() ? "" : input_slots[index].name; }
			std::string GetOutputParameterValue() const;// { return ext_out_parameter ? ext_out_parameter->GetCurrentValueString() : ""; }
			std::string GetInputParameterValue() const;// { return ext_in_parameter ? ext_in_parameter->GetCurrentValueString() : ""; }
			void GetAllOutputSlotNames(std::vector<std::string>& _output_slots)
			{
				for(const auto& slot : output_slots)
					_output_slots.push_back(slot.name);
			}
			void GetAllInputSlotNames(std::vector<std::string>& _input_slots)
			{
				for(const auto& slot : input_slots)
					_input_slots.push_back(slot.name);
			}

		protected:
			void OnRender(const UIGraphStyle& style, ImDrawList* draw_list, std::vector<UINode*>& selected_nodes, UINode** input_node, UINode** output_node, std::string& selected_node_input, std::string& selected_node_output, UIParameter** changed_parameter, bool& node_active, bool& on_group_changed, bool refresh_selection) override;
			void CalculateNodeWidth() override;

			unsigned id;
			std::unique_ptr<UIParameter> ext_out_parameter;
			std::unique_ptr<UIParameter> ext_in_parameter;
			std::vector<std::string> split_selections;
			std::vector<std::string> slot_selections;

			int selected_split_index;
			std::string changed_extension_point;
			std::string previous_extension_point;
			int selected_slot_index;
			bool selected_slot_is_input; 
			std::string parameter_value;
		};

		class UIGroupRegion : public UINode
		{
		public:
			friend class UIGraphManager;
			UIGroupRegion(const unsigned index, const unsigned group, const std::vector<UINode*>& nodes);
			UIGroupRegion(const UIGroupRegion& in_group);
			~UIGroupRegion();

			unsigned GetId() const { return id; }

			void SetUIPosition(const simd::vector2& _position) override;
			void SetUISize(const simd::vector2& _position) override;
			bool SetUIResize(const simd::vector2& delta) override;
			void SavePositionAndSize() override;
			void RevertPositionAndSize() override;
			void AddInputSlot(const std::string& name, const GraphType& type, const bool loop) override;
			void AddOutputSlot(const std::string& name, const GraphType& type, const bool loop) override;

			void AddChildren(const std::vector<UINode*>& nodes, const std::vector<std::unique_ptr<UINodeLink>>& ui_links = std::vector<std::unique_ptr<UINodeLink>>());
			void RemoveChildren(const std::vector<UINode*>& nodes, const std::vector<std::unique_ptr<UINodeLink>>& ui_links = std::vector<std::unique_ptr<UINodeLink>>());

			void RemoveSlots(std::vector<std::unique_ptr<UINodeLink>>& ui_links);
			void UpdateSlots();

			const std::vector<UINode*>& GetChildren() const { return children; }

			void ResnapSize();
			bool IsMouseInside() const { return is_mouse_inside; }

			bool IsExternalLinkingDisabled() const { return !input_slots.empty() || !output_slots.empty(); }
			void IncrementExternalLinks() { ++num_external_links; }
			void DecrementExternalLinks() { --num_external_links; }

		protected:
			void OnRender(const UIGraphStyle& style, ImDrawList* draw_list, std::vector<UINode*>& selected_nodes, UINode** input_node, UINode** output_node, std::string& selected_node_input, std::string& selected_node_output, UIParameter** changed_parameter, bool& node_active, bool& on_group_changed, bool refresh_selection) override;
			void CalculateNodeWidth() override {}
			bool IntersectsBox(const ImVec2& min, const ImVec2& max) const override;

			unsigned id = 0;
			std::vector<UINode*> children;
			UINode::NodeRect local_space_size;
			bool is_loop_box = false;
			bool edge_left_hovered = false;
			bool edge_top_hovered = false;
			bool edge_right_hovered = false;
			bool edge_bottom_hovered = false;
			bool changing_description = false;
			bool selected_slot_is_input = false;
			std::string current_link_name;
			GraphType current_link_type = GraphType::Bool;
			int to_remove_input_slot = -1;
			int to_remove_output_slot = -1;
			std::vector<UIConnectorInfo> modified_input_slots;
			std::vector<UIConnectorInfo> modified_output_slots;
			bool show_error = false;
			bool is_mouse_inside = false;
			unsigned num_external_links = 0;
		};

		class UIGraphManager
		{
		public:
			UIGraphManager();

			void AddCreatorId(const std::string& type_id, const unsigned group);

			// callbacks
			typedef bool(*OnAddNodeCallback)(UINode& new_node, void* pUserContext);
			typedef void(*OnRemoveNodeCallback)(UINode& node, const bool last, void* pUserContext);
			typedef bool(*OnLinkNodesCallback)(UINode& source_node, UINode& input_node, unsigned int input_slot_index, const std::string& input_slot_mask, unsigned int output_slot_index, const std::string& output_slot_mask, void* pUserContext);
			typedef void(*OnUnlinkNodesCallback)(UINode& source_node, UINode& input_node, unsigned int input_slot_index, const std::string& input_slot_mask, unsigned int output_slot_index, const std::string& output_slot_mask, void* pUserContext);
			typedef void(*OnSplitSlotComponentsCallback)(UINode& source_node, bool is_input, unsigned int slot_index, const std::vector<std::string>& prev_masks, const std::vector<std::string>& masks, const std::vector<UINodeLink*>& connected_links, void* pUserContext);
			typedef void(*OnParameterChangedCallback)(UINode& node, UIParameter& parameter, void* pUserContext);
			typedef void(*OnExtensionPointStageChangedCallback)(const std::string& selected_stage, const std::string& previous_stage, void* pUserContext);
			typedef void(*OnAddNewExtensionPointCallback)(const std::string& new_extension_point_name, const simd::vector2& current_pos, void* pUserContext);
			typedef void(*OnAddNewExtensionPointSlotCallback)(const std::string& slot_name, const std::string& stage_name, const simd::vector2& input_pos, const simd::vector2& output_pos, void* pUserContext);
			typedef void(*OnRemoveExtensionPointSlotCallback)(const std::string& slot_name, const std::string& stage_name, void* pUserContext);
			typedef void(*OnAddDefaultExtensionPointsCallback)(const unsigned group, void* pUserContext);
			typedef void(*OnPositionChangedCallback)(UINode& node, void* pUserContext);
			typedef void(*OnResizeCallback)(UINode& node, void* pUserContext);
			typedef bool(*OnRenderGraphProperties)(void* pUserContext);
			typedef void(*OnCopyCallback)(std::vector<UINode*>& new_node, void* pUserContext);
			typedef void(*OnPasteCallback)(void* pUserContext);
			typedef bool(*OnCloseCallback)(void* pUserContext);
			typedef void(*OnSaveCallback)(void* pUserContext);
			typedef void(*OnCustomParameterSet)(UINode& node, const char* custom_param, void* pUserContext);
			typedef void(*OnUndoCallback)(void* pUserContext);
			typedef void(*OnRedoCallback)(void* pUserContext);
			typedef void(*OnGraphChangedCallback)(void* pUserContext);
			typedef void(*OnCustomRangeCallback)(UINode& node, void* pUserContext);
			typedef void(*OnChildRemovedCallback)(UINode& node, const unsigned group_index, void* pUserContext);
			typedef void(*OnChildAddedCallback)(UINode& node, const unsigned group_index, void* pUserContext);
			typedef void(*OnGroupChanged)(UINode& node, void* pUserContext);
			void SetCallbacks(
				OnAddNodeCallback on_add_callback, 
				OnRemoveNodeCallback on_remove_callback, 
				OnLinkNodesCallback on_link_callback, 
				OnUnlinkNodesCallback on_ulink_callback, 
				OnSplitSlotComponentsCallback on_splitslot_components_callback,
				OnParameterChangedCallback on_parameter_changed_callback,
				OnExtensionPointStageChangedCallback on_extensionpoint_stage_changed_callback,
				OnAddNewExtensionPointCallback on_add_new_extension_point_callback,
				OnAddNewExtensionPointSlotCallback on_add_new_extension_point_slot_callback,
				OnRemoveExtensionPointSlotCallback on_remove_extension_point_slot_callback,
				OnAddDefaultExtensionPointsCallback on_add_default_extension_points_callback,
				OnPositionChangedCallback on_position_changed_callback,
				OnResizeCallback on_resize_callback,
				OnRenderGraphProperties on_render_graph_properties_callback,
				OnCopyCallback on_copy_callback,
				OnPasteCallback on_paste_callback,
				OnSaveCallback on_save_callback,
				OnCustomParameterSet on_custom_param_callback,
				OnUndoCallback on_undo_callback,
				OnRedoCallback on_redo_callback,
				OnGraphChangedCallback on_graph_changed_callback,
				OnCustomRangeCallback on_custom_range_callback,
				OnChildRemovedCallback on_child_removed,
				OnChildAddedCallback on_child_added,
				OnGroupChanged on_group_changed,
				void* pUserContext);
			
			bool AddUINode(const UINode& node, bool select /*=false*/);
			void AddUILink(const std::string& input_node, const std::string& output_node, const std::string& input_slot, const std::string& input_slot_mask, const std::string& output_slot, const std::string& output_slot_mask);
			void AddUIUnusedLink(const std::string& source_node, const std::string& slot, const std::string& mask, const bool is_input);
			void AddUIChildren(const unsigned group_index, const std::string& node_instance);
			bool AddOutputExtensionPoint(const unsigned group, unsigned index, const std::string& slot_name, const GraphType& type, const UIParameter* parameter, const simd::vector2& position);
			bool AddInputExtensionPoint(const unsigned group, unsigned index, const std::string& slot_name, const GraphType& type, const UIParameter* parameter, const simd::vector2& position);
			std::optional<std::string> GetExtensionPointName(const unsigned index, bool get_closest = false);
			void FinalizeExtensionPoints(const std::map<unsigned, std::vector<std::string>>& slot_selections);
			bool HasExtensionPointsByGroup(unsigned group);

			bool MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
			bool ProcessInput();
			void OnRender();
			
			void Reset(bool recover);
			void ResetSelection() { selected_nodes.clear(); }
			void ResetToOrigin();
			void FocusToPos(const simd::vector2& pos, bool recenter);

			void SetErrorMessage(const std::string& msg);
			void SetDivisionHeight(const float height) { division_height = height; }
			void SetContextGroups(unsigned group) { group_0 = group; has_division = false; }
			void SetContextGroups(unsigned group0, unsigned group1) { group_0 = group0; group_1 = group1; has_division = true; }
			void SetHighlightFlow(bool highlight) { highlight_flow = highlight; }
			void SetStyle(const UIGraphStyle& s) { style = s; }
			UIGraphStyle& GetStyle() { return style; }
			const UIGraphStyle& GetStyle() const { return style; }

		private:
			void AddNode(const std::string type_id, const simd::vector2& pos);
			void DeleteNode(const UINode* node, const bool last);
			void CreateLink(UINode* input_node, UINode* output_node, const std::string& input_slot, const std::string& output_slot, const bool is_group_edge_link);
			void RemoveLink(const UINodeLink* link);
			void ConnectNodes(UINode* input_node, UINode* output_node, const std::string& selected_node_input, const std::string& selected_node_output, const bool reference_group_edge_link = true);
			bool IsLinkingDisabled(const UINode& input, const UINode& output);
			bool CanGroupRejectNode(const UIGroupRegion* group_node, const UINode* node);
			bool CanGroupAcceptNode(const UIGroupRegion* group_node, const UINode* node);
			void ResetPerFrame();
			void HandleExtensionPointMessages(UIExtensionPoint * ext_node);
			void GetSelectionRect(ImVec2& out_min, ImVec2& out_max);
			void OpenCustomParameterWindow(UINode* node, const simd::vector2& window_pos, bool show);
			void OpenSearchWindow(const simd::vector2& window_pos);
			void RenderSplitComponentsMenuItems();
			void OnSlotComponentsChanged(UINode* uinode, bool is_input, unsigned int slot_index, const std::vector<std::string>& prev_mask, const std::vector<std::string>& masks);
			void OnScaleUpdate(float scale);
			void HighlightFlow(UINode* highlight_node, ImDrawList* draw_list, bool flag);
			void DrawPendingLinks(const LinkEndPoints& input_end_points, ImDrawList* draw_list);
			void DrawEstablishedLinks(ImDrawList* draw_list);
			void DrawHighlightedLinks(ImDrawList* draw_list);
			void CaptureReconnect();
			void AddGroupRegionNode(const std::vector<UINode*>& to_group, const simd::vector2& pos);
			void RemoveGroupRegionNode(const UINode* selected_node, const bool last);
			void RefreshGroupNodeLinks(const UIGroupRegion* group_node, const std::function<void()>& func);
			unsigned GetDivisionArea(const UINode* node) const;
			bool CheckNodesDivisionArea(const std::vector<UINode*>& nodes) const;
			LinkEndPoints UpdateReconnectEndPoints();
			LinkEndPoints SetupReconnection();

			OnAddNodeCallback m_pOnAddNodeCallback = nullptr;
			OnRemoveNodeCallback m_pOnRemoveNodeCallback = nullptr;
			OnLinkNodesCallback m_pOnLinkNodesCallback = nullptr;
			OnUnlinkNodesCallback m_pOnUnlinkNodesCallback = nullptr;
			OnSplitSlotComponentsCallback m_pOnSplitSlotComponents = nullptr;
			OnParameterChangedCallback m_pOnParameterChangedCallback = nullptr;
			OnExtensionPointStageChangedCallback m_pOnExtensionPointStageChanged = nullptr;
			OnAddNewExtensionPointCallback m_pOnAddNewExtensionPoint = nullptr;
			OnAddNewExtensionPointSlotCallback m_pOnAddNewExtensionPointSlot = nullptr;
			OnRemoveExtensionPointSlotCallback m_pOnRemoveExtensionPointSlot = nullptr;
			OnAddDefaultExtensionPointsCallback m_pOnAddDefaultExtensionPoints = nullptr;
			OnPositionChangedCallback m_pOnPositionChangedCallback = nullptr;
			OnResizeCallback m_pOnResizeCallback = nullptr;
			OnCopyCallback m_pOnCopyCallback = nullptr;
			OnPasteCallback m_pOnPasteCallback = nullptr;
			OnSaveCallback m_pOnSaveCallback = nullptr;
			OnCustomParameterSet m_pOnCustomParameterSet = nullptr;
			OnUndoCallback m_pOnUndoCallback = nullptr;
			OnRedoCallback m_pOnRedoCallback = nullptr;
			OnGraphChangedCallback m_pOnGraphChangedCallback = nullptr;
			OnCustomRangeCallback m_pOnCustomRangeCallback = nullptr;
			OnChildRemovedCallback m_pOnChildRemoved = nullptr;
			OnChildAddedCallback m_pOnChildAdded = nullptr;
			OnGroupChanged m_pOnGroupChanged = nullptr;
			void* m_pCallbackEventUserContext = nullptr;

			std::vector<std::pair<std::string, unsigned>> type_ids;
			std::unique_ptr<UINode> properties_node;
			std::vector<std::unique_ptr<UINode>> ui_nodes;
			std::vector<std::unique_ptr<UINodeLink>> ui_links;
			std::map<unsigned, UIExtensionPoint*> ui_extension_points;
			std::map<unsigned, UIGroupRegion*> ui_group_regions;
			UIGroupRegion* resized_group_region = nullptr;
			UIGraphStyle style;
			std::string error_msg;
			float error_timeout = 0.0f;
			bool has_division = false;
			unsigned group_0 = 0;
			unsigned group_1 = 1;
			float division_height = 0.f;
			int highest_depth = -1;

			std::vector<UINode*> selected_nodes;
			UINode* input_node = nullptr;			// The node that currently has it's input slot (i.e. the left side slots of the node ui) selected
			UINode* output_node = nullptr;			// The node that currently has it's output slot (i.e the right side slots of the node ui) selected
			std::string selected_node_input;		// The name of the currently selected input slot (i.e. the left side slots of the node ui)
			std::string selected_node_output;		// The name of the currently selected output slot (i.e. the right side slots of the node ui)
			ImGuiTextFilter context_menu_filter;
			simd::vector2 transform_pos = simd::vector2(0.f);
			simd::vector2 context_menu_pos = simd::vector2(0.f);
			simd::vector2 scrolling = simd::vector2(0.f);
			simd::vector2 selection_start_rect = simd::vector2(0.f);
			bool refresh_selection = true;
			bool show_search_window = false;
			bool multi_select_mode = false;
			bool context_menu_open = false;
			bool highlight_flow = false;
			bool position_changed = false;
			bool refresh_group_region_sizes = false;
			bool create_group_region = false;

			struct ReconnectInfo
			{
				UINodeLink link;
				bool is_input = false;
			};
			Memory::Vector<ReconnectInfo, Memory::Tag::EffectGraphNodeLinks> reconnect_infos;
		};
	}
}