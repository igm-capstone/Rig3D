#include "Engine.h"
#include "rig_defines.h"
#include "DX3D11Renderer.h"
#include "IScene.h"

using namespace Rig3D;

static const float DEFAULT_WINDOW_SIZE = 800.0f;
static const wchar_t* WND_CLASS_NAME = L"Rig3D Window Class";

Engine::Engine() : mShouldQuit(false)
{
	mRenderer = &DX3D11Renderer::SharedInstance();
	mEventHandler = &WMEventHandler::SharedInstance();
	mTimer = &Timer::SharedInstance();
	mInput = &Input::SharedInstance();
}

Engine::~Engine()
{
}

int Engine::Initialize(HINSTANCE hInstance, HINSTANCE prevInstance, PSTR cmdLine, int showCmd, int windowWidth, int windowHeight, const char* windowCaption)
{
	if (InitializeMainWindow(hInstance, prevInstance, cmdLine, showCmd, windowWidth, windowHeight, windowCaption) == RIG_ERROR)
	{
		return RIG_ERROR;
	}

	if (mRenderer->VInitialize(hInstance, mHWND, windowWidth, windowHeight, windowCaption) == RIG_ERROR)
	{
		return RIG_ERROR;
	}

	if (mInput->Initialize() == RIG_ERROR)
	{
		return RIG_ERROR;
	}

	mEventHandler->RegisterObserver(WM_CLOSE, this);
	mEventHandler->RegisterObserver(WM_QUIT, this);
	mEventHandler->RegisterObserver(WM_DESTROY, this);

	return RIG_SUCCESS;
}

int Engine::InitializeMainWindow(HINSTANCE hInstance, HINSTANCE prevInstance, PSTR cmdLine, int showCmd, int windowWidth, int windowHeight, const char* windowCaption)
{
	WNDCLASSEX ex;
	ex.cbSize = sizeof(WNDCLASSEX);
	ex.style = CS_HREDRAW | CS_VREDRAW;
	ex.lpfnWndProc = WinProc;
	ex.cbClsExtra = 0;
	ex.cbWndExtra = 0;
	ex.hInstance = hInstance;
	ex.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	ex.hCursor = LoadCursor(NULL, IDC_ARROW);
	ex.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
	ex.lpszMenuName = NULL;
	ex.lpszClassName = WND_CLASS_NAME;
	ex.hIconSm = NULL;

	if (!RegisterClassEx(&ex)) {
		MessageBox(0, L"RegisterClass Failed.", 0, 0);
		return RIG_ERROR;
	}

	// Compute window rectangle dimensions based on requested client area dimensions.
	RECT R = { 0, 0, windowWidth, windowHeight };
	AdjustWindowRect(&R, WS_OVERLAPPEDWINDOW, false);
	int width = R.right - R.left;
	int height = R.bottom - R.top;

	// Create the window 

	mHWND = CreateWindowEx(NULL,
		WND_CLASS_NAME,
		L"Rig3D",
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT,
		width, height,
		NULL,
		NULL,
		hInstance,
		NULL);

	ShowWindow(mHWND, SW_SHOW);
	UpdateWindow(mHWND);

	return RIG_SUCCESS;
}

void Engine::BeginScene()
{
	// The message loop
	double deltaTime = 0.0;
	mTimer->Reset();
	while (!mShouldQuit)
	{
		mTimer->Update(&deltaTime);
		mEventHandler->Update();
		mRenderer->VUpdateScene(deltaTime);
		mRenderer->VRenderScene();
	}
}

void Engine::EndScene()
{

}

int Engine::Shutdown()
{
	mRenderer->VShutdown();
	return RIG_SUCCESS;
}


void Engine::HandleEvent(const IEvent& iEvent)
{
	const WMEvent& wmEvent = (const WMEvent&)iEvent;
	switch (wmEvent.msg)
	{
	case WM_QUIT:
		mShouldQuit = true;
		break;
	case WM_CLOSE:
		DestroyWindow(mHWND);
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		break;
	}
}

void Engine::RunScene(IScene* iScene)
{
	iScene->VInitialize();

	// The message loop
	double deltaTime = 0.0;
	mTimer->Reset();
	while (!mShouldQuit)
	{
		mTimer->Update(&deltaTime);
		mEventHandler->Update();
		iScene->VHandleInput();
		iScene->VUpdate(deltaTime);
		iScene->VRender();
		mInput->Flush();
	}

	Shutdown();
}