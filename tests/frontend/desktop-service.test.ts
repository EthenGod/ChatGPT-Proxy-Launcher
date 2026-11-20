import { afterEach, describe, expect, it, vi } from 'vitest'
import { invoke, isTauri } from '@tauri-apps/api/core'
import { createSetupService, parseProgramList, parseSaveResult } from '../../frontend/src/platform/setup-service'
import { desktopWindow } from '../../frontend/src/platform/desktop'
import fixture from '../fixtures/setup-snapshot.json'

const surfaces = vi.hoisted(() => ({ setTheme: vi.fn(), setBackgroundColor: vi.fn() }))
vi.mock('@tauri-apps/api/core', () => ({ invoke: vi.fn(), isTauri: vi.fn() }))
vi.mock('@tauri-apps/api/webviewWindow', () => ({ getCurrentWebviewWindow: () => surfaces }))
afterEach(() => vi.resetAllMocks())
describe('desktop window', () => {
  it('waits for both native appearance changes before starting the next theme after a failure', async () => {
    vi.mocked(isTauri).mockReturnValue(true)
    const failure = new Error('Title bar unavailable')
    let finishBackground!: () => void
    surfaces.setTheme.mockRejectedValueOnce(failure).mockResolvedValue(undefined)
    surfaces.setBackgroundColor.mockImplementationOnce(() => new Promise<void>(resolve => { finishBackground = resolve }))
      .mockResolvedValue(undefined)
    const dark = desktopWindow.setTheme(true).catch(error => error)
    const light = desktopWindow.setTheme(false)
    await vi.waitFor(() => expect(surfaces.setBackgroundColor).toHaveBeenCalled())
    try {
      expect(surfaces.setTheme.mock.calls).toEqual([['dark']])
    } finally { finishBackground() }
    expect(await dark).toBe(failure)
    await light
    expect(surfaces.setTheme.mock.calls).toEqual([['dark'], ['light']])
    expect(surfaces.setBackgroundColor.mock.calls).toEqual([['#111111'], ['#fcfcfc']])
  })
})
describe('desktop command boundary', () => {
  it('skips obsolete queued previews and runs the current one after the pending native operation', async () => {
    let finish!: () => void
    vi.mocked(invoke).mockImplementationOnce(() => new Promise(resolve => { finish = () => resolve(undefined) }))
      .mockResolvedValue({ valid: true, arguments: '--latest', errors: {} })
    const service = createSetupService()
    const busy = service.openLocation('startup')
    const obsolete = new AbortController()
    const skipped = service.validate(fixture.options, obsolete.signal)
    const current = { ...fixture.options, extra: '--latest' }
    const latest = service.validate(current)
    obsolete.abort()
    await Promise.resolve()
    expect(invoke).toHaveBeenCalledExactlyOnceWith('open_setup_location', { location: 'startup' })
    finish()
    await expect(skipped).rejects.toMatchObject({ name: 'AbortError' })
    await expect(latest).resolves.toMatchObject({ arguments: '--latest' })
    await Promise.all([busy, service.settled!()])
    expect(invoke).toHaveBeenCalledTimes(2)
    expect(invoke).toHaveBeenLastCalledWith('validate_setup', { options: current })
  })
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
    expect(service.picker).toBeUndefined()
    expect(service).not.toHaveProperty('addProgram')
    expect(invoke).toHaveBeenCalledTimes(1)
  })
  it('keeps a committed result with warnings and rejects malformed outcomes', () => {
    const saved = { commit: 'saved', message: 'ui.save.response', detail: '', nativeError: 8, tasks: [], warnings: ['ui.save.response'], conflictDirectory: '', options: null }
    expect(parseSaveResult(saved).commit).toBe('saved')
    expect(parseSaveResult(saved).nativeError).toBe(8)
    expect(() => parseSaveResult({ ...saved, nativeError: -1 })).toThrow()
    expect(() => parseSaveResult({ ...saved, commit: 'maybe' })).toThrow()
    expect(() => parseSaveResult({ ...saved, commit: ['saved'] })).toThrow()
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
    const service = createSetupService()
    expect(await service.listPrograms()).toEqual(value)
    await expect(service.settled!()).resolves.toBeUndefined()
    expect(invoke).toHaveBeenCalledWith('list_setup_programs', undefined)
    expect(() => parseProgramList({ ...value, entries: [row, row] })).toThrow()
    expect(parseProgramList({ ...value, entries: [{ ...row, status: 'dependency-blocked' }] }).entries[0]?.status).toBe('dependency-blocked')
    expect(() => parseProgramList({ ...value, entries: [{ ...row, status: 'unknown' }] })).toThrow()
    expect(() => parseProgramList({ ...value, incomplete: 'false' })).toThrow()
    const png = 'iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mP8/x8AAwMCAO+aFlgAAAAASUVORK5CYII='
    const withIcon = (icons: string[], icon: unknown) => parseProgramList({ ...value, icons, entries: [{ ...row, icon }] }).entries[0]
    expect(withIcon([png], 0)?.icon).toBe('data:image/png;base64,' + png)
    expect(withIcon([png], 9)).toEqual(row)
    expect(withIcon(['file:///example.ico'], 0)).toEqual(row)
    expect(withIcon([png], '0')).toEqual(row)
  })
})
