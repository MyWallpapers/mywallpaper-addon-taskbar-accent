interface MyWallpaperLayerApi {
  root: HTMLElement
  setPointerEvents?: (value: 'none') => void
}

declare global {
  interface Window {
    MyWallpaper?: {
      layer?: MyWallpaperLayerApi
    }
  }
}

const layer = window.MyWallpaper?.layer
const root = layer?.root ?? document.body

root.style.width = '100%'
root.style.height = '100%'
root.style.margin = '0'
root.style.overflow = 'hidden'
root.style.pointerEvents = 'none'
root.style.background = 'transparent'
root.replaceChildren()
layer?.setPointerEvents?.('none')

document.documentElement.style.width = '100%'
document.documentElement.style.height = '100%'
document.documentElement.style.margin = '0'
document.documentElement.style.background = 'transparent'
document.body.style.width = '100%'
document.body.style.height = '100%'
document.body.style.margin = '0'
document.body.style.overflow = 'hidden'
document.body.style.background = 'transparent'
