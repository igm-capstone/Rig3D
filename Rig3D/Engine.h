#pragma once
#include <Windows.h>
#include "Rig3D\Common\WMEventHandler.h"
#include "Rig3D\Common\Timer.h"
#include "Rig3D\Common\Input.h"
#include "Rig3D\rig_defines.h"

#ifdef _WINDLL
#define RIG3D __declspec(dllexport)
#else
#define RIG3D __declspec(dllimport)
#endif

namespace Rig3D
{
	class IRenderer;
	class IScene;

	class RIG3D Engine : public virtual IObserver
	{
	public:
		Engine();
		Engine(GRAPHICS_API graphicsAPI);
		~Engine();

		int		Initialize(HINSTANCE hInstance, HINSTANCE prevInstance, PSTR cmdLine, int showCmdEngine, int windowWidth, int windowHeight, const char* windowCaption);
		void	BeginScene();
		void	EndScene();
		int		Shutdown();
		void	HandleEvent(const IEvent& iEvent) override;

		void	RunScene(IScene* iScene);

	protected:
		HDC mHDC;
		HWND mHWND;
	
	private:
		WMEventHandler* mEventHandler;
		Timer*			mTimer;
		Input*			mInput;
		IRenderer*		mRenderer;
		bool			mShouldQuit;

		int InitializeMainWindow(HINSTANCE hInstance, HINSTANCE prevInstance, PSTR cmdLine, int showCmd, int windowWidth, int windowHeight, const char* windowCaption);
	};
}



