import zh from '../../../locales/zh-CN.json'
import en from '../../../locales/en-US.json'
import ja from '../../../locales/ja-JP.json'

type Key = keyof typeof zh.strings
const catalogs: Record<string, { strings: Partial<Record<Key, string>> }> = {
  'zh-CN': zh, 'en-US': en, 'ja-JP': ja,
}

export function translator(language: string) {
  const strings = catalogs[language]?.strings ?? zh.strings
  return (key: Key, values: Record<string, string> = {}): string =>
    (strings[key] ?? zh.strings[key]).replace(/\{([^{}]+)\}/g, (match, name: string) => values[name] ?? match)
}
