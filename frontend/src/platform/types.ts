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
}

export function nativeRecord(value: unknown): Record<string, unknown> {
  if (!value || typeof value !== 'object' || Array.isArray(value)) throw new Error('Invalid native response')
  return value as Record<string, unknown>
}

export function assertOptions(value: unknown): asserts value is SetupOptions {
  const options = nativeRecord(value)
  for (const key of ['startup', 'silent', 'desktop', 'proxy', 'codexEnv']) {
    if (typeof options[key] !== 'boolean') throw new Error('Invalid native option')
  }
  for (const key of ['protocol', 'host', 'port', 'bypass', 'extra', 'directory']) {
    if (typeof options[key] !== 'string') throw new Error('Invalid native option')
  }
}

/** A malformed/older native response must never become a partially valid form. */
export function parseSnapshot(value: unknown): SetupSnapshot {
  const state = nativeRecord(value)
  const preferences = nativeRecord(state.preferences)
  if (state.schemaVersion !== 1 || !Number.isSafeInteger(state.payloadBytes) || Number(state.payloadBytes) <= 0) {
    throw new Error('Unsupported native response')
  }
  for (const key of ['setupVersion', 'deployedVersion', 'notice', 'defaultDirectory']) {
    if (typeof state[key] !== 'string') throw new Error('Invalid native text')
  }
  for (const key of ['deployed', 'hasSettings', 'validArguments']) {
    if (typeof state[key] !== 'boolean') throw new Error('Invalid native state')
  }
  assertOptions(state.options)
  if (typeof preferences.dark !== 'boolean' || typeof preferences.language !== 'string'
    || typeof preferences.resolvedLanguage !== 'string') throw new Error('Invalid native preferences')
  return value as SetupSnapshot
}
