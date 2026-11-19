import { invoke, isTauri } from '@tauri-apps/api/core'
import { listen } from '@tauri-apps/api/event'
import { getCurrentWebviewWindow } from '@tauri-apps/api/webviewWindow'
import type { WindowHost } from '../features/setup/contract'

let painting = Promise.resolve()

export const desktopWindow: WindowHost = {
  connect: async onClose => isTauri() ? listen('setup-close-requested', onClose) : () => {},
  ready: async () => { if (isTauri()) await invoke<void>('show_setup_window') },
  close: async () => { if (isTauri()) await invoke<void>('close_setup_window') },
  setTheme: async dark => {
    if (!isTauri()) return
    const apply = async () => {
      const window = getCurrentWebviewWindow()
      // Both native surfaces can be exposed during resize. Finish both before the next theme.
      const results = await Promise.allSettled([
        window.setTheme(dark ? 'dark' : 'light'),
        window.setBackgroundColor(dark ? '#111111' : '#fcfcfc'),
      ])
      for (const result of results) if (result.status === 'rejected') throw result.reason
    }
    const job = painting.then(apply)
    painting = job.catch(() => {})
    await job
  },
}
