import type { MessageKey } from '../../i18n'

export type ConflictLocation = 'conflict' | 'program-conflict'
export interface ErrorInfo { nativeError?: number; detail?: string; conflictDirectory?: string; conflictLocation?: ConflictLocation }
export function nativeIssue(value: unknown, conflictLocation?: ConflictLocation): ErrorInfo {
  const record = value && typeof value === 'object' ? value as Record<string, unknown> : {}
  const code = record.nativeError
  return {
    nativeError: typeof code === 'number' && Number.isInteger(code) && code > 0 && code <= 0xffffffff ? code : undefined,
    detail: typeof record.message === 'string' ? record.message : typeof value === 'string' ? value : '',
    conflictDirectory: typeof record.conflictDirectory === 'string' ? record.conflictDirectory : undefined,
    conflictLocation,
  }
}

const categories: Record<number, string> = {
  2: 'missing', 3: 'missing', 15: 'missing', 5: 'access', 8: 'memory', 14: 'memory',
  13: 'data', 23: 'io', 29: 'io', 30: 'io', 31: 'io', 32: 'locked', 33: 'locked',
  39: 'space', 112: 'space', 80: 'exists', 183: 'exists', 87: 'input',
  123: 'path', 161: 'path', 206: 'path', 170: 'busy', 1117: 'io',
}
export function errorPresentation(error: ErrorInfo) {
  const code = error.nativeError && Number.isInteger(error.nativeError) && error.nativeError > 0 && error.nativeError <= 0xffffffff
    ? error.nativeError : undefined
  // Only HRESULT_FROM_WIN32 has a Win32 meaning in its low word. Never infer HTTP codes.
  const win32 = code !== undefined && (code >>> 16) === 0x8007 ? code & 0xffff : code
  const category = win32 !== undefined ? categories[win32] ?? 'unknown' : 'unknown'
  return {
    code: code === undefined ? '' : code > 0xffff ? `0x${code.toString(16).toUpperCase().padStart(8, '0')}` : String(code),
    meaning: `ui.error.${category}.meaning` as MessageKey,
    explanation: `ui.error.${category}.explanation` as MessageKey,
    action: `ui.error.${category}.action` as MessageKey,
  }
}
