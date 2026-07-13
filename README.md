# Taskbar Accent

Taskbar Accent is a MyWallpaper native add-on for the single Windows taskbar.
Its Canvas layer stays transparent while one shared `windhawk-v1` hook applies
blur, acrylic, transparency or a solid accent inside Explorer.

All four controls are add-on scoped because Windows has one effective taskbar
configuration. MyWallpaper stores those values once, projects them to the
Windhawk settings store, and calls `Wh_ModSettingsChanged` after live updates.

## Development

```powershell
corepack pnpm install
corepack pnpm dev
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

The add-on source is GPL-3.0-or-later because it is adapted from Windhawk's
Windows 11 Taskbar Styler.
