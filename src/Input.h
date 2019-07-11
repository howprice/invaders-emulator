#pragma once
#ifndef INPUT_H
#define INPUT_H

#include <SDL.h>

class Input
{
public:
	
	static const unsigned int kMaxControllers = 8;

	static void Init();
	static void Shutdown();

	static void FrameStart();

	static bool GetKeyState(SDL_Scancode scancode);
	static bool IsKeyDownThisFrame(SDL_Scancode scancode);

	static bool IsControllerConnected(unsigned int index);
	static const char* GetControllerName(unsigned int index);

	static bool GetButtonState(unsigned int controllerIndex, unsigned int buttonId);
	static bool IsButtonDownThisFrame(unsigned int controllerIndex, unsigned int buttonId);

	// Returns [-1.0f,+1.0f] where -ve is left and +ve is right
	static float GetAxisValue(unsigned int controllerIndex, unsigned int axisId);

	// SDL event handlers
	static void OnKeyDown(const SDL_KeyboardEvent& key);
	static void OnKeyUp(const SDL_KeyboardEvent& key);

	static void OnJoyDeviceAdded(const SDL_JoyDeviceEvent& jdevice);
	static void OnJoyDeviceRemoved(const SDL_JoyDeviceEvent& jdevice);
	static void OnJoyButtonDown(const SDL_JoyButtonEvent& jbutton);
	static void OnJoyButtonUp(const SDL_JoyButtonEvent& jbutton);
	static void OnJoyAxisMotion(const SDL_JoyAxisEvent& jaxis);
	static void OnJoyHatMotion(const SDL_JoyHatEvent& jhat);

	static void OnControllerDeviceAdded(const SDL_ControllerDeviceEvent& cdevice);
	static void OnControllerDeviceRemoved(const SDL_ControllerDeviceEvent& cdevice);
	static void OnControllerButtonDown(const SDL_ControllerButtonEvent& cbutton);
	static void OnControllerButtonUp(const SDL_ControllerButtonEvent& cbutton);
	static void OnControllerAxisMotion(const SDL_ControllerAxisEvent& caxis);

private:

	struct Controller
	{
		static const int kInvalidControllerInstanceId = -1;

		SDL_Joystick* pJoystick = nullptr;
		SDL_GameController* pGameController = nullptr;
		int instanceId = kInvalidControllerInstanceId;

		bool buttonDownThisFrame[SDL_CONTROLLER_BUTTON_MAX] = {false};
		bool buttonState[SDL_CONTROLLER_BUTTON_MAX] = {false};

		Sint16 axisValue[SDL_CONTROLLER_AXIS_MAX];
	};

	static void	addSdlGameController(Sint32 joystickDeviceIndex);
	static void	addSdlJoystick(Sint32 joystickDeviceIndex);
	static Controller* getControllerFromInstanceId(int instanceId);

	static bool s_keyState[SDL_NUM_SCANCODES];
	static bool s_keyDownThisFrame[SDL_NUM_SCANCODES];

	static Controller s_controller[kMaxControllers];
};

#endif
