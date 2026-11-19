import { describe, expect, it } from 'vitest'
import { parseSnapshot } from '../../frontend/src/platform/types'
import { languageItems, resolveLanguage, translator } from '../../frontend/src/i18n'
import registry from '../../locales/languages.json'
import fixture from '../fixtures/setup-snapshot.json'

describe('native snapshot contract', () => {
  it('preserves saved values, Unicode and raw arguments', () => {
    const state = parseSnapshot(fixture)
    expect(state.options.extra).toBe('"" "中文 参数" "quote\\"end"')
    expect(state.options.silent).toBe(true)
  })
  it('rejects partial, unsupported and resource-less snapshots', () => {
    expect(() => parseSnapshot({ ...fixture, schemaVersion: 2 })).toThrow()
    expect(() => parseSnapshot({ ...fixture, payloadBytes: 0 })).toThrow()
    expect(() => parseSnapshot({ ...fixture, options: { ...fixture.options, codexEnv: undefined } })).toThrow()
    expect(() => parseSnapshot({ ...fixture, preferences: {} })).toThrow()
  })
  it('shares translations and substitutes only supplied placeholders', () => {
    expect(translator('en-US')('setup.action.refresh')).toBe('Refresh')
    expect(translator('ja-JP')('setup.action.refresh')).toBe('更新')
    expect(translator('unknown')('setup.action.refresh')).toBe('刷新')
    expect(translator('zh-CN')('setup.version', { version: '1.2.3' })).toBe('配置器 v1.2.3')
  })
  it('offers registered languages in order and matches exact tags before language families', () => {
    expect(languageItems.map(item => item.value)).toEqual(registry.languages.map(item => item.tag))
    for (const { tag } of registry.languages) expect(resolveLanguage('auto', tag.toLowerCase())).toBe(tag)
    expect(resolveLanguage('auto', 'en-x-fixture')).toBe(registry.languages.find(item => item.tag.startsWith('en-'))?.tag)
    expect(resolveLanguage('auto', 'javanese')).toBe('zh-CN')
  })
})
