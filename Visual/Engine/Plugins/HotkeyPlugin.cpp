#include "HotkeyPlugin.h"
#include "Visual/GUI2/imgui/imgui.h"
#include "Visual/GUI2/imgui/imgui_ex.h"
#include "Visual/GUI2/Icons.h"
#include "Visual/Device/Constants.h"
#include "Visual/Utility/JsonUtility.h"

namespace Engine::Plugins
{
#if DEBUG_GUI_ENABLED
	namespace
	{
		void FormatKeyLabel(char* buffer, size_t buffer_size, const Input::KeyModifier& key)
		{
			if (key.first == 0)
			{
				ImFormatString(buffer, buffer_size, "< None >");
				return;
			}

			char mod_shift[32] = { '\0' };
			char mod_ctrl[32] = { '\0' };
			char mod_alt[32] = { '\0' };
			if (key.second & Input::Modifier_Shift)	ImFormatString(mod_shift, std::size(mod_shift), "Shift + ");
			if (key.second & Input::Modifier_Ctrl)	ImFormatString(mod_ctrl, std::size(mod_ctrl), "Ctrl + ");
			if (key.second & Input::Modifier_Alt)	ImFormatString(mod_alt, std::size(mod_alt), "Alt + ");

			switch (key.first)
			{
				case VK_BACK:		ImFormatString(buffer, buffer_size, "%s%s%sBackspace", mod_shift, mod_ctrl, mod_alt);	break;
				case VK_TAB:		ImFormatString(buffer, buffer_size, "%s%s%sTab", mod_shift, mod_ctrl, mod_alt);			break;
				case VK_CLEAR:		ImFormatString(buffer, buffer_size, "%s%s%sClear", mod_shift, mod_ctrl, mod_alt);		break;
				case VK_RETURN:		ImFormatString(buffer, buffer_size, "%s%s%sReturn", mod_shift, mod_ctrl, mod_alt);		break;
				case VK_PAUSE:		ImFormatString(buffer, buffer_size, "%s%s%sPause", mod_shift, mod_ctrl, mod_alt);		break;
				case VK_CAPITAL:	ImFormatString(buffer, buffer_size, "%s%s%sCapslock", mod_shift, mod_ctrl, mod_alt);	break;
				case VK_SPACE:		ImFormatString(buffer, buffer_size, "%s%s%sSpacebar", mod_shift, mod_ctrl, mod_alt);	break;
				case VK_PRIOR:		ImFormatString(buffer, buffer_size, "%s%s%sPage Up", mod_shift, mod_ctrl, mod_alt);		break;
				case VK_NEXT:		ImFormatString(buffer, buffer_size, "%s%s%sPage Down", mod_shift, mod_ctrl, mod_alt);	break;
				case VK_END:		ImFormatString(buffer, buffer_size, "%s%s%sEnd", mod_shift, mod_ctrl, mod_alt);			break;
				case VK_HOME:		ImFormatString(buffer, buffer_size, "%s%s%sHome", mod_shift, mod_ctrl, mod_alt);		break;
				case VK_LEFT:		ImFormatString(buffer, buffer_size, "%s%s%sLeft Arrow", mod_shift, mod_ctrl, mod_alt);	break;
				case VK_UP:			ImFormatString(buffer, buffer_size, "%s%s%sUp Arrow", mod_shift, mod_ctrl, mod_alt);	break;
				case VK_RIGHT:		ImFormatString(buffer, buffer_size, "%s%s%sRight Arrow", mod_shift, mod_ctrl, mod_alt);	break;
				case VK_DOWN:		ImFormatString(buffer, buffer_size, "%s%s%sDown Arrow", mod_shift, mod_ctrl, mod_alt);	break;
				case VK_SELECT:		ImFormatString(buffer, buffer_size, "%s%s%sSelect", mod_shift, mod_ctrl, mod_alt);		break;
				case VK_PRINT:		ImFormatString(buffer, buffer_size, "%s%s%sPrint", mod_shift, mod_ctrl, mod_alt);		break;
				case VK_EXECUTE:	ImFormatString(buffer, buffer_size, "%s%s%sExecute", mod_shift, mod_ctrl, mod_alt);		break;
				case VK_SNAPSHOT:	ImFormatString(buffer, buffer_size, "%s%s%sPrint", mod_shift, mod_ctrl, mod_alt);		break;
				case VK_INSERT:		ImFormatString(buffer, buffer_size, "%s%s%sInsert", mod_shift, mod_ctrl, mod_alt);		break;
				case VK_DELETE:		ImFormatString(buffer, buffer_size, "%s%s%sDelete", mod_shift, mod_ctrl, mod_alt);		break;
				case VK_HELP:		ImFormatString(buffer, buffer_size, "%s%s%sHelp", mod_shift, mod_ctrl, mod_alt);		break;
				case VK_SLEEP:		ImFormatString(buffer, buffer_size, "%s%s%sSleep", mod_shift, mod_ctrl, mod_alt);		break;
				case VK_NUMPAD0:	ImFormatString(buffer, buffer_size, "%s%s%sNumpad 0", mod_shift, mod_ctrl, mod_alt);	break;
				case VK_NUMPAD1:	ImFormatString(buffer, buffer_size, "%s%s%sNumpad 1", mod_shift, mod_ctrl, mod_alt);	break;
				case VK_NUMPAD2:	ImFormatString(buffer, buffer_size, "%s%s%sNumpad 2", mod_shift, mod_ctrl, mod_alt);	break;
				case VK_NUMPAD3:	ImFormatString(buffer, buffer_size, "%s%s%sNumpad 3", mod_shift, mod_ctrl, mod_alt);	break;
				case VK_NUMPAD4:	ImFormatString(buffer, buffer_size, "%s%s%sNumpad 4", mod_shift, mod_ctrl, mod_alt);	break;
				case VK_NUMPAD5:	ImFormatString(buffer, buffer_size, "%s%s%sNumpad 5", mod_shift, mod_ctrl, mod_alt);	break;
				case VK_NUMPAD6:	ImFormatString(buffer, buffer_size, "%s%s%sNumpad 6", mod_shift, mod_ctrl, mod_alt);	break;
				case VK_NUMPAD7:	ImFormatString(buffer, buffer_size, "%s%s%sNumpad 7", mod_shift, mod_ctrl, mod_alt);	break;
				case VK_NUMPAD8:	ImFormatString(buffer, buffer_size, "%s%s%sNumpad 8", mod_shift, mod_ctrl, mod_alt);	break;
				case VK_NUMPAD9:	ImFormatString(buffer, buffer_size, "%s%s%sNumpad 9", mod_shift, mod_ctrl, mod_alt);	break;
				case VK_F1:			ImFormatString(buffer, buffer_size, "%s%s%sF 1", mod_shift, mod_ctrl, mod_alt);			break;
				case VK_F2:			ImFormatString(buffer, buffer_size, "%s%s%sF 2", mod_shift, mod_ctrl, mod_alt);			break;
				case VK_F3:			ImFormatString(buffer, buffer_size, "%s%s%sF 3", mod_shift, mod_ctrl, mod_alt);			break;
				case VK_F4:			ImFormatString(buffer, buffer_size, "%s%s%sF 4", mod_shift, mod_ctrl, mod_alt);			break;
				case VK_F5:			ImFormatString(buffer, buffer_size, "%s%s%sF 5", mod_shift, mod_ctrl, mod_alt);			break;
				case VK_F6:			ImFormatString(buffer, buffer_size, "%s%s%sF 6", mod_shift, mod_ctrl, mod_alt);			break;
				case VK_F7:			ImFormatString(buffer, buffer_size, "%s%s%sF 7", mod_shift, mod_ctrl, mod_alt);			break;
				case VK_F8:			ImFormatString(buffer, buffer_size, "%s%s%sF 8", mod_shift, mod_ctrl, mod_alt);			break;
				case VK_F9:			ImFormatString(buffer, buffer_size, "%s%s%sF 9", mod_shift, mod_ctrl, mod_alt);			break;
				case VK_F10:		ImFormatString(buffer, buffer_size, "%s%s%sF 10", mod_shift, mod_ctrl, mod_alt);		break;
				case VK_F11:		ImFormatString(buffer, buffer_size, "%s%s%sF 11", mod_shift, mod_ctrl, mod_alt);		break;
				case VK_F12:		ImFormatString(buffer, buffer_size, "%s%s%sF 12", mod_shift, mod_ctrl, mod_alt);		break;

				#ifdef VK_NUMLOCK
				case VK_NUMLOCK:	ImFormatString(buffer, buffer_size, "%s%s%sNumlock", mod_shift, mod_ctrl, mod_alt);		break;
				#endif

				default:
					if(key.first >= 0x30 && key.first <= 0x39)
						ImFormatString(buffer, buffer_size, "%s%s%s%c", mod_shift, mod_ctrl, mod_alt, char('0' + key.first - 0x30));
					else if(key.first >= 0x41 && key.first <= 0x5A)
						ImFormatString(buffer, buffer_size, "%s%s%s%c", mod_shift, mod_ctrl, mod_alt, char('A' + key.first - 0x41));
					else
						ImFormatString(buffer, buffer_size, "%s%s%s???", mod_shift, mod_ctrl, mod_alt);

					break;
			}
		}
	}

	HotkeyPlugin::HotkeyPlugin() : Plugin(Feature_Visible | Feature_NoDisable)
	{

	}

	void HotkeyPlugin::OnRender(float elapsed_time)
	{
		if (!IsVisible())
			return;

		bool visible = true;
		if (ImGui::Begin("Plugin Hotkeys", &visible))
			RenderHotkeys();

		ImGui::End();

		if (!visible)
			SetVisible(false);
	}

	void HotkeyPlugin::OnReloadSettings(const rapidjson::Value& settings)
	{
		PluginManager::Get().ProcessActions([&](Input::Action* action, const Input::KeyModifier& key)
		{
			if (action->GetPlugin() == nullptr || action->GetPlugin()->GetName().empty() || action->GetName().empty())
				return;

			const auto name = action->GetPlugin()->GetName() + "." + action->GetName();
			if (auto f = settings.FindMember(name.c_str()); f != settings.MemberEnd() && f->value.IsObject())
			{
				if (auto a = f->value.FindMember("Enabled"); a != f->value.MemberEnd() && a->value.IsBool())
					action->SetActive(a->value.GetBool());

				if (auto a = f->value.FindMember("Key"); a != f->value.MemberEnd() && a->value.IsUint())
				{
					const auto k = a->value.GetUint();
					const auto mod = k & 0xFF;
					const auto key = k >> 8;

					if (mod <= Input::Modifier_All && key <= 255)
						PluginManager::Get().SetKeyBinding(action, Input::KeyModifier(uint8_t(key), Input::Modifier(mod)));
				}
			}
		});
	}

	void HotkeyPlugin::OnLoadSettings( const rapidjson::Value& settings )
	{
		OnReloadSettings( settings );
	}

	void HotkeyPlugin::OnSaveSettings(rapidjson::Value& settings, rapidjson::Document::AllocatorType& allocator)
	{
		PluginManager::Get().ProcessActions([&](Input::Action* action, const Input::KeyModifier& key)
		{
			if (action->GetPlugin() == nullptr || action->GetPlugin()->GetName().empty() || action->GetName().empty())
				return;

			const auto name = action->GetPlugin()->GetName() + "." + action->GetName();
			rapidjson::Value save(rapidjson::kObjectType);
			save.AddMember(rapidjson::Value().SetString("Enabled", allocator), rapidjson::Value().SetBool(action->IsActiveFlag()), allocator);
			save.AddMember(rapidjson::Value().SetString("Key", allocator), rapidjson::Value().SetUint(key.first == 0 ? unsigned(0) : ((unsigned(key.first) << 8) | unsigned(key.second))), allocator);
			
			JsonUtility::ReplaceMember( settings, std::move( save ), name, allocator );
		});

		JsonUtility::SortMembers( settings );
	}

	void HotkeyPlugin::RenderHotkeys()
	{
		ImGui::PushID("EnginePluginHotkeys");

		int id = 0;
		char buffer0[256] = { '\0' };
		char buffer1[256] = { '\0' };

		const auto width = ImGui::CalcTextSize("Shift + Ctrl + Alt + Very Long Key Name").x;

		PluginManager::Get().ProcessActions([&](Input::Action* action, const Input::KeyModifier& key)
		{
			if (action->GetPlugin() == nullptr || action->GetPlugin()->GetName().empty() || action->GetName().empty())
				return;

			ImGui::PushID(id++);
			ImGui::BeginGroup();

			bool disable = !action->GetPlugin()->IsEnabled();
			ImGuiEx::PushDisable(disable);
			bool enabled = action->IsActiveFlag() && action->GetPlugin()->IsEnabled();
			if (ImGui::Checkbox("##Active", &enabled))
				action->SetActive(enabled);

			ImGuiEx::PopDisable();

			disable = disable || !action->IsActiveFlag();
			ImGuiEx::PushDisable(disable);
			ImGui::SameLine();
			FormatKeyLabel(buffer0, std::size(buffer0), key);
			ImFormatString(buffer1, std::size(buffer1), "%s###Binding", buffer0);
			ImGui::PushStyleColor(ImGuiCol_Button, PluginManager::Get().GetRebind() == action ? ImGui::GetColorU32(ImGuiCol_ButtonHovered) : ImGui::GetColorU32(ImGuiCol_Button));
			if (ImGui::Button(buffer1, ImVec2(width, 0.0f)))
				PluginManager::Get().RebindKeys(action);

			ImGui::PopStyleColor();
			ImGui::SameLine();
			ImGui::Text("%s %s", action->GetPlugin()->GetName().c_str(), action->GetName().c_str());
			ImGuiEx::PopDisable();

			ImGui::EndGroup();
			ImGui::PopID();
		});

		ImGui::PopID();
	}
#endif
}