#include "ProfileAllocatorPlugin.h"
#include "Visual/GUI2/imgui/imgui.h"
#include "Visual/GUI2/imgui/imgui_ex.h"
#include "Visual/GUI2/Icons.h"

#include "Common/Utility/StringManipulation.h"
#include "Common/Utility/Counters.h"

#include "Include/magic_enum/magic_enum.hpp"

#if defined(PS4)
#include <np_toolkit2.h>
#endif

namespace Engine::Plugins
{
#if DEBUG_GUI_ENABLED && defined(PROFILING)
	namespace {
		ImU32 GetCategoryColor(uint64_t used)
		{
			if (used >= 512 * Memory::MB)		return ImColor(1.0f, 0.0f, 0.0f);
			else if (used >= 128 * Memory::MB)	return ImColor(1.0f, 1.0f, 0.0f);
			else								return ImColor(0.5f, 0.5f, 0.5f);
		}

		ImU32 GetGroupColor(uint64_t used)
		{
			if (used >= 128 * Memory::MB)		return ImColor(1.0f, 0.0f, 0.0f);
			else if (used >= 32 * Memory::MB)	return ImColor(1.0f, 1.0f, 0.0f);
			else								return ImColor(0.5f, 0.5f, 0.5f);
		}

		ImU32 GetTagColor(uint64_t used)
		{
			if (used >= 64 * Memory::MB)		return ImColor(1.0f, 0.0f, 0.0f);
			else if (used >= 16 * Memory::MB)	return ImColor(1.0f, 1.0f, 0.0f);
			else								return ImColor(0.5f, 0.5f, 0.5f);
		}
	}

	void ProfileAllocatorPlugin::RenderCategories(const Memory::Stats& stats)
	{
		ImGui::BeginGroup();

		char buffer[128] = { '\0' };
		uint64_t total_size = 0;
		uint64_t total_used = 0;
		uint64_t total_frame = 0;

		auto flags = ImGuiTableFlags_NoBordersInBody | ImGuiTableFlags_SizingStretchSame;
		if (IsPoppedOut())
			flags |= ImGuiTableFlags_BordersV | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Sortable | ImGuiTableFlags_SortMulti | ImGuiTableFlags_SortTristate;
		else
			flags |= ImGuiTableFlags_NoPadOuterX | ImGuiTableFlags_NoSavedSettings;

		if (GetWindowFlags() & ImGuiWindowFlags_NoInputs)
			flags &= ~(ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY);

		ImVec2 outerSize = ImVec2(300.0f, 0.0f);
		if (IsPoppedOut())
			outerSize = ImVec2(ImGui::GetContentRegionAvail().x, std::max(ImGui::GetContentRegionAvail().y - ImGui::GetTextLineHeightWithSpacing(), ImGui::GetTextLineHeight()));

		enum TableID : ImU32
		{
			ID_Categories,
			ID_Used,
		};

		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(ImGui::GetStyle().ItemSpacing.x, 0));
		ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(ImGui::GetStyle().CellPadding.x, 0));

		if (ImGui::BeginTable("##CategoriesTable", int(magic_enum::enum_count<TableID>()), flags, outerSize))
		{
			ImGui::TableSetupColumn("Categories", ImGuiTableColumnFlags_WidthFixed, 0.0f, ID_Categories);
			ImGui::TableSetupColumn("Used", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_PreferSortDescending, 0.0f, ID_Used);
			if (IsPoppedOut())
			{
				ImGui::TableSetupScrollFreeze(0, 1); // Make row always visible
				ImGui::TableHeadersRow();
			}
			else
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

			std::array<unsigned, (unsigned)Memory::Category::Count> order;
			std::array<size_t, (unsigned)Memory::Category::Count> used_sizes;
			for (unsigned i = 0; i < (unsigned)Memory::Category::Count; ++i)
			{
				order[i] = i;
				used_sizes[i] = stats.category_used_sizes[i].load(std::memory_order_relaxed);
			}

			if (auto sortSpecs = ImGui::TableGetSortSpecs(); sortSpecs && sortSpecs->SpecsCount > 0)
			{
				auto SortCompare = [&](unsigned a, unsigned b, TableID id) -> int
				{
					switch (id)
					{
						case ID_Categories:
						{
							if (a == b)
								return 0;

							return a > b ? 1 : -1;
						}
						case ID_Used:
						{
							const auto ua = used_sizes[a];
							const auto ub = used_sizes[b];

							if (ua == ub)
								return 0;

							return ua > ub ? 1 : -1;
						}
						default:
							break;
					}

					return 0;
				};

				auto SortFunc = [&](unsigned a, unsigned b)
				{
					for (int i = 0; i < sortSpecs->SpecsCount; i++)
					{
						const auto& specs = sortSpecs->Specs[i];
						const auto order = SortCompare(a, b, (TableID)specs.ColumnUserID);

						if (order == 0)
							continue;

						return (order < 0) == (specs.SortDirection == ImGuiSortDirection_Ascending);
					}

					return false;
				};

				std::sort(order.begin(), order.end(), SortFunc);
			}

			for (const auto& i : order)
			{
				const auto used_size = used_sizes[i];

				const auto used_count = stats.category_used_counts[i].load(std::memory_order_relaxed);

				const auto frame_count = stats.last_frame_category_counts[i];

				total_size += used_size;
				total_used += used_count;
				total_frame += frame_count;

				ImGui::TableNextRow();

				// Category
				{
					ImGui::PushStyleColor(ImGuiCol_Text, GetCategoryColor(used_size));
					ImGui::TableNextColumn();
					const auto name = WstringToString(Memory::CategoryName((Memory::Category) i));
					ImGui::Text("%s", name.c_str());
					ImGui::PopStyleColor();
				}

				// Used
				{
					ImGui::PushStyleColor(ImGuiCol_Text, GetCategoryColor(used_size));
					ImGui::TableNextColumn();
					ImGui::Text("%s (%u | %u)", ImGuiEx::FormatMemory(buffer, used_size), unsigned(used_count), unsigned(frame_count));
					ImGui::PopStyleColor();
				}
			}

			ImGui::EndTable();
		}

		ImGui::PopStyleVar(2);

		ImGui::Text("TOTAL: %s (%u | %u)", ImGuiEx::FormatMemory(buffer, total_size), unsigned(total_used), unsigned(total_frame));

		ImGui::EndGroup();
	}

	void ProfileAllocatorPlugin::RenderGroups(const Memory::Stats& stats)
	{
		ImGui::BeginGroup();

		char buffer[128] = { '\0' };
		uint64_t total_size = 0;
		uint64_t total_used = 0;
		uint64_t total_frame = 0;

		auto flags = ImGuiTableFlags_NoBordersInBody | ImGuiTableFlags_SizingStretchSame;
		if (IsPoppedOut())
			flags |= ImGuiTableFlags_BordersV | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Sortable | ImGuiTableFlags_SortMulti | ImGuiTableFlags_SortTristate;
		else
			flags |= ImGuiTableFlags_NoPadOuterX | ImGuiTableFlags_NoSavedSettings;

		if (GetWindowFlags() & ImGuiWindowFlags_NoInputs)
			flags &= ~(ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY);

		ImVec2 outerSize = ImVec2(300.0f, 0.0f);
		if (IsPoppedOut())
			outerSize = ImVec2(ImGui::GetContentRegionAvail().x, std::max(ImGui::GetContentRegionAvail().y - ImGui::GetTextLineHeightWithSpacing(), ImGui::GetTextLineHeight()));

		enum TableID : ImU32
		{
			ID_Groups,
			ID_Used,
		};

		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(ImGui::GetStyle().ItemSpacing.x, 0));
		ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(ImGui::GetStyle().CellPadding.x, 0));

		if (ImGui::BeginTable("##GroupsTable", int(magic_enum::enum_count<TableID>()), flags, outerSize))
		{
			ImGui::TableSetupColumn("Groups", ImGuiTableColumnFlags_WidthFixed, 0.0f, ID_Groups);
			ImGui::TableSetupColumn("Used", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_PreferSortDescending, 0.0f, ID_Used);
			if (IsPoppedOut())
			{
				ImGui::TableSetupScrollFreeze(0, 1); // Make row always visible
				ImGui::TableHeadersRow();
			}
			else
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

			std::array<unsigned, (unsigned)Memory::Group::Count> order;
			std::array<size_t, (unsigned)Memory::Group::Count> used_sizes;
			for (unsigned i = 0; i < (unsigned)Memory::Group::Count; ++i)
			{
				order[i] = i;
				used_sizes[i] = stats.group_used_sizes[i].load(std::memory_order_relaxed);
			}

			if (auto sortSpecs = ImGui::TableGetSortSpecs(); sortSpecs && sortSpecs->SpecsCount > 0)
			{
				auto SortCompare = [&](unsigned a, unsigned b, TableID id) -> int
				{
					switch (id)
					{
						case ID_Groups:
						{
							if (a == b)
								return 0;

							return a > b ? 1 : -1;
						}
						case ID_Used:
						{
							const auto ua = used_sizes[a];
							const auto ub = used_sizes[b];

							if (ua == ub)
								return 0;

							return ua > ub ? 1 : -1;
						}
						default:
							break;
					}

					return 0;
				};

				auto SortFunc = [&](unsigned a, unsigned b)
				{
					for (int i = 0; i < sortSpecs->SpecsCount; i++)
					{
						const auto& specs = sortSpecs->Specs[i];
						const auto order = SortCompare(a, b, (TableID)specs.ColumnUserID);

						if (order == 0)
							continue;

						return (order < 0) == (specs.SortDirection == ImGuiSortDirection_Ascending);
					}

					return false;
				};

				std::sort(order.begin(), order.end(), SortFunc);
			}

			for (const auto& i : order)
			{
				const auto used_size = used_sizes[i];

				const auto used_count = stats.group_used_counts[i].load(std::memory_order_relaxed);

				const auto frame_count = stats.last_frame_group_counts[i];

				total_size += used_size;
				total_used += used_count;
				total_frame += frame_count;

				ImGui::TableNextRow();

				// Group
				{
					ImGui::PushStyleColor(ImGuiCol_Text, GetGroupColor(used_size));
					ImGui::TableNextColumn();
					const auto name = WstringToString(Memory::GroupName((Memory::Group) i));
					ImGui::Text("%s", name.c_str());
					ImGui::PopStyleColor();
				}

				// Used
				{
					ImGui::PushStyleColor(ImGuiCol_Text, GetGroupColor(used_size));
					ImGui::TableNextColumn();
					ImGui::Text("%s (%u | %u)", ImGuiEx::FormatMemory(buffer, used_size), unsigned(used_count), unsigned(frame_count));
					ImGui::PopStyleColor();
				}
			}

			ImGui::EndTable();
		}

		ImGui::PopStyleVar(2);

		ImGui::Text("TOTAL: %s (%u | %u)", ImGuiEx::FormatMemory(buffer, total_size), unsigned(total_used), unsigned(total_frame));

		ImGui::EndGroup();
	}

	void ProfileAllocatorPlugin::RenderTags(const Memory::Stats& stats)
	{
		ImGui::BeginGroup();

		char buffer[128] = { '\0' };
		uint64_t total_size = 0;
		uint64_t total_used = 0;
		uint64_t total_frame = 0;

		auto flags = ImGuiTableFlags_NoBordersInBody | ImGuiTableFlags_SizingStretchSame;
		if (IsPoppedOut())
			flags |= ImGuiTableFlags_BordersV | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Sortable | ImGuiTableFlags_SortMulti | ImGuiTableFlags_SortTristate;
		else
			flags |= ImGuiTableFlags_NoPadOuterX | ImGuiTableFlags_NoSavedSettings;

		if (GetWindowFlags() & ImGuiWindowFlags_NoInputs)
			flags &= ~(ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY);

		ImVec2 outerSize = ImVec2(500.0f, 0.0f);
		if (IsPoppedOut())
			outerSize = ImVec2(ImGui::GetContentRegionAvail().x, std::max(ImGui::GetContentRegionAvail().y - ImGui::GetTextLineHeightWithSpacing(), ImGui::GetTextLineHeight()));

		enum TableID : ImU32
		{
			ID_Tags,
			ID_Used,
			ID_Delta,
			ID_Peak,
		};

		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(ImGui::GetStyle().ItemSpacing.x, 0));
		ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(ImGui::GetStyle().CellPadding.x, 0));

		if (ImGui::BeginTable("##TagsTable", int(magic_enum::enum_count<TableID>()), flags, outerSize))
		{
			ImGui::TableSetupColumn("Tags", ImGuiTableColumnFlags_WidthStretch, 0.0f, ID_Tags);
			ImGui::TableSetupColumn("Used", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_PreferSortDescending, 0.0f, ID_Used);
			ImGui::TableSetupColumn("Delta", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_PreferSortDescending, 0.0f, ID_Delta);
			ImGui::TableSetupColumn("Peak", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_PreferSortDescending, 0.0f, ID_Peak);
			if (IsPoppedOut())
			{
				ImGui::TableSetupScrollFreeze(0, 1); // Make row always visible
				ImGui::TableHeadersRow();
			}
			else
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

			std::array<unsigned, (unsigned)Memory::Tag::Count> order;
			std::array<size_t, (unsigned)Memory::Tag::Count> used_sizes;
			std::array<int64_t, (unsigned)Memory::Tag::Count> delta_sizes;
			std::array<size_t, (unsigned)Memory::Tag::Count> peak_sizes;
			for (unsigned i = 0; i < (unsigned)Memory::Tag::Count; ++i)
			{
				order[i] = i;
				used_sizes[i] = stats.tag_used_sizes[i].load(std::memory_order_relaxed);
				delta_sizes[i] = stats.tag_delta_sizes[i].load(std::memory_order_relaxed);
				peak_sizes[i] = stats.tag_peak_sizes[i].load(std::memory_order_relaxed);
			}

			if (auto sortSpecs = ImGui::TableGetSortSpecs(); sortSpecs && sortSpecs->SpecsCount > 0)
			{
				auto SortCompare = [&](unsigned a, unsigned b, TableID id) -> int
				{
					switch (id)
					{
						case ID_Tags:
						{
							if (a == b)
								return 0;

							return a > b ? 1 : -1;
						}
						case ID_Used:
						{
							const auto ua = used_sizes[a];
							const auto ub = used_sizes[b];

							if (ua == ub)
								return 0;

							return ua > ub ? 1 : -1;
						}
						case ID_Delta:
						{
							const auto ua = delta_sizes[a];
							const auto ub = delta_sizes[b];

							if (ua == ub)
								return 0;

							return ua > ub ? 1 : -1;
						}
						case ID_Peak:
						{
							const auto ua = peak_sizes[a];
							const auto ub = peak_sizes[b];

							if (ua == ub)
								return 0;

							return ua > ub ? 1 : -1;
						}
						default:
							break;
					}

					return 0;
				};

				auto SortFunc = [&](unsigned a, unsigned b)
				{
					for (int i = 0; i < sortSpecs->SpecsCount; i++)
					{
						const auto& specs = sortSpecs->Specs[i];
						const auto order = SortCompare(a, b, (TableID)specs.ColumnUserID);

						if (order == 0)
							continue;

						return (order < 0) == (specs.SortDirection == ImGuiSortDirection_Ascending);
					}

					return false;
				};

				std::sort(order.begin(), order.end(), SortFunc);
			}

			for (const auto& i : order)
			{
				const auto used_size = used_sizes[i];
				const auto delta_size = delta_sizes[i];
				const auto peak_size = peak_sizes[i];

				const auto used_count = stats.tag_used_counts[i].load(std::memory_order_relaxed);
				const auto delta_count = stats.tag_delta_counts[i].load(std::memory_order_relaxed);
				const auto peak_counts = stats.tag_peak_counts[i].load(std::memory_order_relaxed);

				const auto frame_count = stats.last_frame_tag_counts[i];

				total_size += used_size;
				total_used += used_count;
				total_frame += frame_count;

				ImGui::TableNextRow();

				// Tag
				{
					ImGui::PushStyleColor(ImGuiCol_Text, GetTagColor(used_size));
					ImGui::TableNextColumn();
					const auto name = WstringToString(Memory::TagName((Memory::Tag) i));
					ImGui::Text("%s", name.c_str());
					ImGui::PopStyleColor();
				}

				// Used
				{
					ImGui::PushStyleColor(ImGuiCol_Text, GetTagColor(used_size));
					ImGui::TableNextColumn();
					ImGui::Text("%s (%u | %u)", ImGuiEx::FormatMemory(buffer, used_size), unsigned(used_count), unsigned(frame_count));
					ImGui::PopStyleColor();
				}

				// Delta
				{
					ImGui::PushStyleColor(ImGuiCol_Text, GetTagColor(std::max((int64_t)0, delta_size)));
					ImGui::TableNextColumn();
					if (delta_size >= 0)
						ImGui::Text("%s (%d)", ImGuiEx::FormatMemory(buffer, delta_size), unsigned(delta_count));
					else
						ImGui::Text("-%s (%d)", ImGuiEx::FormatMemory(buffer, -delta_size), unsigned(delta_count));
					ImGui::PopStyleColor();
				}

				// Peak
				{
					ImGui::PushStyleColor(ImGuiCol_Text, GetTagColor(peak_size));
					ImGui::TableNextColumn();
					ImGui::Text("%s (%u)", ImGuiEx::FormatMemory(buffer, peak_size), unsigned(peak_counts));
					ImGui::PopStyleColor();
				}

			}

			ImGui::EndTable();
		}

		ImGui::PopStyleVar(2);

		ImGui::Text("TOTAL: %s (%u | %u)", ImGuiEx::FormatMemory(buffer, total_size), unsigned(total_used), unsigned(total_frame));

		ImGui::EndGroup();
	}

	void ProfileAllocatorPlugin::RenderBuckets(const Memory::Stats& memory_stats)
	{
		ImGui::BeginGroup();

		char buffer0[128] = { '\0' };
		char buffer1[128] = { '\0' };
		char buffer2[128] = { '\0' };

		size_t total_tiny_used_size = 0;
		size_t total_tiny_reserved_size = 0;
		for (unsigned i = 0; i < Memory::TinyCount; ++i)
		{
			total_tiny_used_size += memory_stats.tiny_used_sizes[i].load(std::memory_order_relaxed);
			total_tiny_reserved_size += memory_stats.tiny_reserved_sizes[i].load(std::memory_order_relaxed);
		}
		size_t total_small_used_size = 0;
		size_t total_small_reserved_size = 0;
		for (unsigned i = 0; i < Memory::SmallCount; ++i)
		{
			total_small_used_size += memory_stats.small_used_sizes[i].load(std::memory_order_relaxed);
			total_small_reserved_size += memory_stats.small_reserved_sizes[i].load(std::memory_order_relaxed);
		}
		size_t total_medium_used_size = 0;
		size_t total_medium_reserved_size = 0;
		for (unsigned i = 0; i < Memory::MediumCount; ++i)
		{
			total_medium_used_size += memory_stats.medium_used_sizes[i].load(std::memory_order_relaxed);
			total_medium_reserved_size += memory_stats.medium_reserved_sizes[i].load(std::memory_order_relaxed);
		}
		size_t total_large_used_size = 0;
		size_t total_large_reserved_size = 0;
		for (unsigned i = 0; i < Memory::LargeCount; ++i)
		{
			total_large_used_size += memory_stats.large_used_sizes[i].load(std::memory_order_relaxed);
			total_large_reserved_size += memory_stats.large_reserved_sizes[i].load(std::memory_order_relaxed);
		}

		const size_t overhead_size =
			(total_tiny_reserved_size - total_tiny_used_size) +
			(total_small_reserved_size - total_small_used_size) +
			(total_medium_reserved_size - total_medium_used_size) +
			(total_large_reserved_size - total_large_used_size) +
			(memory_stats.os_reserved_size - memory_stats.os_used_size);
		const double overhead_percentage = memory_stats.used_size > 0 ? 100.0 * (double)overhead_size / (double)memory_stats.used_size : 0.0;

		struct BucketInfo {
			uint64_t bucket_size;
			uint64_t used_size;
			uint64_t reserved_size;
			uint64_t used_count;
			uint64_t frame_count;
		};

		auto MemoryBucket = [&](const char* label, size_t used_size, size_t reserved_size, size_t num_buckets, const std::function<BucketInfo(size_t)>& buckets)
		{
			const auto usage = reserved_size > 0 ? 100.0 * (double)used_size / (double)reserved_size : 0.0;
			ImFormatString(buffer0, std::size(buffer0), "%s: %.1f %% (%s / %s)", label, float(usage), ImGuiEx::FormatMemory(buffer1, used_size), ImGuiEx::FormatMemory(buffer2, reserved_size));
			ImFormatString(buffer1, std::size(buffer1), "##TreeNode%s", label);
			const bool tree_open = BeginSection(buffer1, buffer0, false);
			if (!tree_open)
				ImGui::TreePush(buffer1);

			if (num_buckets > 0)
			{
				for (size_t a = 0; a < num_buckets; a++)
				{
					const auto bucket = buckets(a);

					if (IsPoppedOut() && tree_open && bucket.used_count > 0)
					{
						const auto bucket_usage = bucket.reserved_size > 0 ? 100.0 * (double)bucket.used_size / (double)bucket.reserved_size : 0.0;
						ImGui::Text("%s = %.1f %% (%s / %s) %u | %u", ImGuiEx::FormatMemory(buffer0, bucket.bucket_size), float(bucket_usage), ImGuiEx::FormatMemory(buffer1, bucket.used_size), ImGuiEx::FormatMemory(buffer2, bucket.reserved_size), unsigned(bucket.used_count), unsigned(bucket.frame_count));
					}
				}
			}

			if (tree_open)
				EndSection();
			else
				ImGui::TreePop();
		};

		MemoryBucket("Tiny", total_tiny_used_size, total_tiny_reserved_size, Memory::TinyCount, [&memory_stats](size_t i)
		{
			return BucketInfo{
				Memory::TinyIncrement * (i + 1),
				memory_stats.tiny_used_sizes[i].load(std::memory_order_relaxed),
				memory_stats.tiny_reserved_sizes[i].load(std::memory_order_relaxed),
				memory_stats.tiny_counts[i].load(std::memory_order_relaxed),
				memory_stats.last_frame_tiny_counts[i]
			};
		});

		MemoryBucket("Small", total_small_used_size, total_small_reserved_size, Memory::SmallCount, [&memory_stats](size_t i)
		{
			return BucketInfo{
				Memory::SmallIncrement * (i + 1),
				memory_stats.small_used_sizes[i].load(std::memory_order_relaxed),
				memory_stats.small_reserved_sizes[i].load(std::memory_order_relaxed),
				memory_stats.small_counts[i].load(std::memory_order_relaxed),
				memory_stats.last_frame_small_counts[i]
			};
		});

		MemoryBucket("Medium", total_medium_used_size, total_medium_reserved_size, Memory::MediumCount, [&memory_stats](size_t i)
		{
			return BucketInfo{
				Memory::MediumIncrement * (i + 1),
				memory_stats.medium_used_sizes[i].load(std::memory_order_relaxed),
				memory_stats.medium_reserved_sizes[i].load(std::memory_order_relaxed),
				memory_stats.medium_counts[i].load(std::memory_order_relaxed),
				memory_stats.last_frame_medium_counts[i]
			};
		});

		MemoryBucket("Large", total_large_used_size, total_large_reserved_size, Memory::LargeCount, [&memory_stats](size_t i)
		{
			return BucketInfo{
				uint64_t(1) << (Memory::LargeBegin_log2 + i),
				memory_stats.large_used_sizes[i].load(std::memory_order_relaxed),
				memory_stats.large_reserved_sizes[i].load(std::memory_order_relaxed),
				memory_stats.large_counts[i].load(std::memory_order_relaxed),
				memory_stats.last_frame_large_counts[i]
			};
		});

		ImGui::NewLine();

		const auto os_usage = memory_stats.os_reserved_size > 0 ? 100.0 * (double)memory_stats.os_used_size / (double)memory_stats.os_reserved_size : 0.0;
		ImGui::Text("OS: %.1f %% (%s / %s)", float(os_usage), ImGuiEx::FormatMemory(buffer1, memory_stats.os_used_size), ImGuiEx::FormatMemory(buffer2, memory_stats.os_reserved_size));

		ImGui::NewLine();

		const auto total_usage = memory_stats.total_size > 0 ? 100.0 * (double)memory_stats.used_size / (double)memory_stats.total_size : 0.0;
		ImGui::Text("Total: %.3f %% (%s / %s)", float(total_usage), ImGuiEx::FormatMemory(buffer1, memory_stats.used_size), ImGuiEx::FormatMemory(buffer2, memory_stats.total_size));
		ImGui::Text("Peak: %s", ImGuiEx::FormatMemory(buffer0, memory_stats.peak_size));
		ImGui::Text("Overhead: %.3f %% (%s)", float(overhead_percentage), ImGuiEx::FormatMemory(buffer0, overhead_size));

		ImGui::EndGroup();
	}

	void ProfileAllocatorPlugin::RenderStats(const Memory::Stats& memory_stats)
	{
		ImGui::BeginGroup();

		ImGui::NewLine();

		char buffer0[128] = { '\0' };
		char buffer1[128] = { '\0' };
		char buffer2[128] = { '\0' };

	#if defined(PS4)
		sce::Toolkit::NP::V2::Core::MemoryPoolStats memPoolStats;
		const auto ret = sce::Toolkit::NP::V2::Core::getMemoryPoolStats(memPoolStats);
		if (ret < 0)
			throw std::runtime_error("failed get memory pool stats");

		static const unsigned count = 9;
		std::array<std::string, count> names = { "npToolkit", "json", "webApi", "http", "ssl", "net", "matching", "matchinSsl", "inGameMsg" };
		std::array<size_t, count> current_sizes =
		{
			memPoolStats.npToolkitPoolStats.currentInuseSize,
			memPoolStats.jsonPoolStats.currentInuseSize,
			memPoolStats.webApiPoolStats.currentInuseSize,
			memPoolStats.httpPoolStats.currentInuseSize,
			memPoolStats.sslPoolStats.currentInuseSize,
			memPoolStats.netPoolStats.currentInuseSize,
			memPoolStats.matchingPoolStats.curMemUsage,
			memPoolStats.matchingSslPoolStats.curMemUsage,
			memPoolStats.inGameMsgPoolStats.currentInuseSize
		};
		std::array<size_t, count> peak_sizes =
		{
			memPoolStats.npToolkitPoolStats.maxInuseSize,
			memPoolStats.jsonPoolStats.maxInuseSize,
			memPoolStats.webApiPoolStats.maxInuseSize,
			memPoolStats.httpPoolStats.maxInuseSize,
			memPoolStats.sslPoolStats.maxInuseSize,
			memPoolStats.netPoolStats.maxInuseSize,
			memPoolStats.matchingPoolStats.maxMemUsage,
			memPoolStats.matchingSslPoolStats.maxMemUsage,
			memPoolStats.inGameMsgPoolStats.maxInuseSize
		};
		std::array<size_t, count> max_sizes =
		{
			memPoolStats.npToolkitPoolStats.poolSize,
			memPoolStats.jsonPoolStats.poolSize,
			memPoolStats.webApiPoolStats.poolSize,
			memPoolStats.httpPoolStats.poolSize,
			memPoolStats.sslPoolStats.poolSize,
			memPoolStats.netPoolStats.poolSize,
			memPoolStats.matchingPoolStats.totalMemSize,
			memPoolStats.matchingSslPoolStats.totalMemSize,
			memPoolStats.inGameMsgPoolStats.poolSize
		};
		std::array<float, count> usages;
		for (unsigned i = 0; i < count; ++i)
			usages[i] = max_sizes[i] > 0 ? float(100.0 * (double)current_sizes[i] / (double)max_sizes[i]) : 0.0f;

		if (BeginSection("System Pools"))
		{
			for (unsigned i = 0; i < count; ++i)
				ImGui::Text("%s: %.1f %% (%s / %s / %s)", names[i].c_str(), usages[i], ImGuiEx::FormatMemory(buffer0, current_sizes[i]), ImGuiEx::FormatMemory(buffer1, peak_sizes[i]), ImGuiEx::FormatMemory(buffer2, max_sizes[i]));

			EndSection();
		}
	#endif

		if (BeginSection("String Pools", false))
		{
			for (unsigned i = 0; i < Memory::Internal::BaseStringPool::get_pool_count(); ++i)
			{
				auto* pool = Memory::Internal::BaseStringPool::get_pools()[i];
				const double percentage = pool->get_total_size() > 0 ? pool->get_used_size() * 100.0 / pool->get_total_size() : 0.0;
				ImGui::Text("%s: %.1f %% (%s / %s) %u: %u | %u (%u)", pool->get_name(), float(percentage), ImGuiEx::FormatMemory(buffer0, pool->get_used_size()), ImGuiEx::FormatMemory(buffer1, pool->get_total_size()), unsigned(pool->get_referenced_count()), unsigned(pool->get_used_count()), unsigned(pool->get_frame_count()), unsigned(pool->get_bag_count()));
				pool->reset_frame_count();
			}

			EndSection();
		}

		if (BeginSection("Allocators", false))
		{
			for (auto& stats : Memory::BaseAllocator::GetAllocatorStats())
			{
				if (stats.alloc_count > 0)
				{
					const double percentage = stats.real_size > 0 ? stats.used_size * 100.0 / stats.real_size : 0.0;
					ImGui::Text("%s: %.1f %% (%s / %s  peak %s) %u", stats.name, float(percentage), ImGuiEx::FormatMemory(buffer0, stats.used_size), ImGuiEx::FormatMemory(buffer1, stats.real_size), ImGuiEx::FormatMemory(buffer2, stats.peak_size), stats.alloc_count);
				}
			}

			EndSection();
		}

	#if defined(COUNTER)
		if (BeginSection("Counters"))
		{
			const auto lines = Counters::Gather();
			for (auto& line : lines)
			{
				const auto s = WstringToString(line);
				ImGui::Text("%s", s.c_str());
			}

			EndSection();
		}
	#endif

		ImGui::EndGroup();
	}

	void ProfileAllocatorPlugin::RenderTraces(const Memory::Stats& memory_stats)
	{
		ImGui::BeginGroup();

		if (BeginSection("Traces"))
		{
			static int threshold = 20;
			static int count = 20;
			bool clear = false;
			bool snapshot = false;
			static bool sort_by_size = true;
			static bool frame_only = false;
			static bool snapshot_diff = false;
			{
				ImGui::PushItemWidth(240.0f);

				if (ImGui::Button("Clear", ImVec2(120, 0)))
					clear = true;
				ImGui::SameLine();
				if (ImGui::Button("Snapshot", ImVec2(120, 0)))
					snapshot = true;

				ImGui::SliderInt("Min Alloc Count", &threshold, 1, 100, "%d", ImGuiSliderFlags_AlwaysClamp);
				ImGui::SliderInt("Top Capture Count", &count, 1, 100, "%d", ImGuiSliderFlags_AlwaysClamp);

				static int sort_type = 0;
				ImGui::Combo("Sort", &sort_type, "Size\0Count\0");
				sort_by_size = sort_type == 0;

				static int display_type = 0;
				ImGui::Combo("Display", &display_type, "All\0Frame\0Snapshot\0");
				frame_only = display_type == 1;
				snapshot_diff = display_type == 2;

				ImGui::PopItemWidth();
			}

			const auto traces = Memory::GetTraces(threshold, count, clear, snapshot, sort_by_size, frame_only, snapshot_diff);
			for (auto& trace : traces)
				ImGui::Text("(%d, %s) %s", trace.alloc_count, Memory::ReadableSize(trace.total_size).c_str(), trace.name.c_str());

			EndSection();
		}

		ImGui::EndGroup();
	}

	void ProfileAllocatorPlugin::OnRender(float elapsed_time)
	{
		if (Begin(1000.0f))
		{
			const auto& memory_stats = Memory::GetStats();
	
			if (IsPoppedOut())
			{
				if (ImGui::BeginTabBar("##Tabs"))
				{
					if (ImGui::BeginTabItem("Stats"))
					{
						if (ImGui::BeginChild("##ScrollChild", ImVec2(0, 0), false, GetWindowFlags()))
						{
							RenderBuckets(memory_stats);
							RenderStats(memory_stats);
						}
			
						ImGui::EndChild();
						ImGui::EndTabItem();
					}
			
					if (ImGui::BeginTabItem("Categories"))
					{
						if (ImGui::BeginChild("##ScrollChild", ImVec2(0, 0), false, GetWindowFlags()))
							RenderCategories(memory_stats);

						ImGui::EndChild();
						ImGui::EndTabItem();
					}
			
					if (ImGui::BeginTabItem("Groups"))
					{
						if (ImGui::BeginChild("##ScrollChild", ImVec2(0, 0), false, GetWindowFlags()))
							RenderGroups(memory_stats);

						ImGui::EndChild();
						ImGui::EndTabItem();
					}
			
					if (ImGui::BeginTabItem("Tags"))
					{
						if (ImGui::BeginChild("##ScrollChild", ImVec2(0, 0), false, GetWindowFlags()))
							RenderTags(memory_stats);
			
						ImGui::EndChild();
						ImGui::EndTabItem();
					}
			
					if (ImGui::BeginTabItem("Traces"))
					{
						if (ImGui::BeginChild("##ScrollChild", ImVec2(0, 0), false, GetWindowFlags()))
							RenderTraces(memory_stats);
			
						ImGui::EndChild();
						ImGui::EndTabItem();
					}
			
					ImGui::EndTabBar();
				}
			}
			else if (ImGui::BeginTable("##TableWrapper", 2, ImGuiTableFlags_NoBordersInBody | ImGuiTableFlags_NoSavedSettings | ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoPadOuterX))
			{
				ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthStretch, 1.0f);
				ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthStretch, 1.2f);
				ImGui::TableNextRow();

				if (ImGui::TableSetColumnIndex(0))
				{
					RenderBuckets(memory_stats);
					RenderStats(memory_stats);
					RenderTraces(memory_stats);
				}

				if (ImGui::TableSetColumnIndex(1))
				{
					RenderCategories(memory_stats);

					ImGui::NewLine();

					RenderGroups(memory_stats);
				}

				ImGui::EndTable();
			}

			End();
		}
	}
#endif
}