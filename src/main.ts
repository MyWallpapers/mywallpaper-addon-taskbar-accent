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
        'active',
        'Live · shared across every Windows taskbar',
      )
      return
    }
    renderStatus(status.state, `${status.cause} ${status.action}`)
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

  return () => {
    stopState()
  }
}
