// SPDX-License-Identifier: GPL-3.0-only
// Taskbar Accent for MyWallpaper.
//
// Uses the same narrow Windows composition boundary as Windhawk's official
// Taskbar Background Helper: style the primary and secondary taskbar windows
// and intercept Explorer's own writes so the selected appearance remains the
// single effective value. No XAML diagnostics session or visual-tree contract
// is required.

#include <windows.h>

#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <utility>

#include <mywallpaper_windhawk.hpp>
#include <windhawk_utils.h>

namespace {

enum ACCENT_STATE {
    ACCENT_DISABLED = 0,
    ACCENT_ENABLE_GRADIENT = 1,
    ACCENT_ENABLE_TRANSPARENTGRADIENT = 2,
    ACCENT_ENABLE_BLURBEHIND = 3,
    ACCENT_ENABLE_ACRYLICBLURBEHIND = 4,
};

struct ACCENT_POLICY {
    ACCENT_STATE state;
    UINT flags;
    DWORD gradientColor;
    LONG animationId;
};

enum WINDOWCOMPOSITIONATTRIB {
    WCA_ACCENT_POLICY = 19,
};

struct WINDOWCOMPOSITIONATTRIBDATA {
    WINDOWCOMPOSITIONATTRIB attribute;
    void* data;
    UINT size;
};

using SetWindowCompositionAttribute_t =
    BOOL(WINAPI*)(HWND, const WINDOWCOMPOSITIONATTRIBDATA*);

SetWindowCompositionAttribute_t SetWindowCompositionAttribute_Original;

enum class Mode {
    Blur,
    Acrylic,
    Transparent,
    Accent,
};

struct Settings {
    bool valid = false;
    bool enabled = false;
    Mode mode = Mode::Blur;
    DWORD gradientColor = 0;
};

struct WindhawkStringDeleter {
    void operator()(const WCHAR* value) const {
        if (value) {
            Wh_FreeStringSetting(value);
        }
    }
};

using WindhawkString =
    std::unique_ptr<const WCHAR[], WindhawkStringDeleter>;

std::mutex g_settingsMutex;
Settings g_settings;

void Emit(const char* topic, const char* payload) {
    mywallpaper::windhawk::emit_event(topic, payload);
}

void EmitInvalidSettings() {
    Emit("taskbar.invalid-settings",
         R"({"cause":"Taskbar Accent received settings outside its manifest contract.","action":"Reset this add-on in Settings → Add-ons, then enable it again."})");
}

void EmitApplyFailure() {
    Emit("taskbar.apply-failed",
         R"({"cause":"Windows rejected the requested taskbar composition appearance.","action":"Restart Windows Explorer from Task Manager, then choose Retry in Settings → Add-ons."})");
}

std::optional<DWORD> ParseGradientColor(PCWSTR text, int opacity) {
    if (!text || wcslen(text) != 7 || text[0] != L'#' || opacity < 0 ||
        opacity > 255) {
        return std::nullopt;
    }
    auto nibble = [](wchar_t value) -> std::optional<BYTE> {
        if (value >= L'0' && value <= L'9') {
            return static_cast<BYTE>(value - L'0');
        }
        if (value >= L'a' && value <= L'f') {
            return static_cast<BYTE>(value - L'a' + 10);
        }
        if (value >= L'A' && value <= L'F') {
            return static_cast<BYTE>(value - L'A' + 10);
        }
        return std::nullopt;
    };
    auto byte = [&](std::size_t offset) -> std::optional<BYTE> {
        const auto high = nibble(text[offset]);
        const auto low = nibble(text[offset + 1]);
        if (!high || !low) {
            return std::nullopt;
        }
        return static_cast<BYTE>((*high << 4) | *low);
    };
    const auto red = byte(1);
    const auto green = byte(3);
    const auto blue = byte(5);
    if (!red || !green || !blue) {
        return std::nullopt;
    }
    // ACCENT_POLICY consumes AABBGGRR, not COLORREF's 00BBGGRR.
    return static_cast<DWORD>(*red) |
           (static_cast<DWORD>(*green) << 8) |
           (static_cast<DWORD>(*blue) << 16) |
           (static_cast<DWORD>(opacity) << 24);
}

Settings ReadSettings() {
    Settings settings;
    const int enabled = Wh_GetIntSetting(L"enabled");
    const int opacity = Wh_GetIntSetting(L"opacity");
    WindhawkString mode(Wh_GetStringSetting(L"mode"));
    WindhawkString color(Wh_GetStringSetting(L"accentColor"));
    if ((enabled != 0 && enabled != 1) || !mode || !color) {
        return settings;
    }
    if (wcscmp(mode.get(), L"blur") == 0) {
        settings.mode = Mode::Blur;
    } else if (wcscmp(mode.get(), L"acrylic") == 0) {
        settings.mode = Mode::Acrylic;
    } else if (wcscmp(mode.get(), L"transparent") == 0) {
        settings.mode = Mode::Transparent;
    } else if (wcscmp(mode.get(), L"accent") == 0) {
        settings.mode = Mode::Accent;
    } else {
        return settings;
    }
    const auto gradientColor = ParseGradientColor(color.get(), opacity);
    if (!gradientColor) {
        return settings;
    }
    settings.valid = true;
    settings.enabled = enabled != 0;
    settings.gradientColor = *gradientColor;
    return settings;
}

Settings CurrentSettings() {
    std::lock_guard lock(g_settingsMutex);
    return g_settings;
}

void PublishSettings(const Settings& settings) {
    std::lock_guard lock(g_settingsMutex);
    g_settings = settings;
}

bool IsTaskbarWindow(HWND window) {
    DWORD processId = 0;
    WCHAR className[32]{};
    return GetWindowThreadProcessId(window, &processId) != 0 &&
           processId == GetCurrentProcessId() &&
           GetClassNameW(window, className, ARRAYSIZE(className)) != 0 &&
           (_wcsicmp(className, L"Shell_TrayWnd") == 0 ||
            _wcsicmp(className, L"Shell_SecondaryTrayWnd") == 0);
}

ACCENT_POLICY PolicyFor(const Settings& settings) {
    if (!settings.enabled) {
        return {ACCENT_ENABLE_TRANSPARENTGRADIENT, 0x13, 0, 0};
    }
    switch (settings.mode) {
        case Mode::Blur:
            return {ACCENT_ENABLE_BLURBEHIND, 0, settings.gradientColor, 0};
        case Mode::Acrylic:
            return {ACCENT_ENABLE_ACRYLICBLURBEHIND, 0,
                    settings.gradientColor, 0};
        case Mode::Transparent:
            return {ACCENT_ENABLE_TRANSPARENTGRADIENT, 0x13, 0, 0};
        case Mode::Accent:
            return {ACCENT_ENABLE_TRANSPARENTGRADIENT, 0x13,
                    settings.gradientColor, 0};
    }
    return {ACCENT_DISABLED, 0, 0, 0};
}

BOOL Apply(HWND window, const Settings& settings) {
    ACCENT_POLICY policy = PolicyFor(settings);
    WINDOWCOMPOSITIONATTRIBDATA data{
        WCA_ACCENT_POLICY,
        &policy,
        sizeof(policy),
    };
    return SetWindowCompositionAttribute_Original(window, &data);
}

struct ApplyResult {
    unsigned windows = 0;
    bool success = true;
};

struct ApplyContext {
    const Settings* settings;
    ApplyResult* result;
};

BOOL CALLBACK ApplyTaskbarWindow(HWND window, LPARAM parameter) {
    auto* context = reinterpret_cast<ApplyContext*>(parameter);
    if (!IsTaskbarWindow(window)) {
        return TRUE;
    }
    context->result->windows++;
    if (!Apply(window, *context->settings)) {
        context->result->success = false;
    }
    return TRUE;
}

ApplyResult ApplyAll(const Settings& settings) {
    ApplyResult result;
    ApplyContext context{&settings, &result};
    EnumWindows(ApplyTaskbarWindow, reinterpret_cast<LPARAM>(&context));
    result.success = result.success && result.windows > 0;
    return result;
}

BOOL WINAPI SetWindowCompositionAttribute_Hook(
    HWND window,
    const WINDOWCOMPOSITIONATTRIBDATA* data) {
    if (!data || data->attribute != WCA_ACCENT_POLICY ||
        !IsTaskbarWindow(window)) {
        return SetWindowCompositionAttribute_Original(window, data);
    }
    return Apply(window, CurrentSettings());
}

void ApplyAndReport(const Settings& settings) {
    const auto result = ApplyAll(settings);
    if (!result.success) {
        EmitApplyFailure();
        return;
    }
    if (settings.enabled) {
        Emit("taskbar.applied", R"({"state":"active"})");
    } else {
        Emit("taskbar.disabled", R"({"state":"disabled"})");
    }
}

}  // namespace

BOOL Wh_ModInit() {
    const Settings settings = ReadSettings();
    if (!settings.valid) {
        EmitInvalidSettings();
        return FALSE;
    }
    PublishSettings(settings);
    HMODULE user32 = LoadLibraryExW(
        L"user32.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!user32) {
        EmitApplyFailure();
        return FALSE;
    }
    auto setWindowCompositionAttribute =
        reinterpret_cast<SetWindowCompositionAttribute_t>(
            GetProcAddress(user32, "SetWindowCompositionAttribute"));
    if (!setWindowCompositionAttribute ||
        !WindhawkUtils::SetFunctionHook(
            setWindowCompositionAttribute,
            SetWindowCompositionAttribute_Hook,
            &SetWindowCompositionAttribute_Original)) {
        EmitApplyFailure();
        return FALSE;
    }
    return TRUE;
}

void Wh_ModAfterInit() {
    ApplyAndReport(CurrentSettings());
}

void Wh_ModSettingsChanged() {
    const Settings settings = ReadSettings();
    if (!settings.valid) {
        EmitInvalidSettings();
        return;
    }
    PublishSettings(settings);
    ApplyAndReport(settings);
}

void Wh_ModUninit() {
    Settings disabled;
    disabled.valid = true;
    disabled.enabled = false;
    ApplyAll(disabled);
}
