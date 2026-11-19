import zh from '../../../locales/zh-CN.json'
import registeredCatalogs from 'virtual:setup-locales'

export type MessageKey = keyof typeof zh.strings
type Key = MessageKey
const catalogs = new Map(registeredCatalogs.map(catalog => [catalog.locale, catalog]))
export const languageItems = Array.from(catalogs, ([value, catalog]) => ({ value, label: catalog.nativeName }))

export function resolveLanguage(preference: string, system = navigator.language): string {
  const tag = (preference === 'auto' ? system : preference).toLowerCase()
  const exact = languageItems.find(item => item.value.toLowerCase() === tag)
  if (exact) return exact.value
  const primary = tag.split('-')[0]
  const family = preference === 'auto' && languageItems.find(item => item.value.toLowerCase().split('-')[0] === primary)
  return family ? family.value : zh.locale
}

export function translator(language: string) {
  const strings = catalogs.get(language)?.strings ?? zh.strings
  return (key: Key, values: Record<string, string> = {}): string =>
    (strings[key] || zh.strings[key]).replace(/\{([^{}]+)\}/g, (match, name: string) => values[name] ?? match)
}
