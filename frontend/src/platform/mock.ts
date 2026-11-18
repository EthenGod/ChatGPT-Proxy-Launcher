import type { SetupOptions, SetupSnapshot } from './types'
import type { SetupService, SaveResult, ProgramEntry, ListedProgram, Detection } from '../features/setup/contract'
import { translator, resolveLanguage, type MessageKey } from '../i18n'
import { previewOptions } from '../features/setup/preview'

export const scenarios = ['normal', 'first', 'missing', 'unknown', 'silent', 'long', 'load-error', 'saving', 'save-error', 'rollback', 'incomplete', 'warnings'] as const
export type Scenario = typeof scenarios[number]
export const scenarioLabels: Record<Scenario, MessageKey> = {
  normal: 'ui.scenario.normal', first: 'ui.scenario.first', missing: 'ui.scenario.missing', unknown: 'ui.scenario.unknown',
  silent: 'ui.scenario.silent', long: 'ui.scenario.long', 'load-error': 'ui.scenario.loaderror',
  saving: 'ui.scenario.saving',
  'save-error': 'ui.scenario.saveerror', rollback: 'ui.scenario.rollback', incomplete: 'ui.scenario.incomplete', warnings: 'ui.scenario.warnings',
}
export const defaultDirectory = 'C:\\Apps\\ChatGPT Launcher'
export const samplePrograms: ProgramEntry[] = [
  { name: 'Notes.lnk', path: 'C:\\Apps\\Notes\\Notes.lnk', arguments: '--profile "Work notes"' },
  { name: 'Helper.exe', path: 'C:\\Apps\\Helper\\Helper.exe', arguments: '' },
  { name: '工具.lnk', path: 'C:\\Apps\\工具\\工具.lnk', arguments: '"中文 参数"' },
]
const wait = (ms: number) => new Promise<void>(resolve => setTimeout(resolve, ms))

/** No invoke, filesystem, registry, fetch, storage or system actions. Reset on reload. */
export class MockSetupService implements SetupService {
  picker = { directories: [defaultDirectory, 'D:\\Tools\\ChatGPT Launcher', 'C:\\Apps\\工作工具\\ChatGPT Launcher'], programs: samplePrograms }
  programs: ListedProgram[] = []
  calls = { load: 0, detect: 0, save: 0, add: 0, open: 0, list: 0 }
  private snapshot: SetupSnapshot
  constructor(readonly scenario: Scenario = 'normal', private delay = 1) {
    this.snapshot = {
      schemaVersion: 1, defaultDirectory, setupVersion: '1.1.3', deployedVersion: scenario === 'unknown' ? '' : '1.1.2',
      deployed: !['first', 'missing'].includes(scenario), hasSettings: scenario !== 'first', validArguments: true, notice: '', payloadBytes: 1,
      preferences: { dark: true, language: 'auto', resolvedLanguage: 'zh-CN' },
      options: { startup: false, silent: scenario === 'silent', desktop: true, proxy: true, codexEnv: true,
        protocol: 'http', host: '127.0.0.1', port: '7890', bypass: '', extra: '',
        directory: scenario === 'long' ? 'C:\\Apps\\' + '较长的目录名称\\'.repeat(12) + 'ChatGPT Launcher' : defaultDirectory },
    }
    if (scenario !== 'first') {
      const path = defaultDirectory + '\\StartupApps\\Quick Notes.lnk'
      this.programs = [
        { id: path, name: 'Quick Notes.lnk', path, arguments: '', status: 'ordinary' },
        { id: 'example.session-tools', name: 'Session Tools', path: 'C:\\Apps\\Session Tools\\SessionTools.exe', arguments: '', status: 'adapted' },
      ]
    }
  }
  async load() {
    this.calls.load++
    await wait(180 * this.delay)
    if (this.scenario === 'load-error') throw { nativeError: 5, message: this.snapshot.options.directory }
    return structuredClone(this.snapshot)
  }
  async detectPort(protocol: string, host: string): Promise<Detection> {
    this.calls.detect++
    await wait(1000 * this.delay)
    const t = translator(resolveLanguage(document.documentElement.lang || 'auto'))
    const found = ['127.0.0.1', 'localhost'].includes(host) && protocol !== 'https'
    return { port: found ? '7897' : null, source: found ? t('ui.detect.fixture') : '',
      message: found ? t('proxy.detected', { source: t('ui.detect.fixture') }) : t(protocol === 'https' ? 'proxy.httpsmanual' : 'proxy.localonly') }
  }
  async save(options: SetupOptions): Promise<SaveResult> {
    this.calls.save++
    await wait((this.scenario === 'saving' ? 8000 : 1200) * this.delay)
    if (!previewOptions(options).valid) return { commit: 'not-started', message: 'ui.save.invalid', tasks: [] }
    const failure = { 'save-error': 'not-started', rollback: 'rolled-back', incomplete: 'rollback-incomplete' } as const
    if (this.scenario in failure) {
      const commit = failure[this.scenario as keyof typeof failure]
      return { commit, message: commit === 'not-started' ? 'ui.save.failed' : commit === 'rolled-back' ? 'ui.save.rollback' : 'ui.save.incomplete',
        detail: options.directory + '\\ChatGPTProxyLauncher.exe', nativeError: commit === 'rollback-incomplete' ? 29 : 5, tasks: [] }
    }
    this.snapshot.options = structuredClone(options)
    this.snapshot.deployed = this.snapshot.hasSettings = true
    this.snapshot.deployedVersion = this.snapshot.setupVersion
    return { commit: 'saved', message: this.scenario === 'warnings' ? 'ui.save.warnings' : 'setup.status.saved',
      tasks: this.scenario === 'warnings' ? ['startup', 'restart'] : [] }
  }
  async addProgram(entry: ProgramEntry) {
    this.calls.add++
    await wait(160 * this.delay)
    const name = entry.name.replace(/\.[^.]+$/, '.lnk')
    const path = defaultDirectory + '\\StartupApps\\' + name
    if (!this.programs.some(p => p.path === path)) this.programs.push({ ...entry, id: path, name, path, status: 'ordinary' })
  }
  async listPrograms() {
    this.calls.list++
    await wait(100 * this.delay)
    return { entries: structuredClone(this.programs).sort((a, b) => a.name.localeCompare(b.name)), incomplete: false }
  }
  async openLocation() { this.calls.open++; await wait(0) }
}
