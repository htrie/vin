
#pragma once
#include "Common/Utility/Listenable.h"
#include "Visual/Device/Constants.h"
#include "Visual/Device/Resource.h"
#include "Visual/Device/Shader.h"
#include "Visual/Device/VertexDeclaration.h"
#include "Visual/Device/State.h"
#include "Visual/Utility/ToolSettings.h"
#include "Visual/GUI2/GUIInstance.h"

#include <functional>

struct ImDrawData;
struct ImGuiViewport;
struct ImFont;
struct ImGuiStyle;

namespace Device
{
	class IDevice;
	class Shader;
	class VertexBuffer;
	class IndexBuffer;
	class VertexDeclaration;
	class Texture;
	class SwapChain;
	struct BlendState;
	struct RasterizerState;
	struct DepthStencilState;

	namespace GUI
	{
		struct ImGuiViewportData;

		struct BufferData
		{
			~BufferData();

			void Clear();

			Device::Handle<Device::VertexBuffer> vertex_buffer;
			Device::Handle<Device::IndexBuffer> index_buffer;
			int vertex_buffer_size = 0;
			int index_buffer_size = 0;
		};

#if DEBUG_GUI_ENABLED
		// Class with UI debugging visuals. May eventually go to a new file if this gets bigger
		class UIDebug
		{
		public:
			void Render();

			bool draw_mouse_lines = false;
			bool draw_mouse_coords = false;
			bool hide_cursor = false;
			int mouse_x;
			int mouse_y;
			simd::color line_color = { 1.0f, 1.0f, 1.0f, 1.0f };
		};
#endif

		class GUIResourceManager : public Utility::Listenable<GUIInstance>
		{
		public:
			virtual ~GUIResourceManager();

			float opacity = 0.8f;
			float scale = 1.0f;
			float desired_scale = 1.0f;
			float dpi_scale = 1.0f;

			static std::shared_ptr<GUIResourceManager>& GetGUIResourceManager();

			void Destroy();

			virtual void AddListener( GUIInstance& listener ) const;
			virtual void RemoveListener( GUIInstance& listener ) const;

			void ResetLayout() { reset_layout = true; }

			bool MainMsgProc(HWND hwnd, UINT umsg, WPARAM wParam, LPARAM lParam); // Msg Proc for main window only. Calls platform specific callbacks
			bool MsgProc(HWND hwnd, UINT umsg, WPARAM wParam, LPARAM lParam);
			bool HasFocus();

			void Update( float fElapsedTime );
			void PostRender();
			void SaveSettings();
			float LastSaveTime() const { return last_save_time; }

			void Present();
			void ForEachSwapChain(const std::function<void(Device::SwapChain*)>& func);

			void Flush(Device::CommandBuffer* command_buffer);

			HRESULT OnCreateDevice(IDevice* pd3dDevice);
			HRESULT OnDestroyDevice();
			void OnResetDevice( IDevice* device );
			void OnLostDevice();

			// for testing purposes
			void ShowDemoWindow( bool& open );
			void ShowDemoWindow();

			int GetVertexOffset() const { return vtx_offset; }
			int GetIndexOffset() const { return idx_offset; }
			Device::IDevice* GetDevice() const { return saved_device; }
			const VertexDeclaration* GetVertexDeclaration() const { return &vertex_declaration; }
			Device::Shader* GetVertexShader() const { return vertex_blob.Get(); }
			Device::Shader* GetPixelShader() const { return pixel_blob.Get(); }
			Device::DynamicUniformBuffer* GetVertexUniformBuffer() const { return vertex_uniform_buffer.Get(); }

			simd::matrix GetDefaultMVP( ImDrawData* draw_data ) const;
			void SetMVP( const simd::matrix& matrix );

			std::shared_ptr<Utility::ToolSettings> GetSettings() { return settings; }

			void SetImGuiStyle( const ImGuiStyle& style );
			ImFont* GetMonospaceFont() const { return monospace_font; }

			void RenderDrawData(Device::Pass* pass, ImGuiViewport* viewport, Device::CommandBuffer* command_buffer, const simd::vector2_int& target_size);
			void RenderViewport(Device::RenderTarget* render_target, ImGuiViewport* viewport, Device::CommandBuffer* command_buffer);

			template< typename... Args >
			void NotifyGUIListeners( GUIInstance::ListenFlags flag, void ( GUIInstance::*f )( Args... ), Utility::detail::identity_t< Args >... args )
			{
				NotifyListenersIf( f, [flag]( GUIInstance* instance ) { return instance->IsListenFlagSet( flag ); }, args... );
			}

			void ShowMenu( const bool show ) { show_menu = show; }
			bool IsMenuShowing() const { return show_menu; }

			void SetConfigPath( const std::string& path );

			void SetKeyInputsDisabledCallback( const std::function< bool() >& func ) { key_inputs_disabled = func; }

#if DEBUG_GUI_ENABLED
			UIDebug ui_debug;
#endif
		#pragma pack(push)
		#pragma pack(1)
			struct PassKey
			{
				Device::PointerID<Device::RenderTarget> render_target;
				bool clear = false;

				PassKey() {}
				PassKey(Device::RenderTarget* render_target, bool clear);
			};
		#pragma pack(pop)
			Device::Cache<PassKey, Device::Pass> passes;

		#pragma pack(push)
		#pragma pack(1)
			struct PipelineKey
			{
				Device::PointerID<Device::Pass> pass;

				PipelineKey() {}
				PipelineKey(Device::Pass* pass);
			};
		#pragma pack(pop)
			Device::Cache<PipelineKey, Device::Pipeline> pipelines;

		#pragma pack(push)
		#pragma pack(1)
			struct BindingSetKey
			{
				Device::PointerID<Device::Shader> shader;
				uint32_t inputs_hash = 0;

				BindingSetKey() {}
				BindingSetKey(Device::Shader* shader, uint32_t inputs_hash);
			};
		#pragma pack(pop)
			Device::Cache<BindingSetKey, Device::DynamicBindingSet> binding_sets;

		#pragma pack(push)
		#pragma pack(1)
			struct DescriptorSetKey
			{
				Device::PointerID<Device::Pipeline> pipeline;
				Device::PointerID<Device::DynamicBindingSet> pixel_binding_set;
				uint32_t samplers_hash = 0;

				DescriptorSetKey() {}
				DescriptorSetKey(Device::Pipeline* pipeline, Device::DynamicBindingSet* pixel_binding_set, uint32_t samplers_hash);
			};
		#pragma pack(pop)
			Device::Cache<DescriptorSetKey, Device::DescriptorSet> descriptor_sets;

		private:
			GUIResourceManager();

			void Initialise();

			void ReloadFonts();
			void RecreateFontTexture();

			static std::shared_ptr<GUIResourceManager> instance;

			IDevice* saved_device;

			Handle<Shader> vertex_blob;
			Handle<Shader> pixel_blob;

			Handle<DynamicUniformBuffer> vertex_uniform_buffer;

			VertexDeclaration vertex_declaration;

			BufferData main_buffers;

			std::shared_ptr<Utility::ToolSettings> settings;

			const ImGuiStyle* current_style = nullptr;
			ImFont* monospace_font = nullptr;

			mutable std::vector<GUIInstance*> init_next_frame;

			Device::Handle<Device::Texture> font_texture;

			enum class FrameState : uint8_t
			{
				FirstFrame,
				Recorded,
				Rendered,
			};

			int mouse_buttons_down = 0;
			bool clicked_inside_window = false;
			bool defer_initialise = false;
			bool reset_layout = false;
			bool show_menu = false;
			bool destroyed = false;
			bool show_reset_popup = false;
			bool in_loading_screen = false;
			float last_save_time = FLT_MAX;
			FrameState frame_state = FrameState::FirstFrame;

			int vtx_offset = 0;
			int idx_offset = 0;

			std::function< bool() > key_inputs_disabled;

		#if DEBUG_GUI_ENABLED
			GUIInstance* active_input_instance = nullptr;
		#endif
		};
	}
}
