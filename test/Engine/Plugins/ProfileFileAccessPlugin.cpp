#include "ProfileFileAccessPlugin.h"
#include "Visual/GUI2/imgui/imgui.h"
#include "Visual/GUI2/imgui/imgui_ex.h"
#include "Visual/GUI2/Icons.h"

#include "Visual/Renderer/ShaderSystem.h"
#include "Common/File/FileSystem.h"
#include "Common/Utility/StringManipulation.h"
#include "Common/Utility/HighResTimer.h"

namespace Engine::Plugins
{
#if DEBUG_GUI_ENABLED && defined(PROFILING)
	namespace {
		struct TableItem {
			std::string text;
			float diff;
		};

		void RenderTable(const char* title, bool popped_out, size_t item_count, const std::function<TableItem(size_t)>& get_item, ImGuiWindowFlags window_flags)
		{
			char buffer[1024] = { '\0' };

			auto flags = ImGuiTableFlags_NoBordersInBody | ImGuiTableFlags_SizingStretchProp;
			if (popped_out)
				flags |= ImGuiTableFlags_BordersV | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable | ImGuiTableFlags_ScrollY;
			else
				flags |= ImGuiTableFlags_NoPadOuterX | ImGuiTableFlags_NoSavedSettings;

			if (window_flags & ImGuiWindowFlags_NoInputs)
				flags &= ~(ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY);

			ImVec2 outerSize = ImVec2(750.0f, 400.0f);
			if (popped_out)
				outerSize = ImVec2(ImGui::GetContentRegionAvail().x, std::max(ImGui::GetContentRegionAvail().y - ImGui::GetTextLineHeightWithSpacing(), ImGui::GetTextLineHeight()));

			ImFormatString(buffer, std::size(buffer), "##ScrollArea%s", title);
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(ImGui::GetStyle().ItemSpacing.x, 0.0f));
			
			if (ImGui::BeginChild(buffer, outerSize, false, window_flags))
			{
				ImFormatString(buffer, std::size(buffer), "##Table%s", title);
				if (ImGui::BeginTable(buffer, 1, flags, outerSize))
				{
					ImGui::TableSetupColumn(title, ImGuiTableColumnFlags_WidthStretch);
					if (!popped_out)
					{
						ImGui::TableNextRow();
						const auto column_count = ImGui::TableGetColumnCount();
						for (int a = 0; a < column_count; a++)
						{
							if (!ImGui::TableSetColumnIndex(a))
								continue;

							ImGui::Text("%s", ImGui::TableGetColumnName(a));
						}
					}

					ImGuiListClipper clipper(int(item_count), ImGui::GetTextLineHeightWithSpacing());
					while (clipper.Step())
					{
						for (int a = clipper.DisplayStart; a < clipper.DisplayEnd; a++)
						{
							const auto item = get_item(size_t(a));

							ImGui::TableNextRow();
							ImGui::TableNextColumn();

							const auto col = ImGui::GetColorU32(ImGuiCol_Text, ImLerp(1.0f, 0.3f, std::min(1.0f, item.diff / 5.0f)));
							ImGui::PushStyleColor(ImGuiCol_Text, col);

							ImGuiEx::FormatStringEllipsis(ImGui::GetContentRegionAvail().x, false, buffer, std::size(buffer), "%s", item.text.c_str());
							ImGui::TextUnformatted(buffer);

							ImGui::PopStyleColor();
						}
					}

					ImGui::EndTable();
				}
			}

			ImGui::EndChild();
			ImGui::PopStyleVar();
		}
	}

	void ProfileFileAccessPlugin::RenderDriveAccess()
	{
		const auto file_accesses = File::GetFileAccesses();
		const auto now = HighResTimer::Get().GetTimeUs();

		RenderTable("Drive Access", IsPoppedOut(), file_accesses.size(), [&](size_t i) 
		{
			const auto diff = double(now - file_accesses[i].time) * 0.000001;
			return TableItem{ WstringToString(file_accesses[i].path.c_str()), float(diff) };
		}, GetWindowFlags());
	}

	void ProfileFileAccessPlugin::RenderShaderAccess()
	{
		const auto shader_tasks = ::Shader::System::Get().GetTasks();
		const auto now = HighResTimer::Get().GetTimeUs();

		RenderTable("Shader Tasks", IsPoppedOut(), shader_tasks.size(), [&](size_t i)
		{
			const auto diff = double(now - shader_tasks[i].time) * 0.000001;
			return TableItem{ shader_tasks[i].name.c_str(), float(diff) };
		}, GetWindowFlags());
	}

	void ProfileFileAccessPlugin::OnRender(float elapsed_time)
	{
		if (Begin(750.0f))
		{
			if (IsPoppedOut())
			{
				if (ImGui::BeginTabBar("##Tabs"))
				{
					if (ImGui::BeginTabItem("Drive Access"))
					{
						RenderDriveAccess();
						ImGui::EndTabItem();
					}

					if (ImGui::BeginTabItem("Shader Tasks"))
					{
						RenderShaderAccess();
						ImGui::EndTabItem();
					}

					ImGui::EndTabBar();
				}
			}
			else 
			{
				RenderDriveAccess();
				RenderShaderAccess();
			}

			End();
		}
	}
#endif
}