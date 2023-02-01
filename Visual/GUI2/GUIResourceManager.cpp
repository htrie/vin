#include "Visual/Device/Constants.h"
#include "Visual/Device/State.h"
#include "Visual/Device/VertexDeclaration.h"
#include "Visual/Device/Shader.h"
#include "Visual/Utility/DXHelperFunctions.h"
#include "Visual/Utility/WindowsUtility.h"
#include "Visual/Renderer/GlobalShaderDeclarations.h"
#include "Visual/Renderer/Utility.h"
#include "Visual/Renderer/CachedHLSLShader.h"
#include "Visual/Device/Device.h"
#include "Visual/Device/VertexBuffer.h"
#include "Visual/Device/IndexBuffer.h"
#include "Visual/Device/Texture.h"
#include "Visual/Device/Compiler.h"
#include "Visual/GUI2/GUIResourceManager.h"
#include "Visual/Device/SwapChain.h"
#include "Visual/Device/RenderTarget.h"
#include "Visual/Utility/IMGUI/ToolGUI.h"
#include "Visual/Utility/IMGUI/DebugGUIDefines.h"
#include "Visual/Utility/JsonUtility.h"
#include "Visual/Gamepad/GamepadState.h"
#include "Common/File/FileSystem.h"
#include "Common/File/PathHelper.h"
#include "Common/FileReader/BlendMode.h"
#include "Common/Utility/Logger/Logger.h"
#include "Common/Utility/LoadingScreen.h"
#include "Common/Utility/Profiler/ScopedProfiler.h"

#if DEBUG_GUI_ENABLED
#	include "Common/Utility/WindowsHeaders.h"
#	include "ImGuiImplPlatform.h"
#	if defined(WIN32) && !defined(CONSOLE)
#		include <windowsx.h>
#	endif

#	include "Visual/GUI2/imgui/imgui.h"
#	include "Visual/GUI2/imgui/implot.h"
#	include "imgui/imgui_internal.h"

#	include "FontData/FontAwesome6.inl"

#else
	struct ImDrawData {};
	struct ImGuiViewport {};
#endif

// Create input layout
const static Device::VertexElements vertex_elements =
{
	{0, 0, Device::DeclType::FLOAT2, Device::DeclUsage::POSITION, 0},
	{0, sizeof(float) * 2, Device::DeclType::FLOAT2, Device::DeclUsage::TEXCOORD, 0},
	{0, sizeof(float) * 4, Device::DeclType::UBYTE4N, Device::DeclUsage::COLOR, 0},
};

namespace Device
{
	namespace GUI
	{
#if DEBUG_GUI_ENABLED
		struct ImGuiViewportData
		{
		    std::unique_ptr<Device::SwapChain> swap_chain = nullptr;
			BufferData buffer_data;

			~ImGuiViewportData() { IM_ASSERT( swap_chain == nullptr && !buffer_data.vertex_buffer && !buffer_data.index_buffer ); }
		};

		static void ImGui_Impl_CreateWindow(ImGuiViewport* viewport)
		{
			PROFILE;

			auto& manager = GUIResourceManager::GetGUIResourceManager();
			assert( manager );

			auto* device = manager->GetDevice();
			assert( device );

			ImGuiViewportData* data = IM_NEW( ImGuiViewportData )( );
		    viewport->RendererUserData = data;

			data->swap_chain = Device::SwapChain::Create((HWND)viewport->PlatformHandle, device, static_cast< int >( viewport->Size.x ), static_cast< int >( viewport->Size.y ), false, false, 0);
			assert( data->swap_chain );
		}

		static void ImGui_Impl_DestroyWindow(ImGuiViewport* viewport)
		{
			PROFILE;

			auto& manager = GUIResourceManager::GetGUIResourceManager();
			assert(manager);

			manager->passes.Clear();

		    // The main viewport (owned by the application) will always have RendererUserData == nullptr since we didn't create the data for it.
			if( ImGuiViewportData* data = ( ImGuiViewportData* )viewport->RendererUserData )
			{
				data->buffer_data.Clear();
				data->swap_chain.reset();
		        IM_DELETE(data);
			}

		    viewport->RendererUserData = nullptr;
		}

		static void ImGui_Impl_SetWindowSize(ImGuiViewport* viewport, ImVec2 size)
		{
			PROFILE;

			auto& manager = GUIResourceManager::GetGUIResourceManager();
			assert(manager);

			manager->passes.Clear();

			ImGuiViewportData* data = ( ImGuiViewportData* )viewport->RendererUserData;
			if( data->swap_chain )
				data->swap_chain->Resize( static_cast< int >( size.x ), static_cast< int >( size.y ) );
		}

		static void ImGui_Impl_UpdateGamepads()
		{
			Utility::GamepadState::SetImGuiFocus(false);

			ImGuiIO& io = ImGui::GetIO();
			if ((io.ConfigFlags & ImGuiConfigFlags_NavEnableGamepad) == 0)
				return;

			Utility::GamepadState::SetImGuiFocus(io.NavActive);

			io.BackendFlags &= ~ImGuiBackendFlags_HasGamepad;

			Utility::GamepadState gamepad;
			if (Utility::GamepadState::GetState(gamepad, true))
			{
				io.BackendFlags |= ImGuiBackendFlags_HasGamepad;

			#define IM_SATURATE(V)						(V < 0.0f ? 0.0f : V > 1.0f ? 1.0f : V)
			#define MAP_BUTTON(KEY_NO, BUTTON_ENUM)		{ io.AddKeyEvent(KEY_NO, gamepad.ButtonPressed(Utility::GamepadState::Button::BUTTON_ENUM)); }
			#define MAP_ANALOG(KEY_NO, VALUE, V0, V1)	{ float vn = (float)(VALUE - V0) / (float)(V1 - V0); io.AddKeyAnalogEvent(KEY_NO, vn > 0.10f, IM_SATURATE(vn)); }
				MAP_BUTTON(ImGuiKey_GamepadStart,		Start);
				MAP_BUTTON(ImGuiKey_GamepadBack,		Back);
				MAP_BUTTON(ImGuiKey_GamepadFaceDown,	A);
				MAP_BUTTON(ImGuiKey_GamepadFaceRight,	B);
				MAP_BUTTON(ImGuiKey_GamepadFaceLeft,	X);
				MAP_BUTTON(ImGuiKey_GamepadFaceUp,		Y);
				MAP_BUTTON(ImGuiKey_GamepadDpadLeft,	DPadLeft);
				MAP_BUTTON(ImGuiKey_GamepadDpadRight,	DPadRight);
				MAP_BUTTON(ImGuiKey_GamepadDpadUp,		DPadUp);
				MAP_BUTTON(ImGuiKey_GamepadDpadDown,	DPadDown);
				MAP_BUTTON(ImGuiKey_GamepadL1,			LeftShoulder);
				MAP_BUTTON(ImGuiKey_GamepadR1,			RightShoulder);
				MAP_ANALOG(ImGuiKey_GamepadL2,			gamepad.GetLeftTrigger(), 0, 255);
				MAP_ANALOG(ImGuiKey_GamepadR2,			gamepad.GetRightTrigger(), 0, 255);
				MAP_BUTTON(ImGuiKey_GamepadL3,			LeftThumb);
				MAP_BUTTON(ImGuiKey_GamepadR3,			RightThumb);
				MAP_ANALOG(ImGuiKey_GamepadLStickLeft,	gamepad.GetLeftThumb().x, 0.0f, -1.0f);
				MAP_ANALOG(ImGuiKey_GamepadLStickRight,	gamepad.GetLeftThumb().x, 0.0f,  1.0f);
				MAP_ANALOG(ImGuiKey_GamepadLStickUp,	gamepad.GetLeftThumb().y, 0.0f,  1.0f);
				MAP_ANALOG(ImGuiKey_GamepadLStickDown,	gamepad.GetLeftThumb().y, 0.0f, -1.0f);
				MAP_ANALOG(ImGuiKey_GamepadRStickLeft,	gamepad.GetRightThumb().x, 0.0f, -1.0f);
				MAP_ANALOG(ImGuiKey_GamepadRStickRight,	gamepad.GetRightThumb().x, 0.0f,  1.0f);
				MAP_ANALOG(ImGuiKey_GamepadRStickUp,	gamepad.GetRightThumb().y, 0.0f,  1.0f);
				MAP_ANALOG(ImGuiKey_GamepadRStickDown,	gamepad.GetRightThumb().y, 0.0f, -1.0f);
			#undef MAP_BUTTON
			#undef MAP_ANALOG
			}
		}
#endif

		GUIResourceManager::PassKey::PassKey(Device::RenderTarget* render_target, bool clear)
			: render_target(render_target), clear(clear)
		{}

		GUIResourceManager::PipelineKey::PipelineKey(Device::Pass * pass)
			: pass(pass)
		{}

		GUIResourceManager::BindingSetKey::BindingSetKey(Device::Shader * shader, uint32_t inputs_hash)
			: shader(shader), inputs_hash(inputs_hash)
		{}

		GUIResourceManager::DescriptorSetKey::DescriptorSetKey(Device::Pipeline* pipeline, Device::DynamicBindingSet* pixel_binding_set, uint32_t samplers_hash)
			: pipeline(pipeline), pixel_binding_set(pixel_binding_set), samplers_hash(samplers_hash)
		{}

		std::shared_ptr<GUIResourceManager> GUIResourceManager::instance = nullptr;

		std::shared_ptr<GUIResourceManager>& GUIResourceManager::GetGUIResourceManager()
		{
			if(instance == nullptr)
			{
				instance.reset( new GUIResourceManager() );
			}
			return instance;
		}

		void GUIResourceManager::Destroy()
		{
			if( destroyed || defer_initialise )
				return;

			OnDestroyDevice();

			NotifyGUIListeners( GUIInstance::DeviceCallbacks, &GUIInstance::Destroy );

#if DEBUG_GUI_ENABLED && defined(IMGUI_PLATFORM)
			LOG_INFO("[GUI] Destroy");

		    ImGui::DestroyPlatformWindows();
			ImGui_Platform_Shutdown();
			ImPlot::DestroyContext();
			ImGui::DestroyContext();
#endif

			destroyed = true;
		}

		namespace
		{
			void* ImGuiMallocWrapper(size_t size, void* user_data) { return Memory::Allocate(Memory::Tag::Profile, size); }
			void  ImGuiFreeWrapper(void* ptr, void* user_data) { Memory::Free(user_data); }
		}

		GUIResourceManager::GUIResourceManager()
			: settings( std::make_shared<Utility::ToolSettings>() )
			, vertex_declaration(vertex_elements)
		{
#	if DEBUG_GUI_ENABLED && defined(IMGUI_PLATFORM)
			LOG_INFO("[GUI] Create");

			ImGui::SetAllocatorFunctions(ImGuiMallocWrapper, ImGuiFreeWrapper, nullptr);
		    ImGui_Platform_EnableDpiAwareness();

		    IMGUI_CHECKVERSION();
			ImGui::CreateContext();
			ImPlot::CreateContext();
			ImGuiIO& io = ImGui::GetIO();
			io.IniFilename = nullptr;

		    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;       // Enable Keyboard Controls
		    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;           // Enable Docking
		    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;        // Enable Gamepad Controls
		    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;         // Enable Multi-Viewport / Platform Windows

			io.ConfigViewportsNoTaskBarIcon = false;

			ImGui::StyleColorsDark();

		    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		    {
				auto& platform_io = ImGui::GetPlatformIO();
			    platform_io.Renderer_CreateWindow = ImGui_Impl_CreateWindow;
			    platform_io.Renderer_DestroyWindow = ImGui_Impl_DestroyWindow;
			    platform_io.Renderer_SetWindowSize = ImGui_Impl_SetWindowSize;
			    platform_io.Renderer_RenderWindow = nullptr;
			    platform_io.Renderer_SwapBuffers = nullptr;
		    }

#ifdef IMGUI_WIN32
			io.GetClipboardTextFn = [](void*) -> const char*
				{
					ImGuiContext& g = *GImGui;
					g.ClipboardHandlerData.clear();
					if (!::OpenClipboard(NULL))
						return "";

					HANDLE wbuf_handle = ::GetClipboardData(CF_UNICODETEXT);
					if (wbuf_handle == NULL)
					{
						::CloseClipboard();
						return "";
					}

					if (const WCHAR* wbuf_global = (const WCHAR*)::GlobalLock(wbuf_handle))
					{
						int buf_len = ::WideCharToMultiByte(CP_UTF8, 0, wbuf_global, -1, NULL, 0, NULL, NULL);
						g.ClipboardHandlerData.resize(buf_len);
						::WideCharToMultiByte(CP_UTF8, 0, wbuf_global, -1, g.ClipboardHandlerData.Data, buf_len, NULL, NULL);
					}

					::GlobalUnlock(wbuf_handle);
					::CloseClipboard();
					return g.ClipboardHandlerData.Data ? g.ClipboardHandlerData.Data : "";
				};
#endif

			ReloadFonts();			

			Initialise();

		    IMGUI_CHECKVERSION();
#endif
		}

		void GUIResourceManager::AddListener( GUIInstance& listener ) const
		{
			Utility::Listenable<GUIInstance>::AddListener( listener );

			if( !defer_initialise )
				init_next_frame.push_back( &listener );
		}

		void GUIResourceManager::RemoveListener( GUIInstance& listener ) const
		{
			Listenable<Device::GUI::GUIInstance>::RemoveListener( listener );
			RemoveFirst( init_next_frame, &listener );
		}

		void GUIResourceManager::Initialise()
		{
#if DEBUG_GUI_ENABLED && defined(IMGUI_PLATFORM)
			if( Device::WindowDX* window = Device::WindowDX::GetWindow() )
			{
				LOG_INFO("[GUI] Initialise");

				defer_initialise = false;

				ImGui::GetIO().BackendFlags |= ImGuiBackendFlags_RendererHasViewports | ImGuiBackendFlags_RendererHasVtxOffset;

				if( !ImGui_Platform_Init( window ) )
					throw std::runtime_error( "Failed to initialise ImGui_Platform" );

				NotifyGUIListeners( GUIInstance::DeviceCallbacks, &GUIInstance::Initialise );
			}
			else
				defer_initialise = true;
#endif
		}

		void GUIResourceManager::ReloadFonts()
		{
#if DEBUG_GUI_ENABLED && defined(IMGUI_PLATFORM)
			ImGuiIO& io = ImGui::GetIO();
			io.Fonts->Clear();
			monospace_font = nullptr;
			const float final_scale = scale * dpi_scale;

			const auto MergeIcons = [&io, final_scale](float font_size = 13.0f)
			{
				static const ImWchar icon_glyph_range[] = { 0x21, 0xf8ff, 0 };

				ImFontConfig merge_config;
				merge_config.MergeMode = true;
				merge_config.OversampleH = 2;
				merge_config.OversampleV = 2;
				merge_config.GlyphMinAdvanceX = 15.0f * final_scale;
				io.Fonts->AddFontFromMemoryCompressedTTF(FontData::FontAwesome6_compressed_data, FontData::FontAwesome6_compressed_size, Round<float>(font_size * final_scale), &merge_config, icon_glyph_range);
			};

			io.FontDefault = io.Fonts->AddFontDefault();
			MergeIcons();

		#if defined(WIN32) && !defined(CONSOLE)
			{
				wchar_t windows_dir[256];
				GetSystemWindowsDirectoryW(windows_dir, 256);
				const std::wstring font_path = std::wstring(windows_dir) + L"/Fonts/consola.ttf";

				if (FileSystem::FileExists(font_path))
				{
					ImFontConfig config;
					config.OversampleH = 2;
					config.OversampleV = 2;
					config.PixelSnapH = true;
					monospace_font = io.FontDefault = io.Fonts->AddFontFromFileTTF(WstringToString(font_path).c_str(), Round<float>(14 * final_scale), &config);
					MergeIcons();
				}
			}
		#endif

			if (FileSystem::FileExists(L"Tools/common/gui_font.ttf"))
			{
				ImFontConfig config;
				config.OversampleH = 3;
				config.OversampleV = 3;
				config.PixelSnapH = true;
				io.FontDefault = io.Fonts->AddFontFromFileTTF( "Tools/common/gui_font.ttf", Round<float>( 14 * final_scale ), &config );

				if( FileSystem::FileExists( L"Tools/common/arialuni.ttf" ) )
				{
					ImFontConfig merge_config;
					merge_config.MergeMode = true;
					io.Fonts->AddFontFromFileTTF( "Tools/common/arialuni.ttf", Round<float>( 14 * final_scale ), &merge_config );
				}

				MergeIcons();
			}
#endif
		}

		void GUIResourceManager::RecreateFontTexture()
		{
#if DEBUG_GUI_ENABLED
			ImGuiIO& io = ImGui::GetIO();

			// Setup the fonts texture
			unsigned char* pixels;
			int width, height, bytes_per_pixel;
			io.Fonts->GetTexDataAsRGBA32( &pixels, &width, &height, &bytes_per_pixel );

			font_texture = Device::Texture::CreateTextureFromMemory( "GUI Font", saved_device, width, height, pixels, bytes_per_pixel );

			// Store our identifier
			io.Fonts->TexID = ( void* )font_texture.Get();
#endif
		}

		GUIResourceManager::~GUIResourceManager()
		{
#if DEBUG_GUI_ENABLED
			if( !destroyed && !defer_initialise )
			{
				LOG_CRIT( L"ImGui unsafe shutdown detected, GUIResourceManager::Destroy was not called before exiting main function" );
				ImGui::GetIO().IniFilename = nullptr;
			}

			// This is not ideal
			Destroy();
#endif
		}

		HRESULT GUIResourceManager::OnCreateDevice(IDevice* pd3dDevice)
		{
#if DEBUG_GUI_ENABLED
			if (destroyed)
				return S_OK;

			HRESULT hr = S_OK;
			saved_device = pd3dDevice;

			vertex_blob = ::Renderer::CreateCachedHLSLAndLoad(saved_device, "GUIResourceManager VS", ::Renderer::LoadShaderSource(L"Shaders/ImGui.hlsl"), nullptr, "mainVS", Device::VERTEX_SHADER);
			pixel_blob  = ::Renderer::CreateCachedHLSLAndLoad(saved_device, "GUIResourceManager PS", ::Renderer::LoadShaderSource(L"Shaders/ImGui.hlsl"), nullptr, "mainPS", Device::PIXEL_SHADER);

			vertex_uniform_buffer = Device::DynamicUniformBuffer::Create("GUIResourceManager", saved_device, vertex_blob.Get());

			RecreateFontTexture();

			for( ImGuiViewport* viewport : ImGui::GetPlatformIO().Viewports )
			{
				if( auto* user_data = ( ImGuiViewportData* )viewport->RendererUserData )
				{
					user_data->buffer_data.Clear();
					user_data->swap_chain = Device::SwapChain::Create( ( HWND )viewport->PlatformHandle, saved_device, static_cast< int >( viewport->Size.x ), static_cast< int >( viewport->Size.y ), false, false, 0 );
				}
			}
#endif
			return S_OK;
		}

		void GUIResourceManager::OnLostDevice()
		{
#if DEBUG_GUI_ENABLED
			if (destroyed)
				return;

			descriptor_sets.Clear();
			binding_sets.Clear();
			pipelines.Clear();
			passes.Clear();
#endif
		}

		void GUIResourceManager::OnResetDevice( IDevice* device )
		{
#if DEBUG_GUI_ENABLED
			if (destroyed)
				return;
#endif
		}

		HRESULT GUIResourceManager::OnDestroyDevice()
		{
#if DEBUG_GUI_ENABLED
			if( destroyed )
				return S_OK;

			main_buffers.Clear();

			for( ImGuiViewport* viewport : ImGui::GetPlatformIO().Viewports )
			{
				if( auto * user_data = (ImGuiViewportData*)viewport->RendererUserData )
				{
					user_data->buffer_data.Clear();
					user_data->swap_chain.reset();
				}
			}

			font_texture.Reset();
			vertex_blob.Reset();
			pixel_blob.Reset();
			vertex_uniform_buffer.Reset();
#endif
			return S_OK;
		}

#if DEBUG_GUI_ENABLED
		// There is no distinct VK_xxx for keypad enter, instead it is VK_RETURN + KF_EXTENDED, we assign it an arbitrary value to make code more readable (VK_ codes go up to 255)
		#define IM_VK_KEYPAD_ENTER      (VK_RETURN + 256)

		// Map VK_xxx to ImGuiKey_xxx.
		ImGuiKey ImGui_Impl_VirtualKeyToImGuiKey( WPARAM wParam )
		{
			switch( wParam )
			{
			case VK_TAB: return ImGuiKey_Tab;
			case VK_LEFT: return ImGuiKey_LeftArrow;
			case VK_RIGHT: return ImGuiKey_RightArrow;
			case VK_UP: return ImGuiKey_UpArrow;
			case VK_DOWN: return ImGuiKey_DownArrow;
			case VK_PRIOR: return ImGuiKey_PageUp;
			case VK_NEXT: return ImGuiKey_PageDown;
			case VK_HOME: return ImGuiKey_Home;
			case VK_END: return ImGuiKey_End;
			case VK_INSERT: return ImGuiKey_Insert;
			case VK_DELETE: return ImGuiKey_Delete;
			case VK_BACK: return ImGuiKey_Backspace;
			case VK_SPACE: return ImGuiKey_Space;
			case VK_RETURN: return ImGuiKey_Enter;
			case VK_ESCAPE: return ImGuiKey_Escape;
			case VK_OEM_7: return ImGuiKey_Apostrophe;
			case VK_OEM_COMMA: return ImGuiKey_Comma;
			case VK_OEM_MINUS: return ImGuiKey_Minus;
			case VK_OEM_PERIOD: return ImGuiKey_Period;
			case VK_OEM_2: return ImGuiKey_Slash;
			case VK_OEM_1: return ImGuiKey_Semicolon;
			case VK_OEM_PLUS: return ImGuiKey_Equal;
			case VK_OEM_4: return ImGuiKey_LeftBracket;
			case VK_OEM_5: return ImGuiKey_Backslash;
			case VK_OEM_6: return ImGuiKey_RightBracket;
			case VK_OEM_3: return ImGuiKey_GraveAccent;
			case VK_CAPITAL: return ImGuiKey_CapsLock;
			case VK_SNAPSHOT: return ImGuiKey_PrintScreen;
			case VK_PAUSE: return ImGuiKey_Pause;
			case VK_NUMPAD0: return ImGuiKey_Keypad0;
			case VK_NUMPAD1: return ImGuiKey_Keypad1;
			case VK_NUMPAD2: return ImGuiKey_Keypad2;
			case VK_NUMPAD3: return ImGuiKey_Keypad3;
			case VK_NUMPAD4: return ImGuiKey_Keypad4;
			case VK_NUMPAD5: return ImGuiKey_Keypad5;
			case VK_NUMPAD6: return ImGuiKey_Keypad6;
			case VK_NUMPAD7: return ImGuiKey_Keypad7;
			case VK_NUMPAD8: return ImGuiKey_Keypad8;
			case VK_NUMPAD9: return ImGuiKey_Keypad9;
			case VK_DECIMAL: return ImGuiKey_KeypadDecimal;
			case VK_DIVIDE: return ImGuiKey_KeypadDivide;
			case VK_MULTIPLY: return ImGuiKey_KeypadMultiply;
			case VK_SUBTRACT: return ImGuiKey_KeypadSubtract;
			case VK_ADD: return ImGuiKey_KeypadAdd;
			case '0': return ImGuiKey_0;
			case '1': return ImGuiKey_1;
			case '2': return ImGuiKey_2;
			case '3': return ImGuiKey_3;
			case '4': return ImGuiKey_4;
			case '5': return ImGuiKey_5;
			case '6': return ImGuiKey_6;
			case '7': return ImGuiKey_7;
			case '8': return ImGuiKey_8;
			case '9': return ImGuiKey_9;
			case 'A': return ImGuiKey_A;
			case 'B': return ImGuiKey_B;
			case 'C': return ImGuiKey_C;
			case 'D': return ImGuiKey_D;
			case 'E': return ImGuiKey_E;
			case 'F': return ImGuiKey_F;
			case 'G': return ImGuiKey_G;
			case 'H': return ImGuiKey_H;
			case 'I': return ImGuiKey_I;
			case 'J': return ImGuiKey_J;
			case 'K': return ImGuiKey_K;
			case 'L': return ImGuiKey_L;
			case 'M': return ImGuiKey_M;
			case 'N': return ImGuiKey_N;
			case 'O': return ImGuiKey_O;
			case 'P': return ImGuiKey_P;
			case 'Q': return ImGuiKey_Q;
			case 'R': return ImGuiKey_R;
			case 'S': return ImGuiKey_S;
			case 'T': return ImGuiKey_T;
			case 'U': return ImGuiKey_U;
			case 'V': return ImGuiKey_V;
			case 'W': return ImGuiKey_W;
			case 'X': return ImGuiKey_X;
			case 'Y': return ImGuiKey_Y;
			case 'Z': return ImGuiKey_Z;
			case VK_F1: return ImGuiKey_F1;
			case VK_F2: return ImGuiKey_F2;
			case VK_F3: return ImGuiKey_F3;
			case VK_F4: return ImGuiKey_F4;
			case VK_F5: return ImGuiKey_F5;
			case VK_F6: return ImGuiKey_F6;
			case VK_F7: return ImGuiKey_F7;
			case VK_F8: return ImGuiKey_F8;
			case VK_F9: return ImGuiKey_F9;
			case VK_F10: return ImGuiKey_F10;
			case VK_F11: return ImGuiKey_F11;
			case VK_F12: return ImGuiKey_F12;
#ifdef IMGUI_WIN32
			case VK_SCROLL: return ImGuiKey_ScrollLock;
			case VK_NUMLOCK: return ImGuiKey_NumLock;
			case IM_VK_KEYPAD_ENTER: return ImGuiKey_KeypadEnter;
			case VK_LSHIFT: return ImGuiKey_LeftShift;
			case VK_LCONTROL: return ImGuiKey_LeftCtrl;
			case VK_LMENU: return ImGuiKey_LeftAlt;
			case VK_LWIN: return ImGuiKey_LeftSuper;
			case VK_RSHIFT: return ImGuiKey_RightShift;
			case VK_RCONTROL: return ImGuiKey_RightCtrl;
			case VK_RMENU: return ImGuiKey_RightAlt;
			case VK_RWIN: return ImGuiKey_RightSuper;
			case VK_APPS: return ImGuiKey_Menu;
#endif
			default: return ImGuiKey_None;
			}
		}

		static void ImGui_Impl_AddKeyEvent(ImGuiKey key, bool down, int native_keycode, int native_scancode = -1)
		{
			ImGuiIO& io = ImGui::GetIO();
			io.AddKeyEvent(key, down);
			io.SetKeyEventNativeData(key, native_keycode, native_scancode); // To support legacy indexing (<1.87 user code)
			IM_UNUSED(native_scancode);
		}

#ifdef IMGUI_WIN32
		static bool IsVkDown( int vk )
		{
			return ( ::GetKeyState( vk ) & 0x8000 ) != 0;
		}
#endif
#endif // #if DEBUG_GUI_ENABLED

#define DEBUG_OUTPUT(txt) {std::wstring d = std::wstring(txt) + L"\n"; OutputDebugStringW(d.c_str()); }

		bool GUIResourceManager::MainMsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
		{
		#if DEBUG_GUI_ENABLED
			if (destroyed || defer_initialise)
				return false;

			return ImGui_Platform_WndProcHandler(hwnd, msg, wParam, lParam);
		#endif

			return false;
		}

		bool GUIResourceManager::MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
		{
			bool result = false;
#if DEBUG_GUI_ENABLED
			if (destroyed)
				return false;

			static unsigned DEBUG_ID = ImHashStr("Debug##Default", 14);

			auto context = ImGui::GetCurrentContext();
			if( !context )
				return false;
			
			ImGuiIO& io = ImGui::GetIO();
			auto* current = context->NavWindow;
			bool has_focus = io.WantCaptureKeyboard || current && current->ID != DEBUG_ID;
			bool needs_text_input = io.WantTextInput;
			bool is_hovered = io.WantCaptureMouse;
			bool is_input_msg = false;
			GUIInstance::SetMouseWheel( 0.0f );

			auto default_callback = [&]()
			{
				switch (msg)
				{
			#if (defined(WIN32) && !defined(CONSOLE)) || defined(__APPLE__macosx)
				case WM_LBUTTONDOWN: case WM_LBUTTONDBLCLK:
				case WM_RBUTTONDOWN: case WM_RBUTTONDBLCLK:
				case WM_MBUTTONDOWN: case WM_MBUTTONDBLCLK:
				case WM_XBUTTONDOWN: case WM_XBUTTONDBLCLK:
				{
					is_input_msg = true;
					int button = 0;
					if (msg == WM_LBUTTONDOWN || msg == WM_LBUTTONDBLCLK) button = 0;
					if (msg == WM_RBUTTONDOWN || msg == WM_RBUTTONDBLCLK) button = 1;
					if (msg == WM_MBUTTONDOWN || msg == WM_MBUTTONDBLCLK) button = 2;
				#if defined(WIN32) && !defined(CONSOLE)
					if (msg == WM_XBUTTONDOWN || msg == WM_XBUTTONDBLCLK) { button = (GET_XBUTTON_WPARAM(wParam) == XBUTTON1) ? 3 : 4; }
				#endif

					io.AddMouseButtonEvent(button, true);
					GUIInstance::AddMouseButtonEvent(button, true);
					mouse_buttons_down |= 1 << button;
					if (hwnd != WindowDX::GetWindow()->GetWnd() || is_hovered)
					{
					#if defined(WIN32) && !defined(CONSOLE)
						SetCapture(hwnd);
					#endif
						clicked_inside_window = true;
					}			
					return clicked_inside_window;
				}
				case WM_LBUTTONUP:
				case WM_RBUTTONUP:
				case WM_MBUTTONUP:
				case WM_XBUTTONUP:
				{
					is_input_msg = true;
					int button = 0;
					if (msg == WM_LBUTTONUP) button = 0;
					if (msg == WM_RBUTTONUP) button = 1;
					if (msg == WM_MBUTTONUP) button = 2;
				#if defined(WIN32) && !defined(CONSOLE)
					if (msg == WM_XBUTTONUP) { button = (GET_XBUTTON_WPARAM(wParam) == XBUTTON1) ? 3 : 4; }
				#endif

					io.AddMouseButtonEvent( button, false );
					GUIInstance::AddMouseButtonEvent( button, false );
					const auto result = clicked_inside_window;
					mouse_buttons_down &= ~(1 << button);
					if (clicked_inside_window == true && mouse_buttons_down == 0)
					{
					#if defined(WIN32) && !defined(CONSOLE)
						ReleaseCapture();
					#endif
						clicked_inside_window = false;
					}
					return result;
				}
				case WM_MOUSEWHEEL:
					is_input_msg = true;
#if defined(WIN32) && !defined(CONSOLE)
					io.AddMouseWheelEvent(0.0f, (float)GET_WHEEL_DELTA_WPARAM(wParam) / (float)WHEEL_DELTA);
					GUIInstance::SetMouseWheel( (float)GET_WHEEL_DELTA_WPARAM(wParam) / (float)WHEEL_DELTA);
#endif
					return is_hovered;
				case WM_MOUSEHWHEEL:
					is_input_msg = true;
#if defined(WIN32) && !defined(CONSOLE)
					io.AddMouseWheelEvent((float)GET_WHEEL_DELTA_WPARAM(wParam) / (float)WHEEL_DELTA, 0.0f);
#endif
					return is_hovered;
				case WM_MOUSEMOVE:
				{
					is_input_msg = true;
				#if defined(WIN32) && !defined(CONSOLE)
					ui_debug.mouse_x = GET_X_LPARAM( lParam );
					ui_debug.mouse_y = GET_Y_LPARAM( lParam );
				#endif

					return clicked_inside_window;
				}
			#endif
				case WM_KEYDOWN:
				case WM_SYSKEYDOWN:
				case WM_KEYUP:
				case WM_SYSKEYUP:
				{
					is_input_msg = true;
					const bool is_key_down = (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN);
					if (wParam < 256)
					{
#ifdef IMGUI_WIN32
						// Submit modifiers
						io.AddKeyEvent(ImGuiKey_ModCtrl, IsVkDown(VK_CONTROL));
						io.AddKeyEvent(ImGuiKey_ModShift, IsVkDown(VK_SHIFT));
						io.AddKeyEvent(ImGuiKey_ModAlt, IsVkDown(VK_MENU));
						io.AddKeyEvent(ImGuiKey_ModSuper, IsVkDown(VK_APPS));
#endif
						// Obtain virtual key code
						// (keypad enter doesn't have its own... VK_RETURN with KF_EXTENDED flag means keypad enter, see IM_VK_KEYPAD_ENTER definition for details, it is mapped to ImGuiKey_KeyPadEnter.)
						int vk = (int)wParam;
#ifdef IMGUI_WIN32
						if ((wParam == VK_RETURN) && (HIWORD(lParam) & KF_EXTENDED))
							vk = IM_VK_KEYPAD_ENTER;

						const int scancode = (int)LOBYTE(HIWORD(lParam));
#else
						const int scancode = -1;
#endif
						// Submit key event
						const ImGuiKey key = ImGui_Impl_VirtualKeyToImGuiKey(vk);
						if (key != ImGuiKey_None)
							ImGui_Impl_AddKeyEvent(key, is_key_down, vk, scancode);
						GUIInstance::AddKeyEvent((unsigned)wParam, is_key_down);
#ifdef IMGUI_WIN32
						// Submit individual left/right modifier events
						if (vk == VK_SHIFT)
						{
							// Important: Shift keys tend to get stuck when pressed together, missing key-up events are corrected in ImGui_ImplWin32_ProcessKeyEventsWorkarounds()
							if (IsVkDown(VK_LSHIFT) == is_key_down) { ImGui_Impl_AddKeyEvent(ImGuiKey_LeftShift, is_key_down, VK_LSHIFT, scancode); }
							if (IsVkDown(VK_RSHIFT) == is_key_down) { ImGui_Impl_AddKeyEvent(ImGuiKey_RightShift, is_key_down, VK_RSHIFT, scancode); }
						}
						else if (vk == VK_CONTROL)
						{
							if (IsVkDown(VK_LCONTROL) == is_key_down) { ImGui_Impl_AddKeyEvent(ImGuiKey_LeftCtrl, is_key_down, VK_LCONTROL, scancode); }
							if (IsVkDown(VK_RCONTROL) == is_key_down) { ImGui_Impl_AddKeyEvent(ImGuiKey_RightCtrl, is_key_down, VK_RCONTROL, scancode); }
						}
						else if (vk == VK_MENU)
						{
							if (IsVkDown(VK_LMENU) == is_key_down) { ImGui_Impl_AddKeyEvent(ImGuiKey_LeftAlt, is_key_down, VK_LMENU, scancode); }
							if (IsVkDown(VK_RMENU) == is_key_down) { ImGui_Impl_AddKeyEvent(ImGuiKey_RightAlt, is_key_down, VK_RMENU, scancode); }
						}
#endif
					}

					if (is_key_down && wParam == VK_F4 && !io.KeyAlt && !io.KeyShift && !io.KeyCtrl)
					{
						// Always show menu on key down
						if (!show_menu)
						{
							show_menu = true;
							ImGui::FocusWindow(nullptr);
						}
						else
							show_menu = false;

						return true;
					}

					if (is_key_down && wParam == VK_BACK && io.KeyAlt && io.KeyShift && !io.KeyCtrl)
					{
						show_reset_popup = true;
						return true;
					}

					return needs_text_input;
				}
				case WM_ACTIVATE:
				#if defined(WIN32) && !defined(CONSOLE)
					if (wParam == WA_INACTIVE)
					{
						is_input_msg = true;
						GUIInstance::ClearInput();
						return true;
					}
				#endif

					return false;
				case WM_ACTIVATEAPP:
					if (wParam == FALSE)
					{
						is_input_msg = true;
						GUIInstance::ClearInput();
					}
					return false;
				case WM_CHAR:
					is_input_msg = true;
					// You can also use ToAscii()+GetKeyboardState() to retrieve characters.
					if (wParam > 0 && wParam < 0x10000)
						io.AddInputCharacter((unsigned short)wParam);
					return needs_text_input;
				}

				return false;
			};

			result = default_callback();

			if( key_inputs_disabled && key_inputs_disabled() && msg == WM_KEYDOWN )
				return result;

			const auto& listeners = GetListeners();
			const auto num = listeners.size();
			for (size_t i = 0; i < num; ++i)
			{
				if (auto* l = static_cast<GUIInstance*>(listeners[i]))
				{
					if (l->IsListenFlagSet(GUIInstance::MsgProcCallbacks))
					{
						result |= l->MsgProc(hwnd, msg, wParam, lParam);
					}
				}
			}

			// Input processing (managing & processing global input focus)
			if(is_input_msg)
			{
				bool anyKeyDown = GUIInstance::UpdateInput();

				if (active_input_instance != nullptr)
				{
					bool foundActive = false;
					for (size_t i = 0; i < num; ++i)
					{
						if (auto* l = static_cast<GUIInstance*>(listeners[i]))
						{
							if (!l->IsListenFlagSet(GUIInstance::InputCallback))
								continue;

							if (l != active_input_instance)
								continue;

							result |= l->OnProcessInput();
							foundActive = true;
							break;
						}
					}

					if (!anyKeyDown || !foundActive)
						active_input_instance = nullptr;
					else
						result = true; // always return true if we have an active input instance
				}
				
				if(active_input_instance == nullptr && anyKeyDown)
				{
					for (size_t i = 0; i < num; ++i)
					{
						if (auto* l = static_cast<GUIInstance*>(listeners[i]))
						{
							if (!l->IsListenFlagSet(GUIInstance::InputCallback))
								continue;

							if (l->OnProcessInput())
							{
								result = true;
								active_input_instance = l;
								break;
							}
						}
					}
				}
			}
#endif
			return result;
		}

		bool GUIResourceManager::HasFocus()
		{
#if DEBUG_GUI_ENABLED
			return ImGui::IsWindowFocused( ImGuiFocusedFlags_AnyWindow );
#else
			return false;
#endif
		}

		void GUIResourceManager::SaveSettings()
		{
			LOG_INFO("[GUI] Save");

			// Multiple processes can share the same settings; attempt to keep them in sync
			{
				auto& document = settings->GetDocument();
				auto& allocator = document.GetAllocator();

				rapidjson::Value old_settings( document, allocator );
				const bool reload_success = settings->Load();
				rapidjson::Value new_settings( document, allocator );
				NotifyListeners(&GUIInstance::OnSaveSettings, settings.get());

				if( reload_success )
				{
					rapidjson::Value changed_settings( rapidjson::kObjectType );
					for( auto& new_member : new_settings.GetObject() )
					{
						auto old_member = old_settings.FindMember( new_member.name );
						auto current_member = document.FindMember( new_member.name );

						if( old_member == old_settings.MemberEnd() || old_member->value != new_member.value )
						{
							if( current_member == document.MemberEnd() )
								document.AddMember( new_member.name, rapidjson::Value( new_member.value, allocator ), allocator );
							else if( current_member->value == old_member->value )
								current_member->value = rapidjson::Value( new_member.value, allocator );
							else
								continue;  // Merge conflict; prioritise the current changed value over the new one
							changed_settings.AddMember( new_member.name, new_member.value, allocator );
						}
					}

					if( !changed_settings.ObjectEmpty() )
						NotifyListeners( &GUIInstance::OnReloadSettings, changed_settings );
				}

				JsonUtility::SortMembers( document );
			}

			settings->Save();
#if DEBUG_GUI_ENABLED && defined(IMGUI_PLATFORM)
			ImGui::SaveIniSettingsToDisk(settings->GetImGuiINI().c_str());
#endif
			last_save_time = 0.0f;
#if DEBUG_GUI_ENABLED && defined(IMGUI_PLATFORM)
			ImGui::GetIO().WantSaveIniSettings = false;
#endif
		}

		void GUIResourceManager::Update( float fElapsedTime )
		{
			PROFILE;

			if (destroyed)
				return;

			if( defer_initialise )
				Initialise();
			else
			{
				for( auto& l : init_next_frame )
					l->Initialise();
				init_next_frame.clear();
			}

			if (defer_initialise || frame_state == FrameState::Recorded)
				return;

		#if DEBUG_GUI_ENABLED && defined(IMGUI_PLATFORM)
			
			ImGuiIO& io = ImGui::GetIO();
			const float desired_dpi_scale = ImGui::GetMainViewport()->DpiScale > 0.0f ? ImGui::GetMainViewport()->DpiScale : dpi_scale;

			if( desired_dpi_scale != dpi_scale || scale != desired_scale )
			{
				dpi_scale = desired_dpi_scale;
				scale = desired_scale;
				ReloadFonts();
				RecreateFontTexture();
				if( current_style )
				{
					ImGui::GetStyle() = *current_style;
					ImGui::GetStyle().ScaleAllSizes( scale * dpi_scale );
				}
			}

			auto& platform_io = ImGui::GetPlatformIO();

			// Hide OS mouse cursor if ImGui is drawing it
			if (io.MouseDrawCursor || ui_debug.hide_cursor)
			{
			#if defined(WIN32) && !defined(CONSOLE)
				SetCursor(nullptr);
			#endif
			}

			if(frame_state != FrameState::FirstFrame)
				ImGui::UpdatePlatformWindows();

			// Start the frame. This call will update the io.WantCaptureMouse, io.WantCaptureKeyboard flag that you can use to dispatch inputs (or not) to your application.
			if(auto g = ImGui::GetCurrentContext(); g && !g->SettingsLoaded)
				ImGui::LoadIniSettingsFromDisk(settings->GetImGuiINI().c_str());

			ImGui_Platform_NewFrame();
			ImGui_Impl_UpdateGamepads();
			ImGui::NewFrame();

			ImGui::GetStyle().Colors[ImGuiCol_WindowBg].w = opacity;

			if (show_reset_popup)
			{
				show_reset_popup = false;
				ImGui::OpenPopup("###ResetPopup");
			}

			ImGui::SetNextWindowSize(ImVec2(250.0f, ImGui::GetFrameHeight() * 5));
			ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
			if (ImGui::BeginPopupModal("Confirmation Required###ResetPopup", nullptr, ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoResize))
			{
				if (reset_layout)
					ImGui::CloseCurrentPopup();

				const float w = ImGui::GetContentRegionAvail().x;
				ImGui::PushTextWrapPos(w);
				ImGui::TextWrapped("You are about to reset your UI Layout. Are you sure about this?");
				ImGui::PopTextWrapPos();

				const auto buttonSize = ImVec2((w - ImGui::GetStyle().ItemSpacing.x) / 2, 0);
				if (ImGui::Button("Confirm", buttonSize))
				{
					reset_layout = true;
					ImGui::CloseCurrentPopup();
				}

				ImGui::SameLine();

				if (ImGui::Button("Abort", buttonSize))
					ImGui::CloseCurrentPopup();

				ImGui::EndPopup();
			}

			bool dockspace_keepalive = true;
			if (reset_layout || !DockBuilder::DockExists())
			{
				DockBuilder dock_builder;
				NotifyGUIListeners(GUIInstance::LayoutCallbacks, &GUIInstance::OnResetLayout, dock_builder);
				dockspace_keepalive = false;
			}

			reset_layout = false;
			last_save_time += io.DeltaTime;
			NotifyListeners(&GUIInstance::NewFrame);

			ImGuiViewport* viewport = ImGui::GetMainViewport();
			ImGui::SetNextWindowPos(viewport->WorkPos);
			ImGui::SetNextWindowSize(viewport->WorkSize);
			ImGui::SetNextWindowViewport(viewport->ID);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
			const bool visible = ImGui::Begin("Dockspace", nullptr, ImGuiWindowFlags_NoWindow | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoDocking);
			ImGui::PopStyleVar(3);

			if(dockspace_keepalive)
				ImGui::DockSpace(DockBuilder::GetDockspaceId(), ImVec2(0, 0), ImGuiDockNodeFlags_PassthruCentralNode);

			if (visible)
			{
				if (show_menu)
				{
					if (ImGui::Begin("Tools", &show_menu))
						NotifyGUIListeners(GUIInstance::RenderCallbacks, &GUIInstance::OnRenderMenu, fElapsedTime);
					ImGui::End();
				}

				NotifyGUIListeners(GUIInstance::RenderCallbacks, &GUIInstance::OnRender, fElapsedTime);
			}

			ImGui::End();

			ui_debug.Render();

			const auto loading_screen = LoadingScreen::IsActive();

			if (io.WantSaveIniSettings || (!in_loading_screen && loading_screen))
				SaveSettings();

			in_loading_screen = loading_screen;

		#endif

			frame_state = FrameState::Recorded;
		}

		void GUIResourceManager::Flush(Device::CommandBuffer* command_buffer)
		{
		#if DEBUG_GUI_ENABLED
			PROFILE;

			if (destroyed || frame_state != FrameState::Recorded)
				return;

			frame_state = FrameState::Rendered;

			auto* device = GetDevice();
			assert(device);

			ImGui::Render();

			// Main Viewport
			{
				PROFILE_NAMED("Render Main Viewport");
				auto render_target = device->GetMainSwapChain()->GetRenderTarget();
				RenderViewport(render_target, ImGui::GetMainViewport(), command_buffer);
			}

			// Render additional Platform Windows
			if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
			{
				PROFILE_NAMED("Render Viewports");
				ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
				for (int i = 1; i < platform_io.Viewports.Size; i++)
				{
					ImGuiViewport* viewport = platform_io.Viewports[i];
					if (viewport->Flags & ImGuiViewportFlags_Minimized)
						continue;

					if (ImGuiViewportData* data = (ImGuiViewportData*)viewport->RendererUserData)
					{
						auto render_target = data->swap_chain ? data->swap_chain->GetRenderTarget() : nullptr;
						RenderViewport(render_target, viewport, command_buffer);
					}
				}
			}

		#endif
		}

		void GUIResourceManager::PostRender()
		{
			if (destroyed)
				return;

			NotifyGUIListeners( GUIInstance::RenderCallbacks, &GUIInstance::OnPostRender );
		}

		void GUIResourceManager::Present()
		{
			if (destroyed)
				return;

			ForEachSwapChain([](Device::SwapChain* swap_chain) { swap_chain->Present(); });
		}

		void GUIResourceManager::ForEachSwapChain(const std::function<void(Device::SwapChain*)>& func)
		{
			if (!func)
				return;

			if (destroyed)
				return;

		#if DEBUG_GUI_ENABLED

			if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
			{
				ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
				for (int i = 1; i < platform_io.Viewports.Size; i++)
				{
					ImGuiViewport* viewport = platform_io.Viewports[i];
					if (ImGuiViewportData* data = (ImGuiViewportData*)viewport->RendererUserData)
					{
						if (data->swap_chain)
							func(data->swap_chain.get());
					}
				}
			}

		#endif
		}

		void GUIResourceManager::SetMVP(const simd::matrix& matrix)
		{
			vertex_uniform_buffer->SetMatrix("ProjectionMatrix", &matrix);
		}

		simd::matrix GUIResourceManager::GetDefaultMVP(ImDrawData* draw_data) const
		{
#if DEBUG_GUI_ENABLED
			float L = draw_data->DisplayPos.x;
			float R = draw_data->DisplayPos.x + draw_data->DisplaySize.x;
			float T = draw_data->DisplayPos.y;
			float B = draw_data->DisplayPos.y + draw_data->DisplaySize.y;

			float mvp[16] =
			{
				2.0f / (R - L),   0.0f,           0.0f,       0.0f,
				0.0f,         2.0f / (T - B),     0.0f,       0.0f,
				0.0f,         0.0f,           0.5f,       0.0f,
				(R + L) / (L - R),  (T + B) / (B - T),    0.5f,       1.0f,
			};

			return simd::matrix(mvp).transpose();
#else
			return simd::matrix::identity();
#endif
		}

		void GUIResourceManager::SetImGuiStyle( const ImGuiStyle& style )
		{
#if DEBUG_GUI_ENABLED
			ImGui::GetStyle() = style;
			ImGui::GetStyle().ScaleAllSizes( scale * dpi_scale );
			current_style = &style;
#endif
		}

		void GUIResourceManager::RenderDrawData(Device::Pass* pass, ImGuiViewport* viewport, Device::CommandBuffer* command_buffer, const simd::vector2_int& target_size)
		{
		#if DEBUG_GUI_ENABLED
			PROFILE;

			auto* draw_data = viewport->DrawData;

			if (draw_data->CmdListsCount == 0)
				return;

			const bool is_main_viewport = viewport == ImGui::GetMainViewport();
			ImGuiViewportData* user_data = (ImGuiViewportData*)viewport->RendererUserData;
			assert(is_main_viewport == !user_data);

			auto& buffer_data = is_main_viewport ? main_buffers : user_data->buffer_data;

			// Create and grow vertex/index buffers if needed
			if (!buffer_data.vertex_buffer || buffer_data.vertex_buffer_size < draw_data->TotalVtxCount)
			{
				if (buffer_data.vertex_buffer)
					buffer_data.vertex_buffer.Reset();

				buffer_data.vertex_buffer_size = draw_data->TotalVtxCount + 5000;
				buffer_data.vertex_buffer = Device::VertexBuffer::Create("VB GUI", saved_device, sizeof(ImDrawVert) * buffer_data.vertex_buffer_size, Device::FrameUsageHint(), Device::Pool::DEFAULT, nullptr);
				assert(buffer_data.vertex_buffer);
			}

			if (!buffer_data.index_buffer || buffer_data.index_buffer_size < draw_data->TotalIdxCount)
			{
				if (buffer_data.index_buffer)
					buffer_data.index_buffer.Reset();

				buffer_data.index_buffer_size = draw_data->TotalIdxCount + 10000;
				buffer_data.index_buffer = Device::IndexBuffer::Create("IB GUI", saved_device, sizeof(ImDrawIdx) * buffer_data.index_buffer_size, Device::FrameUsageHint(), Device::IndexFormat::_32, Device::Pool::DEFAULT, nullptr);
				assert(buffer_data.index_buffer);
			}

			// Copy and convert all vertices into a single contiguous buffer
			ImDrawVert* vertex_buffer_mem = nullptr;
			ImDrawIdx* index_buffer_mem = nullptr;

			buffer_data.vertex_buffer->Lock(0, sizeof(ImDrawVert) * buffer_data.vertex_buffer_size, (void**)(&vertex_buffer_mem), Device::Lock::DISCARD);
			buffer_data.index_buffer->Lock(0, sizeof(ImDrawIdx) * buffer_data.index_buffer_size, (void**)(&index_buffer_mem), Device::Lock::DISCARD);

			auto* begin_vertex = vertex_buffer_mem;
			auto* begin_index = index_buffer_mem;

			for (int n = 0; n < draw_data->CmdListsCount; n++)
			{
				const ImDrawList* cmd_list = draw_data->CmdLists[n];
				memcpy(vertex_buffer_mem, cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.Size * sizeof(ImDrawVert));
				memcpy(index_buffer_mem, cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx));
				vertex_buffer_mem += cmd_list->VtxBuffer.Size;
				index_buffer_mem += cmd_list->IdxBuffer.Size;
			}

			assert(vertex_buffer_mem - begin_vertex == draw_data->TotalVtxCount);
			assert(index_buffer_mem - begin_index == draw_data->TotalIdxCount);

			buffer_data.vertex_buffer->Unlock();
			buffer_data.index_buffer->Unlock();

			SetMVP(GetDefaultMVP(draw_data));

			auto* pipeline = pipelines.FindOrCreate(PipelineKey(pass), [&]()
			{
				return Device::Pipeline::Create("GUIResourceManager", saved_device, pass, Device::PrimitiveType::TRIANGLELIST, &vertex_declaration, vertex_blob.Get(), pixel_blob.Get(),
					Device::BlendState(
						Device::BlendChannelState(BlendMode::SRCALPHA, BlendOp::ADD, BlendMode::INVSRCALPHA),
						Device::BlendChannelState(BlendMode::INVSRCALPHA, BlendOp::ADD, BlendMode::ZERO)),
					Device::RasterizerState(CullMode::NONE, FillMode::SOLID, 0, 0),
					Device::DisableDepthStencilState());
			}).Get();

			

			// Render command lists
			vtx_offset = 0;
			idx_offset = 0;
			ImVec2 clip_off = draw_data->DisplayPos;
			bool active_rendering = command_buffer->BindPipeline(pipeline);
			bool resetPass = false;
			bool bindBuffers = true;
			Device::Pass* current_pass = pass;

			for (int n = 0; n < draw_data->CmdListsCount; n++)
			{
				const ImDrawList* cmd_list = draw_data->CmdLists[n];
				for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++)
				{
					const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];
					if (pcmd->UserCallback)
					{
						if (pcmd->UserCallback == ImDrawCallback_SetPass || (pcmd->UserCallback == ImDrawCallback_ResetRenderState && resetPass))
						{
							command_buffer->EndPass();

							current_pass = pass;
							if (pcmd->UserCallback == ImDrawCallback_SetPass && pcmd->UserCallbackData != nullptr)
								current_pass = (Device::Pass*)pcmd->UserCallbackData;

							simd::vector2_int size = target_size;
							if (current_pass != pass)
							{
								const auto& color_targets = current_pass->GetColorRenderTargets();
								if (!color_targets.empty())
									size = color_targets[0].get()->GetSize();
								else
									size = current_pass->GetDepthStencil()->GetSize();
							}

							command_buffer->BeginPass(current_pass, size, size);
							resetPass = (current_pass != pass);
						}

						if (pcmd->UserCallback == ImDrawCallback_ResetRenderState)
						{
							active_rendering = command_buffer->BindPipeline(pipeline);
							SetMVP(GetDefaultMVP(draw_data));
						}
						else if(pcmd->UserCallback != ImDrawCallback_SetPass)
							active_rendering = pcmd->UserCallback(cmd_list, pcmd, current_pass, command_buffer);

						bindBuffers = true;
					}
					else if(active_rendering)
					{
						const Device::Rect rect = { (LONG)(pcmd->ClipRect.x - clip_off.x), (LONG)(pcmd->ClipRect.y - clip_off.y), (LONG)(pcmd->ClipRect.z - clip_off.x), (LONG)(pcmd->ClipRect.w - clip_off.y) };
						if (rect.top == rect.bottom || rect.left == rect.right)
							continue;

						if (bindBuffers)
						{
							bindBuffers = false;
							command_buffer->BindBuffers(buffer_data.index_buffer.Get(), buffer_data.vertex_buffer.Get(), 0, sizeof(ImDrawVert));
						}

						command_buffer->SetScissor(rect);

						Device::DynamicBindingSet::Inputs inputs;
						inputs.push_back({ "texture0", (Device::Texture*)pcmd->TextureId });
						auto* pixel_binding_set = binding_sets.FindOrCreate(BindingSetKey(pixel_blob.Get(), inputs.Hash()), [&]()
						{
							return Device::DynamicBindingSet::Create("GUIResourceManager", saved_device, pixel_blob.Get(), inputs);
						}).Get();

						auto* descriptor_set = descriptor_sets.FindOrCreate(DescriptorSetKey(pipeline, pixel_binding_set, saved_device->GetSamplersHash()), [&]()
						{
							return Device::DescriptorSet::Create("GUIResourceManager", saved_device, pipeline, {}, { pixel_binding_set });
						}).Get();

						command_buffer->BindDescriptorSet(descriptor_set, {}, { vertex_uniform_buffer.Get() });

						command_buffer->DrawIndexed(pcmd->ElemCount, 1, idx_offset + pcmd->IdxOffset, vtx_offset + pcmd->VtxOffset, 0);
					}
				}

				idx_offset += cmd_list->IdxBuffer.Size;
				vtx_offset += cmd_list->VtxBuffer.Size;
			}

			assert(idx_offset == draw_data->TotalIdxCount);
			assert(vtx_offset == draw_data->TotalVtxCount);
		#endif
		}

		void GUIResourceManager::RenderViewport(Device::RenderTarget* render_target, ImGuiViewport* viewport, Device::CommandBuffer* command_buffer)
		{
		#if DEBUG_GUI_ENABLED
			PROFILE;

			if (!render_target)
				return;

			auto* device = GetDevice();

			const bool clear = (viewport->Flags & ImGuiViewportFlags_NoRendererClear) == 0 && viewport != ImGui::GetMainViewport();
			auto* pass = passes.FindOrCreate(GUIResourceManager::PassKey(render_target, clear), [&]()
			{
				return Device::Pass::Create("GUI", device, { render_target }, nullptr,
					clear ? (ClearTarget)((UINT)ClearTarget::COLOR | (UINT)ClearTarget::DEPTH) : ClearTarget::NONE,
					simd::color(255, 255, 255, 255), 1.0f);
			}).Get();

			device->SetCurrentUIPass(pass); //TODO: Remove (is just here until everything that relies on this being here uses the imgui rendering callback stuff instead)
			command_buffer->BeginPass(pass, render_target->GetSize(), render_target->GetSize());

			RenderDrawData(pass, viewport, command_buffer, render_target->GetSize());

			command_buffer->EndPass();
			device->SetCurrentUIPass(nullptr); //TODO: Remove
		#endif
		}

		void GUIResourceManager::SetConfigPath( const std::string& path )
		{
#if DEBUG_GUI_ENABLED
			settings->SetConfigPath( path );
#endif
		}

		// For testing purposes
		void GUIResourceManager::ShowDemoWindow()
		{
#if DEBUG_GUI_ENABLED
			ImGui::ShowDemoWindow( nullptr );
#endif
		}

		void GUIResourceManager::ShowDemoWindow( bool& open )
		{
#if DEBUG_GUI_ENABLED
			ImGui::ShowDemoWindow( &open );
#endif
		}

		BufferData::~BufferData()
		{

		}

		void BufferData::Clear()
		{
			vertex_buffer.Reset();
			index_buffer.Reset();
			vertex_buffer_size = 0;
			index_buffer_size = 0;
		}

#if DEBUG_GUI_ENABLED
		void UIDebug::Render()
		{
			auto viewport = ImGui::GetMainViewport();
			const auto pos = viewport->WorkPos;
			const auto size = viewport->WorkSize;
			auto draw_list = ImGui::GetBackgroundDrawList(viewport);

			// Draw lines across the screen that pinpoint the location the mouse is at, letting you line things up with the mouse
			if( draw_mouse_lines )
			{
				draw_list->AddLine(ImVec2(0.0f, (float)mouse_y) + pos, ImVec2(size.x, (float)mouse_y) + pos, ImColor(line_color.rgba()));
				draw_list->AddLine(ImVec2((float)mouse_x, 0.0f) + pos, ImVec2((float)mouse_x, size.y) + pos, ImColor(line_color.rgba()));
			}

			// Draw the current coordinates of the mouse, so you can compare with other positions
			if( draw_mouse_coords )
			{
				const auto padding = ImGui::GetStyle().ItemSpacing;

				std::stringstream text;
				text << mouse_x << ", " << mouse_y;
				const auto text_view = text.str();

				draw_list->AddText(ImVec2(mouse_x + padding.x, mouse_y - (ImGui::GetFontSize() + padding.y)) + pos, ImColor(1.0f, 1.0f, 1.0f), text_view.c_str());
			}
		}
#endif

	}
}
