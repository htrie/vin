#pragma once

#include <type_traits>
#include <string>

#include "Visual/Utility/IMGUI/DebugGUIDefines.h"
#include "Visual/Utility/ToolSettings.h"

namespace Device
{
	class IDevice;
}

namespace Engine
{
	class Plugin;

	namespace Input 
	{
		typedef unsigned Modifier;

		enum Modifier_ : Modifier
		{
			Modifier_None = 0,
			Modifier_Shift = 1 << 0,
			Modifier_Ctrl = 1 << 1,
			Modifier_Alt = 1 << 2,

			Modifier_All = Modifier_Shift | Modifier_Ctrl | Modifier_Alt,
		};

		typedef std::pair<uint8_t, Modifier> KeyModifier;

		class Action
		{
		private:
			Plugin* const parent;
			const std::string name;
			bool is_active = true;
			std::function<bool()> is_active_callback;

		public:
			Action(Plugin* parent, const std::string& name) : parent(parent), name(name) {}
			virtual ~Action() {}
			virtual void OnEnter() {}
			virtual void OnLeave() {}
			virtual void OnFrameMove() {}

			bool IsActive() const;
			bool IsActiveFlag() const { return is_active; }
			Action* SetActive(bool active) { is_active = active; return this; }
			Action* SetActive(const std::function<bool()>& func) { is_active_callback = func; return this; }
			Action* SetActive(std::function<bool()>&& func) { is_active_callback = std::move(func); return this; }
			Action* Activate() { return SetActive(true); }
			Action* Deactivate() { return SetActive(false); }

			const std::string& GetName() const { return name; }
			Plugin* GetPlugin() const { return parent; }
		};
	}

	class PluginManager
	{
		friend class Plugin;
	private:
		class Impl;

		std::unique_ptr<Impl> impl;

		PluginManager();
		PluginManager(const PluginManager&) = delete;
		bool AddPlugin(Plugin* plugin);
		bool AddAction(Input::Action* action, uint8_t key, Input::Modifier modifier);
		void RemoveAction(Input::Action* action);

		template<typename T> struct PluginInstance
		{
			static inline T* instance = nullptr;
		};

	public:
		static PluginManager& Get();

		void Update(float elapsed_time);
		void Clear();
		void OnGarbageCollect();

		template<typename T, typename... Args> T* RegisterPlugin(Args&&... args)
		{
			static_assert(std::is_base_of_v<Plugin, T>, "Type must be a plugin");
			static_assert(std::is_constructible_v<T, Args...>, "No suitable constructor found");

			if (PluginInstance<T>::instance)
				return PluginInstance<T>::instance;

			auto res = Memory::Pointer<T, Memory::Tag::Unknown>::make(std::forward<Args>(args)...);
			if (AddPlugin(res.get()))
			{
				PluginInstance<T>::instance = res.get();
				return res.release();
			}

			return nullptr;
		}

		template<typename T> T* GetPlugin()
		{
			return PluginInstance<T>::instance;
		}

		void ProcessActions(const std::function<void(Input::Action* action, const Input::KeyModifier& key)>& func);
		void SetKeyBinding(Input::Action* action, const Input::KeyModifier& key);
		void RebindKeys(Input::Action* action);
		Input::Action* GetRebind();
		void AbortRebind() { RebindKeys(nullptr); }
	};

	class Plugin
	{
		friend class PluginManager::Impl;

	private:
		bool enabled = false;
		bool visible = false;
		const unsigned features;

	protected:
		enum FeatureFlag : unsigned {
			Feature_Visible = 1 << 0,
			Feature_Settings = 1 << 1,
			Feature_NoDisable = 1 << 2,
			Feature_NoMenu = 1 << 3,
		};

		Plugin(unsigned features = 0) : features(features) {}

	public:
		template<typename T, typename... Args> T* AddAction(const std::string& name, uint8_t key, Input::Modifier modifier, Args&&... args)
		{
			static_assert(std::is_base_of_v<Input::Action, T>, "Type must be a input action");
			static_assert(std::is_constructible_v<T, Plugin*, const std::string&, Args...>, "No suitable constructor found");

			auto res = Memory::Pointer<T, Memory::Tag::Unknown>::make(this, name, std::forward<Args>(args)...);
			if (PluginManager::Get().AddAction(res.get(), key, modifier))
				return res.release();

			return nullptr;
		}

		Input::Action* AddAction(const std::string& name, uint8_t key, Input::Modifier modifier, const std::function<void()>& on_click);
		Input::Action* AddAction(const std::string& name, uint8_t key, Input::Modifier modifier, std::function<void()>&& on_click);
		void RemoveAction(Input::Action* action);

		bool IsVisible() const { return visible; }
		bool FeatureVisible() const { return features & Feature_Visible; }
		bool FeatureSettings() const { return features & Feature_Settings; }
		bool FeatureNoDisable() const { return features & Feature_NoDisable; }
		bool FeatureNoMenu() const { return features & Feature_NoMenu; }
		void SetVisible(bool visible);
		bool IsEnabled() const { return enabled; }
		void SetEnabled(bool enabled);
		void ToggleVisible() { SetVisible(!IsVisible()); }

		virtual ~Plugin() {}
		virtual std::string GetName() const { return ""; }
		virtual bool IsBlinking() const { return false; }
		virtual void OnUpdate(float elapsed_time) {}
		virtual void OnGarbageCollect() {}
		virtual void OnRender(float elapsed_time) {}
		virtual void OnRenderSettings(float elapsed_time) {}
		virtual void OnEnabled() {}
		virtual void OnDisabled() {}
		virtual void OnReloadSettings(const rapidjson::Value& settings) {}
		virtual void OnLoadSettings(const rapidjson::Value& settings) {}
		virtual void OnSaveSettings(rapidjson::Value& settings, rapidjson::Document::AllocatorType& allocator) {}
	};
}