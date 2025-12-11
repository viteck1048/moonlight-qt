#include <Limelight.h>
#include "SDL_compat.h"
#include "streaming/session.h"
#include "settings/mappingmanager.h"
#include "path.h"
#include "utils.h"

#include <QtGlobal>
#include <QGuiApplication>

namespace {
static constexpr SDL_Keymod KMOD_VIRTUAL_EVENT = (SDL_Keymod)0x0020;
}

SdlInputHandler::SdlInputHandler(StreamingPreferences& prefs,
                                 int streamWidth,
                                 int streamHeight,
                                 const QVector<UserKeyComboHx>& userCombos,
                                 int initialDisplayCount)
    : m_MultiController(prefs.multiController),
      m_GamepadMouse(prefs.gamepadMouse),
      m_SwapMouseButtons(prefs.swapMouseButtons),
      m_ReverseScrollDirection(prefs.reverseScrollDirection),
      m_SwapFaceButtons(prefs.swapFaceButtons),
      m_MouseWasInVideoRegion(false),
      m_PendingMouseButtonsAllUpOnVideoRegionLeave(false),
      m_PointerRegionLockActive(prefs.pointerRegionLockActive),
      m_PointerRegionLockToggledByUser(!prefs.pointerRegionLockActive),
      //m_PointerRegionLockToggledByUser(true),
      m_FakeCaptureActive(false),
      m_CaptureSystemKeysMode(prefs.captureSysKeysMode),
      m_MouseCursorCapturedVisibilityState(SDL_DISABLE),
      m_LongPressTimer(0),
      m_StreamWidth(streamWidth),
      m_StreamHeight(streamHeight),
      m_AbsoluteMouseMode(prefs.absoluteMouseMode),
      m_AbsoluteTouchMode(prefs.absoluteTouchMode),
      m_DisabledTouchFeedback(false),
      m_LeftButtonReleaseTimer(0),
      m_RightButtonReleaseTimer(0),
      m_DragTimer(0),
      m_DragButton(0),
      m_NumFingersDown(0),
      m_IsProcessingUserCombo(false),
      m_UserCombosEnabled(prefs.userCombosEnabled)
{
    m_UserKeyCombos = userCombos;
    dspl_len = qBound(1, initialDisplayCount, 9);
    dspl_view = 0;

    // System keys are always captured when running without a DE
    if (!WMUtils::isRunningDesktopEnvironment()) {
        m_CaptureSystemKeysMode = StreamingPreferences::CSK_ALWAYS;
    }

    // Allow gamepad input when the app doesn't have focus if requested
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, prefs.backgroundGamepad ? "1" : "0");

#if !SDL_VERSION_ATLEAST(2, 0, 15)
    // For older versions of SDL (2.0.14 and earlier), use SDL_HINT_GRAB_KEYBOARD
    SDL_SetHintWithPriority(SDL_HINT_GRAB_KEYBOARD,
                            m_CaptureSystemKeysMode != StreamingPreferences::CSK_OFF ? "1" : "0",
                            SDL_HINT_OVERRIDE);
#endif

    // Opt-out of SDL's built-in Alt+Tab handling while keyboard grab is enabled
    SDL_SetHint(SDL_HINT_ALLOW_ALT_TAB_WHILE_GRABBED, "0");

    // Allow clicks to pass through to us when focusing the window. If we're in
    // absolute mouse mode, this will avoid the user having to click twice to
    // trigger a click on the host if the Moonlight window is not focused. In
    // relative mode, the click event will trigger the mouse to be recaptured.
    SDL_SetHint(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "1");

    // Enabling extended input reports allows rumble to function on Bluetooth PS4/PS5
    // controllers, but breaks DirectInput applications. We will enable it because
    // it's likely that working rumble is what the user is expecting. If they don't
    // want this behavior, they can override it with the environment variable.
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS4_RUMBLE, "1");
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS5_RUMBLE, "1");

    // Populate special key combo configuration
    m_SpecialKeyCombos[KeyComboQuit].keyCombo = KeyComboQuit;
    m_SpecialKeyCombos[KeyComboQuit].keyCode = SDLK_q;
    m_SpecialKeyCombos[KeyComboQuit].scanCode = SDL_SCANCODE_Q;
    m_SpecialKeyCombos[KeyComboQuit].enabled = true;

    m_SpecialKeyCombos[KeyComboUngrabInput].keyCombo = KeyComboUngrabInput;
    m_SpecialKeyCombos[KeyComboUngrabInput].keyCode = SDLK_z;
    m_SpecialKeyCombos[KeyComboUngrabInput].scanCode = SDL_SCANCODE_Z;
    m_SpecialKeyCombos[KeyComboUngrabInput].enabled = QGuiApplication::platformName() != "eglfs";

    m_SpecialKeyCombos[KeyComboToggleFullScreen].keyCombo = KeyComboToggleFullScreen;
    m_SpecialKeyCombos[KeyComboToggleFullScreen].keyCode = SDLK_x;
    m_SpecialKeyCombos[KeyComboToggleFullScreen].scanCode = SDL_SCANCODE_X;
    m_SpecialKeyCombos[KeyComboToggleFullScreen].enabled = QGuiApplication::platformName() != "eglfs";

    m_SpecialKeyCombos[KeyComboToggleStatsOverlay].keyCombo = KeyComboToggleStatsOverlay;
    m_SpecialKeyCombos[KeyComboToggleStatsOverlay].keyCode = SDLK_s;
    m_SpecialKeyCombos[KeyComboToggleStatsOverlay].scanCode = SDL_SCANCODE_S;
    m_SpecialKeyCombos[KeyComboToggleStatsOverlay].enabled = true;

    m_SpecialKeyCombos[KeyComboToggleMouseMode].keyCombo = KeyComboToggleMouseMode;
    m_SpecialKeyCombos[KeyComboToggleMouseMode].keyCode = SDLK_m;
    m_SpecialKeyCombos[KeyComboToggleMouseMode].scanCode = SDL_SCANCODE_M;
    m_SpecialKeyCombos[KeyComboToggleMouseMode].enabled = true;

    m_SpecialKeyCombos[KeyComboToggleCursorHide].keyCombo = KeyComboToggleCursorHide;
    m_SpecialKeyCombos[KeyComboToggleCursorHide].keyCode = SDLK_c;
    m_SpecialKeyCombos[KeyComboToggleCursorHide].scanCode = SDL_SCANCODE_C;
    m_SpecialKeyCombos[KeyComboToggleCursorHide].enabled = true;

    m_SpecialKeyCombos[KeyComboToggleMinimize].keyCombo = KeyComboToggleMinimize;
    m_SpecialKeyCombos[KeyComboToggleMinimize].keyCode = SDLK_d;
    m_SpecialKeyCombos[KeyComboToggleMinimize].scanCode = SDL_SCANCODE_D;
    m_SpecialKeyCombos[KeyComboToggleMinimize].enabled = QGuiApplication::platformName() != "eglfs";

    m_SpecialKeyCombos[KeyComboPasteText].keyCombo = KeyComboPasteText;
    m_SpecialKeyCombos[KeyComboPasteText].keyCode = SDLK_v;
    m_SpecialKeyCombos[KeyComboPasteText].scanCode = SDL_SCANCODE_V;
    m_SpecialKeyCombos[KeyComboPasteText].enabled = true;

    m_SpecialKeyCombos[KeyComboTogglePointerRegionLock].keyCombo = KeyComboTogglePointerRegionLock;
    m_SpecialKeyCombos[KeyComboTogglePointerRegionLock].keyCode = SDLK_l;
    m_SpecialKeyCombos[KeyComboTogglePointerRegionLock].scanCode = SDL_SCANCODE_L;
    m_SpecialKeyCombos[KeyComboTogglePointerRegionLock].enabled = true;

    m_SpecialKeyCombos[KeyComboToggleUserCombos].keyCombo = KeyComboToggleUserCombos;
    m_SpecialKeyCombos[KeyComboToggleUserCombos].keyCode = SDLK_u;
    m_SpecialKeyCombos[KeyComboToggleUserCombos].scanCode = SDL_SCANCODE_U;
    m_SpecialKeyCombos[KeyComboToggleUserCombos].enabled = true;

    m_SpecialKeyCombos[KeyComboQuitAndExit].keyCombo = KeyComboQuitAndExit;
    m_SpecialKeyCombos[KeyComboQuitAndExit].keyCode = SDLK_e;
    m_SpecialKeyCombos[KeyComboQuitAndExit].scanCode = SDL_SCANCODE_E;
    m_SpecialKeyCombos[KeyComboQuitAndExit].enabled = true;

    m_SpecialKeyCombos[KeyComboSetDsplLen].keyCombo = KeyComboSetDsplLen;
    m_SpecialKeyCombos[KeyComboSetDsplLen].keyCode = SDLK_p;
    m_SpecialKeyCombos[KeyComboSetDsplLen].scanCode = SDL_SCANCODE_P;
    m_SpecialKeyCombos[KeyComboSetDsplLen].enabled = true;

    m_SpecialKeyCombos[KeyComboNextDsplView].keyCombo = KeyComboNextDsplView;
    m_SpecialKeyCombos[KeyComboNextDsplView].keyCode = SDLK_o;
    m_SpecialKeyCombos[KeyComboNextDsplView].scanCode = SDL_SCANCODE_O;
    m_SpecialKeyCombos[KeyComboNextDsplView].enabled = true;

    m_OldIgnoreDevices = SDL_GetHint(SDL_HINT_GAMECONTROLLER_IGNORE_DEVICES);
    m_OldIgnoreDevicesExcept = SDL_GetHint(SDL_HINT_GAMECONTROLLER_IGNORE_DEVICES_EXCEPT);

    QString streamIgnoreDevices = qgetenv("STREAM_GAMECONTROLLER_IGNORE_DEVICES");
    QString streamIgnoreDevicesExcept = qgetenv("STREAM_GAMECONTROLLER_IGNORE_DEVICES_EXCEPT");

    if (!streamIgnoreDevices.isEmpty() && !streamIgnoreDevices.endsWith(',')) {
        streamIgnoreDevices += ',';
    }
    streamIgnoreDevices += m_OldIgnoreDevices;

    // STREAM_IGNORE_DEVICE_GUIDS allows to specify additional devices to be ignored when starting
    // the stream in case the scope of STREAM_GAMECONTROLLER_IGNORE_DEVICES is too broad. One such
    // case is "Steam Virtual Gamepad" where everything is under the same VID/PID, but different GUIDs.
    // Multiple GUIDs can be provided, but need to be separated by commas:
    //
    //     <GUID>,<GUID>,<GUID>,...
    //
    QString streamIgnoreDeviceGuids = qgetenv("STREAM_IGNORE_DEVICE_GUIDS");
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    m_IgnoreDeviceGuids = streamIgnoreDeviceGuids.split(',', Qt::SkipEmptyParts);
#else
    m_IgnoreDeviceGuids = streamIgnoreDeviceGuids.split(',', QString::SkipEmptyParts);
#endif

    // For SDL_HINT_GAMECONTROLLER_IGNORE_DEVICES, we use the union of SDL_GAMECONTROLLER_IGNORE_DEVICES
    // and STREAM_GAMECONTROLLER_IGNORE_DEVICES while streaming. STREAM_GAMECONTROLLER_IGNORE_DEVICES_EXCEPT
    // overrides SDL_GAMECONTROLLER_IGNORE_DEVICES_EXCEPT while streaming.
    SDL_SetHint(SDL_HINT_GAMECONTROLLER_IGNORE_DEVICES, streamIgnoreDevices.toUtf8());
    SDL_SetHint(SDL_HINT_GAMECONTROLLER_IGNORE_DEVICES_EXCEPT, streamIgnoreDevicesExcept.toUtf8());

    // We must initialize joystick explicitly before gamecontroller in order
    // to ensure we receive gamecontroller attach events for gamepads where
    // SDL doesn't have a built-in mapping. By starting joystick first, we
    // can allow mapping manager to update the mappings before GC attach
    // events are generated.
    SDL_assert(!SDL_WasInit(SDL_INIT_JOYSTICK));
    if (SDL_InitSubSystem(SDL_INIT_JOYSTICK) != 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "SDL_InitSubSystem(SDL_INIT_JOYSTICK) failed: %s",
                     SDL_GetError());
    }

    MappingManager mappingManager;
    mappingManager.applyMappings();

    // Flush gamepad arrival and departure events which may be queued before
    // starting the gamecontroller subsystem again. This prevents us from
    // receiving duplicate arrival and departure events for the same gamepad.
    SDL_FlushEvent(SDL_CONTROLLERDEVICEADDED);
    SDL_FlushEvent(SDL_CONTROLLERDEVICEREMOVED);

    // We need to reinit this each time, since you only get
    // an initial set of gamepad arrival events once per init.
    SDL_assert(!SDL_WasInit(SDL_INIT_GAMECONTROLLER));
    if (SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) != 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) failed: %s",
                     SDL_GetError());
    }

#if !SDL_VERSION_ATLEAST(2, 0, 9)
    SDL_assert(!SDL_WasInit(SDL_INIT_HAPTIC));
    if (SDL_InitSubSystem(SDL_INIT_HAPTIC) != 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "SDL_InitSubSystem(SDL_INIT_HAPTIC) failed: %s",
                     SDL_GetError());
    }
#endif

    // Initialize the gamepad mask with currently attached gamepads to avoid
    // causing gamepads to unexpectedly disappear and reappear on the host
    // during stream startup as we detect currently attached gamepads one at a time.
    m_GamepadMask = getAttachedGamepadMask();

    SDL_zero(m_GamepadState);
    SDL_zero(m_LastTouchDownEvent);
    SDL_zero(m_LastTouchUpEvent);
    SDL_zero(m_TouchDownEvent);
}

SdlInputHandler::~SdlInputHandler()
{
    for (int i = 0; i < MAX_GAMEPADS; i++) {
        if (m_GamepadState[i].mouseEmulationTimer != 0) {
            Session::get()->notifyMouseEmulationMode(false);
            SDL_RemoveTimer(m_GamepadState[i].mouseEmulationTimer);
        }
#if !SDL_VERSION_ATLEAST(2, 0, 9)
        if (m_GamepadState[i].haptic != nullptr) {
            SDL_HapticClose(m_GamepadState[i].haptic);
        }
#endif
        if (m_GamepadState[i].controller != nullptr) {
            SDL_GameControllerClose(m_GamepadState[i].controller);
        }
    }

    SDL_RemoveTimer(m_LongPressTimer);
    SDL_RemoveTimer(m_LeftButtonReleaseTimer);
    SDL_RemoveTimer(m_RightButtonReleaseTimer);
    SDL_RemoveTimer(m_DragTimer);

#if !SDL_VERSION_ATLEAST(2, 0, 9)
    SDL_QuitSubSystem(SDL_INIT_HAPTIC);
    SDL_assert(!SDL_WasInit(SDL_INIT_HAPTIC));
#endif

    SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);
    SDL_assert(!SDL_WasInit(SDL_INIT_GAMECONTROLLER));

    SDL_QuitSubSystem(SDL_INIT_JOYSTICK);
    SDL_assert(!SDL_WasInit(SDL_INIT_JOYSTICK));

    // Return background event handling to off
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "0");

    // Restore the ignored devices
    SDL_SetHint(SDL_HINT_GAMECONTROLLER_IGNORE_DEVICES, m_OldIgnoreDevices.toUtf8());
    SDL_SetHint(SDL_HINT_GAMECONTROLLER_IGNORE_DEVICES_EXCEPT, m_OldIgnoreDevicesExcept.toUtf8());

#ifdef STEAM_LINK
    // Hide SDL's cursor on Steam Link after quitting the stream.
    // FIXME: We should also do this for other situations where SDL
    // and Qt will draw their own mouse cursors like KMSDRM or RPi
    // video backends.
    SDL_ShowCursor(SDL_DISABLE);
#endif
}

void SdlInputHandler::setWindow(SDL_Window *window)
{
    m_Window = window;
}

bool SdlInputHandler::matchUserKeyBinding(const UserKeyKShHx& binding, SDL_Scancode scancode, SDL_Keymod modifiers) const
{
    if (binding.scancode == SDL_SCANCODE_UNKNOWN || binding.scancode != scancode) {
        return false;
    }
    
    if((binding.modifiers & KMOD_CTRL) == KMOD_CTRL) {
        if(modifiers & KMOD_CTRL) {
            modifiers = (SDL_Keymod)(modifiers | KMOD_CTRL);
        }
    }
    if((binding.modifiers & KMOD_ALT) == KMOD_ALT) {
        if(modifiers & KMOD_ALT) {
            modifiers = (SDL_Keymod)(modifiers | KMOD_ALT);
        }
    }
    if((binding.modifiers & KMOD_SHIFT) == KMOD_SHIFT) {
        if(modifiers & KMOD_SHIFT) {
            modifiers = (SDL_Keymod)(modifiers | KMOD_SHIFT);
        }
    }
    if((binding.modifiers & KMOD_GUI) == KMOD_GUI) {
        if(modifiers & KMOD_GUI) {
            modifiers = (SDL_Keymod)(modifiers | KMOD_GUI);
        }
    }

    return binding.modifiers == modifiers;
}

bool SdlInputHandler::applyUserKeyCombo(SDL_KeyboardEvent* event)
{
    if (!m_AbsoluteMouseMode || !m_UserCombosEnabled || m_IsProcessingUserCombo || m_UserKeyCombos.isEmpty()) {
        return false;
    }

    if ((event->keysym.mod & KMOD_VIRTUAL_EVENT) != 0) {
        return false;
    }
    
    const SDL_Scancode scancode = event->keysym.scancode;
    const SDL_Keymod modifiers = (SDL_Keymod)(event->keysym.mod & (KMOD_CTRL | KMOD_ALT | KMOD_SHIFT | KMOD_GUI));
    
    for (const UserKeyComboHx& combo : m_UserKeyCombos) {
        if (!matchUserKeyBinding(combo.input, scancode, modifiers)) {
            continue;
        }
/*
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "User combo intercept: incoming scancode=%d, modifiers=0x%X, state=%s; matched binding scancode=%d, modifiers=0x%X, outputs=%d",
                    scancode,
                    modifiers,
                    event->state == SDL_PRESSED ? "pressed" : "released",
                    combo.input.scancode,
                    combo.input.modifiers,
                    combo.outputs.size());
*/
        if (combo.outputs.isEmpty()) {
            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                        "User combo configured as blocker; swallowing event (scancode=0x%X, mods=0x%X)",
                        scancode,
                        modifiers);
            return true;
            //event->keysym.scancode = SDL_SCANCODE_R;
            //return false;
        }
        else {
            if (event->state == SDL_PRESSED) {
                for (int i = 0; i < combo.outputs.size(); ++i) {
/*
                    const UserKeyKShHx& outputBinding = combo.outputs.at(i);
                    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                                "  [%d] scancode=%d, modifiers=0x%X",
                                i,
                                outputBinding.scancode,
                                outputBinding.modifiers);
*/
                }
                playbackUserKeyCombo(combo, modifiers);
            }
            return true;
        }
    }

    return false;
}

void SdlInputHandler::playbackUserKeyCombo(const UserKeyComboHx& combo, SDL_Keymod input_modifiers)
{
    if (combo.outputs.isEmpty()) {
        return;
    }

    m_IsProcessingUserCombo = true;

    SDL_KeyboardEvent synthesizedEvent = {};
    synthesizedEvent.repeat = 0;

    struct ModifierMapping {
        SDL_Keymod modifier;
        SDL_Scancode scancode;
    };

    static const ModifierMapping modifierMappings[] = {
        {KMOD_LCTRL, SDL_SCANCODE_LCTRL},
        {KMOD_RCTRL, SDL_SCANCODE_RCTRL},
        {KMOD_LSHIFT, SDL_SCANCODE_LSHIFT},
        {KMOD_RSHIFT, SDL_SCANCODE_RSHIFT},
        {KMOD_LALT, SDL_SCANCODE_LALT},
        {KMOD_RALT, SDL_SCANCODE_RALT},
        {KMOD_LGUI, SDL_SCANCODE_LGUI},
        {KMOD_RGUI, SDL_SCANCODE_RGUI},
    };

    auto iterateModifiers = [&](SDL_Keymod mask, auto&& callback) {
        for (const ModifierMapping& mapping : modifierMappings) {
            if ((mask & mapping.modifier) != 0) {
                callback(mapping);
            }
        }
    };

    SDL_Keymod activeModifiers = input_modifiers;

    auto emitModifierEvent = [&](const ModifierMapping& mapping, bool pressed) {
        if (mapping.scancode == SDL_SCANCODE_UNKNOWN) {
            return;
        }

        if (pressed) {
            activeModifiers = (SDL_Keymod)(activeModifiers | mapping.modifier);
        }

        synthesizedEvent.state = pressed ? SDL_PRESSED : SDL_RELEASED;
        synthesizedEvent.type = pressed ? SDL_KEYDOWN : SDL_KEYUP;
        synthesizedEvent.keysym.scancode = mapping.scancode;
        synthesizedEvent.keysym.mod = (SDL_Keymod)(activeModifiers | KMOD_VIRTUAL_EVENT);
        handleKeyEvent(&synthesizedEvent);

        if (!pressed) {
            activeModifiers = (SDL_Keymod)(activeModifiers & ~mapping.modifier);
        }
    };

    QVector<const ModifierMapping*> inputModifierSequence;
    iterateModifiers(activeModifiers, [&](const ModifierMapping& mapping) {
        inputModifierSequence.append(&mapping);
    });

    // Release input modifiers before injecting the outgoing combo
    for (const ModifierMapping* mapping : inputModifierSequence) {
        emitModifierEvent(*mapping, false);
    }

    for (const UserKeyKShHx& binding : combo.outputs) {
        if (binding.scancode == SDL_SCANCODE_UNKNOWN) {
            continue;
        }

        QVector<const ModifierMapping*> outputModifierSequence;
        iterateModifiers(binding.modifiers, [&](const ModifierMapping& mapping) {
            outputModifierSequence.append(&mapping);
        });

        for (const ModifierMapping* mapping : outputModifierSequence) {
            emitModifierEvent(*mapping, true);
        }

        synthesizedEvent.state = SDL_PRESSED;
        synthesizedEvent.type = SDL_KEYDOWN;
        synthesizedEvent.keysym.scancode = binding.scancode;
        synthesizedEvent.keysym.mod = (SDL_Keymod)(activeModifiers | KMOD_VIRTUAL_EVENT);
        handleKeyEvent(&synthesizedEvent);

        synthesizedEvent.state = SDL_RELEASED;
        synthesizedEvent.type = SDL_KEYUP;
        handleKeyEvent(&synthesizedEvent);

        for (auto it = outputModifierSequence.crbegin(); it != outputModifierSequence.crend(); ++it) {
            emitModifierEvent(**it, false);
        }
    }

    // Restore input modifiers so the physical release remains consistent
    for (const ModifierMapping* mapping : inputModifierSequence) {
        emitModifierEvent(*mapping, true);
    }

    m_IsProcessingUserCombo = false;
}

void SdlInputHandler::setUserComboEnabled(bool enabled)
{
    m_UserCombosEnabled = enabled;
}

bool SdlInputHandler::userComboEnabled() const
{
    return m_UserCombosEnabled;
}

void SdlInputHandler::raiseAllKeys()
{
    if (m_KeysDown.isEmpty()) {
        return;
    }

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Raising %d keys",
                (int)m_KeysDown.count());

    for (auto keyDown : m_KeysDown) {
        LiSendKeyboardEvent(keyDown, KEY_ACTION_UP, 0);
    }

    m_KeysDown.clear();
}

void SdlInputHandler::notifyMouseLeave()
{
    // SDL on Windows doesn't send the mouse button up until the mouse re-enters the window
    // after leaving it. This breaks some of the Aero snap gestures, so we'll capture it to
    // allow us to receive the mouse button up events later.
    //
    // On macOS and X11, capturing the mouse allows us to receive mouse motion outside the
    // window (button up already worked without capture).
    if (m_AbsoluteMouseMode && isCaptureActive()) {
        // NB: Not using SDL_GetGlobalMouseState() because we want our state not the system's
        Uint32 mouseState = SDL_GetMouseState(nullptr, nullptr);
        for (Uint32 button = SDL_BUTTON_LEFT; button <= SDL_BUTTON_X2; button++) {
            if (mouseState & SDL_BUTTON(button)) {
                SDL_CaptureMouse(SDL_TRUE);
                break;
            }
        }
    }
}

void SdlInputHandler::notifyFocusLost()
{
    // Release mouse cursor when another window is activated (e.g. by using ALT+TAB).
    // This lets user to interact with our window's title bar and with the buttons in it.
    // Doing this while the window is full-screen breaks the transition out of FS
    // (desktop and exclusive), so we must check for that before releasing mouse capture.
    if (!(SDL_GetWindowFlags(m_Window) & SDL_WINDOW_FULLSCREEN) && !m_AbsoluteMouseMode) {
        setCaptureActive(false);
    }

    // Raise all keys that are currently pressed. If we don't do this, certain keys
    // used in shortcuts that cause focus loss (such as Alt+Tab) may get stuck down.
    raiseAllKeys();

#ifdef Q_OS_WIN32
    // Re-enable text input when window loses focus as a workaround for an SDL bug.
    // See #1617 for details.
    SDL_StartTextInput();
#endif
}

void SdlInputHandler::notifyFocusGained()
{
#ifdef Q_OS_WIN32
    // Disable text input when window gains focus to prevent IME popup interference.
    // See #1617 for details.
    SDL_StopTextInput();
#endif
}

bool SdlInputHandler::isCaptureActive()
{
    if (SDL_GetRelativeMouseMode()) {
        return true;
    }

    // Some platforms don't support SDL_SetRelativeMouseMode
    return m_FakeCaptureActive;
}

void SdlInputHandler::updateKeyboardGrabState()
{
    if (m_CaptureSystemKeysMode == StreamingPreferences::CSK_OFF) {
        return;
    }

    bool shouldGrab = isCaptureActive();
    Uint32 windowFlags = SDL_GetWindowFlags(m_Window);
    if (m_CaptureSystemKeysMode == StreamingPreferences::CSK_FULLSCREEN &&
            !(windowFlags & SDL_WINDOW_FULLSCREEN)) {
        // Ungrab if it's fullscreen only and we left fullscreen
        shouldGrab = false;
    }

    // Don't close the window on Alt+F4 when keyboard grab is enabled
    SDL_SetHint(SDL_HINT_WINDOWS_NO_CLOSE_ON_ALT_F4, shouldGrab ? "1" : "0");

#if SDL_VERSION_ATLEAST(2, 0, 15)
    // On SDL 2.0.15+, we can get keyboard-only grab on Win32, X11, and Wayland.
    // SDL 2.0.18 adds keyboard grab on macOS (if built with non-AppStore APIs).
    SDL_SetWindowKeyboardGrab(m_Window, shouldGrab ? SDL_TRUE : SDL_FALSE);
#endif
}

bool SdlInputHandler::isSystemKeyCaptureActive()
{
    if (m_CaptureSystemKeysMode == StreamingPreferences::CSK_OFF) {
        return false;
    }

    if (m_Window == nullptr) {
        return false;
    }

    Uint32 windowFlags = SDL_GetWindowFlags(m_Window);
    if (!(windowFlags & SDL_WINDOW_INPUT_FOCUS)
#if SDL_VERSION_ATLEAST(2, 0, 15)
            || !(windowFlags & SDL_WINDOW_KEYBOARD_GRABBED)
#else
            || !(windowFlags & SDL_WINDOW_INPUT_GRABBED)
#endif
            )
    {
        return false;
    }

    if (m_CaptureSystemKeysMode == StreamingPreferences::CSK_FULLSCREEN &&
            !(windowFlags & SDL_WINDOW_FULLSCREEN)) {
        return false;
    }

    return true;
}

void SdlInputHandler::setCaptureActive(bool active)
{
    if (active) {
        // If we're in relative mode, try to activate SDL's relative mouse mode
        if (m_AbsoluteMouseMode || SDL_SetRelativeMouseMode(SDL_TRUE) < 0) {
            // Relative mouse mode didn't work or was disabled, so we'll just hide the cursor
            SDL_ShowCursor(m_MouseCursorCapturedVisibilityState);
            m_FakeCaptureActive = true;
        }

        // Synchronize the client and host cursor when activating absolute capture
        if (m_AbsoluteMouseMode) {
            int mouseX, mouseY;
            int windowX, windowY;

            // We have to use SDL_GetGlobalMouseState() because macOS may not reflect
            // the new position of the mouse when outside the window.
            SDL_GetGlobalMouseState(&mouseX, &mouseY);

            // Convert global mouse state to window-relative
            SDL_GetWindowPosition(m_Window, &windowX, &windowY);
            mouseX -= windowX;
            mouseY -= windowY;

            if (isMouseInVideoRegion(mouseX, mouseY)) {
                // Synthesize a mouse event to synchronize the cursor
                SDL_MouseMotionEvent motionEvent = {};
                motionEvent.type = SDL_MOUSEMOTION;
                motionEvent.timestamp = SDL_GetTicks();
                motionEvent.windowID = SDL_GetWindowID(m_Window);
                motionEvent.x = mouseX;
                motionEvent.y = mouseY;
                handleMouseMotionEvent(&motionEvent);
            }
        }
    }
    else {
        if (m_FakeCaptureActive) {
            // Display the cursor again
            SDL_ShowCursor(SDL_ENABLE);
            m_FakeCaptureActive = false;
        }
        else {
            SDL_SetRelativeMouseMode(SDL_FALSE);
        }
    }

    // Update mouse pointer region constraints
    updatePointerRegionLock();

    // Now update the keyboard grab
    updateKeyboardGrabState();
}

void SdlInputHandler::handleTouchFingerEvent(SDL_TouchFingerEvent* event)
{
#if SDL_VERSION_ATLEAST(2, 0, 10)
    if (SDL_GetTouchDeviceType(event->touchId) != SDL_TOUCH_DEVICE_DIRECT) {
        // Ignore anything that isn't a touchscreen. We may get callbacks
        // for trackpads, but we want to handle those in the mouse path.
        return;
    }
#elif defined(Q_OS_DARWIN)
    // SDL2 sends touch events from trackpads by default on
    // macOS. This totally screws our actual mouse handling,
    // so we must explicitly ignore touch events on macOS
    // until SDL 2.0.10 where we have SDL_GetTouchDeviceType()
    // to tell them apart.
    return;
#endif

    if (m_AbsoluteTouchMode) {
        handleAbsoluteFingerEvent(event);
    }
    else {
        handleRelativeFingerEvent(event);
    }
}
