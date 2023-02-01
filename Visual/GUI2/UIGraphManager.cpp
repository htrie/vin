#include "Common/Utility/StringManipulation.h"
#include "Common/Utility/RandomColorTable.h"

#include "Visual/GUI2/imgui/imgui.h"
#include "Visual/Device/Constants.h"
#include "Visual/GUI2/GUIInstance.h"
#include "Visual/GUI2/Icons.h"

#include "UIGraphManager.h"

namespace Device
{
	namespace GUI
	{
		float CURRENT_SCALE = 1.f;
		float PARAMETER_WIDTH_DEFAULT = 200.0f;
		float CONNECTOR_PARAMETER_WIDTH_DEFAULT = PARAMETER_WIDTH_DEFAULT * 0.5f;
		simd::vector2 SLOT_PARAMETER_PADDING_DEFAULT = simd::vector2(4.f, 4.f);
		float PARAMETER_WIDTH = PARAMETER_WIDTH_DEFAULT;
		float CONNECTOR_PARAMETER_WIDTH = CONNECTOR_PARAMETER_WIDTH_DEFAULT;
		simd::vector2 SLOT_PARAMETER_PADDING = SLOT_PARAMETER_PADDING_DEFAULT;
		float NODE_SLOT_RADIUS_DEFAULT = 4.0f;
		ImVec2 NODE_WINDOW_PADDING_DEFAULT(8.0f, 8.0f);
		ImVec2 SLOT_PADDING_DEFAULT(4.0f, 8.0f);
		float GRID_SZ_DEFAULT = 64.0f;
		simd::vector2 GRID_START = 0.f;
		simd::vector2 DEFAULT_PROPERTIES_POS = simd::vector2(800, 450);
		const ImVec2 HALF(0.5f, 0.5f);
		float ZOOM_MIN = 0.1f;
		float ZOOM_MAX = 2.5f;

		float NODE_SLOT_RADIUS = NODE_SLOT_RADIUS_DEFAULT;
		ImVec2 NODE_WINDOW_PADDING = NODE_WINDOW_PADDING_DEFAULT;
		ImVec2 SLOT_PADDING = SLOT_PADDING_DEFAULT;
		float GRID_SZ = GRID_SZ_DEFAULT;

		const simd::vector2 DEFAULT_GROUP_BOX_SIZE = simd::vector2(200.f, 150.f);
		const simd::vector4 DEFAULT_GROUP_BG_COLOR = simd::vector4(0, 0, 70, 150);
		const simd::vector4 DEFAULT_GROUP_BORDER_COLOR = simd::vector4(0, 0, 150, 255);
		constexpr std::string_view GroupBoxNodeName = "<Group>";
		constexpr std::string_view LoopCountInputName = "<loop_count>";
		std::string GroupLinkTypes[] =
		{
			FileReader::FFXReader::GetGraphTypeName(GraphType::Bool),
			FileReader::FFXReader::GetGraphTypeName(GraphType::Int),
			FileReader::FFXReader::GetGraphTypeName(GraphType::Int2),
			FileReader::FFXReader::GetGraphTypeName(GraphType::Int3),
			FileReader::FFXReader::GetGraphTypeName(GraphType::Int4),
			FileReader::FFXReader::GetGraphTypeName(GraphType::UInt),
			FileReader::FFXReader::GetGraphTypeName(GraphType::Float),
			FileReader::FFXReader::GetGraphTypeName(GraphType::Float2),
			FileReader::FFXReader::GetGraphTypeName(GraphType::Float3),
			FileReader::FFXReader::GetGraphTypeName(GraphType::Float4),
			FileReader::FFXReader::GetGraphTypeName(GraphType::Float3x3),
		};
		bool enable_group_link_switching = true;  // TODO: find a better way for this

		struct ScaleInfo
		{
			simd::vector2 pivot_center = simd::vector2(0.f);
			float scale = 0.f;
		};
		std::vector<ScaleInfo> scale_infos;

		void ScalePositionHelper(simd::vector2& position)
		{
			float prev_scale = 1.f;
			for (const auto& scale_info : scale_infos)
			{
				position -= scale_info.pivot_center;
				float scale = scale_info.scale / prev_scale;
				position = position * scale;
				prev_scale = scale_info.scale;
				position += scale_info.pivot_center;
			}
		}

		void UnscalePositionHelper(simd::vector2& position)
		{
			float prev_scale = 1.f;
			std::vector<float> scales;
			for (const auto& scale_info : scale_infos)
			{
				float scale = scale_info.scale / prev_scale;
				scales.push_back(scale);
				prev_scale = scale_info.scale;
			}

			unsigned index = static_cast< unsigned >( scales.size() - 1 );
			for (auto itr = scale_infos.rbegin(); itr != scale_infos.rend(); ++itr)
			{
				auto scale_info = *itr;
				position -= scale_info.pivot_center;
				position = position / scales[index];
				position += scale_info.pivot_center;
				--index;
			}
		}

		ImU32 GetHighlightColor(bool is_highlighted_prev, bool is_highlighted_after, bool is_highlighted_selected)
		{
			if (is_highlighted_selected)
				return IM_COL32(255, 255, 255, 255);

			if (is_highlighted_prev && is_highlighted_after)
				return IM_COL32(0, 0, 255, 255);

			if (is_highlighted_prev)
				return IM_COL32(0, 255, 0, 255);

			if(is_highlighted_after)
				return IM_COL32(0, 120, 0, 255);

			return IM_COL32(80, 80, 80, 255);
		}

		ImU32 GetTypeColor(const UIGraphStyle& style, GraphType type)
		{
			if (const auto idx = magic_enum::enum_index(type))
				return ImGui::GetColorU32(ImVec4(style.colors[*idx].rgba()));

			return IM_COL32(80, 80, 80, 255);
		}

		bool IsThickLink(GraphType ui_type)
		{
			return ui_type == GraphType::UvField || ui_type == GraphType::NormField;
		}

		bool IsEnumValueEnabled(const UIParameter* parameter, size_t index)
		{
			if (!parameter || parameter->GetType() != GraphType::Enum || parameter->GetNames().empty())
				return false;

			Renderer::DrawCalls::JsonDocument doc;
			doc.SetObject();
			parameter->GetLayout(doc, doc.GetAllocator());

			if (!doc.HasMember(L"layout"))
				return false;

			const auto& range = doc[L"layout"];
			if (range.HasMember(L"enabled"))
				for (const auto& value : range[L"enabled"].GetArray())
					if (value.GetUint() == index)
						return true;

			return false;
		}

		size_t GetEnumValue(const UIParameter* parameter)
		{
			if (!parameter || parameter->GetType() != GraphType::Enum || parameter->GetNames().empty())
				return 0;

			Renderer::DrawCalls::JsonDocument doc;
			doc.SetObject();
			parameter->GetValue(doc, doc.GetAllocator());

			int value;
			Renderer::DrawCalls::ParameterUtil::Read(doc, value);

			if (value < 0 || value >= parameter->GetNames().size())
				return parameter->GetNames().size() - 1;

			return size_t(value);
		}

		std::string GetEnumValueString(const UIParameter* parameter)
		{
			if (!parameter || parameter->GetType() != GraphType::Enum || parameter->GetNames().empty())
				return "";

			const auto value = GetEnumValue(parameter);
			if (value < parameter->GetNames().size())
				return parameter->GetNames()[value];

			return "";
		}

		Renderer::DrawCalls::JsonValue& GetOrCreateMember(Renderer::DrawCalls::JsonValue& value, const wchar_t* name, Renderer::DrawCalls::JsonAllocator& allocator, std::optional<rapidjson::Type> type = std::nullopt)
		{
			if (auto f = value.FindMember(name); f != value.MemberEnd())
			{
				if (type && f->value.GetType() != *type)
					f->value = Renderer::DrawCalls::JsonValue(*type);

				return f->value;
			}

			value.AddMember(Renderer::DrawCalls::JsonValue().SetString(name, allocator), Renderer::DrawCalls::JsonValue(type ? *type : rapidjson::Type::kNullType), allocator);
			return value[name];
		}

		UINode::UIConnectorInfo::UIConnectorInfo(const std::string& name, const GraphType& type, const bool loop) : name(name), type(type), loop(loop)
		{
		}

		UINode::UIConnectorInfo::UIConnectorInfo(const UINode::UIConnectorInfo& other)
		{
			name = other.name;
			type = other.type;
			loop = other.loop;
			for(auto itr = other.cached_positions.begin(); itr != other.cached_positions.end(); ++itr)
				cached_positions[itr->first] = itr->second;
		}

		void UINode::UIConnectorInfo::operator = (const UINode::UIConnectorInfo& other)
		{
			name = other.name;
			type = other.type;
			loop = other.loop;
			for(auto itr = other.cached_positions.begin(); itr != other.cached_positions.end(); ++itr)
				cached_positions[itr->first] = itr->second;
		}

		static char* RemoveNamespace(char* buffer, size_t buf_size, const std::string_view& name)
		{
			if (buf_size == 0)
				return buffer;

			size_t pos = 0;
			for (size_t a = 0; a < name.length(); a++)
			{
				if (pos + 1 < buf_size)
					buffer[pos++] = name[a];

				if (name[a] == ':')
					pos = 0;
			}

			buffer[pos] = '\0';
			return buffer;
		}

		static char* GetNamespace(char* buffer, size_t buf_size, const std::string_view& name)
		{
			if (buf_size == 0)
				return buffer;

			buffer[0] = '\0';
			for (size_t a = name.length(); a > 0; a--)
			{
				if (name[a - 1] != ':')
					continue;

				for (; a > 1 && name[a - 2] == ':'; a--) {}

				size_t p = 0;
				for (; p + 1 < buf_size && p < a - 1; p++)
					buffer[p] = name[p];

				buffer[p] = '\0';
				break;
			}

			return buffer;
		}

		float UINode::UIConnectorInfo::OnRender(const UIGraphStyle& style, ImDrawList* draw_list, const float pos_x, const float pos_y, const bool is_input, UINode* current_node, UINode** src_node, UINode** dst_node, std::string& selected_slot)
		{
			char name_buffer[512] = { '\0' };
			char text_buffer[512] = { '\0' };
			char namespace_buffer[512] = { '\0' };

			Memory::SmallVector<size_t, 8, Memory::Tag::Unknown> sorted_masks;
			sorted_masks.reserve(masks.size());
			for (size_t a = 0; a < masks.size(); a++)
				sorted_masks.push_back(a);

			std::sort(sorted_masks.begin(), sorted_masks.end(), [&](size_t a, size_t b) 
			{
				auto weight = [](char c)
				{
					switch (c)
					{
						case 'x':	return 1;
						case 'y':	return 2;
						case 'z':	return 3;
						case 'w':	return 4;
						default:	return 0;
					}
				};

				for (size_t i = 0; i < masks[a].length() && i < masks[b].length(); i++)
				{
					const auto wa = weight(masks[a][i]);
					const auto wb = weight(masks[b][i]);
					if (wa != wb)
						return wa < wb;
				}

				if (masks[a].length() != masks[b].length())
					return masks[a].length() < masks[b].length();

				return false;
			});

			RemoveNamespace(name_buffer, std::size(name_buffer), name);

			size_t mask_index = 0;
			float offset_y = pos_y;
			while (true)
			{
				const std::string& mask = mask_index < sorted_masks.size() ? masks[sorted_masks[mask_index]] : "";

				auto slot_type = type;
				if(!mask.empty())
					slot_type = UINode::GetMaskTypeHelper(mask, type);

				if (IsThickLink(slot_type))
				{
					if(mask.empty())
						ImFormatString(text_buffer, std::size(text_buffer), "(%s)", name_buffer);
					else
						ImFormatString(text_buffer, std::size(text_buffer), "(%s.%s)", name_buffer, mask.c_str());
				}
				else
				{
					if (mask.empty())
						ImFormatString(text_buffer, std::size(text_buffer), "%s", name_buffer);
					else
						ImFormatString(text_buffer, std::size(text_buffer), "%s.%s", name_buffer, mask.c_str());
				}

				ImVec2 slot_text_size = ImGui::CalcTextSize(text_buffer);
				float label_pos_x = is_input ? pos_x + NODE_WINDOW_PADDING.x : pos_x - slot_text_size.x - NODE_WINDOW_PADDING.x;

				ImGui::SetCursorScreenPos(ImVec2(label_pos_x, offset_y));
				ImGui::PushID(int(mask_index));

				const auto type_color = GetTypeColor(style, slot_type);
				ImGui::PushStyleColor(ImGuiCol_Text, type_color);
				ImGui::Selectable(text_buffer, false, ImGuiSelectableFlags_None, ImVec2(slot_text_size.x, 0.f));
				ImGui::PopStyleColor();

				if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem) && *src_node == nullptr)
				{
					ImGui::BeginTooltip();
					ImGui::PushTextWrapPos(35.0f * ImGui::GetFontSize());

					if (mask.empty())
						ImFormatString(text_buffer, std::size(text_buffer), "%s", name_buffer);
					else
						ImFormatString(text_buffer, std::size(text_buffer), "%s.%s", name_buffer, mask.c_str());

					GetNamespace(namespace_buffer, std::size(namespace_buffer), name);
					if (namespace_buffer[0] != '\0')
						ImGui::TextWrapped("%s " IMGUI_ICON_ARROW_RIGHT " %s", namespace_buffer, text_buffer);
					else
						ImGui::TextWrapped("%s", text_buffer);

					ImGui::Separator();

					const auto slot_name = magic_enum::enum_name(slot_type);
					ImGui::TextWrapped("%.*s", unsigned(slot_name.length()), slot_name.data());

					bool starting_link = ImGui::IsMouseDown(ImGuiMouseButton_Left) && !ImGui::IsMouseDragging(ImGuiMouseButton_Left);
					bool ending_link = ImGui::IsMouseReleased(ImGuiMouseButton_Left) && *dst_node != nullptr;
					if (starting_link || ending_link || ImGui::IsMouseClicked(1))
					{
						*src_node = current_node;

						if (mask.empty())
							ImFormatString(text_buffer, std::size(text_buffer), "%s", name.c_str());
						else
							ImFormatString(text_buffer, std::size(text_buffer), "%s.%s", name.c_str(), mask.c_str());

						selected_slot = text_buffer;
					}

					ImGui::PopTextWrapPos();
					ImGui::EndTooltip();
				}

				ImGui::PopID();

				// render the circle
				draw_list->AddCircleFilled(ImVec2(pos_x, offset_y + slot_text_size.y * 0.5f), NODE_SLOT_RADIUS, IM_COL32(150, 150, 150, 150));
				cached_positions[mask] = simd::vector2(pos_x, offset_y + slot_text_size.y * 0.5f);
				offset_y += slot_text_size.y + SLOT_PADDING.y;

				mask_index++;
				if (mask_index >= sorted_masks.size())
					break;
			}

			return offset_y - SLOT_PADDING.y - pos_y;
		}

		void UINode::UIConnectorInfo::AddMask(const std::string& mask)
		{
			auto found = std::find(masks.begin(), masks.end(), mask);
			if(found == masks.end())
				masks.push_back(mask);
		}

		void UINode::UIConnectorInfo::RemoveMask(const std::string& mask)
		{
			auto found = std::find(masks.begin(), masks.end(), mask);
			if(found != masks.end())
				masks.erase(found);
		}

		float UINode::UIConnectorInfo::CalculateWidth()
		{
			std::vector<std::string> slots;

			if(masks.empty())
				slots.emplace_back(name);
			else
			{
				for(const auto& mask : masks)
					slots.emplace_back(name + "." + mask);
			}

			float width = 0.f;
			for(const auto& slot : slots)
			{
				ImVec2 slot_text_size = ImGui::CalcTextSize(slot.c_str());
				if(slot_text_size.x > width)
					width = slot_text_size.x;
			}

			return width;
		}

		std::vector<std::string> UINode::UIConnectorInfo::AddMasksFromFormat(std::string format, std::string previous_mask)
		{
			if(!previous_mask.empty())
				RemoveMask(previous_mask);
			else
				ClearMasks();

			std::vector<std::string> new_masks;
			std::vector<std::wstring> splits;
			SplitString(splits, StringToWstring(format), L',');
			for(const auto& split : splits)
			{
				auto new_mask = WstringToString(TrimString(split));
				AddMask(new_mask);
				new_masks.push_back(new_mask);
			}
			return new_masks;
		}
		
		UINode::UINode(const UINode& in_node)
		{
			data = in_node.data;
			type_id = in_node.type_id;
			group = in_node.group;
			input_slots = in_node.input_slots;
			output_slots = in_node.output_slots;
			instance_name = in_node.instance_name;
			custom_parameter = in_node.custom_parameter;
			node_color = in_node.node_color;
			position = in_node.position;
			size = in_node.size;
			transform_pos = in_node.transform_pos;
			node_width = in_node.node_width;
			node_content_width = in_node.node_content_width;
			parameter_start_x = in_node.parameter_start_x;
			has_custom_ranges = in_node.has_custom_ranges;
			depth = in_node.depth;
			is_extension = false;
			is_group_region = in_node.is_group_region;
			title_color = in_node.title_color;
			parent_id = in_node.parent_id;
			parent_offset = in_node.parent_offset;
			description = in_node.description;

			for (const auto& param : in_node.parameters)
			{
				Renderer::DrawCalls::JsonDocument doc;
				Renderer::DrawCalls::JsonValue param_obj(rapidjson::kObjectType);
				param->Save(param_obj, doc.GetAllocator());

				AddParameter(param->GetNames(), param->GetMins(), param->GetMaxs(), param->GetType(), param_obj, param->GetData(), param->GetIndex());
			}
		}

		const std::string& UINode::GetTypeId() const
		{
			return type_id;
		}

		const std::string& UINode::GetInstanceName() const
		{
			return instance_name;
		}

		unsigned UINode::GetData()
		{
			return data;
		}

		void UINode::SetData(unsigned in_data)
		{
			data = in_data;
		}

		void UINode::SetGroup(unsigned in_group)
		{
			group = in_group;
		}

		void UINode::SetRenderCallback(const UIGraphRenderCallback& callback)
		{
			render_callback = callback;
		}

		void UINode::AddInputSlot(const std::string& name, const GraphType& type, const bool loop)
		{
			input_slots.emplace_back(name, type, loop);
		}

		void UINode::AddOutputSlot(const std::string& name, const GraphType& type, const bool loop)
		{
			output_slots.emplace_back(name, type, loop);
		}

		void UINode::AddParameter(
			const Memory::Vector<std::string, Memory::Tag::EffectGraphParameterInfos>& param_names,
			const Renderer::DrawCalls::ParamRanges& param_mins,
			const Renderer::DrawCalls::ParamRanges& param_maxs,
			GraphType type, const Renderer::DrawCalls::JsonValue& data_stream, 
			void* param_ptr, unsigned index)
		{
			auto new_param = CreateParameter(type, param_ptr, param_names, param_mins, param_maxs, index);
			new_param->Load(data_stream);
			new_param->SetUniqueID(parameters.size());
			has_custom_ranges = has_custom_ranges || new_param->HasLayout();
			parameters.push_back(std::move(new_param));
		}

		void UINode::AddCustomDynamicParameter(const std::string& value, const unsigned index)
		{
			Renderer::DrawCalls::JsonDocument doc;
			Renderer::DrawCalls::JsonValue param_obj(rapidjson::kObjectType);
			Renderer::DrawCalls::ParameterUtil::Save(param_obj, doc.GetAllocator(), value, "");

			auto new_param = CreateParameter(GraphType::Text, nullptr, index);
			new_param->Load(param_obj);
			new_param->SetUniqueID(parameters.size());
			has_custom_ranges = has_custom_ranges || new_param->HasLayout();
			parameters.push_back(std::move(new_param));
		}

		void UINode::SetInstanceName(const std::string& _instance_name)
		{
			instance_name = _instance_name;
		}

		void UINode::SetCustomParameter(const std::string& custom_param)
		{
			custom_parameter = custom_param;
		}

		void UINode::SavePositionAndSize()
		{
			saved_position = position;
			saved_parent_offset = parent_offset;
			saved_size = size;
		}

		void UINode::RevertPositionAndSize() 
		{ 
			parent_offset = saved_parent_offset;
			SetUIPosition(saved_position); 
			SetUISize(saved_size); 
		}

		void UINode::SetUIPosition(const simd::vector2& _position)
		{
			position = _position;
		}

		const simd::vector2& UINode::GetUIPosition() const
		{
			return position;
		}

		void UINode::SetUISize(const simd::vector2& _size)
		{
			size = _size;
		}

		const simd::vector2& UINode::GetUISize() const
		{
			return size;
		}

		void UINode::SetTransformPos(const simd::vector2& _transform_pos)
		{
			transform_pos = _transform_pos;
		}

		bool UINode::GetInputSlotPos(const std::string& input_slot, ImVec2& pos) const
		{
			std::string input_slot_name, input_slot_mask;
			UINode::GetConnectorNameAndMaskHelper(input_slot, input_slot_name, input_slot_mask);

			auto found = std::find_if(input_slots.begin(), input_slots.end(), [&](const UIConnectorInfo& connector)
			{
				return connector.name == input_slot_name;
			});

			if(found != input_slots.end())
			{
				auto found_pos = found->cached_positions.find(input_slot_mask);
				if(found_pos != found->cached_positions.end())
				{
					pos = ImVec2(found_pos->second.x, found_pos->second.y);
					return true;
				}
			}
			return false;
		}

		bool UINode::GetOutputSlotPos(const std::string& output_slot, ImVec2& pos) const
		{
			std::string output_slot_name, output_slot_mask;
			UINode::GetConnectorNameAndMaskHelper(output_slot, output_slot_name, output_slot_mask);

			auto found = std::find_if(output_slots.begin(), output_slots.end(), [&](const UIConnectorInfo& connector)
			{
				return connector.name == output_slot_name;
			});

			if(found != output_slots.end())
			{
				auto found_pos = found->cached_positions.find(output_slot_mask);
				if(found_pos != found->cached_positions.end())
				{
					pos = ImVec2(found_pos->second.x, found_pos->second.y);
					return true;
				}
			}
			return false;
		}

		unsigned int UINode::GetInputSlotColor(const UIGraphStyle& style, const std::string& input_slot) const
		{
			std::string input_slot_name, input_slot_mask;
			UINode::GetConnectorNameAndMaskHelper(input_slot, input_slot_name, input_slot_mask);

			auto found = std::find_if(input_slots.begin(), input_slots.end(), [&](const UIConnectorInfo& connector)
			{
				return connector.name == input_slot_name;
			});

			if (found != input_slots.end())
			{
				auto type = UINode::GetMaskTypeHelper(input_slot_mask, found->type);
				return GetTypeColor(style, type);
			}

			return 1;
		}

		unsigned int UINode::GetOutputSlotColor(const UIGraphStyle& style, const std::string& output_slot) const
		{
			std::string output_slot_name, output_slot_mask;
			UINode::GetConnectorNameAndMaskHelper(output_slot, output_slot_name, output_slot_mask);

			auto found = std::find_if(output_slots.begin(), output_slots.end(), [&](const UIConnectorInfo& connector)
			{
				return connector.name == output_slot_name;
			});

			if (found != output_slots.end())
			{
				auto type = UINode::GetMaskTypeHelper(output_slot_mask, found->type);
				return GetTypeColor(style, type);
			}

			return 1;
		}

		GraphType UINode::GetInputSlotType(const std::string& input_slot) const
		{
			std::string input_slot_name, input_slot_mask;
			UINode::GetConnectorNameAndMaskHelper(input_slot, input_slot_name, input_slot_mask);

			auto found = std::find_if(input_slots.begin(), input_slots.end(), [&](const UIConnectorInfo& connector)
				{
					return connector.name == input_slot_name;
				});
			return found == input_slots.end() ? GraphType::Invalid : found->type;
		}

		int UINode::GetInputSlotIndex(const std::string& input_slot) const
		{
			auto found = std::find_if(input_slots.begin(), input_slots.end(), [&](const UIConnectorInfo& connector)
			{
				return connector.name == input_slot;
			});
			if(found == input_slots.end())
				return -1;
			else
				return (int)(found - input_slots.begin());
		}

		int UINode::GetOutputSlotIndex(const std::string& output_slot) const
		{
			auto found = std::find_if(output_slots.begin(), output_slots.end(), [&](const UIConnectorInfo& connector)
			{
				return connector.name == output_slot;
			});
			if(found == output_slots.end())
				return -1;
			else
				return (int)(found - output_slots.begin());
		}

		simd::vector2 UINode::GetNodeNamePos() const
		{
			simd::vector2 scale_pos = position;
			ScalePositionHelper(scale_pos);
			ImVec2 node_rect_min = transform_pos + scale_pos;
			ImVec2 node_name_size = ImGui::CalcTextSize(instance_name.c_str());
			ImVec2 result = node_rect_min + ImVec2(node_width, 0.f) * HALF - node_name_size * HALF;
			result.y += NODE_WINDOW_PADDING.y;
			return simd::vector2(result.x, result.y);
		}

		void UINode::CalculateNodeWidth()
		{
			// get the length of the node name
			float mid_area_length = ImGui::CalcTextSize(instance_name.c_str()).x;
			if ((parameters.empty() == false || render_callback) && mid_area_length < PARAMETER_WIDTH)
				mid_area_length = PARAMETER_WIDTH;

			// get the longest input slot name length
			float input_slot_length = 0.f;
			for(size_t slot_idx = 0; slot_idx < input_slots.size(); ++slot_idx)
			{
				float length = input_slots[slot_idx].CalculateWidth();
				if(length > input_slot_length)
					input_slot_length = length;
			}

			// get the longest output slot name length
			float output_slot_length = 0.f;
			for(size_t slot_idx = 0; slot_idx < output_slots.size(); ++slot_idx)
			{
				float length = output_slots[slot_idx].CalculateWidth();
				if(length > output_slot_length)
					output_slot_length = length;
			}

			node_width = NODE_WINDOW_PADDING.x * 4.f + output_slot_length + mid_area_length + input_slot_length;
			node_content_width = mid_area_length;
			parameter_start_x = NODE_WINDOW_PADDING.x * 2.f + input_slot_length;
		}

		void UINode::CalculateInitialNodeRect()
		{
			auto scale_pos = position;
			ScalePositionHelper(scale_pos);
			node_rect.min = transform_pos + scale_pos;
			node_rect.max = node_rect.min;
		}

		bool UINode::IntersectsBox(const ImVec2& min, const ImVec2& max) const
		{
			return !(min.x > node_rect.max.x
				|| max.x < node_rect.min.x
				|| min.y > node_rect.max.y
				|| max.y < node_rect.min.y);
		}

		void UINode::GetConnectorNameAndMaskHelper(const std::string& connector_str, std::string& out_connector_name, std::string& out_connector_mask)
		{
			auto start_mask = connector_str.rfind(".");
			out_connector_name = (start_mask != std::string::npos) ? connector_str.substr(0, start_mask) : connector_str;
			out_connector_mask = (start_mask != std::string::npos) ? connector_str.substr(start_mask + 1) : std::string();
		}

		GraphType UINode::GetMaskTypeHelper(const std::string& mask, const GraphType& default_type)
		{
			size_t mask_len = mask.length();

			switch (default_type)
			{
				case GraphType::Float:
				case GraphType::Float2:
				case GraphType::Float3:
				case GraphType::Float4:
					switch (mask_len)
					{
						case 4:	return GraphType::Float4;
						case 3:	return GraphType::Float3;
						case 2:	return GraphType::Float2;
						case 1:	return GraphType::Float;
						default: return default_type;
					}
				case GraphType::Int:
				case GraphType::Int2:
				case GraphType::Int3:
				case GraphType::Int4:
					switch (mask_len)
					{
						case 4:	return GraphType::Int4;
						case 3:	return GraphType::Int3;
						case 2:	return GraphType::Int2;
						case 1:	return GraphType::Int;
						default: return default_type;
					}
				default: return default_type;
			}
		}

		// NODEGRAPH_TODO : This function definitely needs refactoring
		void UINode::OnRender(const UIGraphStyle& style, ImDrawList* draw_list, std::vector<UINode*>& selected_nodes, UINode** input_node, UINode** output_node, std::string& selected_node_input, std::string& selected_node_output, UIParameter** changed_parameter, bool& node_active, bool& on_group_changed, bool refresh_selection)
		{
			char buffer[512] = { '\0' };
			char buffer2[512] = { '\0' };
			ImGui::PushID(instance_name.c_str());

			simd::vector2 scale_pos = position;
			ScalePositionHelper(scale_pos);
			ImVec2 node_rect_min = transform_pos + scale_pos;

			// Begin display node
			draw_list->ChannelsSetCurrent(depth*2 + 1);
			simd::vector2 node_name_pos = GetNodeNamePos();
			ImGui::SetCursorScreenPos(ImVec2(node_name_pos.x, node_name_pos.y));
			ImGui::BeginGroup();
			ImGui::Text("%s", instance_name.c_str());

			// Get the node size and whether any of the widgets are being used
			ImVec2 node_size;
			node_size.x = node_width;
			node_size.y = ImGui::CalcTextSize(instance_name.c_str()).y + NODE_WINDOW_PADDING.y;
			ImVec2 node_rect_max = node_rect_min + node_size;

			// Display the input slots
			float slot_y = node_rect_max.y;
			for(size_t slot_idx = 0; slot_idx < input_slots.size(); ++slot_idx)
			{	
				slot_y += SLOT_PADDING.y;
				slot_y += input_slots[slot_idx].OnRender(style, draw_list, node_rect_min.x, slot_y, true, this, input_node, output_node, selected_node_input);
			}

			// Display the output slots
			slot_y = node_rect_max.y;
			for(size_t slot_idx = 0; slot_idx < output_slots.size(); ++slot_idx)
			{
				slot_y += SLOT_PADDING.y;
				slot_y += output_slots[slot_idx].OnRender(style, draw_list, node_rect_max.x, slot_y, false, this, output_node, input_node, selected_node_output);
			}

			// For updating the selected nodes
			auto add_selected = [&]()
			{
				auto found_selected = std::find(selected_nodes.begin(), selected_nodes.end(), this);
				if(found_selected == selected_nodes.end())
				{
					if(refresh_selection)
						selected_nodes.clear();
					selected_nodes.push_back(this);
				}
				else if(!refresh_selection)
					selected_nodes.erase(found_selected);
			};

			// Display the parameters
			ImGui::SetCursorScreenPos(ImVec2(node_rect_min.x + parameter_start_x, node_rect_max.y + SLOT_PADDING.y));
			ImGui::BeginGroup();

			bool has_icon = false;
			bool has_multiple_icons = false;
			bool has_priority = false;
			int priority = -INT_MAX;
			std::string_view icon = "";
			for (const auto& param : parameters)
			{
				if (!param->HasEditor())
					continue;

				if (const auto p_icon = param->GetIcon())
				{
					if (has_icon && icon != *p_icon)
						has_multiple_icons = true;
					else
						icon = *p_icon;

					has_icon = true;
				}

				priority = std::max(priority, param->GetPriority());
				has_priority = true;
			}

			for (const auto& param : parameters)
			{
				if (!param->HasEditor())
					continue;

				if (param->OnRender(node_content_width, parameters.size(), has_multiple_icons))
				{
					if (changed_parameter)
						*changed_parameter = param.get();

					selected_nodes.clear();
					add_selected();
				}
			}

			if (render_callback)
			{
				ImGui::BeginGroup();

				ImGui::PushItemWidth(PARAMETER_WIDTH);
				if (render_callback())
				{
					selected_nodes.clear();
					add_selected();
				}

				ImGui::PopItemWidth();

				ImGui::EndGroup();
			}

			ImGui::EndGroup(); // Parameter group
			ImGui::EndGroup(); // Node group

			// Display node box
			draw_list->ChannelsSetCurrent(depth*2);
			node_size.y = ImGui::GetItemRectSize().y + NODE_WINDOW_PADDING.y * 2.f;
			node_rect_max = node_rect_min + node_size;

			bool is_hovered = false;
			ImGui::SetCursorScreenPos(node_rect_min);
			ImGui::InvisibleButton("node", node_size);
			if(ImGui::IsItemHovered())
			{
				is_hovered = true;
				if(ImGui::IsMouseClicked(1))
				{
					selected_nodes.clear();
					selected_nodes.push_back(this);
				}
				else if(ImGui::IsMouseClicked(0))
					add_selected();
			}

			if(ImGui::IsItemActive())
				node_active = true;

			const auto is_grouped = GetParentId() != (unsigned)-1;
			const auto is_selected = std::find(selected_nodes.begin(), selected_nodes.end(), this) != selected_nodes.end();
			const auto node_bg_color = (is_hovered || is_selected) ? IM_COL32(node_color.x + 15, node_color.y + 15, node_color.z + 15, node_color.w) : IM_COL32(node_color.x, node_color.y, node_color.z, node_color.w);
			const auto node_border_color = is_selected ? IM_COL32_WHITE : is_grouped ? IM_COL32(DEFAULT_GROUP_BORDER_COLOR.x, DEFAULT_GROUP_BORDER_COLOR.y, DEFAULT_GROUP_BORDER_COLOR.z, DEFAULT_GROUP_BORDER_COLOR.w) : IM_COL32(150, 150, 150, 255);
			draw_list->AddRectFilled(node_rect_min, node_rect_max, node_bg_color, 4.0f);

			if (title_color.Value.w > 0.0f)
				draw_list->AddRectFilled(node_rect_min, ImVec2(node_rect_max.x, node_rect_min.y + ImGui::GetFrameHeight()), title_color, 4.0f, ImDrawFlags_RoundCornersTop);

			draw_list->AddRect(node_rect_min, node_rect_max, node_border_color, 4.0f, 15, (is_selected || is_grouped) ? 3.5f : 1.f);

			if(is_highlighted_prev || is_highlighted_after)
				draw_list->AddRect(node_rect_min, node_rect_max, GetHighlightColor(is_highlighted_prev, is_highlighted_after, is_highlighted_selected), 4.0f, 15, 3.5f);

			node_rect.min = simd::vector2(node_rect_min.x, node_rect_min.y);
			node_rect.max = simd::vector2(node_rect_max.x, node_rect_max.y);

			ImGui::PopID();

			// Display the custom parameter text just above the node box
			if(!custom_parameter.empty())
			{
				if (has_priority)
					ImFormatString(buffer2, std::size(buffer2), "%s (%d)", custom_parameter.c_str(), priority);
				else
					ImFormatString(buffer2, std::size(buffer2), "%s", custom_parameter.c_str());

				if(!has_multiple_icons && icon.length() > 0)
					ImFormatString(buffer, std::size(buffer), "%.*s %s", unsigned(icon.length()), icon.data(), buffer2);
				else
					ImFormatString(buffer, std::size(buffer), "%s", buffer2);

				const auto label_size = ImGui::CalcTextSize(buffer);
				const auto pos = ImVec2(node_rect.min.x + NODE_SLOT_RADIUS, node_rect.min.y - (NODE_SLOT_RADIUS * 6.5f));
				const auto frame_bb = ImRect(pos, pos + label_size + ImVec2(NODE_SLOT_RADIUS * 2, NODE_SLOT_RADIUS * 2));

				draw_list->ChannelsSetCurrent(depth * 2 + 1);
				draw_list->AddRectFilled(frame_bb.Min, frame_bb.Max, node_bg_color, 4.0f, ImDrawFlags_RoundCornersAll);
				draw_list->AddRect(frame_bb.Min, frame_bb.Max, node_border_color, 4.0f, ImDrawFlags_RoundCornersAll, is_selected ? 3.5f : 1.f);

				ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0x27, 0xAE, 0x60, 0xFF));
				ImGui::RenderTextClipped(frame_bb.Min + ImVec2(NODE_SLOT_RADIUS, NODE_SLOT_RADIUS), frame_bb.Max - ImVec2(NODE_SLOT_RADIUS, NODE_SLOT_RADIUS), buffer, ImGui::FindRenderedTextEnd(buffer), &label_size);
				ImGui::PopStyleColor();
			}
		}

		UIExtensionPoint::UIExtensionPoint(const unsigned id, const unsigned group) : UINode("", group), id(id), selected_split_index(-1), selected_slot_index(-1), selected_slot_is_input(false)
		{
			std::stringstream stream;
			stream << id;
			instance_name = stream.str();
			is_extension = true;
			//node_color = simd::vector4(100, 120, 100, 255);
		}

		void UIExtensionPoint::SetOutputParameter(const UIParameter* parameter)
		{
			// update the out selections (right side)
			if (parameter)
			{
				// clear the output slots when the output parameter is changed
				if (ext_out_parameter && GetEnumValueString(ext_out_parameter.get()) != GetEnumValueString(parameter))
					input_slots.clear();

				Renderer::DrawCalls::JsonDocument doc;
				Renderer::DrawCalls::JsonValue param_obj(rapidjson::kObjectType);
				parameter->Save(param_obj, doc.GetAllocator());

				ext_out_parameter = CreateParameter(GraphType::Enum, nullptr, parameter->GetNames(), parameter->GetMins(), parameter->GetMaxs(), parameter->GetIndex());
				ext_out_parameter->Load(param_obj);
				ext_out_parameter->SetUniqueID(1);

				// update the split selections
				split_selections.clear();

				const auto& elements = ext_out_parameter->GetNames();
				const auto min_value = ext_in_parameter ? GetEnumValue(ext_in_parameter.get()) : 0;
				const auto max_value = GetEnumValue(ext_out_parameter.get());

				for (size_t a = 0; a < elements.size(); a++)
					if (a > min_value && a < max_value && IsEnumValueEnabled(ext_out_parameter.get(), a))
						split_selections.push_back(elements[a]);
			}
		}

		void UIExtensionPoint::SetInputParameter(const UIParameter* parameter)
		{
			// update the in selections (left side)
			if(parameter)
			{
				// clear the input slots when the input parameter is changed
				if (ext_in_parameter && GetEnumValueString(ext_in_parameter.get()) != GetEnumValueString(parameter))
					input_slots.clear();

				Renderer::DrawCalls::JsonDocument doc;
				Renderer::DrawCalls::JsonValue param_obj(rapidjson::kObjectType);
				parameter->Save(param_obj, doc.GetAllocator());

				ext_in_parameter = CreateParameter(GraphType::Enum, nullptr, parameter->GetNames(), parameter->GetMins(), parameter->GetMaxs(), parameter->GetIndex());
				ext_in_parameter->Load(param_obj);
				ext_in_parameter->SetUniqueID(0);

				// update the split selections
				split_selections.clear();

				const auto& elements = ext_in_parameter->GetNames();
				const auto min_value = GetEnumValue(ext_in_parameter.get());
				const auto max_value = ext_out_parameter ? GetEnumValue(ext_out_parameter.get()) : (elements.size() - 1);

				for (size_t a = 0; a < elements.size(); a++)
					if (a > min_value && a < max_value && IsEnumValueEnabled(ext_in_parameter.get(), a))
						split_selections.push_back(elements[a]);
			}
		}

		std::string UIExtensionPoint::GetOutputParameterValue() const
		{
			return GetEnumValueString(ext_out_parameter.get());
		}

		std::string UIExtensionPoint::GetInputParameterValue() const
		{
			return GetEnumValueString(ext_in_parameter.get());
		}

		void UIExtensionPoint::OnRender(const UIGraphStyle& style, ImDrawList* draw_list, std::vector<UINode*>& selected_nodes, UINode** input_node, UINode** output_node, std::string& selected_node_input, std::string& selected_node_output, UIParameter** changed_parameter, bool& node_active, bool& on_group_changed, bool refresh_selection)
		{
			char buffer0[512] = { '\0' };
			char buffer1[512] = { '\0' };
			char buffer2[512] = { '\0' };
			ImGui::PushID(instance_name.c_str());

			simd::vector2 scale_pos = position;
			ScalePositionHelper(scale_pos);
			ImVec2 node_rect_min = transform_pos + scale_pos;

			// Begin display node
			draw_list->ChannelsSetCurrent(1);
			ImGui::SetCursorScreenPos(node_rect_min + NODE_WINDOW_PADDING);
			ImGui::BeginGroup();
			
			const float split_button_w = node_width - ((ext_in_parameter ? CONNECTOR_PARAMETER_WIDTH : ImGui::CalcTextSize("Begin").x) + (ext_out_parameter ? CONNECTOR_PARAMETER_WIDTH : ImGui::CalcTextSize("End").x) + (4.0f * NODE_WINDOW_PADDING.x));

			// render the input parameter (left side)
			if (ext_in_parameter && ext_in_parameter->HasEditor())
			{
				const auto prev = GetEnumValueString(ext_in_parameter.get());
				if (ext_in_parameter->OnRender(CONNECTOR_PARAMETER_WIDTH))
				{
					auto next = GetEnumValueString(ext_in_parameter.get());
					if (prev != next)
					{
						previous_extension_point = prev;
						changed_extension_point = next;
					}
				}
			}
			else
			{
				ImGui::Text("Begin");
			}

			// render the split button
			ImGui::SameLine(0.0f, NODE_WINDOW_PADDING.x);
			if (split_selections.empty())
				ImGui::Dummy(ImVec2(split_button_w, ImGui::GetFrameHeight()));
			else if (ImGui::Button(" " IMGUI_ICON_PLAY " ##SplitButton", ImVec2(split_button_w, ImGui::GetFrameHeight())))
				ImGui::OpenPopup("split_context_menu");

			// render the split context menu
			if(ImGui::BeginPopup("split_context_menu"))
			{
				for(size_t i = 0; i < split_selections.size(); ++i)
					if(ImGui::MenuItem(split_selections[i].c_str()))
						selected_split_index = (int)i;

				ImGui::EndPopup();
			}

			// render the output parameters (right side)
			ImGui::SameLine(0.0f, NODE_WINDOW_PADDING.x);
			if (ext_out_parameter && ext_out_parameter->HasEditor())
			{
				const auto prev = GetEnumValueString(ext_out_parameter.get());
				if (ext_out_parameter->OnRender(CONNECTOR_PARAMETER_WIDTH))
				{
					auto next = GetEnumValueString(ext_out_parameter.get());
					if (prev != next)
					{
						previous_extension_point = prev;
						changed_extension_point = next;
					}
				}
			}
			else
			{
				ImGui::Text("End");
			}

			// Get the initial node size
			ImVec2 node_size;
			node_size.x = node_width;
			node_size.y = ImGui::CalcTextSize(instance_name.c_str()).y + NODE_WINDOW_PADDING.y;
			ImVec2 node_rect_max = node_rect_min + node_size;

			// Dislay the input slots
			float slot_y = node_rect_max.y;
			for(size_t slot_idx = 0; slot_idx < input_slots.size(); ++slot_idx)
			{
				slot_y += SLOT_PADDING.y;
				slot_y += input_slots[slot_idx].OnRender(style, draw_list, node_rect_min.x, slot_y, true, this, input_node, output_node, selected_node_input);
			}

			// Display the add slots button for the input slots
			slot_y += SLOT_PADDING.y;
			if(input_slots.size())
			{
				ImGui::SetCursorScreenPos(ImVec2(node_rect_min.x + NODE_WINDOW_PADDING.x, slot_y));
				if(ImGui::Button(" " IMGUI_ICON_ADD " ##AddInputSlot"))
				{
					ImGui::OpenPopup("slot_context_menu");
					selected_slot_is_input = true;
					parameter_value = GetEnumValueString(ext_in_parameter.get());
				}
			}

			// Dislay the output slots
			slot_y = node_rect_max.y;
			for(size_t slot_idx = 0; slot_idx < output_slots.size(); ++slot_idx)
			{
				slot_y += SLOT_PADDING.y;
				slot_y += output_slots[slot_idx].OnRender(style, draw_list, node_rect_max.x, slot_y, false, this, output_node, input_node, selected_node_output);
			}

			// Display the add slots button for the output slots
			slot_y += SLOT_PADDING.y;
			if(output_slots.size())
			{
				ImGui::SetCursorScreenPos(ImVec2(node_rect_max.x - NODE_WINDOW_PADDING.x - (ImGui::CalcTextSize(" " IMGUI_ICON_ADD " ").x + (2.0f * ImGui::GetStyle().FramePadding.x)), slot_y));
				if(ImGui::Button(" " IMGUI_ICON_ADD " ##AddOutputSlot"))
				{
					ImGui::OpenPopup("slot_context_menu");
					selected_slot_is_input = false;
					parameter_value = GetEnumValueString(ext_out_parameter.get());
				}
			}

			// render the slot context menu
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
			if(ImGui::BeginPopup("slot_context_menu"))
			{
				Memory::SmallVector<size_t, 32, Memory::Tag::Unknown> sorted_slots;
				sorted_slots.reserve(slot_selections.size());
				for (size_t a = 0; a < slot_selections.size(); a++)
					sorted_slots.push_back(a);

				std::sort(sorted_slots.begin(), sorted_slots.end(), [&](size_t a, size_t b) {return slot_selections[a] < slot_selections[b]; });
				
				Memory::Span<const UIConnectorInfo> existing_slots = selected_slot_is_input ? input_slots : output_slots;
				if (existing_slots.size() > 0) // Don't show options to add extension points that we already have
				{
					Memory::SmallVector<size_t, 32, Memory::Tag::Unknown> sorted_existing;
					sorted_existing.reserve(existing_slots.size());
					for (size_t a = 0; a < existing_slots.size(); a++)
						sorted_existing.push_back(a);

					std::sort(sorted_existing.begin(), sorted_existing.end(), [&](size_t a, size_t b) { return existing_slots[a].name < existing_slots[b].name; });
					for (size_t a = 0, b = 0; a < sorted_slots.size() && b < sorted_existing.size();)
					{
						const auto& an = slot_selections[sorted_slots[a]];
						const auto& bn = existing_slots[sorted_existing[b]].name;

						if (an < bn)
							a++;
						else if (an > bn)
							b++;
						else
							sorted_slots.erase(sorted_slots.begin() + a);
					}
				}

				if (sorted_slots.size() > 0)
				{
					buffer0[0] = { '\0' };
					bool in_menu = false;
					bool visible = true;
					bool has_menus = false;

					GetNamespace(buffer1, std::size(buffer1), slot_selections[sorted_slots[0]]);
					for (size_t a = 1; a < sorted_slots.size(); a++)
					{
						GetNamespace(buffer2, std::size(buffer2), slot_selections[sorted_slots[a]]);
						if (strcmp(buffer1, buffer2) != 0)
						{
							has_menus = true;
							break;
						}
					}

					for (size_t a = 0; a < sorted_slots.size(); a++)
					{
						GetNamespace(buffer1, std::size(buffer1), slot_selections[sorted_slots[a]]);

						if (strcmp(buffer0, buffer1) != 0)
						{
							if (in_menu)
								ImGui::EndMenu();

							in_menu = false;
							visible = true;
							if (buffer1[0] != '\0')
							{
								if (has_menus)
								{
									ImFormatString(buffer2, std::size(buffer2), "%s##NS%u", buffer1, unsigned(sorted_slots[a]));
									visible = ImGui::BeginMenu(buffer2);
									in_menu = visible;
								}

								memcpy(buffer0, buffer1, std::min(std::size(buffer0), std::size(buffer1)));
							}
						}

						if (!visible)
							continue;

						RemoveNamespace(buffer1, std::size(buffer1), slot_selections[sorted_slots[a]]);
						ImFormatString(buffer2, std::size(buffer2), "%s##I%u", buffer1, unsigned(sorted_slots[a]));
						if (ImGui::MenuItem(buffer2))
							selected_slot_index = (int)sorted_slots[a];
					}

					if (in_menu)
						ImGui::EndMenu();
				}

				ImGui::EndPopup();
			}
			ImGui::PopStyleVar();

			// For updating the selected nodes
			auto add_selected = [&]()
			{
				auto found_selected = std::find(selected_nodes.begin(), selected_nodes.end(), this);
				if(found_selected == selected_nodes.end())
				{
					if(refresh_selection)
						selected_nodes.clear();
					selected_nodes.push_back(this);
				}
				else if(!refresh_selection)
					selected_nodes.erase(found_selected);
			};

			ImGui::EndGroup();

			// Display node box
			draw_list->ChannelsSetCurrent(0);
			node_size.y = ImGui::GetItemRectSize().y + NODE_WINDOW_PADDING.y * 2.f;
			node_rect_max = node_rect_min + node_size;

			bool is_hovered = false;
			ImGui::SetCursorScreenPos(node_rect_min);
			ImGui::InvisibleButton("node", node_size);
			if(ImGui::IsItemHovered())
			{
				is_hovered = true;
				if(ImGui::IsMouseClicked(1))
				{
					selected_nodes.clear();
					selected_nodes.push_back(this);
				}
				else if(ImGui::IsMouseClicked(0))
					add_selected();
			}

			if(ImGui::IsItemActive())
				node_active = true;

			bool is_selected = std::find(selected_nodes.begin(), selected_nodes.end(), this) != selected_nodes.end();
			ImU32 node_bg_color = (is_hovered || is_selected) ? IM_COL32(node_color.x + 15, node_color.y + 15, node_color.z + 15, node_color.w) : IM_COL32(node_color.x, node_color.y, node_color.z, node_color.w);
			draw_list->AddRectFilled(node_rect_min, node_rect_max, node_bg_color, 4.0f);
			draw_list->AddRect(node_rect_min, node_rect_max, is_selected ? IM_COL32(255, 255, 255, 255) : IM_COL32(150, 150, 150, 255), 4.0f, 15, is_selected ? 3.5f : 1.f);

			if (highlight_flow)
				draw_list->AddRect(node_rect_min, node_rect_max, GetHighlightColor(is_highlighted_prev, is_highlighted_after, is_highlighted_selected), 4.0f, 15, 3.5f);

			node_rect.min = simd::vector2(node_rect_min.x, node_rect_min.y);
			node_rect.max = simd::vector2(node_rect_max.x, node_rect_max.y);

			ImGui::PopID();
		}

		void UIExtensionPoint::CalculateNodeWidth()
		{
			char buffer[512] = { '\0' };

			// get the parameters length
			float param_length = NODE_WINDOW_PADDING.x;
			if(ext_in_parameter)
				param_length += CONNECTOR_PARAMETER_WIDTH + NODE_WINDOW_PADDING.x;
			else
				param_length += ImGui::CalcTextSize("Begin").x + NODE_WINDOW_PADDING.x;		

			param_length += ImGui::CalcTextSize(" " IMGUI_ICON_PLAY " ").x + NODE_WINDOW_PADDING.x;

			if(ext_out_parameter)
				param_length += CONNECTOR_PARAMETER_WIDTH + NODE_WINDOW_PADDING.x;
			else			
				param_length += ImGui::CalcTextSize("End").x + NODE_WINDOW_PADDING.x;

			// get the longest input slot name length
			float input_slot_length = 0.f;
			for(size_t slot_idx = 0; slot_idx < input_slots.size(); ++slot_idx)
			{
				RemoveNamespace(buffer, std::size(buffer), input_slots[slot_idx].name);
				float length = ImGui::CalcTextSize(buffer).x;
				if(length > input_slot_length)
					input_slot_length = length;
			}

			// get the longest output slot name length
			float output_slot_length = 0.f;
			for(size_t slot_idx = 0; slot_idx < output_slots.size(); ++slot_idx)
			{
				RemoveNamespace(buffer, std::size(buffer), output_slots[slot_idx].name);
				float length = ImGui::CalcTextSize(buffer).x;
				if(length > output_slot_length)
					output_slot_length = length;
			}

			node_width = NODE_WINDOW_PADDING.x * 4.f + output_slot_length  + input_slot_length;
			node_content_width = 0.0f;
			if(param_length > node_width)
				node_width = param_length;
		}

		UIGroupRegion::UIGroupRegion(const unsigned index, const unsigned group, const std::vector<UINode*>& nodes) : UINode(std::string(GroupBoxNodeName), group), id(index)
		{
			// Consider using type enum instead
			is_group_region = true;

			SetDescription("Description");

			if (nodes.empty())
			{
				position = simd::vector2(0.f);
				local_space_size.min = simd::vector2(0.f);
				local_space_size.max = DEFAULT_GROUP_BOX_SIZE;
				UINode::SetUISize(local_space_size.max - local_space_size.min);
			}
			else
			{
				position = simd::vector2(FLT_MAX, FLT_MAX);
				local_space_size.min = simd::vector2(FLT_MAX, FLT_MAX);
				local_space_size.max = simd::vector2(-FLT_MAX, -FLT_MAX);
				AddChildren(nodes);
			}
		}

		UIGroupRegion::UIGroupRegion(const UIGroupRegion& other) : UINode(other)
		{
			id = other.id;
			children = other.children;
			local_space_size = other.local_space_size;
			is_loop_box = other.is_loop_box;
			modified_input_slots = other.modified_input_slots;
			modified_output_slots = other.modified_output_slots;
		}

		UIGroupRegion::~UIGroupRegion()
		{
			for (auto child : children)
			{
				child->SetParentOffset(0.f);
				child->SetParentId(-1);
			}
		}

		void UIGroupRegion::OnRender(const UIGraphStyle& style, ImDrawList* draw_list, std::vector<UINode*>& selected_nodes, UINode** input_node, UINode** output_node, std::string& selected_node_input, std::string& selected_node_output, UIParameter** changed_parameter, bool& node_active, bool& on_group_changed, bool refresh_selection)
		{
			std::string str_id = "##GroupRegion" + std::to_string(id);
			draw_list->ChannelsSetCurrent(1);
			ImGui::PushID(str_id.c_str());
			ImGui::BeginGroup();

			auto scale_pos = local_space_size.min;
			ScalePositionHelper(scale_pos);
			node_rect.min = transform_pos + scale_pos - NODE_SLOT_RADIUS * 5.f;
			node_rect.min.x -= NODE_SLOT_RADIUS * 5.f;
			node_rect.min.y -= NODE_SLOT_RADIUS * 5.f;

			scale_pos = local_space_size.max;
			ScalePositionHelper(scale_pos);
			node_rect.max = transform_pos + scale_pos + NODE_SLOT_RADIUS * 5.f;
			node_rect.max.x += NODE_SLOT_RADIUS * 5.f;

			const auto disable_slots = num_external_links > 0;
			const auto loop_slot_name = std::string(LoopCountInputName);

			ImGui::SetCursorScreenPos(ImVec2(node_rect.min) + NODE_WINDOW_PADDING);
			if (disable_slots) ImGui::BeginDisabled(true);
			if (ImGui::Checkbox("Loop", &is_loop_box))
			{
				if (is_loop_box)
				{
					modified_input_slots.emplace(modified_input_slots.begin(), loop_slot_name, GraphType::Int, false);
				}
				else
				{
					auto slot = modified_input_slots.begin();
					if (slot->name == loop_slot_name)
						to_remove_input_slot = 0;
				}
				on_group_changed = true;
			}
			if (disable_slots)
			{
				ImGuiEx::SetItemTooltip("Cannot set box as looping. Remove external links first.");
				ImGui::EndDisabled();
			}

			auto slots_start_y = ImGui::GetCursorScreenPos().y + ImGui::GetFrameHeight();

			// Used for reversing the linking splines if mouse is inside the box
			const auto mouse_pos = ImGui::GetMousePos();
			const auto prev_mouse_inside = is_mouse_inside;
			if (enable_group_link_switching)
				is_mouse_inside = UINode::IntersectsBox(mouse_pos, mouse_pos);
			if (prev_mouse_inside != is_mouse_inside)
			{
				if (*output_node == this)
				{
					*input_node = this;
					*output_node = nullptr;	
					selected_node_input = selected_node_output;
					selected_node_output.clear();
				}
				else if (*input_node == this)
				{
					*output_node = this;
					*input_node = nullptr;
					selected_node_output = selected_node_input;
					selected_node_input.clear();
				}
			}
			UINode** new_input = (is_mouse_inside) ? output_node : input_node;
			UINode** new_output = (is_mouse_inside) ? input_node : output_node;
			std::string& new_input_slot = (is_mouse_inside) ? selected_node_output : selected_node_input;
			std::string& new_output_slot = (is_mouse_inside) ? selected_node_input : selected_node_output;

			// Display the input slots
			float slot_y = slots_start_y;
			for (size_t slot_idx = 0; slot_idx < input_slots.size(); ++slot_idx)
			{
				slot_y += SLOT_PADDING.y;
				slot_y += input_slots[slot_idx].OnRender(style, draw_list, node_rect.min.x, slot_y, true, this, new_input, new_output, new_input_slot);
				if (is_loop_box && slot_idx == 0)
					slots_start_y = slot_y;
				if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(1) && input_slots[slot_idx].name != loop_slot_name)
					to_remove_input_slot = int(slot_idx);
			}

			slot_y += SLOT_PADDING.y;
			ImGui::SetCursorScreenPos(ImVec2(node_rect.min.x + NODE_WINDOW_PADDING.x, slot_y));
			if (disable_slots) ImGui::BeginDisabled(true);
			if (ImGui::Button(" " IMGUI_ICON_ADD " ##AddInputSlot"))
			{
				ImGui::OpenPopup("group_links_context_menu");
				selected_slot_is_input = true;
			}
			if (disable_slots)
			{
				ImGuiEx::SetItemTooltip("Cannot add slots. Remove external links first.");
				ImGui::EndDisabled();
			}

			// Display the output slots
			slot_y = slots_start_y;
			for (size_t slot_idx = 0; slot_idx < output_slots.size(); ++slot_idx)
			{
				slot_y += SLOT_PADDING.y;
				slot_y += output_slots[slot_idx].OnRender(style, draw_list, node_rect.max.x, slot_y, false, this, new_output, new_input, new_output_slot);
				if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(1))
					to_remove_output_slot = int(slot_idx);
			}

			slot_y += SLOT_PADDING.y;
			ImGui::SetCursorScreenPos(ImVec2(node_rect.max.x - NODE_WINDOW_PADDING.x - (ImGui::CalcTextSize(" " IMGUI_ICON_ADD " ").x + (2.0f * ImGui::GetStyle().FramePadding.x)), slot_y));
			if (disable_slots) ImGui::BeginDisabled(true);
			if (ImGui::Button(" " IMGUI_ICON_ADD " ##AddOutputSlot"))
			{
				ImGui::OpenPopup("group_links_context_menu");
				selected_slot_is_input = false;
			}
			if (disable_slots)
			{
				ImGuiEx::SetItemTooltip("Cannot add slots. Remove external links first.");
				ImGui::EndDisabled();
			}

			// render the input/output slot context menu
			if (ImGui::BeginPopup("group_links_context_menu"))
			{
				char buffer[512] = { '\0' };

				ImGui::SetNextItemWidth(PARAMETER_WIDTH);
				ImFormatString(buffer, std::size(buffer), "%s", current_link_name.c_str());
				if (ImGui::InputTextWithHint("##link_name", "Slot name", buffer, std::size(buffer), ImGuiInputTextFlags_CharsNoBlank))
					current_link_name = buffer;

				ImGui::SameLine();
				ImGui::SetNextItemWidth(PARAMETER_WIDTH * 0.5f);
				ImFormatString(buffer, std::size(buffer), "%s", FileReader::FFXReader::GetGraphTypeName(current_link_type).c_str());
				if (ImGui::BeginCombo("##GroupLinkTypeCombo", buffer))
				{
					for (const auto type : GroupLinkTypes)
					{
						ImFormatString(buffer, std::size(buffer), "%s##GroupLinkType", type.c_str());
						if (ImGui::Selectable(buffer, FileReader::FFXReader::GetGraphType(type) == current_link_type))
							current_link_type = FileReader::FFXReader::GetGraphType(type);
					}
					ImGui::EndCombo();
				}

				auto slot_name_used = [&](const std::string& name) -> bool
				{
					for (const auto slot : input_slots)
						if (slot.name == current_link_name)
							return true;
					for (const auto slot : output_slots)
						if (slot.name == current_link_name)
							return true;
					return false;
				};

				auto on_links_changed = [&]()
				{
					std::vector<UIConnectorInfo> new_inputs;
					std::vector<UIConnectorInfo> new_outputs;
					auto is_priority = [&](const UIConnectorInfo& slot) -> bool { return (slot.loop || slot.name == loop_slot_name); };
					for (const auto& slot : modified_input_slots)
					{
						if (is_priority(slot))
							new_inputs.push_back(slot);
					}
					for (const auto& slot : modified_input_slots)
					{
						if (!is_priority(slot))
							new_inputs.push_back(slot);
					}
					for (const auto& slot : modified_output_slots)
					{
						if (is_priority(slot))
							new_outputs.push_back(slot);
					}
					for (const auto& slot : modified_output_slots)
					{
						if (!is_priority(slot))
							new_outputs.push_back(slot);
					}
					modified_input_slots.clear();
					modified_output_slots.clear();
					modified_input_slots = new_inputs;
					modified_output_slots = new_outputs;
					on_group_changed = true;
					ImGui::CloseCurrentPopup();
					show_error = false;
				};

				ImGui::SameLine();
				if (!is_loop_box)
					ImGui::BeginDisabled(true);
				if (ImGui::Button(" " IMGUI_ICON_INFINITY " ##AddSlotLoop") && !current_link_name.empty())
				{		
					if (!slot_name_used(current_link_name))
					{
						// Adding a link as looping means it should be added on both input and output slots as they need to be symmetric
						modified_input_slots.emplace_back(current_link_name, current_link_type, true);
						modified_output_slots.emplace_back(current_link_name, current_link_type, true);
						on_links_changed();
					}
					else
						show_error = true;
				}
				ImGuiEx::SetItemTooltip("Add link for looping");
				if (!is_loop_box)
					ImGui::EndDisabled();

				ImGui::SameLine();
				if (ImGui::Button(" " IMGUI_ICON_ADD " ##AddSlot") && !current_link_name.empty())
				{
					if (!slot_name_used(current_link_name))
					{
						auto& slots = selected_slot_is_input ? modified_input_slots : modified_output_slots;
						slots.emplace_back(current_link_name, current_link_type, false);
						on_links_changed();
					}
					else
						show_error = true;
				}
				ImGuiEx::SetItemTooltip("Add link");

				if (show_error)
					ImGui::TextColored(ImVec4(1.f, 0.f, 0.f, 1.f), "Slot name already used.");

				ImGui::EndPopup();
			}

			// For updating the selected nodes
			auto add_selected = [&]()
			{
				auto found_selected = std::find(selected_nodes.begin(), selected_nodes.end(), this);
				if (found_selected == selected_nodes.end())
				{
					if (refresh_selection)
						selected_nodes.clear();
					selected_nodes.push_back(this);
				}
				else if (!refresh_selection)
					selected_nodes.erase(found_selected);

				if (const auto resizing = edge_left_hovered || edge_top_hovered || edge_right_hovered || edge_bottom_hovered)
				{
					selected_nodes.clear();
					selected_nodes.push_back(this);
				}
			};

			bool is_hovered = false;
			ImGui::SetCursorScreenPos(node_rect.min);
			ImGui::InvisibleButton("group_region_node", node_rect.max - node_rect.min);
			if (ImGui::IsItemHovered())
			{
				is_hovered = true;

				// determine if edge is hovered here
				if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
				{
					const auto edge_radius = 10.f;
					const auto mouse_pos = ImGui::GetMousePos();
					edge_left_hovered = std::fabs(node_rect.min.x - mouse_pos.x) < edge_radius;
					edge_top_hovered = std::fabs(node_rect.min.y - mouse_pos.y) < edge_radius;
					edge_right_hovered = std::fabs(node_rect.max.x - mouse_pos.x) < edge_radius;
					edge_bottom_hovered = std::fabs(node_rect.max.y - mouse_pos.y) < edge_radius;
				}
				
				if (ImGui::IsMouseClicked(1))
				{
					selected_nodes.clear();
					selected_nodes.push_back(this);
				}
				else if (ImGui::IsMouseClicked(0))
					add_selected();
			}
			else
			{
				if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
				{
					edge_left_hovered = false;
					edge_top_hovered = false;
					edge_right_hovered = false;
					edge_bottom_hovered = false;
				}
			}

			if (ImGui::IsItemActive())
				node_active = true;

			ImGui::EndGroup();

			// Display the box
			const auto is_selected = std::find(selected_nodes.begin(), selected_nodes.end(), this) != selected_nodes.end();
			const auto fill_color = DEFAULT_GROUP_BG_COLOR;
			const auto border_color = DEFAULT_GROUP_BORDER_COLOR;
			const auto title_color = DEFAULT_GROUP_BORDER_COLOR * 0.75f;
			const auto rounding = 10.0f;
			const auto thickness = 3.5f;
			const auto node_bg_color = (is_hovered || is_selected) ? IM_COL32(fill_color.x + 15, fill_color.y + 15, fill_color.z + 15, fill_color.w) : IM_COL32(fill_color.x, fill_color.y, fill_color.z, fill_color.w);
			const auto node_border_color = (is_selected) ? IM_COL32_WHITE : IM_COL32(border_color.x, border_color.y, border_color.z, border_color.w);
			const auto header_color = IM_COL32(title_color.x, title_color.y, title_color.z, title_color.w);

			draw_list->ChannelsSetCurrent(0);
			draw_list->AddRectFilled(node_rect.min, node_rect.max, node_bg_color, rounding);
			draw_list->AddRect(node_rect.min, node_rect.max, node_border_color, rounding, ImDrawFlags_RoundCornersAll, thickness);
			draw_list->AddRectFilled(node_rect.min, ImVec2(node_rect.max.x, node_rect.min.y + ImGui::GetFrameHeightWithSpacing() + NODE_WINDOW_PADDING.y), header_color, rounding, ImDrawFlags_RoundCornersTop);

			// Display the description text just above the node box
			const auto pos = ImVec2(node_rect.min.x + NODE_SLOT_RADIUS, node_rect.min.y - (NODE_SLOT_RADIUS * 6.5f));
			if (changing_description)
			{
				char buffer[512] = { '\0' };
				ImFormatString(buffer, std::size(buffer), "%s", description.c_str());
				ImGui::SetCursorScreenPos(pos);
				if (ImGui::InputText("##GroupNodeDescription", buffer, std::size(buffer), ImGuiInputTextFlags_EnterReturnsTrue))
				{
					description = buffer;
					on_group_changed = true;
					changing_description = false;			
				}
				if (!ImGui::IsItemHovered() && (ImGui::IsMouseClicked(ImGuiMouseButton_Left) || ImGui::IsMouseClicked(ImGuiMouseButton_Middle) || ImGui::IsMouseClicked(ImGuiMouseButton_Right)))
				{
					changing_description = false;
				}
			}
			else
			{
				char buffer[512] = { '\0' };
				ImFormatString(buffer, std::size(buffer), "%s", description.c_str());

				const auto label_size = ImGui::CalcTextSize(buffer);
				const auto frame_bb = ImRect(pos, pos + label_size + ImVec2(NODE_SLOT_RADIUS * 2, NODE_SLOT_RADIUS * 2));

				draw_list->AddRectFilled(frame_bb.Min, frame_bb.Max, node_bg_color, rounding * 0.5f, ImDrawFlags_RoundCornersAll);
				draw_list->AddRect(frame_bb.Min, frame_bb.Max, node_border_color, rounding * 0.5f, ImDrawFlags_RoundCornersAll, thickness * 0.5f);

				ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0x27, 0xAE, 0x60, 0xFF));
				ImGui::RenderTextClipped(frame_bb.Min + ImVec2(NODE_SLOT_RADIUS, NODE_SLOT_RADIUS), frame_bb.Max - ImVec2(NODE_SLOT_RADIUS, NODE_SLOT_RADIUS), buffer, ImGui::FindRenderedTextEnd(buffer), &label_size);
				ImGui::PopStyleColor();

				ImGui::SetCursorScreenPos(frame_bb.Min);
				ImGui::InvisibleButton("group_region_description", frame_bb.Max - frame_bb.Min);
				if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(0))
				{
					changing_description = true;
					node_active = true;
					selected_nodes.clear();
					selected_nodes.push_back(this);
				}
			}

			ImGui::PopID();
		}

		bool UIGroupRegion::IntersectsBox(const ImVec2& min, const ImVec2& max) const
		{
			// Only select the box if the the selection min is lesser than our region min
			return (min.x < node_rect.min.x || min.y < node_rect.min.y) && UINode::IntersectsBox(min, max);
		}

		void UIGroupRegion::SetUIPosition(const simd::vector2& position)
		{
			const auto resizing = edge_left_hovered || edge_top_hovered || edge_right_hovered || edge_bottom_hovered;
			if (!resizing)
			{
				UINode::SetUIPosition(position);
				for (auto child : children)
					child->SetUIPosition(position + child->GetParentOffset());

				const auto size = local_space_size.max - local_space_size.min;
				local_space_size.min = GetUIPosition();
				local_space_size.max = local_space_size.min + size;
			}
		}

		void UIGroupRegion::SetUISize(const simd::vector2& size)
		{
			local_space_size.max = local_space_size.min + size;
			UINode::SetUISize(size);
		}

		bool UIGroupRegion::SetUIResize(const simd::vector2& delta)
		{
			if (edge_left_hovered)
				local_space_size.min.x += delta.x;
			if (edge_top_hovered)
				local_space_size.min.y += delta.y;
			if (edge_right_hovered)
				local_space_size.max.x += delta.x;
			if (edge_bottom_hovered)
				local_space_size.max.y += delta.y;

			local_space_size.max.x = std::max((local_space_size.min + DEFAULT_GROUP_BOX_SIZE).x, local_space_size.max.x);
			local_space_size.max.y = std::max((local_space_size.min + DEFAULT_GROUP_BOX_SIZE).y, local_space_size.max.y);

			UINode::SetUIPosition(local_space_size.min);
			UINode::SetUISize(local_space_size.max - local_space_size.min);
			for (auto& child : children)
			{
				const auto offset = child->GetUIPosition() - GetUIPosition();
				child->SetParentOffset(offset);
			}

			return edge_left_hovered || edge_top_hovered || edge_right_hovered || edge_bottom_hovered;
		}

		void UIGroupRegion::SavePositionAndSize()
		{
			for (auto& child : children)
				child->SavePositionAndSize();
			UINode::SavePositionAndSize();
		}

		void UIGroupRegion::RevertPositionAndSize()
		{
			edge_left_hovered = false;
			edge_top_hovered = false;
			edge_right_hovered = false;
			edge_bottom_hovered = false;
			for (auto& child : children)
				child->RevertPositionAndSize();
			UINode::RevertPositionAndSize();
		}

		void UIGroupRegion::AddInputSlot(const std::string& name, const GraphType& type, const bool loop)
		{
			if (name == std::string(LoopCountInputName))
				is_loop_box = true;
			modified_input_slots.emplace_back(name, type, loop);
			UINode::AddInputSlot(name, type, loop);
		}

		void UIGroupRegion::AddOutputSlot(const std::string& name, const GraphType& type, const bool loop)
		{
			modified_output_slots.emplace_back(name, type, loop);
			UINode::AddOutputSlot(name, type, loop);
		}

		void UpdateExternalLinksCounter(const UINode* node, UIGroupRegion* parent, const std::vector<std::unique_ptr<UINodeLink>>& ui_links, bool add)
		{
			for (const auto& link : ui_links)
			{
				if (!link->is_group_edge_link)
				{
					if (link->input.node == node)
					{
						if (parent->GetId() == link->output.node->GetParentId())
							if (add) parent->DecrementExternalLinks(); else parent->IncrementExternalLinks();
						else
							if (add) parent->IncrementExternalLinks(); else parent->DecrementExternalLinks();
					}
					if (link->output.node == node)
					{
						if (parent->GetId() == link->input.node->GetParentId())
							if (add) parent->DecrementExternalLinks(); else parent->IncrementExternalLinks();
						else
							if (add) parent->IncrementExternalLinks(); else parent->DecrementExternalLinks();
					}
				}
			}
		}

		void UIGroupRegion::AddChildren(const std::vector<UINode*>& nodes, const std::vector<std::unique_ptr<UINodeLink>>& ui_links /*= std::vector<std::unique_ptr<UINodeLink>>()*/)
		{
			if (nodes.empty())
				return;

			// Note: no way to get the global transform pos (i.e draw region offset), so we get it from one of the selected nodes here. TODO: Just pass transform pos when adding children or creating a group region.
			if (transform_pos.sqrlen() == 0.f)
				transform_pos = nodes[0]->GetTransformPos();

			for (auto node : nodes)
			{
				if (!node->IsGroupRegion() && !node->IsExtensionPoint())
				{
					if (node->GetParentId() != id)
					{
						node->SetParentId(id);
						children.push_back(node);
						UpdateExternalLinksCounter(node, this, ui_links, true);
					}
				}
			}

			ResnapSize();
		}

		void UIGroupRegion::RemoveChildren(const std::vector<UINode*>& nodes, const std::vector<std::unique_ptr<UINodeLink>>& ui_links /*= std::vector<std::unique_ptr<UINodeLink>>()*/)
		{
			if (nodes.empty())
				return;

			children.erase(std::remove_if(children.begin(), children.end(), [&](const auto child)
			{
				for (auto node : nodes)
				{
					if (node == child)
					{
						node->SetParentId(-1);
						UpdateExternalLinksCounter(node, this, ui_links, false);
						return true;
					}
				}
				return false;
			}), children.end());

			ResnapSize();
		}

		void UIGroupRegion::ResnapSize()
		{
			UINode::NodeRect viewport_space_size;
			auto scale_pos = local_space_size.min;
			ScalePositionHelper(scale_pos);
			viewport_space_size.min = transform_pos + scale_pos;
			scale_pos = local_space_size.max;
			ScalePositionHelper(scale_pos);
			viewport_space_size.max = transform_pos + scale_pos;

			for (auto node : children)
			{
				const auto& node_rect = node->GetNodeRect();
				viewport_space_size.min.x = std::min(viewport_space_size.min.x, node_rect.min.x);
				viewport_space_size.min.y = std::min(viewport_space_size.min.y, node_rect.min.y);
				viewport_space_size.max.x = std::max(viewport_space_size.max.x, node_rect.max.x);
				viewport_space_size.max.y = std::max(viewport_space_size.max.y, node_rect.max.y);
			}

			local_space_size.min = viewport_space_size.min - transform_pos;
			UnscalePositionHelper(local_space_size.min);
			local_space_size.max = viewport_space_size.max - transform_pos;
			UnscalePositionHelper(local_space_size.max);

			UINode::SetUIPosition(local_space_size.min);
			UINode::SetUISize(local_space_size.max - local_space_size.min);
			for (auto child : children)
			{
				const auto offset = child->GetUIPosition() - GetUIPosition();
				child->SetParentOffset(offset);
			}
		}

		void UIGroupRegion::RemoveSlots(std::vector<std::unique_ptr<UINodeLink>>& ui_links)
		{
			std::vector<std::string> removed_in_slots;
			std::vector<std::string> removed_out_slots;

			if (to_remove_input_slot >= 0)
			{
				if (modified_input_slots[to_remove_input_slot].loop)
				{
					const auto itr = modified_output_slots.begin() + to_remove_input_slot + ((is_loop_box) ? -1 : 0);
					removed_out_slots.push_back(itr->name);
					modified_output_slots.erase(itr);
				}
				const auto itr = modified_input_slots.begin() + to_remove_input_slot;
				removed_in_slots.push_back(itr->name);
				modified_input_slots.erase(itr);
				to_remove_input_slot = -1;
			}

			if (to_remove_output_slot >= 0)
			{
				if (modified_output_slots[to_remove_output_slot].loop)
				{
					const auto itr = modified_input_slots.begin() + to_remove_output_slot + ((is_loop_box) ? 1 : 0);
					removed_in_slots.push_back(itr->name);
					modified_input_slots.erase(itr);
				}
				const auto itr = modified_output_slots.begin() + to_remove_output_slot;
				removed_out_slots.push_back(itr->name);
				modified_output_slots.erase(itr);
				to_remove_output_slot = -1;
			}

			if (!removed_in_slots.empty() || !removed_out_slots.empty())
			{
				auto found_links = std::remove_if(ui_links.begin(), ui_links.end(), [&](const std::unique_ptr<UINodeLink>& link)->bool
				{
					if (link->input.node == this || link->output.node == this)
					{
						for (const auto slot : removed_in_slots)
							if (link->input.slot == slot)
								return true;
						for (const auto slot : removed_out_slots)
							if (link->output.slot == slot)
								return true;
					}
					return false;
				});
				ui_links.erase(found_links, ui_links.end());
			}
		}

		void UIGroupRegion::UpdateSlots()
		{
			input_slots = modified_input_slots;
			output_slots = modified_output_slots;
		}

		namespace
		{
			// Re-construct the color table as it used to be, to avoid changing colors for everyone on first creation
			namespace StyleColorOrder
			{
				constexpr GraphType OldTypeOrder[] = {
					GraphType::Bool,
					GraphType::Int,
					GraphType::Int2,
					GraphType::Int3,
					GraphType::Int4,
					GraphType::UInt,
					GraphType::Float,
					GraphType::Float2,
					GraphType::Float3,
					GraphType::Float4,
					GraphType::Float3x3,
					GraphType::Texture,
					GraphType::Sampler,
					GraphType::Float4x4,
					GraphType::Enum,
					GraphType::Spline5,
					GraphType::SplineColour,
					GraphType::NormField,
					GraphType::UvField,
					GraphType::Text,
				};

				constexpr size_t NumColors = magic_enum::enum_count<GraphType>();

				constexpr std::array<size_t, NumColors> ComputeTypeTable()
				{
					std::array<size_t, NumColors> res = {};
					for (size_t a = 0; a < NumColors; a++)
						res[a] = NumColors;
					
					for (size_t a = 0; a < std::size(OldTypeOrder); a++)
						if (const auto idx = magic_enum::enum_index(OldTypeOrder[a]))
							res[*idx] = a;

					for (size_t a = 0, b = std::size(OldTypeOrder); a < NumColors; a++)
						if (res[a] == NumColors)
							res[a] = b++;

					return res;
				}

				constexpr auto TypeTable = ComputeTypeTable();
			}
		}

		UIGraphStyle::UIGraphStyle()
		{
			auto table = Utility::RandomColorTable(0.5f, 0.5f);
			for (size_t a = 0; a < std::size(colors); a++)
				colors[a] = table[a < StyleColorOrder::NumColors ? StyleColorOrder::TypeTable[a] : a];
		}

		UIGraphManager::UIGraphManager()
		{
			if(properties_node == nullptr)
			{
				properties_node = std::make_unique<UINode>("", -1);
				properties_node->SetInstanceName("Graph Properties");
				properties_node->position = DEFAULT_PROPERTIES_POS;
			}
		}

		void UIGraphManager::AddCreatorId(const std::string& type_id, const unsigned group)
		{
			type_ids.push_back(std::make_pair(type_id, group));

			std::sort(type_ids.begin(), type_ids.end(), [](const std::pair<std::string, unsigned>& a, const std::pair<std::string, unsigned>& b)
			{
				const auto result = CaseInsensitiveCmp(a.first.c_str(), b.first.c_str());
				if(result == 0)
					return a.second > b.second;
				return result < 0;
			});
		}

		void UIGraphManager::SetCallbacks(
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
			void* pUserContext)
		{
			m_pOnAddNodeCallback = on_add_callback;
			m_pOnRemoveNodeCallback = on_remove_callback;
			m_pOnLinkNodesCallback = on_link_callback;
			m_pOnUnlinkNodesCallback = on_ulink_callback;
			m_pOnSplitSlotComponents = on_splitslot_components_callback;
			m_pOnParameterChangedCallback = on_parameter_changed_callback;
			m_pOnExtensionPointStageChanged = on_extensionpoint_stage_changed_callback;
			m_pOnAddNewExtensionPoint = on_add_new_extension_point_callback;
			m_pOnAddNewExtensionPointSlot = on_add_new_extension_point_slot_callback;
			m_pOnRemoveExtensionPointSlot = on_remove_extension_point_slot_callback;
			m_pOnAddDefaultExtensionPoints = on_add_default_extension_points_callback;
			m_pOnPositionChangedCallback = on_position_changed_callback;
			m_pOnResizeCallback = on_resize_callback;
			m_pOnCopyCallback = on_copy_callback;
			m_pOnPasteCallback = on_paste_callback;
			m_pOnSaveCallback = on_save_callback;
			m_pOnCustomParameterSet = on_custom_param_callback;
			m_pOnUndoCallback = on_undo_callback;
			m_pOnRedoCallback = on_redo_callback;
			m_pOnGraphChangedCallback = on_graph_changed_callback;
			m_pOnCustomRangeCallback = on_custom_range_callback;
			m_pOnChildRemoved = on_child_removed;
			m_pOnChildAdded = on_child_added;
			m_pOnGroupChanged = on_group_changed;
			m_pCallbackEventUserContext = pUserContext;

			properties_node->SetRenderCallback([on_render_graph_properties_callback, pUserContext]() { return on_render_graph_properties_callback ? on_render_graph_properties_callback(pUserContext) : false; });
		}

		void UIGraphManager::Reset(bool recover)
		{	
			ui_extension_points.clear();
			ui_group_regions.clear();
			ui_links.clear();
			selected_nodes.clear();
			highest_depth = -1;
			ResetPerFrame();

			properties_node->node_width = 0.f;
			properties_node->node_content_width = 0.f;
			properties_node->parameters.clear();

			ui_nodes.clear();
		}

		void UIGraphManager::ResetToOrigin()
		{
			FocusToPos(simd::vector2(0.f), false);
		}

		void UIGraphManager::SetErrorMessage(const std::string& msg)
		{
			error_msg = msg;
			if (error_msg.length() > 0)
				error_timeout = 5.0f;
		}

		void UIGraphManager::FocusToPos(const simd::vector2& pos, bool recenter)
		{
			const auto context = ImGui::GetCurrentContext();
			for (int i = 0; context && i != context->Windows.Size; i++)
			{
				const auto window = context->Windows[i];
				if (window)
				{
					std::string window_name = window->Name;
					if (window_name.find("draw_region") != std::string::npos)
					{
						scrolling = pos - ((recenter) ? simd::vector2(window->Size * 0.5f) : 0.f);
						scale_infos.clear();
						window->FontWindowScale = 1.f;
						OnScaleUpdate(1.f);
						break;
					}
				}
			}
		}

		void UIGraphManager::ResetPerFrame()
		{
			input_node = nullptr;
			output_node = nullptr;
			selected_node_input.clear();
			selected_node_output.clear();
			reconnect_infos.clear();
			enable_group_link_switching = true;
		}

		bool UIGraphManager::MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
		{
#if !defined( CONSOLE_UI ) && !defined( __APPLE__ ) && !defined( ANDROID )
			ImGuiIO& io = ImGui::GetIO();
			auto* window = ImGui::GetCurrentContext()->NavWindow;
			bool is_focused = false;
			if (window)
			{
				std::string window_name = window->Name;
				is_focused = window_name.find("draw_region") != std::string::npos;
			}

			switch (msg)
			{
			case WM_LBUTTONDOWN: case WM_LBUTTONDBLCLK:
			case WM_RBUTTONDOWN: case WM_RBUTTONDBLCLK:
			case WM_MBUTTONDOWN: case WM_MBUTTONDBLCLK:
			case WM_XBUTTONDOWN: case WM_XBUTTONDBLCLK:	
				if (!is_focused)
				{
					multi_select_mode = false;
					refresh_selection = true;
					return true;
				}
				break;
			case WM_ACTIVATE:
				if(wParam == WA_INACTIVE && is_focused)
				{
					multi_select_mode = false;
					refresh_selection = true;
					return true;
				}	
				break;
			}
#endif
			return false;
		}

		bool UIGraphManager::ProcessInput()
		{
			bool result = false;
#ifndef CONSOLE_UI
			ImGuiIO& io = ImGui::GetIO();
			auto* window = ImGui::GetCurrentContext()->NavWindow;
			bool is_focused = false;
			if (window)
			{
				std::string window_name = window->Name;
				is_focused = window_name.find("draw_region") != std::string::npos;
			}

			if (GUIInstance::GetMouseWheel() != 0.0f && window && !GUIInstance::IsMouseButtonDown(2) && io.MousePos.x != -FLT_MAX && !(io.KeyShift || io.KeyCtrl))
			{
				bool is_hovered = window->Pos.x < io.MousePos.x && (window->Pos.x + window->Size.x) > io.MousePos.x && window->Pos.y < io.MousePos.y && (window->Pos.y + window->Size.y) > io.MousePos.y;
				if (is_focused && is_hovered)
				{
					ImVec2 offset = window->Pos - scrolling;
					ImVec2 pivot_center = io.MousePos - offset;
					const float new_font_scale = ImClamp(window->FontWindowScale + GUIInstance::GetMouseWheel() * 0.10f, ZOOM_MIN, ZOOM_MAX);
					if (scale_infos.empty() || ImLengthSqr(pivot_center - scale_infos.back().pivot_center) > 25.f)
					{
						ScaleInfo new_scale = { simd::vector2(pivot_center), new_font_scale };
						scale_infos.push_back(new_scale);
					}
					else
						scale_infos.back().scale = new_font_scale;
					window->FontWindowScale = new_font_scale;
					OnScaleUpdate(new_font_scale);
					result = true;
				}
			}

			if (is_focused && !context_menu_open)
			{
				if (GUIInstance::IsKeyDown(VK_CONTROL))
				{
					result = true;
					if (reconnect_infos.empty())
						multi_select_mode = true;
					else
						multi_select_mode = false;
				}
				else
				{
					if (multi_select_mode)
						result = true;
					multi_select_mode = false;
				}

				if (GUIInstance::IsKeyDown(VK_SHIFT))
				{
					result = true;
					refresh_selection = false;
				}
				else
				{
					if (!refresh_selection)
						result = true;
					refresh_selection = true;
				}

				if (GUIInstance::IsKeyPressed(VK_DELETE))
				{
					if (!io.WantTextInput)
					{
						for (auto itr = selected_nodes.begin(); itr != selected_nodes.end(); ++itr)
							DeleteNode(*itr, std::distance(selected_nodes.begin(), itr) == (selected_nodes.size()-1));
						selected_nodes.clear();
						ResetPerFrame();
					}

					result = true;
				}

				if (GUIInstance::IsKeyPressed(L'S') && io.KeyCtrl)
				{
					SetErrorMessage("Saving...");
					m_pOnSaveCallback(m_pCallbackEventUserContext);
					SetErrorMessage("Save Successful!");
					result = true;
				}

				if (GUIInstance::IsKeyPressed(L'Z') && io.KeyCtrl)
				{
					m_pOnUndoCallback(m_pCallbackEventUserContext);
					result = true;
				}

				if (GUIInstance::IsKeyPressed(L'Y') && io.KeyCtrl)
				{
					m_pOnRedoCallback(m_pCallbackEventUserContext);
					result = true;
				}

				if (!io.WantTextInput)
				{
					if (io.KeyCtrl)
					{
						if (GUIInstance::IsKeyPressed(L'C'))
						{
							m_pOnCopyCallback(selected_nodes, m_pCallbackEventUserContext);
							result = true;
						}
						else if (GUIInstance::IsKeyPressed(L'V'))
						{
							m_pOnPasteCallback(m_pCallbackEventUserContext);
							result = true;
						}
						else if (GUIInstance::IsKeyPressed(L'F'))
						{
							show_search_window = true;
							result = true;
						}
						else if (GUIInstance::IsKeyPressed(L'B'))
						{
							if (!selected_nodes.empty())
							{
								// filter out selected nodes that are not grouped yet
								std::vector<UINode*> to_group;
								for (auto node : selected_nodes)
								{
									if (!node->IsGroupRegion() && !node->IsExtensionPoint() && node->GetParentId() == unsigned(-1))
										to_group.push_back(node);
								}

								if (!to_group.empty())
								{
									AddGroupRegionNode(to_group, simd::vector2(0.f));
									selected_nodes.clear();
								}
							}
							result = true;
						}
					}
					else if (io.KeyShift)
					{
						if (GUIInstance::IsKeyPressed(L'F'))
						{
							ResetToOrigin();
							result = true;
						}
					}
				}
			}

			if (context_menu_open)
				result = true;
#endif
			return result;
		}

		void UIGraphManager::GetSelectionRect(ImVec2& out_min, ImVec2& out_max)
		{
			ImVec2 mouse_pos = ImGui::GetMousePos();
			if(selection_start_rect.x < mouse_pos.x)
			{
				out_min.x = selection_start_rect.x;
				out_max.x = mouse_pos.x;
			}
			else
			{
				out_min.x = mouse_pos.x;
				out_max.x = selection_start_rect.x;
			}

			if(selection_start_rect.y < mouse_pos.y)
			{
				out_min.y = selection_start_rect.y;
				out_max.y = mouse_pos.y;
			}
			else
			{
				out_min.y = mouse_pos.y;
				out_max.y = selection_start_rect.y;
			}
		}

		void UIGraphManager::OnScaleUpdate(float scale)
		{
			NODE_SLOT_RADIUS = NODE_SLOT_RADIUS_DEFAULT * scale;
			NODE_WINDOW_PADDING = NODE_WINDOW_PADDING_DEFAULT * scale;
			SLOT_PADDING = SLOT_PADDING_DEFAULT * scale;
			PARAMETER_WIDTH = PARAMETER_WIDTH_DEFAULT * scale;
			CONNECTOR_PARAMETER_WIDTH = CONNECTOR_PARAMETER_WIDTH_DEFAULT * scale;
			SLOT_PARAMETER_PADDING = SLOT_PARAMETER_PADDING_DEFAULT * scale;
			GRID_SZ = GRID_SZ_DEFAULT * scale;
			CURRENT_SCALE = scale;
		//	TEXTURE_PREVIEW_SIZE = TEXTURE_PREVIEW_SIZE_DEFAULT * scale;
		}

		void UIGraphManager::OnRender()
		{		
			UINode* hovered_node = nullptr;
			UIParameter* changed_parameter = nullptr;
			unsigned link_parameter_index = 0;
			bool node_active = false;
			bool on_group_changed = false;
			bool show_customparam_window = false;
				
			ImGui::BeginGroup();

			const auto frame_padding = ImGui::GetStyle().FramePadding;
			const auto window_padding = ImGui::GetStyle().WindowPadding;

			// Create our child canvas
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(1, 1));
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
			ImGui::BeginChild("draw_region", ImVec2(0, 0), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoMove);
			ImGui::PopStyleVar();

			ImVec2 offset = ImGui::GetCursorScreenPos() - ImVec2(scrolling.x, scrolling.y);
			transform_pos = offset;
			ImDrawList* draw_list = ImGui::GetWindowDrawList();
			int num_channels = std::max(highest_depth, static_cast<int>(2 + ui_nodes.size() * 2));
			highest_depth = num_channels;
			draw_list->ChannelsSplit(num_channels);

			// Display grid
			ImU32 GRID_COLOR = IM_COL32(200, 200, 200, 40);
			ImVec2 win_pos = ImGui::GetCursorScreenPos();
			ImVec2 canvas_sz = ImGui::GetWindowSize();

			simd::vector2 grid_start = ImVec2(0.0f);
			UnscalePositionHelper(grid_start);
			grid_start -= simd::vector2(GRID_SZ_DEFAULT + std::fmod(grid_start.x, GRID_SZ_DEFAULT), GRID_SZ_DEFAULT + std::fmod(grid_start.y, GRID_SZ_DEFAULT));
			ScalePositionHelper(grid_start);
			grid_start = win_pos + grid_start - ImVec2(GRID_SZ + std::fmod(scrolling.x, GRID_SZ), GRID_SZ + std::fmod(scrolling.y, GRID_SZ));

			simd::vector2 grid_end = canvas_sz;
			UnscalePositionHelper(grid_end);
			grid_end += simd::vector2(GRID_SZ_DEFAULT - std::fmod(grid_end.x, GRID_SZ_DEFAULT), GRID_SZ_DEFAULT - std::fmod(grid_end.y, GRID_SZ_DEFAULT));
			ScalePositionHelper(grid_end);
			grid_end = win_pos + grid_end + ImVec2(GRID_SZ - std::fmod(scrolling.x, GRID_SZ), GRID_SZ - std::fmod(scrolling.y, GRID_SZ));

			for (float x = grid_start.x; x < grid_end.x; x += GRID_SZ)
			{
				simd::vector2 start = simd::vector2(x, grid_start.y);
				simd::vector2 end = simd::vector2(x, grid_end.y);
				draw_list->AddLine(start, end, GRID_COLOR);
			}
			for (float y = grid_start.y; y < grid_end.y; y += GRID_SZ)
			{
				simd::vector2 start = simd::vector2(grid_start.x, y);
				simd::vector2 end = simd::vector2(grid_end.x, y);
				draw_list->AddLine(start, end, GRID_COLOR);
			}

			// Display the division line
			if(has_division)
			{
				ImU32 DIV_COLOR = IM_COL32(200, 200, 200, 80);
				simd::vector2 temp(0.f, division_height);
				ScalePositionHelper(temp);
				simd::vector2 start = simd::vector2(grid_start.x, offset.y + temp.y);
				simd::vector2 end = simd::vector2(grid_end.x, offset.y + temp.y);
				draw_list->AddLine(start, end, DIV_COLOR, 3.f);
			}

			UINode* prev_selected_node = selected_nodes.empty() ? nullptr : selected_nodes[0];

			// Display the properties box
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImGui::GetStyle().ItemSpacing * CURRENT_SCALE);
			ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, ImGui::GetStyle().ItemInnerSpacing * CURRENT_SCALE);

			properties_node->CalculateNodeWidth();
			properties_node->SetTransformPos(simd::vector2(offset.x, offset.y));
			properties_node->OnRender(style, draw_list, selected_nodes, &input_node, &output_node, selected_node_input, selected_node_output, nullptr, node_active, on_group_changed, refresh_selection);

			// Display the nodes
			for(auto node_itr = ui_nodes.rbegin(); node_itr != ui_nodes.rend(); ++node_itr)
			{
				UINode* node = node_itr->get();
				node->CalculateNodeWidth();
				node->SetTransformPos(simd::vector2(offset.x, offset.y));
				node->OnRender(style, draw_list, selected_nodes, &input_node, &output_node, selected_node_input, selected_node_output, &changed_parameter, node_active, on_group_changed, refresh_selection);

				// Handle extension point messages
				if(node->is_extension)
				{
					UIExtensionPoint* ext_node = static_cast<UIExtensionPoint*>(node);
					HandleExtensionPointMessages(ext_node);
				}

				// Handle node parameter changed
				if (changed_parameter)
				{
					m_pOnParameterChangedCallback(*node, *changed_parameter, m_pCallbackEventUserContext);
					changed_parameter = nullptr;
				}

				if (node->IsGroupRegion() && on_group_changed)
				{
					auto group_node = static_cast<UIGroupRegion*>(node);
					RefreshGroupNodeLinks(static_cast<UIGroupRegion*>(node), [&]() 
					{ 
						group_node->RemoveSlots(ui_links);
						group_node->UpdateSlots();
						m_pOnGroupChanged(*node, m_pCallbackEventUserContext);
					});		
					on_group_changed = false;
				}
			}

			// Refresh group region sizes (this is when a newly added node is created inside a group region but it's node_rect is not yet fully set at that time, 
			// so we just recalculate the group sizes again here, since the nodes need to get rendered to get their node_rect)
			if (refresh_group_region_sizes)
			{
				for (auto& group : ui_group_regions)
				{
					group.second->ResnapSize();
					m_pOnResizeCallback(*group.second, m_pCallbackEventUserContext);
				}
				refresh_group_region_sizes = false;
			}

			ImGui::PopStyleVar(2);

			// Display select box
			ImVec2 select_min = ImVec2(0.f, 0.f);
			ImVec2 select_max = ImVec2(0.f, 0.f);
			if(multi_select_mode)
			{
				if(ImGui::IsMouseClicked(0))
				{
					if(refresh_selection)
						selected_nodes.clear();
					ImVec2 mouse_pos = ImGui::GetMousePos();
					selection_start_rect = simd::vector2(mouse_pos.x, mouse_pos.y);
				}
				else if(ImGui::IsMouseDown(0) || ImGui::IsMouseDragging(0))
				{
					draw_list->ChannelsSetCurrent(0);
					GetSelectionRect(select_min, select_max);
					draw_list->AddRectFilled(select_min, select_max, IM_COL32(60, 60, 70, 150), 0.f, 0);
					draw_list->AddRect(select_min, select_max, IM_COL32(100, 100, 100, 150), 0.f, 0);

					// Gather all the nodes that intersects the selection box
					for(auto node_itr = ui_nodes.begin(); node_itr != ui_nodes.end(); ++node_itr)
					{
						UINode* node = node_itr->get();
						auto found_selected = std::find(selected_nodes.begin(), selected_nodes.end(), node);
						bool is_already_selected = found_selected != selected_nodes.end();
						if(node->IntersectsBox(select_min, select_max))
						{
							if(!is_already_selected)
								selected_nodes.push_back(node);
						}
					}
				}
			}

			// Unselect any nodes if their parent group box is already selected
			Memory::Set<unsigned, Memory::Tag::Unknown> selected_group_boxes;
			for (const auto& selected : selected_nodes)
			{
				if (selected->IsGroupRegion())
					selected_group_boxes.insert(static_cast<UIGroupRegion*>(selected)->GetId());
			}
			for (auto itr = selected_nodes.begin(); itr != selected_nodes.end();)
			{
				auto selected = *itr;
				if (!selected->IsGroupRegion())
				{
					auto found = selected_group_boxes.find(selected->GetParentId());
					if (found != selected_group_boxes.end())
						itr = selected_nodes.erase(itr);
					else ++itr;
				}
				else ++itr;
			}

			// Handle node position/size changed
			if(!multi_select_mode && node_active && (ImGui::IsMouseDown(0) || ImGui::IsMouseDragging(0)))
			{
				float current_scale = scale_infos.empty() ? 1.f : scale_infos.back().scale;
				for(auto itr = selected_nodes.begin(); itr != selected_nodes.end(); ++itr)
				{
					auto selected_node = *itr;
					ImVec2 new_pos = ImVec2(selected_node->position.x, selected_node->position.y) + ImGui::GetIO().MouseDelta / current_scale;

					if(has_division)
					{
						if (selected_node->group == group_0)
						{
							auto current_scale = scale_infos.empty() ? 1.f : scale_infos.back().scale;
							new_pos.y = std::min(new_pos.y, division_height - (selected_node->node_rect.max.y - selected_node->node_rect.min.y) / current_scale);
						}
						else if(selected_node->group == group_1)
							new_pos.y = std::max(new_pos.y, division_height);
					}
					
					if (ImGui::IsMouseClicked(0))
						selected_node->SavePositionAndSize();
					selected_node->SetUIPosition(simd::vector2(new_pos.x, new_pos.y));
					if (selected_node->SetUIResize(ImGui::GetIO().MouseDelta / current_scale))
					{
						assert(selected_node->IsGroupRegion());
						resized_group_region = static_cast<UIGroupRegion*>(selected_node);
					}
					
					position_changed = true;
				}
			}

			if (ImGui::IsMouseReleased(0) && position_changed)
			{
				std::map<unsigned, std::vector<UINode*>> grouped_nodes;
				std::map<unsigned, std::vector<UINode*>> ungrouped_nodes;
				bool show_group_external_link_error = false;
				bool show_group_shader_type_error = false;

				auto check_node_division = [&](const UIGroupRegion* group_node, const UINode* node) -> bool
				{
					std::vector<UINode*> temp;
					if (const auto& children = group_node->GetChildren(); !children.empty())
					{
						temp.push_back(children[0]);
						temp.push_back(const_cast<UINode*>(node));
						if (!CheckNodesDivisionArea(temp))
						{
							show_group_shader_type_error = true;
							return false;
						}
					}
					return true;
				};

				// Handle group region resized
				if (resized_group_region)
				{
					// test this resized group region to all the nodes not inside it
					const auto group_id = resized_group_region->GetId();
					const auto& group_rect = resized_group_region->GetNodeRect();
					for (auto node_itr = ui_nodes.rbegin(); node_itr != ui_nodes.rend(); ++node_itr)
					{
						UINode* node = node_itr->get();
						if (!node->IsExtensionPoint() && !node->IsGroupRegion())
						{
							// check first for nodes that were moved out from this group
							if (node->GetParentId() == group_id)
							{
								if (!node->IntersectsBox(group_rect.min, group_rect.max))
								{
									if (CanGroupRejectNode(resized_group_region, node))
										ungrouped_nodes[group_id].push_back(node);
									else
									{
										resized_group_region->RevertPositionAndSize();
										show_group_external_link_error = true;
									}
								}
							}

							// check for nodes that are going to be added
							if (node->GetParentId() == (unsigned)-1)
							{
								if (node->IntersectsBox(group_rect.min, group_rect.max))
								{
									if (CanGroupAcceptNode(resized_group_region, node))
									{
										if (check_node_division(resized_group_region, node))
											grouped_nodes[group_id].push_back(node);
										else
											resized_group_region->RevertPositionAndSize();
									}
									else
									{
										resized_group_region->RevertPositionAndSize();
										show_group_external_link_error = true;
									}
								}
							}
						}
					}

					// auto-snap when nothing gets grouped/ungrouped
					if (grouped_nodes.empty() && ungrouped_nodes.empty())
						resized_group_region->ResnapSize();

					m_pOnResizeCallback(*resized_group_region, m_pCallbackEventUserContext);
				}

				// Handle individual nodes dragged in to or out from the group regions
				for (auto itr = selected_nodes.begin(); itr != selected_nodes.end(); ++itr)
				{
					auto selected_node = *itr;
					
					if (selected_node->IsGroupRegion())
					{
						auto group_region = static_cast<UIGroupRegion*>(selected_node);
						if (!CheckNodesDivisionArea(group_region->GetChildren()))
						{
							selected_node->RevertPositionAndSize();
							show_group_shader_type_error = true;
						}
					}
					else if (!selected_node->IsExtensionPoint())
					{
						// check first for nodes that were moved out from the group
						if (selected_node->GetParentId() != (unsigned)-1)
						{
							auto found_group = ui_group_regions.find(selected_node->GetParentId());
							if (found_group != ui_group_regions.end())
							{
								const auto& group_rect = found_group->second->GetNodeRect();
								if (!selected_node->IntersectsBox(group_rect.min, group_rect.max))
								{
									if (CanGroupRejectNode(found_group->second, selected_node))
										ungrouped_nodes[found_group->first].push_back(selected_node);
									else
									{
										selected_node->RevertPositionAndSize();
										show_group_external_link_error = true;
									}
								}
							}
						}

						// check for nodes that are moved/transfered into another group
						for (auto group_region : ui_group_regions)
						{
							const auto& group_rect = group_region.second->GetNodeRect();
							const auto node_stayed = ungrouped_nodes.find(selected_node->GetParentId()) == ungrouped_nodes.end();
							const auto skip = selected_node->GetParentId() != unsigned(-1) && node_stayed && group_region.first != selected_node->GetParentId();
							if (!skip && selected_node->IntersectsBox(group_rect.min, group_rect.max))
							{
								if (CanGroupAcceptNode(group_region.second, selected_node))
								{
									if (check_node_division(group_region.second, selected_node))
										grouped_nodes[group_region.first].push_back(selected_node);
									else
										selected_node->RevertPositionAndSize();
								}
								else
								{
									selected_node->RevertPositionAndSize();
									show_group_external_link_error = true;
								}
								break;
							}
						}
					}

					m_pOnPositionChangedCallback(**itr, m_pCallbackEventUserContext);
				}

				if (show_group_external_link_error == false && show_group_shader_type_error == false)
				{
					for (auto group : ungrouped_nodes)
					{
						auto found_group = ui_group_regions.find(group.first);
						if (found_group != ui_group_regions.end())
						{
							found_group->second->RemoveChildren(group.second, ui_links);
							for (const auto child : group.second)
								m_pOnChildRemoved(*child, group.first, m_pCallbackEventUserContext);
							m_pOnResizeCallback(*found_group->second, m_pCallbackEventUserContext);
						}
					}

					for (auto group : grouped_nodes)
					{
						auto found_group = ui_group_regions.find(group.first);
						if (found_group != ui_group_regions.end())
						{
							found_group->second->AddChildren(group.second, ui_links);
							for (const auto child : group.second)
								m_pOnChildAdded(*child, group.first, m_pCallbackEventUserContext);
							m_pOnResizeCallback(*found_group->second, m_pCallbackEventUserContext);
						}
					}
				}

				if (show_group_external_link_error)
					SetErrorMessage("Groups that have input/output slots cannot have their child nodes be connected outside");
				if (show_group_shader_type_error)
					SetErrorMessage("Group box children needs to be in the same shader type area.");

				resized_group_region = nullptr;
				position_changed = false;
			}

			auto input_end_points = SetupReconnection();
	
			draw_list->ChannelsSetCurrent(0);

			if (!input_end_points.empty() && !selected_node_output.empty() && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
			{
				for (const auto& input_point : input_end_points)
					ConnectNodes(input_point.node, output_node, input_point.slot, selected_node_output);
			}
			else if (!ImGui::IsPopupOpen("breaklink_context_menu") && !ImGui::IsPopupOpen("breakremove_context_menu"))
				DrawPendingLinks(input_end_points, draw_list);

			DrawEstablishedLinks(draw_list);

			DrawHighlightedLinks(draw_list);

			draw_list->ChannelsMerge();

			// Open context menu
			const auto is_group_region_selected = (!selected_nodes.empty() && selected_nodes.back()->IsGroupRegion());
			if((!ImGui::IsAnyItemHovered() || is_group_region_selected) && ImGui::IsWindowHovered())
			{
				if((refresh_selection && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) || ImGui::IsMouseClicked(ImGuiMouseButton_Right))
					selected_nodes.clear();
				if (ImGui::IsMouseClicked(ImGuiMouseButton_Right))
				{
					ImGui::OpenPopup("context_menu");
					context_menu_open = true;
				}
			}

			// Draw context menu
			if (context_menu_open && ImGui::BeginPopup("context_menu"))
			{				
				const bool context_menu_filter_clear = ImGui::IsWindowAppearing();
				const bool context_menu_filter_focus = context_menu_filter_clear || (ImGui::GetIO().KeyCtrl && ImGui::IsKeyReleased(ImGuiKey_F));

				ImVec2 mouse_pos = ImGui::GetMousePosOnOpeningCurrentPopup() - offset;
				context_menu_pos = simd::vector2(mouse_pos.x, mouse_pos.y);
				UnscalePositionHelper(context_menu_pos);
				bool group0_only = !has_division || context_menu_pos.y < division_height;

				if (HasExtensionPointsByGroup(group0_only ? group_0 : group_1))
				{
					auto window = ImGui::GetCurrentWindow();
					if(context_menu_filter_clear)
						context_menu_filter.Clear();

					if(context_menu_filter_focus)
						ImGui::SetKeyboardFocusHere();

					context_menu_filter.Draw("##ContextMenuFilter");
					const bool force_select = ImGui::IsItemDeactivated() && (ImGui::IsKeyDown(ImGuiKey_Enter) || ImGui::IsKeyReleased(ImGuiKey_Enter));

					ImGui::BeginChild("context_menu_sub", ImVec2(ImGui::GetContentRegionAvail().x, 550.f), false, ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_NavFlattened);

					for (int i = 0; i < (int)type_ids.size(); ++i)
					{
						const auto& label = type_ids[i].first;
						bool visible = (type_ids[i].second == ~unsigned(0) || type_ids[i].second == (group0_only ? group_0 : group_1)) && context_menu_filter.PassFilter(label.c_str());
						if (!visible)
							continue;

						if (ImGui::Selectable(label.c_str(), false) || (ImGui::IsItemFocused() && ImGui::IsKeyPressed(ImGuiKey_Enter, false)) || force_select)
						{
							AddNode(type_ids[i].first, context_menu_pos);
							ImGui::CloseCurrentPopup();
							break;
						}
					}

					ImGui::EndChild();
				}
				else
				{
					if (ImGui::MenuItem("Add Default Extension Points"))
						m_pOnAddDefaultExtensionPoints(group0_only ? group_0 : group_1, m_pCallbackEventUserContext);
				}
				ImGui::EndPopup();
			}
			else
			{
				ImGui::CloseCurrentPopup();
				context_menu_open = false;
			}

			// Open break-link context menu
			bool breaklink_popup = false;
			if((input_node || output_node) && ImGui::IsAnyItemHovered() && ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
			{
				if(input_node && input_node->IsExtensionPoint())
					ImGui::OpenPopup("breakremove_context_menu");
				else if(output_node && output_node->IsExtensionPoint())
					ImGui::OpenPopup("breakremove_context_menu");
				else if (input_node && input_node->IsGroupRegion())
				{
					ImGui::OpenPopup("breakremove_group_context_menu");
					enable_group_link_switching = false;
				}
				else if (output_node && output_node->IsGroupRegion())
				{
					ImGui::OpenPopup("breakremove_group_context_menu");
					enable_group_link_switching = false;
				}
				else
					ImGui::OpenPopup("breaklink_context_menu");
				breaklink_popup = true;
			}

			// for removing links
			auto remove_links = [&]()
			{
				auto found_links = std::remove_if(ui_links.begin(), ui_links.end(), [&](const std::unique_ptr<UINodeLink>& link)->bool
				{
					if ((link->input.node == input_node && link->input.slot == selected_node_input) || (link->output.node == output_node && link->output.slot == selected_node_output))
					{
						RemoveLink(link.get());
						m_pOnGraphChangedCallback(m_pCallbackEventUserContext);
						return true;
					}
					return false;
				});
				ui_links.erase(found_links, ui_links.end());
				ResetPerFrame();
			};

			// for removing edge links for groups
			auto remove_group_edge_links = [&](const bool inside)
			{
				auto found_links = std::remove_if(ui_links.begin(), ui_links.end(), [&](const std::unique_ptr<UINodeLink>& link)->bool
				{
					if (inside)
					{
						if (link->is_group_edge_link)
						{
							if ((link->input.node == input_node && link->input.slot == selected_node_input) || (link->output.node == output_node && link->output.slot == selected_node_output))
							{
								RemoveLink(link.get());
								m_pOnGraphChangedCallback(m_pCallbackEventUserContext);
								return true;
							}
						}
						return false;
					}
					else
					{
						if (!link->is_group_edge_link)
						{
							if ((link->input.node == output_node && link->input.slot == selected_node_output) || (link->output.node == input_node && link->output.slot == selected_node_input))
							{
								RemoveLink(link.get());
								m_pOnGraphChangedCallback(m_pCallbackEventUserContext);
								return true;
							}
						}
						return false;
					}
				});
				ui_links.erase(found_links, ui_links.end());
				ResetPerFrame();
			};

			// Draw break-link context menu
			if(ImGui::BeginPopup("breaklink_context_menu"))
			{
				if(ImGui::MenuItem("Break Link"))
					remove_links();

				ImGui::Separator();
				RenderSplitComponentsMenuItems();

				ImGui::EndPopup();
			}

			// Draw break-remove context menu
			if(ImGui::BeginPopup("breakremove_context_menu"))
			{
				if(ImGui::MenuItem("Break Link"))
					remove_links();
				if(ImGui::MenuItem("Remove Slot"))
				{
					UIExtensionPoint* ext_node = input_node ? static_cast<UIExtensionPoint*>(input_node) : static_cast<UIExtensionPoint*>(output_node);
					if (ext_node)
					{
						auto slot_to_remove = input_node ? selected_node_input : selected_node_output;
						auto stage = input_node ? ext_node->GetInputParameterValue() : ext_node->GetOutputParameterValue();
						m_pOnRemoveExtensionPointSlot(slot_to_remove, stage, m_pCallbackEventUserContext);
						ResetPerFrame();
					}
				}

				ImGui::Separator();
				RenderSplitComponentsMenuItems();

				ImGui::EndPopup();
			}

			// Draw break-remove group context menu
			if (ImGui::BeginPopup("breakremove_group_context_menu"))
			{
				if (ImGui::MenuItem("Break Link Out"))
					remove_group_edge_links(false);
				if (ImGui::MenuItem("Break Link In"))
					remove_group_edge_links(true);
				if (ImGui::MenuItem("Remove Slot"))
				{
					auto group_node = input_node ? static_cast<UIGroupRegion*>(input_node) : static_cast<UIGroupRegion*>(output_node);
					if (group_node)
					{
						RefreshGroupNodeLinks(group_node, [&]() 
						{ 
							group_node->RemoveSlots(ui_links);
							group_node->UpdateSlots();
							m_pOnGroupChanged(*group_node, m_pCallbackEventUserContext);
						});								
						ResetPerFrame();
					}
				}

				// TODO: don't support splitting channels for group slots for now
				//ImGui::Separator();
				//RenderSplitComponentsMenuItems();

				ImGui::EndPopup();
			}

			// Open the node context menu
			if(!breaklink_popup && ImGui::IsAnyItemHovered() && ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
			{
				if (!selected_nodes.empty() && !selected_nodes[0]->IsExtensionPoint())
				{
					ImVec2 mouse_pos = ImGui::GetMousePosOnOpeningCurrentPopup() - offset;
					context_menu_pos = simd::vector2(mouse_pos.x, mouse_pos.y);

					show_customparam_window = true;
					ResetPerFrame();
				}
			}

			// Reset pending links if necessary
			if(ImGui::IsMouseReleased(0))
				ResetPerFrame();

			// Display error message
			if(!error_msg.empty())
			{
				float alpha = std::clamp(error_timeout, 0.0f, 1.0f);

				ImVec2 msg_pos(std::max(win_pos.x, 10.f), std::max(win_pos.y, 10.f));
				ImGui::SetCursorScreenPos(msg_pos);
				ImGui::TextColored(ImVec4(1, 0, 0, alpha), "%s", error_msg.c_str());
			}

			if (error_timeout > 0.0f)
			{
				error_timeout -= ImGui::GetIO().DeltaTime;
				if (error_timeout <= 0.0f)
					error_msg = "";
			}

			// Scrolling
			if(ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem) && ImGui::IsMouseDragging(2, 0.0f))
				scrolling = scrolling - simd::vector2(ImGui::GetIO().MouseDelta.x, ImGui::GetIO().MouseDelta.y);

			ImGui::EndChild();
			ImGui::PopStyleVar();
			ImGui::EndGroup();

			// Prevent custom parameter and custom ranges window from showing when selection/parameters have changed
			UINode* current_selected_node = selected_nodes.empty() ? nullptr : selected_nodes[0];
			
			// Display custom parameter window
			if (current_selected_node == prev_selected_node && !selected_nodes.empty() && !current_selected_node->IsGroupRegion())
			{
				ImVec2 pos = ImVec2(context_menu_pos.x, context_menu_pos.y) + offset;
				OpenCustomParameterWindow(selected_nodes[0], simd::vector2(pos.x, pos.y), show_customparam_window);
			}

			if (show_search_window)
			{
				ImVec2 pos = ImGui::GetWindowPos() + ImVec2(ImGui::GetWindowWidth()*0.5f, ImGui::GetWindowHeight()*0.5f) - ImVec2(0.f, 50.f);
				OpenSearchWindow(simd::vector2(pos.x, pos.y));
			}	
		}

		void UIGraphManager::DrawPendingLinks(const LinkEndPoints& input_end_points, ImDrawList* draw_list)
		{
			if (!selected_node_output.empty() && ImGui::IsMouseDown(ImGuiMouseButton_Left))
			{
				assert(output_node);
				ImVec2 p1, p2;
				if (output_node->IsGroupRegion() && static_cast<UIGroupRegion*>(output_node)->IsMouseInside())
				{
					if (output_node->GetInputSlotPos(selected_node_output, p1))
						p2 = ImGui::GetMousePos();
				}
				else
				{
					if (output_node->GetOutputSlotPos(selected_node_output, p1))
					{
						p2 = ImGui::GetMousePos();
						enable_group_link_switching = output_node->IsGroupRegion();
					}
				}
				draw_list->AddBezierCurve(p1, p1 + ImVec2(+50, 0), p2 + ImVec2(-50, 0), p2, IM_COL32(200, 200, 100, 255), 3.0f);
			}
			else if (!input_end_points.empty() && ImGui::IsMouseDown(ImGuiMouseButton_Left))
			{
				for (const auto& input_point : input_end_points)
				{
					assert(input_point.node);
					ImVec2 p1, p2;
					if (input_point.node->IsGroupRegion() && static_cast<UIGroupRegion*>(input_point.node)->IsMouseInside())
					{
						if (input_point.node->GetOutputSlotPos(input_point.slot, p2))
							p1 = ImGui::GetMousePos();
					}
					else
					{
						if (input_point.node->GetInputSlotPos(input_point.slot, p2))
						{
							p1 = ImGui::GetMousePos();
							enable_group_link_switching = input_point.node->IsGroupRegion();
						}
					}
					draw_list->AddBezierCurve(p1, p1 + ImVec2(+50, 0), p2 + ImVec2(-50, 0), p2, IM_COL32(200, 200, 100, 255), 3.0f);
				}
			}
		}

		void UIGraphManager::DrawEstablishedLinks(ImDrawList* draw_list)
		{
			for (auto link_itr = ui_links.begin(); link_itr != ui_links.end(); ++link_itr)
			{
				const UINodeLink* link = link_itr->get();
				const UINode* node_inp = link->input.node;
				const UINode* node_out = link->output.node;
				ImVec2 p1, p2;
				if (node_out->IsGroupRegion() && link->is_group_edge_link)
				{
					if (node_out->GetInputSlotPos(link->output.slot, p1) && node_inp->GetInputSlotPos(link->input.slot, p2))
					{
						float width = IsThickLink(node_inp->GetInputSlotType(link->input.slot)) ? 6.0f : 3.0f;
						draw_list->AddBezierCurve(p1, p1 + ImVec2(+50, 0), p2 + ImVec2(-50, 0), p2, highlight_flow ? GetHighlightColor(false, false, false) : node_inp->GetInputSlotColor(style, link->input.slot), width);
					}
				}
				else if (node_inp->IsGroupRegion() && link->is_group_edge_link)
				{
					if (node_out->GetOutputSlotPos(link->output.slot, p1) && node_inp->GetOutputSlotPos(link->input.slot, p2))
					{
						float width = IsThickLink(node_inp->GetInputSlotType(link->input.slot)) ? 6.0f : 3.0f;
						draw_list->AddBezierCurve(p1, p1 + ImVec2(+50, 0), p2 + ImVec2(-50, 0), p2, highlight_flow ? GetHighlightColor(false, false, false) : node_inp->GetOutputSlotColor(style, link->input.slot), width);
					}
				}
				else
				{
					if (node_out->GetOutputSlotPos(link->output.slot, p1) && node_inp->GetInputSlotPos(link->input.slot, p2))
					{
						float width = IsThickLink(node_inp->GetInputSlotType(link->input.slot)) ? 6.0f : 3.0f;
						draw_list->AddBezierCurve(p1, p1 + ImVec2(+50, 0), p2 + ImVec2(-50, 0), p2, highlight_flow ? GetHighlightColor(false, false, false) : node_inp->GetInputSlotColor(style, link->input.slot), width);
					}
				}
			}
		}

		void UIGraphManager::DrawHighlightedLinks(ImDrawList* draw_list)
		{
			for (auto& node : ui_nodes)
			{
				node->is_highlighted_prev = false;
				node->is_highlighted_after = false;
				node->is_highlighted_selected = false;
				node->highlight_flow = highlight_flow;
			}

			if (highlight_flow && !selected_nodes.empty())
			{
				for (auto& starting_node : selected_nodes)
					HighlightFlow(starting_node, draw_list, true);
			}
		}

		void UIGraphManager::HighlightFlow(UINode* highlight_node, ImDrawList* draw_list, bool start)
		{
			highlight_node->is_highlighted_after = true;
			highlight_node->is_highlighted_prev = true;
			highlight_node->is_highlighted_selected = true;

			typedef Memory::VectorSet<UINode*, Memory::Tag::EffectGraphs> Generation;
			Generation last_gen;
			Generation next_gen;

			// Find input nodes:
			last_gen.insert(highlight_node);
			do {
				for (const auto& link : ui_links)
				{
					auto* node_inp = link->input.node;
					auto* node_out = link->output.node;
					if (last_gen.contains(node_out))
					{
						ImVec2 p1, p2;
						if (node_out->GetOutputSlotPos(link->output.slot, p1) && node_inp->GetInputSlotPos(link->input.slot, p2))
							draw_list->AddBezierCurve(p1, p1 + ImVec2(+50, 0), p2 + ImVec2(-50, 0), p2, GetHighlightColor(true, node_inp->is_highlighted_after, false), 3.f);

						if (!node_inp->is_highlighted_prev)
						{
							node_inp->is_highlighted_prev = true;
							next_gen.insert(node_inp);
						}
					}
				}

				std::swap(last_gen, next_gen);
				next_gen.clear();
			} while (!last_gen.empty());

			// Find output nodes:
			last_gen.insert(highlight_node);
			do {
				for (const auto& link : ui_links)
				{
					auto* node_inp = link->input.node;
					auto* node_out = link->output.node;
					if (last_gen.contains(node_inp))
					{
						ImVec2 p1, p2;
						if (node_out->GetOutputSlotPos(link->output.slot, p1) && node_inp->GetInputSlotPos(link->input.slot, p2))
							draw_list->AddBezierCurve(p1, p1 + ImVec2(+50, 0), p2 + ImVec2(-50, 0), p2, GetHighlightColor(node_out->is_highlighted_prev, true, false), 3.f);

						if (!node_out->is_highlighted_after)
						{
							node_out->is_highlighted_after = true;
							next_gen.insert(node_out);
						}
					}
				}

				std::swap(last_gen, next_gen);
				next_gen.clear();
			} while (!last_gen.empty());
		}

		void UIGraphManager::CaptureReconnect()
		{
			if (multi_select_mode)
			{
				if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
				{
					for (auto link_itr = ui_links.begin(); link_itr != ui_links.end();)
					{
						const auto& link = *link_itr;

						// TODO: support relinking with groups
						if (link->is_group_edge_link || link->input.node->IsGroupRegion() || link->output.node->IsGroupRegion())
						{
							++link_itr;
							continue;
						}

						if (link->output.node == output_node && link->output.slot == selected_node_output)
						{
							reconnect_infos.push_back({ *link, false });
							RemoveLink(link.get());
							link_itr = ui_links.erase(link_itr);
						}
						else if (link->input.node == input_node && link->input.slot == selected_node_input)
						{
							reconnect_infos.push_back({ *link, true });
							RemoveLink(link.get());
							link_itr = ui_links.erase(link_itr);
						}
						else
							++link_itr;
					}
				}
			}
		}

		LinkEndPoints UIGraphManager::UpdateReconnectEndPoints()
		{
			LinkEndPoints input_end_points;

			if (reconnect_infos.empty())
			{
				if (!selected_node_input.empty())
					input_end_points.push_back({ input_node, selected_node_input });
			}
			else
			{
				if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
				{
					// Handle reverting the link if connection fails
					for (const auto& reconnect_info : reconnect_infos)
					{					
						if (reconnect_info.is_input)
						{
							if (selected_node_input.empty())
							{
								input_node = reconnect_info.link.input.node;
								selected_node_input = reconnect_info.link.input.slot;
							}
							input_end_points.push_back({ input_node, selected_node_input });
						}
						else
						{
							if (selected_node_output.empty())
							{
								output_node = reconnect_info.link.output.node;
								selected_node_output = reconnect_info.link.output.slot;
							}
							input_end_points.push_back({ reconnect_info.link.input.node, reconnect_info.link.input.slot });
						}
					}
				}
				else
				{
					for (const auto& reconnect_info : reconnect_infos)
					{
						if (reconnect_info.is_input)
						{
							input_node = nullptr;
							selected_node_input.clear();
							output_node = reconnect_info.link.output.node;
							selected_node_output = reconnect_info.link.output.slot;
						}
						else
						{
							output_node = nullptr;
							selected_node_output.clear();
							input_node = reconnect_info.link.input.node;
							selected_node_input = reconnect_info.link.input.slot;
							input_end_points.push_back({ reconnect_info.link.input.node, reconnect_info.link.input.slot });
						}
					}
				}
			}

			return input_end_points;
		}

		LinkEndPoints UIGraphManager::SetupReconnection()
		{
			CaptureReconnect();
			return UpdateReconnectEndPoints();
		}

		namespace
		{
			uint8_t SlotMaskLengthFromType(GraphType default_type)
			{
				switch (default_type)
				{
					case GraphType::Float:
					case GraphType::Int:
					case GraphType::UInt:
						return 1;
					case GraphType::Float2:
					case GraphType::Int2:
						return 2;
					case GraphType::Float3:
					case GraphType::Int3:
						return 3;
					case GraphType::Float4:
					case GraphType::Int4:
						return 4;
					default:
						return 1;
				}
			}

			uint8_t SlotMaskFromString(const std::string& mask, GraphType default_type)
			{
				if (mask.empty())
					return (uint8_t(1) << SlotMaskLengthFromType(default_type)) - 1;

				uint8_t res = 0;
				for (size_t p = 0; p < mask.length(); p++)
				{
					switch (mask[p])
					{
						case 'x': res |= uint8_t(0b0001); break;
						case 'y': res |= uint8_t(0b0010); break;
						case 'z': res |= uint8_t(0b0100); break;
						case 'w': res |= uint8_t(0b1000); break;
					}
				}

				return res;
			}

			size_t FormatSlotMask(char* buffer, size_t buffer_len, uint8_t mask)
			{
				size_t p = 0;
				if (p + 1 < buffer_len && (mask & 0b0001)) buffer[p++] = 'x';
				if (p + 1 < buffer_len && (mask & 0b0010)) buffer[p++] = 'y';
				if (p + 1 < buffer_len && (mask & 0b0100)) buffer[p++] = 'z';
				if (p + 1 < buffer_len && (mask & 0b1000)) buffer[p++] = 'w';
				if (p < buffer_len) buffer[p] = '\0';
				return p;
			}
		}

		void UIGraphManager::RenderSplitComponentsMenuItems()
		{
			UINode* ui_node = input_node ? input_node : output_node;
			if(!ui_node)
				return;

			auto selected_slot_name = input_node ? selected_node_input : selected_node_output;
			std::string slot_name, slot_mask;
			UINode::GetConnectorNameAndMaskHelper(selected_slot_name, slot_name, slot_mask);

			int slot_idx = input_node ? ui_node->GetInputSlotIndex(slot_name) : ui_node->GetOutputSlotIndex(slot_name);
			assert(slot_idx != -1);

			auto& connector = input_node ? ui_node->input_slots[slot_idx] : ui_node->output_slots[slot_idx];
			GraphType type = UINode::GetMaskTypeHelper(slot_mask, connector.type);

			char buffer[16] = { '\0' };
			const auto current_mask = SlotMaskFromString(slot_mask, connector.type);

			// Show merge option
			if (SlotMaskLengthFromType(type) < SlotMaskLengthFromType(connector.type))
			{
				// Option to merge all together:
				const auto max_mask = SlotMaskFromString("", connector.type);
				const auto last_masks = connector.GetMasks();
				size_t p = 0;
				p += FormatSlotMask(buffer + p, std::size(buffer) - p, SlotMaskFromString("", connector.type));

				if (ImGui::MenuItem(buffer))
				{
					connector.ClearMasks();
					OnSlotComponentsChanged(ui_node, input_node != nullptr, slot_idx, last_masks, connector.GetMasks());
				}

				// All options to merge two things together:
				for (size_t a = 0; a + 1 < last_masks.size(); a++)
				{
					const auto mask_a = SlotMaskFromString(last_masks[a], connector.type);
					for (size_t b = a + 1; b < last_masks.size(); b++)
					{
						const auto mask_b = SlotMaskFromString(last_masks[b], connector.type);

						if (mask_a != mask_b && (mask_a | mask_b) != max_mask)
						{
							p = 0;
							p += FormatSlotMask(buffer + p, std::size(buffer) - p, mask_a | mask_b);

							if (ImGui::MenuItem(buffer))
							{
								connector.ClearMasks();
								for (size_t c = 0; c < last_masks.size(); c++)
									if (c != a && c != b)
										connector.AddMask(last_masks[c]);

								std::vector<std::string> new_masks = { buffer };
								connector.AddMask(buffer);
								OnSlotComponentsChanged(ui_node, input_node != nullptr, slot_idx, { last_masks[a], last_masks[b] }, new_masks);
							}
						}
					}
				}
			}

			// Show split options
			if (SlotMaskLengthFromType(type) > 1)
			{
				for (uint8_t next_mask = 0b0001; next_mask <= 0b1111; next_mask++) // Show options to split into two parts
				{
					if ((current_mask & next_mask) == next_mask && current_mask != next_mask)
					{
						const auto other_mask = current_mask ^ next_mask;

						if (other_mask > next_mask)
						{
							size_t p = 0;
							p += FormatSlotMask(buffer + p, std::size(buffer) - p, next_mask);
							buffer[p++] = ',';
							buffer[p++] = ' ';
							p += FormatSlotMask(buffer + p, std::size(buffer) - p, other_mask);


							if (ImGui::MenuItem(buffer))
							{
								auto new_masks = connector.AddMasksFromFormat(buffer, slot_mask);
								OnSlotComponentsChanged(ui_node, input_node != nullptr, slot_idx, { slot_mask }, new_masks);
							}
						}
					}
				}

				// Show option to split into individual parts
				if (SlotMaskLengthFromType(type) > 2)
				{
					size_t p = 0;
					if ((current_mask & 0b0001)) { buffer[p++] = 'x'; buffer[p++] = ','; buffer[p++] = ' '; }
					if ((current_mask & 0b0010)) { buffer[p++] = 'y'; buffer[p++] = ','; buffer[p++] = ' '; }
					if ((current_mask & 0b0100)) { buffer[p++] = 'z'; buffer[p++] = ','; buffer[p++] = ' '; }
					if ((current_mask & 0b1000)) { buffer[p++] = 'w'; buffer[p++] = ','; buffer[p++] = ' '; }
					if (p > 2)
						p -= 2;

					buffer[p] = '\0';

					if (ImGui::MenuItem(buffer))
					{
						auto new_masks = connector.AddMasksFromFormat(buffer, slot_mask);
						OnSlotComponentsChanged(ui_node, input_node != nullptr, slot_idx, { slot_mask }, new_masks);
					}
				}
			}
		}

		void UIGraphManager::OnSlotComponentsChanged(UINode* uinode, bool is_input, unsigned int slot_index, const std::vector<std::string>& prev_mask, const std::vector<std::string>& masks)
		{
			uinode->CalculateNodeWidth();

			std::string selected_input_slot_name, selected_input_slot_mask;
			UINode::GetConnectorNameAndMaskHelper(selected_node_input, selected_input_slot_name, selected_input_slot_mask);

			std::string selected_output_slot_name, selected_output_slot_mask;
			UINode::GetConnectorNameAndMaskHelper(selected_node_output, selected_output_slot_name, selected_output_slot_mask);

			std::vector<UINodeLink*> connected_links;
			for(const auto& link : ui_links)
			{
				if(masks.empty())
				{
					std::string input_slot_name, input_slot_mask;
					UINode::GetConnectorNameAndMaskHelper(link->input.slot, input_slot_name, input_slot_mask);

					std::string output_slot_name, output_slot_mask;
					UINode::GetConnectorNameAndMaskHelper(link->output.slot, output_slot_name, output_slot_mask);

					if((link->input.node == uinode && input_slot_name == selected_input_slot_name) || (link->output.node == uinode && output_slot_name == selected_output_slot_name))
						connected_links.push_back(link.get());
				}
				else
				{
					if((link->input.node == uinode && link->input.slot == selected_node_input) || (link->output.node == uinode && link->output.slot == selected_node_output))
						connected_links.push_back(link.get());
				}
			}

			m_pOnSplitSlotComponents(*uinode, is_input, slot_index, prev_mask, masks, connected_links, m_pCallbackEventUserContext);
		}

		bool UIGraphManager::AddUINode(const UINode& node, bool select /*=false*/)
		{
			auto found = std::find_if(ui_nodes.begin(), ui_nodes.end(), [&](const std::unique_ptr<UINode>& in_node)
			{
				return in_node->instance_name == node.instance_name;
			});
			if(found == ui_nodes.end())
			{
				if (node.IsGroupRegion())
				{
					auto new_group = std::make_unique<UIGroupRegion>((const UIGroupRegion&)node);
					const auto id = new_group->GetId();
					ui_nodes.insert(ui_nodes.begin(), std::move(new_group)); // Insert new group regions at the first always, so that they would always be in the back before actual nodes to prevent hovering issues
					ui_group_regions[id] = static_cast<UIGroupRegion*>(ui_nodes.begin()->get());
					if (select)
						selected_nodes.push_back(ui_nodes[0].get());
				}
				else
				{
					ui_nodes.emplace_back(new UINode(node));
					ui_nodes.back()->SetDepth((int)ui_nodes.size() - 1);
					ui_nodes.back()->CalculateNodeWidth();
					ui_nodes.back()->SetTransformPos(transform_pos);
					ui_nodes.back()->CalculateInitialNodeRect();
					if (select)
						selected_nodes.push_back(ui_nodes.back().get());
				}
				return true;
			}
			return false;
		}

		void UIGraphManager::AddUILink(const std::string& input_node, const std::string& output_node, const std::string& input_slot, const std::string& input_slot_mask, const std::string& output_slot, const std::string& output_slot_mask)
		{
			// Note:
			// input_node is the node right side of the link (the one linked from left side slots of the node ui)
			// output_node is the node left side of the link (the one linked from right side slots of the node ui)

			auto input = std::find_if(ui_nodes.begin(), ui_nodes.end(), [&](const std::unique_ptr<UINode>& node)
			{
				return node->instance_name == input_node;
			});

			auto output = std::find_if(ui_nodes.begin(), ui_nodes.end(), [&](const std::unique_ptr<UINode>& node)
			{
				return node->instance_name == output_node;
			});

			if(input != ui_nodes.end() && output != ui_nodes.end())
			{
				const auto is_group_internal_input = input->get()->IsGroupRegion() && output->get()->GetParentId() == static_cast<UIGroupRegion*>(input->get())->GetId();
				const auto is_group_internal_output = output->get()->IsGroupRegion() && input->get()->GetParentId() == static_cast<UIGroupRegion*>(output->get())->GetId();

				int input_slot_idx = is_group_internal_input ? input->get()->GetOutputSlotIndex(input_slot) : input->get()->GetInputSlotIndex(input_slot);
				int output_slot_idx = is_group_internal_output ? output->get()->GetInputSlotIndex(output_slot) : output->get()->GetOutputSlotIndex(output_slot);
				if (input_slot_idx < 0 || output_slot_idx < 0)
					return;

				if (is_group_internal_input)
				{
					auto& input_connector = input->get()->output_slots[input_slot_idx];
					input_connector.AddMask(input_slot_mask);
				}
				else
				{
					auto& input_connector = input->get()->input_slots[input_slot_idx];
					input_connector.AddMask(input_slot_mask);
				}
				input->get()->CalculateNodeWidth();

				if (is_group_internal_output)
				{
					auto& output_connector = output->get()->input_slots[output_slot_idx];
					output_connector.AddMask(output_slot_mask);
				}
				else
				{
					auto& output_connector = output->get()->output_slots[output_slot_idx];
					output_connector.AddMask(output_slot_mask);
				}
				output->get()->CalculateNodeWidth();

				const std::string& input_mask = input_slot_mask.empty() ? std::string() : "." + input_slot_mask;
				const std::string& output_mask = output_slot_mask.empty() ? std::string() : "." + output_slot_mask;
				CreateLink(input->get(), output->get(), input_slot + input_mask, output_slot + output_mask, is_group_internal_input || is_group_internal_output);
			}
		}

		void UIGraphManager::AddUIUnusedLink(const std::string& source_node, const std::string& slot, const std::string& mask, const bool is_input)
		{
			auto itr = std::find_if(ui_nodes.begin(), ui_nodes.end(), [&](const std::unique_ptr<UINode>& node)
			{
				return node->instance_name == source_node;
			});

			if (itr != ui_nodes.end())
			{
				auto& node = *itr->get();
				int slot_idx = is_input ? node.GetInputSlotIndex(slot) : node.GetOutputSlotIndex(slot);
				assert(slot_idx != -1);

				auto& connector = is_input ? node.input_slots[slot_idx] : node.output_slots[slot_idx];
				connector.AddMask(mask);
				node.CalculateNodeWidth();
			}
		}

		void UIGraphManager::AddUIChildren(const unsigned group_index, const std::string& node_instance)
		{
			auto found_group = ui_group_regions.find(group_index);
			if (found_group != ui_group_regions.end())
			{
				auto found = std::find_if(ui_nodes.begin(), ui_nodes.end(), [&](const std::unique_ptr<UINode>& node)
				{
					return node->instance_name == node_instance;
				});
				std::vector<UINode*> temp;
				temp.push_back(found->get());
				found->get()->SetTransformPos(transform_pos);
				found->get()->SetParentId(-1);
				found_group->second->AddChildren(temp);
			}
		}

		bool UIGraphManager::AddOutputExtensionPoint(const unsigned group, const unsigned index, const std::string& slot_name, const GraphType& type, const UIParameter* parameter, const simd::vector2& position)
		{
			unsigned id = index;

			UIExtensionPoint* extension_point = nullptr;
			auto found = ui_extension_points.find(id);
			if(found == ui_extension_points.end())
			{
				ui_nodes.emplace_back(new UIExtensionPoint(id, group));
				ui_extension_points[id] = static_cast<UIExtensionPoint*>(ui_nodes.back().get());
				extension_point = ui_extension_points[id];
				extension_point->SetInputParameter(parameter);
			}
			else
				extension_point = static_cast<UIExtensionPoint*>(found->second);
			
			// add the input slots
			auto found_slot = std::find_if(extension_point->input_slots.begin(), extension_point->input_slots.end(), [&](const UINode::UIConnectorInfo& connector)
			{
				return connector.name == slot_name;
			});
			if(found_slot == extension_point->input_slots.end())
				extension_point->AddInputSlot(slot_name, type, false);
			
			extension_point->SetUIPosition(position);
			extension_point->CalculateNodeWidth();

			return true;
		}

		bool UIGraphManager::AddInputExtensionPoint(const unsigned group, const unsigned index, const std::string& slot_name, const GraphType& type, const UIParameter* parameter, const simd::vector2& position)
		{
			unsigned id = index;

			UIExtensionPoint* extension_point = nullptr;
			if(ui_extension_points.empty())
			{
				// this is the starting input extension point
				ui_nodes.emplace_back(new UIExtensionPoint(id, group));
				ui_extension_points[id] = static_cast<UIExtensionPoint*>(ui_nodes.back().get());
				extension_point = ui_extension_points[id];
			}
			else
			{
				auto found = ui_extension_points.find(id);
				if(found == ui_extension_points.end())
				{
					auto found = ui_extension_points.lower_bound(id);
					if(found == ui_extension_points.begin() || std::prev(found)->second->group != group)
					{
						// this is the starting input extension point
						ui_nodes.emplace_back(new UIExtensionPoint(id, group));
						ui_extension_points[id] = static_cast<UIExtensionPoint*>(ui_nodes.back().get());
						extension_point = ui_extension_points[id];
					}
					else
					{
						// partner this input to the nearest extension point
						extension_point = std::prev(found)->second;
					}
				}
				else
					extension_point = static_cast<UIExtensionPoint*>(found->second);
			}
			assert(extension_point);

			// set the output parameter before setting the output slots as it can be cleared depending if the parameter is changed
			extension_point->SetOutputParameter(parameter);

			// add the output slots
			auto found_slot = std::find_if(extension_point->output_slots.begin(), extension_point->output_slots.end(), [&](const UINode::UIConnectorInfo& connector)
			{
				return connector.name == slot_name;
			});
			if(found_slot == extension_point->output_slots.end())
				extension_point->AddOutputSlot(slot_name, type, false);

			if(extension_point->ext_in_parameter == nullptr)
				extension_point->SetUIPosition(position);
			extension_point->CalculateNodeWidth();

			return true;
		}

		std::optional<std::string> UIGraphManager::GetExtensionPointName(const unsigned index, bool get_closest)
		{
			if (auto found = ui_extension_points.find(index); found != ui_extension_points.end())
				return found->second->instance_name;
			else if (found = ui_extension_points.lower_bound(index); found != ui_extension_points.begin() && get_closest)
				return std::prev(found)->second->instance_name;
			else
				return std::nullopt;
		}

		void UIGraphManager::FinalizeExtensionPoints(const std::map<unsigned, std::vector<std::string>>& slot_selections)
		{
			// finalize the allowable range for each extension point
			for(auto itr = ui_extension_points.begin(); itr != ui_extension_points.end(); ++itr)
			{
				UIExtensionPoint* prev_ext_point = (itr == ui_extension_points.begin()) ? nullptr : std::prev(itr)->second;
				UIExtensionPoint* next_ext_point = (std::next(itr) == ui_extension_points.end()) ? nullptr : std::next(itr)->second;
				UIExtensionPoint* ext_point = itr->second;

				if (ext_point->ext_in_parameter)
				{
					bool disable_merge = false;

					// disable merging if there is something connected to the input slot of the current extension point other than the previous extension point
					if (prev_ext_point)
						for (const auto& link : ui_links)
							if (link->input.node == ext_point && link->output.node != prev_ext_point)
								disable_merge = true;

					const auto& elements = ext_point->ext_in_parameter->GetNames();
					const auto min_value = prev_ext_point && prev_ext_point->ext_in_parameter ? GetEnumValue(prev_ext_point->ext_in_parameter.get()) : 0;
					const auto max_value = ext_point->ext_out_parameter ? GetEnumValue(ext_point->ext_out_parameter.get()) : (elements.size() - 1);

					Renderer::DrawCalls::JsonDocument doc;
					doc.SetObject();
					ext_point->ext_in_parameter->GetLayout(doc, doc.GetAllocator());

					auto& layout = GetOrCreateMember(doc, L"layout", doc.GetAllocator(), rapidjson::kObjectType);
					GetOrCreateMember(layout, L"min", doc.GetAllocator()).SetUint(unsigned(min_value));
					GetOrCreateMember(layout, L"max", doc.GetAllocator()).SetUint(unsigned(max_value));
					GetOrCreateMember(layout, L"disable_min", doc.GetAllocator()).SetBool(disable_merge);

					ext_point->ext_in_parameter->SetLayout(doc);
				}

				if (ext_point->ext_out_parameter)
				{
					bool disable_merge = false;

					// disable merging if there is something connected to the input slot of the current extension point other than the previous extension point
					if (next_ext_point)
						for (const auto& link : ui_links)
							if (link->input.node == next_ext_point && link->output.node != ext_point)
								disable_merge = true;

					const auto& elements = ext_point->ext_out_parameter->GetNames();
					const auto min_value = ext_point->ext_in_parameter ? GetEnumValue(ext_point->ext_in_parameter.get()) : 0;
					const auto max_value = next_ext_point && next_ext_point->ext_out_parameter ? GetEnumValue(next_ext_point->ext_out_parameter.get()) : (elements.size() - 1);

					Renderer::DrawCalls::JsonDocument doc;
					doc.SetObject();
					ext_point->ext_out_parameter->GetLayout(doc, doc.GetAllocator());

					auto& layout = GetOrCreateMember(doc, L"layout", doc.GetAllocator(), rapidjson::kObjectType);
					GetOrCreateMember(layout, L"min", doc.GetAllocator()).SetUint(unsigned(min_value + 1));
					GetOrCreateMember(layout, L"max", doc.GetAllocator()).SetUint(unsigned(max_value + 1));
					GetOrCreateMember(layout, L"disable_max", doc.GetAllocator()).SetBool(disable_merge);

					ext_point->ext_out_parameter->SetLayout(doc);
				}

				// store the slot selections
				const auto& found_group = slot_selections.find(ext_point->group);
				if(found_group != slot_selections.end())
					ext_point->slot_selections = found_group->second;
			}
		}

		void UIGraphManager::HandleExtensionPointMessages(UIExtensionPoint * ext_node)
		{
			if(ext_node->selected_split_index != -1)
			{
				m_pOnAddNewExtensionPoint(ext_node->split_selections[ext_node->selected_split_index], ext_node->GetUIPosition(), m_pCallbackEventUserContext);
				ext_node->selected_split_index = -1;
			}

			if(!ext_node->changed_extension_point.empty())
			{
				m_pOnExtensionPointStageChanged(ext_node->changed_extension_point, ext_node->previous_extension_point, m_pCallbackEventUserContext);
				ext_node->changed_extension_point.clear();
				ext_node->previous_extension_point.clear();
			}

			if(ext_node->selected_slot_index != -1)
			{
				simd::vector2 input_position, output_position;
				if(ext_node->selected_slot_is_input)
				{
					auto ext_point_itr = ui_extension_points.find(ext_node->id);
					auto prev_ext_node = ext_point_itr != ui_extension_points.begin() ? std::prev(ext_point_itr)->second : nullptr;
					if(prev_ext_node)
						input_position = prev_ext_node->GetUIPosition();
					else
						input_position = ext_node->GetUIPosition();
					output_position = ext_node->GetUIPosition();
				}
				else
				{
					auto ext_point_itr = ui_extension_points.find(ext_node->id);
						input_position = ext_node->GetUIPosition();
					auto next_ext_node = std::next(ext_point_itr) != ui_extension_points.end() ? std::next(ext_point_itr)->second : nullptr;
					if(next_ext_node)
						output_position = next_ext_node->GetUIPosition();
					else
						output_position = ext_node->GetUIPosition();
				}

				m_pOnAddNewExtensionPointSlot(ext_node->slot_selections[ext_node->selected_slot_index], ext_node->parameter_value, input_position, output_position, m_pCallbackEventUserContext);
				ext_node->selected_slot_index = -1;
				ext_node->selected_slot_is_input = false;
				ext_node->parameter_value.clear();
			}
		}

		bool UIGraphManager::HasExtensionPointsByGroup(unsigned group)
		{
			auto found = std::find_if(ui_extension_points.begin(), ui_extension_points.end(), [&](const std::pair<unsigned, UIExtensionPoint*>& extension_point)
			{
				return extension_point.second->group == group;
			});
			return found != ui_extension_points.end();
		}

		unsigned UIGraphManager::GetDivisionArea(const UINode* node) const
		{
			auto node_pos = node->GetNodeRect().min - transform_pos;
			UnscalePositionHelper(node_pos);
			if (has_division)
			{
				if (node_pos.y < division_height)
					return group_0;
				else
					return group_1;
			}
			else
			{
				return group_0;
			}
		}

		bool UIGraphManager::CheckNodesDivisionArea(const std::vector<UINode*>& nodes) const
		{
			if (!nodes.empty())
			{
				auto current_division = GetDivisionArea(nodes[0]);
				for (const auto& node : nodes)
				{
					if (const auto division = GetDivisionArea(node); division != current_division)
						return false;
				}
			}
			return true;
		}

		void UIGraphManager::AddGroupRegionNode(const std::vector<UINode*>& to_group, const simd::vector2& pos)
		{
			// temp code for setting id
			static unsigned id = 0;
			++id;

			// check if the selected nodes to group are in the same division area
			if (!CheckNodesDivisionArea(to_group))
			{
				SetErrorMessage("Cannot create group box.  Selected nodes needs to be in the same shader type area.");
				return;
			}

			auto new_group = std::make_unique<UIGroupRegion>(id, -1, to_group);
			if (to_group.empty())
				new_group->SetUIPosition(pos);
			if (m_pOnAddNodeCallback(*new_group, m_pCallbackEventUserContext))
			{
				ui_nodes.insert(ui_nodes.begin(), std::move(new_group)); // Insert new group regions at the first always, so that they would always be in the back before actual nodes to prevent hovering issues
				ui_group_regions[id] = static_cast<UIGroupRegion*>(ui_nodes.begin()->get());
			}
		}

		void UIGraphManager::RemoveGroupRegionNode(const UINode* selected_node, const bool last)
		{
			auto found = ui_group_regions.find(static_cast<const UIGroupRegion*>(selected_node)->GetId());
			if (found != ui_group_regions.end())
			{
				ui_group_regions.erase(found);
			}

			auto found_node = std::find_if(ui_nodes.begin(), ui_nodes.end(), [&](const auto& node)
			{
				return selected_node == node.get();
			});
			if (found_node != ui_nodes.end())
			{
				m_pOnRemoveNodeCallback(*found_node->get(), last, m_pCallbackEventUserContext);
				ui_nodes.erase(found_node);
			}
		}

		void UIGraphManager::RefreshGroupNodeLinks(const UIGroupRegion* group_node, const std::function<void()>& func)
		{
			for(const auto& link : ui_links)
			{
				if (link->input.node == group_node || link->output.node == group_node)
				{
					RemoveLink(link.get());
					m_pOnGraphChangedCallback(m_pCallbackEventUserContext);
				}
			}

			func();

			for (const auto& link : ui_links)
			{
				if (link->input.node == group_node || link->output.node == group_node)
				{
					ConnectNodes(link->input.node, link->output.node, link->input.slot, link->output.slot, link->is_group_edge_link);
				}
			}
		}

		void UIGraphManager::AddNode(const std::string type_id, const simd::vector2& pos)
		{
			if (type_id == GroupBoxNodeName)
			{
				AddGroupRegionNode(std::vector<UINode*>(), pos);
			}
			else
			{
				std::unique_ptr<UINode> new_ui_node = std::make_unique<UINode>(type_id);
				new_ui_node->SetUIPosition(pos);
				if (m_pOnAddNodeCallback(*new_ui_node, m_pCallbackEventUserContext))
				{
					new_ui_node->CalculateNodeWidth();
					new_ui_node->SetTransformPos(transform_pos);
					new_ui_node->CalculateInitialNodeRect();
					new_ui_node->SetDepth((int)ui_nodes.size());
					
					// add to a group region if applicable		
					for (auto group_region : ui_group_regions)
					{
						const auto& group_rect = group_region.second->GetNodeRect();			
						if (new_ui_node->IntersectsBox(group_rect.min, group_rect.max))
						{
							std::vector<UINode*> temp;
							temp.push_back(new_ui_node.get());
							group_region.second->AddChildren(temp);
							refresh_group_region_sizes = true;
							m_pOnChildAdded(*new_ui_node, group_region.first, m_pCallbackEventUserContext);
							break;
						}
					}

					ui_nodes.push_back(std::move(new_ui_node));
				}
			}	
		}

		void UIGraphManager::DeleteNode(const UINode* selected_node, const bool last)
		{
			// remove links
			auto found_links = std::remove_if(ui_links.begin(), ui_links.end(), [&](const std::unique_ptr<UINodeLink>& link)->bool
			{
				if (link->input.node == selected_node || link->output.node == selected_node)
				{
					RemoveLink(link.get());
					return true;
				}
				else
					return false;
			});
			ui_links.erase(found_links, ui_links.end());

			if (selected_node->IsGroupRegion())
			{
				RemoveGroupRegionNode(selected_node, last);
			}
			else
			{
				// remove from parent group regions if applicable
				auto found = ui_group_regions.find(selected_node->GetParentId());
				if (found != ui_group_regions.end())
				{
					std::vector<UINode*> temp;
					temp.push_back(const_cast<UINode*>(selected_node));
					found->second->RemoveChildren(temp);
				}

				// remove from the ui nodes list
				auto found_node = std::find_if(ui_nodes.begin(), ui_nodes.end(), [&](const std::unique_ptr<UINode>& node)
				{
					return selected_node == node.get();
				});
				if(found_node != ui_nodes.end())
				{
					m_pOnRemoveNodeCallback(*found_node->get(), last, m_pCallbackEventUserContext);
					ui_nodes.erase(found_node);
				}
			}
		}

		void UIGraphManager::CreateLink(UINode* input_node, UINode* output_node, const std::string& input_slot, const std::string& output_slot, const bool is_group_edge_link)
		{
			ui_links.emplace_back(new UINodeLink(input_node, output_node, input_slot, output_slot, is_group_edge_link));

			if (!is_group_edge_link)
			{
				auto found_group_in = ui_group_regions.find(input_node->GetParentId());
				auto found_group_out = ui_group_regions.find(output_node->GetParentId());
				const auto groupid_in = found_group_in != ui_group_regions.end() ? found_group_in->second->GetId() : -1;
				const auto groupid_out = found_group_out != ui_group_regions.end() ? found_group_out->second->GetId() : -1;
				if (groupid_in != groupid_out)
				{
					if (groupid_in != -1) found_group_in->second->IncrementExternalLinks();
					if (groupid_out != -1) found_group_out->second->IncrementExternalLinks();
				}
			}
		}

		void UIGraphManager::RemoveLink(const UINodeLink* link)
		{
			UINode& input_node = *link->input.node;
			UINode& output_node = *link->output.node;

			std::string input_slot_name, input_slot_mask;
			UINode::GetConnectorNameAndMaskHelper(link->input.slot, input_slot_name, input_slot_mask);

			std::string output_slot_name, output_slot_mask;
			UINode::GetConnectorNameAndMaskHelper(link->output.slot, output_slot_name, output_slot_mask);

			int input_slot_idx = -1;
			int output_slot_idx = -1;
			if (link->is_group_edge_link)
			{
				if (input_node.IsGroupRegion())
				{
					input_slot_idx = input_node.GetOutputSlotIndex(input_slot_name);
					output_slot_idx = output_node.GetOutputSlotIndex(output_slot_name);
				}
				else
				{
					input_slot_idx = input_node.GetInputSlotIndex(input_slot_name);
					output_slot_idx = output_node.GetInputSlotIndex(output_slot_name);
				}
			}
			else
			{
				input_slot_idx = input_node.GetInputSlotIndex(input_slot_name);
				output_slot_idx = output_node.GetOutputSlotIndex(output_slot_name);
			}
			assert(input_slot_idx != -1 && output_slot_idx != -1);
			m_pOnUnlinkNodesCallback(input_node, output_node, input_slot_idx, input_slot_mask, output_slot_idx, output_slot_mask, m_pCallbackEventUserContext);

			if (!link->is_group_edge_link)
			{
				auto found_group_in = ui_group_regions.find(input_node.GetParentId());
				auto found_group_out = ui_group_regions.find(output_node.GetParentId());
				const auto groupid_in = found_group_in != ui_group_regions.end() ? found_group_in->second->GetId() : -1;
				const auto groupid_out = found_group_out != ui_group_regions.end() ? found_group_out->second->GetId() : -1;
				if (groupid_in != groupid_out)
				{
					if (groupid_in != -1) found_group_in->second->DecrementExternalLinks();
					if (groupid_out != -1) found_group_out->second->DecrementExternalLinks();
				}
			}
		}

		static bool IsScalarGraphType(GraphType ui_type)
		{
			return ui_type == GraphType::Float || ui_type == GraphType::Bool || ui_type == GraphType::Int || ui_type == GraphType::UInt;
		}
		static bool GraphTypesCompatible(GraphType input_type, GraphType output_type)
		{
			if (input_type == output_type) return true;
			if (input_type == GraphType::Float3 && output_type == GraphType::NormField) return true;
			if (input_type == GraphType::NormField) return false;
			return IsScalarGraphType(output_type);
		}

		bool UIGraphManager::IsLinkingDisabled(const UINode& input, const UINode& output)
		{	
			if (const auto in_parent_id = input.GetParentId(); in_parent_id != unsigned(-1))
			{
				if (const auto out_parent_id = output.GetParentId(); in_parent_id != out_parent_id && !output.IsGroupRegion())
				{
					const auto found = ui_group_regions.find(in_parent_id);
					if (found != ui_group_regions.end())
						return found->second->IsExternalLinkingDisabled();
				}
			}
			if (const auto out_parent_id = output.GetParentId(); out_parent_id != unsigned(-1))
			{
				if (const auto in_parent_id = input.GetParentId(); out_parent_id != in_parent_id && !input.IsGroupRegion())
				{
					const auto found = ui_group_regions.find(out_parent_id);
					if (found != ui_group_regions.end())
						return found->second->IsExternalLinkingDisabled();
				}
			}
			return false;
		}

		bool UIGraphManager::CanGroupRejectNode(const UIGroupRegion* group_node, const UINode* node)
		{
			if (!group_node->IsExternalLinkingDisabled())
				return true;

			if (node->GetParentId() == group_node->GetId())
			{
				auto found_link = std::find_if(ui_links.begin(), ui_links.end(), [&](const std::unique_ptr<UINodeLink>& link)
				{
					return link->input.node == node || link->output.node == node;
				});
				return found_link == ui_links.end();
			}
			return false;
		}

		bool UIGraphManager::CanGroupAcceptNode(const UIGroupRegion* group_node, const UINode* node)
		{
			if (!group_node->IsExternalLinkingDisabled())
				return true;

			auto found_link = std::find_if(ui_links.begin(), ui_links.end(), [&](const std::unique_ptr<UINodeLink>& link)
			{
				if (link->is_group_edge_link)
					return false;
				if (link->input.node == node)
					return link->output.node->GetParentId() != group_node->GetId();
				if (link->output.node == node)
					return link->input.node->GetParentId() != group_node->GetId();
				return false;
			});		
			return found_link == ui_links.end();
		}

		void UIGraphManager::ConnectNodes(UINode* input_node, UINode* output_node, const std::string& selected_node_input, const std::string& selected_node_output, const bool reference_group_edge_link /*= true*/)
		{
			// Note:
			// input_node is the node right side of the link (the one linked from left side slots of the node ui)
			// output_node is the node left side of the link (the one linked from right side slots of the node ui)
			// selected_node_input is the currently selected input slot from the input node (the slot from the left side of the node ui)
			// selected_node_output is the currently selected output slot from the output node (the slot from the right side of the node ui)

			assert(input_node && output_node);

			if (input_node == output_node)
			{
				m_pOnGraphChangedCallback(m_pCallbackEventUserContext);
				return;
			}

			UINode& input = *input_node;
			UINode& output = *output_node;

			if (IsLinkingDisabled(input, output))
			{
				SetErrorMessage("Groups that have input/output slots cannot have their child nodes be connected outside");
				return;
			}

			std::string input_slot_name, input_slot_mask;
			UINode::GetConnectorNameAndMaskHelper(selected_node_input, input_slot_name, input_slot_mask);

			std::string output_slot_name, output_slot_mask;
			UINode::GetConnectorNameAndMaskHelper(selected_node_output, output_slot_name, output_slot_mask);

			int input_slot_idx = -1;
			int output_slot_idx = -1;
			auto default_input_type = GraphType::Invalid;
			auto default_output_type = GraphType::Invalid;
			auto is_group_edge_input_link = false;
			auto is_group_edge_output_link = false;
			if (input.IsGroupRegion() && (static_cast<UIGroupRegion*>(&input)->IsMouseInside() && reference_group_edge_link))
			{
				input_slot_idx = input.GetOutputSlotIndex(input_slot_name);
				default_input_type = input.output_slots[input_slot_idx].type;
				is_group_edge_input_link = true;
			}
			else
			{
				input_slot_idx = input.GetInputSlotIndex(input_slot_name);
				default_input_type = input.input_slots[input_slot_idx].type;
			}
			if (output.IsGroupRegion() && (static_cast<UIGroupRegion*>(&output)->IsMouseInside() && reference_group_edge_link))
			{
				output_slot_idx = output.GetInputSlotIndex(output_slot_name);
				default_output_type = output.input_slots[output_slot_idx].type;
				is_group_edge_output_link = true;
			}
			else
			{
				output_slot_idx = output.GetOutputSlotIndex(output_slot_name);
				default_output_type = output.output_slots[output_slot_idx].type;
			}

			GraphType input_type = UINode::GetMaskTypeHelper(input_slot_mask, default_input_type);
			GraphType output_type = UINode::GetMaskTypeHelper(output_slot_mask, default_output_type);

			if (!GraphTypesCompatible(input_type, output_type))
			{
				SetErrorMessage("Cannot connect slots with different types");
				m_pOnGraphChangedCallback(m_pCallbackEventUserContext);
				return;
			}

			if(m_pOnLinkNodesCallback(input, output, input_slot_idx, input_slot_mask, output_slot_idx, output_slot_mask, m_pCallbackEventUserContext))
			{
				// check if the selected input slot is already linked
				auto found_link = std::find_if(ui_links.begin(), ui_links.end(), [&](const std::unique_ptr<UINodeLink>& link)
				{
					if (link->input.node == input_node && link->input.slot == selected_node_input)
						return (is_group_edge_input_link == false || is_group_edge_input_link == link->is_group_edge_link);
					return false;
				});

				if(found_link != ui_links.end())
				{
					// input slot already has a connection, just update it to the new link (Note: consumer graph should handle this as well)
					if (is_group_edge_input_link == false || is_group_edge_input_link == (*found_link)->is_group_edge_link)
					{
						(*found_link)->output.node = output_node;
						(*found_link)->output.slot = selected_node_output;
					}
				}
				else
					CreateLink(input_node, output_node, selected_node_input, selected_node_output, is_group_edge_input_link || is_group_edge_output_link);
			}
		}

		void UIGraphManager::OpenCustomParameterWindow(UINode* node, const simd::vector2& window_pos, bool show)
		{
			ImGui::SetNextWindowPos( ImVec2( window_pos.x, window_pos.y ), ImGuiCond_Always );

			const auto popup_id = ImGui::GetCurrentWindow()->GetID("##CustomParameterWindow");

			if(show)
				ImGui::OpenPopup(popup_id);

			if (ImGui::BeginPopupEx(popup_id, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings))
			{
				ImGui::Text("Custom Parameter");
				ImGui::Separator();

				ImGui::PushItemWidth(std::max(250.f, ImGui::GetContentRegionAvail().x));

				char text[256];
				ImFormatString(text, std::size(text), "%s", node->custom_parameter.c_str());
				if(ImGui::InputTextWithHint("##custom_param", "Custom Parameter Name", text, std::size(text)))
					node->custom_parameter = text;

				if (ImGui::IsItemDeactivatedAfterEdit())
				{
					node->custom_parameter = text;
					m_pOnCustomParameterSet(*node, text, m_pCallbackEventUserContext);
					if (!node->HasCustomRanges())
						ImGui::CloseCurrentPopup();
				}

				if (node->HasCustomRanges())
				{
					if (ImGui::CollapsingHeader("Custom Layout", ImGuiTreeNodeFlags_DefaultOpen))
					{
						UIParameter* changed_parameter = nullptr;
						bool add_seperator = false;
						for (const auto& param : node->parameters)
						{
							if (!param->HasLayout())
								continue;

							if (add_seperator)
								ImGui::Separator();

							if(param->OnRenderLayoutEditor())
								changed_parameter = param.get();

							add_seperator = true;
						}

						if (changed_parameter)
							m_pOnCustomRangeCallback(*node, m_pCallbackEventUserContext);
					}
				}

				ImGui::PopItemWidth();
				ImGui::EndPopup();
			}
		}

		const UINode* FindNodeByName(const std::string& keyword, const std::vector<std::unique_ptr<UINode>>& nodes, std::vector<std::string>& previous_searches)
		{
			for (auto node_itr = nodes.begin(); node_itr != nodes.end(); ++node_itr)
			{
				const auto node = node_itr->get();
				const auto& name = node->GetInstanceName();
				const auto text = TrimChar(keyword, '\0');
				if (CaseInsensitiveFind(name, text) != -1 && std::find(previous_searches.begin(), previous_searches.end(), name) == previous_searches.end())
					return node;
			}
			return nullptr;
		}

		void UIGraphManager::OpenSearchWindow(const simd::vector2& window_pos)
		{
			float window_width = 250.f;
			bool opened = true;
			ImGui::SetNextWindowFocus();
			ImGui::SetNextWindowSize(ImVec2(window_width, 0.f), ImGuiCond_Always);
			ImGui::SetNextWindowPos(ImVec2(window_pos.x, window_pos.y), ImGuiCond_Always);
			static std::vector<std::string> previous_searches;

			auto end_window = [&]()
			{
				show_search_window = false;
				previous_searches.clear();
				ImGui::End();
			};

			if (!ImGui::Begin("Search Node", &opened, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings))
			{
				end_window();
				return;
			}

			if (opened == false || ImGui::IsKeyDown(ImGuiKey_Escape))
			{
				end_window();
				return;
			}

			ImGui::Text("Find: ");
			ImGui::SameLine();

			ImGui::SetKeyboardFocusHere();
			
			static std::string buffer;
			ImGui::PushItemWidth(window_width - 10.f);
			if (ImGuiEx::InputText("##search_node", buffer, 1024, nullptr, ImVec2(0.f,0.f), ImGuiInputTextFlags_EnterReturnsTrue))
			{
				auto search = [&]() -> bool
				{
					auto found_node = FindNodeByName(buffer, ui_nodes, previous_searches);
					if (found_node)
					{
						const auto& pos = found_node->GetUIPosition();
						const auto& name = found_node->GetInstanceName();
						previous_searches.push_back(name);
						FocusToPos(pos, true);
						return true;
					}
					return false;
				};
				if (!search() && !previous_searches.empty())
				{
					previous_searches.clear();
					search();
				}
			}			
			ImGui::PopItemWidth();

			if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !(ImGui::IsWindowHovered() || ImGui::IsItemHovered()))
				show_search_window = false;

			ImGui::End();
		}
	}
}
