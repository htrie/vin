// (C) 2009 Grinding Gear Games. All Rights Reserved

#pragma once

namespace Renderer
{
	typedef DirectX::XMFLOAT4 Color;

	typedef IDirect3DDevice9 RenderDevice;

	/// An interface for render subsystem, which is associated with a device
	struct IRenderSubsystem
	{
		virtual ~IRenderSubsystem() { }

		/// the device has been created
		virtual HRESULT OnCreateDevice( RenderDevice *const device ) = 0;

		/// the device has changed
		virtual HRESULT OnResetDevice( RenderDevice *const device ) = 0;

		/// the device has been lost
		virtual void OnLostDevice( ) = 0;

		/// the device has been destroyed
		virtual void OnDestroyDevice( ) = 0;

		/// draw the associated primitives
		virtual void Render(RenderDevice *const device ) const = 0;
	};

} // Renderer

//EOF
