import { afterEach, describe, expect, it, vi } from 'vitest'
import { invoke } from '@tauri-apps/api/core'
import { createSetupService, parseProgramList, parseSaveResult } from '../../frontend/src/platform/setup-service'
import fixture from '../fixtures/setup-snapshot.json'

vi.mock('@tauri-apps/api/core', () => ({ invoke: vi.fn() }))
afterEach(() => vi.resetAllMocks())
describe('desktop command boundary', () => {
  it('serializes preferences, detection and save, then continues after an error', async () => {
    const calls: string[] = []
    let release!: () => void
    vi.mocked(invoke).mockImplementation(async command => {
      calls.push(command)
      if (command === 'set_setup_preferences') await new Promise<void>(resolve => { release = resolve })
      if (command === 'detect_setup_port') throw new Error('Detection failed')
      return undefined as never
    })
    const service = createSetupService()
    const preferences = service.setPreferences!(false, 'ja-JP')
    const detection = service.detectPort('http', '127.0.0.1').catch(() => null)
    const open = service.openLocation('startup')
    await vi.waitFor(() => expect(calls).toEqual(['set_setup_preferences']))
    release(); await Promise.all([preferences, detection, open])
    expect(calls).toEqual(['set_setup_preferences', 'detect_setup_port', 'open_setup_location'])
  })
  it('never sends arbitrary program paths or test roots from the page', async () => {
    vi.mocked(invoke).mockResolvedValue({ path: '', entry: null })
    const service = createSetupService()
    await service.selectProgram!()
    expect(invoke).toHaveBeenCalledWith('add_setup_program', undefined)
    await expect(service.addProgram({ name: 'x', path: 'C:\\x.exe', arguments: '' })).rejects.toThrow()
    expect(invoke).toHaveBeenCalledTimes(1)
  })
  it('keeps a committed result with warnings and rejects malformed outcomes', () => {
    const saved = { commit: 'saved', message: 'ui.save.response', detail: '', nativeError: 8, tasks: [], warnings: ['ui.save.response'], conflictDirectory: '', options: null }
    expect(parseSaveResult(saved).commit).toBe('saved')
    expect(parseSaveResult(saved).nativeError).toBe(8)
    expect(() => parseSaveResult({ ...saved, nativeError: -1 })).toThrow()
    expect(() => parseSaveResult({ ...saved, commit: 'maybe' })).toThrow()
    expect(() => parseSaveResult({ ...saved, warnings: ['unrecognized'] })).toThrow()
    expect(() => parseSaveResult({ ...saved, options: { ...fixture.options, silent: undefined } })).toThrow()
    expect(parseSaveResult({ ...saved, options: fixture.options }).options?.extra).toBe(fixture.options.extra)
  })
  it('preserves the detection source and native explanation', async () => {
    const response = {port: '7897', source: 'Clash Verge Rev', message: 'Detected locally'}
    vi.mocked(invoke).mockResolvedValue(response)
    expect(await createSetupService().detectPort('http', '127.0.0.1')).toEqual(response)
  })
  it('reads the fixed companion roots and rejects ambiguous identities or badge values', async () => {
    const row = { id: 'org.example.tool', name: 'Example', path: 'tool.exe', arguments: '', status: 'adapted' }
    const value = { entries: [row], incomplete: false }
    vi.mocked(invoke).mockResolvedValue(value)
    expect(await createSetupService().listPrograms()).toEqual(value)
    expect(invoke).toHaveBeenCalledWith('list_setup_programs', undefined)
    expect(() => parseProgramList({ ...value, entries: [row, row] })).toThrow()
    expect(() => parseProgramList({ ...value, entries: [{ ...row, status: 'unknown' }] })).toThrow()
    expect(() => parseProgramList({ ...value, incomplete: 'false' })).toThrow()
  })
})
