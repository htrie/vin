#include "PluginManager.h"
#include "Visual/GUI2/GUIInstance.h"

namespace Engine
{
	namespace 
	{
		class DefaultAction final : public Input::Action
		{
		private:
			std::function<void()> on_click;

		public:
			DefaultAction(Plugin* parent, const std::string& name, const std::function<void()>& on_click) : Action(parent, name), on_click(on_click) {}
			DefaultAction(Plugin* parent, const std::string& name, std::function<void()>&& on_click) : Action(parent, name), on_click(std::move(on_click)) {}

			void OnEnter() override final {}

			void OnLeave() override final
			{
				if (on_click && IsActive())
					on_click();
			}
		};
	}

	namespace Input
	{
		bool Action::IsActive() const 
		{ 
			return is_active && parent->IsEnabled() && (!is_active_callback || is_active_callback()); 
		}
	}

	class PluginManager::Impl : public Device::GUI::GUIInstance
	{
	private:
		Memory::SmallVector<std::pair<std::string, Memory::Pointer<Plugin, Memory::Tag::Unknown>>, 8, Memory::Tag::Unknown> plugins;
		size_t named_plugins = 0;
		Device::IDevice* device = nullptr;
		bool loaded_settings = false;

	#if DEBUG_GUI_ENABLED
		Memory::FlatMap<Input::Action*, Input::KeyModifier, Memory::Tag::Unknown> actions;
		Memory::FlatMap<uint8_t, Input::Action*, Memory::Tag::Unknown> active_actions;
		Input::Action* active_rebind = nullptr;
		bool key_down[256] = { false };

		Input::Modifier GetCurrentModifier() const
		{
			Input::Modifier res = Input::Modifier_None;
			if (key_down[VK_CONTROL])	res |= Input::Modifier_Ctrl;
			if (key_down[VK_SHIFT])		res |= Input::Modifier_Shift;
			if (key_down[VK_MENU])		res |= Input::Modifier_Alt;
			return res;
		}

		unsigned GetModifierFit(Input::Modifier m) const
		{
			unsigned f = 1;
			if ((m & Input::Modifier_Ctrl) != 0)	f++;
			if ((m & Input::Modifier_Shift) != 0)	f++;
			if ((m & Input::Modifier_Alt) != 0)		f++;
			return f;
		}

		bool PassActionKey(const Input::KeyModifier& key, Input::Modifier current_modifier) const
		{
			if (key.first < std::size(key_down) && current_modifier == key.second)
				return key_down[key.first];

			return false;
		}

		void ReleaseKey(uint8_t key)
		{
			if (const auto f = active_actions.find(key); f != active_actions.end())
			{
				auto action = f->second;
				active_actions.erase(f);

				if (const auto a = actions.find(action); a != actions.end())
					action->OnLeave();
			}
		}

		Input::Action* FindBestAction(uint8_t key) const
		{
			const auto current_modifier = GetCurrentModifier();

			Input::Action* best_action = nullptr;
			unsigned best_fit = 0;

			for (const auto& a : actions)
			{
				if (a.second.first != key)
					continue;

				if (!PassActionKey(a.second, current_modifier) || !a.first->IsActive())
					continue;

				const auto fit = GetModifierFit(a.second.second & current_modifier);
				if (fit > best_fit)
				{
					best_fit = fit;
					best_action = a.first;
				}
			}

			return best_action;
		}

		void UpdateCurrentActions(uint8_t key)
		{
			if (!key_down[key])
				ReleaseKey(key);
			else if (active_actions.find(key) != active_actions.end())
				return;
			else if (auto action = FindBestAction(key))
			{
				active_actions[key] = action;
				action->OnEnter();
			}
		}

		void UpdateKeyBindings(uint8_t key)
		{
			if (!key_down[key])
				return;

			if (key == VK_ESCAPE)
				active_rebind = nullptr;
			else
			{
				if (key != VK_SHIFT && key != VK_CONTROL && key != VK_MENU)
				{
					if (auto f = actions.find(active_rebind); f != actions.end())
						f->second = std::make_pair(key, GetCurrentModifier());

					active_rebind = nullptr;
				}
			}
		}

		void SetKeyState(WPARAM key, bool state)
		{
			if (key >= std::size(key_down) || key_down[key] == state)
				return;

			key_down[key] = state;
			if (active_rebind)
				UpdateKeyBindings(uint8_t(key));
			else
				UpdateCurrentActions(uint8_t(key));
		}

		void ClearKeyState()
		{
			for (auto& a : key_down)
				a = false;

			Memory::SmallVector<Input::Action*, 8, Memory::Tag::Unknown> last_active;

			for (const auto& a : active_actions)
			{
				if (const auto f = actions.find(a.second); f != actions.end())
					last_active.push_back(a.second);
			}

			active_actions.clear();

			for (const auto& a : last_active)
				a->OnLeave();
		}

		bool IsKeyActive() const
		{
			return active_actions.size() > 0;
		}

	#endif

	public:
		Impl() : GUIInstance(true, DeviceCallbacks | RenderCallbacks | MsgProcCallbacks) {}

		~Impl()
		{
		#if DEBUG_GUI_ENABLED
			for (const auto& a : actions)
				Memory::Pointer<Input::Action, Memory::Tag::Unknown>(a.first).reset();
		#endif
		}

		void OnLoadSettings(Utility::ToolSettings* settings)
		{
			loaded_settings = true;

			for (const auto& plugin : plugins)
			{
				if (auto name = plugin.second->GetName(); !name.empty())
				{
					const auto settings_name = "Engine." + name + "Plugin";
					const auto enabled_name = settings_name + ".Enabled";
					if (!plugin.second->FeatureNoDisable())
						if (auto f = settings->GetDocument().FindMember(enabled_name.c_str()); f != settings->GetDocument().MemberEnd() && f->value.IsBool())
							plugin.second->SetEnabled(f->value.GetBool());

					if (auto f = settings->GetDocument().FindMember(settings_name.c_str()); f != settings->GetDocument().MemberEnd() && f->value.IsObject())
						plugin.second->OnLoadSettings(f->value);
				}
			}
		}

		void OnSaveSettings(Utility::ToolSettings* settings)
		{
			if( !loaded_settings )
				return;

			for (const auto& plugin : plugins)
			{
				if (auto name = plugin.second->GetName(); !name.empty())
				{
					const auto settings_name = "Engine." + name + "Plugin";
					const auto enabled_name = settings_name + ".Enabled";

					rapidjson::Value plugin_settings(rapidjson::kObjectType);
					plugin.second->OnSaveSettings(plugin_settings, settings->GetAllocator());

					settings->GetDocument().RemoveMember(enabled_name.c_str());
					if (!plugin.second->FeatureNoDisable())
						settings->GetDocument().AddMember(rapidjson::Value().SetString(enabled_name.c_str(), settings->GetAllocator()), rapidjson::Value().SetBool(plugin.second->IsEnabled()), settings->GetAllocator());

					settings->GetDocument().RemoveMember(settings_name.c_str());
					if (!plugin_settings.Empty())
						settings->GetDocument().AddMember(rapidjson::Value().SetString(settings_name.c_str(), settings->GetAllocator()), plugin_settings, settings->GetAllocator());
				}
			}
		}

		bool AddPlugin(Plugin* plugin)
		{
			const auto name = plugin->GetName();
			if (name.empty() || std::find_if(plugins.begin(), plugins.end(), [&name](const auto& a) { return a.first == name; }) == plugins.end())
			{
				if (!plugin->FeatureNoMenu() && name.length() > 0)
					named_plugins++;

				plugin->SetEnabled(true);
				plugins.emplace_back(name, plugin);
				return true;
			}

			return false;
		}

		bool AddAction(Input::Action* action, uint8_t key, Input::Modifier modifier)
		{
		#if DEBUG_GUI_ENABLED
			actions[action] = std::make_pair(key, modifier);
			return true;
		#else
			return false;
		#endif
		}

		void RemoveAction(Input::Action* action)
		{
		#if DEBUG_GUI_ENABLED
			if (const auto f = actions.find(action); f != actions.end())
			{
				if (const auto a = active_actions.find(f->second.first); a != active_actions.end() && a->second == action)
				{
					auto action = a->second;
					active_actions.erase(a);
					action->OnLeave();
				}

				if (active_rebind == action)
					active_rebind = nullptr;

				actions.erase(f);
				Memory::Pointer<Input::Action, Memory::Tag::Unknown>(action).reset();
			}
		#endif
		}

		void SetKeyBinding(Input::Action* action, const Input::KeyModifier& key)
		{
		#if DEBUG_GUI_ENABLED
			if (const auto f = actions.find(action); f != actions.end())
			{
				if (const auto a = active_actions.find(f->second.first); a != active_actions.end() && a->second == action)
				{
					auto action = a->second;
					active_actions.erase(a);
					action->OnLeave();
				}

				if (active_rebind == action)
					active_rebind = nullptr;

				f->second = key;
			}
		#endif
		}

		void ProcessActions(const std::function<void(Input::Action* action, const Input::KeyModifier& key)>& func)
		{
		#if DEBUG_GUI_ENABLED
			if (!func)
				return;

			for (const auto& a : actions)
				func(a.first, a.second);
		#endif
		}

		void RebindKeys(Input::Action* action)
		{
		#if DEBUG_GUI_ENABLED
			if (const auto f = actions.find(action); f != actions.end())
			{
				ClearKeyState();
				active_rebind = action;
			}
			else
				active_rebind = nullptr;
		#endif
		}

		Input::Action* GetRebind()
		{
		#if DEBUG_GUI_ENABLED
			return active_rebind;
		#else
			return nullptr;
		#endif
		}

		void Update(float elapsed_time)
		{
		#if DEBUG_GUI_ENABLED
			for (auto a = active_actions.begin(); a != active_actions.end();)
			{
				if (actions.find(a->second) == actions.end())
				{
					a = active_actions.erase(a);
				}
				else if (!a->second->IsActive())
				{
					auto action = a->second;
					a = active_actions.erase(a);
					action->OnLeave();
				}
				else
				{
					++a;
				}
			}
		#endif

			for (const auto& a : plugins)
				if(a.second->IsEnabled())
					a.second->OnUpdate(elapsed_time);
		}

		void OnGarbageCollect()
		{
			for (const auto& a : plugins)
				if (a.second->IsEnabled())
					a.second->OnGarbageCollect();
		}

		Device::IDevice* GetDevice() const
		{
			return device;
		}

		void OnCreateDevice(Device::IDevice* _device) override
		{
			device = _device;
		}

		void OnDestroyDevice() override
		{
			device = nullptr;
		}

		void OnRender(const float elapsed_time) override
		{
			for (const auto& a : plugins)
				if(a.second->IsEnabled())
					a.second->OnRender(elapsed_time);
		}

		void OnRenderMenu(const float elapsed_time) override
		{
		#if DEBUG_GUI_ENABLED
			if (named_plugins == 0)
				return;

			if (ImGui::CollapsingHeader("Engine Plugins"))
			{
				const auto column_length = (named_plugins + 1) / 2;

				double t = 0;
				const bool blink = std::modf(ImGui::GetTime(), &t) < 0.5;

				auto PluginCheckbox = [blink, elapsed_time](const std::string& name, Plugin* plugin, size_t index)
				{
					ImGui::PushID(int(index));
					ImGuiEx::PushDisable(!plugin->FeatureSettings());
					if (ImGui::BeginCombo("##PluginSettings", "", ImGuiComboFlags_NoPreview))
					{
						ImGui::PushItemWidth(std::max(ImGui::GetContentRegionAvail().x, 150.0f));
						plugin->OnRenderSettings(elapsed_time);
						ImGui::PopItemWidth();
						ImGui::EndCombo();
					}

					if (ImGui::IsItemHovered())
						ImGui::SetTooltip("%s Settings", name.c_str());

					ImGuiEx::PopDisable();

					ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
					ImGui::PushStyleColor(ImGuiCol_Text, plugin->IsBlinking() && blink ? ImU32(ImColor(1.0f, 1.0f, 0.0f)) : ImGui::GetColorU32(ImGuiCol_Text));

					if (plugin->FeatureNoDisable())
					{
						if (plugin->FeatureVisible())
						{
							bool visible = plugin->IsVisible();
							if (ImGui::Checkbox(name.c_str(), &visible))
								plugin->SetVisible(visible);
							else if (ImGui::IsMouseReleased(ImGuiMouseButton_Right) && ImGui::IsItemHovered())
								plugin->SetVisible(!visible);
						}
						else
						{
							ImGui::Dummy(ImVec2(ImGui::GetFrameHeight(), ImGui::GetFrameHeight()));
						}
					}
					else
					{
						if (plugin->FeatureVisible())
						{
							bool enabled = plugin->IsEnabled();
							bool visible = plugin->IsVisible();

							if (enabled)
							{
								const bool was_visible = visible;

								if (!was_visible)
									ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, true);

								bool b = true;
								if (ImGui::Checkbox(name.c_str(), &b))
									plugin->SetVisible(!visible);
								else if (ImGui::IsMouseReleased(ImGuiMouseButton_Right) && ImGui::IsItemHovered())
									plugin->SetEnabled(false);

								if (!was_visible)
									ImGui::PopItemFlag();
							}
							else
							{
								if (ImGui::Checkbox(name.c_str(), &enabled))
								{
									plugin->SetEnabled(enabled);
									plugin->SetVisible(true);
								}
								else if (ImGui::IsMouseReleased(ImGuiMouseButton_Right) && ImGui::IsItemHovered())
									plugin->SetEnabled(true);
							}
						}
						else
						{
							bool enabled = plugin->IsEnabled();
							if (ImGui::Checkbox(name.c_str(), &enabled))
								plugin->SetEnabled(enabled);
							else if (ImGui::IsMouseReleased(ImGuiMouseButton_Right) && ImGui::IsItemHovered())
								plugin->SetEnabled(!enabled);
						}
					}

					ImGui::PopStyleColor();
					ImGui::PopID();
				};

				size_t i = 0;
				{
					ImGui::BeginGroup();
					for (size_t a = 0; i < plugins.size() && a < column_length; i++)
					{
						if (plugins[i].first.length() > 0 && !plugins[i].second->FeatureNoMenu())
						{
							PluginCheckbox(plugins[i].first, plugins[i].second.get(), i);
							a++;
						}
					}

					ImGui::EndGroup();
				}

				ImGui::SameLine();

				{
					ImGui::BeginGroup();
					for (; i < plugins.size(); i++)
						if (plugins[i].first.length() > 0 && !plugins[i].second->FeatureNoMenu())
							PluginCheckbox(plugins[i].first, plugins[i].second.get(), i);

					ImGui::EndGroup();
				}
			}

		#endif
		}

		bool MsgProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) override
		{
		#if DEBUG_GUI_ENABLED
			ImGuiIO& io = ImGui::GetIO();

			switch (uMsg)
			{
				case WM_KEYDOWN:
				case WM_SYSKEYDOWN:
				{
					if (io.WantTextInput)
					{
						ClearKeyState();
						return true;
					}

					SetKeyState(wParam, true);
					return IsKeyActive();
				}
				case WM_KEYUP:
				case WM_SYSKEYUP:
				{
					if (io.WantTextInput)
					{
						ClearKeyState();
						return true;
					}

					SetKeyState(wParam, false);
					return IsKeyActive();
				}
				case WM_ACTIVATEAPP:
				{
					if (wParam == FALSE)
						ClearKeyState();

					return false;
				}
				default:
					break;
			}
		#endif

			return false;
		}

		
	};

	PluginManager::PluginManager() : impl(std::make_unique<Impl>()) {}

	bool PluginManager::AddPlugin(Plugin* plugin) { return impl->AddPlugin(plugin); }
	bool PluginManager::AddAction(Input::Action* action, uint8_t key, Input::Modifier modifier) { return impl->AddAction(action, key, modifier); }
	void PluginManager::RemoveAction(Input::Action* action) { impl->RemoveAction(action); }
	void PluginManager::ProcessActions(const std::function<void(Input::Action* action, const Input::KeyModifier& key)>& func) { impl->ProcessActions(func); }
	void PluginManager::SetKeyBinding(Input::Action* action, const Input::KeyModifier& key) { impl->SetKeyBinding(action, key); }
	void PluginManager::RebindKeys(Input::Action* action) { impl->RebindKeys(action); }
	Input::Action* PluginManager::GetRebind() { return impl->GetRebind(); }

	PluginManager& PluginManager::Get()
	{
		static PluginManager instance;
		return instance;
	}

	Device::IDevice* PluginManager::GetDevice() const { return impl->GetDevice(); }

	void PluginManager::Update(float elapsed_time) { impl->Update(elapsed_time); }
	void PluginManager::OnGarbageCollect() { impl->OnGarbageCollect(); }
	void PluginManager::Clear() { impl.reset(); }

	Device::IDevice* Plugin::GetDevice() const
	{
		return PluginManager::Get().impl->GetDevice();
	}

	void Plugin::SetVisible(bool v)
	{
		visible = FeatureVisible() && v;
	}

	void Plugin::SetEnabled(bool e)
	{
		if (FeatureNoDisable())
			e = true;

		if (enabled == e)
			return;

		enabled = e;
		if (enabled)
			OnEnabled();
		else
			OnDisabled();
	}

	void Plugin::RemoveAction(Input::Action* action)
	{
		PluginManager::Get().RemoveAction(action);
	}

	Input::Action* Plugin::AddAction(const std::string& name, uint8_t key, Input::Modifier modifier, const std::function<void()>& on_click)
	{
		return AddAction<DefaultAction>(name, key, modifier, on_click);
	}

	Input::Action* Plugin::AddAction(const std::string& name, uint8_t key, Input::Modifier modifier, std::function<void()>&& on_click)
	{
		return AddAction<DefaultAction>(name, key, modifier, std::move(on_click));
	}
}