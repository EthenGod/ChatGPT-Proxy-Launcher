import { useEffect, useLayoutEffect, useReducer, useRef, useState, type ReactNode, type RefObject } from 'react'
import { AppWindow, BadgeCheck, ChevronRight, CircleAlert, FileCode2, FileJson2, FileSymlink, FileTerminal, Folder, FolderOpen, Plus, RefreshCw } from 'lucide-react'
import { Button, Hint } from '../../components/controls'
import { ScrollBox } from '../../components/ScrollBox'
import type { translator } from '../../i18n'
import type { ListedProgram, ProgramList, SetupService } from './contract'
import { nativeIssue, type ErrorInfo } from './errors'

type Translate = ReturnType<typeof translator>
const fileIcons = new Map([['exe', AppWindow], ['lnk', FileSymlink], ['ps1', FileTerminal], ['cmd', FileTerminal], ['bat', FileTerminal], ['json', FileJson2]])

function ProgramIcon({ program }: { program: ListedProgram }) {
  const [failed, setFailed] = useState<string>()
  const image = program.icon && program.icon !== failed ? program.icon : undefined
  const extension = program.path.slice(program.path.lastIndexOf('.') + 1).toLowerCase()
  const Icon = fileIcons.get(extension) ?? FileCode2
  return <span className="program-icon" data-image={image ? '' : undefined} aria-hidden="true">
    {image ? <img src={image} alt="" draggable={false} onError={() => setFailed(image)} /> : <Icon size={16} />}
  </span>
}

type StatusProps = { status: ListedProgram['status']; t: Translate }
function AdaptedBadge({ status, t }: StatusProps) {
  if (status !== 'adapted' && status !== 'adapted-disabled') return null
  return <span className="program-badge"><BadgeCheck size={13} aria-hidden="true" /><span data-motion-text>{t('ui.programs.adapted')}</span></span>
}
function ProgramState({ status, t }: StatusProps) {
  if (status === 'disabled' || status === 'adapted-disabled') return <span className="program-state" data-motion-text>{t('ui.programs.disabled')}</span>
  if (status === 'invalid' || status === 'unavailable') return <span className="program-state" data-warning>
    <CircleAlert size={13} aria-hidden="true" /><span data-motion-text>{t(status === 'invalid' ? 'ui.programs.invalid' : 'ui.programs.unavailable')}</span></span>
  return null
}

export function ProgramDetails({ program, t }: { program: ListedProgram; t: Translate }) {
  return <div className="program-details">
    <div className="program-identity"><ProgramIcon program={program} /><strong dir="auto">{program.name}</strong><AdaptedBadge status={program.status} t={t} /></div>
    <ProgramState status={program.status} t={t} />
    <dl className="program-properties">
      <div><dt>{t('ui.programs.path')}</dt><dd dir="auto">{program.path}</dd></div>
      {program.arguments && <div><dt>{t('ui.arguments')}</dt><dd dir="auto">{program.arguments}</dd></div>}
    </dl>
    {(program.status === 'invalid' || program.status === 'unavailable') && <p className="program-advice">
      {t(program.status === 'invalid' ? 'ui.programs.repair.registration' : 'ui.programs.repair.shortcut')}</p>}
  </div>
}

export function CompanionPrograms({ service, active, revision, locked, help, t, reveal, onAdd, onOpen, onDetails, onError }: {
  service: SetupService; active: boolean; revision: number; locked: boolean; help: ReactNode; t: Translate
  reveal: RefObject<boolean>; onAdd: () => void; onOpen: () => void; onDetails: (program: ListedProgram) => void; onError: (error: ErrorInfo) => void
}) {
  const [list, setList] = useState<ProgramList | null>(null)
  const [issue, setIssue] = useState<ErrorInfo | null>(null)
  const [pending, setPending] = useState(false)
  const [request, refresh] = useReducer(n => n + 1, 0)
  useEffect(() => {
    if (!active) return
    let current = true
    setPending(true)
    void service.listPrograms().then(result => {
      if (current) { setList(result); setIssue(null) }
    }).catch(cause => { if (current) setIssue(nativeIssue(cause)) })
      .finally(() => { if (current) setPending(false) })
    return () => { current = false }
  }, [service, active, revision, request])
  useEffect(() => {
    if (!active || locked || pending) return
    window.addEventListener('focus', refresh)
    return () => window.removeEventListener('focus', refresh)
  }, [active, locked, pending])

  const section = useRef<HTMLElement>(null)
  useLayoutEffect(() => {
    if (!reveal.current || (!list && !issue)) return
    reveal.current = false
    section.current?.scrollIntoView?.({ block: 'nearest' })
  }, [reveal, list, issue])

  const entries = list?.entries ?? []
  return <section ref={section} className="programs-group" aria-labelledby="programs-title">
    <div className="programs-heading">
      <div className="programs-label"><h2 id="programs-title">{t('ui.programs')}</h2>{help}</div>
      <div className="program-actions">
        <Hint label={t('ui.programs.refresh')}><Button className="icon-button program-refresh" disabled={locked || pending}
          focusableWhenDisabled={pending && !locked} aria-label={t('ui.programs.refresh')} aria-busy={pending} onClick={refresh}>
          <RefreshCw size={15} style={{ transform: 'rotate(' + request * 180 + 'deg)' }} /></Button></Hint>
        <Hint label={t('setup.action.openstartup')}><Button className="icon-button" disabled={locked} aria-label={t('setup.action.openstartup')} onClick={onOpen}><FolderOpen size={16} /></Button></Hint>
        <Button className="control-button program-add" disabled={locked} onClick={onAdd}><Plus size={15} />{t('setup.action.add')}</Button>
      </div>
    </div>
    <div className="program-results" aria-busy={pending}>
      {entries.length > 0 && <ScrollBox className="program-scroll" label={t('ui.programs')}>
        <ul className="program-list">{entries.map(program => <li key={program.id}>
          <Hint label={[program.path, program.arguments].filter(Boolean).join('\n')}>
            <Button className="program-row" disabled={locked} aria-haspopup="dialog" onClick={() => onDetails(program)}>
              <ProgramIcon program={program} /><span className="program-title"><span className="program-name" dir="auto">{program.name}</span>
                <AdaptedBadge status={program.status} t={t} /></span>
              <ProgramState status={program.status} t={t} /><ChevronRight className="program-chevron" size={14} aria-hidden="true" />
            </Button>
          </Hint>
        </li>)}</ul>
      </ScrollBox>}
      {!list && !issue && <div className="program-placeholder" aria-hidden="true">{[0, 1].map(i => <div key={i}>
        <span className="skeleton-bar program-skeleton-icon" /><span className="skeleton-bar skeleton-label" /></div>)}</div>}
      {list && !entries.length && !issue && !list.incomplete && <p className="program-empty"><Folder size={18} aria-hidden="true" /><span data-motion-text>{t('ui.programs.empty')}</span></p>}
      {issue ? <div className="program-message" role="status"><CircleAlert size={15} aria-hidden="true" /><span data-motion-text>{t('ui.programs.failed')}</span>
        <Button className="text-button" onClick={() => onError(issue)}>{t('ui.details')}</Button></div>
        : list?.incomplete && <p className="program-message" role="status"><CircleAlert size={15} aria-hidden="true" /><span data-motion-text>{t('ui.programs.partial')}</span></p>}
    </div>
  </section>
}
