import { invoke } from '@tauri-apps/api/core'

// Native window commands shared by the operational and prototype pages.
export const desktop = {
  ready: () => invoke<void>('show_setup_window'),
  close: () => invoke<void>('close_setup_window'),
}
