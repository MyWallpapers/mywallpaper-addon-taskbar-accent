import './styles.css'

type HookStatus = { hookId: string } & (
  | { state: 'active' }
  | {
      state: 'disabled' | 'starting' | 'degraded' | 'quarantined' | 'incompatible' | 'conflict'
      cause: string
      action: string
    }
)

interface HookEvent {
  hookId: string
  topic: string
  payload: unknown
}

interface MyWallpaperLayerApi {
  root: HTMLElement
  setPointerEvents(value: 'none'): void
  lifecycle: { onDispose(listener: () => void): () => void }
  native: {
    hooks: {
      readonly available: boolean
      status(hookId: string): HookStatus | null
      onStateChange(listener: (statuses: readonly HookStatus[]) => void): () => void
      onEvent(listener: (event: HookEvent) => void): () => void
    }
  }
}

declare global {
  interface Window {
    MyWallpaper: { layer: MyWallpaperLayerApi }
  }
}

const layer = window.MyWallpaper.layer
const root = layer.root
layer.setPointerEvents('none')
root.className = 'taskbar-accent-root'
root.innerHTML = `
  <div class="status" data-state="starting" role="status" aria-live="polite">
    <span class="dot"></span>
    <div><strong>Taskbar Accent</strong><small>Starting native integration…</small></div>
  </div>
`

const statusCard = requireElement<HTMLElement>('.status')
const statusDetail = requireElement<HTMLElement>('small')

const stopState = layer.native.hooks.onStateChange((statuses) => {
  const status = statuses.find((candidate) => candidate.hookId === 'taskbar')
  if (!status) {
    renderStatus('disabled', 'Native hook is not attached. Open Settings → Add-ons for details.')
    return
  }
  if (status.state === 'active') {
    renderStatus('active', 'Live · shared across every Taskbar Accent layer')
    return
  }
  renderStatus(status.state, `${status.cause} ${status.action}`)
})

const stopEvents = layer.native.hooks.onEvent((event) => {
  if (event.hookId !== 'taskbar' || event.topic !== 'taskbar.restart-required') return
  const payload = isRecord(event.payload) ? event.payload : {}
  const cause = typeof payload['cause'] === 'string' ? payload['cause'] : 'Explorer must restart before the accent can activate.'
  const action = typeof payload['action'] === 'string' ? payload['action'] : 'Open Settings → Add-ons and retry.'
  renderStatus('degraded', `${cause} ${action}`)
})

layer.lifecycle.onDispose(() => {
  stopState()
  stopEvents()
})

function renderStatus(state: HookStatus['state'], detail: string): void {
  statusCard.dataset['state'] = state
  statusDetail.textContent = detail
  statusCard.title = detail
}

function requireElement<TElement extends Element>(selector: string): TElement {
  const element = root.querySelector<TElement>(selector)
  if (!element) throw new Error(`Taskbar Accent UI is missing ${selector}`)
  return element
}

function isRecord(value: unknown): value is Record<string, unknown> {
  return typeof value === 'object' && value !== null && !Array.isArray(value)
}
