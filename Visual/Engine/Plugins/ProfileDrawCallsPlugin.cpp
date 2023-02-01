#include "Common/Utility/StringManipulation.h"

#include "Visual/GUI2/imgui/imgui.h"
#include "Visual/GUI2/imgui/imgui_ex.h"
#include "Visual/GUI2/Icons.h"
#include "Visual/Renderer/DrawCalls/DrawCall.h"
#include "Visual/Renderer/RendererSubsystem.h"
#include "Visual/Renderer/ShaderSystem.h"
#include "Visual/Renderer/Scene/SceneSystem.h"
#include "Visual/Device/Device.h"
#include "Visual/Device/DynamicResolution.h"
#include "Visual/Device/RenderTarget.h"
#include "Visual/Device/IndexBuffer.h"
#include "Visual/Device/VertexBuffer.h"
#include "Visual/Physics/PhysicsSystem.h"
#include "Visual/Animation/Components/ComponentsStatistics.h"

#include "ProfileDrawCallsPlugin.h"


namespace Engine::Plugins
{
#if DEBUG_GUI_ENABLED && defined(PROFILING)
	namespace {
		std::string GetDynamicCullingFilterText()
		{
		#ifdef ENABLE_DEBUG_VIEWMODES
			switch (Device::DynamicCulling::Get().GetViewFilter())
			{
				case Device::DynamicCulling::ViewFilter::Default: return "None";
				case Device::DynamicCulling::ViewFilter::NoCosmetic: return "No Cosmetic";
				case Device::DynamicCulling::ViewFilter::WorstCase: return "Worst Case";
				case Device::DynamicCulling::ViewFilter::CosmeticOnly: return "Cosmetic Only";
				case Device::DynamicCulling::ViewFilter::ImportantOnly: return "Important Only";
				case Device::DynamicCulling::ViewFilter::EssentialOnly: return "Essential Only";
				default: return "Error: Unknown Value";
			}
		#endif
			return "";
		};
	}

	void ProfileDrawCallsPlugin::RenderBlendModes(const Device::IDevice::Counters& counters)
	{
		char buffer[256] = { '\0' };

		if (BeginSection("Blend Modes"))
		{
			for (unsigned i = 0; i < (unsigned) Renderer::DrawCalls::BlendMode::NumBlendModes; ++i)
			{
				const auto name = WstringToString(Renderer::DrawCalls::GetBlendModeName((Renderer::DrawCalls::BlendMode) i));

				ImColor col;
				if (counters.blend_mode_draw_counts[i] >= 200)
					col = ImColor(1.0f, 0.0f, 0.0f);
				else if(counters.blend_mode_draw_counts[i] >= 40)
					col = ImColor(1.0f, 1.0f, 0.0f);
				else
					col = ImGui::GetColorU32(ImGuiCol_Text, counters.blend_mode_draw_counts[i] > 0 ? 1.0f : 0.3f);

				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(col));
				ImGui::Text("%s = %u", name.c_str(), unsigned(counters.blend_mode_draw_counts[i]));
				ImGui::PopStyleColor();
			}

			EndSection();
		}
	}

	void ProfileDrawCallsPlugin::RenderDrawCallInfo(const Device::IDevice::Counters& counters)
	{
		char buffer[256] = { '\0' };

		if (BeginSection("Counters"))
		{
			ImGui::Text("Pass = %u", unsigned(counters.pass_count));
			ImGui::Text("Pipeline = %u", unsigned(counters.pipeline_count));
			ImGui::Text("Draw (all) = %u", unsigned(counters.draw_count));
			ImGui::Text("Draw (blend modes) = %u", unsigned(counters.blend_mode_draw_count));
			ImGui::Text("Triangle = %u", unsigned(counters.primitive_count));
			EndSection();
		}

		if (BeginSection("Render Objects"))
		{
			ImGui::Text("DrawCall = %u", unsigned(Renderer::DrawCalls::DrawCall::GetCount()));
			EndSection();
		}

		if (BeginSection("Cull"))
		{
			if (BeginSection("##Particles", buffer))
			{
				ImGui::Text("Dynamic Culling = %s", Device::DynamicCulling::Get().Enabled() ? "ON" : "OFF");
				if (Device::DynamicCulling::Get().Enabled())
				{
				#ifdef ENABLE_DEBUG_VIEWMODES
					const auto aggression = Device::DynamicCulling::Get().GetViewFilter() == Device::DynamicCulling::ViewFilter::WorstCase ? 100.0f : (Device::DynamicCulling::Get().GetAggression() * 100.0f);
					const auto filter = GetDynamicCullingFilterText();
					ImGui::Text("Aggression = %.1f %%", aggression);
					ImGui::Text("Filter = %s", filter.c_str());
				#else
					ImGui::Text("Aggression = %.1f %%", Device::DynamicCulling::Get().GetAggression() * 100.0f);
				#endif
				}

				EndSection();
			}

			EndSection();
		}
	}

	void ProfileDrawCallsPlugin::RenderDeviceInfo()
	{
		const auto backbuffer_resolution = Device::DynamicResolution::Get().GetBackbufferResolution();
		const auto dynamic_scale = Device::DynamicResolution::Get().GetBackbufferToDynamicScale();
		const auto dynamic_resolution = backbuffer_resolution * dynamic_scale;

		if (BeginSection("Screen"))
		{
			ImGui::Text("Resolution = %u x %u", unsigned(backbuffer_resolution.x), unsigned(backbuffer_resolution.y));
			ImGui::Text("Scale = %.4f x %.4f", dynamic_scale.x, dynamic_scale.y);
			ImGui::Text("Dynamic = %u x %u", unsigned(dynamic_resolution.x), unsigned(dynamic_resolution.y));
			EndSection();
		}
	}

	void ProfileDrawCallsPlugin::RenderGameInfo()
	{
		const auto& components_stats = Animation::Components::Stats::GetStats();
		const auto physics_stats = Physics::System::Get().GetStats();
		const auto component_stats = [&](const Animation::Components::Type type) -> unsigned { return unsigned(components_stats.ticked_counts[(unsigned) type]); };

		if (BeginSection("Game Components"))
		{
			ImGui::Text("Animated Render = %u", component_stats(Animation::Components::Type::AnimatedRender));
			ImGui::Text("Bone Group Trail = %u", component_stats(Animation::Components::Type::BoneGroupTrail));
			ImGui::Text("Client Animation Controller = %u", component_stats(Animation::Components::Type::ClientAnimationController));
			ImGui::Text("Lights = %u", component_stats(Animation::Components::Type::Lights));
			ImGui::Text("Particle Effects = %u", component_stats(Animation::Components::Type::ParticleEffects));
			ImGui::Text("Trails Effects = %u", component_stats(Animation::Components::Type::TrailsEffects));
			ImGui::Text("Wind Events = %u", component_stats(Animation::Components::Type::WindEvents));
			ImGui::Text("Sound Events = %u", component_stats(Animation::Components::Type::SoundEvents));
			EndSection();
		}

		if (BeginSection("Physics"))
		{
			ImGui::Text("Meshes = %u", unsigned(physics_stats.mesh_count));
			ImGui::Text("Colliders = %u", unsigned(physics_stats.collider_count));
			ImGui::Text("Collisions = %u", unsigned(physics_stats.collision_count));
			EndSection();
		}
	}

	void ProfileDrawCallsPlugin::OnRender(float elapsed_time)
	{
		if (Begin(600.0f))
		{
			const auto& counters = Renderer::System::Get().GetDevice()->GetPreviousCounters();
			
			if (IsPoppedOut())
			{
				if (ImGui::BeginTabBar("##Tabs"))
				{
					if (ImGui::BeginTabItem("All"))
					{
						if (ImGui::BeginChild("##ScrollChild", ImVec2(0, 0), false, GetWindowFlags()))
						{
							if (ImGui::BeginTable("##TableWrapper", 3, ImGuiTableFlags_NoBordersInBody | ImGuiTableFlags_NoSavedSettings | ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoPadOuterX))
							{
								ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthStretch);
								ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthStretch);
								ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthStretch);
								ImGui::TableNextRow();

								if (ImGui::TableSetColumnIndex(0))
									RenderBlendModes(counters);

								if (ImGui::TableSetColumnIndex(1))
								{
									RenderDrawCallInfo(counters);
									RenderGameInfo();
								}

								if (ImGui::TableSetColumnIndex(2))
									RenderDeviceInfo();

								ImGui::EndTable();
							}
						}

						ImGui::EndChild();
						ImGui::EndTabItem();
					}

					if (ImGui::BeginTabItem("Blend Modes"))
					{
						if (ImGui::BeginChild("##ScrollChild", ImVec2(0, 0), false, GetWindowFlags()))
							RenderBlendModes(counters);
						
						ImGui::EndChild();
						ImGui::EndTabItem();
					}

					if (ImGui::BeginTabItem("Draw Calls"))
					{
						if (ImGui::BeginChild("##ScrollChild", ImVec2(0, 0), false, GetWindowFlags()))
						{
							RenderDrawCallInfo(counters);
							RenderGameInfo();
						}

						ImGui::EndChild();
						ImGui::EndTabItem();
					}

					if (ImGui::BeginTabItem("Device"))
					{
						if (ImGui::BeginChild("##ScrollChild", ImVec2(0, 0), false, GetWindowFlags()))
							RenderDeviceInfo();

						ImGui::EndChild();
						ImGui::EndTabItem();
					}

					ImGui::EndTabBar();
				}
			}
			else if (ImGui::BeginTable("##TableWrapper", 3, ImGuiTableFlags_NoBordersInBody | ImGuiTableFlags_NoSavedSettings | ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoPadOuterX))
			{
				ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthStretch);
				ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthStretch);
				ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthStretch);
				ImGui::TableNextRow();

				if (ImGui::TableSetColumnIndex(0))
					RenderBlendModes(counters);

				if (ImGui::TableSetColumnIndex(1))
				{
					RenderDrawCallInfo(counters);
					RenderGameInfo();
				}

				if (ImGui::TableSetColumnIndex(2))
					RenderDeviceInfo();

				ImGui::EndTable();
			}
			
			Animation::Components::Stats::Reset();
			End();
		}
	}
#endif
}