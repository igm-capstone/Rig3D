#pragma once
#include <Windows.h>
#include <stdint.h>
#include "Rig3D\Options.h"
#include "Rig3D\Common\WMEventHandler.h"

#ifdef _WINDLL
#define RIG3D __declspec(dllexport)
#else
#define RIG3D __declspec(dllimport)
#endif


namespace Rig3D 
{
	class IScene;
	class IMesh;
	class IShader;
	class IShaderResource;
	class IRenderContext;

	enum InputClass
	{
		INPUT_CLASS_PER_VERTEX,
		INPUT_CLASS_PER_INSTANCE
	};

	enum InputFormat
	{
		R_FLOAT32,
		RG_FLOAT32,
		RGB_FLOAT32,
		RGBA_FLOAT32,
		R_SNORM8,
		RG_SNORM8,
		RGBA_SNORM8,
		R_TYPELESS8,
		RG_TYPELESS8,
		RGBA_TYPELESS8
	};

	struct InputElement
	{
		const char* Name;
		uint32_t	Index;
		uint32_t	InputSlot;
		uint32_t	ByteOffset;
		uint32_t	InstanceStepRate;
		InputFormat	Format;
		InputClass	InputSlotClass;
	};

	enum SamplerStateAddressType
	{
		SAMPLER_STATE_ADDRESS_CLAMP,
		SAMPLER_STATE_ADDRESS_WRAP,
		SAMPLER_STATE_ADDRESS_BORDER,
	};

	class RIG3D IRendererDelegate
	{
	public:
		virtual void VOnResize() = 0;

		IRendererDelegate() {};
		virtual ~IRendererDelegate() {};
	};

	template <class Base>
	class IRenderer : public Base, public IObserver
	{
	public:
		IRenderer() : 
			mHINSTANCE(nullptr),
			mHWND(nullptr),
			mWindowWidth(0),
			mWindowHeight(0),
			mGraphicsAPI(GRAPHICS_API_DIRECTX11),
			mDelegate(nullptr),
			mWindowCaption(""),
			mFullScreen(false)

		{

		}

		~IRenderer()
		{
		
		}

		inline int VInitialize(HINSTANCE hInstance, HWND hwnd, Options options)
		{
			mHINSTANCE = hInstance;
			mHWND = hwnd;
			mWindowWidth = options.mWindowWidth;
			mWindowHeight = options.mWindowHeight;
			mWindowCaption = options.mWindowCaption;
			mFullScreen = options.mFullScreen;

			if (Base::VInitialize(hInstance, hwnd, options) != RIG_SUCCESS)
			{
				return 0;
			}

			WMEventHandler::SharedInstance().RegisterObserver(WM_SIZE, this);
			WMEventHandler::SharedInstance().RegisterObserver(WM_ENTERSIZEMOVE, this);
			WMEventHandler::SharedInstance().RegisterObserver(WM_EXITSIZEMOVE, this);

			return RIG_SUCCESS;
		}

		inline void SetWindowCaption(const char* caption)
		{
			LPCWSTR wideWindowCaption;
			CSTR2WSTR(caption, wideWindowCaption)
			SetWindowTextW(mHWND, wideWindowCaption);
		};

		inline float		GetAspectRatio() const { return static_cast<float>(mWindowWidth) / mWindowHeight; };
		inline const int&	GetWindowWidth() const { return mWindowWidth; };
		inline const int&	GetWindowHeight() const { return mWindowHeight; };
		inline GraphicsAPI	GetGraphicsAPI() const { return mGraphicsAPI; };

		inline void SetDelegate(IRendererDelegate* renderDelegate) { mDelegate = renderDelegate; };

		void HandleEvent(const IEvent& iEvent) override
		{
			const WMEvent& wmEvent = static_cast<const WMEvent&>(iEvent);
			switch (wmEvent.msg)
			{
			case WM_SIZE:
				// Save the new client area dimensions.
				mWindowWidth = LOWORD(wmEvent.lparam);
				mWindowHeight = HIWORD(wmEvent.lparam);

				if (Base::GetDevice())
				{
					if (wmEvent.wparam == SIZE_MINIMIZED)
					{
						mIsPaused = true;
						mIsMinimized = true;
						mIsMaximized = false;
					}
					else if (wmEvent.wparam == SIZE_MAXIMIZED)
					{
						mIsPaused = false;
						mIsMinimized = false;
						mIsMaximized = true;
						VOnResize(mWindowWidth, mWindowHeight);
					}
					else if (wmEvent.wparam == SIZE_RESTORED)
					{
						// Restoring from minimized state?
						if (mIsMinimized)
						{
							mIsPaused = false;
							mIsMinimized = false;
							VOnResize(mWindowWidth, mWindowHeight);
						}

						// Restoring from maximized state?
						else if (mIsMaximized)
						{
							mIsPaused = false;
							mIsMaximized = false;
							VOnResize(mWindowWidth, mWindowHeight);
						}
						else if (mIsResizing)
						{
							// If user is dragging the resize bars, we do not resize 
							// the buffers here because as the user continuously 
							// drags the resize bars, a stream of WM_SIZE messages are
							// sent to the window, and it would be pointless (and slow)
							// to resize for each WM_SIZE message received from dragging
							// the resize bars.  So instead, we reset after the user is 
							// done resizing the window and releases the resize bars, which 
							// sends a WM_EXITSIZEMOVE message.
						}
						else // API call such as SetWindowPos or mSwapChain->SetFullscreenState.
						{
							VOnResize(mWindowWidth, mWindowHeight);
						}
					}
				}
				break;
				// WM_EXITSIZEMOVE is sent when the user grabs the resize bars.
			case WM_ENTERSIZEMOVE:
				mIsPaused = true;
				mIsResizing = true;
				//timer.Stop();
				break;

				// WM_EXITSIZEMOVE is sent when the user releases the resize bars.
				// Here we reset everything based on the new window dimensions.
			case WM_EXITSIZEMOVE:
				mIsPaused = false;
				mIsResizing = false;
				//timer.Start();
				VOnResize(mWindowWidth, mWindowHeight);
				break;
			default:
				break;
			}
		}

		void VOnResize(int windowWidth, int windowHeight)
		{
			Base::VOnResize(mWindowWidth, mWindowHeight);

			if (mDelegate)
			{
				mDelegate->VOnResize();
			}
		}

	protected:
		HINSTANCE			mHINSTANCE;
		HWND				mHWND;
		int					mWindowWidth;
		int					mWindowHeight;
		GraphicsAPI			mGraphicsAPI;
		IRendererDelegate*	mDelegate;
		const char*			mWindowCaption;
		
		bool				mFullScreen;
		bool					mIsPaused;
		bool					mIsMaximized;
		bool					mIsMinimized;
		bool					mIsResizing;
		};
}

