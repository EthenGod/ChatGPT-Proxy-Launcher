import type { SetupOptions, SetupSnapshot } from '../../platform/types'
import type { MessageKey } from '../../i18n'

export type CommitState = 'not-started' | 'saved' | 'rolled-back' | 'rollback-incomplete'
export interface SaveResult {
  commit: CommitState
  message: MessageKey
  detail?: string
  tasks: Array<'startup' | 'restart'>
}
export interface ProgramEntry { name: string; path: string; arguments: string }
export interface SetupService {
  load(): Promise<SetupSnapshot>
  detectPort(protocol: string, host: string): Promise<string | null>
  save(options: SetupOptions): Promise<SaveResult>
  addProgram(entry: ProgramEntry): Promise<void>
  openLocation(location: 'startup' | 'deployment' | 'windows'): Promise<void>
}
export interface WindowHost {
  connect(onClose: () => void): Promise<() => void>
  ready(): Promise<void>
  close(): Promise<void>
  setTheme(dark: boolean): Promise<void>
}
