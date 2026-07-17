# Taskbar Accent

Taskbar Accent is a MyWallpaper native add-on for the Windows 11 taskbar.
Its Canvas layer reports native state while one shared `windhawk-v1` hook
applies blur, acrylic, transparency or a solid accent to every taskbar that
Explorer owns, including taskbars created after a monitor is connected.

All four controls use the `device` scope because Windows has one effective
taskbar configuration. Canvas and the hook observe the same validated device
profile; there is no second native settings store owned by the add-on.

This baseline uses the immutable `canvas-v1` release contract. A published
release is selected explicitly by its SemVer and distribution digest.
MyWallpaper may recommend a newer release, but never updates an installed layer
automatically.

## Development

Use Node.js 22 or newer and the pnpm version pinned by `packageManager`:

```powershell
pnpm install --frozen-lockfile
pnpm dev
```

For the complete native preview, run `mywallpaper dev` from this directory
with the released `@mywallpaper/cli`, enable Developer Mode in MyWallpaper
Desktop, then load the loopback URL printed by the CLI. The CLI owns the pinned
compiler, generated settings projection and x86, x64 and ARM64 hook outputs. A
failed native build removes the previous preview instead of serving stale DLLs.
`mywallpaper generate` derives the committed
`generated/mywallpaper-runtime.d.ts` Canvas contract and
`native/generated/mywallpaper_settings.hpp` after contract or device-setting
changes. The native header is not committed; the CLI recreates it before every
official build. `mywallpaper check` rejects a missing/stale Canvas declaration
or any C/C++ source/header that bypasses the generated device getters.

## Safety

The hook targets MyWallpaper's product-owned `windows-shell-v1` surface, meaning
code executed inside Explorer rather than a taskbar sandbox. The
desktop reports disabled, incompatible and conflicting states. Explorer crashes
attributed to a recently active hook remain diagnostic evidence only: they do
not quarantine the add-on or prevent a later reinjection.

The native code accepts only taskbar windows owned by the current Explorer
process and the `Shell_TrayWnd` or `Shell_SecondaryTrayWnd` classes. It applies
one `WCA_ACCENT_POLICY`, intercepts Explorer writes to that exact attribute and
resets the standard taskbar policy on unload. It has no XAML diagnostics
session, selector engine, image loader or network path. Invalid settings and
failed Windows composition calls are reported to both the Canvas layer and the
desktop add-on status instead of being hidden.

The canonical Canvas instance also publishes the current hook state on the
scene-local `mywallpaper.taskbar-accent/v1/status` bus topic. The payload is
ordinary JSON and includes the state plus the same human-readable detail shown
by the layer, allowing another add-on in the scene to react without receiving
special native privileges.

The add-on source is GPL-3.0-only because its narrow composition boundary is
adapted from Windhawk's official [Taskbar Background Helper at upstream commit
`d8457487c989d6e919c16c2248291af2ae3aa338`](https://github.com/ramensoftware/windhawk-mods/blob/d8457487c989d6e919c16c2248291af2ae3aa338/mods/taskbar-background-helper.wh.cpp).
The full license text is included in [LICENSE](LICENSE). The add-on thumbnail
is an authored, immutable bundle asset; MyWallpaper validates and re-encodes it
for the catalogue without choosing alternate content.
