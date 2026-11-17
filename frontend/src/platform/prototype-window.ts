import { isTauri } from '@tauri-apps/api/core'
import { listen } from '@tauri-apps/api/event'
import { getCurrentWindow } from '@tauri-apps/api/window'
import { desktop } from './desktop'
import type { WindowHost } from '../features/setup/contract'

// Shared window lifecycle for operational and prototype pages; no configuration I/O.
export const prototypeWindow: WindowHost = {
  connect: async onClose => isTauri() ? listen('setup-close-requested', onClose) : () => {},
  ready: async () => { if (isTauri()) await desktop.ready() },
  close: async () => { if (isTauri()) await desktop.close() },
  setTheme: async dark => { if (isTauri()) await getCurrentWindow().setTheme(dark ? 'dark' : 'light') },
}
