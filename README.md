# Taskbar Accent

Taskbar Accent is a GPL-3.0 MyWallpaper native add-on for the Windows 11
taskbar. It uses the deep XAML engine from Windows 11 Taskbar Styler 1.7 rather
than tinting only the outer taskbar window.

The distinction matters: Windows draws `BackgroundFill`, `BackgroundStroke`
and other XAML elements above the taskbar window. Styling only
`Shell_TrayWnd` leaves the familiar black or gray veil. Taskbar Accent now
modifies those real visual-tree elements and restores their original values
when the add-on is disabled or unloaded.

One shared `windhawk-v1` hook applies the device profile to every taskbar owned
by Explorer, including taskbars created after a monitor is connected. The
Canvas layer only reports status; it does not own a second native process or a
second settings store.

## Appearance modes

The Theme setting has three kinds of values:

- **MyWallpaper quick appearance** preserves the simple Blur, Acrylic,
  Transparent and Accent controls. These modes are implemented with XAML
  brushes, not `WCA_ACCENT_POLICY`.
- **None (advanced styles only)** keeps the standard Windows appearance and
  applies only the advanced rules entered below.
- **Integrated themes** exposes all 38 presets shipped with Taskbar Styler 1.7,
  including SimplyTransparent, TranslucentTaskbar, WindowGlass, LiquidGlass,
  DockLike and Borderless.

The application order is deterministic:

1. the integrated theme, if selected;
2. the MyWallpaper quick background rules, only in quick mode;
3. user control styles, which can override either of the previous layers.

Theme style constants and resource variables use the same upstream syntax.

## Advanced styling

`Style constants` accepts one `name=value` entry per line.

`Control styles` accepts a JSON array:

```json
[
  {
    "target": "Taskbar.TaskListButton",
    "styles": ["CornerRadius=0"]
  },
  {
    "target": "Rectangle#RunningIndicator",
    "styles": ["Fill=#FFED7014", "Height=2", "Width=12"]
  }
]
```

Targets, visual states, captured values, XAML values and the complete
`WindhawkBlur` object are handled by the upstream engine. `WindhawkBlur`
supports `BlurAmount`, `TintColor`, `TintOpacity`,
`TintLuminosityOpacity`, `TintSaturation`, `NoiseOpacity`, `NoiseDensity` and
`FallbackColor`.

`Resource variables` accepts one `Key=Value`, `Key@Dark=Value` or
`Key@Light=Value` entry per line.

Windows permits only one XAML diagnostics consumer at a time. The diagnostics
setting can ask before blocking another consumer, always block it, or allow it
at the risk of the taskbar styling session being replaced.

Malformed advanced JSON is rejected without tearing down the last valid
appearance. Device settings are shared by every Taskbar Accent layer so two
layers cannot fight over Explorer. The native transport imposes no local
product quota on the complete advanced-settings document.

## Development

Use Node.js 22 or newer and the pnpm version pinned by `packageManager`:

```powershell
pnpm install --frozen-lockfile
pnpm typecheck
pnpm build
pnpm test
```

For a native preview, run `mywallpaper dev`, enable Developer Mode in
MyWallpaper Desktop and load the loopback URL printed by the CLI. The CLI owns
the pinned compiler, generated device-settings projection and native outputs.

After changing the manifest or runtime contracts, regenerate the committed
types and native header:

```powershell
mywallpaper generate
mywallpaper check
```

Native setting changes call `Wh_ModSettingsChanged` in the already attached
hook. They do not restart Explorer, a companion process or the Canvas layer.

## Publishing

Pushing the newly-created SemVer tag `v4.1.0` triggers the public
MyWallpaper `admission-v1` workflow pinned by its exact reviewed commit SHA.
The repository must have GitHub release immutability enabled once in its
settings. The workflow creates a draft, attaches deterministic 1 GiB
content-addressed parts for each logical archive, publishes it as an immutable
release, and only then requests MyWallpaper admission. Two independent
GitHub-hosted Windows
builds must produce identical
web, companion and hook bytes. A separate verifier creates the deterministic
bundle, distribution SBOM, in-toto provenance and admission subject. The
GitHub-hosted Ubuntu publisher attests the exact release ZIP through
GitHub/Sigstore and uses a separate finalizer OIDC token for native evidence.

```bash
git tag v4.1.0
git push origin v4.1.0
```

An exact rerun reuses only the same draft or already immutable release and the
same content-addressed parts. It never replaces or deletes an asset. If
GitHub immutability is disabled, publication fails closed before any
MyWallpaper token is requested. An owner must delete the resulting mutable
release, enable immutability, and rerun the same workflow. If an immutable
release has already been rejected by MyWallpaper, the correction uses a new
version/tag because GitHub prevents reuse of the locked tag.

The caller uses the exact 40-character commit advertised by the protected
`admission-v1` discovery branch, while the MyWallpaper backend trusts only that
same workflow commit from an explicit SHA allowlist. Moving the public branch
alone therefore neither changes executed code nor authorizes a revision.
GitHub attributes reusable-workflow runner usage to this
caller repository, so this add-on must itself remain public for standard hosted
runner compute to be free and for artifact attestations to remain available
without a GitHub Enterprise Cloud requirement. Actions transfer artifacts are
retained for one day, but their storage still counts against the caller
account's allowance and can be metered beyond it. Publishing only the toolchain
repository does not change that attribution. MyWallpaper does not count one
paid “unit” per add-on creation or publication in this flow; the only variable
cost here is the caller's actual GitHub usage under its plan.

## Security and network behavior

This hook executes in the Windows process targets declared by its official
Windhawk metadata after the normal native consent flow. MyWallpaper does not
rewrite the target, architecture, include/exclude directives, or compiler
options, and the mod is not a sandbox.

The theme-usage statistics present in upstream Taskbar Styler are compiled out
of this add-on. Some optional integrated themes contain explicit image URLs to
GitHub assets pinned to styling-guide commit
`b5d848f098409b240480563b737898af472b595e`; selecting one of those themes
allows its XAML image brush to load that immutable asset. Quick mode,
advanced-only mode and themes without remote images do not use that path.

The canonical Canvas instance publishes hook state on the scene-local
`mywallpaper.taskbar-accent/v1/status` bus topic. The payload contains only the
state and human-readable detail.

## License and provenance

The add-on is GPL-3.0-only. It adapts [Windows 11 Taskbar Styler 1.7 at commit
`cf0c6b1d2269380846d0da868898d35fc8678c06`](https://github.com/ramensoftware/windhawk-mods/blob/cf0c6b1d2269380846d0da868898d35fc8678c06/mods/windows-11-taskbar-styler.wh.cpp).
The upstream license and attributions are preserved in the source. See
`THIRD_PARTY_NOTICES.md` and `LICENSE` for the complete notices.
