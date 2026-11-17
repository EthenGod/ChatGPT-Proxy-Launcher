import type { SetupOptions } from '../../platform/types'
import type { MessageKey } from '../../i18n'

// Immediate field feedback; SetupArguments.cpp still validates every real save.
export function portErrorKey(port: string): MessageKey | undefined {
  return /^\d{1,5}$/.test(port) && +port >= 1 && +port <= 65535 ? undefined : 'ui.invalid.port'
}
// Prototype-only argument generation.
export function quoteArgument(value: string): string {
  if (value && !/[\s"]/.test(value)) return value
  return '"' + value.replace(/(\\*)"/g, '$1$1\\"').replace(/(\\+)$/, '$1$1') + '"'
}
export function previewOptions(options: SetupOptions) {
  const errors: Partial<Record<keyof SetupOptions, MessageKey>> = {}
  if (options.proxy) {
    if (!options.host || /[\s/\\"@]/.test(options.host)) errors.host = 'ui.invalid.host'
    const portError = portErrorKey(options.port)
    if (portError) errors.port = portError
    if (!['http', 'https', 'socks5'].includes(options.protocol)) errors.protocol = 'ui.invalid.protocol'
  }
  if (!/^[a-z]:\\.+/i.test(options.directory) || /[<>"|?*\r\n]/.test(options.directory)) errors.directory = 'ui.invalid.directory'
  if (options.extra.length > 16000 || /[\r\n\0]/.test(options.extra)) errors.extra = 'ui.invalid.extra'
  if (/[\r\n\0]/.test(options.bypass)) errors.bypass = 'ui.invalid.bypass'
  const parts: string[] = []
  if (options.proxy) parts.push(quoteArgument(`--proxy-server=${options.protocol}://${options.host}:${options.port}`))
  if (options.silent) parts.push('--no-startup-window')
  if (options.bypass) parts.push(quoteArgument(`--proxy-bypass-list=${options.bypass}`))
  if (options.extra) parts.push(options.extra)
  return { errors, valid: Object.keys(errors).length === 0, arguments: parts.join(' ') }
}
export function sameOptions(left: SetupOptions, right: SetupOptions) {
  return (Object.keys(left) as Array<keyof SetupOptions>).every(key => left[key] === right[key])
}
