// ImGui Win32 + DirectX11 binding
// You can copy and use unmodified imgui_impl_* files in your project. See main.cpp for an example of using this.
// If you use this binding you'll need to call 4 functions: ImGui_ImplXXXX_Init(), ImGui_ImplXXXX_NewFrame(), ImGui::Render() and ImGui_ImplXXXX_Shutdown().
// If you are new to ImGui, see examples/README.txt and documentation at the top of imgui.cpp.
// https://github.com/ocornut/imgui

#pragma once

#ifdef _WINDLL
#define RIG3D __declspec(dllexport)
#else
#define RIG3D __declspec(dllimport)
#endif

namespace Rig3D
{
	class RIG3D DX11IMGUI  {
	public:
		static bool	Init(void* hwnd, ID3D11Device* device, ID3D11DeviceContext* device_context);
		static void Shutdown();
		static void NewFrame();

		// Use if you want to reset your rendering device without losing ImGui state.
		static void InvalidateDeviceObjects();
		static bool CreateDeviceObjects();

		class GUIObserver : public virtual IObserver {
		public:
			GUIObserver();
			void HandleEvent(const IEvent& iEvent) override;
		};

	public:
		static GUIObserver mGUIObserver;
		
	};
}