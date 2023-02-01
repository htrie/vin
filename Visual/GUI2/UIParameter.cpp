#include <iomanip>

#include "Common/Utility/StringManipulation.h"

#include "Visual/GUI2/imgui/imgui.h"
#include "Visual/GUI2/imgui/misc/cpp/imgui_stdlib.h"
#include "Visual/GUI2/CurveControl.h"
#include "Visual/GUI2/Icons.h"
#include "Visual/Renderer/GlobalSamplersReader.h"
#include "Visual/Utility/CommonDialogs.h"
#include "Visual/Utility/DXHelperFunctions.h"
#include "Visual/Device/Texture.h"
#include "Visual/Device/Device.h"
#include "Visual/Device/State.h"

#include "AssetViewer/Utility/IMGUI/Misc.h"

#include "UIParameter.h"

namespace Device
{
	namespace GUI
	{
		namespace
		{
			struct UnitInfo
			{
				std::string_view name;
				std::string_view symbol;
				size_t group;
				float scale = 1.0f;
				float power = 1.0f;
				float period = -1.0f;
				bool prefer_default = false;

				constexpr UnitInfo(const std::string_view& name, const std::string_view& symbol, int group, float scale, float power, float period, bool prefer_default = false)
					: name(name)
					, symbol(symbol)
					, group(group)
					, scale(scale)
					, power(power)
					, period(period)
					, prefer_default(prefer_default)
				{}
			};

			struct LinearUnit : public UnitInfo
			{
				constexpr LinearUnit(const std::string_view& name, const std::string_view& symbol, int group, float scale, bool prefer_default = false) : UnitInfo(name, symbol, group, scale, 1.0f, -1.0f, prefer_default) {}
			};

			struct AngularUnit : public UnitInfo
			{
				constexpr AngularUnit(const std::string_view& name, const std::string_view& symbol, int group, float scale, float period, bool prefer_default = false) : UnitInfo(name, symbol, group, scale, 1.0f, period, prefer_default) {}
			};

			struct ExponentialUnit : public UnitInfo
			{
				constexpr ExponentialUnit(const std::string_view& name, const std::string_view& symbol, int group, float power, bool prefer_default = false) : UnitInfo(name, symbol, group, 1.0f, power, -1.0f, prefer_default) {}
			};

			constexpr UnitInfo Units[] = {
				LinearUnit("None",						"",				0, 1.0f										),

				LinearUnit("Milliseconds",				"ms",			1, 0.001f									),
				LinearUnit("Seconds",					"sec",			1, 1.0f										),
				LinearUnit("Minutes",					"min",			1, 60.0f									),
				LinearUnit("Hours",						"h",			1, 3600.0f									),

				LinearUnit("Millimeters",				"mm",			2, 0.1f										),
				LinearUnit("Centimeters",				"cm",			2, 1.0f										),
				LinearUnit("Meters",					"m",			2, 100.0f									),
				LinearUnit("Inches",					"in",			2, 2.54f									),
				LinearUnit("Feet",						"ft",			2, 30.48f									),
				LinearUnit("Yards",						"yd",			2, 91.44f									),
																						
				AngularUnit("Degrees",					"\xC2\xB0",		3, IM_PI / 180.0f,	360.0f,			true	),
				AngularUnit("Radians",					"rad",			3, 1.0f,			2.0f * IM_PI			),
																						
				LinearUnit("Milligramms",				"mg",			4, 0.001f									),
				LinearUnit("Gramms",					"g",			4, 1.0f										),
				LinearUnit("Kilogramms",				"kg",			4, 1000.0f									),
					
				LinearUnit("Square Centimeters",		"cm\xC2\xB2",	5, 1.0f										),
				LinearUnit("Square Meters",				"m\xC2\xB2",	5, 10000.0f									),

				LinearUnit("Cubic Centimeters",			"cm\xC2\xB3",	6, 1.0f										),
				LinearUnit("Cubic Meters",				"m\xC2\xB3",	6, 1000000.0f								),
				LinearUnit("Litres",					"l",			6, 1000.0f									),
				LinearUnit("Gallons",					"gal",			6, 3785.41f									),
																						
				LinearUnit("Milligramms per cm\xC2\xB3","mg/c\xC2\xB3",	7, 1.0f										),
				LinearUnit("Gramms per cm\xC2\xB3",		"g/c\xC2\xB3",	7, 1000.0f									),
				LinearUnit("Gramms per Litre",			"g/l",			7, 1.0f										),
				LinearUnit("Kilogramms per Litre",		"kg/l",			7, 1000.0f									),
				LinearUnit("Kilogramms per m\xC2\xB3",	"kg/m\xC2\xB3",	7, 1000000.0f								),

				LinearUnit("Millinewton",				"mN",			8, 1.0f										),
				LinearUnit("Centinewton",				"cN",			8, 10.0f									),
				LinearUnit("Newton",					"N",			8, 1000.0f									),

				LinearUnit("Millinewton-Second",		"mNs",			9, 0.001f									),
				LinearUnit("Centinewton-Second",		"cNs",			9, 0.01f									),
				LinearUnit("Newton-Second",				"Ns",			9, 1.0f										),
				LinearUnit("Killonewton-Second",		"kNs",			9, 1000.0f									),

				LinearUnit("Millimeters per Second",	"mm/s",			10, 0.1f									),
				LinearUnit("Centimeters per Second",	"cm/s",			10, 1.0f									),
				LinearUnit("Meters per Second",			"m/s",			10, 100.0f									),
				LinearUnit("Inches per Second",			"in/s",			10, 2.54f									),
				LinearUnit("Feet per Second",			"ft/s",			10, 30.48f									),
				LinearUnit("Yards per Second",			"yd/s",			10, 91.44f									),
																						
				ExponentialUnit("RGB",					"",				11, 1.0f									),
				ExponentialUnit("sRGB",					"",				11, 2.2f									),

				LinearUnit("Scalar",					"",				12, 1.0f									),
				LinearUnit("Percentage",				"%",			12, 0.01f,							true	),
			};

			constexpr std::string_view UnitGroupNames[] = {
				"None",
				"Time",
				"Distance",
				"Angle",
				"Weight",
				"Area",
				"Volume",
				"Density",
				"Force",
				"Impulse",
				"Velocity",
				"Colour",
				"Scalar",
			};

			constexpr size_t CustomUnit = std::size(Units);

			constexpr size_t CalculateNumUnitGroups()
			{
				size_t r = 0;
				for (const auto& a : Units)
					r = std::max(r, a.group + 1);

				return r;
			}

			constexpr size_t NumUnitGroups = CalculateNumUnitGroups();

			struct UnitGroupInfo
			{
				std::string_view name = "";
				size_t count = 0;
			};

			constexpr std::array<UnitGroupInfo, NumUnitGroups> CalculateUnitGroups()
			{
				std::array<UnitGroupInfo, NumUnitGroups> res = {};

				size_t p = 0;
				for (size_t a = 0; a < NumUnitGroups; a++)
				{
					res[a].name = a < std::size(UnitGroupNames) ? UnitGroupNames[a] : "Unnamed Group";
					res[a].count = 0;
				}

				for (const auto& a : Units)
					res[a.group].count++;

				return res;
			}

			constexpr auto UnitGroups = CalculateUnitGroups();
			constexpr size_t NoUnitGroup = NumUnitGroups;

			// We are saving icons by index, so don't change the order of icons in this list!
			constexpr std::string_view Icons[] = {
				IMGUI_ICON_FILE,
				IMGUI_ICON_FILE_ADD,
				IMGUI_ICON_FOLDER,
				IMGUI_ICON_FOLDER_ADD,
				IMGUI_ICON_FOLDER_REMOVE,
				IMGUI_ICON_FOLDER_OPEN,
				IMGUI_ICON_DRIVE,
				IMGUI_ICON_REDO,
				IMGUI_ICON_UNDO,
				IMGUI_ICON_RANDOM,
				IMGUI_ICON_LOCK,
				IMGUI_ICON_UNLOCK,
				IMGUI_ICON_BOOKMARK,
				IMGUI_ICON_EXPORT,
				IMGUI_ICON_IMPORT,
				IMGUI_ICON_PLAY,
				IMGUI_ICON_PAUSE,
				IMGUI_ICON_STOP,
				IMGUI_ICON_SKIP_FORWARD,
				IMGUI_ICON_FORWARD,
				IMGUI_ICON_SKIP_BACKWARD,
				IMGUI_ICON_BACKWARD,
				IMGUI_ICON_ARROW_UP,
				IMGUI_ICON_ARROW_RIGHT,
				IMGUI_ICON_ARROW_LEFT,
				IMGUI_ICON_ARROW_DOWN,
				IMGUI_ICON_SMALL_ARROW_UP,
				IMGUI_ICON_SMALL_ARROW_RIGHT,
				IMGUI_ICON_SMALL_ARROW_LEFT,
				IMGUI_ICON_SMALL_ARROW_DOWN,
				IMGUI_ICON_SEARCH,
				IMGUI_ICON_SEARCH_LOCATION,
				IMGUI_ICON_ADD,
				IMGUI_ICON_MINUS,
				IMGUI_ICON_TRASH,
				IMGUI_ICON_DELETE,
				IMGUI_ICON_ABORT,
				IMGUI_ICON_HOME,
				IMGUI_ICON_VISIBLE,
				IMGUI_ICON_HIDDEN,
				IMGUI_ICON_CLOSE,
				IMGUI_ICON_SLIDERS,
				IMGUI_ICON_BONE,
				IMGUI_ICON_SAVE,
				IMGUI_ICON_SAVE_ALTERNATIVE,
				IMGUI_ICON_COPY,
				IMGUI_ICON_PASTE,
				IMGUI_ICON_CLONE,
				IMGUI_ICON_EDIT,
				IMGUI_ICON_FIT,
				IMGUI_ICON_STAR,
				IMGUI_ICON_STAR_HALF,
				IMGUI_ICON_CIRCLE_HALF,
				IMGUI_ICON_TRANSLATION,
				IMGUI_ICON_FLIP_X_LINE,
				IMGUI_ICON_FLIP_Y,
				IMGUI_ICON_INFO,
				IMGUI_ICON_CUT,
				IMGUI_ICON_COLLAPSE,
				IMGUI_ICON_EXPAND,
				IMGUI_ICON_SCALE,
				IMGUI_ICON_CHART,
				IMGUI_ICON_ARROW_ALL_DIR,
				IMGUI_ICON_ANGLE_LEFT,
				IMGUI_ICON_ANGLE_RIGHT,
				IMGUI_ICON_LIGHT,
				IMGUI_ICON_WALKING,
				IMGUI_ICON_GRID,
				IMGUI_ICON_CAMERA,
				IMGUI_ICON_CAMERA_ROTATE,
				IMGUI_ICON_TILE,
				IMGUI_ICON_DOODAD,
				IMGUI_ICON_DECAL,
				IMGUI_ICON_SPAWN_POINT,
				IMGUI_ICON_SECTOR,
				IMGUI_ICON_PAINT_ROLLER,
				IMGUI_ICON_COGS,
				IMGUI_ICON_IMAGE,
				IMGUI_ICON_LINK,
				IMGUI_ICON_UNLINK,
				IMGUI_ICON_SUN,
				IMGUI_ICON_MOON,
				IMGUI_ICON_DRAGON,
				IMGUI_ICON_BOX,
				IMGUI_ICON_BOXES,
				IMGUI_ICON_USER,
				IMGUI_ICON_MAP_MARKER,
				IMGUI_ICON_GOAL,
				IMGUI_ICON_SUBOBJECT,
				IMGUI_ICON_EXPAND_NO_ARROWS,
				IMGUI_ICON_EXPAND_ALT,
				IMGUI_ICON_WRENCH,
				IMGUI_ICON_CLOUD_SUN,
				IMGUI_ICON_VOLUME_UP,
				IMGUI_ICON_VOLUME_MUTE,
				IMGUI_ICON_STATISTICS,
				IMGUI_ICON_OBJECT_GROUP,
				IMGUI_ICON_GRAPH,
				IMGUI_ICON_GLOBE,
				IMGUI_ICON_DUNGEON,
				IMGUI_ICON_MOUNTAIN,
				IMGUI_ICON_SLASH,
				IMGUI_ICON_WARNING,
				IMGUI_ICON_DICE,
				IMGUI_ICON_ROUTE,
				IMGUI_ICON_MENU,
				IMGUI_ICON_WINDOW_CLOSE,
				IMGUI_ICON_DOVE,
				IMGUI_ICON_RULER_HORIZONTAL,
				IMGUI_ICON_RULER_VERTICAL,
				IMGUI_ICON_STAMP,
				IMGUI_ICON_HISTORY,
				IMGUI_ICON_DRAFTING_COMPASS,
				IMGUI_ICON_CUBE,
				IMGUI_ICON_ASTERISK,
				IMGUI_ICON_SHARE,
				IMGUI_ICON_HELP,
				IMGUI_ICON_CLOCK,
				IMGUI_ICON_SHAPES,
				IMGUI_ICON_OUTDENT,
				IMGUI_ICON_NETWORK,
				IMGUI_ICON_DRAG_DROP,
				IMGUI_ICON_SOCKET_SQUARE,
				IMGUI_ICON_ELLIPSIS,
				IMGUI_ICON_YES,
				IMGUI_ICON_NO,
				IMGUI_ICON_ROOF,
				IMGUI_ICON_WAND,
				IMGUI_ICON_CROSSHAIRS,
				IMGUI_ICON_WAVE,
				IMGUI_ICON_BEZIER,
				IMGUI_ICON_POLL_VERTICAL,
				IMGUI_ICON_POLL_HORIZONTAL,
				IMGUI_ICON_SCALE_BALANCED,
				IMGUI_ICON_TURN_UP,
				IMGUI_ICON_TURN_DOWN,
				IMGUI_ICON_TOGGLE_ON,
				IMGUI_ICON_TOGGLE_OFF,
				IMGUI_ICON_LOW_VISION,
				IMGUI_ICON_FOOT_PRINTS,
				IMGUI_ICON_WIND,
				IMGUI_ICON_INFINITY,
			};

			constexpr size_t IconComboColumns = 16;
			constexpr size_t NoIcon = 0;

			constexpr size_t FindUnitID(const std::string_view& name)
			{
				for (size_t a = 0; a < std::size(Units); a++)
					if (name == Units[a].name)
						return a;

				return CustomUnit;
			}

			constexpr size_t NoUnit = FindUnitID("None");
			constexpr size_t Unit_RGB = FindUnitID("RGB");
			constexpr size_t Unit_sRGB = FindUnitID("sRGB");

			std::string_view TryGetIcon(size_t icon)
			{
				if (icon != NoIcon && icon < std::size(Icons))
					return Icons[icon - 1];

				return "";
			}

			inline float ConvertUnit(float value, size_t from, size_t to, bool apply_offset = true)
			{
				if (from >= std::size(Units) || to >= std::size(Units) || Units[from].group != Units[to].group)
					return value;

				if (std::abs(Units[from].power - 1.0f) > 1e-3f || std::abs(Units[to].power - 1.0f) > 1e-3f)
					return std::pow(std::max(0.0f, value), Units[to].power / Units[from].power) * (Units[from].scale / Units[to].scale);

				return value * (Units[from].scale / Units[to].scale);
			}

			constexpr size_t UnitGroup(size_t unit)
			{
				if (unit >= CustomUnit)
					return NoUnitGroup;

				return Units[unit].group;
			}

			constexpr size_t UnitGroupCount(size_t unit)
			{
				if (unit >= CustomUnit)
					return 1;

				return UnitGroups[UnitGroup(unit)].count;
			}

			constexpr size_t FindDefaultUnit(size_t unit)
			{
				const auto unit_group = UnitGroup(unit);
				for (size_t a = 0; a < std::size(Units); a++)
					if (Units[a].group == unit_group && Units[a].prefer_default)
						return a;

				return unit;
			}

			char* EscapeFormat(char* buffer, size_t buffer_size, const std::string_view& value)
			{
				if (buffer_size == 0)
					return buffer;

				size_t pos = 0;
				for (size_t a = 0; a < value.length(); a++)
				{
					if (value[a] == '%')
					{
						if (pos + 2 >= buffer_size)
							break;

						buffer[pos++] = '%';
						buffer[pos++] = '%';
					}
					else
					{
						if (pos + 1 >= buffer_size)
							break;

						buffer[pos++] = value[a];
					}
				}

				buffer[pos++] = '\0';
				return buffer;
			}

			char* AppendUnit(char* buffer, size_t buffer_size, const std::string_view& fmt, size_t unit, const std::string_view& custom_unit, bool add_in = false)
			{
				char unit_buffer[256] = { '\0' };
				if (unit < CustomUnit)
					EscapeFormat(unit_buffer, std::size(unit_buffer), Units[unit].symbol);
				else
					EscapeFormat(unit_buffer, std::size(unit_buffer), custom_unit);

				if (unit_buffer[0] != '\0')
				{
					if(add_in)
						ImFormatString(buffer, buffer_size, "%.*s in %s", unsigned(fmt.length()), fmt.data(), unit_buffer);
					else
						ImFormatString(buffer, buffer_size, "%.*s %s", unsigned(fmt.length()), fmt.data(), unit_buffer);
				}
				else
					ImFormatString(buffer, buffer_size, "%.*s", unsigned(fmt.length()), fmt.data());

				return buffer;
			}

			char* GetUnitName(char* buffer, size_t buffer_size, size_t unit, bool small_display)
			{
				if (unit == CustomUnit)
					ImFormatString(buffer, buffer_size, "Custom");
				else if (Units[unit].symbol.length() > 0)
				{
					if (small_display)
						ImFormatString(buffer, buffer_size, "%.*s", unsigned(Units[unit].symbol.length()), Units[unit].symbol.data());
					else
						ImFormatString(buffer, buffer_size, "%.*s (%.*s)", unsigned(Units[unit].name.length()), Units[unit].name.data(), unsigned(Units[unit].symbol.length()), Units[unit].symbol.data());
				}
				else
					ImFormatString(buffer, buffer_size, "%.*s", unsigned(Units[unit].name.length()), Units[unit].name.data());

				return buffer;
			}

			float CalcUnitComboWidth(bool small_display, const std::optional<size_t>& ref_unit = std::nullopt)
			{
				if (ref_unit)
					if (*ref_unit >= CustomUnit || UnitGroupCount(*ref_unit) < 2)
						return 0.0f;
				
				char buffer[256] = { '\0' };

				const float padding = 2.0f * ImGui::GetStyle().FramePadding.x;	
				float width = 0.0f;
				for (size_t a = 0; a < std::size(Units); a++)
				{
					if (ref_unit && UnitGroup(*ref_unit) != UnitGroup(a))
						continue;

					width = std::max(width, ImGui::CalcTextSize(GetUnitName(buffer, std::size(buffer), a, small_display)).x + padding);
				}

				return width;
			}

			bool UnitCombo(size_t* unit, bool small_display, const std::optional<size_t>& ref_unit = std::nullopt)
			{
				bool changed = false;

				if (ref_unit)
				{
					if (UnitGroup(*unit) != UnitGroup(*ref_unit))
					{
						*unit = *ref_unit;
						changed = true;
					}

					if (*ref_unit >= CustomUnit || UnitGroupCount(*ref_unit) < 2)
						return false;
				}

				char buffer0[256] = { '\0' };
				char buffer1[256] = { '\0' };

				ImFormatString(buffer0, std::size(buffer0), "%s###UnitSelection", GetUnitName(buffer1, std::size(buffer1), *unit, small_display));

				const auto w = ImGui::CalcItemWidth();
				ImGui::Button(buffer0, ImVec2(w, ImGui::GetFrameHeight()));

				const auto id = ImGui::GetCurrentWindow()->GetID("##UnitSelectionPopup");
				if (ImGui::IsItemHovered(ImGuiMouseButton_Left) && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup) && ImGui::IsItemDeactivated())
					ImGui::OpenPopupEx(id);

				if (ImGui::BeginPopupEx(id, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoSavedSettings))
				{
					for (size_t g = 0; g < std::size(UnitGroups); g++)
					{
						if (UnitGroups[g].count == 0 || (ref_unit && g != UnitGroup(*ref_unit)))
							continue;

						bool in_menu = false;
						if (!ref_unit && UnitGroups[g].count > 1 && !UnitGroups[g].name.empty())
						{
							ImFormatString(buffer0, std::size(buffer0), "%.*s##Group%u", unsigned(UnitGroups[g].name.length()), UnitGroups[g].name.data(), unsigned(g));
							if (!ImGui::BeginMenu(buffer0))
								continue;

							in_menu = true;
						}

						ImGui::PushID(int(g));

						for (size_t a = 0; a < std::size(Units); a++)
						{
							if (UnitGroup(a) != g)
								continue;

							if (Units[a].symbol.length() > 0)
								ImFormatString(buffer0, std::size(buffer0), "%.*s (%.*s)##Unit%u", unsigned(Units[a].name.length()), Units[a].name.data(), unsigned(Units[a].symbol.length()), Units[a].symbol.data(), unsigned(a));
							else
								ImFormatString(buffer0, std::size(buffer0), "%.*s##Unit%u", unsigned(Units[a].name.length()), Units[a].name.data(), unsigned(a));

							if (ImGui::Selectable(buffer0, a == *unit, ImGuiSelectableFlags_DontClosePopups))
							{
								*unit = a;
								changed = true;
							}
						}

						ImGui::PopID();
						if (in_menu)
							ImGui::EndMenu();
					}

					if (!ref_unit && ImGui::Selectable("Custom##UnitCustom", CustomUnit == *unit, ImGuiSelectableFlags_DontClosePopups))
					{
						*unit = CustomUnit;
						changed = true;
					}

					if (changed)
						ImGui::CloseCurrentPopup();

					ImGui::EndPopup();
				}

				return changed;
			}

			bool IconCombo(size_t* icon)
			{
				char buffer[128] = { '\0' };
				if (*icon > 0 && *icon <= std::size(Icons))
					ImFormatString(buffer, std::size(buffer), "%.*s###IconButton", unsigned(Icons[(*icon) - 1].length()), Icons[(*icon) - 1].data());
				else
					ImFormatString(buffer, std::size(buffer), "No Icon###IconButton");

				bool changed = false;
				const auto w = ImGui::CalcItemWidth();
				ImGui::Button(buffer, ImVec2(w, ImGui::GetFrameHeight()));

				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("Icon");

				const auto id = ImGui::GetCurrentWindow()->GetID("##IconSelectionPopup");
				if (ImGui::IsItemHovered(ImGuiMouseButton_Left) && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup) && ImGui::IsItemDeactivated())
					ImGui::OpenPopupEx(id);

				if (ImGui::BeginPopupEx(id, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoSavedSettings))
				{
					float icon_w = 0.0f;
					for (const auto& a : Icons)
						icon_w = std::max(icon_w, ImGui::CalcTextSize(a.data(), a.data() + a.length(), true).x);

					const auto button_size = ImVec2(icon_w + (2.0f * ImGui::GetStyle().FramePadding.x), ImGui::GetFrameHeight());
					auto IconButton = [&](size_t id) 
					{
						ImGui::PushStyleColor(ImGuiCol_Button, *icon == id ? ImGui::GetColorU32(ImGuiCol_ButtonHovered) : IM_COL32(0, 0, 0, 0));
						
						if (id == NoIcon)
							ImFormatString(buffer, std::size(buffer), "No Icon##0");
						else
							ImFormatString(buffer, std::size(buffer), "%.*s##%u", unsigned(Icons[id - 1].length()), Icons[id - 1].data(), unsigned(id));

						if (ImGui::Button(buffer, id == NoIcon ? ImVec2(0.0f, 0.0f) : button_size))
						{
							*icon = id;
							changed = true;
						}

						ImGui::PopStyleColor();
					};

					IconButton(NoIcon);

					constexpr ImGuiTableFlags tableFlags = ImGuiTableFlags_NoSavedSettings | ImGuiTableFlags_SizingFixedSame | ImGuiTableFlags_NoPadInnerX;
					if (ImGui::BeginTable("##IconTable", int(IconComboColumns), tableFlags))
					{
						for (size_t a = 0; a < std::size(Icons); a += IconComboColumns)
						{
							ImGui::TableNextRow();

							for (size_t b = 0; b < IconComboColumns && a + b < std::size(Icons); b++)
							{
								ImGui::TableNextColumn();

								IconButton(a + b + 1);
							}
						}

						ImGui::EndTable();
					}

					if (changed)
						ImGui::CloseCurrentPopup();

					ImGui::EndPopup();
				}

				return changed;
			}

			bool PrioritySelect(int* value)
			{
				const bool changed = ImGui::DragInt("##Priority", value, 1.0f, -INT_MAX, INT_MAX, "Priority: %d");
				if (ImGui::IsItemHovered())
				{
					ImGui::BeginTooltip();
					ImGui::PushTextWrapPos(35.0f * ImGui::GetFontSize());

					ImGui::TextWrapped("Priority");
					ImGui::Separator();
					ImGui::TextWrapped("This value determines the order in which parameters are listed in the Effect Graph Instance Editor. Parameters with lower priority will be listed first.");

					ImGui::PopTextWrapPos();
					ImGui::EndTooltip();
				}

				return changed;
			}

			char* FormatParameterName(char* buffer, size_t buffer_size, const std::string_view& name, size_t icon, bool show_name = true, bool show_icon = true)
			{
				const bool has_i = show_icon && icon != NoIcon;
				const bool has_n = show_name && name.length() > 0;
				const auto i = TryGetIcon(icon);

			#define SWITCH_FLAG(a,b)((a ? 0x2 : 0x0) | (b ? 0x1 : 0x0))

				switch (SWITCH_FLAG(has_i, has_n))
				{
					case SWITCH_FLAG( true,  true):	ImFormatString(buffer, buffer_size, "%.*s %.*s", unsigned(i.length()), i.data(), unsigned(name.length()), name.data()); break;
					case SWITCH_FLAG( true, false):	ImFormatString(buffer, buffer_size, "%.*s", unsigned(i.length()), i.data()); break;
					case SWITCH_FLAG(false,  true):	ImFormatString(buffer, buffer_size, "%.*s", unsigned(name.length()), name.data()); break;
					case SWITCH_FLAG(false, false):	ImFormatString(buffer, buffer_size, ""); break;
				}

			#undef SWITCH_FLAG

				return buffer;
			}

			char* FormatLabelName(char* buffer, size_t buffer_size, const std::string_view& name, const std::string_view& param, bool show_param)
			{
				if (show_param && param.length() > 0)
					ImFormatString(buffer, buffer_size, "%.*s %.*s", unsigned(name.length()), name.data(), unsigned(param.length()), param.data());
				else
					ImFormatString(buffer, buffer_size, "%.*s", unsigned(name.length()), name.data());

				return buffer;
			}

			void UnitSelectTooltip()
			{
				if (ImGui::IsItemHovered())
				{
					ImGui::BeginTooltip();
					ImGui::PushTextWrapPos(35.0f * ImGui::GetFontSize());

					ImGui::TextWrapped("Select Unit");
					ImGui::Separator(); 
					ImGui::TextWrapped("The node will output values in this unit. Automatic unit conversion to this unit will be done when modifying the parameter value.");

					ImGui::PopTextWrapPos();
					ImGui::EndTooltip();
				}
			}

			void CustomTooltip(const std::string_view& tooltip, size_t icon)
			{
				if (ImGui::IsItemHovered() && tooltip.length() > 0)
				{
					ImGui::BeginTooltip();
					ImGui::PushTextWrapPos(35.0f * ImGui::GetFontSize());

					char buffer[512] = { '\0' };
					FormatParameterName(buffer, std::size(buffer), "", icon, false, false);
					if (buffer[0] != '\0')
					{
						ImGui::TextUnformatted(buffer);
						ImGui::Separator();
					}

					ImGui::TextWrapped("%.*s", unsigned(tooltip.length()), tooltip.data());
					ImGui::PopTextWrapPos();
					ImGui::EndTooltip();
				}
			}

			UIParameter::JsonValue CreateStringValue(const std::wstring_view& str, UIParameter::JsonAllocator& allocator)
			{
				UIParameter::JsonValue str_value;
				str_value.SetString(str.data(), rapidjson::SizeType(str.length()), allocator);
				return str_value;
			}

			UIParameter::JsonValue CreateStringValue(const std::string_view& str, UIParameter::JsonAllocator& allocator)
			{
				return CreateStringValue(StringToWstring(str), allocator);
			}

			std::string_view GetParameterName(const UIParameter::ParamNames& names, size_t index)
			{
				if (names.size() > index)
					return names[index];

				return "";
			}

			void TextClipped(const char* buffer, const float width)
			{
				ImGui::AlignTextToFramePadding();

				const auto size = ImVec2(width, ImGui::GetFrameHeight());

				const auto pos = ImGui::GetCurrentWindow()->DC.CursorPos;
				ImGui::Dummy(size);
				ImGui::RenderTextClipped(pos, pos + size, buffer, ImGui::FindRenderedTextEnd(buffer), nullptr, ImVec2(1.0f, 0.5f));

				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("%s", buffer);
			}

			class EmptyParameter : public UIParameter
			{
			private:
				const GraphType type;

			public:
				EmptyParameter(GraphType type, void* param_ptr, const ParamNames& names, const ParamRanges& mins, const ParamRanges& maxs, unsigned index) : UIParameter(param_ptr, names, mins, maxs, index), type(type) {}

				bool OnRender(float total_width, size_t num_params, bool show_icon, bool horizontal_layout) override { return false; }
				bool OnRenderLayoutEditor() override { return false; }
				
				std::optional<std::string_view> GetIcon() const override { return std::nullopt; }
				bool HasLayout() const override { return false; }
				bool HasEditor() const override { return false; }
				bool PreferHorizontalDisplay() const override { return false; }
				int GetPriority() const override { return -INT_MAX; }
				GraphType GetType() const override { return type; }
				std::optional<uint64_t> GetMergeHash() const override { return std::nullopt; }

				void GetValue(JsonValue& value, JsonAllocator& allocator) const override {}
				void SetValue(const JsonValue& value) override {}

				void GetLayout(JsonValue& value, JsonAllocator& allocator) const override {}
				void SetLayout(const JsonValue& value) override {}

				FileRefreshMode GetFileRefreshMode() const override { return FileRefreshMode::None; }
			};

			template<size_t N> class FloatParameter : public UIParameter
			{
				static_assert(N >= 1 && N <= 4, "Invalid count for FloatParameter");

			private:
				enum class LayoutMode
				{
					Slider,
					Input,
					Drag,
					Variance,
					ColorPicker,
				};

				struct Layout
				{
					size_t unit = NoUnit;
					size_t icon = NoIcon;
					int priority = 0;
					LayoutMode mode = LayoutMode::Slider;
					Memory::DebugStringA<64> custom_unit;
					Memory::DebugStringA<512> tooltip;
					std::optional<float> min_value[N];
					std::optional<float> max_value[N];
					bool merge = false;
				};

				float value[N];
				size_t unit[N];
				Layout layout;

				unsigned StepSize() const
				{
					switch (layout.mode)
					{
						case LayoutMode::ColorPicker:	return 4;
						case LayoutMode::Variance:		return 2;
						default:						return 1;
					}
				}

				bool RenderSlider(size_t i, float total_width, bool display_label, bool show_icon)
				{
					bool changed = false;
					char buffer[512] = { '\0' };
					char buffer2[512] = { '\0' };

					if (UnitGroup(unit[i]) != UnitGroup(layout.unit))
						unit[i] = layout.unit;

					float t = ConvertUnit(value[i], layout.unit, unit[i]);

					float min_r = ConvertUnit(layout.min_value[i] ? *layout.min_value[i] : i < mins.size() ? mins[i] : 0.0f, layout.unit, unit[i]);
					float max_r = ConvertUnit(layout.max_value[i] ? *layout.max_value[i] : i < maxs.size() ? maxs[i] : 0.0f, layout.unit, unit[i]);
					if (min_r > max_r) std::swap(min_r, max_r);

					FormatParameterName(buffer, std::size(buffer), GetParameterName(names, i), layout.icon, display_label, show_icon);
					if(buffer[0] != '\0')
						ImFormatString(buffer2, std::size(buffer2), "%s: %%.3f", buffer);
					else
						ImFormatString(buffer2, std::size(buffer2), "%%.3f");

					AppendUnit(buffer, std::size(buffer), buffer2, unit[i], layout.custom_unit);
					const float unit_selection_width = CalcUnitComboWidth(true, layout.unit);

					ImGui::PushID(int(i));
					if (unit_selection_width > 0.0f)
						ImGui::SetNextItemWidth(total_width - (unit_selection_width + ImGui::GetStyle().ItemSpacing.x));
					else
						ImGui::SetNextItemWidth(total_width);

					if (ImGui::SliderFloat("##Value", &t, min_r, max_r, buffer))
					{
						value[i] = ConvertUnit(t, unit[i], layout.unit);
						if (layout.min_value[i]) value[i] = std::max(value[i], *layout.min_value[i]);
						if (layout.max_value[i]) value[i] = std::min(value[i], *layout.max_value[i]);

						changed = true;
					}

					if (unit_selection_width > 0.0f && unit[i] < CustomUnit)
					{
						ImGui::SameLine(0.0f, ImGui::GetStyle().ItemSpacing.x);
						ImGui::SetNextItemWidth(unit_selection_width);
						if (UnitCombo(&unit[i], true, layout.unit))
						{
							if (ImGui::GetIO().KeyCtrl)
								for (size_t a = 0; a < N; a++)
									unit[a] = unit[i];

							for (size_t a = 0; a < N; a++)
								value[a] = ConvertUnit(ConvertUnit(value[a], layout.unit, unit[a]), unit[a], layout.unit);

							changed = true;
						}
					}

					ImGui::PopID();
					return changed;
				}

				bool RenderInput(size_t i, float total_width, bool display_label, bool show_icon)
				{
					bool changed = false;
					char buffer[512] = { '\0' };
					char buffer2[512] = { '\0' };

					if (UnitGroup(unit[i]) != UnitGroup(layout.unit))
						unit[i] = layout.unit;

					float t = ConvertUnit(value[i], layout.unit, unit[i]);
					
					FormatParameterName(buffer, std::size(buffer), GetParameterName(names, i), layout.icon, display_label, show_icon);
					if (buffer[0] != '\0')
						ImFormatString(buffer2, std::size(buffer2), "%s: %%.3f", buffer);
					else
						ImFormatString(buffer2, std::size(buffer2), "%%.3f");

					AppendUnit(buffer, std::size(buffer), buffer2, unit[i], layout.custom_unit);
					const float unit_selection_width = CalcUnitComboWidth(true, layout.unit);

					ImGui::PushID(int(i));
					if (unit_selection_width > 0.0f)
						ImGui::SetNextItemWidth(total_width - (unit_selection_width + ImGui::GetStyle().ItemSpacing.x));
					else
						ImGui::SetNextItemWidth(total_width);

					if (ImGui::InputFloat("##Value", &t, 0.0f, 0.0f, buffer))
					{
						value[i] = ConvertUnit(t, unit[i], layout.unit);
						if (layout.min_value[i]) value[i] = std::max(value[i], *layout.min_value[i]);
						if (layout.max_value[i]) value[i] = std::min(value[i], *layout.max_value[i]);

						changed = true;
					}

					if (unit_selection_width > 0.0f && unit[i] < CustomUnit)
					{
						ImGui::SameLine(0.0f, ImGui::GetStyle().ItemSpacing.x);
						ImGui::SetNextItemWidth(unit_selection_width);
						if (UnitCombo(&unit[i], true, layout.unit))
						{
							if (ImGui::GetIO().KeyCtrl)
								for (size_t a = 0; a < N; a++)
									unit[a] = unit[i];

							for (size_t a = 0; a < N; a++)
								value[a] = ConvertUnit(ConvertUnit(value[a], layout.unit, unit[a]), unit[a], layout.unit);

							changed = true;
						}
					}

					ImGui::PopID();
					return changed;
				}

				bool RenderDrag(size_t i, float total_width, bool display_label, bool show_icon)
				{
					bool changed = false;
					char buffer[512] = { '\0' };
					char buffer2[512] = { '\0' };

					if (UnitGroup(unit[i]) != UnitGroup(layout.unit))
						unit[i] = layout.unit;

					float t = ConvertUnit(value[i], layout.unit, unit[i]);
					float min_r = layout.min_value[i] ? ConvertUnit(*layout.min_value[i], layout.unit, unit[i]) : -FLT_MAX;
					float max_r = layout.max_value[i] ? ConvertUnit(*layout.max_value[i], layout.unit, unit[i]) : FLT_MAX;
					if (min_r > max_r) std::swap(min_r, max_r);

					FormatParameterName(buffer, std::size(buffer), GetParameterName(names, i), layout.icon, display_label, show_icon);
					if (buffer[0] != '\0')
						ImFormatString(buffer2, std::size(buffer2), "%s: %%.3f", buffer);
					else
						ImFormatString(buffer2, std::size(buffer2), "%%.3f");

					AppendUnit(buffer, std::size(buffer), buffer2, unit[i], layout.custom_unit);
					const float unit_selection_width = CalcUnitComboWidth(true, layout.unit);

					ImGui::PushID(int(i));
					if (unit_selection_width > 0.0f)
						ImGui::SetNextItemWidth(total_width - (unit_selection_width + ImGui::GetStyle().ItemSpacing.x));
					else
						ImGui::SetNextItemWidth(total_width);

					if (ImGui::DragFloat("##Value", &t, 1.0f, min_r, max_r, buffer))
					{
						value[i] = ConvertUnit(t, unit[i], layout.unit);
						if (layout.min_value[i]) value[i] = std::max(value[i], *layout.min_value[i]);
						if (layout.max_value[i]) value[i] = std::min(value[i], *layout.max_value[i]);

						changed = true;
					}

					if (unit_selection_width > 0.0f && unit[i] < CustomUnit)
					{
						ImGui::SameLine(0.0f, ImGui::GetStyle().ItemSpacing.x);
						ImGui::SetNextItemWidth(unit_selection_width);
						if (UnitCombo(&unit[i], true, layout.unit))
						{
							if (ImGui::GetIO().KeyCtrl)
								for (size_t a = 0; a < N; a++)
									unit[a] = unit[i];

							for (size_t a = 0; a < N; a++)
								value[a] = ConvertUnit(ConvertUnit(value[a], layout.unit, unit[a]), unit[a], layout.unit);

							changed = true;
						}
					}

					ImGui::PopID();
					return changed;
				}

				bool RenderVariance(size_t i, float total_width, bool display_label, bool show_icon)
				{
					if (i + 1 >= N)
						return false;

					bool changed = false;
					char buffer[512] = { '\0' };
					char buffer2[512] = { '\0' };

					if (UnitGroup(unit[i]) != UnitGroup(layout.unit))
						unit[i] = layout.unit;

					unit[i + 1] = unit[i];

					float min_v = ConvertUnit(value[i], layout.unit, unit[i]);
					float max_v = ConvertUnit(value[i + 1], layout.unit, unit[i]);
					if (min_v > max_v) std::swap(min_v, max_v);

					float min_r = layout.min_value[i] ? ConvertUnit(*layout.min_value[i], layout.unit, unit[i]) : -FLT_MAX;
					float max_r = layout.max_value[i] ? ConvertUnit(*layout.max_value[i], layout.unit, unit[i]) :  FLT_MAX;
					if (min_r > max_r) std::swap(min_r, max_r);

					float max_diff = unit[i] >= CustomUnit ? -1.0f : Units[unit[i]].period;

					const float unit_selection_width = CalcUnitComboWidth(true, layout.unit);

					FormatParameterName(buffer, std::size(buffer), GetParameterName(names, i), layout.icon, display_label, show_icon);
					if (buffer[0] != '\0')
						ImFormatString(buffer2, std::size(buffer2), "%s: 00.000", buffer);
					else
						ImFormatString(buffer2, std::size(buffer2), "00.000");

					AppendUnit(buffer, std::size(buffer), buffer2, unit[i], layout.custom_unit);
					const auto drag_width = total_width - (unit_selection_width > 0.0f ? unit_selection_width + ImGui::GetStyle().ItemSpacing.x : 0.0f);
					const bool two_lines = drag_width < (ImGui::CalcTextSize(buffer).x + (ImGui::GetStyle().FramePadding.x * 2) + ImGui::GetStyle().ItemInnerSpacing.x) * 3.0f;

					FormatParameterName(buffer, std::size(buffer), GetParameterName(names, i), layout.icon, display_label, show_icon);
					if (buffer[0] != '\0')
						ImFormatString(buffer2, std::size(buffer2), "%s: %%.3f", buffer);
					else
						ImFormatString(buffer2, std::size(buffer2), "%%.3f");

					AppendUnit(buffer, std::size(buffer), buffer2, unit[i], layout.custom_unit);
					AppendUnit(buffer2, std::size(buffer2), "%.3f", unit[i], layout.custom_unit);

					ImGui::PushID(int(i));
					ImGui::SetNextItemWidth(drag_width);

					if (DragFloatRange3("##Value", &min_v, &max_v, 1.0f, min_r, max_r, max_diff, buffer, buffer2, nullptr, 0, two_lines))
					{
						value[i] = ConvertUnit(min_v, unit[i], layout.unit);
						value[i + 1] = ConvertUnit(max_v, unit[i], layout.unit);

						if (value[i] > value[i + 1]) std::swap(value[i], value[i + 1]);
						if (layout.min_value[i]) { value[i] = std::max(value[i], *layout.min_value[i]); value[i + 1] = std::max(value[i + 1], *layout.min_value[i]); }
						if (layout.max_value[i]) { value[i] = std::min(value[i], *layout.max_value[i]); value[i + 1] = std::min(value[i + 1], *layout.max_value[i]); }

						changed = true;
					}

					if (unit_selection_width > 0.0f && unit[i] < CustomUnit)
					{
						ImGui::SameLine(0.0f, ImGui::GetStyle().ItemSpacing.x);
						ImGui::SetNextItemWidth(unit_selection_width);
						if (UnitCombo(&unit[i], true, layout.unit))
						{
							unit[i + 1] = unit[i];
							if (ImGui::GetIO().KeyCtrl)
								for (size_t a = 0; a < N; a++)
									unit[a] = unit[i];

							for (size_t a = 0; a < N; a++)
								value[a] = ConvertUnit(ConvertUnit(value[a], layout.unit, unit[a]), unit[a], layout.unit);

							changed = true;
						}
					}

					ImGui::PopID();
					return changed;
				}

				bool RenderColorPicker(float total_width)
				{
					bool changed = false;
					const auto edit_width = total_width;
					
					const auto x = ImGui::GetCursorPosX();
					ImGui::BeginGroup();
					ImGui::SetNextItemWidth(edit_width);

					float v[N] = {};
					for (size_t a = 0; a < N; a++)
						v[a] = ConvertUnit(value[a], layout.unit, Unit_RGB);

					if constexpr (N == 3)
					{
						if (ImGui::ColorEdit3("##Edit", v, ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR))
							changed = true;
					}
					else if constexpr (N == 4)
					{
						if (ImGui::ColorEdit4("##Edit", v, ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR | ImGuiColorEditFlags_AlphaPreviewHalf))
							changed = true;
					}

					if (changed)
						for (size_t a = 0; a < N; a++)
							value[a] = ConvertUnit(v[a], Unit_RGB, layout.unit);

					ImGui::EndGroup();
					return changed;
				}

				void LoadOldLayout(const JsonValue& value)
				{
					const auto& range = value[L"custom_range"];
					if constexpr (N == 1)
					{
						if (range.IsArray() && range.GetArray().Size() >= 2)
						{
							layout.min_value[0] = range.GetArray()[0].GetFloat();
							layout.max_value[0] = range.GetArray()[1].GetFloat();
						}
					}
					else
					{
						if (range.IsArray() && range.GetArray().Size() >= N)
						{
							for (rapidjson::SizeType a = 0; a < N; a++)
							{
								const auto& r = range.GetArray()[a];
								if (r.IsArray() && r.GetArray().Size() >= 2)
								{
									layout.min_value[a] = r.GetArray()[0].GetFloat();
									layout.max_value[a] = r.GetArray()[1].GetFloat();
								}
							}
						}
					}
				}

				void LoadNewLayout(const JsonValue& value)
				{
					const auto& range = value[L"layout"];
					if (range.IsObject())
					{
						if (range.HasMember(L"mode"))
							if (auto mode = magic_enum::enum_cast<LayoutMode>(WstringToString(range[L"mode"].GetString())))
								layout.mode = *mode;

						if (range.HasMember(L"icon"))
							if (auto icon = range[L"icon"].GetUint(); icon < std::size(Icons))
								layout.icon = size_t(icon) + 1;

						if (range.HasMember(L"priority"))
							layout.priority = range[L"priority"].GetInt();

						if (range.HasMember(L"unit"))
						{
							const auto unit = WstringToString(range[L"unit"].GetString());
							if (auto uid = FindUnitID(unit); uid < CustomUnit)
								layout.unit = uid;
							else
								layout.custom_unit = unit;
						}

						if (layout.mode == LayoutMode::ColorPicker && UnitGroup(layout.unit) != UnitGroup(Unit_RGB))
						{
							layout.custom_unit = "";
							layout.unit = Unit_RGB;
						}

						layout.merge = range.HasMember(L"merge") && range[L"merge"].GetBool();

						if (range.HasMember(L"tooltip"))
							layout.tooltip = WstringToString(range[L"tooltip"].GetString());

						if (range.HasMember(L"range"))
						{
							const auto& input_range = range[L"range"].GetArray();
							for (rapidjson::SizeType a = 0; a < N && a < input_range.Size(); a += StepSize())
							{
								if (input_range[a].HasMember(L"min"))
									layout.min_value[a] = input_range[a][L"min"].GetFloat();

								if (input_range[a].HasMember(L"max"))
									layout.max_value[a] = input_range[a][L"max"].GetFloat();
							}
						}
					}
				}

				void SaveNewLayout(JsonValue& value, JsonAllocator& allocator) const
				{
					JsonValue range(rapidjson::kObjectType);
					bool has_layout = false;

					if (layout.mode != LayoutMode::Slider)
					{
						range.AddMember(L"mode", CreateStringValue(magic_enum::enum_name(layout.mode), allocator), allocator);
						has_layout = true;
					}

					if (layout.priority != 0)
					{
						range.AddMember(L"priority", layout.priority, allocator);
						has_layout = true;
					}

					if (layout.icon != NoIcon)
					{
						range.AddMember(L"icon", JsonValue().SetUint(unsigned(layout.icon - 1)), allocator);
						has_layout = true;
					}

					if ((layout.unit < CustomUnit || layout.custom_unit.length() > 0) && layout.unit != NoUnit)
					{
						range.AddMember(L"unit", CreateStringValue(layout.unit >= CustomUnit ? std::string_view(layout.custom_unit) : Units[layout.unit].name, allocator), allocator);
						has_layout = true;
					}

					if (layout.merge)
					{
						range.AddMember(L"merge", true, allocator);
						has_layout = true;
					}

					if (layout.tooltip.length() > 0)
					{
						range.AddMember(L"tooltip", CreateStringValue(layout.tooltip, allocator), allocator);
						has_layout = true;
					}

					JsonValue input_range(rapidjson::kArrayType);
					bool has_range = false;

					for (size_t a = 0; a < N; a += StepSize())
					{
						JsonValue n_range(rapidjson::kObjectType);

						if (layout.min_value[a])
						{
							n_range.AddMember(L"min", JsonValue().SetFloat(*layout.min_value[a]), allocator);
							has_range = true;
						}

						if (layout.max_value[a])
						{
							n_range.AddMember(L"max", JsonValue().SetFloat(*layout.max_value[a]), allocator);
							has_range = true;
						}

						input_range.PushBack(n_range, allocator);
					}

					if (has_range)
					{
						range.AddMember(L"range", input_range, allocator);
						has_layout = true;
					}

					if(has_layout)
						value.AddMember(L"layout", range, allocator);
				}

				float GetLabelWidth(size_t num_params, bool show_icon) const
				{
					char buffer[512] = { '\0' };

					float label_w = 0.0f;
					for (size_t a = 0; a < N; a += StepSize())
					{
						if (a < names.size())
						{
							FormatParameterName(buffer, std::size(buffer), GetParameterName(names, a), layout.icon, num_params > 1 || N > StepSize(), show_icon);
							label_w = std::max(label_w, ImGui::CalcTextSize(buffer, nullptr, true).x);
						}
					}

					return label_w;
				}

				static constexpr bool ModeSupported(LayoutMode mode)
				{
					switch (mode)
					{
						case LayoutMode::Variance:		return N % 2 == 0;
						case LayoutMode::ColorPicker:	return N >= 3;
						default:						return true;
					}
				}

			public:
				FloatParameter(void* param_ptr, const ParamNames& names, const ParamRanges& mins, const ParamRanges& maxs, unsigned index) : UIParameter(param_ptr, names, mins, maxs, index)
				{
					for (size_t a = 0; a < N; a++)
					{
						value[a] = 0.0f;
						unit[a] = NoUnit;
					}
				}

				bool OnRender(float total_width, size_t num_params, bool show_icon, bool horizontal_layout) override
				{
					bool changed = false;
					char buffer[512] = { '\0' };

					ImGui::PushID(int(id));
					ImGui::BeginGroup();

					switch (layout.mode)
					{
						case LayoutMode::Slider:
						{
							for (size_t a = 0; a < N; a += StepSize())
								if (RenderSlider(a, total_width, num_params > 1 || N > StepSize(), show_icon))
									changed = true;

							break;
						}

						case LayoutMode::Input:
						{
							for (size_t a = 0; a < N; a += StepSize())
								if (RenderInput(a, total_width, num_params > 1 || N > StepSize(), show_icon))
									changed = true;

							break;
						}

						case LayoutMode::Drag:
						{
							for (size_t a = 0; a < N; a += StepSize())
								if (RenderDrag(a, total_width, num_params > 1 || N > StepSize(), show_icon))
									changed = true;

							break;
						}

						case LayoutMode::Variance:
						{
							for (size_t a = 0; a < N; a += StepSize())
								if (RenderVariance(a, total_width, num_params > 1 || N > StepSize(), show_icon))
									changed = true;

							break;
						}

						case LayoutMode::ColorPicker:
						{
							const float label_w = GetLabelWidth(num_params, show_icon);
							if (const float remaining_width = total_width - (label_w > 0.0f ? label_w + ImGui::GetStyle().ItemSpacing.x : 0.0f); remaining_width > 0.0f)
							{
								if (label_w > 0.0f)
								{
									FormatParameterName(buffer, std::size(buffer), GetParameterName(names, 0), layout.icon, num_params > 1, show_icon);
									TextClipped(buffer, label_w);
									ImGui::SameLine(0.0f, ImGui::GetStyle().ItemSpacing.x);
								}

								if (RenderColorPicker(remaining_width))
									changed = true;
							}

							break;
						}

						default:
							break;
					}

					ImGui::EndGroup();
					CustomTooltip(layout.tooltip, layout.icon);

					ImGui::PopID();
					return changed;
				}

				bool OnRenderLayoutEditor() override
				{
					char buffer[512] = { '\0' };
					bool changed = false;
					const auto total_width = ImGui::GetContentRegionAvail().x;
					const float spacing = ImGui::GetStyle().ItemSpacing.x;

					ImGui::PushID(int(id));
					ImGui::BeginGroup();
					ImGui::PushItemWidth(total_width);

					const auto mode_name = magic_enum::enum_name(layout.mode);
					ImFormatString(buffer, std::size(buffer), "%.*s", unsigned(mode_name.length()), mode_name.data());

					float icon_width = ImGui::CalcTextSize("No Icon").x;
					for (const auto& a : Icons)
						icon_width = std::max(icon_width, ImGui::CalcTextSize(a.data(), a.data() + a.length(), true).x);

					ImGui::SetNextItemWidth(std::max(4.0f, total_width - (icon_width + (2.0f * ImGui::GetStyle().FramePadding.x) + spacing)));
					if (ImGui::BeginCombo("##Mode", buffer))
					{
						for (const auto& a : magic_enum::enum_entries<LayoutMode>())
						{
							if (!ModeSupported(a.first))
								continue;

							ImFormatString(buffer, std::size(buffer), "%.*s##%u", unsigned(a.second.length()), a.second.data(), unsigned(a.first));
							if (ImGui::Selectable(buffer, layout.mode == a.first))
							{
								layout.mode = a.first;
								if (layout.mode == LayoutMode::ColorPicker)
								{
									layout.unit = Unit_RGB;
									layout.custom_unit = "";
								}

								changed = true;
							}
						}

						ImGui::EndCombo();
					}

					ImGui::SameLine(0.0f, spacing);

					ImGui::SetNextItemWidth(icon_width + (2.0f * ImGui::GetStyle().FramePadding.x));
					if (IconCombo(&layout.icon))
						changed = true;

					if (layout.mode == LayoutMode::ColorPicker)
					{
						float text_w = ImGui::CalcTextSize("Unit").x;

						ImGui::Text("Unit");
						ImGui::SameLine(text_w, spacing);
						ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
						if (UnitCombo(&layout.unit, false, Unit_RGB))
						{
							layout.custom_unit = "";
							changed = true;
						}

						UnitSelectTooltip();
					}
					else
					{
						float text_w = ImGui::CalcTextSize("Unit").x;

						for (size_t a = 0; a < N; a += StepSize())
						{
							FormatLabelName(buffer, std::size(buffer), "Min", GetParameterName(names, a), N > StepSize());
							text_w = std::max(text_w, ImGui::CalcTextSize(buffer).x);

							FormatLabelName(buffer, std::size(buffer), "Max", GetParameterName(names, a), N > StepSize());
							text_w = std::max(text_w, ImGui::CalcTextSize(buffer).x);
						}

						ImGui::Text("Unit");
						ImGui::SameLine(text_w, spacing);
						ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
						if (UnitCombo(&layout.unit, false))
						{
							layout.custom_unit = "";
							changed = true;
						}

						UnitSelectTooltip();

						if (layout.unit >= CustomUnit)
						{
							ImFormatString(buffer, std::size(buffer), "%s", layout.custom_unit.c_str());
							ImGui::InputTextWithHint("##CustomUnit", "Custom Unit", buffer, std::min(std::size(buffer), layout.custom_unit.max_size() + 1), ImGuiInputTextFlags_EnterReturnsTrue);
							if (ImGui::IsItemDeactivatedAfterEdit() && !ImGui::IsKeyReleased(ImGuiKey_Escape))
							{
								layout.custom_unit = buffer;
								changed = true;
							}
						}

						const auto reset_w = ImGui::CalcTextSize(IMGUI_ICON_ROTATE).x + (2.0f * ImGui::GetStyle().FramePadding.x);
						const auto column_w = (total_width - spacing) / 2;
						const auto slot_w = column_w - (text_w + spacing);
						const auto drag_w = slot_w - (reset_w + ImGui::GetStyle().ItemInnerSpacing.x);

						// Mins
						{
							ImGui::BeginGroup();

							for (size_t a = 0; a < N; a += StepSize())
							{
								ImGui::PushID(int(a));
								FormatLabelName(buffer, std::size(buffer), "Min", GetParameterName(names, a), N > StepSize());
								ImGui::Text("%s", buffer);

								ImGui::SameLine(text_w, spacing);

								float v = a < mins.size() ? mins[a] : 0.0f;
								ImFormatString(buffer, std::size(buffer), "");
								const bool has_v = layout.min_value[a] ? true : false;

								if (has_v)
								{
									v = *layout.min_value[a];
									AppendUnit(buffer, std::size(buffer), "%.3f", layout.unit, layout.custom_unit);
								}

								const float max_r = layout.max_value[a] ? *layout.max_value[a] : FLT_MAX;
								const float min_r = layout.unit < CustomUnit && Units[layout.unit].period > 0.0f ? -Units[layout.unit].period : -FLT_MAX;

								if (has_v && (v < min_r || v > max_r))
								{
									v = std::clamp(v, min_r, max_r);
									layout.min_value[a] = v;
									changed = true;
								}

								ImGui::SetNextItemWidth(has_v ? drag_w : slot_w);
								if (ImGui::DragFloat("##Value", &v, 1.0f, min_r, max_r, buffer) && has_v)
								{
									layout.min_value[a] = std::clamp(v, min_r, max_r);
									changed = true;
								}

								if (ImGui::IsItemActivated() && !has_v)
								{
									layout.min_value[a] = std::clamp(a < mins.size() ? mins[a] : 0.0f, min_r, max_r);
									changed = true;
								}

								if (has_v)
								{
									ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
									if (ImGui::Button(IMGUI_ICON_ROTATE "##Reset", ImVec2(reset_w, ImGui::GetFrameHeight())))
									{
										layout.min_value[a] = std::nullopt;
										changed = true;
									}

									if (ImGui::IsItemHovered())
									{
										FormatLabelName(buffer, std::size(buffer), "Min", GetParameterName(names, a), N > StepSize());
										ImGui::SetTooltip(IMGUI_ICON_ROTATE " Reset %s", buffer);
									}
								}

								ImGui::PopID();
							}

							ImGui::EndGroup();
						}

						ImGui::SameLine(0.0f, spacing);

						// Maxs
						{
							ImGui::BeginGroup();

							for (size_t a = 0; a < N; a += StepSize())
							{
								ImGui::PushID(int(a + N));
								FormatLabelName(buffer, std::size(buffer), "Max", GetParameterName(names, a), N > StepSize());
								ImGui::Text("%s", buffer);

								ImGui::SameLine(text_w, spacing);

								float v = a < maxs.size() ? maxs[a] : 1.0f;
								ImFormatString(buffer, std::size(buffer), "");
								const bool has_v = layout.max_value[a] ? true : false;

								if (has_v)
								{
									v = *layout.max_value[a];
									AppendUnit(buffer, std::size(buffer), "%.3f", layout.unit, layout.custom_unit);
								}

								const float min_r = layout.min_value[a] ? *layout.min_value[a] : -FLT_MAX;
								const float max_r = layout.unit < CustomUnit && Units[layout.unit].period > 0.0f ? Units[layout.unit].period : FLT_MAX;

								if (has_v && (v < min_r || v > max_r))
								{
									v = std::clamp(v, min_r, max_r);
									layout.max_value[a] = v;
									changed = true;
								}

								ImGui::SetNextItemWidth(has_v ? drag_w : slot_w);
								if (ImGui::DragFloat("##Value", &v, 1.0f, min_r, max_r, buffer) && has_v)
								{
									layout.max_value[a] = std::clamp(v, min_r, max_r);
									changed = true;
								}

								if (ImGui::IsItemActivated() && !has_v)
								{
									layout.max_value[a] = std::clamp(a < maxs.size() ? maxs[a] : 1.0f, min_r, max_r);
									changed = true;
								}

								if (has_v)
								{
									ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
									if (ImGui::Button(IMGUI_ICON_ROTATE "##Reset", ImVec2(reset_w, ImGui::GetFrameHeight())))
									{
										layout.max_value[a] = std::nullopt;
										changed = true;
									}

									if (ImGui::IsItemHovered())
									{
										FormatLabelName(buffer, std::size(buffer), "Max", GetParameterName(names, a), N > StepSize());
										ImGui::SetTooltip(IMGUI_ICON_ROTATE " Reset %s", buffer);
									}
								}

								ImGui::PopID();
							}

							ImGui::EndGroup();
						}
					}

					ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
					if (PrioritySelect(&layout.priority))
						changed = true;

					ImGui::Text("Tooltip:");
					ImFormatString(buffer, std::size(buffer), "%s", layout.tooltip.c_str());
					ImGui::InputTextMultiline("##Tooltip", buffer, std::size(buffer), ImVec2(0.0f, ImGui::GetFrameHeightWithSpacing() + ImGui::GetFrameHeight()), ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CtrlEnterForNewLine);
					if (ImGui::IsItemDeactivatedAfterEdit() && !ImGui::IsKeyReleased(ImGuiKey_Escape))
					{
						layout.tooltip = buffer;
						changed = true;
					}

					if (ImGui::Checkbox("Merge", &layout.merge))
						changed = true;

					if (ImGui::IsItemHovered())
					{
						ImGui::BeginTooltip();
						ImGui::PushTextWrapPos(35.0f * ImGui::GetFontSize());
						ImGui::Text("Merge");
						ImGui::Separator();
						ImGui::TextWrapped("If enabled, all parameters with this name and type will be merged into a single parameter in the Parameters Editor window.");
						ImGui::PopTextWrapPos();
						ImGui::EndTooltip();
					}

					ImGui::PopItemWidth();
					ImGui::EndGroup();
					ImGui::PopID();

					return changed;
				}

				std::optional<std::string_view> GetIcon() const override { return TryGetIcon(layout.icon); }
				bool HasLayout() const override { return true; }
				bool HasEditor() const override { return true; }
				bool PreferHorizontalDisplay() const override { return false; }
				int GetPriority() const override { return layout.priority; }

				GraphType GetType() const override
				{
					if constexpr (N == 1)
						return GraphType::Float;
					else if constexpr (N == 2)
						return GraphType::Float2;
					else if constexpr (N == 3)
						return GraphType::Float3;
					else
						return GraphType::Float4;
				}

				std::optional<uint64_t> GetMergeHash() const override
				{
					if (!layout.merge)
						return std::nullopt;

					uint64_t h[] = { uint64_t(GetType()), layout.unit };
					return MurmurHash2_64(h, int(sizeof(h)), 0x1337B33F);
				}

				void GetValue(JsonValue& param_obj, JsonAllocator& allocator) const override
				{
					simd::vector4 t;
					for (size_t a = 0; a < N; a++)
						t[unsigned(a)] = value[a];

					Renderer::DrawCalls::ParameterUtil::Save(param_obj, allocator, t, simd::vector4(FLT_MAX), false, N);

					JsonValue units(rapidjson::kArrayType);
					for (size_t a = 0; a < N; a += StepSize())
						units.PushBack(CreateStringValue(unit[a] >= CustomUnit ? std::string_view(layout.custom_unit) : Units[unit[a]].name, allocator), allocator);
					
					param_obj.AddMember(L"units", units, allocator);
				}

				void SetValue(const JsonValue& data_stream) override
				{
					simd::vector4 t;
					bool srgb = false;
					Renderer::DrawCalls::ParameterUtil::Read(data_stream, t, srgb, N);

					const auto default_unit = FindDefaultUnit(layout.unit);
					for (size_t a = 0; a < N; a++)
						unit[a] = default_unit;

					if (data_stream.HasMember(L"units"))
					{
						const auto& data_units = data_stream[L"units"];
						for (size_t a = 0, b = 0; a < N && b < data_units.GetArray().Size(); a += StepSize(), b++)
						{
							const auto unit_name = WstringToString(data_units.GetArray()[rapidjson::SizeType(b)].GetString());
							if (auto uid = FindUnitID(unit_name); uid < CustomUnit && UnitGroup(uid) == UnitGroup(layout.unit))
								for (size_t c = 0; c < StepSize(); c++)
									unit[a + c] = uid;
						}
					}

					for (size_t a = 0; a < N; a++)
						value[a] = t[unsigned(a)];
				}

				void GetLayout(JsonValue& value, JsonAllocator& allocator) const override
				{
					SaveNewLayout(value, allocator);
				}

				void SetLayout(const JsonValue& value) override
				{
					layout = Layout();

					if (value.HasMember(L"custom_range"))
						LoadOldLayout(value);
					else if (value.HasMember(L"layout"))
						LoadNewLayout(value);

					const auto default_unit = FindDefaultUnit(layout.unit);
					for (size_t a = 0; a < N; a++)
						if(UnitGroup(unit[a]) != UnitGroup(layout.unit))
							unit[a] = default_unit;
				}

				FileRefreshMode GetFileRefreshMode() const override { return FileRefreshMode::None; }
			};

			template<typename T, size_t N> class IntParameter : public UIParameter
			{
				static_assert(std::is_same_v<T, unsigned> || std::is_same_v<T, int>, "Invalid type for IntParameter");
				static_assert(N >= 1 && N <= 4, "Invalid count for IntParameter");

			private:
				enum class LayoutMode
				{
					Slider,
					Input,
					Drag,
					DropDown,
				};

				typedef Memory::DebugStringA<64> EnumName;
				struct Layout
				{
					size_t icon = NoIcon;
					int priority = 0;
					LayoutMode mode = LayoutMode::Slider;
					Memory::DebugStringA<512> tooltip;
					std::optional<T> min_value[N];
					std::optional<T> max_value[N];
					bool merge = false;

					Memory::SmallVector<std::pair<EnumName, T>, 4, Memory::Tag::Unknown> enum_names;
					Memory::FlatMap<T, size_t, Memory::Tag::Unknown> enum_cache;
				};

				T value[N];
				Layout layout;

				bool RenderSlider(size_t i, float total_width, bool display_label, bool show_icon)
				{
					bool changed = false;
					char buffer[512] = { '\0' };
					char buffer2[512] = { '\0' };

					int t = int(value[i]);
					int min_r = int(layout.min_value[i] ? *layout.min_value[i] : T(mins[i]));
					int max_r = int(layout.max_value[i] ? *layout.max_value[i] : T(maxs[i]));
					if (min_r > max_r) std::swap(min_r, max_r);

					if constexpr (std::is_same_v<T, unsigned>)
						min_r = std::max(min_r, 0);

					max_r = std::max(max_r, min_r + 1);

					FormatParameterName(buffer, std::size(buffer), GetParameterName(names, i), layout.icon, display_label, show_icon);
					if (buffer[0] != '\0')
						ImFormatString(buffer2, std::size(buffer2), "%s: %%d", buffer);
					else
						ImFormatString(buffer2, std::size(buffer2), "%%d");

					ImGui::PushID(int(i));
					ImGui::SetNextItemWidth(total_width);
					if (ImGui::SliderInt("##Value", &t, min_r, max_r, buffer2))
					{
						if constexpr (std::is_same_v<T, unsigned>)
							t = std::max(t, 0);

						value[i] = T(t);
						if (layout.min_value[i]) value[i] = std::max(value[i], *layout.min_value[i]);
						if (layout.max_value[i]) value[i] = std::min(value[i], *layout.max_value[i]);

						changed = true;
					}

					ImGui::PopID();
					return changed;
				}

				bool RenderInput(size_t i, float total_width, bool display_label, bool show_icon)
				{
					bool changed = false;
					char buffer[512] = { '\0' };
					char buffer2[512] = { '\0' };

					int t = int(value[i]);

					FormatParameterName(buffer, std::size(buffer), GetParameterName(names, i), layout.icon, display_label, show_icon);
					if (buffer[0] != '\0')
						ImFormatString(buffer2, std::size(buffer2), "%s: %%d", buffer);
					else
						ImFormatString(buffer2, std::size(buffer2), "%%d");

					ImGui::PushID(int(i));
					ImGui::SetNextItemWidth(total_width);
					if (ImGui::InputScalar("##Value", ImGuiDataType_S32, &t, nullptr, nullptr, buffer2))
					{
						if constexpr (std::is_same_v<T, unsigned>)
							t = std::max(t, 0);

						value[i] = T(t);
						if (layout.min_value[i]) value[i] = std::max(value[i], *layout.min_value[i]);
						if (layout.max_value[i]) value[i] = std::min(value[i], *layout.max_value[i]);

						changed = true;
					}

					ImGui::PopID();
					return changed;
				}

				bool RenderDrag(size_t i, float total_width, bool display_label, bool show_icon)
				{
					bool changed = false;
					char buffer[512] = { '\0' };
					char buffer2[512] = { '\0' };

					int t = int(value[i]);
					int min_r = layout.min_value[i] ? int(*layout.min_value[i]) : -INT_MAX;
					int max_r = layout.max_value[i] ? int(*layout.max_value[i]) : INT_MAX;
					if (min_r > max_r) std::swap(min_r, max_r);

					if constexpr (std::is_same_v<T, unsigned>)
						min_r = std::max(min_r, 0);

					max_r = std::max(max_r, min_r + 1);

					FormatParameterName(buffer, std::size(buffer), GetParameterName(names, i), layout.icon, display_label, show_icon);
					if (buffer[0] != '\0')
						ImFormatString(buffer2, std::size(buffer2), "%s: %%d", buffer);
					else
						ImFormatString(buffer2, std::size(buffer2), "%%d");

					ImGui::PushID(int(i));
					ImGui::SetNextItemWidth(total_width);
					if (ImGui::DragInt("##Value", &t, 1.0f, min_r, max_r, buffer2))
					{
						if constexpr (std::is_same_v<T, unsigned>)
							t = std::max(t, 0);

						value[i] = T(t);
						if (layout.min_value[i]) value[i] = std::max(value[i], *layout.min_value[i]);
						if (layout.max_value[i]) value[i] = std::min(value[i], *layout.max_value[i]);

						changed = true;
					}

					ImGui::PopID();
					return changed;
				}

				bool RenderDropDown(size_t i, float total_width)
				{
					bool changed = false;
					char buffer[256] = { '\0' };

					int t = int(value[i]);
					
					ImGui::PushID(int(i));
					if (layout.enum_names.empty() || ImGui::GetActiveID() == ImGui::GetCurrentWindow()->GetID("##ValueInput"))
					{
						const int min_r = std::is_same_v<T, unsigned> ? 0 : -INT_MAX;

						ImGui::SetNextItemWidth(total_width);
						if (ImGui::DragInt("##ValueInput", &t, 1.0f, min_r, INT_MAX, "%d"))
						{
							if constexpr (std::is_same_v<T, unsigned>)
								t = std::max(t, 0);

							value[i] = T(t);
							changed = true;
						}
					}
					else
					{
						ImFormatString(buffer, std::size(buffer), "%d", t);

						if (const auto f = layout.enum_cache.find(value[i]); f != layout.enum_cache.end())
						{
							if(ImGui::GetIO().KeyCtrl)
								ImFormatString(buffer, std::size(buffer), "%s (%d)", layout.enum_names[f->second].first.c_str(), t);
							else
								ImFormatString(buffer, std::size(buffer), "%s", layout.enum_names[f->second].first.c_str());
						}

						ImGui::SetNextItemWidth(total_width);
						if (ImGui::BeginCombo("##ValueDropDown", buffer))
						{
							for (const auto& a : layout.enum_names)
							{
								if (ImGui::GetIO().KeyCtrl)
									ImFormatString(buffer, std::size(buffer), "%s (%d)###%d", a.first.c_str(), int(a.second), int(a.second));
								else
									ImFormatString(buffer, std::size(buffer), "%s###%d", a.first.c_str(), int(a.second));

								if (ImGui::Selectable(buffer, value[i] == a.second))
								{
									value[i] = a.second;
									changed = true;
								}
							}

							ImGui::EndCombo();
						}
					}

					ImGui::PopID();
					return changed;
				}

				void BuildEnumCache()
				{
					layout.enum_cache.clear();
					for (size_t a = 0; a < layout.enum_names.size(); a++)
						layout.enum_cache[layout.enum_names[a].second] = a;
				}

				void LoadOldLayout(const JsonValue& value)
				{
					const auto& range = value[L"custom_range"];
					if constexpr (N == 1)
					{
						if (range.IsArray() && range.GetArray().Size() >= 2)
						{
							layout.min_value[0] = T(range.GetArray()[0].GetInt());
							layout.max_value[0] = T(range.GetArray()[1].GetInt());
						}
					}
					else
					{
						if (range.IsArray() && range.GetArray().Size() >= N)
						{
							for (rapidjson::SizeType a = 0; a < N; a++)
							{
								const auto& r = range.GetArray()[a];
								if (r.IsArray() && r.GetArray().Size() >= 2)
								{
									layout.min_value[a] = T(r.GetArray()[0].GetInt());
									layout.max_value[a] = T(r.GetArray()[1].GetInt());
								}
							}
						}
					}
				}

				void LoadNewLayout(const JsonValue& value)
				{
					const auto& range = value[L"layout"];
					if (range.IsObject())
					{
						if (range.HasMember(L"mode"))
							if (auto mode = magic_enum::enum_cast<LayoutMode>(WstringToString(range[L"mode"].GetString())))
								layout.mode = *mode;

						if (range.HasMember(L"icon"))
							if (auto icon = range[L"icon"].GetUint(); icon < std::size(Icons))
								layout.icon = size_t(icon) + 1;

						if (range.HasMember(L"priority"))
							layout.priority = range[L"priority"].GetInt();

						layout.merge = range.HasMember(L"merge") && range[L"merge"].GetBool();

						if (range.HasMember(L"tooltip"))
							layout.tooltip = WstringToString(range[L"tooltip"].GetString());

						if (layout.mode == LayoutMode::DropDown)
						{
							if (range.HasMember(L"values"))
							{
								const auto& enum_values = range[L"values"];
								if (enum_values.IsObject())
								{
									for (auto a = enum_values.MemberBegin(); a != enum_values.MemberEnd(); ++a)
									{
										const auto name = WstringToString(a->name.GetString());
										const auto value = T(a->value.GetInt());

										layout.enum_names.push_back(std::make_pair(name, value));
									}

									BuildEnumCache();
								}
							}
						}
						else
						{
							if (range.HasMember(L"range"))
							{
								const auto& input_range = range[L"range"].GetArray();
								for (rapidjson::SizeType a = 0; a < N && a < input_range.Size(); a++)
								{
									if (input_range[a].HasMember(L"min"))
										layout.min_value[a] = T(input_range[a][L"min"].GetInt());

									if (input_range[a].HasMember(L"max"))
										layout.max_value[a] = T(input_range[a][L"max"].GetInt());
								}
							}
						}
					}
				}

				void SaveNewLayout(JsonValue& value, JsonAllocator& allocator) const
				{
					JsonValue range(rapidjson::kObjectType);
					bool has_layout = false;

					if (layout.mode != LayoutMode::Slider)
					{
						range.AddMember(L"mode", CreateStringValue(magic_enum::enum_name(layout.mode), allocator), allocator);
						has_layout = true;
					}

					if (layout.priority != 0)
					{
						range.AddMember(L"priority", layout.priority, allocator);
						has_layout = true;
					}

					if (layout.icon != NoIcon)
					{
						range.AddMember(L"icon", JsonValue().SetUint(unsigned(layout.icon - 1)), allocator);
						has_layout = true;
					}

					if (layout.merge)
					{
						range.AddMember(L"merge", true, allocator);
						has_layout = true;
					}

					if (layout.tooltip.length() > 0)
					{
						range.AddMember(L"tooltip", CreateStringValue(layout.tooltip, allocator), allocator);
						has_layout = true;
					}

					if (layout.mode == LayoutMode::DropDown)
					{
						if (!layout.enum_names.empty())
						{
							JsonValue enum_values(rapidjson::kObjectType);

							for (const auto& a : layout.enum_names)
								enum_values.AddMember(CreateStringValue(a.first, allocator), JsonValue().SetInt(int(a.second)), allocator);

							range.AddMember(L"values", enum_values, allocator);
							has_layout = true;
						}
					}
					else
					{
						JsonValue input_range(rapidjson::kArrayType);
						bool has_range = false;

						for (size_t a = 0; a < N; a++)
						{
							JsonValue n_range(rapidjson::kObjectType);

							if (layout.min_value[a])
							{
								n_range.AddMember(L"min", JsonValue().SetInt(int(*layout.min_value[a])), allocator);
								has_range = true;
							}

							if (layout.max_value[a])
							{
								n_range.AddMember(L"max", JsonValue().SetInt(int(*layout.max_value[a])), allocator);
								has_range = true;
							}

							input_range.PushBack(n_range, allocator);
						}

						if (has_range)
						{
							range.AddMember(L"range", input_range, allocator);
							has_layout = true;
						}
					}

					if (has_layout)
						value.AddMember(L"layout", range, allocator);
				}

				float GetLabelWidth(size_t num_params, bool show_icon) const
				{
					char buffer[512] = { '\0' };

					float label_w = 0.0f;
					for (size_t a = 0; a < N; a++)
					{
						if (a < names.size())
						{
							FormatParameterName(buffer, std::size(buffer), GetParameterName(names, a), layout.icon, num_params > 1 || N > 1, show_icon);
							label_w = std::max(label_w, ImGui::CalcTextSize(buffer, nullptr, true).x);
						}
					}

					return label_w;
				}

			public:
				IntParameter(void* param_ptr, const ParamNames& names, const ParamRanges& mins, const ParamRanges& maxs, unsigned index) : UIParameter(param_ptr, names, mins, maxs, index)
				{
					for (size_t a = 0; a < N; a++)
						value[a] = 0;
				}

				bool OnRender(float total_width, size_t num_params, bool show_icon, bool horizontal_layout) override
				{
					bool changed = false;
					char buffer[512] = { '\0' };
					ImGui::PushID(int(id));
					ImGui::BeginGroup();

					switch (layout.mode)
					{
						case LayoutMode::Slider:
						{
							for (size_t a = 0; a < N; a++)
								if (RenderSlider(a, total_width, num_params > 1 || N > 1, show_icon))
									changed = true;

							break;
						}

						case LayoutMode::Input:
						{
							for (size_t a = 0; a < N; a++)
								if (RenderInput(a, total_width, num_params > 1 || N > 1, show_icon))
									changed = true;

							break;
						}

						case LayoutMode::Drag:
						{
							for (size_t a = 0; a < N; a++)
								if (RenderDrag(a, total_width, num_params > 1 || N > 1, show_icon))
									changed = true;

							break;
						}

						case LayoutMode::DropDown:
						{
							float label_w = GetLabelWidth(num_params, show_icon);

							if (const float remaining_width = label_w > 0.0f ? total_width - (label_w + ImGui::GetStyle().ItemSpacing.x) : total_width; remaining_width > 0.0f)
							{
								for (size_t a = 0; a < N; a++)
								{
									if (label_w > 0.0f)
									{
										FormatParameterName(buffer, std::size(buffer), GetParameterName(names, a), layout.icon, num_params > 1 || N > 1, show_icon);
										TextClipped(buffer, label_w);
										ImGui::SameLine(0.0f, ImGui::GetStyle().ItemSpacing.x);
									}

									if (RenderDropDown(a, remaining_width))
										changed = true;
								}
							}

							break;
						}

						default:
							break;
					}

					ImGui::EndGroup();
					CustomTooltip(layout.tooltip, layout.icon);

					ImGui::PopID();
					return changed;
				}

				bool OnRenderLayoutEditor() override
				{
					char buffer[512] = { '\0' };
					bool changed = false;
					const auto total_width = ImGui::GetContentRegionAvail().x;
					const float spacing = ImGui::GetStyle().ItemSpacing.x;

					ImGui::PushID(int(id));
					ImGui::BeginGroup();
					ImGui::PushItemWidth(total_width);

					const auto mode_name = magic_enum::enum_name(layout.mode);
					ImFormatString(buffer, std::size(buffer), "%.*s", unsigned(mode_name.length()), mode_name.data());

					float icon_width = ImGui::CalcTextSize("No Icon").x;
					for (const auto& a : Icons)
						icon_width = std::max(icon_width, ImGui::CalcTextSize(a.data(), a.data() + a.length(), true).x);

					ImGui::SetNextItemWidth(std::max(4.0f, total_width - (icon_width + (2.0f * ImGui::GetStyle().FramePadding.x) + spacing)));
					if (ImGui::BeginCombo("##Mode", buffer))
					{
						for (const auto& a : magic_enum::enum_entries<LayoutMode>())
						{
							ImFormatString(buffer, std::size(buffer), "%.*s##%u", unsigned(a.second.length()), a.second.data(), unsigned(a.first));
							if (ImGui::Selectable(buffer, layout.mode == a.first))
							{
								layout.mode = a.first;
								changed = true;
							}
						}

						ImGui::EndCombo();
					}

					ImGui::SameLine(0.0f, spacing);

					ImGui::SetNextItemWidth(icon_width + (2.0f * ImGui::GetStyle().FramePadding.x));
					if (IconCombo(&layout.icon))
						changed = true;

					if (layout.mode == LayoutMode::DropDown)
					{
						ImGui::SetNextItemWidth(total_width);
						if (ImGui::BeginCombo("##Values", "Named Values"))
						{
							static EnumName new_name;
							static int new_value;

							if (ImGui::IsWindowAppearing())
							{
								new_name = "";
								new_value = 0;
								for (const auto& a : layout.enum_names)
									new_value = std::max(new_value, int(a.second + 1));
							}

							constexpr ImGuiTableFlags tableFlags = ImGuiTableFlags_NoSavedSettings | ImGuiTableFlags_SizingStretchProp;
							if (ImGui::BeginTable("##ValueTable", 3, tableFlags))
							{
								size_t value_to_remove = ~0;

								const float row_height = ImGui::GetFrameHeightWithSpacing();
								ImGuiListClipper clipper;
								clipper.Begin(int(layout.enum_names.size()), row_height);
								while (clipper.Step())
								{
									for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
									{
										ImGui::PushID(i);
										ImGui::TableNextRow();

										if (ImGui::TableNextColumn())
										{
											int t = int(layout.enum_names[i].second);
											ImGui::SetNextItemWidth(ImGui::GetFrameHeight() * 2);
											if (ImGui::InputInt("##ModifyValue", &t, 0, 0, ImGuiInputTextFlags_EnterReturnsTrue))
											{
												if constexpr (std::is_same_v<T, unsigned>)
													t = std::max(t, 0);

												bool valid_value = true;
												for (size_t a = 0; a < layout.enum_names.size(); a++)
													if (a != size_t(i) && layout.enum_names[a].second == T(t))
														valid_value = false;

												if(valid_value)
												{
													layout.enum_names[i].second = T(t);
													BuildEnumCache();
													changed = true;
												}
											}
										}

										if (ImGui::TableNextColumn())
										{
											ImFormatString(buffer, std::size(buffer), "%s", layout.enum_names[i].first.c_str());
											ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
											if (ImGui::InputText("##ModifyName", buffer, std::size(buffer), ImGuiInputTextFlags_EnterReturnsTrue))
											{
												EnumName res = buffer;

												bool valid_name = !res.empty();
												for (size_t a = 0; a < layout.enum_names.size(); a++)
													if (a != size_t(i) && layout.enum_names[a].first == res)
														valid_name = false;

												if(valid_name)
												{
													layout.enum_names[i].first = res;
													BuildEnumCache();
													changed = true;
												}
											}
										}

										if (ImGui::TableNextColumn())
											if (ImGui::Button(IMGUI_ICON_ABORT "##RemoveValue"))
												value_to_remove = size_t(i);

										ImGui::PopID();
									}
								}

								if (value_to_remove < layout.enum_names.size())
								{
									for (size_t a = value_to_remove; a + 1 < layout.enum_names.size(); a++)
										layout.enum_names[a] = layout.enum_names[a + 1];

									layout.enum_names.pop_back();
									BuildEnumCache();
									changed = true;
								}

								ImGui::EndTable();
							}

							const auto w = std::max(ImGui::GetContentRegionAvail().x, 200.0f);
							const auto add_w = ImGui::CalcTextSize(IMGUI_ICON_ADD).x + (2.0f * ImGui::GetStyle().FramePadding.x);
							const auto input_w = (w - (add_w + spacing + spacing)) / 2.0f;
							bool force_add = false;

							ImFormatString(buffer, std::size(buffer), "%s", new_name.c_str());
							ImGui::SetNextItemWidth(input_w);
							if(ImGui::InputTextWithHint("##NewValueName", "New Value", buffer, std::size(buffer)))
								new_name = buffer;

							if (ImGui::IsItemDeactivated() && (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter)))
								force_add = true;

							ImGui::SameLine(0.0f, spacing);
							ImGui::SetNextItemWidth(input_w);
							if (ImGui::InputInt("##NewValue", &new_value, 0, 0))
							{
								if constexpr (std::is_same_v<T, unsigned>)
									new_value = std::max(new_value, 0);
							}

							if (ImGui::IsItemDeactivated() && (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter)))
								force_add = true;
							
							ImGui::SameLine(0.0f, spacing);
							ImGui::BeginDisabled(new_name.empty());
							if (ImGui::Button(IMGUI_ICON_ADD "##AddNewValue", ImVec2(add_w, ImGui::GetFrameHeight())) || force_add)
							{
								bool valid_name = true;
								bool valid_value = true;

								for (size_t a = 0; a < layout.enum_names.size(); a++)
								{
									if (layout.enum_names[a].first == new_name)
										valid_name = false;

									if (layout.enum_names[a].second == T(new_value))
										valid_value = false;
								}

								if(!valid_name)
									ImGui::OpenPopup("###ErrInvalidName");
								else if(!valid_value)
									ImGui::OpenPopup("###ErrInvalidValue");
								else
								{
									layout.enum_names.push_back(std::make_pair(new_name, T(new_value)));
									BuildEnumCache();
									new_name = "";
									new_value = 0;
									for (const auto& a : layout.enum_names)
										new_value = std::max(new_value, int(a.second + 1));

									changed = true;
								}
							}

							ImGui::EndDisabled();

							if (ImGui::BeginPopupModal("Error###ErrInvalidName", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
							{
								ImGui::Text("A value with this name already exists");
								if (ImGui::Button("OK", ImVec2(ImGui::GetContentRegionAvail().x, 0.0f)) || ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_Escape))
									ImGui::CloseCurrentPopup();

								ImGui::EndPopup();
							}

							if (ImGui::BeginPopupModal("Error###ErrInvalidValue", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
							{
								ImGui::Text("This value already has a name");
								if (ImGui::Button("OK", ImVec2(ImGui::GetContentRegionAvail().x, 0.0f)) || ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_Escape))
									ImGui::CloseCurrentPopup();

								ImGui::EndPopup();
							}

							ImGui::EndCombo();
						}
					}
					else
					{
						float text_w = 0.0f;
						for (size_t a = 0; a < N; a++)
						{
							FormatLabelName(buffer, std::size(buffer), "Min", GetParameterName(names, a), N > 1);
							text_w = std::max(text_w, ImGui::CalcTextSize(buffer).x);

							FormatLabelName(buffer, std::size(buffer), "Max", GetParameterName(names, a), N > 1);
							text_w = std::max(text_w, ImGui::CalcTextSize(buffer).x);
						}
					
						const auto reset_w = ImGui::CalcTextSize(IMGUI_ICON_ROTATE).x + (2.0f * ImGui::GetStyle().FramePadding.x);
						const auto column_w = (total_width - spacing) / 2;
						const auto slot_w = column_w - (text_w + spacing);
						const auto drag_w = slot_w - (reset_w + ImGui::GetStyle().ItemInnerSpacing.x);

						// Mins
						{
							ImGui::BeginGroup();

							for (size_t a = 0; a < N; a++)
							{
								ImGui::PushID(int(a));
								FormatLabelName(buffer, std::size(buffer), "Min", GetParameterName(names, a), N > 1);
								ImGui::Text("%s", buffer);

								ImGui::SameLine(text_w, spacing);

								int v = int(a < mins.size() ? mins[a] : 0.0f);
								ImFormatString(buffer, std::size(buffer), "");
								const bool has_v = layout.min_value[a] ? true : false;

								if (has_v)
								{
									v = int(*layout.min_value[a]);
									ImFormatString(buffer, std::size(buffer), "%%d");
								}

								const int max_r = layout.max_value[a] ? int(*layout.max_value[a]) : INT_MAX;
								const int min_r = std::is_same_v<T, unsigned> ? 0 : -INT_MAX;

								if (has_v && (v < min_r || v > max_r))
								{
									v = std::clamp(v, min_r, max_r);
									layout.min_value[a] = T(v);
									changed = true;
								}

								ImGui::SetNextItemWidth(has_v ? drag_w : slot_w);
								if (ImGui::DragInt("##Value", &v, 1.0f, min_r, max_r, buffer) && has_v)
								{
									layout.min_value[a] = T(std::clamp(v, min_r, max_r));
									changed = true;
								}

								if (ImGui::IsItemActivated() && !has_v)
								{
									layout.min_value[a] = T(std::clamp(int(a < mins.size() ? mins[a] : 0.0f), min_r, max_r));
									changed = true;
								}

								if (has_v)
								{
									ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
									if (ImGui::Button(IMGUI_ICON_ROTATE "##Reset", ImVec2(reset_w, ImGui::GetFrameHeight())))
									{
										layout.min_value[a] = std::nullopt;
										changed = true;
									}

									if (ImGui::IsItemHovered())
									{
										FormatLabelName(buffer, std::size(buffer), "Min", GetParameterName(names, a), N > 1);
										ImGui::SetTooltip(IMGUI_ICON_ROTATE " Reset %s", buffer);
									}
								}

								ImGui::PopID();
							}

							ImGui::EndGroup();
						}

						ImGui::SameLine(0.0f, spacing);

						// Maxs
						{
							ImGui::BeginGroup();

							for (size_t a = 0; a < N; a++)
							{
								ImGui::PushID(int(a + N));
								FormatLabelName(buffer, std::size(buffer), "Max", GetParameterName(names, a), N > 1);
								ImGui::Text("%s", buffer);

								ImGui::SameLine(text_w, spacing);

								int v = int(a < maxs.size() ? maxs[a] : 1.0f);
								ImFormatString(buffer, std::size(buffer), "");
								const bool has_v = layout.max_value[a] ? true : false;

								if (has_v)
								{
									v = int(*layout.max_value[a]);
									ImFormatString(buffer, std::size(buffer), "%%d");
								}

								const int min_r = layout.min_value[a] ? int(*layout.min_value[a]) : std::is_same_v<T, unsigned> ? 0 : -INT_MAX;
								const int max_r = INT_MAX;

								if (has_v && (v < min_r || v > max_r))
								{
									v = std::clamp(v, min_r, max_r);
									layout.max_value[a] = T(v);
									changed = true;
								}

								ImGui::SetNextItemWidth(has_v ? drag_w : slot_w);
								if (ImGui::DragInt("##Value", &v, 1.0f, min_r, max_r, buffer) && has_v)
								{
									layout.max_value[a] = T(std::clamp(v, min_r, max_r));
									changed = true;
								}

								if (ImGui::IsItemActivated() && !has_v)
								{
									layout.max_value[a] = T(std::clamp(int(a < maxs.size() ? maxs[a] : 1.0f), min_r, max_r));
									changed = true;
								}

								if (has_v)
								{
									ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
									if (ImGui::Button(IMGUI_ICON_ROTATE "##Reset", ImVec2(reset_w, ImGui::GetFrameHeight())))
									{
										layout.max_value[a] = std::nullopt;
										changed = true;
									}

									if (ImGui::IsItemHovered())
									{
										FormatLabelName(buffer, std::size(buffer), "Max", GetParameterName(names, a), N > 1);
										ImGui::SetTooltip(IMGUI_ICON_ROTATE " Reset %s", buffer);
									}
								}

								ImGui::PopID();
							}

							ImGui::EndGroup();
						}
					}

					ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
					if (PrioritySelect(&layout.priority))
						changed = true;

					ImGui::Text("Tooltip:");
					ImFormatString(buffer, std::size(buffer), "%s", layout.tooltip.c_str());
					ImGui::InputTextMultiline("##Tooltip", buffer, std::size(buffer), ImVec2(0.0f, ImGui::GetFrameHeightWithSpacing() + ImGui::GetFrameHeight()), ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CtrlEnterForNewLine);
					if (ImGui::IsItemDeactivatedAfterEdit() && !ImGui::IsKeyReleased(ImGuiKey_Escape))
					{
						layout.tooltip = buffer;
						changed = true;
					}

					if (ImGui::Checkbox("Merge", &layout.merge))
						changed = true;

					if (ImGui::IsItemHovered())
					{
						ImGui::BeginTooltip();
						ImGui::PushTextWrapPos(35.0f * ImGui::GetFontSize());
						ImGui::Text("Merge");
						ImGui::Separator();
						ImGui::TextWrapped("If enabled, all parameters with this name and type will be merged into a single parameter in the Parameters Editor window.");
						ImGui::PopTextWrapPos();
						ImGui::EndTooltip();
					}

					ImGui::PopItemWidth();
					ImGui::EndGroup();
					ImGui::PopID();

					return changed;
				}

				std::optional<std::string_view> GetIcon() const override { return TryGetIcon(layout.icon); }
				bool HasLayout() const override { return true; }
				bool HasEditor() const override { return true; }
				bool PreferHorizontalDisplay() const override { return false; }
				int GetPriority() const override { return layout.priority; }
				GraphType GetType() const override
				{
					if constexpr (std::is_same_v<T, unsigned>)
						return GraphType::UInt;
					else
					{
						if constexpr (N == 1)
							return GraphType::Int;
						else if constexpr (N == 2)
							return GraphType::Int2;
						else if constexpr (N == 3)
							return GraphType::Int3;
						else
							return GraphType::Int4;
					}
				}

				std::optional<uint64_t> GetMergeHash() const override
				{
					if (!layout.merge)
						return std::nullopt;

					uint64_t h[] = { uint64_t(GetType()) };
					return MurmurHash2_64(h, int(sizeof(h)), 0x1337B33F);
				}

				void GetValue(JsonValue& param_obj, JsonAllocator& allocator) const override
				{
					int t[N] = {};
					for (size_t a = 0; a < N; a++)
						t[a] = int(value[a]);

					Renderer::DrawCalls::ParameterUtil::Save(param_obj, allocator, t, INT_MAX);
				}

				void SetValue(const JsonValue& data_stream) override
				{
					int t[N] = {};
					Renderer::DrawCalls::ParameterUtil::Read(data_stream, t, INT_MAX);

					for (size_t a = 0; a < N; a++)
						value[a] = T(t[a]);
				}

				void GetLayout(JsonValue& value, JsonAllocator& allocator) const override
				{
					SaveNewLayout(value, allocator);
				}

				void SetLayout(const JsonValue& value) override
				{
					layout = Layout();

					if (value.HasMember(L"custom_range"))
						LoadOldLayout(value);
					else if (value.HasMember(L"layout"))
						LoadNewLayout(value);
				}

				FileRefreshMode GetFileRefreshMode() const override { return FileRefreshMode::None; }
			};

			class BoolParameter : public UIParameter
			{
			private:
				enum class LayoutMode
				{
					Toggle,
					DropDown,
				};

				typedef Memory::DebugStringA<64> EnumName;

				struct Layout
				{
					size_t icon = NoIcon;
					int priority = 0;
					LayoutMode mode = LayoutMode::Toggle;
					Memory::DebugStringA<512> tooltip;
					EnumName enum_names[2];
					bool merge = false;

					Layout()
					{
						enum_names[0] = "No";
						enum_names[1] = "Yes";
					}
				};

				bool value;
				Layout layout;

				bool RenderToggle(float total_width, size_t num_params, bool show_icon)
				{
					char buffer[512] = { '\0' };
					char buffer2[512] = { '\0' };

					FormatParameterName(buffer, std::size(buffer), GetParameterName(names, 0), layout.icon, num_params > 1, show_icon);
					ImFormatString(buffer2, std::size(buffer2), "%s###Value", buffer);
					return ImGuiEx::ToggleButton(buffer2, &value);
				}

				bool RenderDropDown(float total_width)
				{
					bool changed = false;
					char buffer[256] = { '\0' };

					ImFormatString(buffer, std::size(buffer), "%s", layout.enum_names[value ? 1 : 0].c_str());
					ImGui::SetNextItemWidth(total_width);
					if (ImGui::BeginCombo("##ValueDropDown", buffer))
					{
						ImFormatString(buffer, std::size(buffer), "%s##False", layout.enum_names[0].c_str());
						if (ImGui::Selectable(buffer, !value))
						{
							value = false;
							changed = true;
						}

						ImFormatString(buffer, std::size(buffer), "%s##True", layout.enum_names[1].c_str());
						if (ImGui::Selectable(buffer, value))
						{
							value = true;
							changed = true;
						}

						ImGui::EndCombo();
					}

					return changed;
				}

				void LoadNewLayout(const JsonValue& value)
				{
					const auto& range = value[L"layout"];
					if (range.IsObject())
					{
						if (range.HasMember(L"mode"))
							if (auto mode = magic_enum::enum_cast<LayoutMode>(WstringToString(range[L"mode"].GetString())))
								layout.mode = *mode;

						if (range.HasMember(L"icon"))
							if (auto icon = range[L"icon"].GetUint(); icon < std::size(Icons))
								layout.icon = size_t(icon) + 1;

						if (range.HasMember(L"priority"))
							layout.priority = range[L"priority"].GetInt();

						layout.merge = range.HasMember(L"merge") && range[L"merge"].GetBool();

						if (range.HasMember(L"tooltip"))
							layout.tooltip = WstringToString(range[L"tooltip"].GetString());

						if (layout.mode == LayoutMode::DropDown)
						{
							if (range.HasMember(L"values"))
							{
								auto& enum_values = range[L"values"];
								if (enum_values.HasMember(L"false"))
									layout.enum_names[0] = WstringToString(enum_values[L"false"].GetString());

								if (enum_values.HasMember(L"true"))
									layout.enum_names[1] = WstringToString(enum_values[L"true"].GetString());
							}
						}
					}
				}

				void SaveNewLayout(JsonValue& value, JsonAllocator& allocator) const
				{
					JsonValue range(rapidjson::kObjectType);
					bool has_layout = false;

					if (layout.mode != LayoutMode::Toggle)
					{
						range.AddMember(L"mode", CreateStringValue(magic_enum::enum_name(layout.mode), allocator), allocator);
						has_layout = true;
					}

					if (layout.icon != NoIcon)
					{
						range.AddMember(L"icon", JsonValue().SetUint(unsigned(layout.icon - 1)), allocator);
						has_layout = true;
					}

					if (layout.priority != 0)
					{
						range.AddMember(L"priority", layout.priority, allocator);
						has_layout = true;
					}

					if (layout.merge)
					{
						range.AddMember(L"merge", true, allocator);
						has_layout = true;
					}

					if (layout.tooltip.length() > 0)
					{
						range.AddMember(L"tooltip", CreateStringValue(layout.tooltip, allocator), allocator);
						has_layout = true;
					}

					if (layout.mode == LayoutMode::DropDown)
					{
						JsonValue enum_values(rapidjson::kObjectType);
						enum_values.AddMember(L"false", CreateStringValue(layout.enum_names[0], allocator), allocator);
						enum_values.AddMember(L"true", CreateStringValue(layout.enum_names[1], allocator), allocator);
						range.AddMember(L"values", enum_values, allocator);
						has_layout = true;
					}

					if (has_layout)
						value.AddMember(L"layout", range, allocator);
				}

				float GetLabelWidth(size_t num_params, bool show_icon) const
				{
					char buffer[512] = { '\0' };

					float label_w = 0.0f;
					if (!names.empty())
					{
						FormatParameterName(buffer, std::size(buffer), GetParameterName(names, 0), layout.icon, num_params > 1, show_icon);
						label_w = std::max(label_w, ImGui::CalcTextSize(buffer, nullptr, true).x);
					}

					return label_w;
				}

			public:
				BoolParameter(void* param_ptr, const ParamNames& names, const ParamRanges& mins, const ParamRanges& maxs, unsigned index) : UIParameter(param_ptr, names, mins, maxs, index) {}

				bool OnRender(float total_width, size_t num_params, bool show_icon, bool horizontal_layout) override
				{
					bool changed = false;
					ImGui::PushID(int(id));
					ImGui::BeginGroup();

					char buffer[512] = { '\0' };

					switch (layout.mode)
					{
						case LayoutMode::Toggle:
						{
							if (RenderToggle(total_width, num_params, show_icon))
								changed = true;

							break;
						}

						case LayoutMode::DropDown:
						{
							float label_w = GetLabelWidth(num_params, show_icon);

							if (const float remaining_width = label_w > 0.0f ? total_width - (label_w + ImGui::GetStyle().ItemSpacing.x) : total_width; remaining_width > 0.0f)
							{
								if (label_w > 0.0f)
								{
									FormatParameterName(buffer, std::size(buffer), GetParameterName(names, 0), layout.icon, num_params > 1, show_icon);
									TextClipped(buffer, label_w);
									ImGui::SameLine(0.0f, ImGui::GetStyle().ItemSpacing.x);
								}

								if (RenderDropDown(remaining_width))
									changed = true;
							}

							break;
						}

						default:
							break;
					}

					ImGui::EndGroup();
					CustomTooltip(layout.tooltip, layout.icon);

					ImGui::PopID();
					return changed;
				}

				bool OnRenderLayoutEditor() override
				{
					char buffer[512] = { '\0' };
					bool changed = false;
					const auto total_width = ImGui::GetContentRegionAvail().x;
					const float spacing = ImGui::GetStyle().ItemSpacing.x;

					ImGui::PushID(int(id));
					ImGui::BeginGroup();
					ImGui::PushItemWidth(total_width);

					const auto mode_name = magic_enum::enum_name(layout.mode);
					ImFormatString(buffer, std::size(buffer), "%.*s", unsigned(mode_name.length()), mode_name.data());

					float icon_width = ImGui::CalcTextSize("No Icon").x;
					for (const auto& a : Icons)
						icon_width = std::max(icon_width, ImGui::CalcTextSize(a.data(), a.data() + a.length(), true).x);

					ImGui::SetNextItemWidth(std::max(4.0f, total_width - (icon_width + (2.0f * ImGui::GetStyle().FramePadding.x) + spacing)));
					if (ImGui::BeginCombo("##Mode", buffer))
					{
						for (const auto& a : magic_enum::enum_entries<LayoutMode>())
						{
							ImFormatString(buffer, std::size(buffer), "%.*s##%u", unsigned(a.second.length()), a.second.data(), unsigned(a.first));
							if (ImGui::Selectable(buffer, layout.mode == a.first))
							{
								layout.mode = a.first;
								changed = true;
							}
						}

						ImGui::EndCombo();
					}

					ImGui::SameLine(0.0f, spacing);

					ImGui::SetNextItemWidth(icon_width + (2.0f * ImGui::GetStyle().FramePadding.x));
					if (IconCombo(&layout.icon))
						changed = true;

					if (layout.mode == LayoutMode::DropDown)
					{
						ImGui::SetNextItemWidth(total_width);
						if (ImGui::BeginCombo("##Values", "Named Values"))
						{
							ImGui::BeginGroup();

							ImGui::AlignTextToFramePadding();
							ImGui::Text("False");
							ImGui::AlignTextToFramePadding();
							ImGui::Text("True");

							ImGui::EndGroup();

							ImGui::SameLine();

							ImGui::BeginGroup();
							ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);

							ImFormatString(buffer, std::size(buffer), "%s", layout.enum_names[0].c_str());
							if (ImGui::InputText("##ModifyFalse", buffer, std::size(buffer), ImGuiInputTextFlags_EnterReturnsTrue))
							{
								EnumName res = buffer;
								if (res != layout.enum_names[1])
								{
									layout.enum_names[0] = res;
									changed = true;
								}
							}

							ImFormatString(buffer, std::size(buffer), "%s", layout.enum_names[1].c_str());
							if (ImGui::InputText("##ModifyTrue", buffer, std::size(buffer), ImGuiInputTextFlags_EnterReturnsTrue))
							{
								EnumName res = buffer;
								if (res != layout.enum_names[0])
								{
									layout.enum_names[1] = res;
									changed = true;
								}
							}

							ImGui::PopItemWidth();
							ImGui::EndGroup();
							ImGui::EndCombo();
						}
					}

					ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
					if (PrioritySelect(&layout.priority))
						changed = true;

					ImGui::Text("Tooltip:");
					ImFormatString(buffer, std::size(buffer), "%s", layout.tooltip.c_str());
					ImGui::InputTextMultiline("##Tooltip", buffer, std::size(buffer), ImVec2(0.0f, ImGui::GetFrameHeightWithSpacing() + ImGui::GetFrameHeight()), ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CtrlEnterForNewLine);
					if (ImGui::IsItemDeactivatedAfterEdit() && !ImGui::IsKeyReleased(ImGuiKey_Escape))
					{
						layout.tooltip = buffer;
						changed = true;
					}

					if (ImGui::Checkbox("Merge", &layout.merge))
						changed = true;

					if (ImGui::IsItemHovered())
					{
						ImGui::BeginTooltip();
						ImGui::PushTextWrapPos(35.0f * ImGui::GetFontSize());
						ImGui::Text("Merge");
						ImGui::Separator();
						ImGui::TextWrapped("If enabled, all parameters with this name and type will be merged into a single parameter in the Parameters Editor window.");
						ImGui::PopTextWrapPos();
						ImGui::EndTooltip();
					}

					ImGui::PopItemWidth();
					ImGui::EndGroup();
					ImGui::PopID();

					return changed;
				}

				std::optional<std::string_view> GetIcon() const override { return TryGetIcon(layout.icon); }
				bool HasLayout() const override { return true; }
				bool HasEditor() const override { return true; }
				bool PreferHorizontalDisplay() const override { return false; }
				int GetPriority() const override { return layout.priority; }
				GraphType GetType() const override { return GraphType::Bool; }

				std::optional<uint64_t> GetMergeHash() const override
				{
					if (!layout.merge)
						return std::nullopt;

					uint64_t h[] = { uint64_t(GetType()) };
					return MurmurHash2_64(h, int(sizeof(h)), 0x1337B33F);
				}

				void GetValue(JsonValue& param_obj, JsonAllocator& allocator) const override
				{
					Renderer::DrawCalls::ParameterUtil::Save(param_obj, allocator, value, !value);
				}

				void SetValue(const JsonValue& data_stream) override
				{
					Renderer::DrawCalls::ParameterUtil::Read(data_stream, value);
				}

				void GetLayout(JsonValue& value, JsonAllocator& allocator) const override
				{
					SaveNewLayout(value, allocator);
				}

				void SetLayout(const JsonValue& value) override
				{
					layout = Layout();

					if (value.HasMember(L"layout"))
						LoadNewLayout(value);
				}

				FileRefreshMode GetFileRefreshMode() const override { return FileRefreshMode::None; }
			};

			class TextureParameter : public UIParameter
			{
			private:
				static constexpr float PreviewTooltipSize = 512.0f;

				struct Layout
				{
					size_t icon = NoIcon;
					int priority = 0;
					Memory::DebugStringA<512> tooltip;
					bool merge = false;
				};

				Renderer::DrawCalls::ParameterUtil::TextureInfo value;
				GraphType type;
				Layout layout;
				FileRefreshMode refresh = FileRefreshMode::None;
				bool preview = true;

				void LoadNewLayout(const JsonValue& value)
				{
					const auto& range = value[L"layout"];
					if (range.IsObject())
					{
						if (range.HasMember(L"icon"))
							if (auto icon = range[L"icon"].GetUint(); icon < std::size(Icons))
								layout.icon = size_t(icon) + 1;

						if (range.HasMember(L"priority"))
							layout.priority = range[L"priority"].GetInt();

						layout.merge = range.HasMember(L"merge") && range[L"merge"].GetBool();
						
						if (range.HasMember(L"tooltip"))
							layout.tooltip = WstringToString(range[L"tooltip"].GetString());
					}
				}

				void SaveNewLayout(JsonValue& value, JsonAllocator& allocator) const
				{
					JsonValue range(rapidjson::kObjectType);
					bool has_layout = false;

					if (layout.icon != NoIcon)
					{
						range.AddMember(L"icon", JsonValue().SetUint(unsigned(layout.icon - 1)), allocator);
						has_layout = true;
					}

					if (layout.priority != 0)
					{
						range.AddMember(L"priority", layout.priority, allocator);
						has_layout = true;
					}

					if (layout.merge)
					{
						range.AddMember(L"merge", true, allocator);
						has_layout = true;
					}

					if (layout.tooltip.length() > 0)
					{
						range.AddMember(L"tooltip", CreateStringValue(layout.tooltip, allocator), allocator);
						has_layout = true;
					}

					if (has_layout)
						value.AddMember(L"layout", range, allocator);
				}

				void RenderPreview(float size)
				{
					if (type == GraphType::Texture)
					{
						ImGui::BeginGroup();
						const auto pos = ImGui::GetCursorPos();
						ImGui::Dummy(ImVec2(size, 0.0f));
						ImGui::SetCursorPos(pos);

						if (!value.texture_path.empty())
						{
							try {
								auto texture_handle = ::Texture::Handle(::Texture::SRGBTextureDesc(value.texture_path));
								const auto base_w = texture_handle.GetWidth();
								const auto base_h = texture_handle.GetHeight();
								if (base_w > 0 && base_h > 0)
								{
									const auto ratio = float(base_h) / float(base_w);

									const auto w = std::min(size, size / ratio);
									const auto h = std::min(size, size * ratio);

									ImGui::Image(texture_handle.GetTexture().Get(), ImVec2(w, h));
									RenderPreviewTooltip();
								}
							} catch (...) {}
						}

						ImGui::EndGroup();
					}
				}

				void RenderPreviewTooltip()
				{
					if (ImGui::IsItemHovered() && value.texture_path.length() > 0)
					{
						ImGui::BeginTooltip();

						if (type == GraphType::Texture)
						{
							try {
								auto texture_handle = ::Texture::Handle(::Texture::SRGBTextureDesc(value.texture_path));
								const auto base_w = texture_handle.GetWidth();
								const auto base_h = texture_handle.GetHeight();

								const auto w = PreviewTooltipSize;
								const auto h = float(base_h) * (PreviewTooltipSize / float(base_w));
								ImGui::PushTextWrapPos(w);

								ImGui::TextWrapped("%s", WstringToString(value.texture_path).c_str());
								if (ImGui::GetIO().KeyShift)
									ImGui::Image(texture_handle.GetTexture().Get(), ImVec2(w, h));
								else
								{
									ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetColorU32(ImGuiCol_TextDisabled));
									ImGui::TextWrapped("Hold Shift to see a preview of the image");
									ImGui::PopStyleColor();
								}

								ImGui::PopTextWrapPos();
							} catch (...) {}
						}
						else
						{
							ImGui::PushTextWrapPos(35.0f * ImGui::GetFontSize());
							ImGui::TextWrapped("%s", WstringToString(value.texture_path).c_str());
							ImGui::PopTextWrapPos();
						}

						ImGui::EndTooltip();
					}
				}

				bool RenderFileSelect(float size)
				{
					bool changed = false;
					char buffer[512] = { '\0' };
					char buffer2[512] = { '\0' };

					const auto del_w = ImGui::CalcTextSize(IMGUI_ICON_ABORT).x + (2.0f * ImGui::GetStyle().FramePadding.x);
					const auto path_w = size - (del_w + ImGui::GetStyle().ItemInnerSpacing.x);
					ImGuiEx::FormatStringEllipsis(path_w - (2.0f * ImGui::GetStyle().FramePadding.x), false, buffer2, std::size(buffer2), "%s", WstringToString(value.texture_path).c_str());

					ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
					ImFormatString(buffer, std::size(buffer), "%s###FileSelect", buffer2);
					if (ImGui::Button(buffer, ImVec2(path_w, ImGui::GetFrameHeight())))
					{
						refresh = FileRefreshMode::Path;
						changed = true;
					}

					ImGui::PopStyleColor();
					RenderPreviewTooltip();

					ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);

					ImGui::BeginDisabled(value.texture_path.empty());
					if (ImGui::Button(IMGUI_ICON_ABORT "##Remove", ImVec2(del_w, ImGui::GetFrameHeight())))
					{
						refresh = FileRefreshMode::Clear;
						changed = true;
					}

					ImGui::EndDisabled();
					return changed;
				}

				bool RenderFormatSelect(float size)
				{
					char buffer[512] = { '\0' };
					bool changed = false;

					ImGui::SetNextItemWidth(size);
					if (ImGui::BeginCombo("##Format", "Format"))
					{
						for (unsigned index = 0; index < (unsigned)Device::TexResourceFormat::NumTexResourceFormat; ++index)
						{
							ImFormatString(buffer, std::size(buffer), "%s##%u", Utility::TexResourceFormats[index].display_name.c_str(), index);
							if (ImGui::Selectable(buffer, value.format == Device::TexResourceFormat(index)))
							{
								value.format = Device::TexResourceFormat(index);
								refresh = FileRefreshMode::Export;
								changed = true;
							}
						}

						ImGui::EndCombo();
					}

					return changed;
				}

				bool RenderSRGBSelect(float size)
				{
					bool changed = false;
					ImGui::BeginGroup();
					const auto pos = ImGui::GetCursorPos();
					ImGui::Dummy(ImVec2(size, 0.0f));
					ImGui::SameLine(0.0f, 0.0f);
					ImGui::SetCursorPos(pos);

					if (ImGui::Checkbox("SRGB", &value.srgb))
						changed = true;

					ImGui::EndGroup();
					return changed;
				}

			public:
				TextureParameter(GraphType type, void* param_ptr, const ParamNames& names, const ParamRanges& mins, const ParamRanges& maxs, unsigned index) : UIParameter(param_ptr, names, mins, maxs, index), type(type) {}

				bool OnRender(float total_width, size_t num_params, bool show_icon, bool horizontal_layout) override
				{
					refresh = FileRefreshMode::None;

					bool changed = false;
					char buffer[512] = { '\0' };
					char buffer2[512] = { '\0' };

					ImGui::PushID(int(id));
					ImGui::BeginGroup();

					const float preview_size = ImGui::GetFrameHeight() + ImGui::GetFrameHeightWithSpacing();
					const float srgb_w = ImGui::GetFrameHeight() + ImGui::GetStyle().ItemInnerSpacing.x + ImGui::CalcTextSize("sRGB").x;
					const float format_w = ImGui::GetFrameHeight() + ImGui::GetStyle().ItemSpacing.x + ImGui::CalcTextSize("Format").x + (2.0f * ImGui::GetStyle().FramePadding.x);
					const float big_spacing = ImGui::GetStyle().ItemSpacing.x;
					const float small_spacing = ImGui::GetStyle().ItemInnerSpacing.x;

					if (type != GraphType::Texture)
					{
						ImGui::BeginGroup();

						if (RenderFileSelect(total_width))
							changed = true;

						if (RenderFormatSelect(total_width - (srgb_w + small_spacing)))
							changed = true;

						ImGui::SameLine(0.0f, small_spacing);
						if (RenderSRGBSelect(srgb_w))
							changed = true;

						ImGui::EndGroup();
					}
					else if (total_width > preview_size + srgb_w + format_w + big_spacing + small_spacing)
					{
						RenderPreview(preview_size);
						ImGui::SameLine(0.0f, big_spacing);
					
						const float remaining_w = total_width - (preview_size + big_spacing);
						ImGui::BeginGroup();

						if (RenderFileSelect(remaining_w))
							changed = true;
						
						if (RenderFormatSelect(remaining_w - (srgb_w + small_spacing)))
							changed = true;

						ImGui::SameLine(0.0f, small_spacing);
						if (RenderSRGBSelect(srgb_w))
							changed = true;
					
						ImGui::EndGroup();
					}
					else
					{
						ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
						ImGui::BeginDisabled(value.texture_path.empty());
						if (ImGui::ArrowButtonEx("###Collapse", preview && !value.texture_path.empty() ? ImGuiDir_Down : ImGuiDir_Right, ImVec2(ImGui::GetFrameHeight(), ImGui::GetFrameHeight())))
							preview = !preview;

						ImGui::EndDisabled();
						ImGui::PopStyleColor();

						ImGui::SameLine(0.0f, small_spacing);
						if (RenderFileSelect(total_width - (ImGui::GetFrameHeight() + small_spacing)))
							changed = true;

						if (preview)
						{
							if (RenderFormatSelect(total_width - (srgb_w + small_spacing)))
								changed = true;

							ImGui::SameLine(0.0f, small_spacing);
							if (RenderSRGBSelect(srgb_w))
								changed = true;

							RenderPreview(std::min(total_width, preview_size));
						}
					}

					ImGui::EndGroup();
					CustomTooltip(layout.tooltip, layout.icon);

					ImGui::PopID();
					return changed;
				}

				bool OnRenderLayoutEditor() override
				{
					char buffer[512] = { '\0' };
					bool changed = false;
					const auto total_width = ImGui::GetContentRegionAvail().x;
					const float spacing = ImGui::GetStyle().ItemSpacing.x;

					ImGui::PushID(int(id));
					ImGui::BeginGroup();
					ImGui::PushItemWidth(total_width);

					ImGui::Text("Icon:");
					ImGui::SameLine(0.0f, spacing);
					ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
					if (IconCombo(&layout.icon))
						changed = true;

					ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
					if (PrioritySelect(&layout.priority))
						changed = true;

					ImGui::Text("Tooltip:");
					ImFormatString(buffer, std::size(buffer), "%s", layout.tooltip.c_str());
					ImGui::InputTextMultiline("##Tooltip", buffer, std::size(buffer), ImVec2(0.0f, ImGui::GetFrameHeightWithSpacing() + ImGui::GetFrameHeight()), ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CtrlEnterForNewLine);
					if (ImGui::IsItemDeactivatedAfterEdit() && !ImGui::IsKeyReleased(ImGuiKey_Escape))
					{
						layout.tooltip = buffer;
						changed = true;
					}

					if (ImGui::Checkbox("Merge", &layout.merge))
						changed = true;

					if (ImGui::IsItemHovered())
					{
						ImGui::BeginTooltip();
						ImGui::PushTextWrapPos(35.0f * ImGui::GetFontSize());
						ImGui::Text("Merge");
						ImGui::Separator();
						ImGui::TextWrapped("If enabled, all parameters with this name and type will be merged into a single parameter in the Parameters Editor window.");
						ImGui::PopTextWrapPos();
						ImGui::EndTooltip();
					}

					ImGui::PopItemWidth();
					ImGui::EndGroup();
					ImGui::PopID();

					return changed;
				}

				std::optional<std::string_view> GetIcon() const override { return TryGetIcon(layout.icon); }
				bool HasLayout() const override { return true; }
				bool HasEditor() const override { return true; }
				bool PreferHorizontalDisplay() const override { return false; }
				int GetPriority() const override { return layout.priority; }
				GraphType GetType() const override { return type; }

				std::optional<uint64_t> GetMergeHash() const override
				{
					if (!layout.merge)
						return std::nullopt;

					uint64_t h[] = { uint64_t(GetType()) };
					return MurmurHash2_64(h, int(sizeof(h)), 0x1337B33F);
				}

				void GetValue(JsonValue& param_obj, JsonAllocator& allocator) const override
				{
					Renderer::DrawCalls::ParameterUtil::Save(param_obj, allocator, value, true);
				}

				void SetValue(const JsonValue& param_obj) override
				{
					Renderer::DrawCalls::ParameterUtil::Read(param_obj, value);
				}

				void GetLayout(JsonValue& value, JsonAllocator& allocator) const override
				{
					SaveNewLayout(value, allocator);
				}

				void SetLayout(const JsonValue& value) override
				{
					layout = Layout();

					if (value.HasMember(L"layout"))
						LoadNewLayout(value);
				}

				FileRefreshMode GetFileRefreshMode() const override { return refresh; }
			};

			class SamplerParameter : public UIParameter
			{
			private:
				std::string value;

			public:
				SamplerParameter(void* param_ptr, const ParamNames& names, const ParamRanges& mins, const ParamRanges& maxs, unsigned index) : UIParameter(param_ptr, names, mins, maxs, index) {}

				bool OnRender(float total_width, size_t num_params, bool show_icon, bool horizontal_layout) override
				{
					if (is_instance)
						return false;

					bool changed = false;
					char buffer[512] = { '\0' };
					char buffer2[512] = { '\0' };

					ImGui::PushID(int(id));
					ImGui::BeginGroup();

					FormatParameterName(buffer, std::size(buffer), GetParameterName(names, 0), NoIcon, num_params > 1, false);

					if (buffer[0] != '\0')
					{
						const auto w = ImGui::CalcTextSize(buffer).x;
						ImGui::Text("%s", buffer);
						ImGui::SameLine(0.0f, ImGui::GetStyle().ItemSpacing.x);
						ImGui::SetNextItemWidth(total_width - (w + ImGui::GetStyle().ItemSpacing.x));
					}
					else
						ImGui::SetNextItemWidth(total_width);

					if (ImGui::BeginCombo("##SamplerCombo", value.c_str()))
					{
						const auto& samplers = Renderer::GetGlobalSamplersReader().GetSamplers();
						for (unsigned index = 0; index < samplers.size(); ++index)
						{
							if (samplers[index].engine_only)
								continue;

							ImFormatString(buffer, std::size(buffer), "%s##%u", samplers[index].name.c_str(), index);
							if (ImGui::Selectable(buffer, samplers[index].name == value))
							{
								value = samplers[index].name;
								changed = true;
							}
						}

						ImGui::EndCombo();
					}

					ImGui::EndGroup();
					ImGui::PopID();
					return changed;
				}

				bool OnRenderLayoutEditor() override { return false; }

				std::optional<std::string_view> GetIcon() const override { return std::nullopt; }
				bool HasLayout() const override { return false; }
				bool HasEditor() const override { return !is_instance; }
				bool PreferHorizontalDisplay() const override { return false; }
				int GetPriority() const override { return -INT_MAX; }
				GraphType GetType() const override { return GraphType::Sampler; }

				std::optional<uint64_t> GetMergeHash() const override { return std::nullopt; }

				void GetValue(JsonValue& param_obj, JsonAllocator& allocator) const override
				{
					Renderer::DrawCalls::ParameterUtil::Save(param_obj, allocator, value, "");
				}

				void SetValue(const JsonValue& data_stream) override
				{
					Renderer::DrawCalls::ParameterUtil::Read(data_stream, value);
				}

				void GetLayout(JsonValue& value, JsonAllocator& allocator) const override {}

				void SetLayout(const JsonValue& value) override {}

				FileRefreshMode GetFileRefreshMode() const override { return FileRefreshMode::None; }
			};

			class EnumParameter : public UIParameter
			{
			private:
				struct Layout
				{
					unsigned min_value = 0;
					unsigned max_value = 0;
					bool disable_min = false;
					bool disable_max = false;
					Memory::FlatSet<unsigned, Memory::Tag::Unknown> enabled;
				};

				unsigned value;
				Layout layout;

				void LoadNewLayout(const JsonValue& value)
				{
					const auto& range = value[L"layout"];
					if (range.IsObject())
					{
						if (range.HasMember(L"min"))
							layout.min_value = range[L"min"].GetUint();

						if (range.HasMember(L"max"))
							layout.max_value = range[L"max"].GetUint();

						layout.disable_min = range.HasMember(L"disable_min") && range[L"disable_min"].GetBool();
						layout.disable_max = range.HasMember(L"disable_max") && range[L"disable_max"].GetBool();

						if (range.HasMember(L"enabled"))
							for (const auto& value : range[L"enabled"].GetArray())
								layout.enabled.insert(value.GetUint());
					}
				}

				void SaveNewLayout(JsonValue& value, JsonAllocator& allocator) const
				{
					JsonValue range(rapidjson::kObjectType);
					bool has_layout = false;

					if (layout.min_value != 0)
					{
						range.AddMember(L"min", JsonValue().SetUint(layout.min_value), allocator);
						has_layout = true;
					}

					if (layout.max_value != 0)
					{
						range.AddMember(L"max", JsonValue().SetUint(layout.max_value), allocator);
						has_layout = true;
					}

					if (layout.disable_min)
					{
						range.AddMember(L"disable_min", JsonValue().SetBool(layout.disable_min), allocator);
						has_layout = true;
					}

					if (layout.disable_max)
					{
						range.AddMember(L"disable_max", JsonValue().SetBool(layout.disable_max), allocator);
						has_layout = true;
					}

					JsonValue enabled_range(rapidjson::kArrayType);
					for (const auto& a : layout.enabled)
						enabled_range.PushBack(JsonValue().SetUint(a), allocator);

					if (!layout.enabled.empty())
					{
						range.AddMember(L"enabled", enabled_range, allocator);
						has_layout = true;
					}

					if (has_layout)
						value.AddMember(L"layout", range, allocator);
				}

			public:
				EnumParameter(void* param_ptr, const ParamNames& names, const ParamRanges& mins, const ParamRanges& maxs, unsigned index) : UIParameter(param_ptr, names, mins, maxs, index) {}

				bool OnRender(float total_width, size_t num_params, bool show_icon, bool horizontal_layout) override
				{
					if (names.empty() || layout.max_value == 0)
						return false;

					bool changed = false;
					char buffer[512] = { '\0' };
					char buffer2[512] = { '\0' };

					ImGui::PushID(int(id));
					ImGui::BeginGroup();

					const auto min_r = layout.min_value;
					const auto max_r = std::min(unsigned(names.size()), layout.max_value);

					if (value < min_r)
						value = min_r;

					if (value >= max_r)
						value = max_r - 1;

					const auto value_name = GetParameterName(names, value);
					ImFormatString(buffer, std::size(buffer), "%.*s", unsigned(value_name.length()), value_name.data());
					ImGui::SetNextItemWidth(total_width);
					if (ImGui::BeginCombo("##Combo", buffer))
					{
						for (unsigned a = min_r; a < max_r; a++)
						{
							if (layout.enabled.find(a) == layout.enabled.end())
								continue;

							ImGui::BeginDisabled((a == min_r && layout.disable_min) || (a + 1 == max_r && layout.disable_max));
							const auto c_name = GetParameterName(names, a);
							ImFormatString(buffer, std::size(buffer), "%.*s##%u", unsigned(c_name.length()), c_name.data(), a);
							if (ImGui::Selectable(buffer, a == value))
							{
								value = a;
								changed = true;
							}

							ImGui::EndDisabled();
						}

						ImGui::EndCombo();
					}

					ImGui::EndGroup();
					ImGui::PopID();
					return changed;
				}

				bool OnRenderLayoutEditor() override { return false; }

				std::optional<std::string_view> GetIcon() const override { return std::nullopt; }
				bool HasLayout() const override { return true; }
				bool HasEditor() const override { return true; }
				bool PreferHorizontalDisplay() const override { return false; }
				int GetPriority() const override { return -INT_MAX; }
				GraphType GetType() const override { return GraphType::Enum; }

				std::optional<uint64_t> GetMergeHash() const override { return std::nullopt; }

				void GetValue(JsonValue& param_obj, JsonAllocator& allocator) const override
				{
					int t = int(value);
					Renderer::DrawCalls::ParameterUtil::Save(param_obj, allocator, t, ~t);
				}

				void SetValue(const JsonValue& data_stream) override
				{
					int t = 0;
					Renderer::DrawCalls::ParameterUtil::Read(data_stream, t);
					value = unsigned(t);
				}

				void GetLayout(JsonValue& value, JsonAllocator& allocator) const override
				{
					SaveNewLayout(value, allocator);
				}

				void SetLayout(const JsonValue& value) override
				{
					layout = Layout();

					if (value.HasMember(L"layout"))
						LoadNewLayout(value);
				}

				FileRefreshMode GetFileRefreshMode() const override { return FileRefreshMode::None; }
			};

			template<size_t N> class SplineParameter : public UIParameter
			{
				static_assert(N == 5, "Invalid SplineParameter Type");

			private:
				typedef simd::GenericCurve<N> Curve;

				struct Layout
				{
					size_t unit = NoUnit;
					size_t icon = NoIcon;
					int priority = 0;
					Memory::DebugStringA<64> custom_unit;
					Memory::DebugStringA<512> tooltip;
					float default_value = 0.0f;
					bool merge = false;
					bool allow_negative = true;
					bool allow_variance = true;
				};

				Curve value;
				size_t unit;
				Layout layout;

				void LoadOldLayout(const JsonValue& value)
				{
					const auto& obj = value[L"custom_range"];
					if (!obj.IsNull())
					{
						layout.allow_negative = !obj.HasMember(L"allow_negative") || obj[L"allow_negative"].GetBool();
						layout.allow_variance = !obj.HasMember(L"allow_variance") || obj[L"allow_variance"].GetBool();
					}
				}

				void LoadNewLayout(const JsonValue& value)
				{
					const auto& range = value[L"layout"];
					if (range.IsObject())
					{
						if (range.HasMember(L"unit"))
						{
							const auto unit = WstringToString(range[L"unit"].GetString());
							if (auto uid = FindUnitID(unit); uid < CustomUnit)
								layout.unit = uid;
							else
								layout.custom_unit = unit;
						}

						if (range.HasMember(L"icon"))
							if (auto icon = range[L"icon"].GetUint(); icon < std::size(Icons))
								layout.icon = size_t(icon) + 1;

						if (range.HasMember(L"priority"))
							layout.priority = range[L"priority"].GetInt();

						if (range.HasMember(L"default_value"))
							layout.default_value = range[L"default_value"].GetFloat();

						layout.merge = range.HasMember(L"merge") && range[L"merge"].GetBool();
						layout.allow_negative = !range.HasMember(L"allow_negative") || range[L"allow_negative"].GetBool();
						layout.allow_variance = !range.HasMember(L"allow_variance") || range[L"allow_variance"].GetBool();

						if (range.HasMember(L"tooltip"))
							layout.tooltip = WstringToString(range[L"tooltip"].GetString());

						if (!layout.allow_negative)
							layout.default_value = std::max(0.0f, layout.default_value);
					}
				}

				void SaveNewLayout(JsonValue& value, JsonAllocator& allocator) const
				{
					JsonValue range(rapidjson::kObjectType);
					bool has_layout = false;

					if ((layout.unit < CustomUnit || layout.custom_unit.length() > 0) && layout.unit != NoUnit)
					{
						range.AddMember(L"unit", CreateStringValue(layout.unit >= CustomUnit ? std::string_view(layout.custom_unit) : Units[layout.unit].name, allocator), allocator);
						has_layout = true;
					}

					if (layout.priority != 0)
					{
						range.AddMember(L"priority", layout.priority, allocator);
						has_layout = true;
					}

					if (layout.icon != NoIcon)
					{
						range.AddMember(L"icon", JsonValue().SetUint(unsigned(layout.icon - 1)), allocator);
						has_layout = true;
					}

					if (layout.default_value != 0.0f)
					{
						range.AddMember(L"default_value", layout.default_value, allocator);
						has_layout = true;
					}

					if (!layout.allow_negative)
					{
						range.AddMember(L"allow_negative", false, allocator);
						has_layout = true;
					}

					if (!layout.allow_variance)
					{
						range.AddMember(L"allow_variance", false, allocator);
						has_layout = true;
					}

					if (layout.merge)
					{
						range.AddMember(L"merge", true, allocator);
						has_layout = true;
					}

					if (layout.tooltip.length() > 0)
					{
						range.AddMember(L"tooltip", CreateStringValue(layout.tooltip, allocator), allocator);
						has_layout = true;
					}

					if (has_layout)
						value.AddMember(L"layout", range, allocator);
				}

				Curve ConvertCurve(const Curve& src, size_t from, size_t to)
				{
					Curve tmp;
					tmp.variance = ConvertUnit(src.variance, from, to, false);
					for (const auto& point : src.points)
					{
						tmp.points.emplace_back();
						tmp.points.back().left_dt = ConvertUnit(point.left_dt, from, to, false);
						tmp.points.back().right_dt = ConvertUnit(point.right_dt, from, to, false);
						tmp.points.back().value = ConvertUnit(point.value, from, to);
						tmp.points.back().time = point.time;
						tmp.points.back().type = point.type;
					}

					return tmp;
				}

			public:
				SplineParameter(void* param_ptr, const ParamNames& names, const ParamRanges& mins, const ParamRanges& maxs, unsigned index) : UIParameter(param_ptr, names, mins, maxs, index) {}

				bool OnRender(float total_width, size_t num_params, bool show_icon, bool horizontal_layout) override
				{
					bool changed = false;
					char buffer[512] = { '\0' };
					char buffer2[512] = { '\0' };

					ImGui::PushID(int(id));
					ImGui::BeginGroup();

					if (UnitGroup(unit) != UnitGroup(layout.unit))
						unit = layout.unit;

					const float unit_selection_width = unit < CustomUnit ? CalcUnitComboWidth(true, layout.unit) : 0.0f;

					Curve tmp = ConvertCurve(value, layout.unit, unit);

					int flags = CurveControlFlags_None;
					if (!layout.allow_negative)
						flags |= CurveControlFlags_NoNegative;

					if (layout.allow_variance)
						flags |= CurveControlFlags_Variance;

					FormatParameterName(buffer, std::size(buffer), GetParameterName(names, 0), layout.icon, true, show_icon);
					AppendUnit(buffer2, std::size(buffer2), buffer, unit, layout.custom_unit, true);
					ImFormatString(buffer, std::size(buffer), "%s##Spline%u", buffer2, unsigned(N));

					const float spline_w = total_width;
					const float spline_h = horizontal_layout ? ImGui::GetContentRegionAvail().y : std::min(spline_w, 6 * ImGui::GetFrameHeightWithSpacing());
					if (CurveControl(buffer2, tmp, ImVec2(spline_w, spline_h), layout.default_value, flags, unit_selection_width))
					{
						value = ConvertCurve(tmp, unit, layout.unit);
						changed = true;
					}

					if (unit_selection_width > 0.0f)
					{
						ImGui::SameLine(0.0f, 0.0f);
						ImGui::SetCursorPosX(ImGui::GetCursorPosX() - unit_selection_width);
						ImGui::SetNextItemWidth(unit_selection_width);
						if (UnitCombo(&unit, true, layout.unit))
						{
							value = ConvertCurve(ConvertCurve(value, layout.unit, unit), unit, layout.unit);
							changed = true;
						}
					}

					ImGui::EndGroup();
					CustomTooltip(layout.tooltip, layout.icon);

					ImGui::PopID();
					return changed;
				}

				bool OnRenderLayoutEditor() override
				{
					char buffer[512] = { '\0' };
					bool changed = false;
					const auto total_width = ImGui::GetContentRegionAvail().x;
					const float spacing = ImGui::GetStyle().ItemSpacing.x;

					ImGui::PushID(int(id));
					ImGui::BeginGroup();
					ImGui::PushItemWidth(total_width);

					float text_w = std::max({ ImGui::CalcTextSize("Unit").x, ImGui::CalcTextSize("Icon").x, ImGui::CalcTextSize("Default").x });

					ImGui::Text("Icon");
					ImGui::SameLine(text_w, spacing);
					ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
					if (IconCombo(&layout.icon))
						changed = true;

					ImGui::Text("Unit");
					ImGui::SameLine(text_w, spacing);
					ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
					if (UnitCombo(&layout.unit, false))
					{
						layout.custom_unit = "";
						changed = true;
					}

					UnitSelectTooltip();

					if (layout.unit >= CustomUnit)
					{
						ImFormatString(buffer, std::size(buffer), "%s", layout.custom_unit.c_str());
						ImGui::InputTextWithHint("##CustomUnit", "Custom Unit", buffer, std::min(std::size(buffer), layout.custom_unit.max_size() + 1), ImGuiInputTextFlags_EnterReturnsTrue);
						if (ImGui::IsItemDeactivatedAfterEdit() && !ImGui::IsKeyReleased(ImGuiKey_Escape))
						{
							layout.custom_unit = buffer;
							changed = true;
						}
					}

					ImGui::Text("Default");
					ImGui::SameLine(text_w, spacing);
					ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);

					if (!layout.allow_negative && layout.default_value < 0.0f)
					{
						layout.default_value = 0.0f;
						changed = true;
					}

					AppendUnit(buffer, std::size(buffer), "%.3f", layout.unit, layout.custom_unit);
					if (ImGui::DragFloat("##DefaultValue", &layout.default_value, 1.0f, layout.allow_negative ? -FLT_MAX : 0.0f, FLT_MAX, buffer))
					{
						if (!layout.allow_negative)
							layout.default_value = std::max(layout.default_value, 0.0f);

						changed = true;
					}

					ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
					if (PrioritySelect(&layout.priority))
						changed = true;

					ImGui::Text("Tooltip:");
					ImFormatString(buffer, std::size(buffer), "%s", layout.tooltip.c_str());
					ImGui::InputTextMultiline("##Tooltip", buffer, std::size(buffer), ImVec2(0.0f, ImGui::GetFrameHeightWithSpacing() + ImGui::GetFrameHeight()), ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CtrlEnterForNewLine);
					if (ImGui::IsItemDeactivatedAfterEdit() && !ImGui::IsKeyReleased(ImGuiKey_Escape))
					{
						layout.tooltip = buffer;
						changed = true;
					}

					if (ImGui::Checkbox("Allow Negative", &layout.allow_negative))
					{
						layout.default_value = std::max(layout.default_value, 0.0f);
						changed = true;
					}

					if (ImGui::Checkbox("Allow Variance", &layout.allow_variance))
						changed = true;

					if (ImGui::Checkbox("Merge", &layout.merge))
						changed = true;

					if (ImGui::IsItemHovered())
					{
						ImGui::BeginTooltip();
						ImGui::PushTextWrapPos(35.0f * ImGui::GetFontSize());
						ImGui::Text("Merge");
						ImGui::Separator();
						ImGui::TextWrapped("If enabled, all parameters with this name and type will be merged into a single parameter in the Parameters Editor window.");
						ImGui::PopTextWrapPos();
						ImGui::EndTooltip();
					}

					ImGui::PopItemWidth();
					ImGui::EndGroup();
					ImGui::PopID();

					return changed;
				}

				std::optional<std::string_view> GetIcon() const override { return TryGetIcon(layout.icon); }
				bool HasLayout() const override { return true; }
				bool HasEditor() const override { return true; }
				bool PreferHorizontalDisplay() const override { return true; }
				int GetPriority() const override { return layout.priority; }
				GraphType GetType() const override { return GraphType::Spline5; }
				
				std::optional<uint64_t> GetMergeHash() const override
				{
					if (!layout.merge)
						return std::nullopt;

					uint64_t h[] = { uint64_t(GetType()) };
					return MurmurHash2_64(h, int(sizeof(h)), 0x1337B33F);
				}

				void GetValue(JsonValue& param_obj, JsonAllocator& allocator) const override
				{
					Renderer::DrawCalls::ParameterUtil::Save(param_obj, allocator, value);

					param_obj.AddMember(L"unit", CreateStringValue(unit >= CustomUnit ? std::string_view(layout.custom_unit) : Units[unit].name, allocator), allocator);
				}

				void SetValue(const JsonValue& data_stream) override
				{
					Renderer::DrawCalls::ParameterUtil::Read(data_stream, value);

					unit = FindDefaultUnit(layout.unit);
					if (data_stream.HasMember(L"unit"))
					{
						const auto unit_name = WstringToString(data_stream[L"unit"].GetString());
						if (auto uid = FindUnitID(unit_name); uid < CustomUnit && UnitGroup(uid) == UnitGroup(layout.unit))
							unit = uid;
					}
				}

				void GetLayout(JsonValue& value, JsonAllocator& allocator) const override
				{
					SaveNewLayout(value, allocator);
				}

				void SetLayout(const JsonValue& value) override
				{
					layout = Layout();

					if (value.HasMember(L"custom_range"))
						LoadOldLayout(value);
					else if (value.HasMember(L"layout"))
						LoadNewLayout(value);

					if (UnitGroup(unit) != UnitGroup(layout.unit))
						unit = FindDefaultUnit(layout.unit);
				}

				FileRefreshMode GetFileRefreshMode() const override { return FileRefreshMode::None; }
			};

			class SplineColourParameter : public UIParameter
			{
			private:
				struct Layout
				{
					size_t icon = NoIcon;
					int priority = 0;
					Memory::DebugStringA<512> tooltip;
					bool merge = false;
				};

				LUT::Spline value;
				Layout layout;

				void LoadNewLayout(const JsonValue& value)
				{
					const auto& range = value[L"layout"];
					if (range.IsObject())
					{
						if (range.HasMember(L"icon"))
							if (auto icon = range[L"icon"].GetUint(); icon < std::size(Icons))
								layout.icon = size_t(icon) + 1;

						if (range.HasMember(L"priority"))
							layout.priority = range[L"priority"].GetInt();

						layout.merge = range.HasMember(L"merge") && range[L"merge"].GetBool();

						if (range.HasMember(L"tooltip"))
							layout.tooltip = WstringToString(range[L"tooltip"].GetString());
					}
				}

				void SaveNewLayout(JsonValue& value, JsonAllocator& allocator) const
				{
					JsonValue range(rapidjson::kObjectType);
					bool has_layout = false;

					if (layout.icon != NoIcon)
					{
						range.AddMember(L"icon", JsonValue().SetUint(unsigned(layout.icon - 1)), allocator);
						has_layout = true;
					}

					if (layout.priority != 0)
					{
						range.AddMember(L"priority", layout.priority, allocator);
						has_layout = true;
					}

					if (layout.merge)
					{
						range.AddMember(L"merge", true, allocator);
						has_layout = true;
					}

					if (layout.tooltip.length() > 0)
					{
						range.AddMember(L"tooltip", CreateStringValue(layout.tooltip, allocator), allocator);
						has_layout = true;
					}

					if (has_layout)
						value.AddMember(L"layout", range, allocator);
				}

			public:
				SplineColourParameter(void* param_ptr, const ParamNames& names, const ParamRanges& mins, const ParamRanges& maxs, unsigned index) : UIParameter(param_ptr, names, mins, maxs, index) {}

				bool OnRender(float total_width, size_t num_params, bool show_icon, bool horizontal_layout) override
				{
					bool changed = false;
					char buffer[512] = { '\0' };
					char buffer2[512] = { '\0' };

					ImGui::PushID(int(id));
					ImGui::BeginGroup();

					ImGui::SetNextItemWidth(total_width);
					if (CurveColorControl("RGBA", value.r, value.g, value.b, value.a))
						changed = true;

					FormatParameterName(buffer, std::size(buffer), GetParameterName(names, 0), layout.icon, true, show_icon);
					ImFormatString(buffer2, std::size(buffer2), "%s###Value", buffer);

					const float spline_w = total_width;
					const float spline_h = horizontal_layout ? ImGui::GetContentRegionAvail().y : std::min(spline_w, 6 * ImGui::GetFrameHeightWithSpacing());
					CurveSelector select_color(buffer2, 4, ImVec2(spline_w, spline_h), 3);
					while (select_color.Step())
					{
						if (CurveControl("R", value.r, ImVec2(0.0f, 0.0f), 1.0f, CurveControlFlags_NoNegative))
							changed = true;

						if (CurveControl("G", value.g, ImVec2(0.0f, 0.0f), 1.0f, CurveControlFlags_NoNegative))
							changed = true;

						if (CurveControl("B", value.b, ImVec2(0.0f, 0.0f), 1.0f, CurveControlFlags_NoNegative))
							changed = true;

						if (CurveControl("A", value.a, ImVec2(0.0f, 0.0f), 1.0f, CurveControlFlags_NoNegative))
							changed = true;
					}

					ImGui::EndGroup();
					CustomTooltip(layout.tooltip, layout.icon);

					ImGui::PopID();
					return changed;
				}

				bool OnRenderLayoutEditor() override
				{
					char buffer[512] = { '\0' };
					bool changed = false;
					const auto total_width = ImGui::GetContentRegionAvail().x;
					const float spacing = ImGui::GetStyle().ItemSpacing.x;

					ImGui::PushID(int(id));
					ImGui::BeginGroup();
					ImGui::PushItemWidth(total_width);

					ImGui::Text("Icon:");
					ImGui::SameLine(0.0f, spacing);
					ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
					if (IconCombo(&layout.icon))
						changed = true;

					ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
					if (PrioritySelect(&layout.priority))
						changed = true;

					ImGui::Text("Tooltip:");
					ImFormatString(buffer, std::size(buffer), "%s", layout.tooltip.c_str());
					ImGui::InputTextMultiline("##Tooltip", buffer, std::size(buffer), ImVec2(0.0f, ImGui::GetFrameHeightWithSpacing() + ImGui::GetFrameHeight()), ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CtrlEnterForNewLine);
					if (ImGui::IsItemDeactivatedAfterEdit() && !ImGui::IsKeyReleased(ImGuiKey_Escape))
					{
						layout.tooltip = buffer;
						changed = true;
					}

					if (ImGui::Checkbox("Merge", &layout.merge))
						changed = true;

					if (ImGui::IsItemHovered())
					{
						ImGui::BeginTooltip();
						ImGui::PushTextWrapPos(35.0f * ImGui::GetFontSize());
						ImGui::Text("Merge");
						ImGui::Separator();
						ImGui::TextWrapped("If enabled, all parameters with this name and type will be merged into a single parameter in the Parameters Editor window.");
						ImGui::PopTextWrapPos();
						ImGui::EndTooltip();
					}

					ImGui::PopItemWidth();
					ImGui::EndGroup();
					ImGui::PopID();

					return changed;
				}

				std::optional<std::string_view> GetIcon() const override { return TryGetIcon(layout.icon); }
				bool HasLayout() const override { return true; }
				bool HasEditor() const override { return true; }
				bool PreferHorizontalDisplay() const override { return true; }
				int GetPriority() const override { return layout.priority; }
				GraphType GetType() const override { return GraphType::SplineColour; }

				std::optional<uint64_t> GetMergeHash() const override
				{
					if (!layout.merge)
						return std::nullopt;

					uint64_t h[] = { uint64_t(GetType()) };
					return MurmurHash2_64(h, int(sizeof(h)), 0x1337B33F);
				}

				void GetValue(JsonValue& param_obj, JsonAllocator& allocator) const override
				{
					Renderer::DrawCalls::ParameterUtil::Save(param_obj, allocator, value);
				}

				void SetValue(const JsonValue& data_stream) override
				{
					Renderer::DrawCalls::ParameterUtil::Read(data_stream, value);
				}

				void GetLayout(JsonValue& value, JsonAllocator& allocator) const override
				{
					SaveNewLayout(value, allocator);
				}

				void SetLayout(const JsonValue& value) override
				{
					layout = Layout();

					if (value.HasMember(L"layout"))
						LoadNewLayout(value);
				}

				FileRefreshMode GetFileRefreshMode() const override { return FileRefreshMode::None; }
			};

			class TextParameter : public UIParameter
			{
			private:
				std::string value;

			public:
				TextParameter(void* param_ptr, const ParamNames& names, const ParamRanges& mins, const ParamRanges& maxs, unsigned index) : UIParameter(param_ptr, names, mins, maxs, index) {}

				bool OnRender(float total_width, size_t num_params, bool show_icon, bool horizontal_layout) override
				{
					if (is_instance)
						return false;

					bool changed = false;
					char buffer[512] = { '\0' };

					ImGui::PushID(int(id));
					ImGui::BeginGroup();

					FormatParameterName(buffer, std::size(buffer), GetParameterName(names, 0), NoIcon, num_params > 1, false);

					if (buffer[0] != '\0')
					{
						const auto w = ImGui::CalcTextSize(buffer).x;

						ImGui::Text("%s", buffer);
						ImGui::SameLine(0.0f, ImGui::GetStyle().ItemSpacing.x);
						ImGui::SetNextItemWidth(total_width - (w + ImGui::GetStyle().ItemSpacing.x));
					}
					else
						ImGui::SetNextItemWidth(total_width);

					ImFormatString(buffer, std::size(buffer), "%s", value.c_str());
					if (ImGui::InputText("##CustomDynamic", buffer, std::size(buffer), ImGuiInputTextFlags_EnterReturnsTrue))
					{
						changed = true;
						value = buffer;
					}

					ImGui::EndGroup();
					ImGui::PopID();
					return changed;
				}

				bool OnRenderLayoutEditor() override { return false; }

				std::optional<std::string_view> GetIcon() const override { return std::nullopt; }
				bool HasLayout() const override { return false; }
				bool HasEditor() const override { return !is_instance; }
				bool PreferHorizontalDisplay() const override { return false; }
				int GetPriority() const override { return -INT_MAX; }
				GraphType GetType() const override { return GraphType::Text; }

				std::optional<uint64_t> GetMergeHash() const override { return std::nullopt; }

				void GetValue(JsonValue& param_obj, JsonAllocator& allocator) const override
				{
					Renderer::DrawCalls::ParameterUtil::Save(param_obj, allocator, value, "");
				}

				void SetValue(const JsonValue& data_stream) override
				{
					Renderer::DrawCalls::ParameterUtil::Read(data_stream, value);
				}

				void GetLayout(JsonValue& value, JsonAllocator& allocator) const override {}

				void SetLayout(const JsonValue& value) override {}

				FileRefreshMode GetFileRefreshMode() const override { return FileRefreshMode::None; }
			};
		}

		std::unique_ptr<UIParameter> CreateParameter(GraphType type, void* param_ptr, const Memory::Vector<std::string, Memory::Tag::EffectGraphParameterInfos>& names, const Renderer::DrawCalls::ParamRanges& mins, const Renderer::DrawCalls::ParamRanges& maxs, unsigned index)
		{
			switch (type)
			{
				case GraphType::Float:			return std::make_unique<FloatParameter<1>>(param_ptr, names, mins, maxs, index);
				case GraphType::Float2:			return std::make_unique<FloatParameter<2>>(param_ptr, names, mins, maxs, index);
				case GraphType::Float3:			return std::make_unique<FloatParameter<3>>(param_ptr, names, mins, maxs, index);
				case GraphType::Float4:			return std::make_unique<FloatParameter<4>>(param_ptr, names, mins, maxs, index);

				case GraphType::Int:			return std::make_unique<IntParameter<int, 1>>(param_ptr, names, mins, maxs, index);
				case GraphType::Int2:			return std::make_unique<IntParameter<int, 2>>(param_ptr, names, mins, maxs, index);
				case GraphType::Int3:			return std::make_unique<IntParameter<int, 3>>(param_ptr, names, mins, maxs, index);
				case GraphType::Int4:			return std::make_unique<IntParameter<int, 4>>(param_ptr, names, mins, maxs, index);

				case GraphType::UInt:			return std::make_unique<IntParameter<unsigned, 1>>(param_ptr, names, mins, maxs, index);

				case GraphType::Spline5:		return std::make_unique<SplineParameter<5>>(param_ptr, names, mins, maxs, index);

				case GraphType::Texture3d:		return std::make_unique<TextureParameter>(type, param_ptr, names, mins, maxs, index);
				case GraphType::TextureCube:	return std::make_unique<TextureParameter>(type, param_ptr, names, mins, maxs, index);
				case GraphType::Texture:		return std::make_unique<TextureParameter>(type, param_ptr, names, mins, maxs, index);

				case GraphType::Bool:			return std::make_unique<BoolParameter>(param_ptr, names, mins, maxs, index);
				case GraphType::Sampler:		return std::make_unique<SamplerParameter>(param_ptr, names, mins, maxs, index);
				case GraphType::Enum:			return std::make_unique<EnumParameter>(param_ptr, names, mins, maxs, index);
				case GraphType::SplineColour:	return std::make_unique<SplineColourParameter>(param_ptr, names, mins, maxs, index);
				case GraphType::Text:			return std::make_unique<TextParameter>(param_ptr, names, mins, maxs, index);

				default:						return std::make_unique<EmptyParameter>(type, param_ptr, names, mins, maxs, index);
			}
		}
	}
}