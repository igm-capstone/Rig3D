#include "Input.h"
#include "rig_defines.h"

#include <Windowsx.h>

using namespace Rig3D;

Input& Input::SharedInstance()
{
	static Input sharedInput;
	return sharedInput;
}

Input::Input()
{
	mKeysDown		= new std::unordered_set<KeyCode>();
	mKeysUp			= new std::unordered_set<KeyCode>();
	mKeysPressed	= new std::unordered_set<KeyCode>();

	mPrevMouseState = 0;
	mCurrMouseState = 0;

	mEventHandler	= &WMEventHandler::SharedInstance();
}

Input::~Input()
{
	delete mKeysDown;
	delete mKeysUp;
	delete mKeysPressed;
}

int Input::Initialize()
{
	// Mouse Down
	mEventHandler->RegisterObserver(WM_LBUTTONDOWN, this);
	mEventHandler->RegisterObserver(WM_MBUTTONDOWN, this);
	mEventHandler->RegisterObserver(WM_RBUTTONDOWN, this);
	mEventHandler->RegisterObserver(WM_XBUTTONDOWN, this);

	// Mouse Up
	mEventHandler->RegisterObserver(WM_LBUTTONUP, this);
	mEventHandler->RegisterObserver(WM_MBUTTONUP, this);
	mEventHandler->RegisterObserver(WM_RBUTTONUP, this);
	mEventHandler->RegisterObserver(WM_XBUTTONUP, this);

	// Mouse Move
	mEventHandler->RegisterObserver(WM_MOUSEMOVE, this);

	// Key Down
	mEventHandler->RegisterObserver(WM_KEYDOWN, this);
	mEventHandler->RegisterObserver(WM_SYSKEYDOWN, this);

	// Key Up
	mEventHandler->RegisterObserver(WM_KEYUP, this);
	mEventHandler->RegisterObserver(WM_SYSKEYUP, this);

	return RIG_SUCCESS;
}

#pragma region Input Checkers

bool Input::GetKeyDown(KeyCode key)
{
	return mKeysDown->find(key) != mKeysDown->end();
}

bool Input::GetKey(KeyCode key)
{
	return GetKeyDown(key) || mKeysPressed->find(key) != mKeysPressed->end();
}

bool Input::GetKeyUp(KeyCode key)
{
	return mKeysUp->find(key) != mKeysUp->end();
}

bool Input::GetMouseButtonDown(MouseButton button)
{
	bool wasNotPressed = (mPrevMouseState & button) == 0;
	bool isPressed = (mCurrMouseState & button) == button;

	return wasNotPressed && isPressed;
}

bool Input::GetMouseButtonUp(MouseButton button)
{
	bool wasPressed = (mPrevMouseState & button) == button;
	bool isNotPressed = (mCurrMouseState & button) == 0;

	return wasPressed && isNotPressed;
}

bool Input::GetMouseButton(MouseButton button)
{
	bool isPressed = (mCurrMouseState & button) == button;

	return isPressed;
}

#pragma endregion

#pragma region Event Handler

void Input::HandleEvent(const IEvent& iEvent)
{
	const WMEvent& wmEvent = (const WMEvent&)iEvent;
	switch (wmEvent.msg)
	{
	case WM_LBUTTONDOWN:
			OnMouseDown(MOUSEBUTTON_LEFT, wmEvent.wparam, wmEvent.lparam);
		break;
	case WM_MBUTTONDOWN:
			OnMouseDown(MOUSEBUTTON_MIDDLE, wmEvent.wparam, wmEvent.lparam);
		break;
	case WM_RBUTTONDOWN:
			OnMouseDown(MOUSEBUTTON_RIGHT, wmEvent.wparam, wmEvent.lparam);
		break;
	case WM_XBUTTONDOWN:
		OnMouseDown(MOUSEBUTTON_X, wmEvent.wparam, wmEvent.lparam);
		break;
	case WM_LBUTTONUP:
			OnMouseUp(MOUSEBUTTON_LEFT, wmEvent.wparam, wmEvent.lparam);
		break;
	case WM_MBUTTONUP:
			OnMouseUp(MOUSEBUTTON_MIDDLE, wmEvent.wparam, wmEvent.lparam);
		break;
	case WM_RBUTTONUP:
			OnMouseUp(MOUSEBUTTON_RIGHT, wmEvent.wparam, wmEvent.lparam);
		break;
	case WM_XBUTTONUP:
		OnMouseUp(MOUSEBUTTON_X, wmEvent.wparam, wmEvent.lparam);
		break;
	case WM_MOUSEMOVE:
		OnMouseMove(wmEvent.wparam, wmEvent.lparam);
		break;
	case WM_KEYDOWN:
	case WM_SYSKEYDOWN: // key down if alt key is presed
		OnKeyDown(wmEvent.wparam, wmEvent.lparam);
		break;
	case WM_KEYUP:
	case WM_SYSKEYUP: // key up if alt key is pressed
		OnKeyUp(wmEvent.wparam, wmEvent.lparam);
		break;
	default:
		break;
	}
}

#pragma endregion

#pragma region Input Processing

void Input::Flush()
{
	// persist all keys pressed at this frame.
	for (const KeyCode &key : *mKeysDown)
	{
		mKeysPressed->insert(key);
	}

	// clear keys down and up
	mKeysDown->clear();
	mKeysUp->clear();

	// copy current mouse state to previous mouse state
	mPrevMouseState = mCurrMouseState;
}

KeyCode Input::KeyCodeFromWParam(WPARAM wParam)
{
	// any exception from wParam value to keyCode value
	// must be done here
	switch (wParam)
	{
	default:
		return static_cast<KeyCode>(wParam);
	}
}

void Input::OnKeyDown(WPARAM wParam, LPARAM lParam)
{
	// if the previous state is set to 1
	// the keydown event is recurrent
	// and there is no need for processing it
	if (lParam & PREV_KEY_STATE_BIT)
		return;

	KeyCode code = KeyCodeFromWParam(wParam);

	mKeysDown->insert(code);
}

void Input::OnKeyUp(WPARAM wParam, LPARAM lParam)
{
	KeyCode code = KeyCodeFromWParam(wParam);

	// remove the key from pressed keys
	mKeysPressed->erase(code);

	// and add it to up keys
	mKeysUp->insert(code);
}

void Input::OnMouseDown(MouseButton button, WPARAM wParam, LPARAM lParam)
{
	if (button == MOUSEBUTTON_X)
	{
		// GET_XBUTTON_WPARAM returns n for XBUTTON n
		short xButton = GET_XBUTTON_WPARAM(wParam);

		// shift xButton to 4 bits to the left
		// before adding to curr state
		mCurrMouseState |= xButton << 4;
	}
	else
	{
		mCurrMouseState |= button;
	}
}

void Input::OnMouseUp(MouseButton button, WPARAM wParam, LPARAM lParam)
{
	if (button == MOUSEBUTTON_X)
	{
		// GET_XBUTTON_WPARAM returns n for XBUTTON n
		short xButton = GET_XBUTTON_WPARAM(wParam);

		// shift xButton to 4 bits to the left
		// before adding to curr state
		mCurrMouseState &= ~(xButton << 4);
	}
	else
	{
		mCurrMouseState &= ~button;
	}
}

void Input::OnMouseMove(WPARAM wParam, LPARAM lParam)
{
	mousePosition.x = GET_X_LPARAM(lParam);
	mousePosition.y = GET_Y_LPARAM(lParam);
}

#pragma endregion
