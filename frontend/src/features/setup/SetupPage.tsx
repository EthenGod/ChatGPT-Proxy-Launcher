import { useCallback, useEffect, useRef, useState } from 'react'
import { Collapsible } from '@base-ui/react/collapsible'
import { Radio } from '@base-ui/react/radio'
import { RadioGroup } from '@base-ui/react/radio-group'
import { AlertCircle, ArrowUpRight, CheckCircle2, ChevronDown, CircleHelp, File, FlaskConical,
  FolderOpen, Globe2, LoaderCircle, Moon, Plus, Search, Sun } from 'lucide-react'
import { Button, ChoiceMenu, Modal, SwitchControl, TextField, Toggle } from '../../components/controls'
import { resolveLanguage, translator, type MessageKey } from '../../i18n'
import type { SetupOptions, SetupSnapshot } from '../../platform/types'
import { defaultDirectory, samplePrograms } from '../../platform/mock'
import { previewOptions, sameOptions } from './preview'
import type { ProgramEntry, SaveResult, SetupService, WindowHost } from './contract'

type DialogState = 'discard' | 'help' | 'directory' | 'program' | 'result' | 'preview' | null
export function SetupPage({ service, host, scenarioControls, onLanguageChange }: {
  service: SetupService; host: WindowHost; scenarioControls?: React.ReactNode; onLanguageChange?: (language: string) => void
}) {
  const [snapshot, setSnapshot] = useState<SetupSnapshot | null>(null)
  const [options, setOptions] = useState<SetupOptions | null>(null)
  const [language, setLanguage] = useState('auto')
  const [dark, setDark] = useState(true)
  const [loading, setLoading] = useState(true)
  const [loadError, setLoadError] = useState(false)
  const [busy, setBusy] = useState(false)
  const [adding, setAdding] = useState(false)
  const [detecting, setDetecting] = useState(false)
  const [advanced, setAdvanced] = useState(false)
  const [result, setResult] = useState<SaveResult | null>(null)
  const [notice, setNotice] = useState<MessageKey | null>(null)
  const [programs, setPrograms] = useState<ProgramEntry[]>([])
  const [dialog, setDialog] = useState<DialogState>(null)
  const [dialogOpen, setDialogOpen] = useState(false)
  const [help, setHelp] = useState<MessageKey>('ui.help.proxy')
  const [selection, setSelection] = useState('')
  const [closed, setClosed] = useState(false)
  const alive = useRef(true)
  const inFlight = useRef(false)
  const focusAfterSave = useRef(false)
  const detectRevision = useRef(0)
  const loadRevision = useRef(0)
  const closeRequest = useRef<() => void>(() => {})
  const focusReturn = useRef<HTMLElement | null>(null)
  const cancelButton = useRef<HTMLButtonElement | null>(null)
  const saveButton = useRef<HTMLButtonElement | null>(null)
  const doneButton = useRef<HTMLButtonElement | null>(null)
  const resolved = resolveLanguage(language)
  const t = translator(resolved)
  const dirty = !!options && !!snapshot && !sameOptions(options, snapshot.options)
  const validation = options ? previewOptions(options) : null
  const locked = busy || loading || result?.commit === 'rollback-incomplete'

  const openDialog = (next: Exclude<DialogState, null>) => {
    focusReturn.current = document.activeElement as HTMLElement | null
    setDialog(next); setDialogOpen(true)
  }
  async function close() {
    if (inFlight.current) return
    try { await host.close(); if (alive.current) { setDialogOpen(false); setClosed(true) } }
    catch { if (alive.current) setNotice('ui.close.failed') }
  }
  closeRequest.current = () => {
    if (inFlight.current) return
    if (dialogOpen) { if (dialog !== 'discard') setDialogOpen(false); return }
    if (dirty) openDialog('discard')
    else void close()
  }
  const load = useCallback(async () => {
    const revision = ++loadRevision.current
    setLoading(true); setLoadError(false)
    try {
      const data = await service.load()
      if (!alive.current || revision !== loadRevision.current) return
      setSnapshot(data); setOptions({ ...data.options }); setLanguage(data.preferences.language)
      setDark(data.preferences.dark); setResult(null); setNotice(null)
    } catch { if (alive.current && revision === loadRevision.current) setLoadError(true) }
    finally { if (alive.current && revision === loadRevision.current) setLoading(false) }
  }, [service])
  useEffect(() => {
    alive.current = true
    let disconnect: (() => void) | undefined
    void host.connect(() => closeRequest.current()).then(remove => {
      if (!alive.current) remove()
      else { disconnect = remove; return host.ready() }
    }).catch(() => { if (alive.current) setNotice('ui.close.failed') })
    void load()
    return () => { alive.current = false; ++detectRevision.current; ++loadRevision.current; disconnect?.() }
  }, [host, load])
  useEffect(() => {
    document.documentElement.classList.toggle('dark', dark)
    document.documentElement.classList.toggle('light', !dark)
    document.documentElement.lang = resolved
    onLanguageChange?.(resolved)
  }, [dark, resolved, onLanguageChange])
  useEffect(() => {
    const beforeUnload = (event: BeforeUnloadEvent) => { if (dirty || inFlight.current) { event.preventDefault(); event.returnValue = '' } }
    window.addEventListener('beforeunload', beforeUnload)
    return () => window.removeEventListener('beforeunload', beforeUnload)
  }, [dirty])

  function update<K extends keyof SetupOptions>(key: K, value: SetupOptions[K]) {
    if (locked || !options) return
    if (['host', 'port', 'protocol', 'proxy'].includes(key)) { ++detectRevision.current; setDetecting(false) }
    setOptions({ ...options, [key]: value }); setResult(null); setNotice(null)
  }
  async function detect() {
    if (!options || detecting || locked || !options.proxy) return
    const revision = ++detectRevision.current
    setDetecting(true); setNotice(null); setResult(null)
    try {
      const port = await service.detectPort(options.protocol, options.host)
      if (!alive.current || revision !== detectRevision.current || inFlight.current) return
      if (port) setOptions(current => current ? { ...current, port } : current)
      setNotice(port ? 'ui.detect.found' : 'ui.detect.empty')
    } catch { if (alive.current && revision === detectRevision.current) setNotice('ui.detect.failed') }
    finally { if (alive.current && revision === detectRevision.current) setDetecting(false) }
  }
  async function save() {
    if (!options || !snapshot || !validation?.valid || locked || inFlight.current || (snapshot.deployed && !dirty && result?.commit === 'saved')) return
    inFlight.current = true; focusAfterSave.current = true; ++detectRevision.current; setDetecting(false); setBusy(true); setNotice(null); setResult(null)
    const submitted = { ...options }
    try {
      const saved = await service.save(submitted)
      if (!alive.current) return
      setResult(saved)
      if (saved.commit === 'saved') {
        setSnapshot(current => current ? { ...current, options: submitted, deployed: true, hasSettings: true, deployedVersion: current.setupVersion } : current)
        if (saved.tasks.length) {
          focusReturn.current = doneButton.current
          setDialog('result'); setDialogOpen(true)
        }
      }
    } catch {
      // A lost response cannot establish whether writes happened.
      if (alive.current) setResult({ commit: 'rollback-incomplete', message: 'ui.save.unknown', tasks: [] })
    } finally {
      inFlight.current = false
      if (alive.current) setBusy(false)
    }
  }
  useEffect(() => {
    if (!busy && result && !dialogOpen && focusAfterSave.current) {
      focusAfterSave.current = false
      const target = result.commit === 'saved' ? doneButton : saveButton
      target.current?.focus()
    }
  }, [busy, result, dialogOpen])
  async function addProgram() {
    const entry = samplePrograms.find(program => program.path === selection)
    if (!entry || inFlight.current) return
    inFlight.current = true; setBusy(true); setAdding(true)
    try {
      await service.addProgram(entry)
      if (!alive.current) return
      setPrograms(current => current.some(p => p.path === entry.path) ? current : [...current, { ...entry }])
      setNotice('setup.status.added'); setDialogOpen(false)
    } catch { if (alive.current) setNotice('setup.error.add') }
    finally { inFlight.current = false; if (alive.current) { setBusy(false); setAdding(false) } }
  }
  async function openLocation(location: 'startup' | 'deployment' | 'windows') {
    try { await service.openLocation(location); if (alive.current) setNotice('ui.location.preview') }
    catch { if (alive.current) setNotice('setup.error.openfolder') }
  }
  const helpButton = (key: MessageKey, label: string) => <Button className="icon-button help-button" aria-label={label} title={label} onClick={() => { setHelp(key); openDialog('help') }} disabled={busy}><CircleHelp size={16} /></Button>
  const fieldError = (key: keyof SetupOptions) => validation?.errors[key] ? t(validation.errors[key]) : undefined
  const status = busy || loading || loadError ? '' : result ? t(result.commit === 'saved' && !result.tasks.length ? 'setup.action.saved' : result.message)
    : notice ? t(notice) : !validation?.valid && options ? t('ui.save.invalid')
      : options?.proxy && options.codexEnv ? t('setup.status.quitwarning') : dirty ? t('setup.status.unsaved') : ''
  const saved = result?.commit === 'saved' && !dirty
  const failed = !!result && result.commit !== 'saved'
  const deployment = snapshot?.deployed ? snapshot.deployedVersion ? t('setup.state.version', { version: snapshot.deployedVersion }) : t('setup.state.unknownversion')
    : t(snapshot?.hasSettings ? 'setup.state.missing' : 'setup.state.absent')
  const discard = dialog === 'discard'
  const dialogTitle = t(discard ? 'setup.dialog.close' : dialog === 'help' ? 'ui.help.title' : dialog === 'directory' ? 'setup.dialog.directory'
    : dialog === 'program' ? 'setup.dialog.program' : dialog === 'preview' ? 'ui.prototype.title' : saved ? 'ui.save.followup' : 'setup.error.saving')

  if (closed) return <div className="prototype-closed"><CheckCircle2 size={24} /><h1>{t('ui.closed')}</h1><p>{t('ui.prototype.description')}</p>
    <Button className="button primary-button" onClick={() => { setClosed(false); void load() }}>{t('ui.reopen')}</Button></div>

  return <div className="setup-shell">
    <div className="prototype-bar"><Button className="prototype-label" onClick={() => openDialog('preview')} disabled={busy}><FlaskConical size={13} />{t('ui.prototype.title')}</Button><fieldset disabled={busy}>{scenarioControls}</fieldset></div>
    <main className="settings-content" aria-busy={loading}>
      <header className="settings-header"><div className="heading-block"><h1>{t('common.launcher')}</h1>
        <span className="version-badge" aria-label={t('setup.version', { version: snapshot?.setupVersion ?? '1.1.3' })} title={t('setup.version', { version: snapshot?.setupVersion ?? '1.1.3' })}>v{snapshot?.setupVersion ?? '1.1.3'}</span></div>
        <div className="appearance-controls"><ChoiceMenu label={t('ui.language')} value={language} onChange={setLanguage} disabled={busy}
          items={[{ value: 'auto', label: t('setup.language.system') }, { value: 'zh-CN', label: '简体中文' }, { value: 'en-US', label: 'English' }, { value: 'ja-JP', label: '日本語' }]} icon={<Globe2 size={16} />} variant="icon" />
          <Button className="icon-button" onClick={() => setDark(!dark)} disabled={busy} aria-label={t(dark ? 'setup.theme.light' : 'setup.theme.dark')} title={t(dark ? 'setup.theme.light' : 'setup.theme.dark')}>{dark ? <Sun size={16} /> : <Moon size={16} />}</Button>
        </div>
      </header>
      {loading ? <div className="load-state" role="status"><LoaderCircle className="spinner" size={20} />{t('setup.preview.loading')}</div>
        : loadError ? <div className="load-state error-state" role="alert"><AlertCircle size={20} /><p>{t('ui.load.failed')}</p><Button className="button secondary-button" onClick={() => void load()}>{t('ui.retry')}</Button></div>
        : options && snapshot && <>
          <div className="deployment-status"><span className={`status-dot ${snapshot.deployed ? 'is-deployed' : ''}`} /><span>{deployment}</span></div>
          <section className="settings-section" aria-labelledby="startup-title"><h2 id="startup-title">{t('setup.section.startup')}</h2><div className="settings-card">
            <Toggle label={t('ui.startup')} checked={options.startup} disabled={locked} onChange={value => update('startup', value)} help={helpButton('setup.hint.startup', t('ui.help.startup'))} />
            <Toggle label={t('ui.desktop')} checked={options.desktop} disabled={locked} onChange={value => update('desktop', value)} />
            <Toggle label={t('ui.silent')} badge={t('ui.unavailable')} checked={options.silent} disabled onChange={() => {}} />
          </div></section>
          <section className="settings-section" aria-labelledby="proxy-title"><Collapsible.Root open={options.proxy}>
            <div className="section-heading"><h2 id="proxy-title"><label htmlFor="proxy-enabled">{t('ui.proxy')}</label></h2>{helpButton('ui.help.proxy', t('setup.dialog.proxy'))}
              <SwitchControl id="proxy-enabled" label={t('setup.option.proxy')} controls="proxy-details" checked={options.proxy} disabled={locked} onChange={value => update('proxy', value)} /></div>
            <Collapsible.Panel id="proxy-details" className="disclosure-panel">
              <div className="settings-card proxy-card">
                <div className="connection-fields"><div className="field protocol-field"><span id="protocol-label">{t('setup.label.protocol')}</span>
                  <RadioGroup aria-labelledby="protocol-label" className="segmented" value={options.protocol} disabled={locked} onValueChange={value => update('protocol', String(value))}>
                    {['http', 'https', 'socks5'].map(protocol => <Radio.Root key={protocol} value={protocol} className="segment">{protocol.toUpperCase()}</Radio.Root>)}
                  </RadioGroup></div>
                  <TextField id="proxy-host" label={t('setup.label.host')} value={options.host} onChange={event => update('host', event.target.value)} disabled={locked} error={fieldError('host')} autoComplete="off" spellCheck={false} />
                  <TextField id="proxy-port" className="port-field" label={t('setup.label.port')} value={options.port} onChange={event => update('port', event.target.value)} disabled={locked} error={fieldError('port')} inputMode="numeric" maxLength={5} autoComplete="off"
                    suffix={<Button className="input-action" aria-label={t(detecting ? 'ui.detect.running' : 'ui.detect')} title={t('ui.detect')} disabled={locked || detecting} onClick={() => void detect()}>{detecting ? <LoaderCircle className="spinner" size={16} /> : <Search size={16} />}</Button>} />
                </div>
                <Toggle label={t('ui.codex')} checked={options.codexEnv} disabled={locked} onChange={value => update('codexEnv', value)} help={helpButton('ui.help.codex', t('setup.help.codex'))} />
              </div>
            </Collapsible.Panel></Collapsible.Root></section>
          <section className="settings-section" aria-labelledby="preview-title"><h2 id="preview-title">{t('ui.arguments')}</h2>
            <pre className={`argument-preview ${!validation?.valid ? 'invalid-preview' : ''}`} aria-label={t('ui.arguments')}>{validation?.valid ? validation.arguments || t('ui.arguments.empty') : t('setup.preview.invalid')}</pre>
          </section>
          <Collapsible.Root className="advanced-section" open={advanced} onOpenChange={setAdvanced}>
            <Collapsible.Trigger className="advanced-trigger" disabled={busy}>{t('setup.section.advanced')}<ChevronDown size={16} /></Collapsible.Trigger>
            <Collapsible.Panel className="disclosure-panel advanced-panel">
              <div className="settings-card advanced-card">
                <TextField label={t('setup.label.directory')} value={options.directory} onChange={event => update('directory', event.target.value)} disabled={locked} error={fieldError('directory')} spellCheck={false}
                  labelActions={<>{helpButton('setup.hint.directory', t('ui.help.directory'))}<Button className="text-button field-reset" disabled={locked || options.directory === defaultDirectory} onClick={() => update('directory', defaultDirectory)}>{t('setup.action.reset')}</Button></>}
                  suffix={<Button className="input-action" aria-label={t('setup.action.browse')} title={t('setup.action.browse')} disabled={locked} onClick={() => { setSelection(options.directory); openDialog('directory') }}><FolderOpen size={16} /></Button>} />
                <TextField label={t('ui.bypass')} value={options.bypass} onChange={event => update('bypass', event.target.value)} disabled={locked} error={fieldError('bypass')} placeholder="localhost;*.example.com" spellCheck={false} />
                <TextField label={t('ui.extra')} value={options.extra} onChange={event => update('extra', event.target.value)} disabled={locked} error={fieldError('extra')} spellCheck={false} />
                <div className="programs-group"><div className="programs-heading"><h2>{t('ui.programs')}</h2>{helpButton('ui.help.programs', t('ui.help.programs.title'))}</div>
                {programs.length > 0 && <ul className="program-list">{programs.map(program => <li key={program.path} title={`${program.path}\n${program.arguments}`}><File size={16} /><span>{program.name}</span><CheckCircle2 size={14} /></li>)}</ul>}
                <div className="field-actions"><Button className="control-button" disabled={locked} onClick={() => { setSelection(samplePrograms[0]!.path); openDialog('program') }}><Plus size={16} />{t('setup.action.add')}</Button>
                  <Button className="icon-button" disabled={locked} aria-label={t('setup.action.openstartup')} title={t('setup.action.openstartup')} onClick={() => void openLocation('startup')}><FolderOpen size={16} /></Button></div></div>
              </div>
            </Collapsible.Panel>
          </Collapsible.Root>
        </>}
    </main>
    <footer className="settings-footer">
      <div className={`footer-status ${failed ? 'error-text' : saved ? 'success-text' : ''}`} role={failed ? 'alert' : 'status'}>
        {failed ? <AlertCircle size={16} /> : saved ? <CheckCircle2 size={16} /> : null}
        <span>{status}</span>
        {result && (failed || result.tasks.length > 0) && <Button className="text-button" onClick={() => openDialog('result')}>{t('ui.details')}</Button>}
      </div>
      <div className="footer-actions"><Button ref={doneButton} className={`button ${saved ? 'primary-button' : 'secondary-button'}`} disabled={busy} onClick={() => closeRequest.current()}>{t(saved ? 'common.done' : 'common.close')}</Button>
        {!saved && <Button ref={saveButton} className="button primary-button" disabled={locked || !validation?.valid || loadError} onClick={() => void save()}>{busy && !adding && <LoaderCircle size={16} className="spinner" />}{t(busy && !adding ? 'setup.status.saving' : snapshot?.hasSettings ? 'setup.action.save' : 'setup.action.deploy')}</Button>}</div>
    </footer>
    <Modal open={dialogOpen} title={dialogTitle} focusReturn={focusReturn} initialFocus={discard ? cancelButton : undefined} onClose={() => { if (!inFlight.current) setDialogOpen(false) }}
      description={discard ? t('setup.dialog.discard') : dialog === 'help' ? t(help) : dialog === 'preview' ? t('ui.prototype.description')
        : dialog === 'directory' || dialog === 'program' ? t('ui.picker.preview') : result && !saved ? t(result.message) : undefined}
      actions={discard ? <><Button ref={cancelButton} className="button secondary-button" onClick={() => setDialogOpen(false)}>{t('ui.keepediting')}</Button><Button className="button danger-button" onClick={() => void close()}>{t('ui.discard')}</Button></>
        : dialog === 'directory' || dialog === 'program' ? <><Button className="button secondary-button" disabled={busy} onClick={() => setDialogOpen(false)}>{t('download.cancel')}</Button>
          <Button className="button primary-button" disabled={busy || !selection} onClick={() => { if (dialog === 'program') void addProgram(); else { update('directory', selection); setDialogOpen(false) } }}>{adding && <LoaderCircle size={16} className="spinner" />}{t(adding ? 'ui.adding' : 'ui.select')}</Button></>
          : <Button className="button primary-button" onClick={() => setDialogOpen(false)}>{t('ui.understood')}</Button>}>
      {(dialog === 'directory' || dialog === 'program') && <RadioGroup className="picker-list" value={selection} onValueChange={value => setSelection(String(value))} disabled={busy} aria-label={dialogTitle}>
        {(dialog === 'program' ? samplePrograms.map(program => ({ path: program.path, name: program.name })) : [defaultDirectory, 'D:\\Tools\\ChatGPT Launcher', 'C:\\Apps\\工作工具\\ChatGPT Launcher'].map(path => ({ path, name: path })))
          .map(item => <Radio.Root className="picker-item" key={item.path} value={item.path}><Radio.Indicator keepMounted className="picker-indicator"><CheckCircle2 size={16} /></Radio.Indicator><span>{item.name}</span></Radio.Root>)}
      </RadioGroup>}
      {dialog === 'result' && result && <>
        {result.detail && <p className="error-detail">{result.detail}</p>}
        {result.commit === 'rollback-incomplete' && <p>{t('ui.recovery')}</p>}
        {result.tasks.map(task => <div className="followup" key={task}><p>{t(task === 'startup' ? 'ui.task.startup' : 'ui.task.restart')}</p>{task === 'startup' && <Button className="control-button" onClick={() => void openLocation('windows')}>{t('ui.openstartup')}<ArrowUpRight size={14} /></Button>}</div>)}
        {notice === 'ui.location.preview' && <p className="secondary small" role="status">{t(notice)}</p>}
      </>}
    </Modal>
  </div>
}
