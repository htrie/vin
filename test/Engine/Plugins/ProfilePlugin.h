#pragma once

#include "Visual/Engine/PluginManager.h"
#include "Common/Job/JobSystem.h"

namespace Engine::Plugins
{
#if DEBUG_GUI_ENABLED && defined(PROFILING)
	class ProfilePlugin final : public Plugin
	{
	private:
		class Impl;

		Impl* impl;
		static inline ProfilePlugin* instance = nullptr;

	public:
		ProfilePlugin();
		~ProfilePlugin();
		std::string GetName() const override { return "Profile"; }
		void OnRender(float elapsed_time) override;
		void OnRenderSettings(float elapsed_time) override;
		void OnEnabled() override;
		void OnDisabled() override;
		void OnLoadSettings(const rapidjson::Value& settings) override;
		void OnSaveSettings(rapidjson::Value& settings, rapidjson::Document::AllocatorType& allocator) override;
		
		enum class PageType
		{
			Memory,
			System,
			Jobs,
			DrawCalls,
			Device,
			FileAccess,
			Resource,
			Count
		};

		void CyclePages();
		void TogglePage(PageType type);

		static void PushGPU(Job2::Profile hash, uint64_t begin, uint64_t end);
		static void PushServer(Job2::Profile hash, uint64_t begin, uint64_t end);
		static void QueueBegin(Job2::Profile hash);
		static void QueueEnd();
		static void Swap();
		static void Hook();
		static void Unhook();

		static ProfilePlugin* Get() { return instance; }

		class Page : public Plugin
		{
		private:
			const std::string title;
			int window_flags;
			bool page_visible = false;
			bool popout_visible = false;
			bool popped_out = false;
			bool in_main_viewport = false;

		protected:
			bool Begin(float width = 300.0f);
			void End();

			bool BeginSection(const char* str_id, const char* label, bool default_open = true, int flags = 0);
			bool BeginSection(const char* label, bool default_open = true, int flags = 0) { return BeginSection(label, label, default_open, flags); }
			void EndSection();
			int GetWindowFlags() const { return window_flags; }

		public:
			Page(const std::string& title);
			std::string GetName() const override { return title; }
			bool IsPoppedOut() const { return popped_out; }
			void SetPoppedOut(bool popped) { popped_out = popped; }
			void SetPageVisible(bool visible);
			void SetPopoutVisible(bool visible) { popout_visible = visible; }
		};
	};
#endif
}