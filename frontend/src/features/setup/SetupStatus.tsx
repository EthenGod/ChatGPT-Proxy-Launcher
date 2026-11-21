import { useCallback, useLayoutEffect, useRef, useState } from 'react'
import { CheckCircle2, CircleAlert, Info, TriangleAlert } from 'lucide-react'
import { Button } from '../../components/controls'
import type { MessageKey, translator } from '../../i18n'
import type { SaveResult } from './contract'
import type { ErrorInfo } from './errors'

export type OperationIssue = { message: MessageKey; error: ErrorInfo }
const icons = { info: Info, success: CheckCircle2, warning: TriangleAlert, error: CircleAlert }

type StatusMessage = { kind: 'result' | 'issue' | 'input' | 'reminder' | 'preference'; tone: keyof typeof icons; text: MessageKey }
const order = { result: 0, issue: 1, input: 2, reminder: 2, preference: 3 }
const statusKey = (message: StatusMessage) => [message.kind, message.tone, message.text].join('/')

function StatusLine({ message, present, onExit, onAction, t }: {
  message: StatusMessage; present: boolean; onExit: (key: string) => void; onAction?: () => void; t: ReturnType<typeof translator>
}) {
  const row = useRef<HTMLDivElement>(null)
  const key = statusKey(message)
  useLayoutEffect(() => {
    if (present) return
    const transitions = row.current?.getAnimations?.() ?? []
    if (!transitions.length) { onExit(key); return }
    let exiting = true
    void Promise.allSettled(transitions.map(motion => motion.finished)).then(() => { if (exiting) onExit(key) })
    return () => { exiting = false }
  }, [key, present, onExit])

  const Icon = icons[message.tone]
  const caption = present && message.kind === 'result' ? 'save-result-status' : present && message.kind === 'issue' ? 'operation-status' : undefined
  const details = message.kind === 'issue' || (message.kind === 'result' && message.tone !== 'success')
  return <div ref={row} className="status-presence" data-exiting={!present || undefined} inert={!present} aria-hidden={!present || undefined}>
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
  const current: StatusMessage[] = []
  if (active && result) current.push({ kind: 'result', tone: failed ? 'error' : hasFollowUp ? 'warning' : 'success',
    text: result.message === 'setup.status.saved' ? 'setup.action.saved' : result.message })
  if (active && issue) current.push({ kind: 'issue', tone: 'error', text: issue.message })
  if (actionable && invalid) current.push({ kind: 'input', tone: 'error', text: 'ui.input.review' })
  else if (actionable && warnBeforeSave) current.push({ kind: 'reminder', tone: 'info', text: 'setup.status.quitwarning' })
  for (const text of warningKeys) current.push({ kind: 'preference', tone: 'warning', text })

  // Keep a departing row until its CSS transition finishes. Reappearing keys reuse that same element.
  const [rows, setRows] = useState<StatusMessage[]>(current)
  const added = current.filter(next => !rows.some(row => statusKey(row) === statusKey(next)))
  if (added.length) setRows([...rows, ...added].sort((a, b) => order[a.kind] - order[b.kind]))
  const remove = useCallback((key: string) => setRows(previous => previous.filter(row => statusKey(row) !== key)), [])
  return <div className="footer-status" role={active && (failed || !!issue) ? 'alert' : 'status'}>
    {loading && <span className="sr-only">{t('setup.preview.loading')}</span>}
    {rows.map(row => <StatusLine key={statusKey(row)} message={row} present={current.some(next => statusKey(next) === statusKey(row))}
      onExit={remove} t={t} onAction={row.kind === 'result' ? onResult : row.kind === 'input' ? onShowInput : issue ? () => onIssue(issue.error) : undefined} />)}
  </div>
}
