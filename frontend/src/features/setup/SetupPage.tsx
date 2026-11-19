import { useCallback, useEffect, useLayoutEffect, useRef, useState } from 'react'
import { Collapsible } from '@base-ui/react/collapsible'
import { Radio } from '@base-ui/react/radio'
import { RadioGroup } from '@base-ui/react/radio-group'
import { AlertCircle, ArrowUpRight, Check, CheckCircle2, ChevronDown, CircleHelp, FlaskConical,
  ChevronRight, FolderOpen, Globe2, LoaderCircle, Moon, Search, Sun } from 'lucide-react'
import { Button, ChoiceMenu, Hint, Modal, SwitchControl, TextField, Toggle } from '../../components/controls'
import { ScrollBox } from '../../components/ScrollBox'
import { ErrorDetails } from '../../components/ErrorDetails'
import { NoticeToasts, createNoticeManager, type NoticeData } from '../../components/NoticeToasts'
import { useAppearanceTransition } from '../../components/useAppearanceTransition'
import { languageItems, resolveLanguage, translator, type MessageKey } from '../../i18n'
import type { SetupOptions, SetupSnapshot } from '../../platform/types'
import { nativeIssue, type ErrorInfo } from './errors'
import { portErrorKey, sameOptions } from './options'
import { useSetupValidation } from './useSetupValidation'
import { ArgumentSkeleton, SetupSkeleton } from './SetupSkeleton'
import { SetupStatus, type OperationIssue } from './SetupStatus'
import type { ListedProgram, SaveResult, SetupService, WindowHost } from './contract'
import { CompanionPrograms, ProgramDetails } from './CompanionPrograms'

type DialogState = 'discard' | 'help' | 'directory' | 'program' | 'program-details' | 'result' | 'preview' | 'error' | 'notice' | null
type DetectionPhase = 'idle' | 'pending' | 'scanning' | 'found'
type Operation = 'save' | 'directory' | 'program' | 'close'
interface DialogText { title: MessageKey; description?: MessageKey }
const dialogText: Record<Exclude<DialogState, 'help' | 'result' | null>, DialogText> = {
  discard: { title: 'setup.dialog.close', description: 'setup.dialog.discard' },
  directory: { title: 'setup.dialog.directory', description: 'ui.picker.preview' },
  program: { title: 'setup.dialog.program', description: 'ui.picker.preview' },
  'program-details': { title: 'ui.programs.details' },
  preview: { title: 'ui.prototype.title', description: 'ui.prototype.description' },
  error: { title: 'ui.error.title' },
  notice: { title: 'ui.notice.label' },
}
const helpTitles = {
  'setup.hint.startup': 'ui.help.startup',
  'ui.help.proxy': 'setup.dialog.proxy',
  'help.codex': 'setup.help.codex',
  'setup.hint.directory': 'ui.help.directory',
  'ui.help.programs': 'ui.help.programs.title',
} as const satisfies Partial<Record<MessageKey, MessageKey>>

// The first release that starts ordinary StartupApps entries is v1.0.4.
function supportsStartupApps(version: string) {
  const parts = /^(\d+)\.(\d+)\.(\d+)(?:\.\d+)?(?:-[\w.-]+)?$/.exec(version)
  if (!parts) return false
  const major = Number(parts[1]), minor = Number(parts[2]), patch = Number(parts[3])
  return major > 1 || (major === 1 && (minor > 0 || patch >= 4))
}

export function SetupPage({ service, host, scenarioControls, onLanguageChange, showPrograms = false }: {
  service: SetupService; host: WindowHost; scenarioControls?: React.ReactNode; onLanguageChange?: (language: string) => void; showPrograms?: boolean
}) {
  const [snapshot, setSnapshot] = useState<SetupSnapshot | null>(null)
  const [options, setOptions] = useState<SetupOptions | null>(null)
  const [language, setLanguage] = useState('auto')
  const [dark, setDark] = useState(true)
  const requestedDark = useRef(true)
  const transitionAppearance = useAppearanceTransition()
  const [loading, setLoading] = useState(true)
  const [loadError, setLoadError] = useState<MessageKey | null>(null)
  const [error, setError] = useState<ErrorInfo | null>(null)
  const [noticeManager] = useState(createNoticeManager)
  const [selectedNotice, setSelectedNotice] = useState<NoticeData | null>(null)
  const [preferenceWarnings, setPreferenceWarnings] = useState<MessageKey[]>([])
  const preferenceRevision = useRef(0)
  const requestedLanguage = useRef('auto')
  const [operation, setOperation] = useState<Operation | null>(null)
  const activeOperation = useRef<Operation | null>(null)
  const busy = operation !== null
  const saving = operation === 'save'
  const adding = operation === 'program'
  const [detectionPhase, setDetectionPhase] = useState<DetectionPhase>('idle')
  const detecting = detectionPhase === 'pending' || detectionPhase === 'scanning'
  const appearanceLocked = busy || loading || detecting || loadError === 'setup.error.webviewstart'
  const [advanced, setAdvanced] = useState(showPrograms)
  const revealPrograms = useRef(showPrograms)
  const [inspectedProgram, setInspectedProgram] = useState<ListedProgram | null>(null)
  const [result, setResult] = useState<SaveResult | null>(null)
  const [notice, setNotice] = useState<OperationIssue | null>(null)
  const [programRevision, setProgramRevision] = useState(0)
  const [dialog, setDialog] = useState<DialogState>(null)
  const [dialogOpen, setDialogOpen] = useState(false)
  const [help, setHelp] = useState<keyof typeof helpTitles>('ui.help.proxy')
  const [selection, setSelection] = useState('')
  const [closed, setClosed] = useState(false)
  const alive = useRef(true)
  const openingLocation = useRef(false)
  const focusAfterSave = useRef(false)
  const detectRevision = useRef(0)
  const loadRevision = useRef(0)
  const savedWithoutSnapshot = useRef(false)
  const closeRequest = useRef<() => void>(() => {})
  const focusReturn = useRef<HTMLElement | null>(null)
  const cancelButton = useRef<HTMLButtonElement | null>(null)
  const saveButton = useRef<HTMLButtonElement | null>(null)
  const doneButton = useRef<HTMLButtonElement | null>(null)
  const preview = useRef<HTMLPreElement | null>(null)
  const defaultDirectory = snapshot?.defaultDirectory ?? ''
  const samplePrograms = service.picker?.programs ?? []
  const resolved = resolveLanguage(language, service.native ? snapshot?.preferences.resolvedLanguage : undefined)
  const t = translator(resolved)
  const dirty = !!options && !!snapshot && !sameOptions(options, snapshot.options)
  const { validation, previewValue, resetValidation } = useSetupValidation(service, options, resolved, busy || loading || closed)
  const locked = busy || loading || result?.commit === 'rollback-incomplete'

  function notify(message: MessageKey, values: Record<string, string> = {}, detail: NoticeData['detail'] = '', type = 'info', problem?: ErrorInfo, fullDetail?: NoticeData['fullDetail']) {
    noticeManager.add({ id: 'activity', type, timeout: problem ? 5000 : 3500,
      data: { message, values, detail, error: problem, fullDetail } })
  }
  function reportFailure(message: MessageKey, cause: unknown) {
    const problem = nativeIssue(cause)
    setNotice({ message, error: problem }); notify(message, {}, '', 'error', problem)
  }

  const openDialog = (next: Exclude<DialogState, null>) => {
    focusReturn.current = document.activeElement as HTMLElement | null
    setDialog(next); setDialogOpen(true)
  }
  function showError(problem: ErrorInfo) {
    setError(problem); openDialog('error')
  }
  // A second click or native close event can arrive before React paints disabled controls.
  function beginOperation(next: Operation) {
    if (activeOperation.current) return false
    activeOperation.current = next
    setOperation(next)
    return true
  }
  function finishOperation(restoreFocus?: HTMLElement | null) {
    activeOperation.current = null
    if (!alive.current) return
    setOperation(null)
    if (restoreFocus) requestAnimationFrame(() => {
      if (alive.current && !activeOperation.current && restoreFocus.isConnected) restoreFocus.focus()
    })
  }
  async function close() {
    if (!beginOperation('close')) return
    const revision = loadRevision.current
    const sameWindow = () => alive.current && revision === loadRevision.current
    try {
      await service.settled?.()
      if (sameWindow()) await host.close()
      if (sameWindow()) {
        setDialogOpen(false)
        setClosed(true)
        return
      }
    } catch (cause) {
      if (sameWindow()) reportFailure('ui.close.failed', cause)
    }
    finishOperation()
  }
  closeRequest.current = () => {
    if (activeOperation.current || openingLocation.current) return
    if (dialogOpen) { if (dialog !== 'discard') setDialogOpen(false); return }
    if (dirty) openDialog('discard')
    else void close()
  }
  const load = useCallback(async () => {
    const revision = ++loadRevision.current
    savedWithoutSnapshot.current = false
    ++detectRevision.current; setDetectionPhase('idle')
    setLoading(true); setLoadError(null)
    try {
      const data = await service.load()
      if (!alive.current || revision !== loadRevision.current) return
      resetValidation()
      setSnapshot(data); setOptions({ ...data.options }); setLanguage(data.preferences.language)
      requestedLanguage.current = data.preferences.language
      setError(null); setPreferenceWarnings([])
      setDark(data.preferences.dark); setResult(null); setNotice(null)
    } catch (cause) { if (alive.current && revision === loadRevision.current) { setLoadError('ui.load.failed'); setError(nativeIssue(cause)) } }
    finally { if (alive.current && revision === loadRevision.current) setLoading(false) }
  }, [service, resetValidation])
  useEffect(() => {
    alive.current = true
    let active = true
    let disconnect: (() => void) | undefined
    // Capture also covers Base UI portals and keyboard context-menu requests.
    const preventBrowserMenu = (event: MouseEvent) => event.preventDefault()
    window.addEventListener('contextmenu', preventBrowserMenu, true)
    setLoading(true)
    void host.connect(() => { if (active) closeRequest.current() }).then(async remove => {
      if (!active) return remove()
      disconnect = remove
      await host.ready()
      if (active) await load()
    }).catch(cause => {
      if (!active) return
      setError(nativeIssue(cause)); setLoadError('setup.error.webviewstart'); setLoading(false)
    })
    return () => {
      active = false
      alive.current = false
      ++detectRevision.current
      ++loadRevision.current
      disconnect?.()
      window.removeEventListener('contextmenu', preventBrowserMenu, true)
    }
  }, [host, load])
  useLayoutEffect(() => {
    document.documentElement.classList.toggle('dark', dark)
    document.documentElement.classList.toggle('light', !dark)
    document.documentElement.lang = resolved
    onLanguageChange?.(resolved)
  }, [dark, resolved, onLanguageChange])
  useLayoutEffect(() => { requestedDark.current = dark }, [dark])
  useEffect(() => {
    let current = true
    void host.setTheme(dark).catch(cause => { if (current) reportFailure('ui.theme.failed', cause) })
    return () => { current = false }
  }, [dark, host])
  useEffect(() => {
    const beforeUnload = (event: BeforeUnloadEvent) => {
      if (activeOperation.current !== 'close' && (dirty || activeOperation.current)) {
        event.preventDefault(); event.returnValue = ''
      }
    }
    window.addEventListener('beforeunload', beforeUnload)
    return () => window.removeEventListener('beforeunload', beforeUnload)
  }, [dirty])

  // These timers affect the glyph only. The request, result and retry have no minimum duration.
  useEffect(() => {
    if (detectionPhase !== 'pending' && detectionPhase !== 'found') return
    const revision = detectRevision.current
    const next = detectionPhase === 'pending' ? 'scanning' : 'idle'
    const timer = window.setTimeout(() => {
      if (revision === detectRevision.current) setDetectionPhase(current => current === detectionPhase ? next : current)
    }, detectionPhase === 'pending' ? 180 : 800)
    return () => window.clearTimeout(timer)
  }, [detectionPhase])

  async function preferences(nextDark: boolean, nextLanguage: string) {
    if (!service.setPreferences) return
    const revision = ++preferenceRevision.current
    try {
      const reply = await service.setPreferences(nextDark, nextLanguage)
      if (!alive.current || revision !== preferenceRevision.current) return
      const apply = () => {
        setSnapshot(current => current ? { ...current, preferences: reply.preferences } : current)
        setLanguage(reply.preferences.language)
      }
      if (reply.preferences.language !== language || reply.preferences.resolvedLanguage !== resolved) transitionAppearance('language', apply)
      else apply()
      setPreferenceWarnings(reply.warnings)
      // Native notices include the OS error details. Refresh only their text,
      // without replacing edits or the baseline for unsaved changes.
      if (snapshot?.notice && reply.preferences.resolvedLanguage !== resolved) {
        const previous = snapshot.notice
        try {
          const refreshed = await service.load()
          if (alive.current && revision === preferenceRevision.current) {
            setSnapshot(current => current?.notice === previous ? { ...current, notice: refreshed.notice } : current)
          }
        } catch (cause) {
          if (alive.current && revision === preferenceRevision.current) reportFailure('ui.load.failed', cause)
        }
      }
    } catch { if (alive.current && revision === preferenceRevision.current) setPreferenceWarnings(['setup.language.unsaved', 'setup.status.themeunsaved']) }
  }
  function changeLanguage(value: string) {
    requestedLanguage.current = value
    if (!service.setPreferences) transitionAppearance('language', () => setLanguage(value))
    void preferences(requestedDark.current, value)
  }
  async function chooseDirectory() {
    if (!options || locked) return
    if (!service.chooseDirectory) { setSelection(options.directory); openDialog('directory'); return }
    if (!beginOperation('directory')) return
    const trigger = document.activeElement as HTMLElement | null
    setError(null)
    try {
      const path = await service.chooseDirectory(options.directory)
      if (alive.current && path && path !== options.directory) {
        setOptions(current => current ? { ...current, directory: path } : current)
        setResult(null)
      }
    } catch (cause) { if (alive.current) reportFailure('setup.error.directory', cause) }
    finally { finishOperation(trigger) }
  }
  function programAdded(path: string) {
    setProgramRevision(previous => previous + 1)
    const ready = snapshot?.deployed && (savedWithoutSnapshot.current || supportsStartupApps(snapshot.deployedVersion))
    notify(ready ? 'setup.status.added' : 'setup.status.addeddeploy', {}, '', 'success', undefined, path)
    setError(null)
    setNotice(null)
  }
  async function chooseProgram() {
    if (locked) return
    if (!service.selectProgram) { setSelection(samplePrograms[0]?.path ?? ''); openDialog('program'); return }
    if (!beginOperation('program')) return
    const trigger = document.activeElement as HTMLElement | null
    try {
      const entry = await service.selectProgram()
      if (alive.current && entry) programAdded(entry.path)
    } catch (cause) { if (alive.current) reportFailure('setup.error.add', cause) }
    finally { finishOperation(trigger) }
  }

  function update<K extends keyof SetupOptions>(key: K, value: SetupOptions[K]) {
    if (!options || locked || activeOperation.current || options[key] === value) return
    if (['host', 'port', 'protocol', 'proxy'].includes(key)) { ++detectRevision.current; setDetectionPhase('idle'); noticeManager.close('activity') }
    setOptions({ ...options, [key]: value }); setResult(null); setNotice(null); setError(null)
  }
  function changeProxy(enabled: boolean) {
    if (!options || locked || activeOperation.current || enabled === options.proxy) return
    update('proxy', enabled)
    if (enabled) void detect({ ...options, proxy: true })
  }
  async function detect(input = options) {
    if (!input || detecting || locked || activeOperation.current || !input.proxy) return
    const revision = ++detectRevision.current
    setDetectionPhase('pending'); setNotice(null); setResult(null)
    let found = false
    try {
      const detection = await service.detectPort(input.protocol, input.host)
      if (!alive.current || revision !== detectRevision.current || activeOperation.current) return
      const port = detection.port
      if (port) {
        found = true
        setOptions(current => current && current.port !== port ? { ...current, port } : current)
      }
      setError(null)
      notify(detection.port ? 'ui.detect.port' : 'ui.detect.empty', { port: detection.port ?? '' },
        detection.port && detection.source ? { message: 'ui.detect.source', values: { source: detection.source } } : detection.message,
        detection.port ? 'success' : 'info', undefined, detection.message)
    } catch (cause) { if (alive.current && revision === detectRevision.current && !activeOperation.current) reportFailure('ui.detect.failed', cause) }
    finally { if (alive.current && revision === detectRevision.current) setDetectionPhase(found ? 'found' : 'idle') }
  }
  async function save() {
    if (!options || !snapshot || !validation?.valid || locked || (snapshot.deployed && !dirty && result?.commit === 'saved')) return
    if (!beginOperation('save')) return
    focusAfterSave.current = true; ++detectRevision.current; setDetectionPhase('idle'); setNotice(null); setResult(null)
    noticeManager.close('activity')
    const submitted = { ...options }
    const revision = loadRevision.current
    const currentPage = () => alive.current && revision === loadRevision.current
    try {
      const saved = await service.save(submitted)
      if (!currentPage()) return
      setResult(saved); setError(null)
      if (saved.commit === 'saved') {
        try {
          const state = await service.load()
          if (!currentPage()) return
          savedWithoutSnapshot.current = false
          const restored = saved.warnings?.includes('setup.status.envunsaved')
            ? { ...state.options, codexEnv: (saved.options ?? submitted).codexEnv } : state.options
          setSnapshot({ ...state, options: restored }); setOptions({ ...restored })
        } catch (cause) {
          if (!currentPage()) return
          savedWithoutSnapshot.current = true
          const committed = saved.options ?? submitted
          setOptions(committed)
          setSnapshot(current => current && { ...current, options: committed, deployed: true, hasSettings: true,
            validArguments: true, notice: '', deployedVersion: '' })
          reportFailure('ui.save.refreshfailed', cause)
        }
        if (saved.tasks.length || saved.warnings?.length) {
          focusReturn.current = doneButton.current
          setDialog('result'); setDialogOpen(true)
        }
      }
    } catch (cause) {
      // A lost response cannot establish whether writes happened.
      if (currentPage()) setResult({ commit: 'rollback-incomplete', message: 'ui.save.unknown', tasks: [], ...nativeIssue(cause) })
    } finally { finishOperation() }
  }
  useEffect(() => {
    if (!busy && result && !dialogOpen && focusAfterSave.current) {
      focusAfterSave.current = false
      const target = result.commit === 'saved' ? doneButton : saveButton
      target.current?.focus()
    }
  }, [busy, result, dialogOpen])
  async function addProgram() {
    const picker = service.picker
    const entry = picker?.programs.find(program => program.path === selection)
    if (!picker || !entry || !beginOperation('program')) return
    try {
      await picker.addProgram(entry)
      if (!alive.current) return
      programAdded(entry.path)
      setDialogOpen(false)
    } catch (cause) { if (alive.current) reportFailure('setup.error.add', cause) }
    finally { finishOperation() }
  }
  async function openLocation(location: 'startup' | 'deployment' | 'windows') {
    if (activeOperation.current || openingLocation.current) return
    openingLocation.current = true
    try { await service.openLocation(location); if (alive.current && !service.native) notify('ui.location.preview') }
    catch (cause) { if (alive.current) reportFailure('setup.error.openfolder', cause) }
    finally { openingLocation.current = false }
  }
  const helpButton = (key: keyof typeof helpTitles) => {
    const label = t(helpTitles[key])
    return <Hint label={label}><Button className="icon-button help-button" aria-label={label} onClick={() => { setHelp(key); openDialog('help') }} disabled={busy}><CircleHelp size={16} /></Button></Hint>
  }
  const portError = options?.proxy ? portErrorKey(options.port) : undefined
  const invalidInput = !!portError || validation?.valid === false
  const checking = !!options && !validation && !portError
  let previewText = ''
  if (portError) previewText = t(portError)
  else if (previewValue?.valid) previewText = previewValue.arguments || t('ui.arguments.empty')
  else if (validation) previewText = validation.detail || t(Object.values(validation.errors)[0] ?? 'setup.preview.invalid')
  const fieldError = (key: keyof SetupOptions) => key === 'port' && portError ? t(portError)
    : validation?.errors[key] ? t(validation.errors[key]) : undefined
  const validationDetails = validation && !validation.valid && validation.nativeError ? validation : null
  const saved = result?.commit === 'saved' && !dirty
  const deployment = snapshot?.deployed ? snapshot.deployedVersion ? t('setup.state.version', { version: snapshot.deployedVersion }) : t('setup.state.unknownversion')
    : t(snapshot?.hasSettings ? 'setup.state.missing' : 'setup.state.absent')
  const discard = dialog === 'discard'
  const pickerOpen = dialog === 'directory' || dialog === 'program'
  let text: DialogText | undefined
  if (dialog === 'help') text = { title: helpTitles[help], description: help }
  else if (dialog === 'result') text = { title: saved ? 'ui.save.followup' : 'setup.error.saving', description: saved ? undefined : result?.message }
  else if (dialog) text = dialogText[dialog]
  const dialogTitle = text ? t(text.title) : ''
  const noticeDetail = selectedNotice?.fullDetail || selectedNotice?.detail

  if (closed && !service.native) return <div className="prototype-closed"><CheckCircle2 size={24} /><h1>{t('ui.closed')}</h1><p>{t('ui.prototype.description')}</p>
    <Button className="button primary-button" onClick={() => { finishOperation(); setClosed(false); void load() }}>{t('ui.reopen')}</Button></div>

  return <div className={`setup-shell ${service.native ? 'native-shell' : ''}`}>
    {!service.native && <div className="prototype-bar"><Button className="prototype-label" onClick={() => openDialog('preview')} disabled={busy}><FlaskConical size={13} />{t('ui.prototype.title')}</Button><fieldset disabled={busy}>{scenarioControls}</fieldset></div>}
    <ScrollBox persistent className="settings-scroll" contentClassName="settings-content" label={t('common.launcher')} render={<main role="main" aria-busy={loading} />}>
      <header className="settings-header"><div className="heading-block"><h1>{t('common.launcher')}</h1>
        {snapshot ? <span className="version-badge" aria-label={t('setup.version', { version: snapshot.setupVersion })}>v{snapshot.setupVersion}</span>
          : loading && <span className="skeleton-bar skeleton-version" aria-hidden="true" />}</div>
        <div className="appearance-controls" data-detect-pending={detectionPhase === 'pending' || undefined}><ChoiceMenu label={t('ui.language')} value={language} onChange={changeLanguage} changeAfterClose disabled={appearanceLocked}
          items={[{ value: 'auto', label: t('setup.language.system') }, ...languageItems]} icon={<Globe2 size={16} />} variant="icon" />
          <Hint label={t(dark ? 'setup.theme.light' : 'setup.theme.dark')}><Button className="icon-button" onClick={() => {
            const next = !requestedDark.current; requestedDark.current = next
            transitionAppearance('theme', () => setDark(next)); void preferences(next, requestedLanguage.current)
          }} disabled={appearanceLocked} aria-label={t(dark ? 'setup.theme.light' : 'setup.theme.dark')}>{dark ? <Sun size={16} /> : <Moon size={16} />}</Button></Hint>
        </div>
      </header>
      {loading ? <SetupSkeleton />
        : loadError ? <div className="load-state error-state" role="alert"><AlertCircle size={20} /><p>{t(loadError)}</p>{error && <ErrorDetails error={error} t={t} />}
          {loadError === 'ui.load.failed' && <Button className="button secondary-button" disabled={busy} onClick={() => void load()}>{t('ui.retry')}</Button>}</div>
        : options && snapshot && <>
          <div className="deployment-status"><span className={`status-dot ${snapshot.deployed ? 'is-deployed' : ''}`} /><span>{deployment}</span></div>
          {snapshot.notice && <p className="secondary small" role="status">{snapshot.notice}</p>}
          <section className="settings-section" aria-labelledby="startup-title"><h2 id="startup-title">{t('setup.section.startup')}</h2><div className="settings-card">
            <Toggle label={t('ui.startup')} checked={options.startup} disabled={locked} onChange={value => update('startup', value)} help={helpButton('setup.hint.startup')} />
            <Toggle label={t('ui.desktop')} checked={options.desktop} disabled={locked} onChange={value => update('desktop', value)} />
            <Toggle label={t('ui.silent')} badge={t('ui.unavailable')} checked={options.silent} disabled onChange={() => {}} />
          </div></section>
          <section className="settings-section" aria-labelledby="proxy-title"><Collapsible.Root open={options.proxy}>
            <div className="section-heading"><h2 id="proxy-title">{t('ui.proxy')}</h2>{helpButton('ui.help.proxy')}
              <SwitchControl id="proxy-enabled" label={t('setup.option.proxy')} controls={options.proxy ? 'proxy-details' : undefined} checked={options.proxy} disabled={locked} onChange={changeProxy} /></div>
            <Collapsible.Panel id="proxy-details" className="disclosure-panel">
              <div className="settings-card proxy-card">
                <div className="connection-fields">
                  <div className="connection-protocol-row">
                    <div className="field protocol-field"><span id="protocol-label" data-motion-text>{t('setup.label.protocol')}</span>
                      <RadioGroup aria-labelledby="protocol-label" className="segmented" data-protocol={options.protocol} value={options.protocol} disabled={locked} onValueChange={value => update('protocol', String(value))}>
                        {['http', 'https', 'socks5'].map(protocol => <Radio.Root key={protocol} value={protocol} className="segment">{protocol.toUpperCase()}</Radio.Root>)}
                      </RadioGroup>
                    </div>
                  </div>
                  <div className="connection-endpoints">
                    <TextField id="proxy-host" label={t('setup.label.host')} value={options.host} onChange={event => update('host', event.target.value)} disabled={locked} error={fieldError('host')} autoComplete="off" spellCheck={false} />
                    <TextField id="proxy-port" className="port-field" label={t('setup.label.port')} value={options.port} onChange={event => update('port', event.target.value)} disabled={locked} error={fieldError('port')} errorDescriptionId="validation-message" inputMode="numeric" maxLength={5} autoComplete="off"
                      suffix={<Hint label={t(detecting ? 'ui.detect.running' : 'ui.detect')}>
                        <Button className="input-action" aria-label={t(detecting ? 'ui.detect.running' : 'ui.detect')} aria-busy={detecting}
                          disabled={locked || detecting} focusableWhenDisabled={detecting && !locked} onClick={() => void detect()}>
                          <span className="detect-glyph" data-phase={detectionPhase} aria-hidden="true">
                            <Search className="detect-search" size={16} /><LoaderCircle className="detect-progress" size={16} /><Check className="detect-check" size={16} />
                          </span>
                        </Button></Hint>} />
                  </div>
                </div>
                <Toggle label={t('ui.codex')} checked={options.codexEnv} disabled={locked} onChange={value => update('codexEnv', value)} help={helpButton('help.codex')} />
              </div>
            </Collapsible.Panel></Collapsible.Root></section>
          <section className="settings-section" aria-labelledby="preview-title" aria-busy={checking}><div className="preview-heading"><h2 id="preview-title">{t('ui.arguments')}</h2>
              <div className="preview-actions">{checking && <span className="preview-checking" data-motion-text>{t('ui.arguments.checking')}</span>}
                <div className="preview-details" data-visible={!!validationDetails || undefined} inert={!validationDetails} aria-hidden={!validationDetails || undefined}>
                  <Button className="text-button" onClick={() => { if (validationDetails) showError(validationDetails) }}>
                    {t('ui.details')}<ChevronRight size={12} aria-hidden="true" />
                  </Button>
                </div>
              </div>
            </div>
            <ScrollBox className="argument-scroll" label={t('ui.arguments')}><pre ref={preview} id="validation-message" tabIndex={-1} className={`argument-preview ${invalidInput ? 'invalid-preview' : ''}`} aria-label={t('ui.arguments')}>{checking && !previewValue ? <ArgumentSkeleton /> : previewText}</pre></ScrollBox>
          </section>
          <Collapsible.Root className="advanced-section" open={advanced} onOpenChange={setAdvanced}>
            <Collapsible.Trigger className="advanced-trigger" disabled={busy}><span data-motion-text>{t('setup.section.advanced')}</span><ChevronDown size={16} /></Collapsible.Trigger>
            <Collapsible.Panel className="disclosure-panel advanced-panel">
              <div className="settings-card advanced-card">
                <TextField label={t('setup.label.directory')} value={options.directory} onChange={event => update('directory', event.target.value)} disabled={locked} error={fieldError('directory')} spellCheck={false}
                  labelActions={<>{helpButton('setup.hint.directory')}<Button className="text-button field-reset" disabled={locked || options.directory === defaultDirectory} onClick={() => update('directory', defaultDirectory)}>{t('setup.action.reset')}</Button></>}
                  suffix={<Hint label={t('setup.action.browse')}><Button className="input-action" aria-label={t('setup.action.browse')} disabled={locked} onClick={() => void chooseDirectory()}><FolderOpen size={16} /></Button></Hint>} />
                <TextField label={t('ui.bypass')} value={options.bypass} onChange={event => update('bypass', event.target.value)} disabled={locked} error={fieldError('bypass')} placeholder="localhost;*.example.com" spellCheck={false} />
                <TextField label={t('ui.extra')} value={options.extra} onChange={event => update('extra', event.target.value)} disabled={locked} error={fieldError('extra')} spellCheck={false} />
                <CompanionPrograms service={service} active={advanced} revision={programRevision} locked={locked}
                  help={helpButton('ui.help.programs')} t={t} reveal={revealPrograms} onAdd={() => void chooseProgram()} onOpen={() => void openLocation('startup')}
                  onDetails={program => { setInspectedProgram(program); openDialog('program-details') }} onError={showError} />
              </div>
            </Collapsible.Panel>
          </Collapsible.Root>
        </>}
    </ScrollBox>
    <footer className="settings-footer">
      <ScrollBox className="status-scroll" label={t('ui.status')}><SetupStatus t={t} result={result} issue={notice}
        warnings={preferenceWarnings} active={!busy && !loading && !loadError} loading={loading} invalid={invalidInput}
        warnBeforeSave={!!options?.proxy && options.codexEnv} onResult={() => openDialog('result')} onIssue={showError}
        onShowInput={() => { preview.current?.focus({ preventScroll: true }); preview.current?.scrollIntoView({ block: 'nearest' }) }} /></ScrollBox>
      <div className="footer-actions"><Button ref={doneButton} className={`button ${saved ? 'primary-button' : 'secondary-button'}`} disabled={busy} onClick={() => closeRequest.current()}>{t(saved ? 'common.done' : 'common.close')}</Button>
        {dirty && <span className="sr-only" id="unsaved-status">{t('setup.status.unsaved')}</span>}
        {loading ? <span className="skeleton-bar skeleton-save" aria-hidden="true" /> : !saved && !loadError &&
          <Button ref={saveButton} className="button primary-button save-button" data-edited={dirty && !busy || undefined}
            data-validating={!locked && checking && previewValue?.valid || undefined} aria-describedby={dirty ? 'unsaved-status' : undefined}
            disabled={locked || !validation?.valid} onClick={() => void save()}>
            {saving && <LoaderCircle size={16} className="spinner" />}{t(saving ? 'setup.status.saving' : snapshot?.hasSettings ? 'setup.action.save' : 'setup.action.deploy')}
          </Button>}</div>
      <NoticeToasts manager={noticeManager} t={t} onOpen={item => { setSelectedNotice(item); openDialog('notice'); focusReturn.current = doneButton.current }} />
    </footer>
    <Modal open={dialogOpen} title={dialogTitle} variant={dialog === 'help' ? 'help' : undefined} focusReturn={focusReturn} initialFocus={discard ? cancelButton : undefined} onClose={() => { if (!activeOperation.current) setDialogOpen(false) }}
      description={text?.description && t(text.description)}
      actions={discard ? <><Button ref={cancelButton} className="button secondary-button" disabled={busy} onClick={() => setDialogOpen(false)}>{t('ui.keepediting')}</Button><Button className="button danger-button" disabled={busy} onClick={() => void close()}>{t('ui.discard')}</Button></>
        : pickerOpen ? <><Button className="button secondary-button" disabled={busy} onClick={() => setDialogOpen(false)}>{t('download.cancel')}</Button>
          <Button className="button primary-button" disabled={busy || !selection} onClick={() => { if (dialog === 'program') void addProgram(); else { update('directory', selection); setDialogOpen(false) } }}>{adding && <LoaderCircle size={16} className="spinner" />}{t(adding ? 'ui.adding' : 'ui.select')}</Button></>
          : <Button className="button primary-button" onClick={() => setDialogOpen(false)}>{t('ui.understood')}</Button>}>
      {pickerOpen && <RadioGroup className="picker-list" value={selection} onValueChange={value => setSelection(String(value))} disabled={busy} aria-label={dialogTitle}>
        {(dialog === 'program' ? samplePrograms.map(program => ({ path: program.path, name: program.name })) : (service.picker?.directories ?? []).map(path => ({ path, name: path })))
          .map(item => <Radio.Root className="picker-item" key={item.path} value={item.path}><Radio.Indicator keepMounted className="picker-indicator"><CheckCircle2 size={16} /></Radio.Indicator><span>{item.name}</span></Radio.Root>)}
      </RadioGroup>}
      {dialog === 'program-details' && inspectedProgram && <ProgramDetails program={inspectedProgram} t={t} />}
      {dialog === 'result' && result && <>
        {result.warnings?.map(key => <p key={key}>{t(key)}</p>)}
        {!saved || !!result.nativeError ? <ErrorDetails error={result} t={t} /> : result.detail && <p className="error-detail">{result.detail}</p>}
        {result.commit === 'rollback-incomplete' && <p>{t('ui.recovery')}</p>}
        {result.conflictDirectory && <p className="error-detail">{result.conflictDirectory}</p>}
        {result.tasks.map(task => <div className="followup" key={task}><p>{t(task === 'startup' ? 'ui.task.startup' : 'ui.task.restart')}</p>{task === 'startup' && <Button className="control-button" onClick={() => void openLocation('windows')}>{t('ui.openstartup')}<ArrowUpRight size={14} /></Button>}</div>)}
      </>}
      {dialog === 'error' && error && <ErrorDetails error={error} t={t} />}
      {dialog === 'notice' && selectedNotice && <div className="notice-detail">
        <p className="notice-summary">{t(selectedNotice.message, selectedNotice.values)}</p>
        {noticeDetail && <p>{typeof noticeDetail === 'string' ? noticeDetail : t(noticeDetail.message, noticeDetail.values)}</p>}
        {selectedNotice.error && <ErrorDetails error={selectedNotice.error} t={t} />}
      </div>}
    </Modal>
  </div>
}
