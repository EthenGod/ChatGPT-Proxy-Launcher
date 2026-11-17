export interface SetupOptions {
  startup: boolean
  silent: boolean
  desktop: boolean
  proxy: boolean
  codexEnv: boolean
  protocol: string
  host: string
  port: string
  bypass: string
  extra: string
  directory: string
}

export interface SetupSnapshot {
  schemaVersion: 1
  setupVersion: string
  deployedVersion: string
  deployed: boolean
  hasSettings: boolean
  validArguments: boolean
  notice: string
  options: SetupOptions
  preferences: { dark: boolean; language: string; resolvedLanguage: string }
  payloadBytes: number
  defaultDirectory: string
  programs: Array<{name: string; path: string; arguments: string}>
}

export interface BridgeError {
  nativeError: number
  message: string
  commit: 'not-started' | 'saved' | 'rolled-back' | 'rollback-incomplete' | 'unknown'
}

/** A malformed/older native response must never become a partially valid form. */
export function parseSnapshot(value: unknown): SetupSnapshot {
  const object = (v: unknown): Record<string, unknown> => {
    if (!v || typeof v !== 'object' || Array.isArray(v)) throw new Error('Invalid native response')
    return v as Record<string, unknown>
  }
  const state = object(value)
  const options = object(state.options)
  const preferences = object(state.preferences)
  if (state.schemaVersion !== 1 || !Number.isSafeInteger(state.payloadBytes) || Number(state.payloadBytes) <= 0) {
    throw new Error('Unsupported native response')
  }
  for (const key of ['setupVersion', 'deployedVersion', 'notice', 'defaultDirectory']) {
    if (typeof state[key] !== 'string') throw new Error('Invalid native text')
  }
  for (const key of ['deployed', 'hasSettings', 'validArguments']) {
    if (typeof state[key] !== 'boolean') throw new Error('Invalid native state')
  }
  for (const key of ['startup', 'silent', 'desktop', 'proxy', 'codexEnv']) {
    if (typeof options[key] !== 'boolean') throw new Error('Invalid native option')
  }
  for (const key of ['protocol', 'host', 'port', 'bypass', 'extra', 'directory']) {
    if (typeof options[key] !== 'string') throw new Error('Invalid native option')
  }
  if (typeof preferences.dark !== 'boolean' || typeof preferences.language !== 'string'
    || typeof preferences.resolvedLanguage !== 'string') throw new Error('Invalid native preferences')
  if (!Array.isArray(state.programs) || state.programs.some(p => !p || typeof p !== 'object'
    || ['name', 'path', 'arguments'].some(k => typeof p[k] !== 'string'))) throw new Error('Invalid program list')
  return value as SetupSnapshot
}
