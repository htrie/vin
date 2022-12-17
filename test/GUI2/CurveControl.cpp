#include <rapidjson/document.h>
#include <rapidjson/istreamwrapper.h>
#include <rapidjson/error/en.h>

#include <rapidjson/stringbuffer.h>
#include <rapidjson/prettywriter.h>

#include "CurveControl.h"
#include "Common/Utility/StringManipulation.h"
#include "Visual/GUI2/imgui/imgui.h"
#include "Visual/GUI2/Icons.h"
#include "Visual/Renderer/DrawCalls/EffectGraphParameter.h"

#include "Include/magic_enum/magic_enum.hpp"

namespace Device::GUI
{
	namespace {
		const ImColor ColorFrame(0.2f, 0.2f, 0.2f);
		const ImColor ColorHelpAxis(0.25f, 0.25f, 0.25f);
		const ImColor ColorSelectedAxis(0.0f, 0.45f, 0.0f);
		const ImColor ColorSelectedAxisActive(0.0f, 0.65f, 0.0f);
		const ImColor ColorGraph(1.0f, 1.0f, 0.0f);
		const ImColor ColorGraphPoint(0.4f, 0.4f, 0.4f);
		const ImColor ColorGraphPointBorder(0.0f, 0.0f, 0.0f);
		const ImColor ColorDerivative(0.8f, 0.0f, 0.0f);
		const ImColor ColorSpline(1.0f, 1.0f, 1.0f);
		const ImColor ColorVariance(0.8f, 0.0f, 0.0f);
		constexpr float ScaleSliderSensitivity = 1.0f / 25.0f;
		constexpr float VarianceSliderSensitivity = 1.0f / 100.0f;
		constexpr float MinScale = 1e-2f;
		constexpr float DerivativeScale = 0.1f;
		constexpr float LineThickness = 1.0f;
		constexpr const char* PointEditPopup = "##ValuePopup";
		constexpr const char* ColorPointEditPopup = "##ValueColorPopup";
		constexpr const char* CopyPasteErrorPopup = "Failed to parse Clipboard##CopyPasteError";
		constexpr const char* CopyRawDataPopup = "##CopyRawDataPopup";

		enum KeyBinding {
			KeyShift = 0,
			KeyCtrl = 1,
			KeyAlt = 2,
		};

		constexpr auto KeyMoveAll = KeyBinding::KeyShift;
		constexpr auto KeyIgnoreSnap = KeyBinding::KeyCtrl;
		constexpr auto KeyPointSelect = KeyBinding::KeyShift;
		constexpr auto KeyEditPoint = KeyBinding::KeyCtrl;
		constexpr auto KeyReverseMode = KeyBinding::KeyCtrl;

		enum class ScaleGraphMode {
			ByFactor,
			ToRange,
		};

		enum class SelectorState
		{
			Simple,
			Button,
			Select,
			Post,
		};

		enum SelectorStage : unsigned
		{
			SelectorStage_Init,
			SelectorStage_DropDown,
			SelectorStage_DropDownClosed,
			SelectorStage_Buttons,
			SelectorStage_Graphs,
			SelectorStage_Post,
		};

		struct ColorTrackPoint
		{
			size_t index[4] = { 0,0,0,0 };
		};

		struct TrackView
		{
			simd::vector4* segments = nullptr;
			float* times = nullptr;
			size_t num_segments = 0;
		};

		template<typename T> class SizeableArrayView {
		private:
			T* ptr;
			size_t cur_size;
			size_t max_size;
			size_t min_size;

		public:
			SizeableArrayView() : ptr(nullptr), cur_size(0), max_size(0), min_size(0) {}
			SizeableArrayView(T* ptr, size_t size, size_t max_size, size_t min_size) : ptr(ptr), cur_size(size), max_size(max_size), min_size(min_size) { assert(min_size <= cur_size && cur_size <= max_size); }

			T* data() const { return ptr; }
			size_t size() const { return cur_size; }
			size_t get_max_size() const { return max_size; }
			size_t get_min_size() const { return min_size; }
			T* begin() const { return ptr; }
			T* end() const { return ptr + cur_size; }
			T& operator[](size_t i) { assert(i < cur_size); return ptr[i]; }

			T& front() const { assert(cur_size > 0); return ptr[0]; }
			T& back() const { assert(cur_size > 0); return ptr[cur_size - 1]; }

			void push_back(const T& v) 
			{
				assert(cur_size < max_size);
				ptr[cur_size++] = v;
			}

			void pop_back()
			{
				assert(cur_size > min_size);
				cur_size--;
			}

			void insert(T* pos, const T& v)
			{
				assert(pos >= ptr && pos <= ptr + cur_size);
				const auto idx = pos - ptr;
				insert(idx, v);
			}

			void insert(size_t pos, const T& v)
			{
				assert(pos <= cur_size);
				assert(cur_size < max_size);
				for (size_t a = cur_size; a > pos; a--)
					ptr[a] = ptr[a - 1];

				cur_size++;
				ptr[pos] = v;
			}

			void erase(T* pos)
			{
				assert(pos >= ptr && pos < ptr + cur_size);
				const auto idx = pos - ptr;
				erase(idx);
			}

			void erase(size_t pos)
			{
				assert(pos < cur_size);
				assert(cur_size > min_size);
				for (size_t a = pos; a + 1 < cur_size; a++)
					ptr[a] = ptr[a + 1];

				cur_size--;
			}

			void erase(T* s, T* e)
			{
				assert(s >= ptr && s <= ptr + cur_size);
				assert(e >= ptr && e <= ptr + cur_size);
				if (s == e)
					return;
				
				assert(size_t(e - s) <= cur_size - min_size);
				for (; e != end(); e++, s++)
					*s = *e;

				cur_size = s - ptr;
			}

			bool empty() const { return cur_size == 0; }
			bool can_insert() const { return cur_size < max_size; }
			bool can_erase() const { return cur_size > min_size; }
		};

		struct PersistentData {
		private:
			bool is_in_popout = false;
			ImGuiID dragging_id = 0;
			size_t drag_button = 0;
			size_t drag_point = ~size_t(0);
			bool drag_ended = false;
			ImGuiID derivative_id = 0;
			int derivative_edit = 0;

		public:
			ImGuiID intern_id = 0;
			ImGuiID popout_id = 0;
			int frame = 0;

			bool changed = false;
			bool first_frame = false;

			float min_value = 0.0f;
			float max_value = 0.0f;
			bool popped_out = false;
			bool fixed_timesteps = false;
			bool new_point = false;
			bool hovered_control = false;
			size_t clicked_point = ~size_t(0);
			size_t selected_point = ~size_t(0);
			size_t hovered_point = ~size_t(0);
			int hovered_derivative = 0;

			bool key_state[magic_enum::enum_count<KeyBinding>()];

			SizeableArrayView<simd::CurveControlPoint> points;

			struct {
				ScaleGraphMode mode = ScaleGraphMode::ByFactor;
				float factor = 1;
				std::pair<float, float> range;
			} scale;

			struct {
				size_t selected_point = ~0;
				float value = 0;
				float time = 0;
				simd::CurveControlPoint::Interpolation mode = simd::CurveControlPoint::Interpolation::Flat;
			} edit;

			void NewFrame(int current_frame)
			{
				if(frame == current_frame)
					return;

				ImGuiContext& g = *GImGui;
				changed = false;
				is_in_popout = false;
				first_frame = false;
				hovered_control = false;
				intern_id = 0;
				hovered_point = ~size_t(0);
				hovered_derivative = 0;

				if (drag_ended)
				{
					drag_ended = false;
					dragging_id = 0;
					drag_point = ~size_t(0);
					new_point = false;
				}

				for (auto& a : key_state)
					a = false;

				key_state[KeyBinding::KeyShift] = g.IO.KeyShift;
				key_state[KeyBinding::KeyAlt] = g.IO.KeyAlt;
				key_state[KeyBinding::KeyCtrl] = g.IO.KeyCtrl;

				if (frame + 1 != current_frame)
				{
					first_frame = true;
					popped_out = false;
					new_point = false;
					fixed_timesteps = false;
					min_value = 0.0f;
					max_value = 0.0f;
					clicked_point = ~size_t(0);
					selected_point = ~size_t(0);
					derivative_edit = 0;
					derivative_id = 0;
				}

				frame = current_frame;
			}

			void BeginPopout() 
			{ 
				is_in_popout = true;
				ImGui::PushID("Popout");
			}

			void EndPopout() 
			{ 
				ImGui::PopID();
				is_in_popout = false;
			}

			bool IsPopout() const { return is_in_popout; }
			bool WasDraggingPoint() const { return drag_point < points.size() && intern_id == dragging_id; }
			bool IsDraggingPoint() const { return !drag_ended && WasDraggingPoint(); }
			bool IsDraggingPoint(size_t p) const { return IsDraggingPoint() && drag_point == p; }
			size_t GetDraggingPoint() const { return drag_point; }
			size_t GetDraggingButton() const { return drag_button; }

			void BeginDraggingPoint(size_t point, size_t button)
			{
				dragging_id = intern_id;
				drag_point = point;
				drag_button = button;
			}

			void EndDraggingPoint() { drag_ended = true; }

			bool IsDraggingDerivative() const { return derivative_id == intern_id && derivative_edit != 0; }
			int GetDerivativeEditDirection() const { return derivative_edit; }

			void BeginDraggingDerivative(int v) 
			{
				derivative_id = intern_id;
				derivative_edit = v < 0 ? -1 : 1;
			}

			void EndDraggingDerivative() 
			{
				derivative_id = 0;
				derivative_edit = 0;
			}
		};

		struct PersistentSelector
		{
			int frame = 0;

			char buffer[256] = { '\0' };
			size_t selected_id = 0;
			size_t current_id = 0;
			size_t count = 0;
			ImVec2 size;
			SelectorState state;

			std::stringstream copy_tmp;

			bool flip_x = false;
			bool flip_y = false;
			bool copy = false;
			bool paste = false;
			bool reset = false;

			void NewFrame(int current_frame)
			{
				if (frame == current_frame)
					return;

				copy_tmp.str("");
				flip_x = false;
				flip_y = false;
				reset = false;
				paste = false;
				copy = false;

				if (frame + 1 != current_frame)
				{
					selected_id = ~size_t(0);
				}

				frame = current_frame;
			}
		};

		static ImGuiID last_popup = 0;
		static bool last_was_popout = false;
		static ImVec2 last_popup_size = ImVec2(0.0f, 0.0f);
		static PersistentSelector* active_selector = nullptr;

		PersistentData* GetPersistentData(ImGuiID id, simd::CurveControlPoint* points, size_t num_points, size_t max_points, bool can_remove_points)
		{
			static Memory::Map<ImGuiID, PersistentData, Memory::Tag::Unknown> datas;

			auto& data = datas[id];
			data.NewFrame(ImGui::GetCurrentContext()->FrameCount);
			data.points = SizeableArrayView<simd::CurveControlPoint>(points, num_points, max_points, can_remove_points ? 2 : num_points);
			return &data;
		}

		PersistentSelector* GetPersistentSelector(ImGuiID id)
		{
			static Memory::Map<ImGuiID, PersistentSelector, Memory::Tag::Unknown> datas;

			auto& data = datas[id];
			data.NewFrame(ImGui::GetCurrentContext()->FrameCount);
			return &data;
		}

		constexpr const char* GetKeyName(KeyBinding key)
		{
			switch (key)
			{
				case Device::GUI::KeyShift:
					return "Shift";
				case Device::GUI::KeyCtrl:
					return "Ctrl";
				case Device::GUI::KeyAlt:
					return "Alt";
				default:
					return "<Unknown Key>";
			}
		}

		bool IsSorted(const simd::CurveControlPoint* points, size_t num_points)
		{
			if (num_points < 2)
				return true;

			for (size_t a = 1; a < num_points; a++)
				if (points[a - 1].time > points[a].time)
					return false;

			return true;
		}

		ImColor HighlightColor(ImColor color, bool highlight = true)
		{
			if (highlight)
			{
				color.Value.x = (color.Value.x * 1.5f) + 0.25f;
				color.Value.y = (color.Value.y * 1.5f) + 0.25f;
				color.Value.z = (color.Value.z * 1.5f) + 0.25f;
			}

			return color;
		}

		ImColor MakeColor(const ImColor& c)
		{
			return ImVec4(c.Value.x, c.Value.y, c.Value.z, c.Value.w * ImGui::GetStyle().Alpha);
		}

		ImVec2 GetPointSize(const ImVec2& maxSize)
		{
			float sf = 0.45f;
			float radius = ImGui::GetFrameHeight() * sf;
			if (maxSize.x > 0)
				radius = std::min(radius, maxSize.x / 2);

			if (maxSize.y > 0)
				radius = std::min(radius, maxSize.y / 2);

			return ImVec2(radius, radius);
		}

		void RenderAxis(float coord, bool horizontal, const ImRect& point_area, ImU32 color)
		{
			auto draw_list = ImGui::GetWindowDrawList();
			ImVec2 p1 = point_area.Min;
			ImVec2 p2 = point_area.Max;

			if (horizontal)
				p1.y = p2.y = point_area.Min.y + (coord * point_area.GetHeight());
			else
				p1.x = p2.x = point_area.Min.x + (coord * point_area.GetWidth());

			if (ImLengthSqr(p1 - p2) > 1.0f)
				draw_list->AddLine(p1, p2, color);
		}

		void RenderSpline(const ImRect& point_area, PersistentData* persistent_data, const simd::vector4* spline, const float* spline_time, size_t spline_count, float start_time, float end_time, const ImColor& color, float offset = 0.0f)
		{
			auto draw_list = ImGui::GetWindowDrawList();

			const auto start_area = (int)point_area.Min.x;
			const auto end_area = (int)point_area.Max.x;
			const auto dt = (end_time - start_time) / (end_area - start_area);

			Memory::SmallVector<float, 8, Memory::Tag::Unknown> jumps;
			for (size_t a = 1; a < spline_count; a++)
			{
				const auto t = spline_time[a - 1];
				const auto t2 = t * t;
				const auto f = simd::vector4(t2 * t, t2, t, 1.0f);
				const auto x0 = simd::vector4::dot(spline[a - 1], f);
				const auto x1 = simd::vector4::dot(spline[a], f);
				if (std::abs(x0 - x1) > 1e-4f)
					jumps.push_back(spline_time[a]);
			}

			std::sort(jumps.begin(), jumps.end(), [](float a, float b) { return a > b; });

			for (int x = start_area; x <= end_area; x++)
			{
				const auto t = start_time + ((x - start_area) * dt);
				if (!jumps.empty() && jumps.back() < t)
				{
					do { jumps.pop_back(); } while (!jumps.empty() && jumps.back() < t);
					draw_list->PathStroke(color, 0, LineThickness);
				}
				
				const auto v = (persistent_data->max_value + offset - simd::CurveUtility::Interpolate(spline_time, spline, spline_count, t, start_time, end_time)) / (persistent_data->max_value - persistent_data->min_value);
				const auto y = point_area.Min.y + (point_area.GetHeight() * v);

				draw_list->PathLineTo(ImVec2(float(x), y));
			}

			draw_list->PathStroke(color, 0, LineThickness);
		}

		ImVec2 FindClosestPoint(const ImVec2& pos, const ImRect& point_area, PersistentData* persistent_data, const simd::vector4* spline, const float* spline_time, size_t spline_count, float start_time, float end_time)
		{
			const auto start_area = (int)point_area.Min.x;
			const auto end_area = (int)point_area.Max.x;
			const auto dt = (end_time - start_time) / (end_area - start_area);

			Memory::SmallVector<float, 8, Memory::Tag::Unknown> jumps;
			for (size_t a = 1; a < spline_count; a++)
			{
				const auto t = spline_time[a - 1];
				const auto t2 = t * t;
				const auto f = simd::vector4(t2 * t, t2, t, 1.0f);
				const auto x0 = simd::vector4::dot(spline[a - 1], f);
				const auto x1 = simd::vector4::dot(spline[a], f);
				if (std::abs(x0 - x1) > 1e-4f)
					jumps.push_back(spline_time[a]);
			}

			std::sort(jumps.begin(), jumps.end(), [](float a, float b) { return a > b; });

			float min_distance = std::numeric_limits<float>::infinity();
			ImVec2 result_point;

			ImVec2 p1;
			for (int x = start_area; x <= end_area; x++)
			{
				const auto t = start_time + ((x - start_area) * dt);
				const auto v = (persistent_data->max_value - simd::CurveUtility::Interpolate(spline_time, spline, spline_count, t, start_time, end_time)) / (persistent_data->max_value - persistent_data->min_value);
				const auto y = point_area.Min.y + (point_area.GetHeight() * v);

				if (!jumps.empty() && jumps.back() < t)
				{
					do { jumps.pop_back(); } while (!jumps.empty() && jumps.back() < t);
					p1 = ImVec2(float(x), y);
				}
				else
				{
					const auto p2 = ImVec2(float(x), y);
					if (x > start_area)
					{
						const ImVec2 p = ImLineClosestPoint(p1, p2, pos);
						const float d = ImLengthSqr(p - pos);
						if (d < min_distance)
						{
							min_distance = d;
							result_point = p;
						}
					}

					p1 = p2;
				}
			}

			return result_point;
		}

		void RenderModeSymbol(ImDrawList* draw_list, const ImVec2& pos, const ImVec2& size, ImColor color, bool filled, simd::CurveControlPoint::Interpolation mode)
		{
			switch (mode)
			{
				case simd::CurveControlPoint::Interpolation::Flat:
				{
					ImVec2 points[4];
					points[0].x = pos.x;			points[0].y = pos.y - size.y;
					points[1].x = pos.x - size.x;	points[1].y = pos.y;
					points[2].x = pos.x;			points[2].y = pos.y + size.y;
					points[3].x = pos.x + size.x;	points[3].y = pos.y;

					if (filled)
						draw_list->AddConvexPolyFilled(points, static_cast<int>(std::size(points)), color);
					else
						draw_list->AddPolyline(points, static_cast<int>(std::size(points)), color, true, 2.0f);
					break;
				}
				case simd::CurveControlPoint::Interpolation::Linear:
				{
					ImVec2 points[6];
					points[0].x = pos.x;					points[0].y = pos.y - size.y;
					points[1].x = pos.x - (size.x * 0.6f);	points[1].y = pos.y - (size.y * 0.5f);
					points[2].x = pos.x - (size.x * 0.6f);	points[2].y = pos.y + (size.y * 0.5f);
					points[3].x = pos.x;					points[3].y = pos.y + size.y;
					points[4].x = pos.x + (size.x * 0.6f);	points[4].y = pos.y + (size.y * 0.5f);
					points[5].x = pos.x + (size.x * 0.6f);	points[5].y = pos.y - (size.y * 0.5f);

					if (filled)
						draw_list->AddConvexPolyFilled(points, static_cast<int>(std::size(points)), color);
					else
						draw_list->AddPolyline(points, static_cast<int>(std::size(points)), color, true, 2.0f);
					break;
				}
				case simd::CurveControlPoint::Interpolation::Smooth:
				{
					if (filled)
						draw_list->AddCircleFilled(pos, size.x, color, 16);
					else
						draw_list->AddCircle(pos, size.x, color, 16, 2.0f);
					break;
				}
				case simd::CurveControlPoint::Interpolation::CustomSmooth:
				{
					ImVec2 points[5];
					points[0].x = pos.x - size.x;	points[0].y = pos.y - size.y;
					points[1].x = pos.x;			points[1].y = pos.y - size.y;
					points[2].x = pos.x + size.x;	points[2].y = pos.y;
					points[3].x = pos.x;			points[3].y = pos.y + size.y;
					points[4].x = pos.x - size.x;	points[4].y = pos.y + size.y;

					if (filled)
						draw_list->AddConvexPolyFilled(points, static_cast<int>(std::size(points)), color);
					else
						draw_list->AddPolyline(points, static_cast<int>(std::size(points)), color, true, 2.0f);
					break;
				}
				case simd::CurveControlPoint::Interpolation::Custom:
				{
					ImVec2 points[7];
					points[0].x = pos.x - 1.0f - size.x;	points[0].y = pos.y - size.y;
					points[1].x = pos.x - 1.0f;				points[1].y = pos.y - size.y;
					points[2].x = pos.x - 1.0f;				points[2].y = pos.y + size.y;
					points[3].x = pos.x - 1.0f - size.x;	points[3].y = pos.y + size.y;

					points[4].x = pos.x + 1.0f;				points[4].y = pos.y - size.y;
					points[5].x = pos.x + 1.0f + size.x;	points[5].y = pos.y;
					points[6].x = pos.x + 1.0f;				points[6].y = pos.y + size.y;

					if (filled)
					{
						draw_list->AddConvexPolyFilled(&points[0], 4, color);
						draw_list->AddConvexPolyFilled(&points[4], 3, color);
					}
					else
					{
						draw_list->AddPolyline(&points[0], 4, color, true, 2.0f);
						draw_list->AddPolyline(&points[4], 3, color, true, 2.0f);
					}
					break;
				}
				default:
				{
					ImVec2 tl = pos - size;
					ImVec2 br = pos + size;
					if (filled)
						draw_list->AddRectFilled(tl, br, color);
					else
						draw_list->AddRect(tl, br, color, 0, 15, 1.5f);
					break;
				}
			}
		}

		void RenderPoint(const ImVec2& pos, const ImVec2& size, ImColor color, bool filled, ImColor filled_color, simd::CurveControlPoint::Interpolation mode)
		{
			auto draw_list = ImGui::GetWindowDrawList();

			if (filled)
			{
				RenderModeSymbol(draw_list, pos, size, filled_color, true, mode);
				if (filled_color == color)
					return;
			}

			RenderModeSymbol(draw_list, pos, size, color, false, mode);
		}

		void RenderPoint(const ImVec2& pos, const ImVec2& size, ImColor color, bool filled, simd::CurveControlPoint::Interpolation mode) { RenderPoint(pos, size, color, filled, color, mode); }

		void RenderColorSymbol(ImDrawList* draw_list, const ImVec2& pos, const ImVec2& size, ImColor color, bool filled)
		{
			ImVec2 points[3];
			points[0].x = pos.x;			points[0].y = pos.y - size.y;
			points[1].x = pos.x + size.x;	points[1].y = pos.y + size.y;
			points[2].x = pos.x - size.x;	points[2].y = pos.y + size.y;

			if (filled)
				draw_list->AddConvexPolyFilled(points, static_cast<int>(std::size(points)), color);
			else
				draw_list->AddPolyline(points, static_cast<int>(std::size(points)), color, true, 2.0f);
		}

		void RenderColorPoint(const ImVec2& pos, const ImVec2& size, ImColor color, bool filled, ImColor filled_color)
		{
			auto draw_list = ImGui::GetWindowDrawList();

			if (filled)
			{
				RenderColorSymbol(draw_list, pos, size, filled_color, true);
				if (filled_color == color)
					return;
			}

			RenderColorSymbol(draw_list, pos, size, color, false);
		}

		void RenderColorPoint(const ImVec2& pos, const ImVec2& size, ImColor color, bool filled) { RenderColorPoint(pos, size, color, filled, color); }

		ImVec2 GetPointScreenPosition(const simd::CurveControlPoint& point, const ImRect& point_area, PersistentData* persistent_data, float start_time, float end_time)
		{
			float x = (point.time - start_time) / (end_time - start_time);
			float y = (persistent_data->max_value - point.value) / (persistent_data->max_value - persistent_data->min_value);

			return ImVec2(ImLerp(point_area.Min.x, point_area.Max.x, x), ImLerp(point_area.Min.y, point_area.Max.y, y));
		}

		simd::vector2 GetScreenPositionPoint(const ImVec2& pos, const ImRect& point_area, PersistentData* persistent_data, float start_time, float end_time)
		{
			float x = (pos.x - point_area.Min.x) / point_area.GetWidth();
			float y = (point_area.Max.y - pos.y) / point_area.GetHeight();

			return simd::vector2(ImLerp(start_time, end_time, x), ImLerp(persistent_data->min_value, persistent_data->max_value, y));
		}

		ImVec2 GetColorPointScreenPosition(const ColorTrackPoint& point, const ImRect& point_area, PersistentData* (&persistent_data)[4], float start_time, float end_time)
		{
			float x = (persistent_data[0]->points[point.index[0]].time - start_time) / (end_time - start_time);
			return ImVec2(ImLerp(point_area.Min.x, point_area.Max.x, x), ImLerp(point_area.Min.y, point_area.Max.y, 0.5f));
		}

		float GetColorScreenPositionPoint(const ImVec2& pos, const ImRect& point_area, float start_time, float end_time)
		{
			float x = (pos.x - point_area.Min.x) / point_area.GetWidth();
			return ImLerp(start_time, end_time, x);
		}

		void RenderPoints(const ImRect& point_area, PersistentData* persistent_data, const ImVec2& point_size, float start_time, float end_time)
		{
			size_t a = 0;
			const auto mid_time = 0.5f * (start_time + end_time);
			for (; a < persistent_data->points.size(); a++)
			{
				if (persistent_data->points[a].time >= mid_time)
					break;

				const auto pos = GetPointScreenPosition(persistent_data->points[a], point_area, persistent_data, start_time, end_time);
				const auto border_color = HighlightColor(MakeColor(persistent_data->selected_point == a ? ColorSelectedAxisActive : ColorGraphPointBorder), a == persistent_data->hovered_point || a == persistent_data->clicked_point);
				const auto filled_color = HighlightColor(MakeColor(ColorGraphPoint), a == persistent_data->hovered_point);
				RenderPoint(pos, point_size, border_color, true, filled_color, persistent_data->points[a].type);
			}

			for (size_t b = persistent_data->points.size(); b > a; b--)
			{
				const auto pos = GetPointScreenPosition(persistent_data->points[b - 1], point_area, persistent_data, start_time, end_time);
				const auto border_color = HighlightColor(MakeColor(persistent_data->selected_point == b - 1 ? ColorSelectedAxisActive : ColorGraphPointBorder), b - 1 == persistent_data->hovered_point || b - 1 == persistent_data->clicked_point);
				const auto filled_color = HighlightColor(MakeColor(ColorGraphPoint), b - 1 == persistent_data->hovered_point);
				RenderPoint(pos, point_size, border_color, true, filled_color, persistent_data->points[b - 1].type);
			}
		}

		void RenderColorPoints(const ImRect& frame_bb, const ImVec2& point_size, PersistentData* (&persistent_data)[4], bool hovered, float start_time, float end_time, SizeableArrayView<ColorTrackPoint>& visible_points)
		{
			ImGuiWindow* window = ImGui::GetCurrentWindow();
			const auto draw_list = window->DrawList;

			size_t a = 0;
			const auto mid_time = 0.5f * (start_time + end_time);
			for (; a < visible_points.size(); a++)
			{
				if (persistent_data[0]->points[visible_points[a].index[0]].time >= mid_time)
					break;

				auto pos = GetColorPointScreenPosition(visible_points[a], frame_bb, persistent_data, start_time, end_time);
				const auto border_color = HighlightColor(MakeColor(persistent_data[0]->selected_point == a ? ColorSelectedAxisActive : ColorGraphPointBorder), a == persistent_data[0]->hovered_point || a == persistent_data[0]->clicked_point);
				const auto filled_color = HighlightColor(MakeColor(ColorGraphPoint), a == persistent_data[0]->hovered_point);
				RenderColorPoint(pos, point_size, border_color, true, filled_color);
			}

			for (size_t b = visible_points.size(); b > a; b--)
			{
				auto pos = GetColorPointScreenPosition(visible_points[b - 1], frame_bb, persistent_data, start_time, end_time);
				const auto border_color = HighlightColor(MakeColor(persistent_data[0]->selected_point == b - 1 ? ColorSelectedAxisActive : ColorGraphPointBorder), b - 1 == persistent_data[0]->hovered_point || b - 1 == persistent_data[0]->clicked_point);
				const auto filled_color = HighlightColor(MakeColor(ColorGraphPoint), b - 1 == persistent_data[0]->hovered_point);
				RenderColorPoint(pos, point_size, border_color, true, filled_color);
			}
		}

		std::pair<float, float> GetDerivativesAtPoint(PersistentData* persistent_data, size_t point)
		{
			switch (persistent_data->points[point].type)
			{
				case simd::CurveControlPoint::Interpolation::Linear:
				{
					float l = 0.0f;
					float r = 0.0f;

					if (point > 0 && persistent_data->points[point - 1].time + 1e-4f < persistent_data->points[point].time)
					{
						if (persistent_data->points[point - 1].type == simd::CurveControlPoint::Interpolation::Linear)
							l = (persistent_data->points[point].value - persistent_data->points[point - 1].value) / (persistent_data->points[point].time - persistent_data->points[point - 1].time);
						else
						{
							const auto m = GetDerivativesAtPoint(persistent_data, point - 1).second;
							l = (2 * (persistent_data->points[point].value - persistent_data->points[point - 1].value) / (persistent_data->points[point].time - persistent_data->points[point - 1].time)) - m;
						}
					}

					if (point + 1 < persistent_data->points.size() && persistent_data->points[point].time + 1e-4f < persistent_data->points[point + 1].time)
					{
						if (persistent_data->points[point + 1].type == simd::CurveControlPoint::Interpolation::Linear)
							r = (persistent_data->points[point + 1].value - persistent_data->points[point].value) / (persistent_data->points[point + 1].time - persistent_data->points[point].time);
						else
						{
							const auto m = GetDerivativesAtPoint(persistent_data, point + 1).first;
							r = (2 * (persistent_data->points[point + 1].value - persistent_data->points[point].value) / (persistent_data->points[point + 1].time - persistent_data->points[point].time)) - m;
						}
					}

					return std::make_pair(l, r);
				}
				case simd::CurveControlPoint::Interpolation::Smooth:
				{
					float d = 0.0f;
					if (point > 0 && persistent_data->points[point - 1].time + 1e-4f < persistent_data->points[point].time && point + 1 < persistent_data->points.size() && persistent_data->points[point].time + 1e-4f < persistent_data->points[point + 1].time)
						d = (persistent_data->points[point + 1].value - persistent_data->points[point - 1].value) / (persistent_data->points[point + 1].time - persistent_data->points[point - 1].time);

					return std::make_pair(d, d);
				}
				case simd::CurveControlPoint::Interpolation::Flat:
					return std::make_pair(0.0f, 0.0f);
				case simd::CurveControlPoint::Interpolation::Custom:
					return std::make_pair(persistent_data->points[point].left_dt, persistent_data->points[point].right_dt);
				case simd::CurveControlPoint::Interpolation::CustomSmooth:
					return std::make_pair(persistent_data->points[point].left_dt, persistent_data->points[point].left_dt);
				default:
					return std::make_pair(0.0f, 0.0f);
			}
		}

		std::pair<ImVec2, ImVec2> GetDerivativesPosition(PersistentData* persistent_data, size_t point, const ImRect& point_area, float start_time, float end_time)
		{
			const auto d = GetDerivativesAtPoint(persistent_data, point);
			const auto p = GetPointScreenPosition(persistent_data->points[point], point_area, persistent_data, start_time, end_time);
			const auto dt = end_time - start_time;

			const auto lv = persistent_data->points[point].value - (d.first * dt);
			const auto lt = persistent_data->points[point].time - dt;

			const auto rv = persistent_data->points[point].value + (d.second * dt);
			const auto rt = persistent_data->points[point].time + dt;

			auto lp = ImVec2(ImLerp(point_area.Min.x, point_area.Max.x, (lt - start_time) / dt), ImLerp(point_area.Min.y, point_area.Max.y, (persistent_data->max_value - lv) / (persistent_data->max_value - persistent_data->min_value))) - p;
			auto rp = ImVec2(ImLerp(point_area.Min.x, point_area.Max.x, (rt - start_time) / dt), ImLerp(point_area.Min.y, point_area.Max.y, (persistent_data->max_value - rv) / (persistent_data->max_value - persistent_data->min_value))) - p;

			const auto scale = point_area.GetWidth() * DerivativeScale;
			lp = p + (lp * scale * ImInvLength(lp, 1.0f));
			rp = p + (rp * scale * ImInvLength(rp, 1.0f));

			return std::make_pair(lp, rp);
		}

		void RenderDerivatives(const ImRect& point_area, const ImVec2& size, PersistentData* persistent_data, float start_time, float end_time)
		{
			if (persistent_data->clicked_point < persistent_data->points.size())
			{
				auto draw_list = ImGui::GetWindowDrawList();
				const auto pos = GetDerivativesPosition(persistent_data, persistent_data->clicked_point, point_area, start_time, end_time);
				const auto mid = GetPointScreenPosition(persistent_data->points[persistent_data->clicked_point], point_area, persistent_data, start_time, end_time);
				draw_list->AddLine(pos.first, mid, MakeColor(ColorDerivative), LineThickness * 1.5f);
				draw_list->AddLine(mid, pos.second, MakeColor(ColorDerivative), LineThickness * 1.5f);

				auto TransformShape = [](size_t count, ImVec2* vertices, const ImVec2& dir, const ImVec2& offset)
				{
					const float m = dir.y / dir.x;
					auto cos_sin = ImVec2(1.0f, m);
					cos_sin *= ImInvLength(cos_sin, 1.0f);

					for (size_t a = 0; a < count; a++)
						vertices[a] = ImVec2((vertices[a].x * cos_sin.x) - (vertices[a].y * cos_sin.y), (vertices[a].x * cos_sin.y) + (vertices[a].y * cos_sin.x)) + offset;
				};

				switch (persistent_data->points[persistent_data->clicked_point].type)
				{
					case simd::CurveControlPoint::Interpolation::Custom:
					case simd::CurveControlPoint::Interpolation::CustomSmooth:
					{
						// Left
						{
							const auto color = HighlightColor(MakeColor(ColorDerivative), persistent_data->hovered_derivative < 0);

							ImVec2 points[4];
							points[0].x = -0.5f * size.x;	points[0].y = -size.y;
							points[1].x =  0.5f * size.x;	points[1].y = -size.y;
							points[2].x =  0.5f * size.x;	points[2].y =  size.y;
							points[3].x = -0.5f * size.x;	points[3].y =  size.y;

							TransformShape(std::size(points), points, mid - pos.first, pos.first);
							draw_list->AddConvexPolyFilled(points, static_cast<int>(std::size(points)), color);
						}

						// Right
						{
							const auto color = HighlightColor(MakeColor(ColorDerivative), persistent_data->hovered_derivative > 0);

							ImVec2 points[3];
							points[0].x = -0.5f * size.x;	points[0].y = -size.y;
							points[1].x = 0.5f * size.x;	points[1].y = 0;
							points[2].x = -0.5f * size.x;	points[2].y =  size.y;

							TransformShape(std::size(points), points, pos.second - mid, pos.second);
							draw_list->AddConvexPolyFilled(points, static_cast<int>(std::size(points)), color);
						}

						break;
					}
					default:
						break;
				}
			}
		}

		simd::CurveControlPoint::Interpolation GetNextMode(simd::CurveControlPoint::Interpolation current, bool forward)
		{
			if (auto idx = magic_enum::enum_index(current))
			{
				const auto i = (*idx + magic_enum::enum_count<simd::CurveControlPoint::Interpolation>() + (forward ? 1 : -1)) % magic_enum::enum_count<simd::CurveControlPoint::Interpolation>();
				return magic_enum::enum_value<simd::CurveControlPoint::Interpolation>(i);
			}
			else
				return simd::CurveControlPoint::Interpolation::Linear;
		}

		void ProcessInputBegin(const ImRect& point_area, const ImVec2& point_size, PersistentData* persistent_data, bool hovered, float start_time, float end_time)
		{
			ImGuiContext& g = *GImGui;
			persistent_data->hovered_point = ~size_t(0);
			persistent_data->hovered_derivative = 0;

			// Process derivative dragging:
			if (persistent_data->IsDraggingDerivative())
			{
				persistent_data->hovered_derivative = persistent_data->GetDerivativeEditDirection();
				if (!g.IO.MouseDown[ImGuiMouseButton_Left] || persistent_data->clicked_point >= persistent_data->points.size())
				{
					persistent_data->EndDraggingDerivative();
					if (g.ActiveId == persistent_data->intern_id)
						ImGui::ClearActiveID();
				}
				else if (g.IO.MouseDragMaxDistanceSqr[ImGuiMouseButton_Left] >= g.IO.MouseDragThreshold * g.IO.MouseDragThreshold)
				{
					const auto d_scale = -(persistent_data->max_value - persistent_data->min_value) / (end_time - start_time);

					persistent_data->changed = true;
					auto dir = g.IO.MousePos - GetPointScreenPosition(persistent_data->points[persistent_data->clicked_point], point_area, persistent_data, start_time, end_time);
					if (persistent_data->GetDerivativeEditDirection() < 0)
						dir.x = std::min(dir.x, -1.0f);
					else
						dir.x = std::max(dir.x, 1.0f);

					// Snap to zero
					if (!persistent_data->key_state[KeyIgnoreSnap])
					{
						if (std::abs(dir.y) < point_size.y)
							dir.y = 0.0f;

						if (persistent_data->points[persistent_data->clicked_point].type == simd::CurveControlPoint::Interpolation::Custom)
						{
							const float other_m = persistent_data->GetDerivativeEditDirection() < 0 ? persistent_data->points[persistent_data->clicked_point].right_dt : persistent_data->points[persistent_data->clicked_point].left_dt;
							const auto other_d = dir.x * (point_area.GetHeight() / point_area.GetWidth()) * other_m / d_scale;
							if (std::abs(dir.y - other_d) < point_size.y)
								dir.y = other_d;
						}
					}

					dir /= point_area.GetSize();

					const float m = std::clamp(dir.y / dir.x, -573.0f, 573.0f) * d_scale; // Clamped to [-89.9 ; 89.9] degree
					switch (persistent_data->points[persistent_data->clicked_point].type)
					{
						case simd::CurveControlPoint::Interpolation::Custom:
							if (persistent_data->GetDerivativeEditDirection() < 0)
								persistent_data->points[persistent_data->clicked_point].left_dt = m;
							else
								persistent_data->points[persistent_data->clicked_point].right_dt = m;

							break;
						case simd::CurveControlPoint::Interpolation::CustomSmooth:
							persistent_data->points[persistent_data->clicked_point].left_dt = persistent_data->points[persistent_data->clicked_point].right_dt = m;
							break;
						default:
							break;
					}
				}
			}
			else if (hovered && persistent_data->clicked_point < persistent_data->points.size())
			{
				switch (persistent_data->points[persistent_data->clicked_point].type)
				{
					case simd::CurveControlPoint::Interpolation::Custom:
					case simd::CurveControlPoint::Interpolation::CustomSmooth:
					{
						const auto pos = GetDerivativesPosition(persistent_data, persistent_data->clicked_point, point_area, start_time, end_time);
						ImVec2 touch_size(point_size.x + 2, point_size.y + 2);
						ImRect point_covered_area_left(pos.first - touch_size, pos.first + touch_size);
						const ImRect rect_for_touch_left(point_covered_area_left.Min - g.Style.TouchExtraPadding, point_covered_area_left.Max + g.Style.TouchExtraPadding);
						ImRect point_covered_area_right(pos.second - touch_size, pos.second + touch_size);
						const ImRect rect_for_touch_right(point_covered_area_right.Min - g.Style.TouchExtraPadding, point_covered_area_right.Max + g.Style.TouchExtraPadding);
						if (g.MouseViewport->GetMainRect().Overlaps(point_covered_area_left) && rect_for_touch_left.Contains(g.IO.MousePos))
							persistent_data->hovered_derivative = -1;
						else if (g.MouseViewport->GetMainRect().Overlaps(point_covered_area_right) && rect_for_touch_right.Contains(g.IO.MousePos))
							persistent_data->hovered_derivative = 1;

						break;
					}
					default:
						break;
				}

				if (g.IO.MouseClicked[ImGuiMouseButton_Left] && persistent_data->hovered_derivative != 0)
				{
					persistent_data->BeginDraggingDerivative(persistent_data->hovered_derivative);
					ImGui::SetActiveID(persistent_data->intern_id, ImGui::GetCurrentWindow());
				}
			}

			if (!persistent_data->IsDraggingDerivative())
			{
				// Process clicking on points:
				if (persistent_data->IsDraggingPoint() && !g.IO.MouseDown[persistent_data->GetDraggingButton()] && g.IO.MouseDragMaxDistanceSqr[persistent_data->GetDraggingButton()] < g.IO.MouseDragThreshold * g.IO.MouseDragThreshold && !persistent_data->new_point)
				{
					if (persistent_data->GetDraggingButton() == ImGuiMouseButton_Left)
					{
						if (persistent_data->key_state[KeyPointSelect])
						{
							if (persistent_data->selected_point == persistent_data->GetDraggingPoint())
								persistent_data->selected_point = ~size_t(0);
							else
								persistent_data->selected_point = persistent_data->GetDraggingPoint();
						}
						else if (persistent_data->key_state[KeyEditPoint])
						{
							persistent_data->edit.selected_point = persistent_data->GetDraggingPoint();
							persistent_data->edit.value = persistent_data->points[persistent_data->GetDraggingPoint()].value;
							persistent_data->edit.time = persistent_data->points[persistent_data->GetDraggingPoint()].time;
							persistent_data->edit.mode = persistent_data->points[persistent_data->GetDraggingPoint()].type;
							ImGui::OpenPopup(PointEditPopup);
						}
						else if (persistent_data->clicked_point != persistent_data->GetDraggingPoint())
							persistent_data->clicked_point = persistent_data->GetDraggingPoint();
						else
							persistent_data->clicked_point = ~size_t(0);
					}
					else if (persistent_data->GetDraggingButton() == ImGuiMouseButton_Right)
					{
						if (persistent_data->points.can_erase() && persistent_data->GetDraggingPoint() > 0 && persistent_data->GetDraggingPoint() + 1 < persistent_data->points.size())
						{
							persistent_data->points.erase(persistent_data->GetDraggingPoint());
							persistent_data->changed = true;

							if (persistent_data->selected_point == persistent_data->GetDraggingPoint())
								persistent_data->selected_point = ~size_t(0);
							else if (persistent_data->selected_point > persistent_data->GetDraggingPoint() && persistent_data->selected_point < persistent_data->points.size())
								persistent_data->selected_point--;

							if (persistent_data->clicked_point == persistent_data->GetDraggingPoint())
								persistent_data->clicked_point = ~size_t(0);
							else if (persistent_data->clicked_point > persistent_data->GetDraggingPoint() && persistent_data->clicked_point < persistent_data->points.size())
								persistent_data->clicked_point--;

							if (persistent_data->fixed_timesteps)
							{
								const float time_step = (end_time - start_time) / (persistent_data->points.size() - 1);

								for (size_t a = 1; a + 1 < persistent_data->points.size(); a++)
									persistent_data->points[a].time = a * time_step;
							}
						}
					}
					else if (persistent_data->GetDraggingButton() == ImGuiMouseButton_Middle)
					{
						auto next_mode = GetNextMode(persistent_data->points[persistent_data->GetDraggingPoint()].type, !persistent_data->key_state[KeyReverseMode]);
						if (next_mode != persistent_data->points[persistent_data->GetDraggingPoint()].type)
						{
							persistent_data->points[persistent_data->GetDraggingPoint()].type = next_mode;
							persistent_data->changed = true;
						}
					}

					persistent_data->EndDraggingPoint();
				}

				// Process point dragging/hovering:
				for (size_t a = 0; a < persistent_data->points.size(); a++)
				{
					auto pos = GetPointScreenPosition(persistent_data->points[a], point_area, persistent_data, start_time, end_time);
					if (persistent_data->IsDraggingPoint(a))
					{
						persistent_data->hovered_point = a;
						if (persistent_data->GetDraggingButton() != ImGuiMouseButton_Middle && g.IO.MouseDragMaxDistanceSqr[persistent_data->GetDraggingButton()] >= g.IO.MouseDragThreshold * g.IO.MouseDragThreshold)
						{
							const auto last_pos = pos;
							const auto last_value = persistent_data->points[a].value;
							pos = ImMax(point_area.Min, ImMin(point_area.Max, g.IO.MousePos));

							if (persistent_data->key_state[KeyMoveAll])
								pos.x = last_pos.x;

							if (!persistent_data->key_state[KeyIgnoreSnap])
							{
								// Snap to zero:
								if (persistent_data->max_value > 0.0f && persistent_data->min_value < 0.0f)
								{
									const float snap_perc = (persistent_data->max_value - 0.0f) / (persistent_data->max_value - persistent_data->min_value);
									const float snap_pos = ImLerp(point_area.Min.y, point_area.Max.y, snap_perc);
									if (std::abs(snap_pos - pos.y) < point_size.y)
										pos.y = snap_pos;
								}

								// Snap to selected point:
								if (persistent_data->selected_point < persistent_data->points.size() && persistent_data->selected_point != a)
								{
									const auto snap_pos = GetPointScreenPosition(persistent_data->points[persistent_data->selected_point], point_area, persistent_data, start_time, end_time);
									if (std::abs(snap_pos.y - pos.y) < point_size.y)
										pos.y = snap_pos.y;
								}
							}

							if (persistent_data->points.size() == 1)
								pos.x = point_area.GetCenter().x;
							else if (a == 0)
								pos.x = point_area.Min.x;
							else if (a + 1 == persistent_data->points.size())
								pos.x = point_area.Max.x;
							else if (persistent_data->fixed_timesteps)
								pos.x = point_area.Min.x + (a * point_area.GetWidth() / (persistent_data->points.size() - 1));

							const auto res = GetScreenPositionPoint(pos, point_area, persistent_data, start_time, end_time);
							persistent_data->points[a].time = std::clamp(res.x, start_time, end_time);
							persistent_data->points[a].value = std::clamp(res.y, persistent_data->min_value, persistent_data->max_value);
							persistent_data->changed = true;
							const auto delta_value = persistent_data->points[a].value - last_value;

							if (persistent_data->key_state[KeyIgnoreSnap])
							{
								if (pos.x < last_pos.x)
								{
									for (size_t b = a - 1; b > 0; b--)
										persistent_data->points[b].time = std::min(persistent_data->points[b].time, persistent_data->points[a].time);
								}
								else
								{
									for (size_t b = a + 1; b + 1 < persistent_data->points.size(); b++)
										persistent_data->points[b].time = std::max(persistent_data->points[b].time, persistent_data->points[a].time);
								}
							}
							else
							{
								if (a > 0)
									persistent_data->points[a].time = std::max(persistent_data->points[a].time, persistent_data->points[a - 1].time);

								if (a + 1 < persistent_data->points.size())
									persistent_data->points[a].time = std::min(persistent_data->points[a].time, persistent_data->points[a + 1].time);
							}

							if (persistent_data->key_state[KeyMoveAll])
							{
								for (size_t b = 0; b < persistent_data->points.size(); b++)
								{
									if (b == a)
										continue;

									if (persistent_data->GetDraggingButton() == ImGuiMouseButton_Left)
										persistent_data->points[b].value = std::clamp(persistent_data->points[b].value + delta_value, persistent_data->min_value, persistent_data->max_value);
									else if (persistent_data->GetDraggingButton() == ImGuiMouseButton_Right)
										persistent_data->points[b].value = persistent_data->points[a].value;
								}
							}
						}
					}
					else if (!persistent_data->IsDraggingPoint() && hovered)
					{
						ImVec2 touch_size(point_size.x + 2, point_size.y + 2);
						ImRect point_covered_area(pos - touch_size, pos + touch_size);
						const ImRect rect_for_touch(point_covered_area.Min - g.Style.TouchExtraPadding, point_covered_area.Max + g.Style.TouchExtraPadding);
						if (g.MouseViewport->GetMainRect().Overlaps(point_covered_area) && rect_for_touch.Contains(g.IO.MousePos))
						{
							if (persistent_data->hovered_point == ~0 || pos.x < point_area.GetCenter().x)
								persistent_data->hovered_point = a;
						}
					}
				}

				// Update dragging state:
				if (!persistent_data->WasDraggingPoint() && hovered && persistent_data->hovered_point < persistent_data->points.size() && (g.IO.MouseClicked[ImGuiMouseButton_Left] || g.IO.MouseClicked[ImGuiMouseButton_Right] || g.IO.MouseClicked[ImGuiMouseButton_Middle]))
				{
					if (g.IO.MouseDown[ImGuiMouseButton_Left])
						persistent_data->BeginDraggingPoint(persistent_data->hovered_point, ImGuiMouseButton_Left);
					else if (g.IO.MouseDown[ImGuiMouseButton_Right])
						persistent_data->BeginDraggingPoint(persistent_data->hovered_point, ImGuiMouseButton_Right);
					else if (g.IO.MouseDown[ImGuiMouseButton_Middle])
						persistent_data->BeginDraggingPoint(persistent_data->hovered_point, ImGuiMouseButton_Middle);

					ImGui::SetActiveID(persistent_data->intern_id, ImGui::GetCurrentWindow());
				}
				else if (persistent_data->WasDraggingPoint() && !g.IO.MouseDown[persistent_data->GetDraggingButton()])
				{
					persistent_data->EndDraggingPoint();
					if (g.ActiveId == persistent_data->intern_id)
						ImGui::ClearActiveID();
				}
			}
		}

		void ProcessInputEnd(const ImRect& point_area, const ImVec2& point_size, PersistentData* persistent_data, const simd::vector4* spline, const float* spline_time, size_t spline_count, bool hovered, float start_time, float end_time)
		{
			ImGuiContext& g = *GImGui;

			auto ShowTooltip = [](const simd::CurveControlPoint& point)
			{
				ImGui::BeginTooltip();
				ImGui::PushTextWrapPos(35.0f * ImGui::GetFontSize());
				ImGui::TextWrapped("Position: %.3f", point.time);
				ImGui::TextWrapped("Value: %.3f", point.value);
				ImGui::PopTextWrapPos();
				ImGui::EndTooltip();
			};

			// Point Tooltip & Point Adding:
			if (hovered && !persistent_data->IsDraggingDerivative() && persistent_data->hovered_derivative == 0)
			{
				if (!persistent_data->WasDraggingPoint())
				{
					if (persistent_data->hovered_point == ~size_t(0))
					{
						if (persistent_data->points.can_insert())
						{
							auto pos = FindClosestPoint(g.IO.MousePos, point_area, persistent_data, spline, spline_time, spline_count, start_time, end_time);
							if (ImLengthSqr(pos - g.IO.MousePos) <= ImLengthSqr(point_size))
							{
								simd::CurveControlPoint::Interpolation mode = simd::CurveControlPoint::Interpolation::Linear;
								float min_dist = std::numeric_limits<float>::infinity();
								for (size_t a = 0; a < persistent_data->points.size(); a++)
								{
									const auto p = GetPointScreenPosition(persistent_data->points[a], point_area, persistent_data, start_time, end_time);
									const auto l = ImLengthSqr(p - pos);
									if (l < min_dist)
									{
										min_dist = l;
										mode = persistent_data->points[a].type;
									}
								}

								RenderPoint(pos, point_size, MakeColor(ColorGraphPoint), false, mode);

								if (!persistent_data->key_state[KeyIgnoreSnap])
								{
									// Snap to zero:
									if (persistent_data->max_value > 0.0f && persistent_data->min_value < 0.0f)
									{
										const float snap_perc = (persistent_data->max_value - 0.0f) / (persistent_data->max_value - persistent_data->min_value);
										const float snap_pos = ImLerp(point_area.Min.y, point_area.Max.y, snap_perc);
										if (std::abs(snap_pos - pos.y) < point_size.y)
											pos.y = snap_pos;
									}

									// Snap to selected point:
									if (persistent_data->selected_point < persistent_data->points.size())
									{
										const auto snap_pos = GetPointScreenPosition(persistent_data->points[persistent_data->selected_point], point_area, persistent_data, start_time, end_time);
										if (std::abs(snap_pos.y - pos.y) < point_size.y)
											pos.y = snap_pos.y;
									}
								}

								auto res = GetScreenPositionPoint(pos, point_area, persistent_data, start_time, end_time);
								const auto t = std::clamp(res.x, start_time, end_time);
								size_t next_point = 0;
								while (next_point < persistent_data->points.size() && persistent_data->points[next_point].time < t)
									next_point++;

								const auto derivative = simd::CurveUtility::CalculateDerivative(spline_time, spline, spline_count, t, start_time, end_time);
								auto new_point = simd::CurveControlPoint(mode, t, std::clamp(res.y, persistent_data->min_value, persistent_data->max_value), derivative, derivative);

								if (g.ActiveId == 0 && g.IO.MouseClicked[ImGuiMouseButton_Left])
								{
									persistent_data->changed = true;
									persistent_data->new_point = true;
									persistent_data->points.insert(next_point, new_point);

									if (persistent_data->selected_point < persistent_data->points.size() && persistent_data->selected_point >= next_point)
										persistent_data->selected_point++;

									if (persistent_data->clicked_point < persistent_data->points.size() && persistent_data->clicked_point >= next_point)
										persistent_data->clicked_point++;

									if (persistent_data->fixed_timesteps)
									{
										const float time_step = (end_time - start_time) / (persistent_data->points.size() - 1);

										for (size_t a = 1; a + 1 < persistent_data->points.size(); a++)
											persistent_data->points[a].time = a * time_step;
									}

									persistent_data->BeginDraggingPoint(next_point, ImGuiMouseButton_Left);
									ImGui::SetActiveID(persistent_data->intern_id, ImGui::GetCurrentWindow());
								}

								if (persistent_data->key_state[KeyPointSelect])
									ShowTooltip(new_point);
							}
						}
					}
					else if(persistent_data->key_state[KeyPointSelect])
						ShowTooltip(persistent_data->points[persistent_data->hovered_point]);
				}
				else if (persistent_data->IsDraggingPoint() && persistent_data->key_state[KeyPointSelect])
					ShowTooltip(persistent_data->points[persistent_data->GetDraggingPoint()]);

				if (!persistent_data->WasDraggingPoint() && (g.IO.MouseDown[ImGuiMouseButton_Left] || g.IO.MouseDown[ImGuiMouseButton_Right] || g.IO.MouseDown[ImGuiMouseButton_Middle]))
					persistent_data->clicked_point = ~size_t(0);
			}
		}

		bool BeginControl(const char* label, PersistentData* persistent_data, ImVec2* size)
		{
			if (persistent_data->IsPopout())
			{
				char buffer[1024] = { '\0' };
				ImFormatString(buffer, std::size(buffer), "%s###CurveControlPopout", label);

				if (last_popup == persistent_data->popout_id)
					ImGui::OpenPopup(buffer);

				last_popup = 0;

				// Need to specifiy a valid constraints area so that manual resizing still works:
				ImGui::SetNextWindowSizeConstraints(ImVec2(ImGui::GetFrameHeight() * 10, ImGui::GetFrameHeight() * 10), ImVec2(std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity()), [](ImGuiSizeCallbackData* data)
				{
					ImVec2* size = (ImVec2*)data->UserData;
					if (size->x > 0) data->DesiredSize.x = std::max(data->DesiredSize.x, size->x);
					if (size->y > 0) data->DesiredSize.y = std::max(data->DesiredSize.y, size->y);
				}, size);

				if (!last_popup_size.IsZero())
					ImGui::SetNextWindowSize(last_popup_size, ImGuiCond_Appearing);

				if (!ImGui::BeginPopupEx(ImGui::GetCurrentWindow()->GetID(buffer), ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoSavedSettings))
				{
					persistent_data->popped_out = false;
					return false;
				}

				last_was_popout = true;
				last_popup_size = ImGui::GetWindowSize();
				*size = ImGui::GetContentRegionAvail() - ImGui::GetStyle().WindowPadding;

				if (size->x <= 0 || size->y <= 0 || ImGui::GetCurrentWindow()->SkipItems)
				{
					ImGui::EndPopup();
					return false;
				}
			}
			else
			{
				auto window = ImGui::GetCurrentWindow();
				if (window->SkipItems)
					return false;

				if (size->y <= 0)
					size->y = ImGui::GetContentRegionAvail().y;

				if (size->x <= 0 || size->y <= 0)
					return false;
			}

			ImGui::BeginGroup();
			return true;
		}

		void EndControl(PersistentData* persistent_data)
		{
			ImGui::EndGroup();
			if (persistent_data->IsPopout())
				ImGui::EndPopup();
		}

		void FlipX(PersistentData* persistent_data)
		{
			persistent_data->changed = true;
			const auto start_end = simd::CurveUtility::GetStartEndTime(persistent_data->points.data(), persistent_data->points.size());
			const auto duration = start_end.second - start_end.first;

			for (size_t a = 0; a < persistent_data->points.size(); a++)
			{
				persistent_data->points[a].time = start_end.first + duration - persistent_data->points[a].time;
				if (persistent_data->points[a].type == simd::CurveControlPoint::Interpolation::Custom)
					std::swap(persistent_data->points[a].left_dt, persistent_data->points[a].right_dt);

				persistent_data->points[a].left_dt = -persistent_data->points[a].left_dt;
				persistent_data->points[a].right_dt = -persistent_data->points[a].right_dt;
			}

			std::stable_sort(persistent_data->points.begin(), persistent_data->points.end(), [](const auto& a, const auto& b) { return a.time < b.time; });
		}

		void FlipY(PersistentData* persistent_data)
		{
			persistent_data->changed = true;
			const auto min_max = simd::CurveUtility::CalculateMinMax(persistent_data->points.data(), persistent_data->points.size(), 0.0f);

			const auto d = min_max.second + min_max.first;

			for (size_t a = 0; a < persistent_data->points.size(); a++)
			{
				persistent_data->points[a].value = d - persistent_data->points[a].value;
				persistent_data->points[a].left_dt = -persistent_data->points[a].left_dt;
				persistent_data->points[a].right_dt = -persistent_data->points[a].right_dt;
			}
		}

		void Reset(PersistentData* persistent_data, float default_value, float* variance, CurveControlFlags flags)
		{
			persistent_data->changed = true;
			persistent_data->min_value = default_value - 1.0f;
			persistent_data->max_value = default_value + 1.0f;
			if (variance)
				*variance = 0.0f;

			if (flags & CurveControlFlags_NoNegative)
			{
				if (persistent_data->min_value < 0.0f)
				{
					persistent_data->max_value -= persistent_data->min_value;
					persistent_data->min_value = 0.0f;
				}
			}

			for (auto& a : persistent_data->points)
			{
				a.value = default_value;
				a.type = simd::CurveControlPoint::Interpolation::Linear;
			}

			while (persistent_data->points.size() > 2 && persistent_data->points.can_erase())
				persistent_data->points.erase(1);
		}

		void Copy(PersistentData* persistent_data, float* variance, std::stringstream& tmp)
		{
			tmp << persistent_data->points.size();
			for (size_t a = 0; a < persistent_data->points.size(); a++)
			{
				const auto& p = persistent_data->points[a];
				tmp << " " << p.time << " " << p.value << " \"" << magic_enum::enum_name(p.type) << "\"";
				if (p.type == simd::CurveControlPoint::Interpolation::Custom)
					tmp << " " << p.left_dt << " " << p.right_dt;
				else if (p.type == simd::CurveControlPoint::Interpolation::CustomSmooth)
					tmp << " " << p.left_dt;
			}

			tmp << " " << (variance ? *variance : 0.0f);
		}

		void Copy(PersistentData* persistent_data, float* variance)
		{
			if (active_selector && ImGui::GetIO().KeyCtrl)
			{
				if (!active_selector->copy)
				{
					active_selector->copy = true;
					active_selector->copy_tmp << "DataTrackMulti " << active_selector->count;
				}
				else if (active_selector->state != SelectorState::Post)
					return;

				active_selector->copy_tmp << "\n";
				Copy(persistent_data, variance, active_selector->copy_tmp);
			}
			else
			{
				std::stringstream tmp;
				tmp << "DataTrack ";
				Copy(persistent_data, variance, tmp);

				const auto t = tmp.str();
				ImGui::SetClipboardText(t.c_str());
			}
		}

		void Paste(PersistentData* persistent_data, float* variance, CurveControlFlags flags)
		{
			std::stringstream tmp;
			if( const char* clipboard = ImGui::GetClipboardText() )
				tmp << clipboard;

			float new_variance = variance ? *variance : 0.0f;
			Memory::SmallVector<simd::CurveControlPoint, 8, Memory::Tag::Unknown> points;
			const auto start_end = simd::CurveUtility::GetStartEndTime(persistent_data->points.data(), persistent_data->points.size());

			std::string s;
			tmp >> s;
			if (s == "Graph1D")
			{
				float v;
				size_t i;
				size_t dim;
				tmp >> v >> dim >> i;
				if (!tmp || v < 0.0f || dim != 1 || i < persistent_data->points.get_min_size() || i > persistent_data->points.get_max_size())
				{
					ImGui::OpenPopup(CopyPasteErrorPopup);
					return;
				}

				for (size_t a = 0; a < i; a++)
				{
					simd::CurveControlPoint p;
					tmp >> p.time >> p.value;

					if (tmp && ExtractString(tmp, s))
					{
						if (auto mode = magic_enum::enum_cast<simd::CurveControlPoint::Interpolation>(s))
						{
							p.type = *mode;
							p.left_dt = p.right_dt = 0.0f;

							if (!tmp)
							{
								ImGui::OpenPopup(CopyPasteErrorPopup);
								return;
							}
						}
						else
						{
							ImGui::OpenPopup(CopyPasteErrorPopup);
							return;
						}
					}
					else
					{
						ImGui::OpenPopup(CopyPasteErrorPopup);
						return;
					}

					points.push_back(p);
				}

				if (variance)
					new_variance = v;
			}
			else
			{
				size_t skip_lines = 0;

				if (s == "DataTrackMulti")
				{
					size_t count = 0;
					tmp >> count;
					if(!tmp || count == 0)
					{
						ImGui::OpenPopup(CopyPasteErrorPopup);
						return;
					}

					if (active_selector && count == active_selector->count)
					{
						active_selector->paste = true;
						skip_lines = active_selector->current_id;
					}
				}
				else if (s != "DataTrack")
				{
					ImGui::OpenPopup(CopyPasteErrorPopup);
					return;
				}

				for (size_t a = 0; a <= skip_lines; a++)
				{
					points.clear();

					size_t i = 0;
					tmp >> i;
					if (tmp && i >= persistent_data->points.get_min_size() && i <= persistent_data->points.get_max_size())
					{
						for (size_t a = 0; a < i; a++)
						{
							simd::CurveControlPoint p;
							tmp >> p.time >> p.value;

							if (tmp && ExtractString(tmp, s))
							{
								if (auto mode = magic_enum::enum_cast<simd::CurveControlPoint::Interpolation>(s))
								{
									p.type = *mode;

									if (p.type == simd::CurveControlPoint::Interpolation::Custom)
										tmp >> p.left_dt >> p.right_dt;
									else if (p.type == simd::CurveControlPoint::Interpolation::CustomSmooth)
										tmp >> p.left_dt;

									if (!tmp)
									{
										ImGui::OpenPopup(CopyPasteErrorPopup);
										return;
									}
								}
								else
								{
									ImGui::OpenPopup(CopyPasteErrorPopup);
									return;
								}
							}
							else
							{
								ImGui::OpenPopup(CopyPasteErrorPopup);
								return;
							}

							points.push_back(p);
						}

						float v = 0;
						tmp >> v;

						if (variance)
						{
							if (tmp && v > 0)
								new_variance = v;
							else
								new_variance = 0.0f;
						}
					}
					else
					{
						ImGui::OpenPopup(CopyPasteErrorPopup);
						return;
					}
				}
			}

			if (points.size() > 0)
			{
				float new_start = std::numeric_limits<float>::infinity();
				float new_end = -std::numeric_limits<float>::infinity();

				for (size_t a = 0; a < points.size(); a++)
				{
					new_start = std::min(new_start, points[a].time);
					new_end = std::max(new_end, points[a].time);
				}

				for (size_t a = 0; a < points.size(); a++)
					points[a].time = start_end.first + ((start_end.second - start_end.first) * (points[a].time - new_start) / (new_end - new_start));

				if (persistent_data->points.size() > points.size())
					persistent_data->points.erase(persistent_data->points.begin() + points.size(), persistent_data->points.end());

				for (size_t a = 0; a < persistent_data->points.size(); a++)
					persistent_data->points[a] = points[a];

				for (size_t a = persistent_data->points.size(); a < points.size(); a++)
					persistent_data->points.push_back(points[a]);

				persistent_data->changed = true;
				std::stable_sort(persistent_data->points.begin(), persistent_data->points.end(), [](const auto& a, const auto& b) { return a.time < b.time; });

				float new_min = std::numeric_limits<float>::infinity();
				float new_max = -std::numeric_limits<float>::infinity();

				for (size_t a = 0; a < persistent_data->points.size(); a++)
				{
					new_min = std::min(new_min, persistent_data->points[a].value);
					new_max = std::max(new_max, persistent_data->points[a].value);
				}

				if (variance)
				{
					*variance = new_variance;
					new_min -= *variance;
					new_max += *variance;
				}

				if (flags & CurveControlFlags_NoNegative)
				{
					new_min = std::max(new_min, 0.0f);
					new_max = std::max(new_min, new_max);
				}

				if (new_max - new_min < MinScale)
				{
					const float d = 0.5f * (2.0f + new_min - new_max);
					new_max += d;
					new_min -= d;

					if (flags & CurveControlFlags_NoNegative)
					{
						if (new_min < 0.0f)
						{
							new_max -= new_min;
							new_min = 0.0f;
						}
					}
				}

				persistent_data->min_value = new_min;
				persistent_data->max_value = new_max;
			}
		}

		void RenderCopyPastePopup()
		{
			ImGuiWindow* window = ImGui::GetCurrentWindow();
			ImGui::SetNextWindowSizeConstraints(ImVec2(150.0f, ImGui::GetFrameHeight() * 4), ImVec2(FLT_MAX, FLT_MAX));
			if (ImGui::BeginPopupEx(window->GetID(CopyPasteErrorPopup), ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoSavedSettings))
			{
				ImGui::PushTextWrapPos(ImGui::GetContentRegionAvail().x);
				ImGui::TextWrapped("Data Track could not be pasted: The clipboard does not contain a compatible data track.");
				ImGui::PopTextWrapPos();

				if (ImGui::Button("Ok", ImVec2(ImGui::GetContentRegionAvail().x)))
					ImGui::CloseCurrentPopup();

				ImGui::EndPopup();
			}
		}

		void CopyRawSpline(PersistentData* persistent_data, float* variance, CurveControlFlags flags, bool normailised)
		{
			assert(persistent_data->points.size() >= 2);
			const auto num_segments = persistent_data->points.size() - 1;
			simd::vector4* segments = (simd::vector4*)alloca(sizeof(simd::vector4) * num_segments);
			float* times = num_segments > 1 ? (float*)alloca(sizeof(float) * (num_segments - 1)) : (float*)nullptr;
			const auto start_end = simd::CurveUtility::MakeTrack(times, segments, num_segments, persistent_data->points.data(), persistent_data->points.size(), 0.0f, normailised);

			std::stringstream s;
			auto WriteFloat = [&](float x) -> std::ostream& { return s << std::fixed << std::setw(11) << std::setprecision(3) << std::setfill(' ') << x; };

			s << "       Time |    Segments" << std::endl;

			if (!normailised)
				WriteFloat(start_end.first) << " | " << std::endl;

			for (size_t a = 0; a < num_segments; a++)
			{
				WriteFloat(a + 1 < num_segments ? times[a] : start_end.second) << " | ";
				WriteFloat(segments[a].x) << "   ";
				WriteFloat(segments[a].y) << "   ";
				WriteFloat(segments[a].z) << "   ";
				WriteFloat(segments[a].w) << std::endl;
			}

			if (flags & CurveControlFlags_Variance)
			{
				s << "Variance: ";
				WriteFloat(variance ? *variance : 0.0f) << std::endl;
			}

			const auto t = s.str();
			ImGui::SetClipboardText(t.c_str());
		}

		void CopySplineAsGraph(PersistentData* persistent_data, float* variance, CurveControlFlags flags, int mode)
		{
			assert(persistent_data->points.size() >= 2);
			const auto num_segments = persistent_data->points.size() - 1;
			simd::vector4* segments = (simd::vector4*)alloca(sizeof(simd::vector4) * num_segments);
			float* times = num_segments > 1 ? (float*)alloca(sizeof(float) * (num_segments - 1)) : (float*)nullptr;
			simd::CurveUtility::MakeTrack(times, segments, num_segments, persistent_data->points.data(), persistent_data->points.size(), 0.0f, true);

			using JsonEncoding = rapidjson::UTF16LE<>;
			using JsonDocument = rapidjson::GenericDocument< JsonEncoding >;
			using JsonValue = rapidjson::GenericValue< JsonEncoding >;
			using JsonAllocator = rapidjson::MemoryPoolAllocator<>;
			using JsonStringBuffer = rapidjson::GenericStringBuffer<JsonEncoding>;
			using JsonWriter = rapidjson::PrettyWriter<JsonStringBuffer, JsonEncoding, JsonEncoding>;
			using JsonString = rapidjson::GenericStringRef<wchar_t>;

			JsonDocument document;	document.SetObject();
			auto& allocator = document.GetAllocator();

			document.AddMember(L"version", 3, allocator);
			JsonValue group;		group.SetArray();
			group.PushBack(JsonValue().SetString(L"Material", allocator), allocator);

			JsonValue nodes;		nodes.SetArray();
			JsonValue links;		links.SetArray();

			auto ConstantFloat = [&](size_t index, float value, const simd::vector2& pos)
			{
				JsonValue n;
				n.SetObject();
				n.AddMember(L"type", JsonValue().SetString(L"ConstantFloat", allocator), allocator);
				n.AddMember(L"index", JsonValue().SetUint64(index), allocator);

				JsonValue parameters;
				parameters.SetArray();

				JsonValue p;
				p.SetObject();
				Renderer::DrawCalls::ParameterUtil::Save(p, allocator, value, FLT_MAX);
				parameters.PushBack(p, allocator);

				JsonValue ui;
				ui.SetArray();
				ui.PushBack(pos.x, allocator);
				ui.PushBack(pos.y, allocator);

				n.AddMember(L"parameters", parameters, allocator);
				n.AddMember(L"ui_position", ui, allocator);
				nodes.PushBack(n, allocator);
			};

			auto ConstantFloat4 = [&](size_t index, const simd::vector4& values, const simd::vector2& pos)
			{
				JsonValue n;
				n.SetObject();
				n.AddMember(L"type", JsonValue().SetString(L"ConstantFloat4", allocator), allocator);
				n.AddMember(L"index", JsonValue().SetUint64(index), allocator);

				JsonValue parameters;
				parameters.SetArray();

				JsonValue p;
				p.SetObject();
				Renderer::DrawCalls::ParameterUtil::Save(p, allocator, values, simd::vector4(FLT_MAX), false, 4);
				parameters.PushBack(p, allocator);

				JsonValue ui;
				ui.SetArray();
				ui.PushBack(pos.x, allocator);
				ui.PushBack(pos.y, allocator);

				n.AddMember(L"parameters", parameters, allocator);
				n.AddMember(L"ui_position", ui, allocator);
				nodes.PushBack(n, allocator);
			};

			auto Node = [&](size_t index, const simd::vector2& pos, const wchar_t* type)
			{
				JsonValue n;
				n.SetObject();
				n.AddMember(L"type", JsonValue().SetString(type, allocator), allocator);
				n.AddMember(L"index", JsonValue().SetUint64(index), allocator);

				JsonValue ui;
				ui.SetArray();
				ui.PushBack(pos.x, allocator);
				ui.PushBack(pos.y, allocator);

				n.AddMember(L"ui_position", ui, allocator);
				nodes.PushBack(n, allocator);
			};

			auto SelectFloat4 = [&](size_t index, const simd::vector2& pos) { Node(index, pos, L"SelectFloat4"); };
			auto Saturate = [&](size_t index, const simd::vector2& pos) { Node(index, pos, L"Saturate"); };
			auto LessThan = [&](size_t index, const simd::vector2& pos) { Node(index, pos, L"LessThan"); };
			auto Multiply = [&](size_t index, const simd::vector2& pos) { Node(index, pos, L"Multiply"); };

			auto Link = [&](const wchar_t* srcType, size_t srcIndex, const wchar_t* srcVar, const wchar_t* srcSwizzle, const wchar_t* dstType, size_t dstIndex, const wchar_t* dstVar, const wchar_t* dstSwizzle)
			{
				JsonValue l;
				l.SetObject();

				JsonValue src;
				src.SetObject();
				src.AddMember(L"type", JsonValue().SetString(srcType, allocator), allocator);
				src.AddMember(L"index", JsonValue().SetUint64(srcIndex), allocator);
				src.AddMember(L"variable", JsonValue().SetString(srcVar, allocator), allocator);
				if (std::wstring_view(srcSwizzle).length() > 0)
					src.AddMember(L"swizzle", JsonValue().SetString(srcSwizzle, allocator), allocator);

				JsonValue dst;
				dst.SetObject();
				dst.AddMember(L"type", JsonValue().SetString(dstType, allocator), allocator);
				dst.AddMember(L"index", JsonValue().SetUint64(dstIndex), allocator);
				dst.AddMember(L"variable", JsonValue().SetString(dstVar, allocator), allocator);
				if (std::wstring_view(dstSwizzle).length() > 0)
					dst.AddMember(L"swizzle", JsonValue().SetString(dstSwizzle, allocator), allocator);

				l.AddMember(L"src", src, allocator);
				l.AddMember(L"dst", dst, allocator);
				links.PushBack(l, allocator);
			};

			Saturate(0, simd::vector2(0.0f, 310.0f));
			Multiply(0, simd::vector2(0.0f, 510.0f));
			Node(0, simd::vector2(400.0f, 510.0f), L"Dummy4");
			
			Link(L"Saturate", 0, L"output", L"", L"Multiply", 0, L"a", L"");
			Link(L"Saturate", 0, L"output", L"", L"Multiply", 0, L"b", L"");
			
			if (mode < 0)
			{
				Node(0, simd::vector2(std::max(300.0f * num_segments, 720.0f), 170.0f), L"DotProduct4");
				Node(0, simd::vector2(200.0f, 510.0f), L"One");
				Node(0, simd::vector2(200.0f, 590.0f), L"Zero");
				Node(0, simd::vector2(200.0f, 670.0f), L"Two");
				Node(0, simd::vector2(680.0f, 530.0f), L"Add");
				Node(0, simd::vector2(850.0f, 510.0f), L"Multiply4");

				Link(L"Multiply", 0, L"output", L"", L"Dummy4", 0, L"in_value", L"x");
				Link(L"Saturate", 0, L"output", L"", L"Dummy4", 0, L"in_value", L"y");
				Link(L"One", 0, L"output", L"", L"Dummy4", 0, L"in_value", L"z");
				Link(L"Zero", 0, L"output", L"", L"Dummy4", 0, L"in_value", L"w");
				Link(L"Two", 0, L"output", L"", L"Add", 0, L"a", L"");
				Link(L"One", 0, L"output", L"", L"Add", 0, L"b", L"");
				Link(L"Dummy4", 0, L"out_value", L"", L"Multiply4", 0, L"a", L"");
				Link(L"Add", 0, L"output", L"", L"Multiply4", 0, L"b", L"x");
				Link(L"Two", 0, L"output", L"", L"Multiply4", 0, L"b", L"y");
				Link(L"One", 0, L"output", L"", L"Multiply4", 0, L"b", L"z");
				Link(L"Zero", 0, L"output", L"", L"Multiply4", 0, L"b", L"w");
				Link(L"Multiply4", 0, L"output", L"", L"DotProduct4", 0, L"b", L"");
			}
			else if (mode == 0)
			{
				Node(0, simd::vector2(std::max(300.0f * num_segments, 720.0f), 170.0f), L"DotProduct4");
				Multiply(1, simd::vector2(200.0f, 510.0f));
				Node(0, simd::vector2(250.0f, 620.0f), L"One");

				Link(L"Saturate", 0, L"output", L"", L"Multiply", 1, L"b", L"");
				Link(L"Multiply", 0, L"output", L"", L"Multiply", 1, L"a", L"");
				Link(L"Multiply", 1, L"output", L"", L"Dummy4", 0, L"in_value", L"x");
				Link(L"Multiply", 0, L"output", L"", L"Dummy4", 0, L"in_value", L"y");
				Link(L"Saturate", 0, L"output", L"", L"Dummy4", 0, L"in_value", L"z");
				Link(L"One", 0, L"output", L"", L"Dummy4", 0, L"in_value", L"w");
				Link(L"Dummy4", 0, L"out_value", L"", L"DotProduct4", 0, L"b", L"");
			}
			else
			{
				Multiply(1, simd::vector2(200.0f, 510.0f));
				Multiply(2, simd::vector2(200.0f, 620.0f));
				Node(0, simd::vector2(680.0f, 510.0f), L"Two");
				Node(0, simd::vector2(680.0f, 590.0f), L"One");
				Node(0, simd::vector2(850.0f, 510.0f), L"Add");
				Node(1, simd::vector2(850.0f, 620.0f), L"Add");
				Node(0, simd::vector2(1020.0f, 510.0f), L"Divide4");

				Link(L"Multiply", 0, L"output", L"", L"Multiply", 1, L"a", L"");
				Link(L"Multiply", 0, L"output", L"", L"Multiply", 1, L"b", L"");
				Link(L"Multiply", 0, L"output", L"", L"Multiply", 2, L"a", L"");
				Link(L"Saturate", 0, L"output", L"", L"Multiply", 2, L"b", L"");

				Link(L"Multiply", 1, L"output", L"", L"Dummy4", 0, L"in_value", L"x");
				Link(L"Multiply", 2, L"output", L"", L"Dummy4", 0, L"in_value", L"y");
				Link(L"Multiply", 0, L"output", L"", L"Dummy4", 0, L"in_value", L"z");
				Link(L"Saturate", 0, L"output", L"", L"Dummy4", 0, L"in_value", L"w");

				Link(L"Two", 0, L"output", L"", L"Add", 0, L"a", L"");
				Link(L"Two", 0, L"output", L"", L"Add", 0, L"b", L"");
				Link(L"Two", 0, L"output", L"", L"Add", 1, L"a", L"");
				Link(L"One", 0, L"output", L"", L"Add", 1, L"b", L"");

				Link(L"Dummy4", 0, L"out_value", L"", L"Divide4", 0, L"a", L"");
				Link(L"Add", 0, L"output", L"", L"Divide4", 0, L"b", L"x");
				Link(L"Add", 1, L"output", L"", L"Divide4", 0, L"b", L"y");
				Link(L"Two", 0, L"output", L"", L"Divide4", 0, L"b", L"z");
				Link(L"One", 0, L"output", L"", L"Divide4", 0, L"b", L"w");
			}

			if (mode > 0)
			{
				if (num_segments == 1)
				{
					ConstantFloat4(0, segments[0], simd::vector2(200.0f, 180.0f));
					Node(0, simd::vector2(200.0f, 350.0f), L"DotProduct4");

					Link(L"ConstantFloat4", 0, L"output", L"", L"DotProduct4", 0, L"a", L"");
					Link(L"Divide4", 0, L"output", L"", L"DotProduct4", 0, L"b", L"");
				}
				else
				{
					Node(1, simd::vector2(0.0f, 150.0f), L"Zero");

					for (size_t a = 0; a < num_segments; a++)
					{
						float t1 = a + 1 < num_segments ? times[a] : 1.0f;
						float t2 = t1 * t1;
						float t4 = t2 * t2;

						const float x = (400.0f * a) + 200.0f;
						ConstantFloat4((a * 2) + 0, segments[a], simd::vector2(x, -210.0f));
						ConstantFloat4((a * 2) + 1, simd::vector4(t4 / 4.0f, t2 * t1 / 3.0f, t2 / 2.0f, t1), simd::vector2(x, 320.0f));
						Node(a, simd::vector2(x, 180.0f), L"Clamp4");
						Node((a * 2) + 0, simd::vector2(x, -40.0f), L"DotProduct4");
						Node((a * 2) + 1, simd::vector2(x, 70.0f), L"DotProduct4");
						Node(a, simd::vector2(x + 210.0f, -40.0f), L"Subtract");
						Node(a + 2, simd::vector2(x + 210.0f, 70.0f), L"Add");

						if (a == 0)
						{
							Link(L"Zero", 1, L"output", L"", L"Clamp4", a, L"iMin", L"");
							Link(L"Zero", 1, L"output", L"", L"DotProduct4", (a * 2) + 1, L"b", L"");
							Link(L"Zero", 1, L"output", L"", L"Add", a + 2, L"a", L"");
						}
						else
						{
							Link(L"ConstantFloat4", (a * 2) - 1, L"output", L"", L"Clamp4", a, L"iMin", L"");
							Link(L"ConstantFloat4", (a * 2) - 1, L"output", L"", L"DotProduct4", (a * 2) + 1, L"b", L"");
							Link(L"Add", a + 1, L"output", L"", L"Add", a + 2, L"a", L"");
						}

						Link(L"Divide4", 0, L"output", L"", L"Clamp4", a, L"input", L"");
						Link(L"ConstantFloat4", (a * 2) + 1, L"output", L"", L"Clamp4", a, L"iMax", L"");
						Link(L"Clamp4", a, L"output", L"", L"DotProduct4", (a * 2) + 0, L"b", L"");
						Link(L"ConstantFloat4", (a * 2) + 0, L"output", L"", L"DotProduct4", (a * 2) + 0, L"a", L"");
						Link(L"ConstantFloat4", (a * 2) + 0, L"output", L"", L"DotProduct4", (a * 2) + 1, L"a", L"");
						Link(L"DotProduct4", (a * 2) + 0, L"output", L"", L"Subtract", a, L"a", L"");
						Link(L"DotProduct4", (a * 2) + 1, L"output", L"", L"Subtract", a, L"b", L"");
						Link(L"Subtract", a, L"output", L"", L"Add", a + 2, L"b", L"");
					}
				}

				if ((flags & CurveControlFlags_Variance) && variance && *variance > 0.0f)
				{
					Node(0, simd::vector2(0.0f, 730.0f), L"RandomSeed");
					Node(0, simd::vector2(50.0f, 810.0f), L"Half");
					Node(num_segments, simd::vector2(250.0f, 730.0f), L"Subtract");
					ConstantFloat(0, 2.0f * (*variance), simd::vector2(430.0f, 760.0f));
					Multiply(3, simd::vector2(740.0f, 730.0f));
					Multiply(4, simd::vector2(960.0f, 730.0f));
					Node(num_segments + 2, simd::vector2(std::max((400.0f * num_segments) + 220.0f, 1180.0f), 730.0f), L"Add");

					Link(L"RandomSeed", 0, L"output", L"", L"Subtract", num_segments, L"a", L"");
					Link(L"Half", 0, L"output", L"", L"Subtract", num_segments, L"b", L"");
					Link(L"Subtract", num_segments, L"output", L"", L"Multiply", 3, L"a", L"");
					Link(L"ConstantFloat", 0, L"output", L"", L"Multiply", 3, L"b", L"");
					Link(L"Multiply", 3, L"output", L"", L"Multiply", 4, L"a", L"");
					Link(L"Saturate", 0, L"output", L"", L"Multiply", 4, L"b", L"");
					Link(L"Multiply", 4, L"output", L"", L"Add", num_segments + 2, L"b", L"");

					if (num_segments == 1)
						Link(L"DotProduct4", 0, L"output", L"", L"Add", num_segments + 2, L"a", L"");
					else
						Link(L"Add", num_segments + 1, L"output", L"", L"Add", num_segments + 2, L"a", L"");
				}
			}
			else
			{
				if (num_segments == 1)
				{
					ConstantFloat4(0, segments[0], simd::vector2(0.0f, 0.0f));
					Link(L"ConstantFloat4", 0, L"output", L"", L"DotProduct4", 0, L"a", L"");
				}
				else
				{
					ConstantFloat4(0, segments[0], simd::vector2(0.0f, 0.0f));
					for (size_t a = 1; a < num_segments; a++)
					{
						ConstantFloat4(a, segments[a], simd::vector2(300.0f * a, 0.0f));
						SelectFloat4(a, simd::vector2(300.0f * a, 170.0f));
						LessThan(a, simd::vector2(300.0f * a, 310.0f));
						ConstantFloat(a, times[a - 1], simd::vector2(300.0f * (a - 1), 420.0f));

						if (a == 1)
							Link(L"ConstantFloat4", 0, L"output", L"", L"SelectFloat4", a, L"b", L"");
						else
							Link(L"SelectFloat4", a - 1, L"output", L"", L"SelectFloat4", a, L"b", L"");

						Link(L"ConstantFloat4", a, L"output", L"", L"SelectFloat4", a, L"a", L"");
						Link(L"LessThan", a, L"output", L"", L"SelectFloat4", a, L"condition", L"");
						Link(L"Saturate", 0, L"output", L"", L"LessThan", a, L"a", L"");
						Link(L"ConstantFloat", a, L"output", L"", L"LessThan", a, L"b", L"");
					}

					Link(L"SelectFloat4", num_segments - 1, L"output", L"", L"DotProduct4", 0, L"a", L"");
				}

				if ((flags & CurveControlFlags_Variance) && variance && *variance > 0.0f && mode == 0)
				{
					Node(0, simd::vector2(0.0f, 700.0f), L"RandomSeed");
					Node(0, simd::vector2(50.0f, 780.0f), L"Half");
					Node(0, simd::vector2(250.0f, 700.0f), L"Subtract");
					ConstantFloat(num_segments, 2.0f * (*variance), simd::vector2(430.0f, 730.0f));
					Multiply(2, simd::vector2(740.0f, 700.0f));
					Node(0, simd::vector2(std::max((300.0f * num_segments) + 220.0f, 960.0f), 700.0f), L"Add");

					Link(L"RandomSeed", 0, L"output", L"", L"Subtract", 0, L"a", L"");
					Link(L"Half", 0, L"output", L"", L"Subtract", 0, L"b", L"");
					Link(L"Subtract", 0, L"output", L"", L"Multiply", 2, L"a", L"");
					Link(L"ConstantFloat", num_segments, L"output", L"", L"Multiply", 2, L"b", L"");
					Link(L"Multiply", 2, L"output", L"", L"Add", 0, L"a", L"");
					Link(L"DotProduct4", 0, L"output", L"", L"Add", 0, L"b", L"");
				}
			}

			document.AddMember(L"nodes", nodes, allocator);
			document.AddMember(L"links", links, allocator);

			JsonStringBuffer buffer;
			rapidjson::PrettyWriter<JsonStringBuffer, JsonEncoding, JsonEncoding> writer(buffer);
			document.Accept(writer);

			const auto t = WstringToString(buffer.GetString());
			ImGui::SetClipboardText(t.c_str());
		}

		void RenderCopyRawPopup(PersistentData* persistent_data, float* variance, CurveControlFlags flags)
		{
			ImGui::SetNextWindowPos(ImGui::GetMousePos(), ImGuiCond_Appearing);
			if (ImGui::BeginPopup(CopyRawDataPopup, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings))
			{
				if (ImGui::MenuItem("Copy Raw Data"))
					CopyRawSpline(persistent_data, variance, flags, false);

				if (ImGui::MenuItem("Copy Normalised Data"))
					CopyRawSpline(persistent_data, variance, flags, true);

				if (ImGui::MenuItem("Copy SampleSpline Nodes"))
					CopySplineAsGraph(persistent_data, variance, flags, 0);

				if (ImGui::MenuItem("Copy SampleDerivative Nodes"))
					CopySplineAsGraph(persistent_data, variance, flags, -1);

				if (ImGui::MenuItem("Copy SampleArea Nodes"))
					CopySplineAsGraph(persistent_data, variance, flags, 1);

				ImGui::EndPopup();
			}
		}

		void RenderToolbar(const char* label, PersistentData* persistent_data, float default_value, float* variance, ImVec2* size, CurveControlFlags flags, float additional_space)
		{
			if (size->y <= 0 || size->x <= 0)
				return;

			ImGuiWindow* window = ImGui::GetCurrentWindow();
			const auto y_pos = ImGui::GetCursorScreenPos().y;
			ImGui::BeginGroup();
			ImGui::PushID("Header");
			ImGui::PushItemWidth(size->x);

			const ImGuiStyle& style = ImGui::GetStyle();
			const float frame_height = ImGui::GetFrameHeight();
			const auto button_padding = style.FramePadding.x * 2;

			const auto fit_to_track_w = button_padding + ImGui::CalcTextSize(IMGUI_ICON_FIT).x;
			const auto help_w = button_padding + ImGui::CalcTextSize(IMGUI_ICON_INFO).x;
			const auto popout_w = persistent_data->IsPopout() ? 0.0f : frame_height;
			const auto additional_w = additional_space > 0.0f ? ImGui::GetStyle().ItemInnerSpacing.x + additional_space : 0.0f;
			bool show_toolbar = true;

			auto FitToTrackButton = [&](float padding = 0.0f)
			{
				if (ImGui::Button(IMGUI_ICON_FIT "##FitToTrack", ImVec2(fit_to_track_w + padding, frame_height)))
				{
					const auto min_max = simd::CurveUtility::CalculateMinMax(persistent_data->points.data(), persistent_data->points.size(), 0.0f);
					float new_min = min_max.first;
					float new_max = min_max.second;
					persistent_data->min_value = min_max.first;
					persistent_data->max_value = min_max.second;

					if (variance)
					{
						new_min -= *variance;
						new_max += *variance;
					}

					if (flags & CurveControlFlags_NoNegative)
					{
						new_min = std::max(new_min, 0.0f);
						new_max = std::max(new_min, new_max);
					}

					if (new_max - new_min < MinScale)
					{
						const float d = 0.5f * (2.0f + new_min - new_max);
						new_max += d;
						new_min -= d;

						if (flags & CurveControlFlags_NoNegative)
						{
							if (new_min < 0.0f)
							{
								new_max -= new_min;
								new_min = 0.0f;
							}
						}
					}

					persistent_data->min_value = new_min;
					persistent_data->max_value = new_max;
				}

				if (ImGui::IsItemHovered())
					ImGui::SetTooltip(IMGUI_ICON_FIT " Fit to track");
			};

			auto HelpButton = [&](float padding = 0.0f)
			{
				ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
				ImGui::Button(IMGUI_ICON_INFO "##Help", ImVec2(help_w + padding, frame_height));
				ImGui::PopItemFlag();

				if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
				{
					ImGui::BeginTooltip();
					ImGui::PushTextWrapPos(35.0f * ImGui::GetFontSize());
					ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(style.ItemSpacing.x, 0.0f));

					ImGui::TextWrapped("Drag points with LMB or RMB");
					ImGui::TextWrapped("LMB on point: Show/Edit derivatives");
					ImGui::TextWrapped("%s + LMB on point: Set value explicitly", GetKeyName(KeyEditPoint));

					if (persistent_data->points.can_insert())
						ImGui::TextWrapped("LMB on line: Add point");

					if (persistent_data->points.can_erase())
						ImGui::TextWrapped("RMB on point: Delete point");

					ImGui::TextWrapped("%s: Show Value of Graph in Tooltip", GetKeyName(KeyPointSelect));
					ImGui::TextWrapped("%s + LMB on point: Set/Remove as reference for snapping", GetKeyName(KeyPointSelect));
					ImGui::TextWrapped("%s while dragging: No snapping", GetKeyName(KeyIgnoreSnap));

					ImGui::TextWrapped("MMB on point: Change interpolation mode to next");
					ImGui::TextWrapped("%s + MMB on point: Change interpolation mode to previous", GetKeyName(KeyReverseMode));

					ImGui::TextWrapped("%s while dragging with LMB: Move all points simultaneously", GetKeyName(KeyMoveAll));
					ImGui::TextWrapped("%s while dragging with RMB: Move all points to the value of the currently dragged point (results in straight line)", GetKeyName(KeyMoveAll));

					ImGui::PopStyleVar();
					ImGui::PopTextWrapPos();
					ImGui::EndTooltip();
				}
			};

			if (!persistent_data->IsPopout())
			{
				const auto header_width = size->x - (fit_to_track_w + help_w + popout_w + (3 * style.ItemInnerSpacing.x) + additional_w);

				ImGui::PushStyleColor(ImGuiCol_Header, style.Colors[ImGuiCol_FrameBg]);
				ImGui::PushStyleColor(ImGuiCol_HeaderActive, style.Colors[ImGuiCol_FrameBgActive]);
				ImGui::PushStyleColor(ImGuiCol_HeaderHovered, style.Colors[ImGuiCol_FrameBgHovered]);
				ImGui::SetNextItemWidth(header_width);
				show_toolbar = ImGuiEx::CollapsingHeader(label);
				ImGui::PopStyleColor(3);

				ImGui::SameLine(header_width + ImGui::GetScrollX(), style.ItemInnerSpacing.x);
				FitToTrackButton();

				ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
				HelpButton();

				ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
				if (ImGui::ArrowButtonEx("##Popout", persistent_data->popped_out ? ImGuiDir_Left : ImGuiDir_Right, ImVec2(popout_w, frame_height), 0))
				{
					persistent_data->popped_out = !persistent_data->popped_out;
					if(persistent_data->popped_out)
						last_popup = persistent_data->popout_id;
				}

				if (!persistent_data->popped_out && ImGui::IsItemHovered())
				{
					ImGui::BeginTooltip();
					ImGui::Text("Popout");
					ImGui::EndTooltip();
				}
			}

			if (show_toolbar)
			{
				ImGui::BeginDisabled(persistent_data->points.size() < 2);
				if (ImGui::Button(IMGUI_ICON_OUTDENT "##ExportRaw"))
					ImGui::OpenPopup(CopyRawDataPopup);

				ImGui::EndDisabled();
				ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
				if (ImGui::Checkbox("Equally Spaced", &persistent_data->fixed_timesteps) && persistent_data->fixed_timesteps)
				{
					persistent_data->changed = true;
					const auto start_end = simd::CurveUtility::GetStartEndTime(persistent_data->points.data(), persistent_data->points.size());
					const float time_step = (start_end.second - start_end.first) / (persistent_data->points.size() - 1);

					for (size_t a = 1; a + 1 < persistent_data->points.size(); a++)
						persistent_data->points[a].time = a * time_step;
				}

				if (persistent_data->IsPopout())
				{
					ImGui::SameLine(std::max(0.0f, size->x - help_w), 0.0f);
					HelpButton();
				}

				bool big_buttons = true;
				float copy_w = button_padding + ImGui::CalcTextSize(IMGUI_ICON_EXPORT " Copy").x;
				float paste_w = button_padding + ImGui::CalcTextSize(IMGUI_ICON_IMPORT " Paste").x;
				float flip_x_w = button_padding + ImGui::CalcTextSize(IMGUI_ICON_FLIP_X " Flip X").x;
				float flip_y_w = button_padding + ImGui::CalcTextSize(IMGUI_ICON_FLIP_Y " Flip Y").x;
				float close_w = button_padding + ImGui::CalcTextSize(IMGUI_ICON_CLOSE).x;
				const float num_buttons = persistent_data->IsPopout() ? 6.0f : 5.0f;
				const float spacing = style.ItemInnerSpacing.x * (num_buttons - 1.0f);
				float min_space = copy_w + paste_w + flip_x_w + flip_y_w + close_w + spacing + (persistent_data->IsPopout() ? fit_to_track_w : 0.0f);
				if (min_space > size->x)
				{
					big_buttons = false;
					copy_w = button_padding + ImGui::CalcTextSize(IMGUI_ICON_EXPORT).x;
					paste_w = button_padding + ImGui::CalcTextSize(IMGUI_ICON_IMPORT).x;
					flip_x_w = button_padding + ImGui::CalcTextSize(IMGUI_ICON_FLIP_X).x;
					flip_y_w = button_padding + ImGui::CalcTextSize(IMGUI_ICON_FLIP_Y).x;
					min_space = copy_w + paste_w + flip_x_w + flip_y_w + close_w + spacing + (persistent_data->IsPopout() ? fit_to_track_w : 0.0f);
				}

				float padding = std::max(0.0f, (size->x - min_space) / num_buttons);

				if (ImGui::Button(big_buttons ? IMGUI_ICON_EXPORT " Copy" : IMGUI_ICON_EXPORT, ImVec2(copy_w + padding, frame_height)))
					Copy(persistent_data, variance);

				if (!big_buttons && ImGui::IsItemHovered())
					ImGui::SetTooltip(IMGUI_ICON_EXPORT " Copy");

				ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
				if (ImGui::Button(big_buttons ? IMGUI_ICON_IMPORT " Paste" : IMGUI_ICON_IMPORT, ImVec2(paste_w + padding, frame_height)))
					Paste(persistent_data, variance, flags);

				if (!big_buttons && ImGui::IsItemHovered())
					ImGui::SetTooltip(IMGUI_ICON_IMPORT " Paste");

				ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
				if (ImGui::Button(big_buttons ? IMGUI_ICON_FLIP_X " Flip X" : IMGUI_ICON_FLIP_X, ImVec2(flip_x_w + padding, frame_height)))
				{
					if (active_selector && ImGui::GetIO().KeyCtrl)
						active_selector->flip_x = true;
					else
						FlipX(persistent_data);
				}

				if (!big_buttons && ImGui::IsItemHovered())
					ImGui::SetTooltip(IMGUI_ICON_FLIP_X " Flip X");

				ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
				if (ImGui::Button(big_buttons ? IMGUI_ICON_FLIP_Y " Flip Y" : IMGUI_ICON_FLIP_Y, ImVec2(flip_y_w + padding, frame_height)))
				{
					if (active_selector && ImGui::GetIO().KeyCtrl)
						active_selector->flip_y = true;
					else
						FlipY(persistent_data);
				}

				if (!big_buttons && ImGui::IsItemHovered())
					ImGui::SetTooltip(IMGUI_ICON_FLIP_Y " Flip Y");

				if (persistent_data->IsPopout())
				{
					ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
					FitToTrackButton(padding);
				}

				ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
				if (ImGui::Button(IMGUI_ICON_CLOSE, ImVec2(close_w + padding, frame_height)))
				{
					if (active_selector && ImGui::GetIO().KeyCtrl)
						active_selector->reset = true;
					else
						Reset(persistent_data, default_value, variance, flags);
				}

				if (ImGui::IsItemHovered())
					ImGui::SetTooltip(IMGUI_ICON_CLOSE " Reset");
			}

			RenderCopyPastePopup();
			RenderCopyRawPopup(persistent_data, variance, flags);

			ImGui::PopItemWidth();
			ImGui::PopID();
			ImGui::EndGroup();
			size->y -= ImGui::GetCursorScreenPos().y - y_pos;
		}

		void RenderScale(const char* label, PersistentData* persistent_data, ImVec2* size, CurveControlFlags flags)
		{
			if (size->y <= 0 || size->x <= 0)
				return;

			const auto x_pos = ImGui::GetCursorScreenPos().x;
			ImGui::BeginGroup();

			ImGui::SetNextWindowSizeConstraints(ImVec2(150.0f, ImGui::GetFrameHeight() * 4), ImVec2(FLT_MAX, FLT_MAX));
			if (ImGui::BeginPopupModal("Scale Track###ScaleGraphPopup", nullptr, ImGuiWindowFlags_NoScrollbar))
			{
				const float start_pos = ImGui::GetCursorPosY();
				const auto popup_size = ImGui::GetContentRegionAvail();

				// Mode Selection
				{
					if (ImGui::ArrowButton("##DecreaseScaleMode", ImGuiDir_Left))
					{
						if (auto idx = magic_enum::enum_index(persistent_data->scale.mode))
							persistent_data->scale.mode = magic_enum::enum_values<ScaleGraphMode>()[(*idx + magic_enum::enum_count<ScaleGraphMode>() - 1) % magic_enum::enum_count<ScaleGraphMode>()];
						else
							persistent_data->scale.mode = magic_enum::enum_values<ScaleGraphMode>().front();
					}

					ImGui::SameLine(0.0f, 0.0f);

					ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
					const auto buttonSize = ImVec2(popup_size.x - (2 * ImGui::GetFrameHeight()), ImGui::GetFrameHeight());

					switch (persistent_data->scale.mode)
					{
						case ScaleGraphMode::ByFactor:
						{
							ImGui::Button("Scale by factor", buttonSize);
							break;
						}
						case ScaleGraphMode::ToRange:
						{
							ImGui::Button("Scale by range", buttonSize);
							break;
						}
						default:
							break;
					}

					ImGui::PopItemFlag();

					ImGui::SameLine(0.0f, 0.0f);

					if (ImGui::ArrowButton("##IncreaseScaleMode", ImGuiDir_Right))
					{
						if (auto idx = magic_enum::enum_index(persistent_data->scale.mode))
							persistent_data->scale.mode = magic_enum::enum_values<ScaleGraphMode>()[(*idx + 1) % magic_enum::enum_count<ScaleGraphMode>()];
						else
							persistent_data->scale.mode = magic_enum::enum_values<ScaleGraphMode>().front();
					}
				}

				ImGui::PushItemWidth(popup_size.x);

				switch (persistent_data->scale.mode)
				{
					case ScaleGraphMode::ByFactor:
					{
						float f = persistent_data->scale.factor * 100.0f;
						if (ImGui::DragFloat("##ScaleFactor", &f, 1.0f, 0.0f, 200.0f, "%.1f %%"))
							persistent_data->scale.factor = std::max(0.0f, f / 100.0f);

						break;
					}
					case ScaleGraphMode::ToRange:
					{
						ImGui::DragFloat("##MinValue", &persistent_data->scale.range.first, ScaleSliderSensitivity, (flags & CurveControlFlags_NoNegative) ? 0.0f : -FLT_MAX, persistent_data->scale.range.second - MinScale, "Min: %.3f");
						ImGui::DragFloat("##MaxValue", &persistent_data->scale.range.second, ScaleSliderSensitivity, persistent_data->scale.range.first + MinScale, FLT_MAX, "Max: %.3f");
						break;
					}
					default:
						break;
				}

				ImGui::PopItemWidth();

				// Apply/Cancel buttons
				{
					ImGui::SetCursorPosY(std::max(ImGui::GetCursorPosY(), popup_size.y + start_pos - ImGui::GetFrameHeight()));
					if (ImGui::Button("Apply", ImVec2((popup_size.x - ImGui::GetStyle().ItemSpacing.x) / 2, ImGui::GetFrameHeight())))
					{
						float new_min = persistent_data->min_value;
						float new_max = persistent_data->max_value;

						switch (persistent_data->scale.mode)
						{
							case ScaleGraphMode::ByFactor:
							{
								new_min = persistent_data->min_value * persistent_data->scale.factor;
								new_max = persistent_data->max_value * persistent_data->scale.factor;
								break;
							}
							case ScaleGraphMode::ToRange:
							{
								new_min = persistent_data->scale.range.first;
								new_max = persistent_data->scale.range.second;
								break;
							}
							default:
								break;
						}

						if (flags & CurveControlFlags_NoNegative)
						{
							new_min = std::max(new_min, 0.0f);
							new_max = std::max(new_min, new_max);
						}

						if (new_max - new_min < MinScale)
						{
							const float d = 0.5f * (MinScale + new_min - new_max);
							new_max += d;
							new_min -= d;

							if (flags & CurveControlFlags_NoNegative)
							{
								if (new_min < 0.0f)
								{
									new_max -= new_min;
									new_min = 0.0f;
								}
							}
						}
						
						float scale = (new_max - new_min) / (persistent_data->max_value - persistent_data->min_value);
						for (size_t a = 0; a < persistent_data->points.size(); a++)
							persistent_data->points[a].value = ((persistent_data->points[a].value - persistent_data->min_value) * scale) + new_min;
						
						persistent_data->min_value = new_min;
						persistent_data->max_value = new_max;
						persistent_data->changed = true;

						ImGui::CloseCurrentPopup();
					}

					ImGui::SameLine(0.0f, ImGui::GetStyle().ItemSpacing.x);

					if (ImGui::Button("Cancel", ImVec2((popup_size.x - ImGui::GetStyle().ItemSpacing.x) / 2, ImGui::GetFrameHeight())) || ImGui::IsKeyPressed(ImGuiKey_Escape, false))
						ImGui::CloseCurrentPopup();
				}

				ImGui::EndPopup();
			}

			const auto slider_width = std::max(ImGui::CalcTextSize(IMGUI_ICON_SCALE).x + (2 * ImGui::GetStyle().FramePadding.x), ImGui::GetFrameHeight());
			const auto slider_height = (size->y - (ImGui::GetFrameHeight() + (2 * ImGui::GetStyle().ItemSpacing.y))) / 2;

			if (ImGui::Button(IMGUI_ICON_SCALE "##ScaleGraph", ImVec2(slider_width, ImGui::GetFrameHeight())))
			{
				ImGui::OpenPopup("###ScaleGraphPopup");
				persistent_data->scale.mode = ScaleGraphMode::ByFactor;
				persistent_data->scale.factor = 1.0f;
				persistent_data->scale.range = std::make_pair(persistent_data->min_value, persistent_data->max_value);
			}

			if (ImGui::IsItemHovered())
			{
				ImGui::BeginTooltip();
				ImGui::PushTextWrapPos(35.0f * ImGui::GetFontSize());
				ImGui::TextWrapped("Scale track");
				ImGui::Separator();
				ImGui::TextWrapped("Opens a popup window that allows you to apply a uniform scale to this data track");
				ImGui::PopTextWrapPos();
				ImGui::EndTooltip();
			}

			float new_min = persistent_data->min_value;
			float new_max = persistent_data->max_value;
			bool changed_min_max = false;

			float min_max = persistent_data->min_value + MinScale;
			float max_min = persistent_data->max_value - MinScale;
			for (size_t a = 0; a < persistent_data->points.size(); a++)
			{
				min_max = std::max(min_max, persistent_data->points[a].value);
				max_min = std::min(max_min, persistent_data->points[a].value);
			}

			if (flags & CurveControlFlags_NoNegative)
			{
				new_min = std::max(new_min, 0.0f);
				new_max = std::max(new_min, new_max);
			}

			if (ImGuiEx::VDragFloat("##MaxValue", ImVec2(slider_width, slider_height), &new_max, ScaleSliderSensitivity, min_max, FLT_MAX, "%.2f", ImGuiSliderFlags_AlwaysClamp))
				changed_min_max = true;

			if (ImGui::IsItemHovered())
			{
				ImGui::BeginTooltip();
				ImGui::Text("Highest visible value in graph: %.3f", new_max);
				ImGui::EndTooltip();
			}

			ImGui::BeginDisabled((flags& CurveControlFlags_NoNegative) && max_min <= 0.0f);
			if (ImGuiEx::VDragFloat("##MinValue", ImVec2(slider_width, slider_height), &new_min, ScaleSliderSensitivity, (flags & CurveControlFlags_NoNegative) ? 0.0f : -FLT_MAX, max_min, "%.2f", ImGuiSliderFlags_AlwaysClamp))
				changed_min_max = true;

			ImGui::EndDisabled();

			if (ImGui::IsItemHovered())
			{
				ImGui::BeginTooltip();
				ImGui::Text("Lowest visible value in graph: %.3f", new_min);
				ImGui::EndTooltip();
			}

			if (changed_min_max)
			{
				if (new_min > new_max)
					std::swap(new_min, new_max);

				if (new_max - new_min < MinScale)
				{
					const float d = 0.5f * (MinScale + new_min - new_max);
					new_max += d;
					new_min -= d;
				}

				if (flags & CurveControlFlags_NoNegative)
				{
					if (new_min < 0.0f)
					{
						new_max -= new_min;
						new_min = 0.0f;
					}
				}

				persistent_data->min_value = new_min;
				persistent_data->max_value = new_max;
			}

			ImGui::EndGroup();
			ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);

			size->x -= ImGui::GetCursorScreenPos().x - x_pos;
		}

		void RenderVariance(const char* label, PersistentData* persistent_data, float* variance, ImVec2* size)
		{
			if (!variance)
				return;

			if (size->y <= 0 || size->x <= 0)
				return;

			const auto x_pos = ImGui::GetCursorScreenPos().x;
			ImGui::BeginGroup();

			const auto button_height = ImGui::GetFrameHeight();
			const auto slider_width = std::max({ ImGui::CalcTextSize(IMGUI_ICON_ADD).x + (2 * ImGui::GetStyle().FramePadding.x),ImGui::CalcTextSize(IMGUI_ICON_MINUS).x + (2 * ImGui::GetStyle().FramePadding.x), ImGui::GetFrameHeight() }); 
			const auto slider_height = size->y - (2 * (button_height + ImGui::GetStyle().ItemSpacing.y));

			if (ImGui::Button(IMGUI_ICON_ADD"##VarianceIncrease", ImVec2(slider_width, button_height)))
			{
				*variance += 0.1f * (persistent_data->max_value - persistent_data->min_value);
				persistent_data->changed = true;
			}

			if (ImGui::IsItemHovered())
			{
				ImGui::BeginTooltip();
				ImGui::Text("Increase variance (%.3f)", *variance);
				ImGui::EndTooltip();
			}

			if (ImGuiEx::VDragFloat("##VarianceValue", ImVec2(slider_width, slider_height), variance, VarianceSliderSensitivity * (persistent_data->max_value - persistent_data->min_value), 0.0f, FLT_MAX, "%.2f"))
				persistent_data->changed = true;

			if (ImGui::IsItemHovered())
			{
				ImGui::BeginTooltip();
				ImGui::Text("Variance (%.3f)", *variance);
				ImGui::EndTooltip();
			}

			ImGuiEx::PushDisable(*variance == 0.0f);
			if (ImGui::Button(IMGUI_ICON_MINUS"##VarianceDecrease", ImVec2(slider_width, button_height)))
			{
				*variance = std::max(0.0f, *variance - 0.1f * (persistent_data->max_value - persistent_data->min_value));
				persistent_data->changed = true;
			}

			if (ImGui::IsItemHovered())
			{
				ImGui::BeginTooltip();
				ImGui::Text("Decrease variance (%.3f)", *variance);
				ImGui::EndTooltip();
			}

			ImGuiEx::PopDisable();

			ImGui::EndGroup();
			ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);

			size->x -= ImGui::GetCursorScreenPos().x - x_pos;
		}

		void RenderTrackPointEdit(PersistentData* persistent_data, const ImRect& point_area, float start_time, float end_time)
		{
			ImGuiWindow* window = ImGui::GetCurrentWindow();
			if (window->SkipItems)
				return;

			ImGuiContext& g = *GImGui;
			const ImGuiStyle& style = g.Style;

			char buffer[256] = { '\0' };

			float point_edit_w = std::max(ImGui::CalcTextSize("Point at").x, ImGui::CalcTextSize("Restore Last").x);
			for (const auto& v : magic_enum::enum_entries<simd::CurveControlPoint::Interpolation>())
			{
				ImFormatString(buffer, std::size(buffer), "%.*s", int(v.second.length()), v.second.data());
				point_edit_w = std::max(point_edit_w, ImGui::CalcTextSize(buffer).x);
			}

			const float point_edit_title_w = point_edit_w + (2 * style.FramePadding.x);
			point_edit_w = point_edit_title_w + ImGui::CalcTextSize("0000.0000").x + (2 * style.FramePadding.x) + style.ItemSpacing.x + (2 * (ImGui::GetFrameHeight() + style.FramePadding.x + style.FramePadding.x));

			ImGui::SetNextWindowSizeConstraints(ImVec2(point_edit_w + (2 * style.WindowPadding.x), ImGui::GetFrameHeight() * 4), ImVec2(FLT_MAX, FLT_MAX));
			if (ImGui::BeginPopupEx(window->GetID(PointEditPopup), ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize))
			{
				ImGui::AlignTextToFramePadding();
				ImGui::Text("Point at");

				ImGui::SameLine(point_edit_title_w + style.WindowPadding.x, style.ItemSpacing.x);
				ImGui::PushItemWidth(std::max(4.0f, ImGui::GetContentRegionAvail().x));
				if (persistent_data->edit.selected_point > 0 && persistent_data->edit.selected_point + 1 < persistent_data->points.size())
				{
					const auto duration = end_time - start_time;
					if (ImGui::InputFloat("##TimeInput", &persistent_data->points[persistent_data->edit.selected_point].time, duration / 100.0f, duration / 10.0f, "%.3f", ImGuiInputTextFlags_EnterReturnsTrue))
					{
						persistent_data->changed = true;
						persistent_data->points[persistent_data->edit.selected_point].time = std::clamp(persistent_data->points[persistent_data->edit.selected_point].time, start_time, end_time);
						for (size_t a = 0; a < persistent_data->edit.selected_point; a++)
							persistent_data->points[a].time = std::min(persistent_data->points[a].time, persistent_data->edit.time);

						for (size_t a = persistent_data->edit.selected_point + 1; a < persistent_data->points.size(); a++)
							persistent_data->points[a].time = std::max(persistent_data->points[a].time, persistent_data->edit.time);
					}
				}
				else
					ImGui::Text("%.3f", persistent_data->points[persistent_data->edit.selected_point].time);

				ImGui::Separator();

				auto mode_name = magic_enum::enum_name(persistent_data->points[persistent_data->edit.selected_point].type);
				ImFormatString(buffer, std::size(buffer), "%.*s", int(mode_name.length()), mode_name.data());

				ImGui::SetNextItemWidth(std::max(4.0f, ImGui::GetContentRegionAvail().x));
				if (ImGui::BeginCombo("##ModeSelect", buffer))
				{
					for (const auto& v : magic_enum::enum_entries<simd::CurveControlPoint::Interpolation>())
					{
						ImFormatString(buffer, std::size(buffer), "%.*s", int(v.second.length()), v.second.data());
						if (ImGui::Selectable(buffer, v.first == persistent_data->points[persistent_data->edit.selected_point].type))
						{
							persistent_data->points[persistent_data->edit.selected_point].type = v.first;
							persistent_data->changed = true;
						}
					}

					ImGui::EndCombo();
				}

				if (ImGui::Button("Restore Last##Restore", ImVec2(point_edit_title_w, 0)))
				{
					persistent_data->changed = true;
					persistent_data->points[persistent_data->edit.selected_point].value = persistent_data->edit.value;
					persistent_data->points[persistent_data->edit.selected_point].time = persistent_data->edit.time;
					persistent_data->points[persistent_data->edit.selected_point].type = persistent_data->edit.mode;
				}

				ImGui::SameLine(point_edit_title_w + style.WindowPadding.x, style.ItemSpacing.x);
				ImGui::SetNextItemWidth(std::max(4.0f, ImGui::GetContentRegionAvail().x));
				if (ImGui::InputFloat("##ValueInput", &persistent_data->points[persistent_data->edit.selected_point].value, 1.0f / 100.0f, 1.0f / 10.0f, "%.3f", ImGuiInputTextFlags_EnterReturnsTrue))
					persistent_data->changed = true;

				const float d_scale = ((persistent_data->max_value - persistent_data->min_value) * (point_area.Max.x - point_area.Min.x)) / ((end_time - start_time) * (point_area.Max.y - point_area.Min.y));
				switch (persistent_data->points[persistent_data->edit.selected_point].type)
				{
					case simd::CurveControlPoint::Interpolation::Custom:
					{
						ImGui::PushItemWidth((ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) / 2);
						auto deg_left = std::atan(persistent_data->points[persistent_data->edit.selected_point].left_dt / d_scale);
						if (ImGui::SliderAngle("##LeftDT", &deg_left, -89.9f, 89.9f, "%.1f deg", ImGuiSliderFlags_AlwaysClamp))
						{
							persistent_data->points[persistent_data->edit.selected_point].left_dt = std::tan(deg_left) * d_scale;
							persistent_data->changed = true;
						}

						ImGui::SameLine();

						auto deg_right = std::atan(persistent_data->points[persistent_data->edit.selected_point].right_dt / d_scale);
						if (ImGui::SliderAngle("##RightDT", &deg_right, -89.9f, 89.9f, "%.1f deg", ImGuiSliderFlags_AlwaysClamp))
						{
							persistent_data->points[persistent_data->edit.selected_point].right_dt = std::tan(deg_right) * d_scale;
							persistent_data->changed = true;
						}

						ImGui::PopItemWidth();
						break;
					}
					case simd::CurveControlPoint::Interpolation::CustomSmooth:
					{
						ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);

						auto deg = std::atan(persistent_data->points[persistent_data->edit.selected_point].left_dt / d_scale);
						if (ImGui::SliderAngle("##LeftDT", &deg, -89.9f, 89.9f, "%.1f deg", ImGuiSliderFlags_AlwaysClamp))
						{
							persistent_data->points[persistent_data->edit.selected_point].left_dt = std::tan(deg) * d_scale;
							persistent_data->changed = true;
						}

						break;
					}
					default:
						break;
				}

				ImGui::PopItemWidth();
				ImGui::EndPopup();
			}
		}

		void RenderTrackUI(PersistentData* persistent_data, float* variance, const ImRect& frame_bb, ImGuiID id, const ImRect& point_area, const ImVec2& point_size, float start_time, float end_time, bool frame_hovered, const simd::vector4* spline, const float* spline_times, size_t spline_size)
		{
			ImGuiContext& g = *GImGui;
			const ImGuiStyle& style = g.Style;

			//Render background frame:
			ImGui::RenderNavHighlight(frame_bb, id);
			ImGui::RenderFrame(frame_bb.Min, frame_bb.Max, MakeColor(ColorFrame), true, style.FrameRounding);

			ImGui::PushClipRect(frame_bb.Min, frame_bb.Max, true);

			// Render Zero-line:
			if (persistent_data->min_value < 0 && persistent_data->max_value > 0)
			{
				const float snap_perc = (persistent_data->max_value - 0.0f) / (persistent_data->max_value - persistent_data->min_value);
				RenderAxis(snap_perc, true, point_area, MakeColor(ColorHelpAxis));
			}

			// Render point lines:
			if (persistent_data->fixed_timesteps)
			{
				for (size_t a = 1; a + 1 < persistent_data->points.size(); a++)
				{
					const float perc = (persistent_data->points[a].time - start_time) / (end_time - start_time);
					RenderAxis(perc, false, point_area, MakeColor(ColorHelpAxis));
				}
			}

			if (persistent_data->key_state[KeyPointSelect] && frame_hovered && point_area.Contains(g.IO.MousePos))
			{
				auto res_point = (g.IO.MousePos.x - point_area.Min.x) / point_area.GetWidth();
				RenderAxis(res_point, false, point_area, MakeColor(ColorHelpAxis));
			}

			if (persistent_data->selected_point < persistent_data->points.size())
			{
				const auto x = (persistent_data->points[persistent_data->selected_point].time - start_time) / (end_time - start_time);
				const auto y = (persistent_data->max_value - persistent_data->points[persistent_data->selected_point].value) / (persistent_data->max_value - persistent_data->min_value);
				RenderAxis(x, false, point_area, MakeColor(ColorSelectedAxisActive));
				RenderAxis(y, true, point_area, MakeColor(ColorSelectedAxisActive));
			}

			if (variance && *variance > 0.0f)
			{
				RenderSpline(point_area, persistent_data, spline, spline_times, spline_size, start_time, end_time, MakeColor(ColorVariance), *variance);
				RenderSpline(point_area, persistent_data, spline, spline_times, spline_size, start_time, end_time, MakeColor(ColorVariance), -(*variance));
			}

			RenderSpline(point_area, persistent_data, spline, spline_times, spline_size, start_time, end_time, MakeColor(ColorSpline));
			RenderPoints(point_area, persistent_data, point_size, start_time, end_time);
			RenderDerivatives(point_area, point_size, persistent_data, start_time, end_time);

			ImGui::PopClipRect();
		}

		void RenderTrack(const char* label, PersistentData* persistent_data, float* variance, ImVec2* size, CurveControlFlags flags)
		{
			if (size->y <= 0 || size->x <= 0)
				return;

			ImGuiWindow* window = ImGui::GetCurrentWindow();
			if (window->SkipItems)
				return;

			ImGuiContext& g = *GImGui;
			const ImGuiStyle& style = g.Style;
			const ImGuiID id = window->GetID(label);
			const ImVec2 point_size = GetPointSize(*size);
			const ImRect frame_bb(window->DC.CursorPos, window->DC.CursorPos + *size);
			const ImRect point_area(frame_bb.Min + point_size, frame_bb.Max - point_size);
			const ImRect total_bb(frame_bb.Min, frame_bb.Max);
			
			ImGui::ItemSize(total_bb, style.FramePadding.y);
			if (!ImGui::ItemAdd(total_bb, id, &frame_bb))
				return;

			const bool changed = persistent_data->changed;
			persistent_data->changed = false;
			persistent_data->intern_id = id;
			const bool frame_hovered = ImGui::ItemHoverable(frame_bb, id);
			const auto start_end = simd::CurveUtility::GetStartEndTime(persistent_data->points.data(), persistent_data->points.size());
			persistent_data->hovered_control = persistent_data->hovered_control || frame_hovered;

			ProcessInputBegin(point_area, point_size, persistent_data, frame_hovered, start_end.first, start_end.second);

			assert(persistent_data->points.size() >= 2);
			const auto num_segments = persistent_data->points.size() - 1;
			
			simd::vector4* segments = (simd::vector4*)alloca(sizeof(simd::vector4) * num_segments);
			float* times = num_segments > 1 ? (float*)alloca(sizeof(float) * (num_segments - 1)) : (float*)nullptr;
			simd::CurveUtility::MakeTrack(times, segments, num_segments, persistent_data->points.data(), persistent_data->points.size(), 0.0f, false);

			RenderTrackUI(persistent_data, variance, frame_bb, id, point_area, point_size, start_end.first, start_end.second, frame_hovered, segments, times, num_segments);
			RenderTrackPointEdit(persistent_data, point_area, start_end.first, start_end.second);
			ProcessInputEnd(point_area, point_size, persistent_data, segments, times, num_segments, frame_hovered, start_end.first, start_end.second);

			if (persistent_data->changed && (g.ActiveId == id || g.ActiveId == 0))
				ImGui::MarkItemEdited(id);

			persistent_data->changed |= changed;
		}

		void RenderControl(const char* label, PersistentData* persistent_data, ImVec2 size, float default_value, float* variance, CurveControlFlags flags, float additional_space)
		{
			if (BeginControl(label, persistent_data, &size))
			{
				ImGui::PushItemWidth(size.x);

				if (!(flags & CurveControlFlags_NoDecorations))
				{
					RenderToolbar(label, persistent_data, default_value, variance, &size, flags, additional_space);
					RenderScale(label, persistent_data, &size, flags);
					RenderVariance(label, persistent_data, variance, &size);
				}

				RenderTrack(label, persistent_data, variance, &size, flags);

				ImGui::PopItemWidth();
				EndControl(persistent_data);
			}
		}

		size_t ColorTrackFindVisiblePoints(ColorTrackPoint* visible_points, size_t max_points, PersistentData* (&persistent_data)[4])
		{
			size_t pos[4] = { 0, 0, 0, 0 };
			size_t num_points = 0;

			while (true)
			{
				if (pos[0] >= persistent_data[0]->points.size())
					return num_points;

				float min_value = persistent_data[0]->points[pos[0]].time;
				size_t min_i = 0;
				bool equal = true;

				for (size_t a = 1; a < 4; a++)
				{
					if (pos[a] >= persistent_data[a]->points.size())
						return num_points;

					const auto t = persistent_data[a]->points[pos[a]].time;
					if (std::abs(t - min_value) >= 1e-3f)
						equal = false;

					if (t < min_value)
					{
						min_value = t;
						min_i = a;
					}
				}

				if (equal)
				{
					for (size_t a = 0; a < 4; a++)
						visible_points[num_points].index[a] = pos[a]++;

					num_points++;
				}
				else
					pos[min_i]++;
			}
		}

		void ProcessColorInputBegin(const ImRect& point_area, const ImVec2& point_size, PersistentData* (&persistent_data)[4], bool hovered, float start_time, float end_time, SizeableArrayView<ColorTrackPoint>& visible_points)
		{
			ImGuiContext& g = *GImGui;
			persistent_data[0]->hovered_point = ~size_t(0);
			persistent_data[0]->hovered_derivative = 0;

			// Process clicking on points:
			if (persistent_data[0]->IsDraggingPoint() && !g.IO.MouseDown[persistent_data[0]->GetDraggingButton()] && g.IO.MouseDragMaxDistanceSqr[persistent_data[0]->GetDraggingButton()] < g.IO.MouseDragThreshold * g.IO.MouseDragThreshold && !persistent_data[0]->new_point)
			{
				if (persistent_data[0]->GetDraggingButton() == ImGuiMouseButton_Left)
				{
					if (persistent_data[0]->key_state[KeyPointSelect])
					{
						if (persistent_data[0]->selected_point == persistent_data[0]->GetDraggingPoint())
							persistent_data[0]->selected_point = ~size_t(0);
						else
							persistent_data[0]->selected_point = persistent_data[0]->GetDraggingPoint();
					}
					else
					{
						for (size_t a = 0; a < 4; a++)
						{
							const auto point_id = visible_points[persistent_data[0]->GetDraggingPoint()].index[a];
							persistent_data[a]->edit.selected_point = persistent_data[0]->GetDraggingPoint();
							persistent_data[a]->edit.value = persistent_data[a]->points[point_id].value;
							persistent_data[a]->edit.time = persistent_data[a]->points[point_id].time;
							persistent_data[a]->edit.mode = persistent_data[a]->points[point_id].type;
						}

						ImGui::OpenPopup(ColorPointEditPopup);
					}
				}
				else if (persistent_data[0]->GetDraggingButton() == ImGuiMouseButton_Right)
				{
					bool can_erase = persistent_data[0]->GetDraggingPoint() < visible_points.size();
					for (size_t a = 0; a < 4; a++)
						can_erase = can_erase && persistent_data[a]->points.can_erase() && visible_points[persistent_data[0]->GetDraggingPoint()].index[a] > 0 && visible_points[persistent_data[0]->GetDraggingPoint()].index[a] + 1 < persistent_data[a]->points.size();

					if (can_erase)
					{
						for (size_t a = 0; a < 4; a++)
						{
							const auto point_id = visible_points[persistent_data[0]->GetDraggingPoint()].index[a];
							persistent_data[a]->points.erase(point_id);
							persistent_data[a]->changed = true;

							for (size_t b = 0; b < visible_points.size(); b++)
								if (visible_points[b].index[a] > point_id)
									visible_points[b].index[a]--;
						}

						visible_points.erase(persistent_data[0]->GetDraggingPoint());

						if (persistent_data[0]->selected_point == persistent_data[0]->GetDraggingPoint())
							persistent_data[0]->selected_point = ~size_t(0);
						else if (persistent_data[0]->selected_point > persistent_data[0]->GetDraggingPoint() && persistent_data[0]->selected_point < visible_points.size())
							persistent_data[0]->selected_point--;
					}
				}

				persistent_data[0]->EndDraggingPoint();
			}

			// Process point dragging/hovering:
			for (size_t a = 0; a < visible_points.size(); a++)
			{
				auto pos = GetColorPointScreenPosition(visible_points[a], point_area, persistent_data, start_time, end_time);
				if (persistent_data[0]->IsDraggingPoint(a))
				{
					persistent_data[0]->hovered_point = a;
					if (persistent_data[0]->GetDraggingButton() != ImGuiMouseButton_Middle && g.IO.MouseDragMaxDistanceSqr[persistent_data[0]->GetDraggingButton()] >= g.IO.MouseDragThreshold * g.IO.MouseDragThreshold)
					{
						const auto last_pos = pos;
						pos.x = ImMax(point_area.Min.x, ImMin(point_area.Max.x, g.IO.MousePos.x));

						for (size_t b = 0; b < 4; b++)
						{
							const auto point_id = visible_points[a].index[b];

							if (persistent_data[b]->points.size() == 1)
								pos.x = point_area.GetCenter().x;
							else if (point_id == 0)
								pos.x = point_area.Min.x;
							else if (point_id + 1 == persistent_data[b]->points.size())
								pos.x = point_area.Max.x;
						}

						auto t = GetColorScreenPositionPoint(pos, point_area, start_time, end_time);
						if (!persistent_data[0]->key_state[KeyIgnoreSnap])
						{
							for (size_t b = 0; b < 4; b++)
							{
								const auto point_id = visible_points[a].index[b];
								if (point_id > 0)
									t = std::max(t, persistent_data[b]->points[point_id - 1].time);

								if (point_id + 1 < persistent_data[b]->points.size())
									t = std::min(t, persistent_data[b]->points[point_id + 1].time);
							}
						}

						t = std::clamp(t, start_time, end_time);

						for (size_t b = 0; b < 4; b++)
						{
							const auto point_id = visible_points[a].index[b];

							persistent_data[b]->points[point_id].time = t;
							persistent_data[b]->changed = true;

							if (persistent_data[0]->key_state[KeyIgnoreSnap])
							{
								if (pos.x < last_pos.x)
								{
									for (size_t c = point_id - 1; c > 0; c--)
										persistent_data[b]->points[c].time = std::min(persistent_data[b]->points[c].time, persistent_data[b]->points[point_id].time);
								}
								else
								{
									for (size_t c = point_id + 1; c + 1 < persistent_data[b]->points.size(); c++)
										persistent_data[b]->points[c].time = std::max(persistent_data[b]->points[c].time, persistent_data[b]->points[point_id].time);
								}
							}
						}
					}
				}
				else if (!persistent_data[0]->IsDraggingPoint() && hovered)
				{
					ImVec2 touch_size(point_size.x + 2, point_size.y + 2);
					ImRect point_covered_area(pos - touch_size, pos + touch_size);
					const ImRect rect_for_touch(point_covered_area.Min - g.Style.TouchExtraPadding, point_covered_area.Max + g.Style.TouchExtraPadding);
					if (g.MouseViewport->GetMainRect().Overlaps(point_covered_area) && rect_for_touch.Contains(g.IO.MousePos))
					{
						if (persistent_data[0]->hovered_point == ~0 || pos.x < point_area.GetCenter().x)
							persistent_data[0]->hovered_point = a;
					}
				}
			}

			// Update dragging state:
			if (!persistent_data[0]->WasDraggingPoint() && hovered && persistent_data[0]->hovered_point < visible_points.size() && (g.IO.MouseClicked[ImGuiMouseButton_Left] || g.IO.MouseClicked[ImGuiMouseButton_Right] || g.IO.MouseClicked[ImGuiMouseButton_Middle]))
			{
				if (g.IO.MouseDown[ImGuiMouseButton_Left])
					persistent_data[0]->BeginDraggingPoint(persistent_data[0]->hovered_point, ImGuiMouseButton_Left);
				else if (g.IO.MouseDown[ImGuiMouseButton_Right])
					persistent_data[0]->BeginDraggingPoint(persistent_data[0]->hovered_point, ImGuiMouseButton_Right);
				else if (g.IO.MouseDown[ImGuiMouseButton_Middle])
					persistent_data[0]->BeginDraggingPoint(persistent_data[0]->hovered_point, ImGuiMouseButton_Middle);

				ImGui::SetActiveID(persistent_data[0]->intern_id, ImGui::GetCurrentWindow());
			}
			else if (persistent_data[0]->WasDraggingPoint() && !g.IO.MouseDown[persistent_data[0]->GetDraggingButton()])
			{
				persistent_data[0]->EndDraggingPoint();
				if (g.ActiveId == persistent_data[0]->intern_id)
					ImGui::ClearActiveID();
			}
		}

		simd::vector4 SampleColor(const TrackView(&tracks)[4], float t, float start_time, float end_time)
		{
			t = std::clamp(t, start_time, end_time);

			simd::vector4 r;
			for (size_t a = 0; a < 4; a++)
				r[unsigned(a)] = simd::CurveUtility::Interpolate(tracks[a].times, tracks[a].segments, tracks[a].num_segments, t, start_time, end_time);

			return r;
		}

		void ProcessColorInputEnd(const ImRect& point_area, const ImVec2& point_size, PersistentData* (&persistent_data)[4], bool hovered, float start_time, float end_time, const TrackView(&tracks)[4], SizeableArrayView<ColorTrackPoint>& visible_points)
		{
			ImGuiContext& g = *GImGui;

			auto ShowTooltip = [&](float time)
			{
				ImGui::BeginTooltipEx(ImGuiWindowFlags_None, ImGuiTooltipFlags_OverridePreviousTooltip);

				const auto& style = ImGui::GetStyle();
				const ImVec4 color = SampleColor(tracks, time, start_time, end_time);
				const ImVec2 sz((ImGui::GetFrameHeightWithSpacing() * 4) - style.ItemSpacing.y, (ImGui::GetFrameHeightWithSpacing() * 4) - style.ItemSpacing.y);
				ImGui::ColorButton("##preview", color, ImGuiColorEditFlags_InputRGB | ImGuiColorEditFlags_AlphaPreviewHalf | ImGuiColorEditFlags_NoTooltip, sz);
				ImGui::SameLine();

				ImGui::BeginGroup();
				ImGui::AlignTextToFramePadding();	ImGui::Text("R: %.2f", color.x);
				ImGui::AlignTextToFramePadding();	ImGui::Text("G: %.2f", color.y);
				ImGui::AlignTextToFramePadding();	ImGui::Text("B: %.2f", color.z);
				ImGui::AlignTextToFramePadding();	ImGui::Text("A: %.2f", color.w);
				ImGui::EndGroup();

				ImGui::EndTooltip();
			};

			// Point Tooltip & Point Adding:
			if (hovered)
			{
				if (!persistent_data[0]->WasDraggingPoint())
				{
					if (persistent_data[0]->hovered_point == ~size_t(0))
					{
						if (visible_points.can_insert())
						{
							auto pos = ImVec2(ImClamp(g.IO.MousePos.x, point_area.Min.x, point_area.Max.x), ImLerp(point_area.Min.y, point_area.Max.y, 0.5f));
							RenderColorPoint(pos, point_size, MakeColor(ColorGraphPoint), false);

							auto t = std::clamp(GetColorScreenPositionPoint(pos, point_area, start_time, end_time), start_time, end_time);

							const auto color = SampleColor(tracks, t, start_time, end_time);
							
							if (g.ActiveId == 0 && g.IO.MouseClicked[ImGuiMouseButton_Left])
							{
								size_t next_vis_point = 0;
								while (next_vis_point < visible_points.size() && persistent_data[0]->points[visible_points[next_vis_point].index[0]].time < t)
									next_vis_point++;

								if (persistent_data[0]->selected_point < visible_points.size() && persistent_data[0]->selected_point >= next_vis_point)
									persistent_data[0]->selected_point++;

								ColorTrackPoint new_vis_point;
								for (size_t a = 0; a < 4; a++)
								{
									assert(persistent_data[a]->points.can_insert());

									simd::CurveControlPoint::Interpolation mode = simd::CurveControlPoint::Interpolation::Linear;
									float min_dist = std::numeric_limits<float>::infinity();
									for (size_t b = 0; b < persistent_data[a]->points.size(); b++)
									{
										const auto p = GetPointScreenPosition(persistent_data[a]->points[b], point_area, persistent_data[a], start_time, end_time);
										const auto l = ImLengthSqr(p - pos);
										if (l < min_dist)
										{
											min_dist = l;
											mode = persistent_data[a]->points[b].type;
										}
									}

									size_t next_point = 0;
									while (next_point < persistent_data[a]->points.size() && persistent_data[a]->points[next_point].time < t)
										next_point++;

									const auto derivative = simd::CurveUtility::CalculateDerivative(tracks[a].times, tracks[a].segments, tracks[a].num_segments, t, start_time, end_time);
									auto new_point = simd::CurveControlPoint(mode, t, color[unsigned(a)], derivative, derivative);

									persistent_data[a]->changed = true;
									persistent_data[a]->new_point = true;
									persistent_data[a]->points.insert(next_point, new_point);
									new_vis_point.index[a] = next_point;

									for (size_t b = 0; b < visible_points.size(); b++)
										if (visible_points[b].index[a] >= next_point)
											visible_points[b].index[a]++;
								}

								visible_points.insert(next_vis_point, new_vis_point);

								persistent_data[0]->BeginDraggingPoint(next_vis_point, ImGuiMouseButton_Left);
								ImGui::SetActiveID(persistent_data[0]->intern_id, ImGui::GetCurrentWindow());
							}

							if (persistent_data[0]->key_state[KeyPointSelect])
								ShowTooltip(t);
						}
					}
					else if (persistent_data[0]->key_state[KeyPointSelect])
						ShowTooltip(persistent_data[0]->points[visible_points[persistent_data[0]->hovered_point].index[0]].time);
				}
				else if (persistent_data[0]->IsDraggingPoint() && persistent_data[0]->key_state[KeyPointSelect])
					ShowTooltip(persistent_data[0]->points[visible_points[persistent_data[0]->GetDraggingPoint()].index[0]].time);
			}

			// Delete all points that are not visible, but have the same time point (eating up invisible points)
			if (persistent_data[0]->WasDraggingPoint())
			{
				for (size_t a = 0; a < 4; a++)
				{
					for (size_t b = 0; b < persistent_data[a]->points.size(); b++)
					{
						bool is_visible = false;
						bool remove = false;

						for (size_t c = 0; c < visible_points.size(); c++)
						{
							if (visible_points[c].index[a] == b)
								is_visible = true;
							else if (std::abs(persistent_data[a]->points[b].time - persistent_data[a]->points[visible_points[c].index[a]].time) < 1e-3f)
								remove = true;
						}

						if (!is_visible && remove)
						{
							persistent_data[a]->changed = true;
							persistent_data[a]->points.erase(b);

							for (size_t c = 0; c < visible_points.size(); c++)
								if (visible_points[c].index[a] > b)
									visible_points[c].index[a]--;

							b--;
						}
					}
				}
			}
		}

		void RenderColorGradient(const ImRect& frame_bb, const TrackView(&tracks)[4], bool hovered, float start_time, float end_time)
		{
			ImGuiWindow* window = ImGui::GetCurrentWindow();
			const auto draw_list = window->DrawList;

			ImGui::RenderColorRectWithAlphaCheckerboard(draw_list, frame_bb.Min, frame_bb.Max, ImColor(0.0f, 0.0f, 0.0f, 0.0f), ImMin(frame_bb.GetWidth(), frame_bb.GetHeight()) / 2.99f, ImVec2(0.0f, 0.0f));

			const float width = frame_bb.GetWidth();
			float start = frame_bb.Min.x;
			ImVec4 start_color = SampleColor(tracks, 0.0f, start_time, end_time);

			while (start < frame_bb.Max.x)
			{
				const float end = std::min(frame_bb.Max.x, start + 1.0f);
				const float t = (end - frame_bb.Min.x) / width;

				const ImVec4 end_color = SampleColor(tracks, t, start_time, end_time);

				draw_list->AddRectFilledMultiColor(ImVec2(start - 0.5f, frame_bb.Min.y), ImVec2(end + 0.5f, frame_bb.Max.y), ImColor(start_color), ImColor(end_color), ImColor(end_color), ImColor(start_color));

				start = start + 1.0f;
				start_color = end_color;
			}
		}

		void RenderColorTrackUI(ImGuiID id, const ImRect& frame_bb, const ImRect& color_bb, const ImRect& point_bb, const ImVec2& point_size, PersistentData* (&persistent_data)[4], bool hovered, float start_time, float end_time, const TrackView(&tracks)[4], SizeableArrayView<ColorTrackPoint>& visible_points)
		{
			ImGuiContext& g = *GImGui;
			const ImGuiStyle& style = g.Style;

			ImGui::RenderNavHighlight(frame_bb, id);
			ImGui::RenderFrame(frame_bb.Min, frame_bb.Max, MakeColor(ColorFrame), true, style.FrameRounding);

			ImGui::PushClipRect(frame_bb.Min, frame_bb.Max, true);
			RenderColorGradient(color_bb, tracks, hovered, start_time, end_time);
			RenderColorPoints(point_bb, point_size, persistent_data, hovered, start_time, end_time, visible_points);
			ImGui::PopClipRect();
		}

		float GetColorTrackPointEditHeight(float width)
		{
			return width + (6 * ImGui::GetFrameHeightWithSpacing()) + ImGui::GetStyle().ItemSpacing.y + (2 * ImGui::GetStyle().WindowPadding.y) - (2 * ImGui::GetStyle().WindowPadding.x);
		}

		void RenderColorTrackPointEdit(PersistentData* (&persistent_data)[4], float start_time, float end_time, SizeableArrayView<ColorTrackPoint>& visible_points)
		{
			ImGuiWindow* window = ImGui::GetCurrentWindow();
			if (window->SkipItems)
				return;

			ImGuiContext& g = *GImGui;
			const ImGuiStyle& style = g.Style;

			float point_edit_w = ImGui::CalcTextSize("Point at").x;

			const float point_edit_title_w = point_edit_w + (2 * style.FramePadding.x);
			point_edit_w = point_edit_title_w + ImGui::CalcTextSize("0000.0000").x + (2 * style.FramePadding.x) + style.ItemSpacing.x + (2 * (ImGui::GetFrameHeight() + style.FramePadding.x + style.FramePadding.x));

			const float color_title_w = std::max({
					ImGui::CalcTextSize("Red").x,
					ImGui::CalcTextSize("Green").x,
					ImGui::CalcTextSize("Blue").x,
					ImGui::CalcTextSize("Alpha").x,
			});

			char buffer[256] = { '\0' };
			float mode_select_w = 0.0f;
			for (const auto& v : magic_enum::enum_entries<simd::CurveControlPoint::Interpolation>())
			{
				ImFormatString(buffer, std::size(buffer), "%.*s", int(v.second.length()), v.second.data());
				mode_select_w = std::max(mode_select_w, ImGui::CalcTextSize(buffer).x + (2 * style.FramePadding.x) + ImGui::GetFrameHeight());
			}

			point_edit_w = std::max(point_edit_w, color_title_w + mode_select_w + ImGui::GetFrameHeight() + (2 * style.ItemSpacing.x)) + (2 * style.WindowPadding.x);

			ImGui::SetNextWindowSizeConstraints(ImVec2(point_edit_w, GetColorTrackPointEditHeight(point_edit_w)), ImVec2(FLT_MAX, FLT_MAX), [](ImGuiSizeCallbackData* data)
			{
				data->DesiredSize.y = GetColorTrackPointEditHeight(data->DesiredSize.x);
			});

			if (ImGui::BeginPopupEx(window->GetID(ColorPointEditPopup), ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_AlwaysAutoResize))
			{
				ImGui::AlignTextToFramePadding();
				ImGui::Text("Point at");
				ImGui::SameLine(point_edit_title_w + style.WindowPadding.x, style.ItemSpacing.x);
				ImGui::PushItemWidth(std::max(4.0f, ImGui::GetContentRegionAvail().x));

				auto time = persistent_data[0]->points[visible_points[persistent_data[0]->edit.selected_point].index[0]].time;

				bool can_edit_time = true;
				for (size_t a = 0; a < 4; a++)
				{
					const auto point_id = visible_points[persistent_data[a]->edit.selected_point].index[a];
					if (point_id == 0 || point_id + 1 >= persistent_data[a]->points.size())
						can_edit_time = false;
				}

				if (can_edit_time)
				{
					const auto duration = end_time - start_time;
					if (ImGui::InputFloat("##TimeInput", &time, duration / 100.0f, duration / 10.0f, "%.3f", ImGuiInputTextFlags_EnterReturnsTrue))
					{
						for (size_t a = 0; a < 4; a++)
						{
							const auto point_id = visible_points[persistent_data[a]->edit.selected_point].index[a];
							persistent_data[a]->changed = true;
							persistent_data[a]->points[point_id].time = std::clamp(time, start_time, end_time);
							for (size_t b = 0; b < point_id; b++)
								persistent_data[a]->points[b].time = std::min(persistent_data[a]->points[b].time, persistent_data[a]->points[point_id].time);

							for (size_t b = point_id + 1; b < persistent_data[a]->points.size(); b++)
								persistent_data[a]->points[b].time = std::max(persistent_data[a]->points[b].time, persistent_data[a]->points[point_id].time);
						}
					}
				}
				else
					ImGui::Text("%.3f", time);

				ImGui::PopItemWidth();
				ImGui::Separator();

				float colors[4] = { 0,0,0,0 };
				float old_colors[4] = { 0,0,0,0 };
				for (size_t a = 0; a < 4; a++)
				{
					const auto point_id = visible_points[persistent_data[a]->edit.selected_point].index[a];
					colors[a] = persistent_data[a]->points[point_id].value;
					old_colors[a] = persistent_data[a]->edit.value;
				}

				ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
				if (ImGui::ColorPicker4("##Picker", colors, ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreviewHalf | ImGuiColorEditFlags_Float | ImGuiColorEditFlags_InputRGB | ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_NoLabel))
				{
					for (size_t a = 0; a < 4; a++)
					{
						const auto point_id = visible_points[persistent_data[a]->edit.selected_point].index[a];
						persistent_data[a]->points[point_id].value = colors[a];
						persistent_data[a]->changed = true;
					}
				}

				for (size_t a = 0; a < 4; a++)
				{
					const auto point_id = visible_points[persistent_data[a]->edit.selected_point].index[a];

					simd::vector4 c(simd::vector3(a < 3 ? 0.0f : 1.0f), 1.0f);
					c[unsigned(a)] = colors[a];

					ImGui::BeginGroup();
					ImGui::PushID(int(a));

					ImGui::ColorButton("##Icon", c, ImGuiColorEditFlags_AlphaPreview);
					if (ImGui::IsItemHovered())
					{
						ImGui::BeginTooltipEx(ImGuiTooltipFlags_OverridePreviousTooltip, ImGuiWindowFlags_None);
						ImGui::Text("%.3f", colors[a]);
						ImGui::EndTooltip();
					}

					ImGui::SameLine(ImGui::GetFrameHeight(), style.ItemSpacing.x);
					
					switch (a)
					{
						case 0: ImGui::Text("Red"); break;
						case 1: ImGui::Text("Green"); break;
						case 2: ImGui::Text("Blue"); break;
						case 3: ImGui::Text("Alpha"); break;
						default: break;
					}

					ImGui::SameLine(color_title_w + ImGui::GetFrameHeight() + style.ItemSpacing.x, style.ItemSpacing.x);
					ImGui::SetNextItemWidth(std::max(4.0f, ImGui::GetContentRegionAvail().x));

					auto mode_name = magic_enum::enum_name(persistent_data[a]->points[point_id].type);
					ImFormatString(buffer, std::size(buffer), "%.*s", int(mode_name.length()), mode_name.data());

					if (ImGui::BeginCombo("##ModeSelect", buffer))
					{
						for (const auto& v : magic_enum::enum_entries<simd::CurveControlPoint::Interpolation>())
						{
							ImFormatString(buffer, std::size(buffer), "%.*s", int(v.second.length()), v.second.data());
							if (ImGui::Selectable(buffer, v.first == persistent_data[a]->points[persistent_data[a]->edit.selected_point].type))
							{
								persistent_data[a]->points[point_id].type = v.first;
								persistent_data[a]->changed = true;
							}
						}

						ImGui::EndCombo();
					}

					ImGui::PopID();
					ImGui::EndGroup();
				}

				ImGui::EndPopup();
			}
		}

		void RenderColorTrack(const char* label, PersistentData* (&persistent_data)[4], ImVec2* size, CurveControlFlags flags)
		{
			if (size->y <= 0 || size->x <= 0)
				return;

			ImGuiWindow* window = ImGui::GetCurrentWindow();
			if (window->SkipItems)
				return;

			ImGuiContext& g = *GImGui;
			const ImGuiStyle& style = g.Style;
			const ImGuiID id = window->GetID(label);

			const auto point_height = (size->y - (2 * style.FrameBorderSize)) * 0.38196601125f; // Golden ratio
			const ImRect frame_bb(window->DC.CursorPos, window->DC.CursorPos + *size);
			const ImRect content_area(frame_bb.Min + ImVec2(style.FrameBorderSize), frame_bb.Max - ImVec2(style.FrameBorderSize));
			const ImRect color_area(content_area.Min, content_area.Max - ImVec2(0.0f, point_height));
			const ImRect point_area(ImVec2(content_area.Min.x, content_area.Max.y - point_height), content_area.Max);
			const ImVec2 point_size(point_height * 0.57735026919f, point_height * 0.5f); // Equilateral triangle
			const ImRect total_bb(frame_bb.Min, frame_bb.Max);

			ImGui::ItemSize(total_bb, style.FramePadding.y);
			if (!ImGui::ItemAdd(total_bb, id, &frame_bb))
				return;

			bool changed = false;
			for (size_t a = 0; a < 4; a++)
			{
				changed |= persistent_data[a]->changed;
				persistent_data[a]->changed = false;
				persistent_data[a]->intern_id = id;
			}

			const bool frame_hovered = ImGui::ItemHoverable(frame_bb, id);

			persistent_data[0]->hovered_control = persistent_data[0]->hovered_control || frame_hovered;

			std::pair<float, float> start_end = { FLT_MAX, -FLT_MAX };
			for (size_t a = 0; a < 4; a++)
			{
				const auto p = simd::CurveUtility::GetStartEndTime(persistent_data[a]->points.data(), persistent_data[a]->points.size());
				start_end.first = std::min(start_end.first, p.first);
				start_end.second = std::max(start_end.second, p.second);
			}

			size_t min_points = 0;
			size_t max_points = ~size_t(0);
			for (size_t a = 0; a < 4; a++)
			{
				max_points = std::min(max_points, persistent_data[a]->points.get_max_size());
				min_points = std::max(min_points, persistent_data[a]->points.get_min_size());
			}

			ColorTrackPoint* visible_points = (ColorTrackPoint*)alloca(sizeof(ColorTrackPoint) * max_points);
			size_t num_visible_points = ColorTrackFindVisiblePoints(visible_points, max_points, persistent_data);
			size_t spill_points = 0;
			for (size_t a = 0; a < 4; a++)
			{
				if (num_visible_points < persistent_data[a]->points.size())
				{
					const auto c = persistent_data[a]->points.size() - num_visible_points;
					const auto d = persistent_data[a]->points.get_max_size() - max_points;
					if (c > d)
						spill_points = std::max(spill_points, c - d);
				}
			}

			auto visible = SizeableArrayView<ColorTrackPoint>(visible_points, num_visible_points, max_points - spill_points, min_points);
			
			ProcessColorInputBegin(point_area, point_size, persistent_data, frame_hovered, start_end.first, start_end.second, visible);

			TrackView tracks[4];
			for (size_t a = 0; a < 4; a++)
			{
				assert(persistent_data[a]->points.size() >= 2);
				const auto num_segments = persistent_data[a]->points.size() - 1;

				simd::vector4* segments = (simd::vector4*)alloca(sizeof(simd::vector4) * num_segments);
				float* times = num_segments > 1 ? (float*)alloca(sizeof(float) * (num_segments - 1)) : (float*)nullptr;
				simd::CurveUtility::MakeTrack(times, segments, num_segments, persistent_data[a]->points.data(), persistent_data[a]->points.size(), 0.0f, false);

				tracks[a].num_segments = num_segments;
				tracks[a].segments = segments;
				tracks[a].times = times;
			}

			RenderColorTrackUI(id, frame_bb, color_area, point_area, point_size, persistent_data, frame_hovered, start_end.first, start_end.second, tracks, visible);
			RenderColorTrackPointEdit(persistent_data, start_end.first, start_end.second, visible);

			ProcessColorInputEnd(point_area, point_size, persistent_data, frame_hovered, start_end.first, start_end.second, tracks, visible);

			for (size_t a = 0; a < 4; a++)
			{
				if (persistent_data[a]->changed && (g.ActiveId == id || g.ActiveId == 0))
					ImGui::MarkItemEdited(id);

				persistent_data[a]->changed |= changed;
			}
		}

		void RenderColorControl(const char* label, PersistentData* (&persistent_data)[4], ImVec2 size, CurveControlFlags flags)
		{
			if (BeginControl(label, persistent_data[0], &size))
			{
				ImGui::PushItemWidth(size.x);

				RenderColorTrack(label, persistent_data, &size, flags);

				ImGui::PopItemWidth();
				EndControl(persistent_data[0]);
			}
		}
	}

	bool CurveControl(const char* label, simd::CurveControlPoint* points, size_t* num_points, size_t max_points, const ImVec2& size, float default_value, float* variance, CurveControlFlags flags, float toolbar_space)
	{
		assert(label && points && num_points && *num_points >= 2 && max_points >= *num_points);

		ImVec2 final_size = size;

		char buffer[1024] = { '\0' };
		ImFormatString(buffer, std::size(buffer), "%s", label);

		if (active_selector)
		{
			const auto my_id = active_selector->current_id++;
			if (my_id >= active_selector->count)
				return false;

			const bool is_selected = my_id == active_selector->selected_id;
			final_size = active_selector->size;

			switch (active_selector->state)
			{
				case SelectorState::Simple:
				{
					ImGui::BeginGroup();
					ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));

					ImFormatString(buffer, std::size(buffer), "%s##Label%u", label, unsigned(my_id));
					if (ImGui::Button(buffer, ImVec2(ImGui::GetFrameHeight(), final_size.y)))
					{
						active_selector->selected_id = my_id;
						ImGui::CloseCurrentPopup();
					}

					ImGui::PopStyleColor();

					ImGui::EndGroup();
					ImGui::SameLine();

					flags |= CurveControlFlags_NoPopOut | CurveControlFlags_NoDecorations;
					break;
				}
				case SelectorState::Button:
				{
					if (is_selected)
					{
						const auto col = ImGui::GetColorU32(ImGuiCol_Button);
						ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetColorU32(ImGuiCol_ButtonHovered));
						ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetColorU32(ImGuiCol_ButtonActive));
						ImGui::PushStyleColor(ImGuiCol_ButtonActive, col);
					}

					ImFormatString(buffer, std::size(buffer), "%s##Select%u", label, unsigned(my_id));
					if (ImGui::Button(label, ImVec2(ImGui::GetFrameHeight(), ImGui::GetFrameHeight())))
						active_selector->selected_id = my_id;

					if (is_selected)
						ImGui::PopStyleColor(3);

					return false;
				}
				case SelectorState::Select:
				{
					if (!is_selected)
						return false;

					break;
				}
				default:
					break;
			}

			ImFormatString(buffer, std::size(buffer), "%s %s", active_selector->buffer, label);
		}

		if (flags & CurveControlFlags_NoPopOut)
			flags &= ~CurveControlFlags_HideWhenPoppedOut;

		last_was_popout = false;
		auto persistent_data = GetPersistentData(ImGui::GetCurrentWindow()->GetID(buffer), points, *num_points, max_points, !(flags & CurveControlFlags_NoRemovePoints));
		if (!IsSorted(persistent_data->points.data(), persistent_data->points.size()))
		{
			std::stable_sort(persistent_data->points.begin(), persistent_data->points.end(), [](const auto& a, const auto& b) { return a.time < b.time; });
			persistent_data->changed = true;
		}

		if (variance && *variance < 0.0f)
		{
			*variance = std::abs(*variance);
			persistent_data->changed = true;
		}

		if (!(flags & CurveControlFlags_Variance))
		{
			if (variance && *variance > 0.0f)
			{
				*variance = 0.0f;
				persistent_data->changed = true;
			}

			variance = nullptr;
		}

		if (flags & CurveControlFlags_NoNegative)
		{
			default_value = std::max(default_value, 0.0f);

			for (auto& point : persistent_data->points)
			{
				if (point.value < 0.0f)
				{
					point.value = 0.0f;
					persistent_data->changed = true;
				}
			}
		}

		float new_min = std::numeric_limits<float>::infinity();
		float new_max = -std::numeric_limits<float>::infinity();

		for (size_t a = 0; a < persistent_data->points.size(); a++)
		{
			new_min = std::min(new_min, persistent_data->points[a].value);
			new_max = std::max(new_max, persistent_data->points[a].value);
		}

		if (persistent_data->first_frame)
		{
			new_min = std::min(new_min, new_min < 0.0f || new_max <= 0.0f ? -1.0f : 0.0f);
			new_max = std::max(new_max, 1.0f);

			if (variance)
			{
				new_min -= *variance;
				new_max += *variance;
			}
		}
		else
		{
			new_min = std::min(new_min, persistent_data->min_value);
			new_max = std::max(new_max, persistent_data->max_value);
		}

		if (flags & CurveControlFlags_NoNegative)
		{
			new_min = std::max(new_min, 0.0f);
			new_max = std::max(new_min, new_max);
		}

		if (new_max - new_min < MinScale)
		{
			const float d = 0.5f * (2.0f + new_min - new_max);
			new_max += d;
			new_min -= d;

			if (flags & CurveControlFlags_NoNegative)
			{
				if (new_min < 0.0f)
				{
					new_max -= new_min;
					new_min = 0.0f;
				}
			}
		}

		if (persistent_data->first_frame || persistent_data->min_value > new_min || ((flags & CurveControlFlags_NoNegative) && persistent_data->min_value < 0.0f))
			persistent_data->min_value = new_min;

		if (persistent_data->first_frame || persistent_data->max_value < new_max)
			persistent_data->max_value = new_max;

		if (active_selector && active_selector->state == SelectorState::Post)
		{
			if (active_selector->flip_x)
				FlipX(persistent_data);

			if (active_selector->flip_y)
				FlipY(persistent_data);

			if (active_selector->reset)
				Reset(persistent_data, default_value, variance, flags);

			if (active_selector->paste)
				Paste(persistent_data, variance, flags);

			if (active_selector->copy)
				Copy(persistent_data, variance);
		}
		else
		{
			ImGuiContext& g = *GImGui;
			ImGui::GetCurrentContext()->NextItemData.ClearFlags();

			ImGui::BeginGroup();
			ImGui::PushID(buffer);
			persistent_data->popout_id = ImGui::GetID("##PopoutID");
			if (!persistent_data->popped_out || !(flags & CurveControlFlags_HideWhenPoppedOut))
				RenderControl(buffer, persistent_data, final_size, default_value, variance, flags, toolbar_space);

			if (persistent_data->popped_out && !(flags & CurveControlFlags_NoPopOut))
			{
				persistent_data->BeginPopout();
				RenderControl(buffer, persistent_data, size, default_value, variance, flags, 0.0f);
				persistent_data->EndPopout();
			}

			if (!persistent_data->hovered_control && !persistent_data->WasDraggingPoint() && !persistent_data->IsDraggingDerivative() && (g.IO.MouseDown[ImGuiMouseButton_Left] || g.IO.MouseDown[ImGuiMouseButton_Right] || g.IO.MouseDown[ImGuiMouseButton_Middle]))
				persistent_data->clicked_point = ~size_t(0);

			ImGui::PopID();
			ImGui::EndGroup();
		}

		*num_points = persistent_data->points.size();
		return persistent_data->changed;
	}

	bool CurveColorControl(const char* label, simd::CurveControlPoint*(&points)[4], size_t(&num_points)[4], size_t max_points, CurveControlFlags flags)
	{
		last_was_popout = false;

		ImVec2 final_size = ImVec2(ImGui::CalcItemWidth(), ImGui::GetFrameHeight());
		ImGui::GetCurrentContext()->NextItemData.ClearFlags();

		char buffer[1024] = { '\0' };

		ImFormatString(buffer, std::size(buffer), "%s##R", label);
		auto persistent_r = GetPersistentData(ImGui::GetCurrentWindow()->GetID(buffer), points[0], num_points[0], max_points, !(flags & CurveControlFlags_NoRemovePoints));

		ImFormatString(buffer, std::size(buffer), "%s##G", label);
		auto persistent_g = GetPersistentData(ImGui::GetCurrentWindow()->GetID(buffer), points[1], num_points[1], max_points, !(flags & CurveControlFlags_NoRemovePoints));

		ImFormatString(buffer, std::size(buffer), "%s##B", label);
		auto persistent_b = GetPersistentData(ImGui::GetCurrentWindow()->GetID(buffer), points[2], num_points[2], max_points, !(flags & CurveControlFlags_NoRemovePoints));

		ImFormatString(buffer, std::size(buffer), "%s##A", label);
		auto persistent_a = GetPersistentData(ImGui::GetCurrentWindow()->GetID(buffer), points[3], num_points[3], max_points, !(flags & CurveControlFlags_NoRemovePoints));

		PersistentData* persistent_channels[] = { persistent_r, persistent_g, persistent_b, persistent_a };

		for (auto& persistent_data : persistent_channels)
		{
			if (!IsSorted(persistent_data->points.data(), persistent_data->points.size()))
			{
				std::stable_sort(persistent_data->points.begin(), persistent_data->points.end(), [](const auto& a, const auto& b) { return a.time < b.time; });
				persistent_data->changed = true;
			}
		}

		ImFormatString(buffer, std::size(buffer), "%s", label);
		ImGui::BeginGroup();
		ImGui::PushID(buffer);
		ImGuiContext& g = *GImGui;

		RenderColorControl(buffer, persistent_channels, final_size, flags);

		ImGui::PopID();
		ImGui::EndGroup();

		bool changed = false;
		for (size_t a = 0; a < 4; a++)
		{
			num_points[a] = persistent_channels[a]->points.size();
			changed = persistent_channels[a]->changed || changed;
		}

		return changed;
	}

	bool IsCurveControlPoppedOut()
	{
		return last_was_popout;
	}

	CurveSelector::CurveSelector(const char* label, size_t count, const ImVec2& size, size_t default_selected)
		: count(count)
		, stage(SelectorStage_Init)
		, size(size)
		, default_selected(default_selected)
	{
		ImFormatString(buffer, std::size(buffer), "%s", label);
	}

	bool CurveSelector::Step()
	{
		if (count == 0)
			return false;

		if (stage == SelectorStage_Init)
		{
			if (size.y <= 0)
				size.y = ImGui::GetContentRegionAvail().y;

			if (size.x <= 0)
				size.x = ImGui::CalcItemWidth();

			assert(active_selector == nullptr);
			const auto id = ImGui::GetID(buffer);
			active_selector = GetPersistentSelector(id);
			active_selector->count = count;

			if (active_selector->selected_id >= count)
				active_selector->selected_id = default_selected < count ? default_selected : 0;

			const auto buffer_end = ImGui::FindRenderedTextEnd(buffer);
			ImFormatString(active_selector->buffer, std::size(active_selector->buffer), "%.*s", unsigned(buffer_end - buffer), buffer);

			ImGui::BeginGroup();
			ImGui::PushID(id);

			if (ImGui::BeginCombo("##CurveSelectCombo", "", ImGuiComboFlags_NoPreview | ImGuiComboFlags_HeightLargest))
			{
				stage = SelectorStage_DropDown;
				active_selector->current_id = 0;
				active_selector->size = ImVec2(size.x - (ImGui::GetFrameHeight() + ImGui::GetStyle().ItemInnerSpacing.x), ImGui::GetFrameHeight() * 3);
				active_selector->state = SelectorState::Simple;
				return true;
			}

			stage = SelectorStage_DropDownClosed;
		}
		else if (stage == SelectorStage_DropDown)
		{
			ImGui::EndCombo();
			stage = SelectorStage_DropDownClosed;
		}

		if (stage == SelectorStage_DropDownClosed)
		{
			const float height = std::max(0.0f, size.y - (ImGui::GetFrameHeight() + (ImGui::GetFrameHeightWithSpacing() * count))) / 2.0f;
			ImGui::SetCursorPosY(ImGui::GetCursorPosY() + height);
			active_selector->current_id = 0;
			active_selector->state = SelectorState::Button;

			stage = SelectorStage_Buttons;
			return true;
		}
		else if (stage == SelectorStage_Buttons)
		{
			ImGui::PopID();
			ImGui::EndGroup();	
			ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
			active_selector->current_id = 0;
			active_selector->size = ImVec2(size.x - (ImGui::GetFrameHeight() + ImGui::GetStyle().ItemInnerSpacing.x), size.y);
			active_selector->state = SelectorState::Select;
			stage = SelectorStage_Graphs;
			return true;
		}
		else if (stage == SelectorStage_Graphs)
		{
			stage = SelectorStage_Post;
			active_selector->current_id = 0;
			active_selector->state = SelectorState::Post;
			return true;
		}

		if (active_selector->copy)
		{
			const auto t = active_selector->copy_tmp.str();
			ImGui::SetClipboardText(t.c_str());
		}

		if (active_selector->paste)
		{
			RenderCopyPastePopup();
		}

		active_selector = nullptr;
		stage = SelectorStage_Init;
		return false;
	}
}
