#include "ProfileDevicePlugin.h"
#include "Visual/GUI2/imgui/imgui.h"
#include "Visual/GUI2/imgui/imgui_ex.h"
#include "Visual/GUI2/Icons.h"

#include "Visual/Device/Device.h"
#include "Visual/Renderer/RendererSubsystem.h"


namespace Engine::Plugins
{
#if DEBUG_GUI_ENABLED && defined(PROFILING)
	namespace {
		bool IsSpace(char c)
		{
			return c == ' ' || c == '\t' || c == '\r' || c == '\n';
		}

		struct ParsedLine {
			bool child;
			const char* start;
			const char* end;

			bool IsEmpty() const { return start >= end; }

			ParsedLine() : child(false), start(nullptr), end(nullptr) {}

			ParsedLine(const std::string& line)
				: start(line.c_str())
				, end(line.c_str() + line.length())
				, child(false)
			{
				while (!IsEmpty() && IsSpace(*start))
					start++;

				if (!IsEmpty() && *start == '-')
				{
					child = true;
					start++;

					while (!IsEmpty() && IsSpace(*start))
						start++;
				}
			}
		};
	}

	void ProfileDevicePlugin::RenderAnimationInfo(const Animation::Stats& stats)
	{
		char buffer0[128] = { '\0' };
		char buffer1[128] = { '\0' };

		if (BeginSection("Animation"))
		{
			const auto overflow = stats.max_size > 0 ? (double)stats.overflow_size / (double)stats.max_size : 0.0;
			const auto usage = stats.max_size > 0 ? (double)stats.used_size / (double)stats.max_size : 0.0;

			ImGui::PushStyleColor(ImGuiCol_Text, overflow < 0.6f ? ImGui::GetColorU32(ImGuiCol_Text) : ImU32(overflow < 0.9f ? ImColor(1.0f, 1.0f, 0.0f) : ImColor(1.0f, 0.0f, 0.0f)));
			ImGui::Text("Palette = %.1f %% (%s / %s)", float(100.0 * usage), ImGuiEx::FormatMemory(buffer0, stats.used_size), ImGuiEx::FormatMemory(buffer1, stats.max_size));
			ImGui::Text("Overflow = %s (Peak = %s)", ImGuiEx::FormatMemory(buffer0, stats.overflow_size), ImGuiEx::FormatMemory(buffer1, peak_animation_overflow));
			ImGui::PopStyleColor();

			EndSection();
		}
	}

	void ProfileDevicePlugin::OnRender(float elapsed_time)
	{
		const auto animation_stats = Animation::System::Get().GetStats();
		peak_animation_overflow = std::max(peak_animation_overflow, uint64_t(animation_stats.overflow_size));

		if (Begin(750.0f))
		{
			char buffer0[128] = { '\0' };
			char buffer1[128] = { '\0' };

			const auto& lines = Renderer::System::Get().GetDevice()->GetStats();
			
			// Renders the lines comming from the device stats in a more interactable way.
			// TODO: Ideally, the device would give a struct with the information to render,
			// rather than a series of lines we have to parse
			auto ParseAndRender = [this, &lines](size_t index, bool default_visible)
			{
				if (index >= lines.size() || lines[index].empty())
					return;

				size_t line_pos = 0;
				auto FindNextLine = [&lines, index, &line_pos]()
				{
					ParsedLine line = lines[index][line_pos++];
					while (line.IsEmpty() && line_pos < lines[index].size())
						line = lines[index][line_pos++];

					return line;
				};

				bool in_section = false;
				bool visible = true;
				size_t section_index = 0;
				char buffer0[128] = { '\0' };
				char buffer1[128] = { '\0' };
				auto prev_line = FindNextLine();
				if (prev_line.IsEmpty())
					return;

				auto RenderText = [&buffer0](const char* str)
				{
					const auto w = ImGui::GetContentRegionAvail().x;
					ImGuiEx::FormatStringEllipsis(w, false, buffer0, std::size(buffer0), "%s", str);
					ImGui::TextUnformatted(buffer0);
				};

				for (; line_pos < lines[index].size();)
				{
					auto line = FindNextLine();
					if (line.IsEmpty())
						break;

					ImFormatString(buffer1, std::size(buffer1), "%.*s", unsigned(prev_line.end - prev_line.start), prev_line.start);
					if (line.child != prev_line.child)
					{
						if (prev_line.child)
						{
							if (visible)
								RenderText(buffer1);

							if (in_section && visible)
								EndSection();

							in_section = false;
							visible = true;
						}
						else
						{
							ImFormatString(buffer0, std::size(buffer0), "##ParsedSection%u_%u", unsigned(index), unsigned(section_index++));
							
							visible = BeginSection(buffer0, buffer1, default_visible);
							in_section = true;
						}
					}
					else if (visible)
						RenderText(buffer1);

					prev_line = line;
				}

				if (!prev_line.IsEmpty() && visible)
				{
					ImFormatString(buffer1, std::size(buffer1), "%.*s", unsigned(prev_line.end - prev_line.start), prev_line.start);
					RenderText(buffer1);
					if (in_section)
						EndSection();
				}
			};

			if (IsPoppedOut())
			{
				if (lines.size() > 2)
				{
					if (ImGui::BeginTabBar("##Tabs"))
					{
						if (ImGui::BeginTabItem("Blend Modes"))
						{
							ParseAndRender(0, false);
							ParseAndRender(1, false);
							RenderAnimationInfo(animation_stats);
							ImGui::EndTabItem();
						}

						if (ImGui::BeginTabItem("Files"))
						{
							ParseAndRender(2, true);
							ImGui::EndTabItem();
						}

						ImGui::EndTabBar();
					}
				}
				else
				{
					ParseAndRender(0, false);
					ParseAndRender(1, false);
					RenderAnimationInfo(animation_stats);
				}
			}
			else if(ImGui::BeginTable("##TableWrapper", 2, ImGuiTableFlags_NoBordersInBody | ImGuiTableFlags_NoSavedSettings | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoPadOuterX))
			{
				ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 250.0f);
				ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthStretch);
				ImGui::TableNextRow();

				if (ImGui::TableSetColumnIndex(0))
				{
					ParseAndRender(0, false);
					ParseAndRender(1, false);
					RenderAnimationInfo(animation_stats);
				}

				if (ImGui::TableSetColumnIndex(1))
					ParseAndRender(2, true);

				ImGui::EndTable();
			}

			End();
		}
	}
#endif
}