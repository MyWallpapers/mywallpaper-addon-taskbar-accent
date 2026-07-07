type PointerEvents = 'auto' | 'none'

interface TaskbarAccentSettings {
  enabled: boolean
  mode: 'transparent' | 'blur' | 'accent'
  accentColor: string
  opacity: number
  showStatus: boolean
}

interface MyWallpaperLayerApi {
  root: HTMLElement
  settings: {
    get(): Partial<TaskbarAccentSettings>
    subscribe(listener: (settings: Partial<TaskbarAccentSettings>) => void): () => void
  }
  lifecycle?: {
    onDispose(listener: () => void): () => void
  }
  setPointerEvents?: (value: PointerEvents) => void
}

declare global {
  interface Window {
    MyWallpaper?: {
      layer?: MyWallpaperLayerApi
    }
  }
}

const DEFAULT_SETTINGS: TaskbarAccentSettings = {
  enabled: true,
  mode: 'blur',
  accentColor: '#1f8fff',
  opacity: 128,
  showStatus: true,
}

const layer = window.MyWallpaper?.layer
const root = layer?.root ?? document.body

root.classList.add('taskbar-accent-root')
root.style.width = '100%'
root.style.height = '100%'
root.style.margin = '0'
root.style.overflow = 'hidden'
root.style.pointerEvents = 'none'
layer?.setPointerEvents?.('none')

const style = document.createElement('style')
style.textContent = `
  html, body {
    width: 100%;
    height: 100%;
    margin: 0;
    overflow: hidden;
    background: transparent;
  }

  .taskbar-accent-status {
    box-sizing: border-box;
    width: 100%;
    height: 100%;
    display: grid;
    place-items: center;
    padding: 16px;
    color: white;
    font: 600 18px/1.3 Inter, ui-sans-serif, system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
    text-align: center;
    text-shadow: 0 2px 18px rgba(0, 0, 0, 0.55);
    background: radial-gradient(circle at 50% 50%, rgba(31, 143, 255, 0.30), transparent 62%);
  }

  .taskbar-accent-status small {
    display: block;
    margin-top: 6px;
    font-size: 12px;
    font-weight: 500;
    opacity: 0.75;
  }
`
document.head.append(style)

const status = document.createElement('div')
status.className = 'taskbar-accent-status'
root.append(status)

function normalize(settings: Partial<TaskbarAccentSettings>): TaskbarAccentSettings {
  return { ...DEFAULT_SETTINGS, ...settings }
}

function render(settings: TaskbarAccentSettings): void {
  status.hidden = !settings.showStatus
  status.innerHTML = `
    <div>
      Taskbar Accent
      <small>${settings.enabled ? `${settings.mode} ${settings.accentColor} / ${settings.opacity}` : 'disabled'}</small>
    </div>
  `
}

render(normalize(layer?.settings.get() ?? {}))

const unsubscribe = layer?.settings.subscribe((next) => render(normalize(next)))
layer?.lifecycle?.onDispose(() => {
  unsubscribe?.()
  style.remove()
  status.remove()
})

