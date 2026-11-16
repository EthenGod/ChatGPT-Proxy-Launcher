import { invoke } from '@tauri-apps/api/core'
import { parseSnapshot } from './types'

// Components use this adapter; no component accesses files, registry or invoke.
export const desktop = {
  load: async () => parseSnapshot(await invoke<unknown>('read_setup_state')),
  ready: () => invoke<void>('show_setup_window'),
  close: () => invoke<void>('close_setup_window'),
}
