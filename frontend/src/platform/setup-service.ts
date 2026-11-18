import { invoke } from '@tauri-apps/api/core'
import { parseSnapshot } from './types'
import type { SetupService, SaveResult, Validation, ProgramEntry, ListedProgram, ProgramList, Detection } from '../features/setup/contract'
import type { MessageKey } from '../i18n'
import catalog from '../../../locales/zh-CN.json'

const record = (value: unknown): Record<string, unknown> => {
  if (!value || typeof value !== 'object' || Array.isArray(value)) throw new Error('Invalid native response')
  return value as Record<string, unknown>
}
const messageKey = (value: unknown): value is MessageKey => typeof value === 'string' && Object.hasOwn(catalog.strings, value)
export function parseProgramList(value: unknown): ProgramList {
  const list = record(value)
  if (typeof list.incomplete !== 'boolean' || !Array.isArray(list.entries)) throw new Error('Invalid program list')
  const ids = new Set<string>()
  // Only native, bounded PNGs reach an image element. A bad optional icon leaves the row usable.
  const icons = Array.isArray(list.icons) ? list.icons.map(data =>
    typeof data === 'string' && data.length <= 21848 && /^iVBORw0KGgo[A-Za-z0-9+/]*={0,2}$/.test(data)
      ? 'data:image/png;base64,' + data : undefined) : []
  const entries = list.entries.map((item): ListedProgram => {
    const row = record(item)
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
  const result = record(value)
  if (!['not-started', 'saved', 'rolled-back', 'rollback-incomplete'].includes(String(result.commit))
    || !messageKey(result.message) || typeof result.detail !== 'string'
    || !Array.isArray(result.tasks) || result.tasks.some(t => t !== 'startup' && t !== 'restart')
    || !Array.isArray(result.warnings) || result.warnings.some(k => !messageKey(k))
    || typeof result.conflictDirectory !== 'string'
    || !Number.isInteger(result.nativeError) || Number(result.nativeError) < 0 || Number(result.nativeError) > 0xffffffff) throw new Error('Invalid save result')
  // Rust already validates the full Options object. Do not accept malformed data in JS either.
  if (result.options !== null) {
    const options = record(result.options)
    for (const key of ['startup', 'silent', 'desktop', 'proxy', 'codexEnv']) if (typeof options[key] !== 'boolean') throw new Error('Invalid saved options')
    for (const key of ['protocol', 'host', 'port', 'bypass', 'extra', 'directory']) if (typeof options[key] !== 'string') throw new Error('Invalid saved options')
  }
  return value as SaveResult
}

/** One FIFO for all native work, including preferences. No polling or browser storage. */
export function createSetupService(): SetupService {
  let tail: Promise<unknown> = Promise.resolve()
  const call = <T,>(command: string, args?: Record<string, unknown>): Promise<T> => {
    const job = tail.then(() => invoke<T>(command, args))
    tail = job.catch(() => {})
    return job
  }
  return {
    native: true,
    settled: () => tail,
    load: async () => parseSnapshot(await call('read_setup_state')),
    validate: options => call<Validation>('validate_setup', { options }),
    detectPort: async (protocol, host) => {
      const result = await call<Detection>('detect_setup_port', { protocol, host })
      if (typeof result.port !== 'string' || typeof result.source !== 'string' || typeof result.message !== 'string') throw new Error('Invalid detection result')
      return { ...result, port: result.port || null }
    },
    save: async options => parseSaveResult(await call('save_setup', { options })),
    chooseDirectory: async initial => (await call<{ path: string }>('choose_setup_directory', { initial })).path || null,
    selectProgram: async () => (await call<{ entry: ProgramEntry | null }>('add_setup_program')).entry,
    listPrograms: async () => parseProgramList(await call('list_setup_programs')),
    // Production imports only the path selected by the native dialog.
    addProgram: async () => { throw new Error('Native selection required') },
    openLocation: location => call<void>('open_setup_location', { location }),
    setPreferences: (dark, language) => call('set_setup_preferences', { dark, language }),
  }
}

export const setupService = createSetupService()
export function nativeMessage(error: unknown): string {
  return error && typeof error === 'object' && 'message' in error && typeof error.message === 'string' ? error.message : ''
}
