# Third-party notices

## Windows 11 Taskbar Styler 1.7

Taskbar Accent contains a modified copy of **Windows 11 Taskbar Styler 1.7** by
Michael Maltsev (`m417z`) and contributors.

- Upstream repository: <https://github.com/ramensoftware/windhawk-mods>
- Pinned source commit: [`cf0c6b1d2269380846d0da868898d35fc8678c06`](https://github.com/ramensoftware/windhawk-mods/commit/cf0c6b1d2269380846d0da868898d35fc8678c06)
- Upstream source SHA-256: `2de27dbfa5fa8ff4f7d73e9bf6cf33d9d61bdf94d25384df15ccb1a22401327f`
- Vendored fork SHA-256: `ac822e3cbc863ba6a45fc2e6cbed9ecac60fb06a76bfd6824d81909ee97dc16c`
- Theme asset repository commit: `b5d848f098409b240480563b737898af472b595e`
- License: GNU General Public License v3.0

The corresponding source is stored at
`native/hooks/vendor/windows-11-taskbar-styler-v1.7.inc`. MyWallpaper's adapter
is stored at `native/hooks/taskbar.wh.cpp`.

MyWallpaper modifications are limited to:

- projecting MyWallpaper device settings into the upstream setting names;
- adding the quick taskbar appearance rules;
- preserving the last valid configuration when advanced JSON is malformed;
- making the two XAML-diagnostics flags atomic for safe cross-thread reloads;
- exposing only the XAML-engine attach result to the MyWallpaper adapter;
- waiting for the real visual-tree subscription and first matching
  customization before reporting the engine as applied;
- owning and joining the asynchronous visual-tree subscription thread during
  unload;
- surfacing rejected user-authored XAML rules without treating incompatible
  upstream theme rules as user errors;
- bypassing the diagnostics-consumer interception while the add-on is disabled;
- pinning runtime theme-asset URLs to an immutable styling-guide commit;
- reporting native state through MyWallpaper events;
- disabling the upstream theme-usage statistics code at compile time.

The upstream engine contains code derived from TranslucentTB's
`XamlBlurBrush`. Its original attribution is retained in the vendored source.
The full GPL-3.0 license text for this add-on is available in `LICENSE`.

Some integrated themes reference image assets hosted by their authors on
GitHub. Those URLs are part of the selected theme; Taskbar Accent does not
mirror, replace or silently download them when another theme is selected.
