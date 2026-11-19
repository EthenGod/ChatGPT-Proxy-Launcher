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
export type ProgramStatus = 'ordinary' | 'adapted' | 'adapted-disabled' | 'disabled' | 'invalid' | 'unavailable'
export interface ListedProgram extends ProgramEntry { id: string; status: ProgramStatus; icon?: string }
export interface ProgramList { entries: ListedProgram[]; incomplete: boolean }
export interface Validation {
  valid: boolean; arguments: string; errors: Partial<Record<keyof SetupOptions, MessageKey>>; detail?: string; nativeError?: number
}
export interface Detection { port: string | null; source: string; message: string }
interface SetupActions {
  native?: boolean
  settled?(): Promise<void>
  picker?: {
    directories: string[]
    programs: ProgramEntry[]
    addProgram(entry: ProgramEntry): Promise<void>
  }
  chooseDirectory?(initial: string): Promise<string | null>
  selectProgram?(): Promise<ProgramEntry | null>
  setPreferences?(dark: boolean, language: string): Promise<{ preferences: SetupSnapshot['preferences']; warnings: MessageKey[] }>
  load(): Promise<SetupSnapshot>
  detectPort(protocol: string, host: string): Promise<Detection>
  save(options: SetupOptions): Promise<SaveResult>
  listPrograms(): Promise<ProgramList>
  openLocation(location: 'startup' | 'deployment' | 'windows' | 'conflict'): Promise<void>
}
export interface DesktopSetupService extends SetupActions {
  validate(options: SetupOptions, signal?: AbortSignal): Promise<Validation>
}
export interface PreviewSetupService extends SetupActions {
  preview(options: SetupOptions): Validation
}
export type SetupService = DesktopSetupService | PreviewSetupService

export interface WindowHost {
  connect(onClose: () => void): Promise<() => void>
  ready(): Promise<void>
  close(): Promise<void>
  setTheme(dark: boolean): Promise<void>
}
