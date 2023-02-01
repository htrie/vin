#include "GUIResourceManager.h"
#include "GUIInstance.h"

namespace Device
{
	namespace GUI
	{
		namespace
		{
			int KeyState[256] = { 0 };
			int MouseState[5] = { 0 };
			float MouseWheel = 0.0f;
		}

		bool GUIInstance::IsListenFlagSet( const ListenFlags flag ) const
		{
			return listen_flags & flag;
		}

		GUIInstance::GUIInstance( const bool listen, const unsigned listen_flags )
			: listening( listen )
			, listen_flags( listen_flags )
			, loaded(false)
		{
			if( listening )
			{
				if( auto manager = GUIResourceManager::GetGUIResourceManager() )
					manager->AddListener( *this );
			}
		}

		GUIInstance::~GUIInstance()
		{
			if( listening )
			{
				if( auto manager = GUIResourceManager::GetGUIResourceManager() )
					manager->RemoveListener( *this );
			}
		}

		void GUIInstance::NewFrame()
		{
			if (!loaded)
			{
				loaded = true;
				if (auto manager = GUIResourceManager::GetGUIResourceManager())
					OnLoadSettings(manager->GetSettings().get());
			}
		}

		bool GUIInstance::UpdateInput()
		{
			bool keyDown = MouseWheel != 0.0f;
		#if DEBUG_GUI_ENABLED

			for (int a = 0; a < int(std::size(KeyState)); a++)
			{
				if (KeyState[a] == 1)
					KeyState[a] = 2;
				else if(KeyState[a] == 2)
					KeyState[a] = 3;
				keyDown |= KeyState[a] > 0;
			}

			for (int a = 0; a < int(std::size(MouseState)); a++)
			{
				if (MouseState[a] == 1)
					MouseState[a] = 2;
				else if(MouseState[a] == 2)
					MouseState[a] = 3;
				keyDown |= MouseState[a] > 0;
			}

		#endif
			return keyDown;
		}

		void GUIInstance::ClearInput()
		{
			memset( KeyState, 0, sizeof( KeyState ) );
			memset( MouseState, 0, sizeof( MouseState) );
			MouseWheel = 0.0f;
		}

		bool GUIInstance::IsKeyPressed(unsigned user_key)
		{
			return KeyState[user_key] == 1 || KeyState[user_key] == 2;
		}

		bool GUIInstance::IsKeyDown(unsigned user_key)
		{
			return KeyState[user_key] > 0;
		}

		bool GUIInstance::IsMouseButtonPressed(unsigned user_key)
		{
			return MouseState[user_key] == 1 || MouseState[user_key] == 2;
		}

		bool GUIInstance::IsMouseButtonDown(unsigned user_key)
		{
			return MouseState[user_key] > 0;
		}

		float GUIInstance::GetMouseWheel()
		{
			return MouseWheel;
		}

		void GUIInstance::SetMouseWheel( const float delta )
		{
			MouseWheel = delta;
		}

		void GUIInstance::AddKeyEvent( const unsigned user_key, const bool down )
		{
			KeyState[user_key] = down;
		}

		void GUIInstance::AddMouseButtonEvent( const unsigned mouse_button, const bool down )
		{
			MouseState[mouse_button] = down;
		}
	}
}