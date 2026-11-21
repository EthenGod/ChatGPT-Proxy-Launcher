import { useCallback, useLayoutEffect, useRef, useState } from 'react'
import { CheckCircle2, CircleAlert, Info, TriangleAlert } from 'lucide-react'
import { Button } from '../../components/controls'
import type { MessageKey, translator } from '../../i18n'
import type { SaveResult } from './contract'
import type { ErrorInfo } from './errors'

export type OperationIssue = { message: MessageKey; error: ErrorInfo }
const icons = { info: Info, success: CheckCircle2, warning: TriangleAlert, error: CircleAlert }

type StatusMessage = { kind: 'result' | 'issue' | 'input' | 'reminder' | 'preference'; tone: keyof typeof icons; text: MessageKey }
type StatusRow = StatusMessage & { slot: number; phase?: 'reserve' | 'collapse' }
type NextPhase = 'ready' | 'collapse' | 'remove'
const statusKey = (message: StatusMessage) => [message.kind, message.tone, message.text].join('/')

// A short line usually fits beside the buttons and can start fading in immediately.
function fitsFooter(row: HTMLElement) {
  const slot = row.parentElement, scroll = row.closest('.status-scroll'), actions = scroll?.parentElement?.querySelector('.footer-actions')
  if (!slot || !scroll || !actions) return false
  const area = scroll.getBoundingClientRect(), buttons = actions.getBoundingClientRect()
  const spare = area.right <= buttons.left + 1 ? Math.max(0, buttons.height - area.height) : 0
  return row.firstElementChild!.scrollHeight <= slot.getBoundingClientRect().height + spare + 1
}

function StatusLine({ message, present, waiting, replacing, advance, onAction, t }: {
  message: StatusRow; present: boolean; waiting: boolean; replacing: boolean
  advance: (key: string, next: NextPhase) => void; onAction?: () => void; t: ReturnType<typeof translator>
}) {
  const row = useRef<HTMLDivElement>(null)
  const key = statusKey(message)
  const reserving = present && message.phase === 'reserve'
  const collapsing = !present && !!message.phase
  useLayoutEffect(() => {
    if (present && !reserving) return
    const element = row.current!
    const transitions = element.getAnimations?.() ?? []
    const next = present ? 'ready' : collapsing ? 'remove' : 'collapse'
    if (!transitions.length || (reserving && fitsFooter(element))) { advance(key, next); return }
    let current = true
    void Promise.allSettled(transitions.map(motion => motion.finished)).then(() => { if (current) advance(key, next) })
    return () => { current = false }
  }, [key, present, reserving, collapsing, replacing, advance])

  const Icon = icons[message.tone]
  const caption = present && message.kind === 'result' ? 'save-result-status' : present && message.kind === 'issue' ? 'operation-status' : undefined
  const details = message.kind === 'issue' || (message.kind === 'result' && message.tone !== 'success')
  return <div ref={row} className="status-presence" data-exiting={!present || undefined} data-waiting={waiting || undefined}
    data-reserving={reserving || undefined} data-collapsing={collapsing || undefined} inert={!present} aria-hidden={!present || undefined}>
    <div className="status-clip">{message.kind === 'input'
      ? <Button className="text-button status-input-link" aria-describedby="validation-message" onClick={present ? onAction : undefined}>
        <Icon size={14} aria-hidden="true" />{t(message.text)}</Button>
      : <div className="status-line" data-tone={message.tone}>
        <Icon size={14} aria-hidden="true" /><span id={caption} data-motion-text>{t(message.text)}</span>
        {details && <Button className="text-button" aria-describedby={caption} onClick={present ? onAction : undefined}>{t('ui.details')}</Button>}
      </div>}
    </div>
  </div>
}

export function SetupStatus({ t, result, issue, warnings, active, loading, invalid, warnBeforeSave, onResult, onIssue, onShowInput }: {
  t: ReturnType<typeof translator>; result: SaveResult | null; issue: OperationIssue | null; warnings: MessageKey[];
  active: boolean; loading: boolean; invalid: boolean; warnBeforeSave: boolean;
  onResult: () => void; onIssue: (error: ErrorInfo) => void; onShowInput: () => void
}) {
  const failed = !!result && result.commit !== 'saved'
  const hasFollowUp = !!result && (!!result.tasks.length || !!result.warnings?.length || !!result.nativeError)
  const actionable = active && !result && !issue
  const warningKeys = new Set(warnings)
  if (warningKeys.has('setup.language.unsaved') && warningKeys.has('setup.status.themeunsaved')) {
    warningKeys.delete('setup.language.unsaved')
    warningKeys.delete('setup.status.themeunsaved')
    warningKeys.add('ui.preferences.unsaved')
  }
  const current: StatusRow[] = []
  if (active && result) current.push({ slot: 0, kind: 'result', tone: failed ? 'error' : hasFollowUp ? 'warning' : 'success',
    text: result.message === 'setup.status.saved' ? 'setup.action.saved' : result.message })
  if (active && issue) current.push({ slot: result ? 1 : 0, kind: 'issue', tone: 'error', text: issue.message })
  if (actionable && invalid) current.push({ slot: 0, kind: 'input', tone: 'error', text: 'ui.input.review' })
  else if (actionable && warnBeforeSave) current.push({ slot: 0, kind: 'reminder', tone: 'info', text: 'setup.status.quitwarning' })
  let preferenceSlot = 2
  for (const text of warningKeys) current.push({ slot: preferenceSlot++, kind: 'preference', tone: 'warning', text })

  const [rows, setRows] = useState(current)
  const added = current.filter(next => !rows.some(row => statusKey(row) === statusKey(next) && row.slot === next.slot && row.phase !== 'collapse'))
  if (added.length) {
    const keys = new Set(added.map(statusKey))
    const entering: StatusRow[] = added.map(row => ({ ...row, phase: 'reserve' }))
    setRows([...rows.filter(row => !keys.has(statusKey(row))), ...entering].sort((a, b) => a.slot - b.slot))
  }
  const advance = useCallback((key: string, next: NextPhase) => setRows(previous => next === 'remove'
    ? previous.filter(row => statusKey(row) !== key)
    : previous.map(row => statusKey(row) === key ? { ...row, phase: next === 'ready' ? undefined : 'collapse' } : row)), [])
  const slots = new Set(rows.map(row => row.slot))
  return <div className="footer-status" role={active && (failed || !!issue) ? 'alert' : 'status'}>
    {loading && <span className="sr-only">{t('setup.preview.loading')}</span>}
    {[...slots].map(slot => {
      const group = rows.filter(row => row.slot === slot)
      const activeRow = current.find(row => row.slot === slot)
      const activeKey = activeRow && statusKey(activeRow)
      const replacing = !!activeRow && group.some(row => !row.phase && statusKey(row) !== activeKey)
      return <div className="status-slot" key={slot} data-replacing={replacing || undefined}>
        {group.map(row => {
          const present = statusKey(row) === activeKey
          return <StatusLine key={statusKey(row)} message={row} present={present} waiting={present && replacing} replacing={replacing}
            advance={advance} t={t} onAction={row.kind === 'result' ? onResult : row.kind === 'input' ? onShowInput : issue ? () => onIssue(issue.error) : undefined} />
        })}
      </div>
    })}
  </div>
}
