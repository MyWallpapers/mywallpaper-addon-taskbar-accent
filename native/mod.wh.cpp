// @include explorer.exe
#include <windows.h>
#include <stdio.h>

struct ACCENT_POLICY {
    int AccentState;
    int AccentFlags;
    int GradientColor;
    int AnimationId;
};

struct WINDOWCOMPOSITIONATTRIBDATA {
    int Attrib;
    PVOID pvData;
    SIZE_T cbData;
};

using SetWindowCompositionAttributeFunc = BOOL(WINAPI*)(HWND, WINDOWCOMPOSITIONATTRIBDATA*);

enum AccentState {
    ACCENT_DISABLED = 0,
    ACCENT_ENABLE_GRADIENT = 1,
    ACCENT_ENABLE_TRANSPARENTGRADIENT = 2,
    ACCENT_ENABLE_BLURBEHIND = 3,
    ACCENT_ENABLE_ACRYLICBLURBEHIND = 4,
};

static constexpr int WCA_ACCENT_POLICY = 19;

static SetWindowCompositionAttributeFunc g_setWindowCompositionAttribute = nullptr;
static SetWindowCompositionAttributeFunc g_originalSetWindowCompositionAttribute = nullptr;

static int ClampInt(int value, int minValue, int maxValue) {
    if (value < minValue) {
        return minValue;
    }
    if (value > maxValue) {
        return maxValue;
    }
    return value;
}

static int HexNibble(WCHAR ch) {
    if (ch >= L'0' && ch <= L'9') {
        return ch - L'0';
    }
    if (ch >= L'a' && ch <= L'f') {
        return ch - L'a' + 10;
    }
    if (ch >= L'A' && ch <= L'F') {
        return ch - L'A' + 10;
    }
    return 0;
}

static BYTE HexByte(PCWSTR text) {
    return static_cast<BYTE>((HexNibble(text[0]) << 4) | HexNibble(text[1]));
}

static COLORREF ParseColor(PCWSTR text, COLORREF fallback) {
    if (!text || !text[0]) {
        return fallback;
    }
    if (text[0] == L'#' &&
        iswxdigit(text[1]) &&
        iswxdigit(text[2]) &&
        iswxdigit(text[3]) &&
        iswxdigit(text[4]) &&
        iswxdigit(text[5]) &&
        iswxdigit(text[6])) {
        return RGB(HexByte(text + 1), HexByte(text + 3), HexByte(text + 5));
    }
    int r = 0;
    int g = 0;
    int b = 0;
    if (swscanf_s(text, L"rgb(%d, %d, %d)", &r, &g, &b) == 3 ||
        swscanf_s(text, L"rgba(%d, %d, %d", &r, &g, &b) == 3) {
        return RGB(ClampInt(r, 0, 255), ClampInt(g, 0, 255), ClampInt(b, 0, 255));
    }
    return fallback;
}

static int GradientColor(COLORREF color, int opacity) {
    return (ClampInt(opacity, 0, 255) << 24) |
        (GetBValue(color) << 16) |
        (GetGValue(color) << 8) |
        GetRValue(color);
}

static bool Equals(PCWSTR left, PCWSTR right) {
    return left && right && _wcsicmp(left, right) == 0;
}

static bool IsTaskbarWindow(HWND hwnd) {
    WCHAR className[64];
    if (!hwnd || !GetClassNameW(hwnd, className, ARRAYSIZE(className))) {
        return false;
    }
    return wcscmp(className, L"Shell_TrayWnd") == 0 ||
        wcscmp(className, L"Shell_SecondaryTrayWnd") == 0;
}

static ACCENT_POLICY ReadAccentPolicy() {
    PCWSTR mode = Wh_GetStringSetting(L"mode");
    PCWSTR colorText = Wh_GetStringSetting(L"accentColor");
    bool enabled = Wh_GetIntSetting(L"enabled") != 0;
    int opacity = ClampInt(Wh_GetIntSetting(L"opacity"), 0, 255);
    COLORREF color = ParseColor(colorText, RGB(31, 143, 255));

    ACCENT_POLICY policy = {};
    if (!enabled) {
        policy.AccentState = ACCENT_DISABLED;
    } else if (Equals(mode, L"transparent")) {
        policy.AccentState = ACCENT_ENABLE_TRANSPARENTGRADIENT;
        policy.GradientColor = GradientColor(color, opacity);
    } else if (Equals(mode, L"accent")) {
        policy.AccentState = ACCENT_ENABLE_GRADIENT;
        policy.GradientColor = GradientColor(color, opacity);
    } else if (Equals(mode, L"acrylic")) {
        policy.AccentState = ACCENT_ENABLE_ACRYLICBLURBEHIND;
        policy.GradientColor = GradientColor(color, opacity);
    } else {
        policy.AccentState = ACCENT_ENABLE_BLURBEHIND;
        policy.GradientColor = GradientColor(color, opacity);
    }

    Wh_FreeStringSetting(mode);
    Wh_FreeStringSetting(colorText);
    return policy;
}

static BOOL CallSetWindowCompositionAttribute(HWND hwnd, WINDOWCOMPOSITIONATTRIBDATA* data) {
    auto call = g_originalSetWindowCompositionAttribute ?
        g_originalSetWindowCompositionAttribute :
        g_setWindowCompositionAttribute;
    return call ? call(hwnd, data) : FALSE;
}

static void ApplyAccentToWindow(HWND hwnd, const ACCENT_POLICY& policy) {
    if (!hwnd) {
        return;
    }

    ACCENT_POLICY localPolicy = policy;
    WINDOWCOMPOSITIONATTRIBDATA data = {};
    data.Attrib = WCA_ACCENT_POLICY;
    data.pvData = &localPolicy;
    data.cbData = sizeof(localPolicy);
    CallSetWindowCompositionAttribute(hwnd, &data);
}

static BOOL WINAPI SetWindowCompositionAttributeHook(HWND hwnd, WINDOWCOMPOSITIONATTRIBDATA* data) {
    if (!data || data->Attrib != WCA_ACCENT_POLICY || !IsTaskbarWindow(hwnd)) {
        return CallSetWindowCompositionAttribute(hwnd, data);
    }

    ACCENT_POLICY policy = ReadAccentPolicy();
    WINDOWCOMPOSITIONATTRIBDATA overrideData = {};
    overrideData.Attrib = WCA_ACCENT_POLICY;
    overrideData.pvData = &policy;
    overrideData.cbData = sizeof(policy);
    return CallSetWindowCompositionAttribute(hwnd, &overrideData);
}

static void EnsureCompositionFunction() {
    if (!g_setWindowCompositionAttribute) {
        HMODULE user32 = GetModuleHandleW(L"user32.dll");
        g_setWindowCompositionAttribute = reinterpret_cast<SetWindowCompositionAttributeFunc>(
            GetProcAddress(user32, "SetWindowCompositionAttribute"));
    }
}

static BOOL CALLBACK ApplyToSecondaryTaskbar(HWND hwnd, LPARAM lParam) {
    if (IsTaskbarWindow(hwnd)) {
        auto context = reinterpret_cast<ACCENT_POLICY*>(lParam);
        ApplyAccentToWindow(hwnd, *context);
    }
    return TRUE;
}

static void ApplyTaskbarAccent() {
    EnsureCompositionFunction();
    if (!g_setWindowCompositionAttribute) {
        return;
    }

    ACCENT_POLICY policy = ReadAccentPolicy();
    HWND mainTaskbar = FindWindowW(L"Shell_TrayWnd", nullptr);
    if (mainTaskbar) {
        ApplyAccentToWindow(mainTaskbar, policy);
    }
    EnumWindows(ApplyToSecondaryTaskbar, reinterpret_cast<LPARAM>(&policy));
}

static void ResetTaskbarAccent() {
    if (!g_setWindowCompositionAttribute) {
        return;
    }

    ACCENT_POLICY policy = {};
    policy.AccentState = ACCENT_DISABLED;
    HWND mainTaskbar = FindWindowW(L"Shell_TrayWnd", nullptr);
    if (mainTaskbar) {
        ApplyAccentToWindow(mainTaskbar, policy);
    }
    EnumWindows(ApplyToSecondaryTaskbar, reinterpret_cast<LPARAM>(&policy));
}

BOOL Wh_ModInit() {
    EnsureCompositionFunction();
    if (g_setWindowCompositionAttribute) {
        Wh_SetFunctionHook(
            reinterpret_cast<void*>(g_setWindowCompositionAttribute),
            reinterpret_cast<void*>(SetWindowCompositionAttributeHook),
            reinterpret_cast<void**>(&g_originalSetWindowCompositionAttribute));
    }
    ApplyTaskbarAccent();
    return TRUE;
}

void Wh_ModSettingsChanged() {
    ApplyTaskbarAccent();
}

void Wh_ModUninit() {
    ResetTaskbarAccent();
}
