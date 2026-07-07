# Taskbar Accent

MyWallpaper add-on with a native Windhawk part. The Canvas OS layer is intentionally transparent; the native part runs inside `explorer.exe` and applies a live taskbar blur, transparent, or color accent.

## Settings

- `enabled`: turns the native taskbar accent on or off.
- `mode`: `blur`, `transparent`, or `accent`.
- `accentColor`: color used by transparent/accent modes.
- `opacity`: alpha from `0` to `255`.
Changing settings in MyWallpaper updates the Windhawk config, and the native mod applies the new value through `Wh_ModSettingsChanged`.

## Build

```bash
pnpm install
pnpm build
```
