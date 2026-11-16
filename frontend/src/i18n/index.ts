import zh from '../../../locales/zh-CN.json'
import en from '../../../locales/en-US.json'
import ja from '../../../locales/ja-JP.json'

export type MessageKey = keyof typeof zh.strings
type Key = MessageKey
const catalogs: Record<string, { strings: Partial<Record<Key, string>> }> = {
  'zh-CN': zh, 'en-US': en, 'ja-JP': ja,
}

export function resolveLanguage(preference: string, system = navigator.language): string {
  if (preference !== 'auto') return preference
  if (system.toLowerCase().startsWith('ja')) return 'ja-JP'
  if (system.toLowerCase().startsWith('en')) return 'en-US'
  return 'zh-CN'
}

export function translator(language: string) {
  const strings = catalogs[language]?.strings ?? zh.strings
  return (key: Key, values: Record<string, string> = {}): string =>
    (strings[key] ?? zh.strings[key]).replace(/\{([^{}]+)\}/g, (match, name: string) => values[name] ?? match)
}
