import type { SetupOptions, SetupSnapshot } from './types'
import type { SetupService, SaveResult, ProgramEntry, ListedProgram, Detection } from '../features/setup/contract'
import { translator, resolveLanguage, type MessageKey } from '../i18n'
import { previewOptions } from '../features/setup/preview'

export const programScenarios = ['programs', 'first', 'programs-partial', 'programs-error'] as const
export const scenarios = ['normal', ...programScenarios, 'missing', 'unknown', 'silent', 'long', 'load-error', 'saving', 'save-error', 'rollback', 'incomplete', 'warnings'] as const
export type Scenario = typeof scenarios[number]
export const scenarioLabels: Record<Scenario, MessageKey> = {
  normal: 'ui.scenario.normal', first: 'ui.scenario.first', missing: 'ui.scenario.missing', unknown: 'ui.scenario.unknown',
  silent: 'ui.scenario.silent', long: 'ui.scenario.long', 'load-error': 'ui.scenario.loaderror',
  saving: 'ui.scenario.saving',
  programs: 'ui.scenario.programs', 'programs-partial': 'ui.scenario.programspartial', 'programs-error': 'ui.scenario.programserror',
  'save-error': 'ui.scenario.saveerror', rollback: 'ui.scenario.rollback', incomplete: 'ui.scenario.incomplete', warnings: 'ui.scenario.warnings',
}
export const defaultDirectory = 'C:\\Apps\\ChatGPT Launcher'
export const samplePrograms: ProgramEntry[] = [
  { name: 'Notes.lnk', path: 'C:\\Apps\\Notes\\Notes.lnk', arguments: '--profile "Work notes"' },
  { name: 'Helper.exe', path: 'C:\\Apps\\Helper\\Helper.exe', arguments: '' },
  { name: '工具.lnk', path: 'C:\\Apps\\工具\\工具.lnk', arguments: '"中文 参数"' },
]
const wait = (ms: number) => new Promise<void>(resolve => setTimeout(resolve, ms))
const ordinal = (left: string, right: string) => left < right ? -1 : left > right ? 1 : 0

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
    if (scenario.startsWith('programs')) {
      const startup = defaultDirectory + '\\StartupApps\\'
      this.programs = [
        { id: 'org.example.audio', name: 'Audio Monitor', path: 'C:\\Apps\\Audio Monitor\\monitor.exe', arguments: '--profile "Studio A"', status: 'adapted' },
        { id: 'org.example.archive', name: 'Meeting Notes and Transcription Archive', path: 'C:\\Apps\\Meeting Notes\\Archive\\archive.exe', arguments: '--folder "会议记录" --mode archive', status: 'adapted-disabled' },
        { id: startup + 'Quick Notes.lnk', name: 'Quick Notes.lnk', path: startup + 'Quick Notes.lnk', arguments: '--notebook "Work notes"', status: 'ordinary' },
        { id: startup + '旧版笔记.lnk', name: '旧版笔记.lnk', path: startup + '旧版笔记.lnk', arguments: '', status: 'unavailable' },
        { id: 'org.example.calendar', name: 'org.example.calendar', path: defaultDirectory + '\\CompanionRegistry\\org.example.calendar.json', arguments: '', status: 'invalid' },
        { id: 'org.example.timer', name: 'org.example.timer', path: 'C:\\Apps\\Focus Timer\\manifest.json', arguments: '', status: 'disabled' },
        { id: startup + '整理笔记.ps1', name: '整理笔记.ps1', path: startup + '整理笔记.ps1', arguments: '', status: 'ordinary' },
      ]
    } else if (scenario !== 'first') {
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
    const firstRead = ++this.calls.list === 1
    await wait(100 * this.delay)
    if (firstRead && this.scenario === 'programs-error') throw { nativeError: 5, message: defaultDirectory + '\\StartupApps' }
    const incomplete = firstRead && this.scenario === 'programs-partial'
    const rows = incomplete ? this.programs.filter(p => p.status === 'ordinary' || p.status === 'unavailable') : this.programs
    // Match the native case-insensitive ordering for the sample names across system locales.
    const entries = structuredClone(rows).sort((a, b) => ordinal(a.name.toUpperCase(), b.name.toUpperCase()) || ordinal(a.id, b.id))
    return { entries, incomplete }
  }
  async openLocation() { this.calls.open++; await wait(0) }
}
