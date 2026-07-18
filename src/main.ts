import './styles.css'
import type { CanvasAddonMountContext, NativeHookStatus } from '../generated/mywallpaper-runtime'

export function mount({ layer, runtime }: CanvasAddonMountContext): () => void {
  const statusTopic = 'mywallpaper.taskbar-accent/v1/status'
  const root = layer.root
  root.classList.add('taskbar-accent-root')
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
      renderStatus(
        'starting',
        'Native hook attached · waiting for confirmation from the Taskbar Styler XAML engine',
      )
      return
    }
    renderStatus(status.state, `${status.cause} ${status.action}`)
  })

  const stopEvents = layer.native.hooks.onEvent((event) => {
    if (event.hookId !== 'taskbar' || !event.topic.startsWith('mywallpaper.taskbar-accent/v1/')) return
    if (event.topic === 'mywallpaper.taskbar-accent/v1/applied') {
      renderStatus('active', 'Live · shared across every Windows taskbar')
      return
    }
    if (event.topic === 'mywallpaper.taskbar-accent/v1/disabled') {
      renderStatus('disabled', 'Disabled in Taskbar Accent settings')
      return
    }
    const payload = isRecord(event.payload) ? event.payload : {}
    const cause = payload['cause']
    const action = payload['action']
    if (typeof cause !== 'string' || typeof action !== 'string') return
    renderStatus('degraded', `${cause} ${action}`)
  })

  function renderStatus(state: NativeHookStatus['state'] | 'starting', detail: string): void {
    statusCard.dataset['state'] = state
    statusDetail.textContent = detail
    statusCard.title = detail
    if (runtime.instance.canonical) {
      layer.bus.emit(statusTopic, { state, detail })
    }
  }

  function requireElement<TElement extends Element>(selector: string): TElement {
    const element = root.querySelector<TElement>(selector)
    if (!element) throw new Error(`Taskbar Accent UI is missing ${selector}`)
    return element
  }

  function isRecord(value: unknown): value is Record<string, unknown> {
    return typeof value === 'object' && value !== null && !Array.isArray(value)
  }

  return () => {
    stopState()
    stopEvents()
  }
}
