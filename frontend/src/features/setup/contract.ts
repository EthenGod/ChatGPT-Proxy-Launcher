import type { SetupOptions, SetupSnapshot } from '../../platform/types'
import type { MessageKey } from '../../i18n'

export type CommitState = 'not-started' | 'saved' | 'rolled-back' | 'rollback-incomplete'
export interface SaveResult {
  commit: CommitState
  message: MessageKey
  detail?: string
  nativeError?: number
  tasks: Array<'startup' | 'restart'>
  warnings?: MessageKey[]
  options?: SetupOptions | null
  conflictDirectory?: string
}
export interface ProgramEntry { name: string; path: string; arguments: string }
export interface Validation {
  valid: boolean; arguments: string; errors: Partial<Record<keyof SetupOptions, MessageKey>>; detail?: string; nativeError?: number
}
export interface Detection { port: string | null; source: string; message: string }
export interface SetupService {
  native?: boolean
  settled?(): Promise<unknown>
  picker?: { directories: string[]; programs: ProgramEntry[] }
  validate?(options: SetupOptions): Promise<Validation>
  chooseDirectory?(initial: string): Promise<string | null>
  selectProgram?(): Promise<ProgramEntry | null>
  setPreferences?(dark: boolean, language: string): Promise<{ preferences: SetupSnapshot['preferences']; warnings: MessageKey[] }>
  load(): Promise<SetupSnapshot>
  detectPort(protocol: string, host: string): Promise<Detection>
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
