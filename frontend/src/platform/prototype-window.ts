import { isTauri } from '@tauri-apps/api/core'
import { listen } from '@tauri-apps/api/event'
import { desktop } from './desktop'
import type { WindowHost } from '../features/setup/contract'

// Prototype interaction never calls native configuration commands.
export const prototypeWindow: WindowHost = {
  connect: async onClose => isTauri() ? listen('setup-close-requested', onClose) : () => {},
  ready: async () => { if (isTauri()) await desktop.ready() },
  close: async () => { if (isTauri()) await desktop.close() },
}
