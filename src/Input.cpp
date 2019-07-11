#include "Input.h"

#include "Assert.h"
#include "Helpers.h"

#include <stdio.h>
#include <stdint.h>

bool Input::s_keyState[SDL_NUM_SCANCODES];
bool Input::s_keyDownThisFrame[SDL_NUM_SCANCODES];

Input::Controller Input::s_controller[Input::kMaxControllers];

void Input::Init()
{
	for(unsigned int i = 0; i < SDL_NUM_SCANCODES; ++i)
	{
		s_keyState[i] = false;
		s_keyDownThisFrame[i] = false;
	}

	for(unsigned int i = 0; i < COUNTOF_ARRAY(s_controller); ++i)
	{
		for(unsigned int j = 0; j < COUNTOF_ARRAY(s_controller[i].buttonState); ++j)
		{
			s_controller[i].buttonState[j] = false;
		}

		for(unsigned int j = 0; j < COUNTOF_ARRAY(s_controller[i].axisValue); ++j)
		{
			s_controller[i].axisValue[j] = 0;
		}
	}

	SDL_GameControllerAddMappingsFromFile("gamecontrollerdb.txt"); // https://github.com/gabomdq/SDL_GameControllerDB

	int numJoysticks = SDL_NumJoysticks();
	printf("Found %d joysticks\n", numJoysticks);
	for(int i = 0; i < numJoysticks; ++i)
	{
		if(SDL_IsGameController(i))
			addSdlGameController(i);
		else
			addSdlJoystick(i);
	}

	// "XInput Controller" = Wired XBOX360 controller
}

void Input::Shutdown()
{
	for(unsigned int i = 0; i < COUNTOF_ARRAY(s_controller); ++i)
	{
		if(s_controller[i].instanceId != Controller::kInvalidControllerInstanceId)
		{
			Controller& controller = s_controller[i];
			if(controller.pJoystick)
			{
				SDL_JoystickClose(controller.pJoystick);
				controller.pJoystick = nullptr;
			}
			else if(controller.pGameController)
			{
				SDL_GameControllerClose(controller.pGameController);
				s_controller[i].pGameController = nullptr;
			}

			s_controller[i].instanceId = Controller::kInvalidControllerInstanceId;
		}
	}
}

void Input::FrameStart()
{
	for(unsigned int i = 0; i < SDL_NUM_SCANCODES; ++i)
	{
		s_keyDownThisFrame[i] = false;
	}

	for(unsigned int i = 0; i < kMaxControllers; ++i)
	{
		for(unsigned int j = 0; j < SDL_CONTROLLER_BUTTON_MAX; ++j)
		{
			s_controller[i].buttonDownThisFrame[j] = false;
		}
	}
}

bool Input::GetKeyState(SDL_Scancode scancode)
{
	return s_keyState[scancode];
}

bool Input::IsKeyDownThisFrame(SDL_Scancode scancode)
{
	return s_keyDownThisFrame[scancode];
}

bool Input::IsControllerConnected(unsigned int index)
{
	HP_ASSERT(index < kMaxControllers);
	return s_controller[index].instanceId != Controller::kInvalidControllerInstanceId;
}

const char* Input::GetControllerName(unsigned int index)
{
	HP_ASSERT(index < kMaxControllers);

	if(!IsControllerConnected(index))
		return "<no controller>";

	const Controller& controller = s_controller[index];
	if(controller.pGameController)
		return SDL_GameControllerName(controller.pGameController);
	else
		return SDL_JoystickName(controller.pJoystick);
}

bool Input::GetButtonState(unsigned int controllerIndex, unsigned int buttonId)
{
	HP_ASSERT(controllerIndex < kMaxControllers);
	HP_ASSERT(buttonId < SDL_CONTROLLER_BUTTON_MAX);
	HP_ASSERT(IsControllerConnected(controllerIndex));
	return s_controller[controllerIndex].buttonState[buttonId];
}

bool Input::IsButtonDownThisFrame(unsigned int controllerIndex, unsigned int buttonId)
{
	HP_ASSERT(controllerIndex < kMaxControllers);
	HP_ASSERT(buttonId < SDL_CONTROLLER_BUTTON_MAX);
	HP_ASSERT(IsControllerConnected(controllerIndex));
	return s_controller[controllerIndex].buttonDownThisFrame[buttonId];
}

float Input::GetAxisValue(unsigned int controllerIndex, unsigned int axisId)
{
	HP_ASSERT(controllerIndex < kMaxControllers);
	HP_ASSERT(axisId < SDL_CONTROLLER_AXIS_MAX);
	HP_ASSERT(IsControllerConnected(controllerIndex));

	const Sint16 iValue = s_controller[controllerIndex].axisValue[axisId];

	// apply deadzone
	
	static float s_analogueStickDeadZone = 0.15f;

	float value = (float)iValue/(float)INT16_MAX; // [-1.0f, 1.0f]   
	float magnitude = fabsf(value);
	magnitude = Max(0.0f, (magnitude - s_analogueStickDeadZone) / (1.0f - s_analogueStickDeadZone));
	float direction = value > 0.0f ? 1.0f : -1.0f;
	value = magnitude * direction;
	return value;
}

void Input::OnJoyDeviceAdded(const SDL_JoyDeviceEvent& jdevice)
{
	Sint32 deviceIndex = jdevice.which;
//	printf("SDL_JOYDEVICEADDED, device index: %d\n", deviceIndex);

	// A SDL_JOYDEVICEADDED event is generated even if a device classed as an SDL Game Controller is added...
	if(SDL_IsGameController(deviceIndex))
		return;

	addSdlJoystick(deviceIndex);
}

void Input::OnJoyDeviceRemoved(const SDL_JoyDeviceEvent& jdevice)
{
	Sint32 instanceId = jdevice.which; // jdevice.which is the instance id for the REMOVED event
//	printf("SDL_JOYDEVICEREMOVED, instanceId: %d\n", instanceId);

	for(unsigned int i = 0; i < kMaxControllers; ++i)
	{
		if(s_controller[i].instanceId == instanceId)
		{
			Controller& controller = s_controller[i];

			// A SDL_JOYDEVICEREMOVED event is generated even if a device classed as an SDL Game Controller is added...
			if(controller.pGameController)
				return;

			printf("Removed controller %i instance ID %i\n", i, instanceId);

			SDL_JoystickClose(controller.pJoystick);

			// invalidate
			controller.pJoystick = nullptr;
			controller.instanceId = Controller::kInvalidControllerInstanceId;
			return;
		}
	}

//	HP_FATAL_ERROR("Unknown joystick removed"); // Can't error here, because if remove a SDL Controller SDL_JOYDEVICEREMOVED fires after SDL_CONTROLLERDEVICEREMOVED  
}

void Input::OnJoyButtonDown(const SDL_JoyButtonEvent& jbutton)
{
	Sint32 instanceId = jbutton.which;
//	printf("SDL_JOYBUTTONDOWN  ControllerInstanceId: %u  Button: %u  State: %s\n", instanceId, jbutton.button, jbutton.state == SDL_RELEASED ? "SDL_RELEASED" : "SDL_PRESSED");
	Controller& controller = getControllerFromInstanceId(instanceId);
	controller.buttonDownThisFrame[jbutton.button] = true;
	controller.buttonState[jbutton.button] = true;
}

void Input::OnJoyButtonUp(const SDL_JoyButtonEvent& jbutton)
{
	Sint32 instanceId = jbutton.which;
//	printf("SDL_JOYBUTTONUP  InstanceId: %u  Button: %u  State: %s\n", jbutton.which, jbutton.button, jbutton.state == SDL_RELEASED ? "SDL_RELEASED" : "SDL_PRESSED");
	Controller& controller = getControllerFromInstanceId(instanceId);
	controller.buttonState[jbutton.button] = false;
}

void Input::OnJoyAxisMotion(const SDL_JoyAxisEvent& jaxis)
{
	Sint32 instanceId = jaxis.which;
//	printf("JoyAxisMotion, instanceId: %d  axis: %u  value: %d\n", instanceId, jaxis.axis, jaxis.value);

	Controller& controller = getControllerFromInstanceId(instanceId);

	if(jaxis.axis == 0)
	{ 
		// horizontal

		if(jaxis.value > 0)
		{
			controller.buttonDownThisFrame[SDL_CONTROLLER_BUTTON_DPAD_RIGHT] = true;
			controller.buttonState[SDL_CONTROLLER_BUTTON_DPAD_RIGHT] = true;
			controller.buttonState[SDL_CONTROLLER_BUTTON_DPAD_LEFT] = false;
		}
		else if(jaxis.value < 0)
		{
			controller.buttonDownThisFrame[SDL_CONTROLLER_BUTTON_DPAD_LEFT] = true;
			controller.buttonState[SDL_CONTROLLER_BUTTON_DPAD_LEFT] = true;
			controller.buttonState[SDL_CONTROLLER_BUTTON_DPAD_RIGHT] = false;
		}
		else // jaxis.value == 0
		{
			controller.buttonState[SDL_CONTROLLER_BUTTON_DPAD_RIGHT] = false;
			controller.buttonState[SDL_CONTROLLER_BUTTON_DPAD_LEFT] = false;
		}
	}
	else if(jaxis.axis == 1)
	{
		// vertical

		if(jaxis.value > 0)
		{
			controller.buttonDownThisFrame[SDL_CONTROLLER_BUTTON_DPAD_DOWN] = true;
			controller.buttonState[SDL_CONTROLLER_BUTTON_DPAD_DOWN] = true;
			controller.buttonState[SDL_CONTROLLER_BUTTON_DPAD_UP] = false;
		}
		else if(jaxis.value < 0)
		{
			controller.buttonDownThisFrame[SDL_CONTROLLER_BUTTON_DPAD_UP] = true;
			controller.buttonState[SDL_CONTROLLER_BUTTON_DPAD_UP] = true;
			controller.buttonState[SDL_CONTROLLER_BUTTON_DPAD_DOWN] = false;
		}
		else // jaxis.value == 0
		{
			controller.buttonState[SDL_CONTROLLER_BUTTON_DPAD_UP] = false;
			controller.buttonState[SDL_CONTROLLER_BUTTON_DPAD_DOWN] = false;
		}
	}
}

// this shouldn't be needed: joy axes provide all the directional info
void Input::OnJoyHatMotion(const SDL_JoyHatEvent& jhat)
{
	Sint32 instanceId = jhat.which;
	HP_UNUSED(instanceId);
#if 0
	printf("JoyHatMotion, instanceId: %d  hat index: %u  value: %s %s %s %s\n", instanceId, jhat.hat,
		(jhat.value & SDL_HAT_UP) ? "SDL_HAT_UP" : "",
		(jhat.value & SDL_HAT_RIGHT) ? "SDL_HAT_RIGHT" : "",
		(jhat.value & SDL_HAT_DOWN) ? "SDL_HAT_DOWN" : "",
		(jhat.value & SDL_HAT_LEFT) ? "SDL_HAT_LEFT" : "");
#endif
}

void Input::OnControllerDeviceAdded(const SDL_ControllerDeviceEvent& cdevice)
{
	// cdevice.which is the the joystick device index for the ADDED event
	addSdlGameController(cdevice.which);
}

void Input::addSdlGameController(Sint32 deviceIndex)
{
	HP_ASSERT(SDL_IsGameController(deviceIndex));
	HP_ASSERT(deviceIndex < (Sint32)kMaxControllers);
	
	const char* name = SDL_GameControllerNameForIndex(deviceIndex);
	SDL_GameController* pGameController = SDL_GameControllerOpen(deviceIndex);
	if(!pGameController)
	{
		printf("Failed to open game controller %i\n", deviceIndex);
	}

	SDL_Joystick* pJoystick = SDL_GameControllerGetJoystick(pGameController);
	int instanceId = SDL_JoystickInstanceID(pJoystick);
	for(unsigned int i = 0; i < kMaxControllers; ++i)
	{
		if(s_controller[i].instanceId == instanceId)
		{
			printf("Controller with instance id %i has already been added\n", instanceId);
			return;
		}
	}

	s_controller[deviceIndex].pJoystick = nullptr; // don't store point to joystick if is an SDL Controller
	s_controller[deviceIndex].pGameController = pGameController;
	s_controller[deviceIndex].instanceId = instanceId;

	printf("Added controller %i instance id %i: \"%s\"\n", deviceIndex, instanceId, name);
}

void Input::addSdlJoystick(Sint32 deviceIndex)
{
	HP_ASSERT(!SDL_IsGameController(deviceIndex));
	HP_ASSERT(deviceIndex < (Sint32)kMaxControllers);

	SDL_Joystick* pJoystick = SDL_JoystickOpen(deviceIndex);
	SDL_JoystickID instanceID = SDL_JoystickGetDeviceInstanceID(deviceIndex);
	s_controller[deviceIndex].pJoystick = pJoystick;
	s_controller[deviceIndex].pGameController = nullptr;
	s_controller[deviceIndex].instanceId = instanceID;

	const char* name = SDL_JoystickName(pJoystick);
	printf("Added controller %i instance id %i: \"%s\"\n", deviceIndex, instanceID, name);
}

void Input::OnControllerDeviceRemoved(const SDL_ControllerDeviceEvent& cdevice)
{
	// SDL_CONTROLLERDEVICEREMOVED
	// cdevice.which is the instance id for the REMOVED event
	const int instanceId = cdevice.which;

	for(unsigned int i = 0; i < kMaxControllers; ++i)
	{
		if(s_controller[i].instanceId == instanceId)
		{
			printf("Removed controller %i instance ID %i\n", i, cdevice.which);

			SDL_GameControllerClose(s_controller[i].pGameController);

			// invalidate the controller
			s_controller[i].pGameController = nullptr;
			s_controller[i].instanceId = Controller::kInvalidControllerInstanceId;
			return;
		}
	}

	HP_FATAL_ERROR("Unknown controller removed");
}

void Input::OnControllerButtonDown(const SDL_ControllerButtonEvent& cbutton)
{
//	printf("SDL_CONTROLLERBUTTONDOWN  ControllerInstanceId: %u  Button: %u  State: %s\n", cbutton.which, cbutton.button, cbutton.state == SDL_RELEASED ? "SDL_RELEASED" : "SDL_PRESSED");
	int instanceId = cbutton.which;
	Controller& controller = getControllerFromInstanceId(instanceId);
	controller.buttonDownThisFrame[cbutton.button] = true;
	controller.buttonState[cbutton.button] = true;
}

void Input::OnControllerButtonUp(const SDL_ControllerButtonEvent& cbutton)
{
//	printf("SDL_CONTROLLERBUTTONUP  ControllerInstanceId: %u  Button: %u  State: %s\n", cbutton.which, cbutton.button, cbutton.state == SDL_RELEASED ? "SDL_RELEASED" : "SDL_PRESSED");
	int instanceId = cbutton.which;
	Controller& controller = getControllerFromInstanceId(instanceId);
	controller.buttonState[cbutton.button] = false;
}

void Input::OnControllerAxisMotion(const SDL_ControllerAxisEvent& caxis)
{
	static bool printAxisMotionEvent = false;
	if(printAxisMotionEvent)
	{
		if(abs(caxis.value) > 10000)
			printf("SDL_CONTROLLERAXISMOTION  Joystick: %u  Axis: %u  Value: %d\n", caxis.which, caxis.axis, caxis.value);
	}

	int instanceId = caxis.which;
	getControllerFromInstanceId(instanceId).axisValue[caxis.axis] = caxis.value;
}

Input::Controller& Input::getControllerFromInstanceId(int instanceId)
{
	for(unsigned int i = 0; i < kMaxControllers; ++i)
	{
		if(s_controller[i].instanceId == instanceId)
			return s_controller[i];
	}

	HP_FATAL_ERROR("Failed to find controller\n");
	return *(Controller*)nullptr;
}

void Input::OnKeyDown(const SDL_KeyboardEvent& key)
{
//	printf("SDL_KEYDOWN Keycode: %i repeat: %u\n", key.keysym.sym, key.repeat);

	if(key.repeat == 0)
	{
		s_keyState[key.keysym.scancode] = true;
		s_keyDownThisFrame[key.keysym.scancode] = true;
	}
}

void Input::OnKeyUp(const SDL_KeyboardEvent& key)
{
//	printf("SDL_KEYUP Keycode: %i\n", key.keysym.sym);
	s_keyState[key.keysym.scancode] = false;
}
