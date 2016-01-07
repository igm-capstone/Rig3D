#include "Engine.h"
#include "rig_defines.h"
#include "Rig3D\Graphics\Interface\IScene.h"

using namespace Rig3D;

static const float DEFAULT_WINDOW_SIZE = 800.0f;
static const wchar_t* WND_CLASS_NAME = L"Rig3D Window Class";

Engine::Engine() : mShouldQuit(false)
{
	
}

Engine::~Engine()
{
}

int Engine::Initialize(HINSTANCE hInstance, HINSTANCE prevInstance, PSTR cmdLine, int showCmd, Options options)
{
	// Event Handler needs to be initialized before creating window.
	mEventHandler = &WMEventHandler::SharedInstance();
	mEventHandler->RegisterObserver(WM_CLOSE, this);
	mEventHandler->RegisterObserver(WM_QUIT, this);
	mEventHandler->RegisterObserver(WM_DESTROY, this);

	if (InitializeMainWindow(hInstance, prevInstance, cmdLine, showCmd, options) == RIG_ERROR)
	{
		return RIG_ERROR;
	}

	mRenderer = &TSingleton<IRenderer, DX3D11Renderer>::SharedInstance();

	TSingleton<IRenderer, DX3D11Renderer>* v = &TSingleton<IRenderer, DX3D11Renderer>::SharedInstance();

	//mRenderer = (options.mGraphicsAPI == GRAPHICS_API_DIRECTX11) ? &DX3D11Renderer::SharedInstance() : NULL; // TO DO: OpenGL Renderer
	if (mRenderer->VInitialize(hInstance, mHWND, options) == RIG_ERROR)
	{
		return RIG_ERROR;
	}

	mInput = &Input::SharedInstance();
	if (mInput->Initialize() == RIG_ERROR)
	{
		return RIG_ERROR;
	}

	mTimer = &Timer::SharedInstance();

	return 0;
}

int Engine::InitializeMainWindow(HINSTANCE hInstance, HINSTANCE prevInstance, PSTR cmdLine, int showCmd, Options options)
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
	ex.lpszMenuName = NULL;
	ex.lpszClassName = WND_CLASS_NAME;
	ex.hIconSm = NULL;

	if (options.mFullScreen == false) {
		ex.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
	}

	if (!RegisterClassEx(&ex)) {
		MessageBox(nullptr, L"RegisterClass Failed.", nullptr, 0);
		return RIG_ERROR;
	}

	// Compute window rectangle dimensions based on requested client area dimensions.
	RECT R = { 0, 0, options.mWindowWidth, options.mWindowHeight };
	AdjustWindowRect(&R, WS_OVERLAPPEDWINDOW, false);
	int width = R.right - R.left;
	int height = R.bottom - R.top;

	LPCWSTR wideWindowCaption;
	CSTR2WSTR(options.mWindowCaption, wideWindowCaption);

	// Create the window 

	mHWND = CreateWindowEx(NULL,
		WND_CLASS_NAME,
		wideWindowCaption,
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
	const WMEvent& wmEvent = static_cast<const WMEvent&>(iEvent);
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
	iScene->mEngine = this;

	iScene->VInitialize();
	// The message loop
	double deltaTime = 0.0;
	mTimer->Reset();
	while (!mShouldQuit)
	{
		mTimer->Update(&deltaTime);
		mEventHandler->Update();
		iScene->VUpdate(deltaTime);
		iScene->VRender();

		mInput->Flush();
	}

	iScene->VShutdown();
	Shutdown();
}

TSingleton<IRenderer, DX3D11Renderer>* Engine::GetRenderer() const
{
	return mRenderer;
}