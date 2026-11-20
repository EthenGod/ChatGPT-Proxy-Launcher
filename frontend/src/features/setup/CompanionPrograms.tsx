import { useEffect, useLayoutEffect, useReducer, useRef, useState, type ReactNode, type RefObject } from 'react'
import { Tooltip } from '@base-ui/react/tooltip'
import { AppWindow, BadgeCheck, ChevronRight, CircleAlert, FileCode2, FileJson2, FileSymlink, FileTerminal, Folder, FolderOpen, Plus, RefreshCw } from 'lucide-react'
import { Button, Hint, HintPopup } from '../../components/controls'
import { ScrollBox } from '../../components/ScrollBox'
import type { translator } from '../../i18n'
import type { ListedProgram, ProgramList, SetupService } from './contract'
import { nativeIssue, type ErrorInfo } from './errors'

type Translate = ReturnType<typeof translator>
const fileIcons = new Map([['exe', AppWindow], ['lnk', FileSymlink], ['ps1', FileTerminal], ['cmd', FileTerminal], ['bat', FileTerminal], ['json', FileJson2]])

function ProgramHints({ children }: { children: ReactNode }) {
  return <Tooltip.Root<ListedProgram>>{({ payload }) => <>
    {children}
    <HintPopup>{payload && (payload.arguments ? payload.path + '\n' + payload.arguments : payload.path)}</HintPopup>
  </>}</Tooltip.Root>
}

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
  if (status !== 'adapted' && status !== 'adapted-disabled' && status !== 'dependency-blocked') return null
  return <span className="program-badge"><BadgeCheck size={13} aria-hidden="true" /><span data-motion-text>{t('ui.programs.adapted')}</span></span>
}
function ProgramState({ status, t }: StatusProps) {
  if (status === 'disabled' || status === 'adapted-disabled') return <span className="program-state" data-motion-text>{t('ui.programs.disabled')}</span>
  if (status === 'ordinary' || status === 'adapted') return null
  const labels = { invalid: 'ui.programs.invalid', unavailable: 'ui.programs.unavailable', 'dependency-blocked': 'ui.programs.dependency' } as const
  return <span className="program-state" data-warning>
    <CircleAlert size={13} aria-hidden="true" /><span data-motion-text>{t(labels[status])}</span></span>
}

type ListLayout = { tops: Map<string, number>; height: number; added: string | undefined }
const listMotion = { duration: 230, easing: 'cubic-bezier(.22,.61,.36,1)' }

function captureListChange(container: HTMLElement, entries: ListedProgram[], afterAdd: boolean): ListLayout | null {
  // A focus refresh often returns identical entries. Read geometry only for an actual list change.
  const rows = [...container.querySelectorAll<HTMLLIElement>('.program-list > li')]
  if (container.querySelector('.program-placeholder') ||
    (rows.length === entries.length && rows.every((row, i) => row.dataset.programId === entries[i]?.id))) return null
  const origin = rows[0]?.parentElement?.getBoundingClientRect().top ?? 0
  const tops = new Map(rows.map(row => [row.dataset.programId!, row.getBoundingClientRect().top - origin]))
  const additions = afterAdd ? entries.filter(entry => !tops.has(entry.id)) : []
  return { tops, height: container.getBoundingClientRect().height, added: additions.length === 1 ? additions[0]?.id : undefined }
}

function animateListChange(container: HTMLElement, before: ListLayout) {
  const viewport = container.querySelector<HTMLElement>('.scroll-viewport')
  const rows = [...container.querySelectorAll<HTMLLIElement>('.program-list > li')]
  // An immediate second change starts from the currently painted position.
  container.getAnimations?.({ subtree: true }).filter(animation => animation.id === 'program-layout').forEach(animation => animation.cancel())
  const motion = !!container.animate && !window.matchMedia('(prefers-reduced-motion: reduce)').matches &&
    !document.documentElement.matches('[data-window-resizing], [data-appearance-transition]')
  const height = container.getBoundingClientRect().height
  const positions = rows.map(row => ({ row, top: row.offsetTop }))
  if (motion && Math.abs(before.height - height) > 1) {
    container.animate([{ height: before.height + 'px' }, { height: height + 'px' }], { ...listMotion, id: 'program-layout' })
  }
  for (const { row, top } of positions) {
    const id = row.dataset.programId!
    const delta = (before.tops.get(id) ?? top) - top
    if (motion && Math.abs(delta) > 1) {
      row.animate([{ transform: `translateY(${delta}px)` }, { transform: 'none' }], { ...listMotion, id: 'program-layout' })
    }
    if (viewport && id === before.added) {
      const bottom = top + row.offsetHeight
      const target = top < viewport.scrollTop ? top : bottom - viewport.clientHeight
      if (top < viewport.scrollTop || bottom > viewport.scrollTop + viewport.clientHeight) {
        viewport.scrollTo?.({ top: target, behavior: motion ? 'smooth' : 'instant' })
      }
    }
  }
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
  const results = useRef<HTMLDivElement>(null)
  const previousLayout = useRef<ListLayout | null>(null)
  const lastRevision = useRef(revision)
  useEffect(() => {
    if (!active) return
    let current = true
    setPending(true)
    void service.listPrograms().then(result => {
      if (!current) return
      if (results.current) previousLayout.current = captureListChange(results.current, result.entries, lastRevision.current !== revision)
      lastRevision.current = revision
      setList(result)
      setIssue(null)
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
    if (previousLayout.current && results.current) animateListChange(results.current, previousLayout.current)
    previousLayout.current = null
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
    <div ref={results} className="program-results" aria-busy={pending}>
      {entries.length > 0 && <ScrollBox className="program-scroll" label={t('ui.programs')}>
        <ProgramHints><ul className="program-list">{entries.map(program => <li key={program.id} data-program-id={program.id}>
          <Tooltip.Trigger payload={program} delay={500} render={
            <Button className="program-row" data-dependency-blocked={program.status === 'dependency-blocked' || undefined}
              disabled={locked} aria-haspopup="dialog" onClick={() => onDetails(program)}>
              <ProgramIcon program={program} /><span className="program-title"><span className="program-name" dir="auto">{program.name}</span>
                <AdaptedBadge status={program.status} t={t} /></span>
              <ProgramState status={program.status} t={t} /><ChevronRight className="program-chevron" size={14} aria-hidden="true" />
            </Button>} />
        </li>)}</ul></ProgramHints>
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
