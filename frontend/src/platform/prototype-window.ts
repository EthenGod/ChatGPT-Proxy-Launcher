import { isTauri } from '@tauri-apps/api/core'
import { listen } from '@tauri-apps/api/event'
import { getCurrentWebviewWindow } from '@tauri-apps/api/webviewWindow'
import { desktop } from './desktop'
import type { WindowHost } from '../features/setup/contract'

let painting = Promise.resolve()

// Shared window lifecycle for operational and prototype pages; no configuration I/O.
export const prototypeWindow: WindowHost = {
  connect: async onClose => isTauri() ? listen('setup-close-requested', onClose) : () => {},
  ready: async () => { if (isTauri()) await desktop.ready() },
  close: async () => { if (isTauri()) await desktop.close() },
  setTheme: async dark => {
    if (!isTauri()) return
    // The compositor can expose the native backing surfaces at the edge of a live resize.
    // Keep both surfaces in sync with the page's Radix gray-1, even across rapid theme changes.
    const apply = async () => {
      const current = getCurrentWebviewWindow()
      await Promise.all([current.setTheme(dark ? 'dark' : 'light'), current.setBackgroundColor(dark ? '#111111' : '#fcfcfc')])
    }
    painting = painting.then(apply, apply)
    await painting
  },
}
