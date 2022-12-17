#include "LoggingPlugin.h"
#include "Common/Utility/Logger/Logger.h"
#include "Common/Utility/HighResTimer.h"
#include "Common/Utility/StringManipulation.h"
#include "Visual/GUI2/imgui/imgui.h"
#include "Visual/GUI2/Icons.h"
#include "Visual/Device/Constants.h"

#include "Include/magic_enum/magic_enum.hpp"

namespace Engine::Plugins
{
#if DEBUG_GUI_ENABLED
	namespace
	{
		struct TextBuffer
		{
			Memory::Vector<char, Memory::Tag::Unknown> buffer;

			TextBuffer() { Clear(); }
			char operator[](size_t i) const { return buffer[i]; }
			char& operator[](size_t i) { return buffer[i]; }
			const char* c_str() const { return buffer.data(); }
			size_t size() const { return buffer.size() - 1; }

			void Append(const char* str, const char* str_end = nullptr)
			{
				const size_t len = str_end != nullptr ? size_t(str_end - str) : size_t(strlen(str));
				buffer.resize(buffer.size() + len);
				memcpy(&buffer[buffer.size() - (len + 1)], str, len * sizeof(char));
				buffer[buffer.size() - 1] = '\0';
			}

			void Clear()
			{
				buffer.clear();
				buffer.push_back('\0');
			}
		};

		struct LogEntry
		{
			std::string type_id;
			std::string time_text;
			Log::LogLevels::Level level;
			LSS::SubsystemType_t subsystem;
			long long subject_id;
			time_t time;
			unsigned proc_id;
			unsigned location_id;
			uint64_t message_id;
			uint64_t log_time;
			size_t skip_pos;
			size_t start_pos;
			size_t end_pos;
		};

		struct Line
		{
			size_t metadata = 0;
			size_t position = 0;
			size_t skip = 0;
			bool pass_filter = true;
		};

		struct LogTypeFilter
		{
			bool enabled[magic_enum::enum_count<Log::LogLevels::Level>()];

			LogTypeFilter()
			{
				for (size_t a = 0; a < std::size(enabled); a++)
					enabled[a] = true;
			}

			bool IsActive() const
			{
				for (size_t a = 0; a < std::size(enabled); a++)
				{
					if (!enabled[a])
						return true;
				}

				return false;
			}

			bool PassFilter(Log::LogLevels::Level level) const
			{
				return enabled[size_t(level)];
			}

			bool Draw(const char* label, float width = 0.0f)
			{
				ImGui::PushID(label);
				if (width > 0.0f)
					ImGui::SetNextItemWidth(width);
				else
					ImGui::SetNextItemWidth(ImGui::CalcTextSize(label).x + (2 * ImGui::GetStyle().FramePadding.x) + ImGui::GetStyle().ItemInnerSpacing.x + ImGui::GetFrameHeight());

				bool res = false;
				char buffer[256] = { '0' };
				if (ImGui::BeginCombo("##LogFilter", label))
				{
					for (const auto& v : magic_enum::enum_entries<Log::LogLevels::Level>())
					{
						ImFormatString(buffer, std::size(buffer), "%.*s", unsigned(v.second.length()), v.second.data());
						if (ImGui::Checkbox(buffer, &enabled[size_t(v.first)]))
							res = true;
					}

					ImGui::EndCombo();
				}

				ImGui::PopID();
				return res;
			}
		};

		std::atomic<bool> has_crit_message = false;
		bool auto_scroll = true;
		bool show_filter = false;
		bool focus_filter = false;
		thread_local bool skip_logging = false;

		Memory::Mutex mutex;
		bool has_new_message = false;
		uint64_t next_message_id = 1;
		float source_width = 0.0f;
		float time_width = 0.0f;
		float last_log_space = 0.0f;
		float last_log_font_size = 0.0f;
		size_t source_length = 0;
		size_t time_length = 0;
		size_t passed_lines = 0;
		size_t last_filtered_line = 0;
		size_t last_message_count = 0;
		size_t last_visible_count = 0;
		size_t last_line_entry = 0;
		size_t last_line_skip = 0;
		TextBuffer text_buffer;
		ImGuiTextFilter search_filter;
		LogTypeFilter type_filter;
		Memory::Vector<LogEntry, Memory::Tag::Unknown> entries;
		Memory::Vector<Line, Memory::Tag::Unknown> lines;
		Memory::FlatMap<unsigned, std::string, Memory::Tag::Unknown> ignored;

		class LogWriter : public Log::LogWriter
		{
		private:
			Log::LogWriter* current_parent = nullptr;
			bool enabled = false;

		public:
			void Inject()
			{
				if (current_parent == nullptr)
				{
					current_parent = Log::GetGlobalLogWriter();

					do 
					{
						if (current_parent)
						{
							type_id = current_parent->type_id;
							proc_id = current_parent->proc_id;
						}
					} while (!Log::SetGlobalLogWriter(this, current_parent));
				}
			}

			void Write(const unsigned line_number, time_t time, const unsigned timer, Log::LogLevels::Level level, const std::string& type_id, const unsigned proc_id, const LSS::SubsystemType_t subsystem, const long long subject_id, const std::wstring& message) override
			{
				if (current_parent)
					current_parent->Write(line_number, time, timer, level, subsystem, subject_id, message);

				if (!enabled || skip_logging)
					return;

				if (level == Log::LogLevels::Critical)
					has_crit_message.store(true, std::memory_order_release);

				std::stringstream stream;

				// type and proc id
				stream << "[" << type_id.c_str() << ' ' << proc_id;

				// Subsystem
				if (subsystem != LSS::None)
				{
					stream << ' ' << LSS::SubSystemNames[subsystem];

					// Subject id
					stream << ' ' << subject_id;
				}

				stream << ']' << ' ';

				// Log Level
				stream << '[' << Log::LogLevels::LongLevelNames[level] << ']' << ' ';

				size_t skip_pos = stream.str().length();

				// Message
				stream << TrimString(WstringToString(message));

				Memory::Lock lock(mutex);
				has_new_message = true;
				size_t old_size = text_buffer.size();
				text_buffer.Append(stream.str().c_str());

				const auto meta_pos = entries.size();
				auto& metadata = entries.emplace_back();
				metadata.level = level;
				metadata.proc_id = proc_id;
				metadata.time = time;
				metadata.type_id = type_id;
				metadata.message_id = next_message_id++;
				metadata.subject_id = subject_id;
				metadata.subsystem = subsystem;
				metadata.location_id = line_number;
				metadata.log_time = HighResTimer::Get().GetTime();
				metadata.time_text = Log::DateTimeString(time);
				metadata.skip_pos = skip_pos;
				metadata.start_pos = old_size;
				metadata.end_pos = text_buffer.size();
			}

			void OnEnable()
			{
				enabled = true;
			}

			void OnDisable()
			{
				enabled = false;
			}
		};

		LogWriter log_writer;

		bool IsSpace(char c)
		{
			constexpr uint32_t MASK = (1 << (' ' - 1)) | (1 << ('\f' - 1)) | (1 << ('\r' - 1)) | (1 << ('\t' - 1)) | (1 << ('\v' - 1));
			return c > 0 && c <= 32 && ((uint32_t(1) << (c - 1)) & MASK) != 0;
		}

		void UpdateLines(float max_width)
		{
			if (last_line_entry == 0)
			{
				lines.clear();
				last_filtered_line = 0;
				passed_lines = 0;
				last_line_skip = 0;
			}

			for (; last_line_entry < entries.size(); last_line_entry++)
			{
				const auto& metadata = entries[last_line_entry];

				size_t skip_pos = metadata.skip_pos + last_line_skip;
				size_t pos = metadata.start_pos;
				while (pos + skip_pos < metadata.end_pos)
				{
					const auto str_start = text_buffer.c_str() + pos + skip_pos;
					const auto str_end = text_buffer.c_str() + metadata.end_pos;
					assert(str_start <= str_end);

					auto start = str_start;
					auto end = str_start;
					float width = 0.0f;

					while (start != str_end)
					{
						while (end != str_end && IsSpace(*end))
							end++;

						while (end != str_end && !IsSpace(*end) && *end != '\n')
							end++;

						const auto word_size = ImGui::CalcTextSize(start, end).x;
						if (width + word_size < max_width)
						{
							width += word_size;
							start = end;

							if (start != str_end && *start == '\n')
								break;
						}
						else
							break;
					}

					if (start == str_start) // Unlikely case: Single word is larger than line width
					{
						end = str_start;
						while (start != str_end)
						{
							while (end != str_end && IsSpace(*end))
								end++;

							if (end != str_end && *end != '\n')
								end++;

							const auto word_size = ImGui::CalcTextSize(start, end).x;
							if (width + word_size < max_width)
							{
								width += word_size;
								start = end;

								if (start != str_end && *start == '\n')
									break;
							}
							else
								break;
						}
					}

					if (start == str_start && start != str_end && *start != '\n') // Unlikely case: Always have at least one character on each line, to avoid infinite loop
						start++;

					pos = size_t(start - text_buffer.c_str());

					auto& line = lines.emplace_back();
					line.metadata = last_line_entry;
					line.pass_filter = true;
					line.position = pos;
					line.skip = skip_pos;

					assert(lines.size() == 1 || lines[lines.size() - 2].position + skip_pos <= pos);

					skip_pos = 0;
					if (pos < metadata.end_pos && *(text_buffer.c_str() + pos) == '\n')
						skip_pos++;

					while (pos + skip_pos < metadata.end_pos && IsSpace(*(text_buffer.c_str() + pos + skip_pos)))
						skip_pos++;
				}

				last_line_skip = skip_pos;
			}
		}

		bool EntryPassesFilter(const LogEntry& entry)
		{
			if (type_filter.IsActive() && !type_filter.PassFilter(entry.level))
				return false;

			if (!ignored.empty() && ignored.find(entry.location_id) != ignored.end())
				return false;

			if (search_filter.IsActive() && !search_filter.PassFilter(text_buffer.c_str() + entry.start_pos, text_buffer.c_str() + entry.end_pos))
				return false;

			return true;
		}

		void CopyMessage(const LogEntry& metadata, size_t time_space = 0, bool open_log = true)
		{
			if(open_log)
				ImGui::LogToClipboard();

			time_space = std::max(time_space, metadata.time_text.length());
			ImGui::LogText("%s%*s %.*s", metadata.time_text.c_str(), unsigned(time_space - metadata.time_text.length()), "", unsigned(metadata.skip_pos), text_buffer.c_str() + metadata.start_pos);

			for (size_t start = metadata.start_pos + metadata.skip_pos, line = 0; start < metadata.end_pos; line++)
			{
				while (start < metadata.end_pos && IsSpace(*(text_buffer.c_str() + start)))
					start++;

				if (start == metadata.end_pos)
					break;

				if(line > 0)
					ImGui::LogText("%*s %*s", unsigned(time_space), "", unsigned(metadata.skip_pos), "");

				size_t end = start;
				while (end < metadata.end_pos && *(text_buffer.c_str() + end) != '\n')
					end++;

				ImGui::LogText("%.*s" IM_NEWLINE, unsigned(end - start), text_buffer.c_str() + start);
				start = end;
				if (start < metadata.end_pos)
					start++;
			}

			if (open_log)
				ImGui::LogFinish();
		}

		void CopyLog()
		{
			Memory::Vector<size_t, Memory::Tag::Unknown> copy_messages;
			for (size_t a = 0; a < entries.size(); a++)
				if (EntryPassesFilter(entries[a]))
					copy_messages.push_back(a);

			size_t time_space = 0;
			for (const auto& a : copy_messages)
				time_space = std::max(time_space, entries[a].time_text.length());

			ImGui::LogToClipboard();

			for (const auto& a : copy_messages)
				CopyMessage(entries[a], time_space, false);

			ImGui::LogFinish();
		}
	}

	LoggingPlugin::LoggingPlugin() : Plugin(Feature_Visible | Feature_Settings) 
	{
		log_writer.Inject();

		AddAction("Toggle", VK_F12, Input::Modifier_Ctrl, [this]() { SetVisible(!IsVisible()); });
	}

	bool LoggingPlugin::IsBlinking() const { return has_crit_message.load(std::memory_order_relaxed); }

	void LoggingPlugin::OnRender(float elapsed_time)
	{
		if (!IsVisible())
			return;

		auto window_flags = click_through && in_main_viewport ? ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoScrollbar : ImGuiWindowFlags_None;

		in_main_viewport = false;
		bool visible = true;
		ImGui::SetNextWindowSizeConstraints(ImVec2(600.0f, 5 * ImGui::GetFrameHeightWithSpacing()), ImVec2(FLT_MAX, FLT_MAX));
		if (ImGui::Begin("Log", &visible, window_flags))
		{
			in_main_viewport = ImGui::GetCurrentWindow()->Viewport == ImGui::GetMainViewport();
			const auto& style = ImGui::GetStyle();

			skip_logging = true; // Avoid recursive locking of mutex if we happen to output anything to the log while rendering the log itself

			Memory::Lock lock(mutex);
			has_crit_message.store(false, std::memory_order_release);
			char buffer[256] = { '\0' };

			auto Clear = []()
			{
				lines.clear();
				entries.clear();
				text_buffer.Clear();
				passed_lines = 0;
				last_filtered_line = 0;
				last_message_count = 0;
				source_width = 0.0f;
				time_width = 0.0f;
				source_length = 0;
				time_length = 0;
				last_line_entry = 0;
				last_line_skip = 0;
			};

			if (ImGui::Button(IMGUI_ICON_EXPORT " Copy"))
				CopyLog();

			ImGui::SameLine();
			if (ImGui::Button(IMGUI_ICON_TRASH " Clear"))
				Clear();

			ImGui::SameLine();
			if (type_filter.Draw("Filter"))
			{
				last_filtered_line = 0;
				passed_lines = 0;
			}

			ImGui::SameLine();
			ImGui::SetNextItemWidth(ImGui::CalcTextSize("Ignored").x + (2 * ImGui::GetStyle().FramePadding.x) + ImGui::GetStyle().ItemInnerSpacing.x + ImGui::GetFrameHeight());
			ImGui::SetNextWindowSizeConstraints(ImVec2(15.0f * ImGui::GetFontSize(), 0.0f), ImVec2(FLT_MAX, FLT_MAX));
			ImGuiEx::PushDisable(ignored.empty());
			if (ImGui::BeginCombo("##IgnoredList", "Ignored"))
			{
				for (auto a = ignored.begin(); a != ignored.end();)
				{
					ImGuiEx::FormatStringEllipsis(ImGui::GetContentRegionAvail().x, true, buffer, std::size(buffer), IMGUI_ICON_ABORT " %s", a->second.c_str());
					ImGui::PushID(int(a->first));
					const bool clicked = ImGui::Selectable(buffer);
					if (ImGui::IsItemHovered())
					{
						const auto tooltip_length = std::min(ImGui::CalcTextSize(a->second.c_str()).x + (2 * ImGui::GetStyle().WindowPadding.x), 35.0f * ImGui::GetFontSize());

						ImGui::SetNextWindowSize(ImVec2(tooltip_length, 0.0f), ImGuiCond_Always);
						ImGui::BeginTooltip();
						ImGuiEx::FormatStringEllipsis(ImGui::GetContentRegionAvail().x, true, buffer, std::size(buffer), "%s", a->second.c_str());
						ImGui::Text("%s", buffer);
						ImGui::EndTooltip();
					}

					ImGui::PopID();
					if (clicked)
					{
						a = ignored.erase(a);
						last_filtered_line = 0;
						passed_lines = 0;
					}
					else
						++a;
				}

				ImGui::EndCombo();
			}

			ImGuiEx::PopDisable();
			ImGui::SameLine();
			ImGui::Checkbox("Auto Scroll", &auto_scroll);

			ImGui::SameLine();
			const float search_clear_width = std::max(ImGui::CalcTextSize(IMGUI_ICON_ABORT).x, ImGui::CalcTextSize(IMGUI_ICON_SEARCH).x) + (2 * style.FramePadding.x);
			const auto remaining_space = ImGui::GetContentRegionAvail().x;

			if (show_filter)
			{
				if (focus_filter)
					ImGui::SetKeyboardFocusHere();

				focus_filter = false;
				if (search_filter.Draw("##SearchFilter", remaining_space - (style.ItemSpacing.x + search_clear_width)))
				{
					last_filtered_line = 0;
					passed_lines = 0;
				}

				if (ImGui::IsItemDeactivated() && !search_filter.IsActive())
					show_filter = false;

				ImGui::SameLine(0.0f, style.ItemSpacing.x);
				if (ImGui::Button(IMGUI_ICON_ABORT "##CloseFilter"))
				{
					search_filter.Clear();
					last_filtered_line = 0;
					passed_lines = 0;
					show_filter = false;
				}
			}
			else
			{
				ImGui::SetCursorPosX(ImGui::GetCursorPosX() + remaining_space - search_clear_width);
				if (ImGui::Button(IMGUI_ICON_SEARCH "##OpenFilter"))
				{
					show_filter = true;
					focus_filter = true;
				}
			}

			ImGui::Separator();
			if (ImGui::BeginChild("##ScrollArea", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar | window_flags))
			{
				if (ImGui::BeginPopupContextWindow("##WindowPopup"))
				{
					if (ImGui::Selectable(IMGUI_ICON_EXPORT " Copy All"))
						CopyLog();

					if (ImGui::Selectable(IMGUI_ICON_TRASH " Clear"))
						Clear();

					ImGui::EndPopup();
				}

				while (last_message_count < entries.size())
				{
					const auto& metadata = entries[last_message_count++];
					source_width = std::max(source_width, ImGui::CalcTextSize(metadata.type_id.c_str()).x);
					source_length = std::max(source_length, metadata.type_id.length());
					time_width = std::max(time_width, ImGui::CalcTextSize(metadata.time_text.c_str()).x);
					time_length = std::max(time_length, metadata.time_text.length());
				}

				const auto item_spacing = style.ItemSpacing;
				const auto message_space = ImGui::GetContentRegionAvail().x - (time_width + (2.0f * item_spacing.x) + (ImGui::GetCurrentWindow()->ScrollbarY ? style.ScrollbarSize : 0.0f));
				if (std::abs(message_space - last_log_space) > 0.5f || std::abs(last_log_font_size - ImGui::GetFontSize()) > 0.5f)
				{
					source_width = 0.0f;
					time_width = 0.0f;
					for (size_t a = 0; a < entries.size(); a++)
					{
						const auto& metadata = entries[a];
						source_width = std::max(source_width, ImGui::CalcTextSize(metadata.type_id.c_str()).x);
						time_width = std::max(time_width, ImGui::CalcTextSize(metadata.time_text.c_str()).x);
					}

					last_log_space = ImGui::GetContentRegionAvail().x - (time_width + (2.0f * item_spacing.x) + (ImGui::GetCurrentWindow()->ScrollbarY ? style.ScrollbarSize : 0.0f));
					last_line_entry = 0;
					last_line_skip = 0;
					last_log_font_size = ImGui::GetFontSize();
				}

				UpdateLines(last_log_space);

				if (search_filter.IsActive() || type_filter.IsActive() || !ignored.empty())
				{
					while (last_filtered_line < lines.size())
					{
						const auto line_id = last_filtered_line;
						size_t first_line = line_id;
						while (first_line > 0 && lines[first_line - 1].metadata == lines[first_line].metadata)
							first_line--;

						size_t line_count = 1;
						while (first_line + line_count < lines.size() && lines[first_line + line_count].metadata == lines[first_line].metadata)
							line_count++;

						const bool passed = EntryPassesFilter(entries[lines[first_line].metadata]);
						for (size_t a = 0; a < line_count; a++)
							lines[first_line + a].pass_filter = passed;

						if (passed)
							passed_lines += line_count;

						last_filtered_line += line_count;
					}
				}

				const auto visible_messages = search_filter.IsActive() || type_filter.IsActive() || !ignored.empty() ? passed_lines : lines.size();
				const auto new_message = has_new_message && visible_messages > last_visible_count;
				has_new_message = false;

				ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));

				ImGuiListClipper clipper;
				clipper.Begin(int(visible_messages));

				const float lineX = ImGui::GetCursorPosX();

				size_t line_id = 0;
				size_t pass_count = 0;
				while (clipper.Step())
				{
					for (int a = clipper.DisplayStart; a < clipper.DisplayEnd; a++)
					{
						if (search_filter.IsActive() || type_filter.IsActive() || !ignored.empty())
						{
							for (; line_id < lines.size(); line_id++)
							{
								if (lines[line_id].pass_filter)
								{
									pass_count++;
									if (pass_count == size_t(a) + 1)
										break;
								}
							}
						}
						else
							line_id = size_t(a);

						if (line_id >= lines.size())
							break;

						ImGui::PushID(int(line_id));
						const size_t line_start = line_id == 0 ? 0 : lines[line_id - 1].position;
						const size_t line_end = lines[line_id].position;
						const size_t line_skip = lines[line_id].skip;
						const auto& metadata = entries[lines[line_id].metadata];

						size_t first_line = line_id;
						while (first_line > 0 && lines[first_line - 1].metadata == lines[line_id].metadata)
							first_line--;

						ImGui::BeginGroup();

						switch (metadata.level)
						{
							case Log::LogLevels::Critical:
								ImGui::PushStyleColor(ImGuiCol_Text, ImColor(1.0f, 0.0f, 0.0f).Value);
								break;
							case Log::LogLevels::Warning:
								ImGui::PushStyleColor(ImGuiCol_Text, ImColor(1.0f, 1.0f, 0.0f).Value);
								break;
							case Log::LogLevels::Debug:
								ImGui::PushStyleColor(ImGuiCol_Text, ImColor(0.6f, 0.6f, 0.6f).Value);
								break;
							default:
								ImGui::PushStyleColor(ImGuiCol_Text, ImColor(1.0f, 1.0f, 1.0f).Value);
								break;
						}

						ImGui::AlignTextToFramePadding();
						const bool is_first_line = line_id == 0 || lines[line_id - 1].metadata != lines[line_id].metadata;

						if (is_first_line)
						{
							ImGui::TextUnformatted(metadata.time_text.c_str());
							ImGui::SameLine(0.0f, 0.0f);
						}

						ImGui::SetCursorPosX(lineX + time_width + item_spacing.x);
						ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
						ImGui::SameLine(0.0f, item_spacing.x);

						ImGui::TextUnformatted(text_buffer.c_str() + line_start + line_skip, text_buffer.c_str() + line_end);
						ImGui::PopStyleColor();

						ImGui::EndGroup();

						if (ImGui::BeginPopupContextItem("##Popup"))
						{
							ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, item_spacing);

							if (ImGui::Selectable(IMGUI_ICON_EXPORT " Copy Message"))
								CopyMessage(entries[lines[first_line].metadata]);

							if (ImGui::Selectable(IMGUI_ICON_HIDDEN " Ignore Message"))
							{
								const auto ls = (first_line == 0 ? 0 : lines[first_line - 1].position) + lines[first_line].skip;
								const auto le = lines[first_line].position;
								ignored[metadata.location_id] = std::string(text_buffer.c_str() + ls, le - ls);
								last_filtered_line = 0;
								passed_lines = 0;
							}

							if (ImGui::IsItemHovered())
							{
								ImGui::BeginTooltip();
								ImGui::PushTextWrapPos(35.0f * ImGui::GetFontSize());
								ImGui::Text(IMGUI_ICON_HIDDEN " Ignore Message");
								ImGui::Separator();
								ImGui::TextWrapped("Hides this message, as well as all other messages from the same source code location");
								ImGui::PopTextWrapPos();
								ImGui::EndTooltip();
							}

							ImGui::PopStyleVar();
							ImGui::EndPopup();
						}

						if (ImGui::IsItemHovered())
						{
							ImGui::BeginTooltip();
							ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, item_spacing);

							ImGui::Text("Message %u", unsigned(metadata.message_id));
							ImGui::Separator();

							ImGui::Text("Source: %s", metadata.type_id.c_str());

							if (metadata.subject_id != LSS::None)
								ImGui::Text("%s %lld", LSS::SubSystemNames[metadata.subject_id], metadata.subject_id);

							const auto type_name = magic_enum::enum_name(metadata.level);
							ImGui::Text("Type: %.*s", unsigned(type_name.length()), type_name.data());
							ImGui::Text("From %s", Log::DateTimeString(metadata.time).c_str());
							const auto age = HighResTimer::Get().GetTime() - metadata.log_time;
							if (age < 1000)
								ImGui::Text("%u ms ago", unsigned(age));
							else if (age < 60000)
								ImGui::Text("%.1f sec ago", float(double(age) / 1000));
							else if (age < 3600000)
								ImGui::Text("%.1f min ago", float(double(age) / 60000));
							else
								ImGui::Text("%.1fh ago", float(double(age) / 3600000));

							ImGui::PopStyleVar();
							ImGui::EndTooltip();
						}

						ImGui::PopID();
						line_id++;
					}
				}

				clipper.End();

				if ((auto_scroll && new_message) || ImGui::IsWindowAppearing())
					ImGui::SetScrollHereY(1.0f);

				last_visible_count = visible_messages;
				ImGui::PopStyleVar();
			}

			ImGui::EndChild();

			skip_logging = false;
		}

		ImGui::End();

		if (!visible)
			SetVisible(false);
	}

	void LoggingPlugin::OnRenderSettings(float elapsed_time)
	{
		ImGui::MenuItem("Click Through", nullptr, &click_through);
	}

	void LoggingPlugin::OnLoadSettings(const rapidjson::Value& settings)
	{
		if (const auto f = settings.FindMember("click_through"); f != settings.MemberEnd() && f->value.IsBool())
			click_through = f->value.GetBool();
	}

	void LoggingPlugin::OnSaveSettings(rapidjson::Value& settings, rapidjson::Document::AllocatorType& allocator)
	{
		settings.AddMember("click_through", click_through, allocator);
	}

	void LoggingPlugin::OnEnabled()
	{
		log_writer.OnEnable();
	}

	void LoggingPlugin::OnDisabled()
	{
		log_writer.OnDisable();
	}

	void LoggingPlugin::RenderNotification()
	{
		ImGuiEx::PushDisable(!IsEnabled());

		double t = 0;
		const bool blink = std::modf(ImGui::GetTime(), &t) < 0.5;
		ImGui::PushStyleColor(ImGuiCol_Text, IsEnabled() && IsBlinking() && blink ? ImU32(ImColor(1.0f, 1.0f, 0.0f)) : ImGui::GetColorU32(ImGuiCol_Text));

		bool visible = IsVisible();
		if (ImGui::Checkbox("Show Log", &visible))
			SetVisible(visible);

		ImGui::PopStyleColor();
		ImGuiEx::PopDisable();
	}
#endif
}
