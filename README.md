# Taskbar Accent

Taskbar Accent is a MyWallpaper native add-on for the Windows 11 taskbar.
Its Canvas layer reports native state while one shared `windhawk-v1` hook
applies blur, acrylic, transparency or a solid accent to every taskbar that
Explorer owns, including taskbars created after a monitor is connected.

All four controls are add-on scoped because Windows has one effective taskbar
configuration. MyWallpaper stores those values once, projects them to the
Windhawk settings store, and calls `Wh_ModSettingsChanged` after live updates.

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

## Safety

The hook targets only MyWallpaper's reviewed `windows-shell-v1` surface. The
desktop reports disabled, incompatible, conflicting and quarantined states and
stops reinjection after an Explorer-correlated crash loop.

The native code accepts only taskbar windows owned by the current Explorer
process and the `Shell_TrayWnd` or `Shell_SecondaryTrayWnd` classes. It applies
one `WCA_ACCENT_POLICY`, intercepts Explorer writes to that exact attribute and
resets the standard taskbar policy on unload. It has no XAML diagnostics
session, selector engine, image loader or network path. Invalid settings and
failed Windows composition calls are reported to both the Canvas layer and the
desktop add-on status instead of being hidden.

The add-on source is GPL-3.0-only because its narrow composition boundary is
adapted from Windhawk's official [Taskbar Background Helper at upstream commit
`d8457487c989d6e919c16c2248291af2ae3aa338`](https://github.com/ramensoftware/windhawk-mods/blob/d8457487c989d6e919c16c2248291af2ae3aa338/mods/taskbar-background-helper.wh.cpp).
The full license text is included in [LICENSE](LICENSE).
