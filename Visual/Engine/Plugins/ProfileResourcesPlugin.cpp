#include "ProfileResourcesPlugin.h"
#include "Visual/GUI2/imgui/imgui.h"
#include "Visual/GUI2/imgui/imgui_ex.h"
#include "Visual/GUI2/Icons.h"

#include "Common/Utility/StringManipulation.h"
#include "Common/Resource/ResourceCache.h"

namespace Engine::Plugins
{
#if DEBUG_GUI_ENABLED && defined(PROFILING)
	namespace {
		constexpr unsigned ext_max_size = 1 * 1024 * 1024;
		constexpr unsigned folder_max_size = 2 * 1024 * 1024;

		ImU32 ColorSize(uint64_t size)
		{
			auto col = ImGui::GetColorU32(ImGuiCol_Text);
			if (size >= 100 * Memory::MB)
				col = ImColor(1.0f, 0.0f, 0.0f);
			else if (size >= 10 * Memory::MB)
				col = ImColor(1.0f, 1.0f, 0.0f);

			return col;
		}
	}

	void ProfileResourcePlugin::OnRender(float elapsed_time)
	{
		if (Begin())
		{
			char buffer0[256] = { '\0' };
			char buffer1[256] = { '\0' };
			char buffer2[256] = { '\0' };
			const auto& stats = Resource::LockStats();

			unsigned total_count = 0;
			size_t total_size = 0;
			for (auto& extension : stats.extensions)
			{
				total_count += extension.second.count;
				total_size += extension.second.size;
			}

			ImFormatString(buffer0, std::size(buffer0), "Extensions (> %s): %u (%s)", ImGuiEx::FormatMemory(buffer1, ext_max_size), unsigned(total_count), ImGuiEx::FormatMemory(buffer2, total_size));
			if (BeginSection(buffer0))
			{
				for (auto& extension : stats.extensions)
				{
					const auto count = extension.second.count;
					const auto size = extension.second.size;
					if (size >= ext_max_size)
					{
						ImGui::PushStyleColor(ImGuiCol_Text, ColorSize(size));
						ImGui::Text("%s = %u (%s)", WstringToString(extension.second.name.c_str()).c_str(), unsigned(count), ImGuiEx::FormatMemory(buffer0, size));
						ImGui::PopStyleColor();
					}
				}

				EndSection();
			}

			ImFormatString(buffer0, std::size(buffer0), "Folders (> %s)", ImGuiEx::FormatMemory(buffer1, folder_max_size));
			if (BeginSection(buffer0))
			{
				for (auto& folder : stats.folders)
				{
					const auto count = folder.second.count;
					const auto size = folder.second.size;
					if (size >= folder_max_size)
					{
						ImGui::PushStyleColor(ImGuiCol_Text, ColorSize(size));
						if(folder.second.name.length() > 0)
							ImGui::Text("%s = %u (%s)", WstringToString(folder.second.name.c_str()).c_str(), unsigned(count), ImGuiEx::FormatMemory(buffer0, size));
						else
							ImGui::Text("%u (%s)", unsigned(count), ImGuiEx::FormatMemory(buffer0, size));

						ImGui::PopStyleColor();
					}
				}

				EndSection();
			}

			Resource::UnlockStats();
			End();
		}
	}
#endif
}