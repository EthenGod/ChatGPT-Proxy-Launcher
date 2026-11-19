import { invoke } from '@tauri-apps/api/core'
import { assertOptions, nativeRecord, parseSnapshot } from './types'
import type { DesktopSetupService, SaveResult, Validation, ProgramEntry, ListedProgram, ProgramList, Detection } from '../features/setup/contract'
import type { MessageKey } from '../i18n'
import catalog from '../../../locales/zh-CN.json'

const messageKey = (value: unknown): value is MessageKey => typeof value === 'string' && Object.hasOwn(catalog.strings, value)
export function parseProgramList(value: unknown): ProgramList {
  const list = nativeRecord(value)
  if (typeof list.incomplete !== 'boolean' || !Array.isArray(list.entries)) throw new Error('Invalid program list')
  const ids = new Set<string>()
  // Only native, bounded PNGs reach an image element. A bad optional icon leaves the row usable.
  const icons = Array.isArray(list.icons) ? list.icons.map(data =>
    typeof data === 'string' && data.length <= 21848 && /^iVBORw0KGgo[A-Za-z0-9+/]*={0,2}$/.test(data)
      ? 'data:image/png;base64,' + data : undefined) : []
  const entries = list.entries.map((item): ListedProgram => {
    const row = nativeRecord(item)
    if (typeof row.id !== 'string' || !row.id || ids.has(row.id) || typeof row.name !== 'string' || !row.name
      || typeof row.path !== 'string' || typeof row.arguments !== 'string'
      || typeof row.status !== 'string' || !['ordinary', 'adapted', 'adapted-disabled', 'disabled', 'invalid', 'unavailable'].includes(row.status)) throw new Error('Invalid program entry')
    ids.add(row.id)
    const icon = typeof row.icon === 'number' && Number.isInteger(row.icon) ? icons[row.icon] : undefined
    return { id: row.id, name: row.name, path: row.path, arguments: row.arguments, status: row.status as ListedProgram['status'], ...(icon && { icon }) }
  })
  return { entries, incomplete: list.incomplete }
}
export function parseSaveResult(value: unknown): SaveResult {
  const result = nativeRecord(value)
  if (typeof result.commit !== 'string' || !['not-started', 'saved', 'rolled-back', 'rollback-incomplete'].includes(result.commit)
    || !messageKey(result.message) || typeof result.detail !== 'string'
    || !Array.isArray(result.tasks) || result.tasks.some(t => t !== 'startup' && t !== 'restart')
    || !Array.isArray(result.warnings) || result.warnings.some(k => !messageKey(k))
    || typeof result.conflictDirectory !== 'string'
    || !Number.isInteger(result.nativeError) || Number(result.nativeError) < 0 || Number(result.nativeError) > 0xffffffff) throw new Error('Invalid save result')
  if (result.options !== null) assertOptions(result.options)
  return value as SaveResult
}

/** One FIFO for all native work, including preferences. No polling or browser storage. */
export function createSetupService(): DesktopSetupService {
  let tail = Promise.resolve()
  const call = <T,>(command: string, args?: Record<string, unknown>, signal?: AbortSignal): Promise<T> => {
    const job = tail.then(() => {
      if (signal?.aborted) throw signal.reason
      return invoke<T>(command, args)
    })
    // Closing waits for completion. Keeping the reply here also retains the last list of PNGs.
    tail = job.then(() => {}, () => {})
    return job
  }
  return {
    native: true,
    settled: () => tail,
    load: async () => parseSnapshot(await call('read_setup_state')),
    validate: (options, signal) => call<Validation>('validate_setup', { options }, signal),
    detectPort: async (protocol, host) => {
      const result = await call<Detection>('detect_setup_port', { protocol, host })
      if (typeof result.port !== 'string' || typeof result.source !== 'string' || typeof result.message !== 'string') throw new Error('Invalid detection result')
      return { ...result, port: result.port || null }
    },
    save: async options => parseSaveResult(await call('save_setup', { options })),
    chooseDirectory: async initial => (await call<{ path: string }>('choose_setup_directory', { initial })).path || null,
    selectProgram: async () => (await call<{ entry: ProgramEntry | null }>('add_setup_program')).entry,
    listPrograms: async () => parseProgramList(await call('list_setup_programs')),
    openLocation: location => call<void>('open_setup_location', { location }),
    setPreferences: (dark, language) => call('set_setup_preferences', { dark, language }),
  }
}

export const setupService = createSetupService()
