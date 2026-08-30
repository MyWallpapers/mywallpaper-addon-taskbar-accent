import { createHash } from 'node:crypto'
import { readFile } from 'node:fs/promises'
import { fileURLToPath } from 'node:url'
import { dirname, resolve } from 'node:path'

const root = resolve(dirname(fileURLToPath(import.meta.url)), '..')
const manifest = JSON.parse(await readFile(resolve(root, 'manifest.json'), 'utf8'))
const packageManifest = JSON.parse(await readFile(resolve(root, 'package.json'), 'utf8'))
const publishWorkflow = await readFile(resolve(root, '.github/workflows/publish.yml'), 'utf8')
const wrapper = await readFile(resolve(root, 'native/hooks/taskbar.wh.cpp'), 'utf8')
const vendor = await readFile(
  resolve(root, 'native/hooks/vendor/windows-11-taskbar-styler-v1.7.inc'),
  'utf8',
)
const notices = await readFile(resolve(root, 'THIRD_PARTY_NOTICES.md'), 'utf8')

function invariant(condition, message) {
  if (!condition) throw new Error(message)
}

invariant(
  packageManifest.version === manifest.version,
  'package version must match manifest version',
)

const reusableWorkflowPins = [
  ...publishWorkflow.matchAll(
    /uses:\s*MyWallpapers\/native-addon-toolchain\/\.github\/workflows\/native-addon-build\.yml@([^\s#]+)/g,
  ),
]
invariant(reusableWorkflowPins.length === 1, 'publish workflow must reference the toolchain once')
invariant(
  /^[0-9a-f]{40}$/.test(reusableWorkflowPins[0][1]),
  'publish workflow must pin the toolchain to an exact 40-character commit SHA',
)

const settings = new Map(manifest.settings.map((setting) => [setting.id, setting]))
const theme = settings.get('theme')
invariant(theme?.type === 'select', 'theme must remain a select setting')
invariant(theme.default === 'mywallpaper-quick', 'quick appearance must remain the compatible default')
invariant(theme.options.length === 40, 'theme must expose quick mode, upstream None and all 38 v1.7 themes')

const themeValues = theme.options.map((option) => option.value)
invariant(new Set(themeValues).size === themeValues.length, 'theme values must be unique')
invariant(themeValues.includes(''), 'the upstream None theme must remain available')

for (const value of themeValues) {
  if (value === '' || value === 'mywallpaper-quick') continue
  invariant(
    vendor.includes(`wcscmp(themeName, L"${value}")`),
    `manifest theme ${value} is not mapped by the pinned engine`,
  )
}

for (const id of ['enabled', 'mode', 'accentColor', 'opacity']) {
  invariant(settings.has(id), `compatible setting ${id} must not be removed`)
}
for (const id of [
  'styleConstants',
  'controlStyles',
  'themeResourceVariables',
  'xamlDiagnosticsHandling',
]) {
  invariant(settings.get(id)?.scope === 'device', `${id} must use the shared device scope`)
}

invariant(wrapper.includes('Rectangle#BackgroundFill'), 'quick mode must target the XAML background fill')
invariant(wrapper.includes('Rectangle#BackgroundStroke'), 'quick mode must remove the XAML background stroke')
invariant(!wrapper.includes('WCA_ACCENT_POLICY'), 'the shallow window-composition implementation must not return')
invariant(
  wrapper.includes('#define MYWALLPAPER_DISABLE_UPSTREAM_STATISTICS 1'),
  'the wrapper must compile out upstream statistics',
)
invariant(
  vendor.includes('#if !defined(MYWALLPAPER_DISABLE_UPSTREAM_STATISTICS)'),
  'the vendored statistics implementation must remain behind the compile-time guard',
)

const vendorDigest = createHash('sha256').update(vendor).digest('hex')
invariant(
  vendorDigest === 'ac822e3cbc863ba6a45fc2e6cbed9ecac60fb06a76bfd6824d81909ee97dc16c',
  'the pinned Styler fork changed without updating its provenance test',
)
invariant(
  wrapper.includes('g_pendingProjection') &&
    wrapper.includes('g_appliedProjection') &&
    wrapper.includes('PromotePendingProjection'),
  'settings updates must keep pending and applied generations separate',
)
invariant(
  wrapper.indexOf('if (!result.enabled)') < wrapper.indexOf('unknown taskbar theme'),
  'disabling must not depend on validating advanced appearance settings',
)
invariant(
  vendor.includes('std::atomic_bool g_inInjectWindhawkTAP') &&
    vendor.includes('std::atomic<XamlDiagnosticsHandling>') &&
    vendor.includes('std::atomic<HRESULT> g_mywallpaperLastTapResult'),
  'cross-thread XAML diagnostics state must remain atomic',
)
invariant(
  wrapper.includes('ReloadResult::Recovered') &&
    wrapper.includes('g_settingsInvalid') &&
    wrapper.includes('g_pendingProjection.reset()') &&
    !wrapper.includes('SettingsAreInvalid()'),
  'recovering settings must keep one explicit pending/applied projection state',
)
invariant(
  wrapper.includes('Wh_GetIntSetting(L"enabled")') &&
    wrapper.includes('Wh_GetStringSetting') &&
    wrapper.includes('Wh_FreeStringSetting'),
  'projection settings must use the official Windhawk settings API',
)
invariant(
  !wrapper.includes('mywallpaper::') &&
    !wrapper.includes('mywallpaper_settings') &&
    !wrapper.includes('mywallpaper_windhawk') &&
    !wrapper.includes('windows-shell-v1') &&
    !wrapper.includes('allowed_hooks') &&
    !wrapper.includes('NativeAllowedHook'),
  'the adapter must not recreate MyWallpaper hook restrictions or protocols',
)
invariant(
  !wrapper.includes('emit_event') &&
    !wrapper.includes('EmitCurrentState') &&
    !wrapper.includes('g_eventEmissionMutex') &&
    !wrapper.includes('g_upstreamEventsEnabled'),
  'the adapter must not maintain a parallel MyWallpaper runtime event channel',
)
invariant(
  vendor.includes('g_mywallpaperUserSettingsHadError') &&
    vendor.includes('MyWallpaperTaskbarStylerNotifyUserSettingsError()') &&
    wrapper.includes('Wh_Log(L"Taskbar Accent settings could not be applied")'),
  'native setting errors must remain visible through Windhawk diagnostics',
)
invariant(
  vendor.indexOf('AddRef();\n    m_adviseThread = CreateThread') >= 0 &&
    vendor.includes('WaitForSingleObject(m_adviseThread, INFINITE)'),
  'the detached XAML advise thread must own a reference and be joined on unload',
)
invariant(
  vendor.includes('m_XamlDiagnostics.try_as<IVisualTreeService3>()') &&
    vendor.includes('hr = E_UNEXPECTED;') &&
    vendor.indexOf('watcher->Release();') >
      vendor.indexOf('MyWallpaperTaskbarStylerNotifyWatcherResult(hr);'),
  'the XAML advise thread must convert interface failures and always release its callback reference',
)
invariant(
  vendor.includes('bool userProvided') &&
    vendor.includes('rule.isXamlValue, userProvided') &&
    vendor.includes('else if (it->userProvided)') &&
    vendor.includes('MyWallpaperTaskbarStylerNotifyUserSettingsError()'),
  'late user-authored XAML resolution failures must reach the Canvas status',
)
invariant(
  vendor.includes('if (!MyWallpaperTaskbarStylerProjectionEnabled())') &&
    vendor.includes('A disabled MyWallpaper projection must be transparent'),
  'disabled mode must not intercept other XAML diagnostics consumers',
)
invariant(
  !vendor.includes('windows-11-taskbar-styling-guide/refs/heads/main/'),
  'runtime theme assets must not follow a mutable branch',
)
invariant(
  notices.includes('cf0c6b1d2269380846d0da868898d35fc8678c06') &&
    notices.includes('2de27dbfa5fa8ff4f7d73e9bf6cf33d9d61bdf94d25384df15ccb1a22401327f'),
  'third-party notices must retain the exact upstream commit and source digest',
)
