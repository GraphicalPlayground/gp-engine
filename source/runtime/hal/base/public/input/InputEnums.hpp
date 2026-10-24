// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground.com/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"

namespace gp::hal::input
{

/// @brief Physical key location on the keyboard (layout-independent).
/// @details Values follow the USB HID Usage Tables (Keyboard/Keypad Page 0x07).
///          These correspond 1:1 with the physical key regardless of OS keyboard layout.
enum class Scancode : gp::UInt16
{
    Unknown = 0,

    /// Letters (HID 0x04-0x1D)
    A = 4,
    B = 5,
    C = 6,
    D = 7,
    E = 8,
    F = 9,
    G = 10,
    H = 11,
    I = 12,
    J = 13,
    K = 14,
    L = 15,
    M = 16,
    N = 17,
    O = 18,
    P = 19,
    Q = 20,
    R = 21,
    S = 22,
    T = 23,
    U = 24,
    V = 25,
    W = 26,
    X = 27,
    Y = 28,
    Z = 29,

    /// Number row (HID 0x1E-0x27)
    Num1 = 30,   // 1 and !
    Num2 = 31,   // 2 and @
    Num3 = 32,   // 3 and #
    Num4 = 33,   // 4 and $
    Num5 = 34,   // 5 and %
    Num6 = 35,   // 6 and ^
    Num7 = 36,   // 7 and &
    Num8 = 37,   // 8 and *
    Num9 = 38,   // 9 and (
    Num0 = 39,   // 0 and )

    /// Core control keys (HID 0x28-0x38)
    Return = 40,
    Escape = 41,
    Backspace = 42,
    Tab = 43,
    Space = 44,
    Minus = 45,          // - and _
    Equals = 46,         // = and +
    LeftBracket = 47,    // [ and {
    RightBracket = 48,   // ] and }
    Backslash = 49,      // \ and |
    NonUsHash = 50,      // ISO # and ~ (non-US keyboards)
    Semicolon = 51,      // ; and :
    Apostrophe = 52,     // ' and "
    Grave = 53,          // ` and ~
    Comma = 54,          // , and <
    Period = 55,         // . and >
    Slash = 56,          // / and ?

    CapsLock = 57,

    /// Function keys (HID 0x3A-0x45)
    F1 = 58,
    F2 = 59,
    F3 = 60,
    F4 = 61,
    F5 = 62,
    F6 = 63,
    F7 = 64,
    F8 = 65,
    F9 = 66,
    F10 = 67,
    F11 = 68,
    F12 = 69,

    /// Print / Lock / Pause (HID 0x46-0x48)
    PrintScreen = 70,
    ScrollLock = 71,
    Pause = 72,

    /// Navigation cluster (HID 0x49-0x52)
    Insert = 73,
    Home = 74,
    PageUp = 75,
    Delete = 76,
    End = 77,
    PageDown = 78,

    /// Arrow keys (HID 0x4F-0x52)
    Right = 79,
    Left = 80,
    Down = 81,
    Up = 82,

    /// Numpad (HID 0x53-0x63)
    NumLock = 83,
    KpDivide = 84,
    KpMultiply = 85,
    KpMinus = 86,
    KpPlus = 87,
    KpEnter = 88,
    Kp1 = 89,
    Kp2 = 90,
    Kp3 = 91,
    Kp4 = 92,
    Kp5 = 93,
    Kp6 = 94,
    Kp7 = 95,
    Kp8 = 96,
    Kp9 = 97,
    Kp0 = 98,
    KpPeriod = 99,

    /// Additional keys (HID 0x64+)
    NonUsBackslash = 100,   // ISO key between Left Shift and Z
    Application = 101,      // Context menu / Compose
    Power = 102,
    KpEquals = 103,

    /// Extended function keys
    F13 = 104,
    F14 = 105,
    F15 = 106,
    F16 = 107,
    F17 = 108,
    F18 = 109,
    F19 = 110,
    F20 = 111,
    F21 = 112,
    F22 = 113,
    F23 = 114,
    F24 = 115,

    /// Execute / Help / Menu / Select (HID 0x74-0x77)
    Execute = 116,
    Help = 117,
    Menu = 118,
    Select = 119,
    Stop = 120,
    Again = 121,   // Redo
    Undo = 122,
    Cut = 123,
    Copy = 124,
    Paste = 125,
    Find = 126,
    Mute = 127,
    VolumeUp = 128,
    VolumeDown = 129,

    /// Numpad extended
    KpComma = 133,
    KpEqualsAS400 = 134,   // Numpad = on AS/400 keyboards

    /// International keys
    International1 = 135,   // Yen / Ro - JIS
    International2 = 136,   // Katakana / Hiragana - JIS
    International3 = 137,   // Yen - JIS
    International4 = 138,   // Henkan - JIS
    International5 = 139,   // Muhenkan - JIS
    International6 = 140,
    International7 = 141,
    International8 = 142,
    International9 = 143,

    /// Language keys
    Lang1 = 144,   // Hangul / English toggle (Korean)
    Lang2 = 145,   // Hanja (Korean)
    Lang3 = 146,   // Katakana (Japanese)
    Lang4 = 147,   // Hiragana (Japanese)
    Lang5 = 148,   // Zenkaku / Hankaku (Japanese)
    Lang6 = 149,
    Lang7 = 150,
    Lang8 = 151,
    Lang9 = 152,

    /// Editing / Alt-Erase / SysReq
    AltErase = 153,
    SysReq = 154,
    Cancel = 155,
    Clear = 156,
    Prior = 157,
    Return2 = 158,
    Separator = 159,
    Out = 160,
    Oper = 161,
    ClearAgain = 162,
    CrSel = 163,
    ExSel = 164,

    /// Numpad extended block (HID 0xB0+)
    Kp00 = 176,
    Kp000 = 177,
    KpThousandsSep = 178,
    KpDecimalSep = 179,
    KpCurrencyUnit = 180,
    KpCurrencySubunit = 181,
    KpLeftParen = 182,
    KpRightParen = 183,
    KpLeftBrace = 184,
    KpRightBrace = 185,
    KpTab = 186,
    KpBackspace = 187,
    KpA = 188,
    KpB = 189,
    KpC = 190,
    KpD = 191,
    KpE = 192,
    KpF = 193,
    KpXor = 194,
    KpPower = 195,
    KpPercent = 196,
    KpLess = 197,
    KpGreater = 198,
    KpAmpersand = 199,
    KpDoubleAmpersand = 200,
    KpVerticalBar = 201,
    KpDoubleVerticalBar = 202,
    KpColon = 203,
    KpHash = 204,
    KpSpace = 205,
    KpAt = 206,
    KpExclam = 207,
    KpMemStore = 208,
    KpMemRecall = 209,
    KpMemClear = 210,
    KpMemAdd = 211,
    KpMemSubtract = 212,
    KpMemMultiply = 213,
    KpMemDivide = 214,
    KpPlusMinus = 215,
    KpClear = 216,
    KpClearEntry = 217,
    KpBinary = 218,
    KpOctal = 219,
    KpDecimal = 220,
    KpHexadecimal = 221,

    /// Modifier keys (HID 0xE0-0xE7)
    LeftCtrl = 224,
    LeftShift = 225,
    LeftAlt = 226,
    LeftGui = 227,   // Windows / Super / Command
    RightCtrl = 228,
    RightShift = 229,
    RightAlt = 230,   // AltGr on international keyboards
    RightGui = 231,   // Windows / Super / Command

    /// @brief Media / Consumer keys (extended, SDL-compatible range 0x100+).
    /// @details These are not standard HID keyboard page codes; they come from
    ///          the HID Consumer Page (0x0C). We map them into the 0x100+ range
    ///          to keep them in a single contiguous enum alongside keyboard scancodes.
    MediaPlay = 258,
    MediaStop = 260,
    MediaPrevious = 259,
    MediaNext = 261,
    MediaEject = 262,
    MediaVolumeUp = 263,
    MediaVolumeDown = 264,
    MediaMute = 265,

    /// Browser / App keys (Consumer page)
    AppBrowser = 266,
    AppMail = 267,
    AppCalculator = 268,
    AppSearch = 269,
    AppHome = 270,
    AppBack = 271,
    AppForward = 272,
    AppStop = 273,
    AppRefresh = 274,
    AppBookmarks = 275,

    /// Display / Power management
    DisplayBrightnessDown = 276,
    DisplayBrightnessUp = 277,
    DisplaySwitch = 278,   // Switch display target
    KeyboardIllumToggle = 279,
    KeyboardIllumDown = 280,
    KeyboardIllumUp = 281,

    Sleep = 282,
    Wake = 283,

    /// Miscellaneous
    ChannelUp = 284,
    ChannelDown = 285,

    /// Sentinel value
    Count = 512
};

/// @brief Bitmask constant used to mark a keycode as scancode-derived.
inline constexpr gp::UInt32 kScancodeMask = 0x40000000u;

/// @brief Converts a Scancode into its corresponding Keycode value.
/// @details Sets bit 30 (0x40000000) to distinguish scancode-derived keycodes
///          from printable ASCII keycodes which occupy the range 0-127.
/// @param[in] scanCode The Scancode to convert.
/// @return The corresponding Keycode value with the scancode bit set.
[[nodiscard]] inline constexpr gp::UInt32 scancodeToKeycode(Scancode scanCode)
{
    return static_cast<gp::UInt32>(scanCode) | kScancodeMask;
}

/// @brief Layout-dependent virtual key.
/// @details The low 16 bits encode the Scancode; bit 30 (0x40000000) is set to
///          mark it as a scancode-derived keycode. Non-scancode keycodes (printable
///          ASCII) live in the range 0-127 and map directly to their ASCII code point.
///          This mirrors the SDL_SCANCODE_TO_KEYCODE scheme used by SDL3.
enum class Keycode : gp::UInt32
{
    Unknown = 0,

    /// Printable ASCII range (layout-dependent)
    Return = '\r',
    Escape = 0x1B,
    Backspace = '\b',
    Tab = '\t',
    Space = ' ',
    Exclaim = '!',
    DoubleQuote = '"',
    Hash = '#',
    Dollar = '$',
    Percent = '%',
    Ampersand = '&',
    Quote = '\'',
    LeftParen = '(',
    RightParen = ')',
    Asterisk = '*',
    Plus = '+',
    Comma = ',',
    Minus = '-',
    Period = '.',
    Slash = '/',
    Num0 = '0',
    Num1 = '1',
    Num2 = '2',
    Num3 = '3',
    Num4 = '4',
    Num5 = '5',
    Num6 = '6',
    Num7 = '7',
    Num8 = '8',
    Num9 = '9',
    Colon = ':',
    Semicolon = ';',
    Less = '<',
    Equals = '=',
    Greater = '>',
    Question = '?',
    At = '@',
    LeftBracket = '[',
    Backslash = '\\',
    RightBracket = ']',
    Caret = '^',
    Underscore = '_',
    Backquote = '`',
    A = 'a',
    B = 'b',
    C = 'c',
    D = 'd',
    E = 'e',
    F = 'f',
    G = 'g',
    H = 'h',
    I = 'i',
    J = 'j',
    K = 'k',
    L = 'l',
    M = 'm',
    N = 'n',
    O = 'o',
    P = 'p',
    Q = 'q',
    R = 'r',
    S = 's',
    T = 't',
    U = 'u',
    V = 'v',
    W = 'w',
    X = 'x',
    Y = 'y',
    Z = 'z',
    Delete = 0x7F,

    /// Scancode-derived keycodes (non-printable / special keys)
    CapsLock = scancodeToKeycode(Scancode::CapsLock),
    F1 = scancodeToKeycode(Scancode::F1),
    F2 = scancodeToKeycode(Scancode::F2),
    F3 = scancodeToKeycode(Scancode::F3),
    F4 = scancodeToKeycode(Scancode::F4),
    F5 = scancodeToKeycode(Scancode::F5),
    F6 = scancodeToKeycode(Scancode::F6),
    F7 = scancodeToKeycode(Scancode::F7),
    F8 = scancodeToKeycode(Scancode::F8),
    F9 = scancodeToKeycode(Scancode::F9),
    F10 = scancodeToKeycode(Scancode::F10),
    F11 = scancodeToKeycode(Scancode::F11),
    F12 = scancodeToKeycode(Scancode::F12),
    F13 = scancodeToKeycode(Scancode::F13),
    F14 = scancodeToKeycode(Scancode::F14),
    F15 = scancodeToKeycode(Scancode::F15),
    F16 = scancodeToKeycode(Scancode::F16),
    F17 = scancodeToKeycode(Scancode::F17),
    F18 = scancodeToKeycode(Scancode::F18),
    F19 = scancodeToKeycode(Scancode::F19),
    F20 = scancodeToKeycode(Scancode::F20),
    F21 = scancodeToKeycode(Scancode::F21),
    F22 = scancodeToKeycode(Scancode::F22),
    F23 = scancodeToKeycode(Scancode::F23),
    F24 = scancodeToKeycode(Scancode::F24),

    PrintScreen = scancodeToKeycode(Scancode::PrintScreen),
    ScrollLock = scancodeToKeycode(Scancode::ScrollLock),
    Pause = scancodeToKeycode(Scancode::Pause),
    Insert = scancodeToKeycode(Scancode::Insert),
    Home = scancodeToKeycode(Scancode::Home),
    PageUp = scancodeToKeycode(Scancode::PageUp),
    End = scancodeToKeycode(Scancode::End),
    PageDown = scancodeToKeycode(Scancode::PageDown),
    Right = scancodeToKeycode(Scancode::Right),
    Left = scancodeToKeycode(Scancode::Left),
    Down = scancodeToKeycode(Scancode::Down),
    Up = scancodeToKeycode(Scancode::Up),

    NumLock = scancodeToKeycode(Scancode::NumLock),
    KpDivide = scancodeToKeycode(Scancode::KpDivide),
    KpMultiply = scancodeToKeycode(Scancode::KpMultiply),
    KpMinus = scancodeToKeycode(Scancode::KpMinus),
    KpPlus = scancodeToKeycode(Scancode::KpPlus),
    KpEnter = scancodeToKeycode(Scancode::KpEnter),
    Kp1 = scancodeToKeycode(Scancode::Kp1),
    Kp2 = scancodeToKeycode(Scancode::Kp2),
    Kp3 = scancodeToKeycode(Scancode::Kp3),
    Kp4 = scancodeToKeycode(Scancode::Kp4),
    Kp5 = scancodeToKeycode(Scancode::Kp5),
    Kp6 = scancodeToKeycode(Scancode::Kp6),
    Kp7 = scancodeToKeycode(Scancode::Kp7),
    Kp8 = scancodeToKeycode(Scancode::Kp8),
    Kp9 = scancodeToKeycode(Scancode::Kp9),
    Kp0 = scancodeToKeycode(Scancode::Kp0),
    KpPeriod = scancodeToKeycode(Scancode::KpPeriod),
    KpEquals = scancodeToKeycode(Scancode::KpEquals),
    KpComma = scancodeToKeycode(Scancode::KpComma),

    Application = scancodeToKeycode(Scancode::Application),
    Power = scancodeToKeycode(Scancode::Power),

    Execute = scancodeToKeycode(Scancode::Execute),
    Help = scancodeToKeycode(Scancode::Help),
    Menu = scancodeToKeycode(Scancode::Menu),
    Select = scancodeToKeycode(Scancode::Select),
    Stop = scancodeToKeycode(Scancode::Stop),
    Again = scancodeToKeycode(Scancode::Again),
    Undo = scancodeToKeycode(Scancode::Undo),
    Cut = scancodeToKeycode(Scancode::Cut),
    Copy = scancodeToKeycode(Scancode::Copy),
    Paste = scancodeToKeycode(Scancode::Paste),
    Find = scancodeToKeycode(Scancode::Find),
    Mute = scancodeToKeycode(Scancode::Mute),
    VolumeUp = scancodeToKeycode(Scancode::VolumeUp),
    VolumeDown = scancodeToKeycode(Scancode::VolumeDown),

    /// Modifier keycodes
    LeftCtrl = scancodeToKeycode(Scancode::LeftCtrl),
    LeftShift = scancodeToKeycode(Scancode::LeftShift),
    LeftAlt = scancodeToKeycode(Scancode::LeftAlt),
    LeftGui = scancodeToKeycode(Scancode::LeftGui),
    RightCtrl = scancodeToKeycode(Scancode::RightCtrl),
    RightShift = scancodeToKeycode(Scancode::RightShift),
    RightAlt = scancodeToKeycode(Scancode::RightAlt),
    RightGui = scancodeToKeycode(Scancode::RightGui),

    /// Media / Consumer keycodes
    MediaPlay = scancodeToKeycode(Scancode::MediaPlay),
    MediaStop = scancodeToKeycode(Scancode::MediaStop),
    MediaPrevious = scancodeToKeycode(Scancode::MediaPrevious),
    MediaNext = scancodeToKeycode(Scancode::MediaNext),
    MediaEject = scancodeToKeycode(Scancode::MediaEject),
    MediaVolumeUp = scancodeToKeycode(Scancode::MediaVolumeUp),
    MediaVolumeDown = scancodeToKeycode(Scancode::MediaVolumeDown),
    MediaMute = scancodeToKeycode(Scancode::MediaMute),

    /// App launch keycodes
    AppBrowser = scancodeToKeycode(Scancode::AppBrowser),
    AppMail = scancodeToKeycode(Scancode::AppMail),
    AppCalculator = scancodeToKeycode(Scancode::AppCalculator),
    AppSearch = scancodeToKeycode(Scancode::AppSearch),
    AppHome = scancodeToKeycode(Scancode::AppHome),
    AppBack = scancodeToKeycode(Scancode::AppBack),
    AppForward = scancodeToKeycode(Scancode::AppForward),
    AppStop = scancodeToKeycode(Scancode::AppStop),
    AppRefresh = scancodeToKeycode(Scancode::AppRefresh),
    AppBookmarks = scancodeToKeycode(Scancode::AppBookmarks),

    Sleep = scancodeToKeycode(Scancode::Sleep),
    Wake = scancodeToKeycode(Scancode::Wake),
};

/// @brief Bitmask representing currently active keyboard modifiers.
/// @details Used in keyboard events, input action bindings, and shortcut detection.
enum class KeyModifier : gp::UInt16
{
    None = 0,
    LeftShift = 1 << 0,
    RightShift = 1 << 1,
    LeftCtrl = 1 << 2,
    RightCtrl = 1 << 3,
    LeftAlt = 1 << 4,
    RightAlt = 1 << 5,
    LeftGui = 1 << 6,
    RightGui = 1 << 7,
    NumLock = 1 << 8,
    CapsLock = 1 << 9,
    ScrollLock = 1 << 10,

    /// Convenience masks
    Shift = LeftShift | RightShift,
    Ctrl = LeftCtrl | RightCtrl,
    Alt = LeftAlt | RightAlt,
    Gui = LeftGui | RightGui,
};

/// @brief Mouse button identifier.
enum class MouseButton : gp::UInt8
{
    Unknown = 0,
    Left = 1,
    Middle = 2,
    Right = 3,
    X1 = 4,   // Back button
    X2 = 5,   // Forward button
    Count = 6
};

/// @brief Bitfield for multiple simultaneous mouse button states.
enum class MouseButtonMask : gp::UInt8
{
    None = 0,
    Left = 1 << 0,
    Middle = 1 << 1,
    Right = 1 << 2,
    X1 = 1 << 3,
    X2 = 1 << 4,
};

/// @brief Platform-neutral gamepad button identifiers.
/// @details Uses positional names (South / East / West / North) to stay platform-agnostic.
enum class GamepadButton : gp::UInt8
{
    Invalid = 255,
    South = 0,            // A / Cross
    East = 1,             // B / Circle
    West = 2,             // X / Square
    North = 3,            // Y / Triangle
    Back = 4,             // Select / Share
    Guide = 5,            // Home / PS Button
    Start = 6,            // Start / Options
    LeftStick = 7,        // L3
    RightStick = 8,       // R3
    LeftShoulder = 9,     // L1 / LB
    RightShoulder = 10,   // R1 / RB
    DpadUp = 11,
    DpadDown = 12,
    DpadLeft = 13,
    DpadRight = 14,
    Misc1 = 15,      // Share on Xbox Series X, Mic button on PS5
    Paddle1 = 16,    // Xbox Elite / Scuf paddle (upper-left)
    Paddle2 = 17,    // Upper-right
    Paddle3 = 18,    // Lower-left
    Paddle4 = 19,    // Lower-right
    Touchpad = 20,   // PS4 / PS5 touchpad click
    Count = 21
};

/// @brief Analog stick and trigger axis identifiers.
enum class GamepadAxis : gp::UInt8
{
    Invalid = 255,
    LeftX = 0,
    LeftY = 1,
    RightX = 2,
    RightY = 3,
    LeftTrigger = 4,    // L2 / LT
    RightTrigger = 5,   // R2 / RT
    Count = 6
};

/// @brief Motion and orientation sensor types on modern controllers.
enum class GamepadSensorType : gp::UInt8
{
    Unknown = 0,
    Accelerometer = 1,   // Reports linear acceleration in m/s^2
    Gyroscope = 2,       // Reports angular velocity in rad/s
    Count = 3
};

/// @brief Bitmask for joystick hat switch positions.
/// @details Diagonal positions are combinations of the cardinal masks.
enum class HatPosition : gp::UInt8
{
    Centered = 0,
    Up = 1 << 0,
    Right = 1 << 1,
    Down = 1 << 2,
    Left = 1 << 3,
    RightUp = Right | Up,
    RightDown = Right | Down,
    LeftUp = Left | Up,
    LeftDown = Left | Down,
};

/// @brief Haptic actuator side for asymmetric rumble (e.g. DualSense adaptive triggers).
enum class ActuatorSide : gp::UInt8
{
    Left = 0,
    Right = 1,
    Both = 2
};

/// @brief Touch finger event state for multi-touch / touchscreen / trackpad input.
enum class TouchFingerState : gp::UInt8
{
    Up = 0,       // Finger lifted
    Down = 1,     // Finger pressed
    Motion = 2,   // Finger moved while held
};

/// @brief Distinguishes between direct (screen) and indirect (trackpad) touch surfaces.
enum class TouchDeviceType : gp::UInt8
{
    Invalid = 0,
    Direct = 1,     // Touch screen (finger directly over content)
    Indirect = 2,   // Trackpad (cursor-based mapping)
};

/// @brief Pen / stylus axis identifiers for pen tablets, Surface Pen, Apple Pencil, etc.
enum class PenAxis : gp::UInt8
{
    Pressure = 0,   // 0.0 to 1.0 normalised pressure
    TiltX = 1,      // Degrees, -90 to +90
    TiltY = 2,      // Degrees, -90 to +90
    Distance = 3,   // Hover distance from surface (0 = touching)
    Rotation = 4,   // Barrel rotation, 0 to 360 degrees
    Slider = 5,     // Auxiliary slider / wheel
    TangentialPressure = 6,
    Count = 7
};

/// @brief Bitfield for pen buttons (barrel buttons, erasers, etc.).
enum class PenButton : gp::UInt8
{
    None = 0,
    Tip = 1 << 0,       // Primary contact / tip switch
    Barrel = 1 << 1,    // First barrel button
    Barrel2 = 1 << 2,   // Second barrel button
    Eraser = 1 << 3,    // Eraser tip
};

/// @brief High-level classification for input device routing.
/// @details Used by the input subsystem to tag event sources and apply
///          device-specific processing (dead zones, acceleration curves, etc.).
enum class InputDeviceType : gp::UInt8
{
    Unknown = 0,
    Keyboard = 1,
    Mouse = 2,
    Gamepad = 3,
    Joystick = 4,   // Generic HID joystick (non-gamepad)
    Touch = 5,
    Pen = 6,        // Stylus / pen tablet
    Sensor = 7,     // Accelerometer / Gyroscope (standalone)
    Count = 8
};

/// @brief State machine for input actions (used by action maps).
/// @details Maps to the lifecycle of a binding: Waiting -> Started -> Ongoing -> Completed
///          (or Cancelled). Mirrors patterns in UE Enhanced Input and Unity's Input System.
enum class InputActionPhase : gp::UInt8
{
    Disabled = 0,    // Action is disabled and won't process input
    Waiting = 1,     // Waiting for interaction to begin
    Started = 2,     // Interaction just started (initial press / threshold crossed)
    Ongoing = 3,     // Interaction is in progress (held / dragged)
    Completed = 4,   // Interaction completed normally (released)
    Cancelled = 5,   // Interaction was cancelled (focus lost, binding conflict)
};

/// @brief Describes the kind of value an input action produces.
/// @details Determines the size of the value payload and how the input mapper
///          composites multiple bindings.
enum class InputValueType : gp::UInt8
{
    Digital = 0,   // Boolean on/off (button press)
    Axis1D = 1,    // Single float (trigger, scroll)
    Axis2D = 2,    // 2D vector (stick, mouse delta, touch position)
    Axis3D = 3,    // 3D vector (accelerometer, VR controller position)
};

/// @brief Key press state.
/// @details Kept as an enum rather than a bool for clarity and future extensibility
///          (e.g. synthetic repeat events).
enum class KeyState : gp::UInt8
{
    Released = 0,
    Pressed = 1,
};

}   // namespace gp::hal::input
