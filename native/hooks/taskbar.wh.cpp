// SPDX-License-Identifier: GPL-3.0-only
// Taskbar Accent for MyWallpaper.
//
// This file adapts the GPL-3.0 Windows 11 Taskbar Styler 1.7 engine to the
// MyWallpaper device-settings and native-event contracts. The vendored engine
// remains in this GPL add-on and is never linked into MyWallpaper's proprietary
// desktop binary.

// ==WindhawkMod==
// @id              mywallpaper-taskbar-accent
// @name            MyWallpaper Taskbar Accent
// @description     Apply configurable Windows taskbar styling through Windhawk.
// @version         4.1.1
// @author          MyWallpaper
// @github          https://github.com/MyWallpapers/mywallpaper-addon-taskbar-accent
// @include         explorer.exe
// @compilerOptions -lcomctl32 -lole32 -lruntimeobject -Wl,--export-all-symbols
// ==/WindhawkMod==

#include <windows.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdarg>
#include <cwchar>
#include <cwctype>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Data.Json.h>

#include <mywallpaper_settings.hpp>
#include <mywallpaper_windhawk.hpp>
#include <windhawk_utils.h>

HRESULT MyWallpaperTaskbarStylerEngineResult() noexcept;
bool MyWallpaperTaskbarStylerHasAppliedCustomization() noexcept;
bool MyWallpaperTaskbarStylerUserSettingsHadError() noexcept;
void MyWallpaperTaskbarStylerResetUserSettingsError() noexcept;
void MyWallpaperTaskbarStylerNotifyWatcherResult(HRESULT result) noexcept;
void MyWallpaperTaskbarStylerNotifyCustomizationApplied() noexcept;
void MyWallpaperTaskbarStylerNotifyUserSettingsError() noexcept;

namespace mywallpaper_taskbar_styler {

using winrt::Windows::Data::Json::JsonArray;
using winrt::Windows::Data::Json::JsonObject;
using winrt::Windows::Data::Json::JsonValueType;

struct ControlStyle {
    std::wstring target;
    std::vector<std::wstring> styles;

    bool operator==(const ControlStyle&) const = default;
};

struct Projection {
    bool enabled = true;
    bool quickAppearance = true;
    std::wstring theme;
    std::wstring diagnosticsHandling = L"alert";
    std::vector<std::wstring> styleConstants;
    std::vector<ControlStyle> controlStyles;
    std::vector<std::wstring> resourceVariables;

    bool operator==(const Projection&) const = default;
};

std::mutex g_projectionMutex;
std::shared_ptr<const Projection> g_pendingProjection;
std::shared_ptr<const Projection> g_appliedProjection;
bool g_settingsInvalid = false;

constexpr std::wstring_view kQuickTheme = L"mywallpaper-quick";

bool IsAllowedTheme(std::wstring_view theme) {
    static constexpr std::wstring_view kThemes[] = {
        L"",
        kQuickTheme,
        L"TranslucentTaskbar",
        L"DockLike",
        L"SimplyTransparent",
        L"Squircle",
        L"Matter",
        L"WinXP",
        L"WinXP_variant_Zune",
        L"Bubbles",
        L"RosePine",
        L"WinVista",
        L"CleanSlate",
        L"Lucent",
        L"Lucent_variant_Light",
        L"SunValley",
        L"21996Taskbar",
        L"BottomDensy",
        L"BottomDensy_variant_NoInd",
        L"TaskbarXII",
        L"xdark",
        L"Windows7",
        L"Aeris",
        L"Plasma",
        L"WindowGlass",
        L"WindowGlass_variant_Split",
        L"WindowGlass_variant_FullLength",
        L"Surface",
        L"Oversimplified&Accentuated",
        L"Luminosity_variant_Dock",
        L"Luminosity_variant_Classic",
        L"Luminosity_variant_Compact",
        L"LayerMicaUI",
        L"Fluid",
        L"TintedGlass",
        L"TaskbarToStatusbar",
        L"UltraWideFriendly",
        L"LiquidGlass",
        L"LiquidGlass_variant_Alternate",
        L"Borderless",
    };
    return std::find(std::begin(kThemes), std::end(kThemes), theme) !=
           std::end(kThemes);
}

std::wstring Trim(std::wstring_view value) {
    while (!value.empty() && iswspace(value.front())) {
        value.remove_prefix(1);
    }
    while (!value.empty() && iswspace(value.back())) {
        value.remove_suffix(1);
    }
    return std::wstring(value);
}

std::vector<std::wstring> SplitNonEmptyLines(std::wstring_view value) {
    std::vector<std::wstring> lines;
    std::size_t start = 0;
    while (start <= value.size()) {
        const std::size_t end = value.find(L'\n', start);
        std::wstring line = Trim(value.substr(
            start, end == std::wstring_view::npos ? value.size() - start
                                                  : end - start));
        if (!line.empty()) {
            if (!line.empty() && line.back() == L'\r') {
                line.pop_back();
            }
            if (!line.empty()) {
                lines.push_back(std::move(line));
            }
        }
        if (end == std::wstring_view::npos) {
            break;
        }
        start = end + 1;
    }
    return lines;
}

std::wstring ReadString(const JsonObject& object,
                        std::wstring_view key,
                        std::wstring_view fallback) {
    const winrt::hstring hkey(key);
    if (!object.HasKey(hkey)) {
        return std::wstring(fallback);
    }
    const auto value = object.GetNamedValue(hkey);
    if (value.ValueType() != JsonValueType::String) {
        throw std::runtime_error("expected a JSON string");
    }
    return std::wstring(value.GetString());
}

bool ReadBoolean(const JsonObject& object,
                 std::wstring_view key,
                 bool fallback) {
    const winrt::hstring hkey(key);
    if (!object.HasKey(hkey)) {
        return fallback;
    }
    const auto value = object.GetNamedValue(hkey);
    if (value.ValueType() != JsonValueType::Boolean) {
        throw std::runtime_error("expected a JSON boolean");
    }
    return value.GetBoolean();
}

double ReadNumber(const JsonObject& object,
                  std::wstring_view key,
                  double fallback) {
    const winrt::hstring hkey(key);
    if (!object.HasKey(hkey)) {
        return fallback;
    }
    const auto value = object.GetNamedValue(hkey);
    if (value.ValueType() != JsonValueType::Number) {
        throw std::runtime_error("expected a JSON number");
    }
    return value.GetNumber();
}

bool IsRgbColor(std::wstring_view color) {
    if (color.size() != 7 || color.front() != L'#') {
        return false;
    }
    return std::all_of(color.begin() + 1, color.end(),
                       [](wchar_t ch) { return iswxdigit(ch) != 0; });
}

std::wstring FormatRatio(double ratio) {
    wchar_t buffer[32]{};
    swprintf_s(buffer, L"%.6f", ratio);
    std::wstring result(buffer);
    while (result.size() > 1 && result.back() == L'0') {
        result.pop_back();
    }
    if (!result.empty() && result.back() == L'.') {
        result.pop_back();
    }
    return result;
}

std::wstring WithAlpha(std::wstring_view rgb, int alpha) {
    wchar_t alphaText[3]{};
    swprintf_s(alphaText, L"%02X", alpha & 0xFF);
    return L"#" + std::wstring(alphaText) + std::wstring(rgb.substr(1));
}

std::vector<ControlStyle> BuildQuickStyles(std::wstring_view mode,
                                           std::wstring_view color,
                                           int opacity,
                                           int blurAmount,
                                           std::wstring_view layout,
                                           int horizontalInset,
                                           int bottomInset,
                                           int cornerRadius,
                                           int contentPadding) {
    const double opacityRatio = static_cast<double>(opacity) / 255.0;
    const std::wstring ratio = FormatRatio(opacityRatio);
    const std::wstring colorWithAlpha = WithAlpha(color, opacity);

    std::wstring brush;
    if (mode == L"transparent") {
        brush = L"<SolidColorBrush Color=\"#00000000\" />";
    } else if (mode == L"accent") {
        brush = L"<SolidColorBrush Color=\"" + colorWithAlpha + L"\" />";
    } else if (mode == L"acrylic") {
        brush = L"<WindhawkBlur BlurAmount=\"" +
                std::to_wstring(blurAmount) + L"\" TintColor=\"" +
                std::wstring(color) + L"\" TintOpacity=\"" + ratio +
                L"\" TintLuminosityOpacity=\"0.35\" "
                L"TintSaturation=\"1.35\" NoiseOpacity=\"0.08\" "
                L"NoiseDensity=\"0.8\" FallbackColor=\"" +
                colorWithAlpha + L"\" />";
    } else {
        brush = L"<WindhawkBlur BlurAmount=\"" +
                std::to_wstring(blurAmount) + L"\" TintColor=\"" +
                std::wstring(color) + L"\" TintOpacity=\"" + ratio +
                L"\" FallbackColor=\"" + colorWithAlpha + L"\" />";
    }

    const std::wstring clearBackground =
        L"Background:=<SolidColorBrush Color=\"#00000000\" />";
    const bool transparent = mode == L"transparent";
    const std::vector<std::wstring> fillStyles = {
        transparent ? L"Visibility=Collapsed" : L"Visibility=Visible",
        L"Fill:=" + brush,
    };
    const std::vector<std::wstring> strokeStyles = {
        L"Visibility=Collapsed",
        L"Fill:=<SolidColorBrush Color=\"#00000000\" />",
    };

    std::vector<ControlStyle> styles = {
        {L"Taskbar.TaskbarFrame > Grid#RootGrid", {clearBackground}},
        {L"Taskbar.TaskbarBackground#BackgroundControl", {clearBackground}},
        {L"Taskbar.TaskbarBackground#BackgroundControl > Grid",
         {clearBackground}},
        {L"Taskbar.TaskbarBackground#BackgroundControl > "
         L"Windows.UI.Xaml.Controls.Grid",
         {clearBackground}},
        {L"Taskbar.TaskbarFrame > Grid#RootGrid > Taskbar.TaskbarBackground > "
         L"Grid > Rectangle#BackgroundFill",
         fillStyles},
        {L"Taskbar.TaskbarBackground#BackgroundControl > Grid > "
         L"Rectangle#BackgroundFill",
         fillStyles},
        {L"Taskbar.TaskbarBackground#BackgroundControl > "
         L"Windows.UI.Xaml.Controls.Grid > "
         L"Windows.UI.Xaml.Shapes.Rectangle#BackgroundFill",
         fillStyles},
        {L"Taskbar.TaskbarBackground#HoverFlyoutBackgroundControl > Grid > "
         L"Rectangle#BackgroundFill",
         fillStyles},
        {L"Rectangle#BackgroundStroke", strokeStyles},
        {L"Taskbar.TaskbarFrame > Grid#RootGrid > Taskbar.TaskbarBackground > "
         L"Grid > Rectangle#BackgroundStroke",
         strokeStyles},
    };

    styles.push_back({
        L"Taskbar.TaskbarFrame > Grid#RootGrid",
        {L"Padding=" + std::to_wstring(contentPadding) + L",0," +
             std::to_wstring(contentPadding) + L",0",
         L"CornerRadius=" + std::to_wstring(cornerRadius)},
    });
    if (layout == L"floating") {
        styles.push_back({
            L"Taskbar.TaskbarFrame",
            {L"Width=Auto", L"HorizontalAlignment=Center",
             L"Margin=" + std::to_wstring(horizontalInset) + L",0," +
                 std::to_wstring(horizontalInset) + L"," +
                 std::to_wstring(bottomInset)},
        });
    }
    return styles;
}

std::vector<ControlStyle> ParseControlStyles(std::wstring_view json) {
    const std::wstring trimmed = Trim(json);
    if (trimmed.empty()) {
        return {};
    }

    const JsonArray array = JsonArray::Parse(winrt::hstring(trimmed));
    std::vector<ControlStyle> result;
    result.reserve(array.Size());
    for (uint32_t index = 0; index < array.Size(); ++index) {
        const auto entry = array.GetAt(index);
        if (entry.ValueType() != JsonValueType::Object) {
            throw std::runtime_error("control style entry must be an object");
        }
        const JsonObject object = entry.GetObject();
        const std::wstring target = ReadString(object, L"target", L"");
        if (target.empty()) {
            throw std::runtime_error("control style target must not be empty");
        }
        if (!object.HasKey(L"styles")) {
            throw std::runtime_error("control style styles are required");
        }
        const auto stylesValue = object.GetNamedValue(L"styles");
        if (stylesValue.ValueType() != JsonValueType::Array) {
            throw std::runtime_error("control style styles must be an array");
        }
        const JsonArray stylesArray = stylesValue.GetArray();
        std::vector<std::wstring> styles;
        styles.reserve(stylesArray.Size());
        for (uint32_t styleIndex = 0; styleIndex < stylesArray.Size();
             ++styleIndex) {
            const auto style = stylesArray.GetAt(styleIndex);
            if (style.ValueType() != JsonValueType::String) {
                throw std::runtime_error("control style must be a string");
            }
            std::wstring text = style.GetString().c_str();
            if (!text.empty()) {
                styles.push_back(std::move(text));
            }
        }
        result.push_back({target, std::move(styles)});
    }
    return result;
}

Projection ParseProjection() {
    const JsonObject root = JsonObject::Parse(
        winrt::to_hstring(mywallpaper::settings::json()));

    Projection result;
    result.enabled = ReadBoolean(root, L"enabled", true);
    if (!result.enabled) {
        // Disabling must remain possible even while an advanced editor contains
        // an incomplete theme or JSON document. The upstream engine only needs
        // an empty projection to restore the taskbar in this state.
        result.quickAppearance = false;
        result.theme.clear();
        return result;
    }

    const std::wstring selectedTheme =
        ReadString(root, L"theme", kQuickTheme);
    if (!IsAllowedTheme(selectedTheme)) {
        throw std::runtime_error("unknown taskbar theme");
    }
    result.quickAppearance = selectedTheme == kQuickTheme;
    result.theme = result.quickAppearance ? L"" : selectedTheme;

    result.diagnosticsHandling =
        ReadString(root, L"xamlDiagnosticsHandling", L"alert");
    if (result.diagnosticsHandling != L"alert" &&
        result.diagnosticsHandling != L"block" &&
        result.diagnosticsHandling != L"allow") {
        throw std::runtime_error("unknown diagnostics handling mode");
    }

    result.styleConstants = SplitNonEmptyLines(
        ReadString(root, L"styleConstants", L""));
    result.resourceVariables = SplitNonEmptyLines(
        ReadString(root, L"themeResourceVariables", L""));

    if (result.quickAppearance) {
        const std::wstring mode = ReadString(root, L"mode", L"blur");
        if (mode != L"blur" && mode != L"acrylic" &&
            mode != L"transparent" && mode != L"accent") {
            throw std::runtime_error("unknown quick appearance mode");
        }
        const std::wstring color =
            ReadString(root, L"accentColor", L"#1f8fff");
        const double opacityValue = ReadNumber(root, L"opacity", 128);
        const double blurValue = ReadNumber(root, L"blurAmount", 18);
        const std::wstring layout = ReadString(root, L"layout", L"full");
        const double horizontalInsetValue =
            ReadNumber(root, L"horizontalInset", 250);
        const double bottomInsetValue = ReadNumber(root, L"bottomInset", 0);
        const double cornerRadiusValue = ReadNumber(root, L"cornerRadius", 12);
        const double contentPaddingValue = ReadNumber(root, L"contentPadding", 6);
        if (!IsRgbColor(color) || !std::isfinite(opacityValue) ||
            std::trunc(opacityValue) != opacityValue || opacityValue < 0 ||
            opacityValue > 255 || !std::isfinite(blurValue) ||
            std::trunc(blurValue) != blurValue || blurValue < 0 ||
            blurValue > 100 ||
            (layout != L"full" && layout != L"floating") ||
            !std::isfinite(horizontalInsetValue) ||
            std::trunc(horizontalInsetValue) != horizontalInsetValue ||
            horizontalInsetValue < 0 || horizontalInsetValue > 600 ||
            !std::isfinite(bottomInsetValue) ||
            std::trunc(bottomInsetValue) != bottomInsetValue ||
            bottomInsetValue < 0 || bottomInsetValue > 16 ||
            !std::isfinite(cornerRadiusValue) ||
            std::trunc(cornerRadiusValue) != cornerRadiusValue ||
            cornerRadiusValue < 0 || cornerRadiusValue > 32 ||
            !std::isfinite(contentPaddingValue) ||
            std::trunc(contentPaddingValue) != contentPaddingValue ||
            contentPaddingValue < 0 || contentPaddingValue > 24) {
            throw std::runtime_error("invalid quick appearance value");
        }
        result.controlStyles = BuildQuickStyles(
            mode, color, static_cast<int>(opacityValue),
            static_cast<int>(blurValue), layout,
            static_cast<int>(horizontalInsetValue),
            static_cast<int>(bottomInsetValue),
            static_cast<int>(cornerRadiusValue),
            static_cast<int>(contentPaddingValue));
    }

    auto advanced = ParseControlStyles(
        ReadString(root, L"controlStyles", L"[]"));
    result.controlStyles.insert(result.controlStyles.end(),
                                std::make_move_iterator(advanced.begin()),
                                std::make_move_iterator(advanced.end()));
    return result;
}

enum class ReloadResult {
    Invalid,
    Unchanged,
    Recovered,
    Changed,
};

ReloadResult InitializeProjection() noexcept {
    try {
        auto next = std::make_shared<const Projection>(ParseProjection());
        std::lock_guard lock(g_projectionMutex);
        g_pendingProjection = next;
        g_appliedProjection = std::move(next);
        g_settingsInvalid = false;
        return ReloadResult::Changed;
    } catch (...) {
        std::lock_guard lock(g_projectionMutex);
        g_pendingProjection.reset();
        g_settingsInvalid = true;
        return ReloadResult::Invalid;
    }
}

ReloadResult ReloadPendingProjection() noexcept {
    try {
        auto next = std::make_shared<const Projection>(ParseProjection());
        std::lock_guard lock(g_projectionMutex);
        const bool recovered = std::exchange(g_settingsInvalid, false);
        if (g_pendingProjection && *next == *g_pendingProjection) {
            return recovered ? ReloadResult::Recovered
                             : ReloadResult::Unchanged;
        }
        if (recovered && g_appliedProjection &&
            *next == *g_appliedProjection) {
            // The invalid editor state never replaced the applied projection.
            // Re-emit that durable state instead of arming a no-op apply.
            g_pendingProjection = g_appliedProjection;
            return ReloadResult::Recovered;
        }
        g_pendingProjection = std::move(next);
        return ReloadResult::Changed;
    } catch (...) {
        std::lock_guard lock(g_projectionMutex);
        // Invalidate any debounced generation that hasn't been promoted yet.
        // A timer already in flight may finish applying the last valid
        // projection, but it must not announce it over the current error.
        g_pendingProjection.reset();
        g_settingsInvalid = true;
        return ReloadResult::Invalid;
    }
}

bool SettingsAreInvalid() {
    std::lock_guard lock(g_projectionMutex);
    return g_settingsInvalid;
}

std::shared_ptr<const Projection> AppliedProjection() {
    std::lock_guard lock(g_projectionMutex);
    if (g_appliedProjection) {
        return g_appliedProjection;
    }
    static const auto empty = std::make_shared<const Projection>();
    return empty;
}

std::shared_ptr<const Projection> PromotePendingProjection() {
    std::lock_guard lock(g_projectionMutex);
    if (g_settingsInvalid || !g_pendingProjection ||
        (g_appliedProjection &&
         *g_pendingProjection == *g_appliedProjection)) {
        return nullptr;
    }
    g_appliedProjection = g_pendingProjection;
    return g_appliedProjection;
}

std::optional<std::size_t> ParseIndex(std::wstring_view value) {
    if (value.empty()) {
        return std::nullopt;
    }
    std::size_t result = 0;
    for (const wchar_t ch : value) {
        if (ch < L'0' || ch > L'9') {
            return std::nullopt;
        }
        const std::size_t digit = static_cast<std::size_t>(ch - L'0');
        if (result > (std::numeric_limits<std::size_t>::max() - digit) / 10) {
            return std::nullopt;
        }
        result = result * 10 + digit;
    }
    return result;
}

std::wstring LookupProjectedSetting(std::wstring_view key) {
    // Keep one immutable generation alive for this lookup. Pending editor
    // changes never become visible to the upstream engine until the lifecycle
    // apply promotes them atomically.
    const auto snapshot = AppliedProjection();
    const Projection& projection = *snapshot;
    if (key == L"xamlDiagnosticsHandling") {
        return projection.diagnosticsHandling;
    }
    if (!projection.enabled) {
        return L"";
    }
    if (key == L"theme") {
        return projection.theme;
    }

    constexpr std::wstring_view kStyleConstantsPrefix = L"styleConstants[";
    if (key.starts_with(kStyleConstantsPrefix) && key.ends_with(L"]")) {
        const auto index = ParseIndex(key.substr(
            kStyleConstantsPrefix.size(),
            key.size() - kStyleConstantsPrefix.size() - 1));
        if (index && *index < projection.styleConstants.size()) {
            return projection.styleConstants[*index];
        }
        return L"";
    }

    constexpr std::wstring_view kResourcePrefix = L"themeResourceVariables[";
    if (key.starts_with(kResourcePrefix) && key.ends_with(L"]")) {
        const auto index = ParseIndex(key.substr(
            kResourcePrefix.size(), key.size() - kResourcePrefix.size() - 1));
        if (index && *index < projection.resourceVariables.size()) {
            return projection.resourceVariables[*index];
        }
        return L"";
    }

    constexpr std::wstring_view kControlPrefix = L"controlStyles[";
    if (key.starts_with(kControlPrefix)) {
        const std::size_t close = key.find(L']', kControlPrefix.size());
        if (close == std::wstring_view::npos) {
            return L"";
        }
        const auto controlIndex = ParseIndex(
            key.substr(kControlPrefix.size(), close - kControlPrefix.size()));
        if (!controlIndex || *controlIndex >= projection.controlStyles.size()) {
            return L"";
        }
        const auto& control = projection.controlStyles[*controlIndex];
        const std::wstring_view suffix = key.substr(close + 1);
        if (suffix == L".target") {
            return control.target;
        }
        constexpr std::wstring_view kStylesPrefix = L".styles[";
        if (suffix.starts_with(kStylesPrefix) && suffix.ends_with(L"]")) {
            const auto styleIndex = ParseIndex(suffix.substr(
                kStylesPrefix.size(),
                suffix.size() - kStylesPrefix.size() - 1));
            if (styleIndex && *styleIndex < control.styles.size()) {
                return control.styles[*styleIndex];
            }
        }
    }
    return L"";
}

void EmitInvalidSettings() {
    mywallpaper::windhawk::emit_event(
        "mywallpaper.taskbar-accent/v1/invalid-settings",
        R"({"cause":"Taskbar Accent received an invalid theme or advanced XAML document.","action":"Correct the highlighted add-on settings; the last valid appearance remains active."})");
}

void EmitCurrentState(const std::shared_ptr<const Projection>& projection) {
    if (projection && projection->enabled) {
        if (MyWallpaperTaskbarStylerUserSettingsHadError()) {
            mywallpaper::windhawk::emit_event(
                "mywallpaper.taskbar-accent/v1/advanced-style-errors",
                R"({"cause":"One or more custom XAML rules could not be parsed or applied.","action":"Review the advanced control styles and resource variables; the compatible theme rules remain active."})");
            return;
        }
        const HRESULT engineResult = MyWallpaperTaskbarStylerEngineResult();
        if (engineResult == E_PENDING) {
            // AdviseVisualTreeChange runs asynchronously. No status is emitted
            // until the watcher has either attached or failed explicitly.
            return;
        }
        if (FAILED(engineResult)) {
            mywallpaper::windhawk::emit_event(
                "mywallpaper.taskbar-accent/v1/engine-unavailable",
                R"({"cause":"Taskbar Styler could not attach to Windows XAML diagnostics.","action":"Close another taskbar styling tool or choose the appropriate XAML diagnostics handling mode."})");
            return;
        }
        if (!MyWallpaperTaskbarStylerHasAppliedCustomization()) {
            // AdviseVisualTreeChange succeeded, but the watcher hasn't yet
            // applied a matching rule. Its first successful customization
            // will emit the durable applied state from the callback thread.
            return;
        }
        mywallpaper::windhawk::emit_event(
            "mywallpaper.taskbar-accent/v1/applied",
            R"({"state":"active","engine":"windows-11-taskbar-styler-1.7"})");
    } else {
        mywallpaper::windhawk::emit_event(
            "mywallpaper.taskbar-accent/v1/disabled",
            R"({"state":"disabled"})");
    }
}

}  // namespace mywallpaper_taskbar_styler

bool MyWallpaperTaskbarStylerProjectionEnabled() noexcept {
    return mywallpaper_taskbar_styler::AppliedProjection()->enabled;
}

constexpr WCHAR kAllocationFailureSetting[] = L"";

PCWSTR MyWallpaperTaskbarStylerGetStringSetting(PCWSTR format, ...) noexcept {
    if (!format) {
        format = L"";
    }
    wchar_t key[256]{};
    va_list arguments;
    va_start(arguments, format);
    const int written = _vsnwprintf_s(key, _countof(key), _TRUNCATE, format,
                                     arguments);
    va_end(arguments);

    std::wstring value;
    if (written >= 0) {
        value = mywallpaper_taskbar_styler::LookupProjectedSetting(key);
    }
    const SIZE_T bytes = (value.size() + 1) * sizeof(wchar_t);
    auto* copy = static_cast<wchar_t*>(
        HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, bytes));
    if (!copy) {
        // The imported engine assumes a non-null setting and calls wcscmp on
        // it. Returning an owned empty fallback keeps Explorer alive under
        // extreme memory pressure; the paired free recognizes this sentinel.
        return kAllocationFailureSetting;
    }
    if (!value.empty()) {
        memcpy(copy, value.data(), value.size() * sizeof(wchar_t));
    }
    return copy;
}

void MyWallpaperTaskbarStylerFreeStringSetting(PCWSTR value) noexcept {
    if (value && value != kAllocationFailureSetting) {
        HeapFree(GetProcessHeap(), 0, const_cast<PWSTR>(value));
    }
}

#define MYWALLPAPER_DISABLE_UPSTREAM_STATISTICS 1
#define Wh_GetStringSetting MyWallpaperTaskbarStylerGetStringSetting
#define Wh_FreeStringSetting MyWallpaperTaskbarStylerFreeStringSetting
#define Wh_ModInit UpstreamTaskbarStylerModInit
#define Wh_ModAfterInit UpstreamTaskbarStylerAfterInit
#define Wh_ModSettingsChanged UpstreamTaskbarStylerSettingsChanged
#define Wh_ModUninit UpstreamTaskbarStylerUninit

#include "vendor/windows-11-taskbar-styler-v1.7.inc"

#undef Wh_ModUninit
#undef Wh_ModSettingsChanged
#undef Wh_ModAfterInit
#undef Wh_ModInit
#undef Wh_FreeStringSetting
#undef Wh_GetStringSetting
#undef MYWALLPAPER_DISABLE_UPSTREAM_STATISTICS

HRESULT MyWallpaperTaskbarStylerEngineResult() noexcept {
    return g_mywallpaperLastTapResult.load(std::memory_order_acquire);
}

bool MyWallpaperTaskbarStylerUserSettingsHadError() noexcept {
    return g_mywallpaperUserSettingsHadError.load(std::memory_order_acquire);
}

bool MyWallpaperTaskbarStylerHasAppliedCustomization() noexcept {
    return g_mywallpaperAppliedAtLeastOneCustomization.load(
        std::memory_order_acquire);
}

void MyWallpaperTaskbarStylerResetUserSettingsError() noexcept {
    g_mywallpaperUserSettingsHadError.store(false,
                                             std::memory_order_release);
}

namespace {

constexpr LONGLONG kSettingsDebounce100ns = 250LL * 10'000LL;

std::mutex g_upstreamLifecycleMutex;
std::mutex g_eventEmissionMutex;
PTP_TIMER g_settingsReloadTimer = nullptr;
bool g_upstreamUnloading = false;
std::atomic_bool g_upstreamEventsEnabled = false;
std::atomic_bool g_upstreamApplyInProgress = false;

class ScopedUpstreamApplyPhase {
   public:
    ScopedUpstreamApplyPhase() noexcept {
        g_upstreamApplyInProgress.store(true, std::memory_order_release);
    }

    ~ScopedUpstreamApplyPhase() {
        g_upstreamApplyInProgress.store(false, std::memory_order_release);
    }
};

class ScopedMultithreadedApartment {
   public:
    ScopedMultithreadedApartment() noexcept
        : result_(CoInitializeEx(nullptr, COINIT_MULTITHREADED)) {}

    ~ScopedMultithreadedApartment() {
        if (result_ == S_OK || result_ == S_FALSE) {
            CoUninitialize();
        }
    }

   private:
    HRESULT result_;
};

void EmitCurrentStateIfEnabled(
    const std::shared_ptr<const mywallpaper_taskbar_styler::Projection>&
        projection) noexcept;

void ApplyProjectedSettings() {
    std::shared_ptr<const mywallpaper_taskbar_styler::Projection> applied;
    {
        std::lock_guard lock(g_upstreamLifecycleMutex);
        if (g_upstreamUnloading) {
            return;
        }
        applied =
            mywallpaper_taskbar_styler::PromotePendingProjection();
        if (!applied) {
            return;
        }
        // Thread-pool callbacks don't guarantee a COM apartment. The imported
        // engine talks to XAML diagnostics and is safe to enter from either an
        // existing apartment or this temporary MTA.
        ScopedMultithreadedApartment apartment;
        ScopedUpstreamApplyPhase applyPhase;
        MyWallpaperTaskbarStylerResetUserSettingsError();
        UpstreamTaskbarStylerSettingsChanged();
    }
    // Native events use a synchronous pipe and must never hold the lifecycle
    // lock needed by another apply or by module unload.
    if (!mywallpaper_taskbar_styler::SettingsAreInvalid()) {
        EmitCurrentStateIfEnabled(applied);
    }
}

void CALLBACK SettingsReloadTimerCallback(PTP_CALLBACK_INSTANCE,
                                          PVOID,
                                          PTP_TIMER) {
    ApplyProjectedSettings();
}

bool ScheduleProjectedSettings() {
    std::lock_guard lock(g_upstreamLifecycleMutex);
    if (g_upstreamUnloading || !g_settingsReloadTimer) {
        return false;
    }
    LARGE_INTEGER dueTime{};
    dueTime.QuadPart = -kSettingsDebounce100ns;
    FILETIME dueTimeFile{
        .dwLowDateTime = dueTime.LowPart,
        .dwHighDateTime = static_cast<DWORD>(dueTime.HighPart),
    };
    SetThreadpoolTimer(g_settingsReloadTimer, &dueTimeFile, 0, 0);
    return true;
}

void EmitCurrentStateFromCallbackNoexcept() noexcept {
    try {
        std::lock_guard emissionLock(g_eventEmissionMutex);
        if (!g_upstreamEventsEnabled.load(std::memory_order_acquire)) {
            return;
        }
        mywallpaper_taskbar_styler::EmitCurrentState(
            mywallpaper_taskbar_styler::AppliedProjection());
    } catch (...) {
        // These callbacks execute inside Explorer. A diagnostic allocation or
        // pipe failure must never escape a noexcept boundary and terminate it.
        Wh_Log(L"Taskbar Accent couldn't emit its runtime state");
    }
}

void EmitCurrentStateIfEnabled(
    const std::shared_ptr<const mywallpaper_taskbar_styler::Projection>&
        projection) noexcept {
    try {
        std::lock_guard emissionLock(g_eventEmissionMutex);
        if (!g_upstreamEventsEnabled.load(std::memory_order_acquire)) {
            return;
        }
        mywallpaper_taskbar_styler::EmitCurrentState(projection);
    } catch (...) {
        Wh_Log(L"Taskbar Accent couldn't emit its runtime state");
    }
}

void DisableRuntimeEventsAndWait() noexcept {
    std::lock_guard emissionLock(g_eventEmissionMutex);
    g_upstreamEventsEnabled.store(false, std::memory_order_release);
}

}  // namespace

void MyWallpaperTaskbarStylerNotifyWatcherResult(HRESULT result) noexcept {
    if (!g_upstreamEventsEnabled.load(std::memory_order_acquire)) {
        return;
    }
    g_mywallpaperLastTapResult.store(result, std::memory_order_release);
    if (!g_upstreamApplyInProgress.load(std::memory_order_acquire)) {
        EmitCurrentStateFromCallbackNoexcept();
    }
}

void MyWallpaperTaskbarStylerNotifyCustomizationApplied() noexcept {
    if (!g_upstreamEventsEnabled.load(std::memory_order_acquire) ||
        g_upstreamApplyInProgress.load(std::memory_order_acquire)) {
        return;
    }
    EmitCurrentStateFromCallbackNoexcept();
}

void MyWallpaperTaskbarStylerNotifyUserSettingsError() noexcept {
    if (!g_upstreamEventsEnabled.load(std::memory_order_acquire) ||
        g_upstreamApplyInProgress.load(std::memory_order_acquire)) {
        return;
    }
    EmitCurrentStateFromCallbackNoexcept();
}

BOOL Wh_ModInit() {
    const auto reload = mywallpaper_taskbar_styler::InitializeProjection();
    if (reload == mywallpaper_taskbar_styler::ReloadResult::Invalid) {
        mywallpaper_taskbar_styler::EmitInvalidSettings();
        return FALSE;
    }
    {
        std::lock_guard lock(g_upstreamLifecycleMutex);
        g_upstreamUnloading = false;
    }
    g_upstreamEventsEnabled.store(true, std::memory_order_release);
    if (!UpstreamTaskbarStylerModInit()) {
        DisableRuntimeEventsAndWait();
        return FALSE;
    }
    g_settingsReloadTimer = CreateThreadpoolTimer(
        SettingsReloadTimerCallback, nullptr, nullptr);
    if (!g_settingsReloadTimer) {
        Wh_Log(L"Taskbar Accent couldn't create the settings debounce timer; "
               L"settings will still apply synchronously.");
    }
    return TRUE;
}

void Wh_ModAfterInit() {
    std::shared_ptr<const mywallpaper_taskbar_styler::Projection> applied;
    {
        std::lock_guard lock(g_upstreamLifecycleMutex);
        if (g_upstreamUnloading) {
            return;
        }
        ScopedUpstreamApplyPhase applyPhase;
        MyWallpaperTaskbarStylerResetUserSettingsError();
        UpstreamTaskbarStylerAfterInit();
        applied = mywallpaper_taskbar_styler::AppliedProjection();
    }
    EmitCurrentStateIfEnabled(applied);
}

void Wh_ModSettingsChanged() {
    const auto reload =
        mywallpaper_taskbar_styler::ReloadPendingProjection();
    if (reload == mywallpaper_taskbar_styler::ReloadResult::Invalid) {
        mywallpaper_taskbar_styler::EmitInvalidSettings();
        return;
    }
    if (reload == mywallpaper_taskbar_styler::ReloadResult::Unchanged) {
        return;
    }
    if (reload == mywallpaper_taskbar_styler::ReloadResult::Recovered) {
        EmitCurrentStateIfEnabled(
            mywallpaper_taskbar_styler::AppliedProjection());
        return;
    }
    if (!ScheduleProjectedSettings()) {
        ApplyProjectedSettings();
    }
}

void Wh_ModUninit() {
    // Stop new events and wait for any synchronous pipe emission already in
    // flight before tearing down the XAML watcher and its state.
    DisableRuntimeEventsAndWait();
    {
        std::lock_guard lock(g_upstreamLifecycleMutex);
        g_upstreamUnloading = true;
    }
    if (g_settingsReloadTimer) {
        SetThreadpoolTimer(g_settingsReloadTimer, nullptr, 0, 0);
        WaitForThreadpoolTimerCallbacks(g_settingsReloadTimer, TRUE);
        CloseThreadpoolTimer(g_settingsReloadTimer);
        g_settingsReloadTimer = nullptr;
    }
    std::lock_guard lock(g_upstreamLifecycleMutex);
    UpstreamTaskbarStylerUninit();
}
