import type { SetupOptions } from '../../platform/types'
import type { MessageKey } from '../../i18n'

// Immediate field feedback; SetupArguments.cpp still validates every real save.
export function portErrorKey(port: string): MessageKey | undefined {
  return /^\d{1,5}$/.test(port) && +port >= 1 && +port <= 65535 ? undefined : 'ui.invalid.port'
}
export function supportsCloudCodex(options: SetupOptions) {
  return options.proxy && (options.protocol === 'http' || options.protocol === 'https')
    && !/\P{White_Space}/u.test(options.bypass) && !/\P{ASCII}/u.test(options.host)
}
export function sameOptions(left: SetupOptions, right: SetupOptions) {
  return (Object.keys(left) as Array<keyof SetupOptions>).every(key => left[key] === right[key])
}
