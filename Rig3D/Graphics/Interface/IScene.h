#pragma once
#include <Windows.h>
#include "Rig3D\Options.h"


#ifdef _WINDLL
#define RIG3D __declspec(dllexport)
#else
#define RIG3D __declspec(dllimport)
#endif

namespace Rig3D
{
	class Engine;

	class RIG3D IScene
	{
	public:
		Options		mOptions;
		Engine*		mEngine;

		IScene();
		virtual ~IScene();

		virtual void VInitialize() = 0;
		virtual void VUpdate(double milliseconds) = 0;
		virtual void VRender() = 0;
		virtual void VShutdown() = 0;
	};
}

#define DECLARE_MAIN(SceneClass)										\
Rig3D::IScene *gRig3DScene = 0;											\
int CALLBACK WinMain(HINSTANCE hInstance,								\
                     HINSTANCE hPrevInstance,							\
                     PSTR cmdLine,										\
                     int showCmd)										\
{																		\
	SceneClass scene;													\
	gRig3DScene = &scene;												\
    Rig3D::Engine engine = Rig3D::Engine();								\
	engine.Initialize(hInstance,										\
						hPrevInstance,									\
						cmdLine,										\
						showCmd,										\
						gRig3DScene->mOptions);							\
	engine.RunScene(gRig3DScene);										\
    return 0;															\
}	