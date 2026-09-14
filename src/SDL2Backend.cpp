#include <vector>
#include <SDL.h>
#include <Windows.h>
#include <hidsdi.h>
#include <cstring>

#include "SDL2Backend.h"

namespace XidiSDL2Plugin
{
    using namespace Xidi;

    std::wstring_view SDL2Backend::PluginName()
    {
        return L"SDL2";
    }

    /* Maximum number of physical controller slots this backend exposes to Xidi. This is
       fixed for the lifetime of the backend so that the physical controller indices handed
       out via MaxPhysicalControllerCount() stay valid and stable even as gamepads are
       hotplugged in and out at runtime. A slot with no gamepad currently assigned to it
       simply reports as not connected until something is plugged into it. Raise this if more
       simultaneous physical controllers need to be supported. */
    static constexpr int kMaxPhysicalControllers = 16;

    static std::vector<SDL_GameController*> gamepads(kMaxPhysicalControllers, nullptr);
    static int gamepadCount = kMaxPhysicalControllers;

    /* Finds the first physical controller slot that does not currently have a gamepad
       assigned to it. Returns -1 if every slot is occupied. */
    static int FindFreeGamepadSlot()
    {
        for (int i = 0; i < gamepadCount; i++)
        {
            if (gamepads[i] == nullptr)
                return i;
        }

        return -1;
    }

    /* Finds the physical controller slot currently holding the gamepad with the given SDL
       joystick instance ID. Returns -1 if no slot holds that instance. */
    static int FindGamepadSlotByInstanceId(SDL_JoystickID instanceId)
    {
        for (int i = 0; i < gamepadCount; i++)
        {
            if ((gamepads[i] != nullptr) &&
                (SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(gamepads[i])) == instanceId))
                return i;
        }

        return -1;
    }

    /* Opens a gamepad (identified by SDL joystick device index) and places it into a free
       physical controller slot, if one is available. Used both for the initial scan at
       startup and for SDL_CONTROLLERDEVICEADDED hotplug events.

       Note: unlike SDL3, SDL2's SDL_GameControllerOpen takes a device index (not a stable
       instance ID), so the index has to be resolved to an instance ID first in order to
       check for and prevent duplicate/late add events for a device we already have open. */
    static void OpenGamepadForHotplug(int deviceIndex)
    {
        const SDL_JoystickID instanceId = SDL_JoystickGetDeviceInstanceID(deviceIndex);

        if (FindGamepadSlotByInstanceId(instanceId) != -1)
            return;

        const int slot = FindFreeGamepadSlot();
        if (slot == -1)
            return; // all physical controller slots are in use; ignore this device (log when ability added)

        SDL_GameController* gp = SDL_GameControllerOpen(deviceIndex);
        if (gp == nullptr)
            return; // failed to open the device (log when ability added)

        gamepads[slot] = gp;
    }

    /* Closes and clears out the physical controller slot holding the gamepad with the given
       SDL joystick instance ID, if any. Used for SDL_CONTROLLERDEVICEREMOVED hotplug events. */
    static void CloseGamepadForHotplug(SDL_JoystickID instanceId)
    {
        const int slot = FindGamepadSlotByInstanceId(instanceId);
        if (slot == -1)
            return;

        SDL_GameControllerClose(gamepads[slot]);
        gamepads[slot] = nullptr;
    }

    /* Drains pending SDL gamepad hotplug events (connect/disconnect) and updates the
       physical controller slots accordingly. SDL reports hotplug purely as events on its
       event queue, so this has to pump that queue; it is cheap and safe to call often, and
       returns immediately once there is nothing left pending.

       Note: in SDL2, event.cdevice.which means different things depending on the event type.
       For SDL_CONTROLLERDEVICEADDED it is a device index (suitable for SDL_GameControllerOpen).
       For SDL_CONTROLLERDEVICEREMOVED it is the joystick instance ID. */
    static void ProcessHotplugEvents()
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            switch (event.type)
            {
            case SDL_CONTROLLERDEVICEADDED:
                OpenGamepadForHotplug(event.cdevice.which);
                break;

            case SDL_CONTROLLERDEVICEREMOVED:
                CloseGamepadForHotplug(event.cdevice.which);
                break;

            default:
                break;
            }
        }
    }

    /* Populates as many physical controller slots as possible with gamepads that are
       already connected at startup, before any hotplug events have had a chance to fire. */
    static void ScanForGamepads()
    {
        const int numJoysticks = SDL_NumJoysticks();

        for (int i = 0; i < numJoysticks; i++)
        {
            if (SDL_IsGameController(i))
                OpenGamepadForHotplug(i);
        }
    }

    bool SDL2Backend::Initialize()
    {
        if (SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1") == SDL_FALSE)
            return false;
        if (SDL_Init(SDL_INIT_GAMECONTROLLER | SDL_INIT_HAPTIC) != 0)
            return false;

        /* Fill in whatever physical controller slots already have a gamepad connected.
           Anything that connects or disconnects afterward is picked up as a hotplug event
           the next time ReadInputState() runs. */
        ScanForGamepads();

        return true;
    }

    TPhysicalControllerIndex SDL2Backend::MaxPhysicalControllerCount()
    {
        return gamepadCount;
    }

    bool SDL2Backend::SupportsControllerByGuidAndPath(const wchar_t* guidAndPath)
    {
        HANDLE hDevice = CreateFileW(guidAndPath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

        if (hDevice == INVALID_HANDLE_VALUE)
            return false;

        HIDD_ATTRIBUTES attributes = { .Size = sizeof(HIDD_ATTRIBUTES) };
        HidD_GetAttributes(hDevice, &attributes);
        CloseHandle(hDevice);

        for (int i = 0; i < gamepadCount; i++)
        {
            if (gamepads[i] == nullptr)
                continue;

            if (attributes.VendorID == SDL_GameControllerGetVendor(gamepads[i]) &&
                attributes.ProductID == SDL_GameControllerGetProduct(gamepads[i]))
                return true;
        }

        return false;
    }

    SPhysicalControllerCapabilities SDL2Backend::GetCapabilities()
    {
        /* right now Xidi provides no way to alter this per-controller, so we claim to support everything */
        return {
            .stick = Controller::kPhysicalCapabilitiesAllAnalogSticks,
            .trigger = Controller::kPhysicalCapabilitiesAllAnalogTriggers,
            .button = Controller::kPhysicalCapabilitiesAllButtons,
            .forceFeedbackActuator = Controller::kPhysicalCapabilitiesAllForceFeedbackActuators
        };
    }

    SPhysicalControllerState SDL2Backend::ReadInputState(TPhysicalControllerIndex physicalControllerIndex)
    {
        /* Pump SDL's event queue for gamepad hotplug (connect/disconnect) notifications and
           update our physical controller slots accordingly. Safe to call on every poll of
           every physical controller index; if nothing is pending it returns immediately. */
        ProcessHotplugEvents();

        SDL_GameControllerUpdate();
        SDL_GameController* gp = gamepads[physicalControllerIndex];

        if (gp == nullptr)
            return {.deviceStatus = Controller::EPhysicalDeviceStatus::NotConnected};

        if (!SDL_GameControllerGetAttached(gp))
        {
            SDL_GameControllerClose(gp);
            gamepads[physicalControllerIndex] = nullptr;

            return {.deviceStatus = Controller::EPhysicalDeviceStatus::NotConnected
            };
        }

        SPhysicalControllerState state = {
            .deviceStatus = Controller::EPhysicalDeviceStatus::Ok,
            .stick = {
                /* vertical axes in SDL2 are inverted compared to what Xidi expects,
                 * and all axes are not clamped to -32767) so we need to do it manually here */
                std::max<Sint16>(SDL_GameControllerGetAxis(gp, SDL_CONTROLLER_AXIS_LEFTX), -32767),
                static_cast<int16_t>(-std::max<Sint16>(SDL_GameControllerGetAxis(gp, SDL_CONTROLLER_AXIS_LEFTY), -32767)),
                std::max<Sint16>(SDL_GameControllerGetAxis(gp, SDL_CONTROLLER_AXIS_RIGHTX), -32767),
                static_cast<int16_t>(-std::max<Sint16>(SDL_GameControllerGetAxis(gp, SDL_CONTROLLER_AXIS_RIGHTY), -32767))
            },
            .trigger = {
                /* Xidi expects triggers from 0-255, where SDL expects 0-32767, so integer divide by 128 */
                static_cast<uint8_t>(SDL_GameControllerGetAxis(gp, SDL_CONTROLLER_AXIS_TRIGGERLEFT) / 128),
                static_cast<uint8_t>(SDL_GameControllerGetAxis(gp, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) / 128)
            },
            .button = [&]() -> std::bitset<16>
            {
                /* this lambda is annoying but bitset doesn't support brace initialization, don't know of a better
                 * way to do this */
                std::bitset<16> button;

                button[static_cast<uint8_t>(Controller::EPhysicalButton::DpadUp)] =
                    SDL_GameControllerGetButton(gp, SDL_CONTROLLER_BUTTON_DPAD_UP);
                button[static_cast<uint8_t>(Controller::EPhysicalButton::DpadDown)] =
                    SDL_GameControllerGetButton(gp, SDL_CONTROLLER_BUTTON_DPAD_DOWN);
                button[static_cast<uint8_t>(Controller::EPhysicalButton::DpadLeft)] =
                    SDL_GameControllerGetButton(gp, SDL_CONTROLLER_BUTTON_DPAD_LEFT);
                button[static_cast<uint8_t>(Controller::EPhysicalButton::DpadRight)] =
                    SDL_GameControllerGetButton(gp, SDL_CONTROLLER_BUTTON_DPAD_RIGHT);
                button[static_cast<uint8_t>(Controller::EPhysicalButton::Start)] =
                    SDL_GameControllerGetButton(gp, SDL_CONTROLLER_BUTTON_START);
                button[static_cast<uint8_t>(Controller::EPhysicalButton::Back)] =
                    SDL_GameControllerGetButton(gp, SDL_CONTROLLER_BUTTON_BACK);
                button[static_cast<uint8_t>(Controller::EPhysicalButton::LS)] =
                    SDL_GameControllerGetButton(gp, SDL_CONTROLLER_BUTTON_LEFTSTICK);
                button[static_cast<uint8_t>(Controller::EPhysicalButton::RS)] =
                    SDL_GameControllerGetButton(gp, SDL_CONTROLLER_BUTTON_RIGHTSTICK);
                button[static_cast<uint8_t>(Controller::EPhysicalButton::LB)] =
                    SDL_GameControllerGetButton(gp, SDL_CONTROLLER_BUTTON_LEFTSHOULDER);
                button[static_cast<uint8_t>(Controller::EPhysicalButton::RB)] =
                    SDL_GameControllerGetButton(gp, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);
                button[static_cast<uint8_t>(Controller::EPhysicalButton::Guide)] =
                    SDL_GameControllerGetButton(gp, SDL_CONTROLLER_BUTTON_GUIDE);
                button[static_cast<uint8_t>(Controller::EPhysicalButton::Share)] =
                    /* games usually map the same control to share and touchpad, so use touchpad if available, otherwise
                    * use misc1 (which is share on Xbox controllers). SDL_CONTROLLER_BUTTON_TOUCHPAD, MISC1, and
                    * SDL_GameControllerHasButton all require SDL 2.0.14 or newer. */
                    SDL_GameControllerHasButton(gp, SDL_CONTROLLER_BUTTON_TOUCHPAD) ?
                        SDL_GameControllerGetButton(gp, SDL_CONTROLLER_BUTTON_TOUCHPAD) :
                        SDL_GameControllerGetButton(gp, SDL_CONTROLLER_BUTTON_MISC1);
                button[static_cast<uint8_t>(Controller::EPhysicalButton::A)] =
                    SDL_GameControllerGetButton(gp, SDL_CONTROLLER_BUTTON_A);
                button[static_cast<uint8_t>(Controller::EPhysicalButton::B)] =
                    SDL_GameControllerGetButton(gp, SDL_CONTROLLER_BUTTON_B);
                button[static_cast<uint8_t>(Controller::EPhysicalButton::X)] =
                    SDL_GameControllerGetButton(gp, SDL_CONTROLLER_BUTTON_X);
                button[static_cast<uint8_t>(Controller::EPhysicalButton::Y)] =
                    SDL_GameControllerGetButton(gp, SDL_CONTROLLER_BUTTON_Y);

                return button;
            }()
        };

        return state;
    }

    bool SDL2Backend::WriteForceFeedbackState(TPhysicalControllerIndex physicalControllerIndex,
        SPhysicalControllerVibration vibrationState)
    {
        SDL_GameController* gp = gamepads[physicalControllerIndex];
        if (gp == nullptr)
            return false;

        /* SDL2 has no properties system to query rumble support ahead of time as SDL3 does;
           instead just attempt the rumble and rely on it being a harmless no-op (return code
           -1) on controllers that don't support it. */
        SDL_GameControllerRumble(gp, vibrationState.leftMotor, vibrationState.rightMotor, 250);
        SDL_GameControllerRumbleTriggers(gp, vibrationState.leftImpulseTrigger, vibrationState.rightImpulseTrigger, 250);

        SDL_GameControllerUpdate();
        return true;
    }
}
