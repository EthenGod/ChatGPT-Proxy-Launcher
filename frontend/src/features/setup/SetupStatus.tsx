import type { ReactNode } from 'react'
import { CheckCircle2, CircleAlert, Info, TriangleAlert } from 'lucide-react'
import { Button } from '../../components/controls'
import type { MessageKey, translator } from '../../i18n'
import type { SaveResult } from './contract'
import type { ErrorInfo } from './errors'

export type OperationIssue = { message: MessageKey; error: ErrorInfo }
const icons = { info: Info, success: CheckCircle2, warning: TriangleAlert, error: CircleAlert }

function StatusLine({ tone, text, id, children }: { tone: keyof typeof icons; text: string; id?: string; children?: ReactNode }) {
  const Icon = icons[tone]
  return <div className="status-line" data-tone={tone}>
    <Icon size={14} aria-hidden="true" /><span id={id} data-motion-text>{text}</span>{children}
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
  return <div className="footer-status" role={active && (failed || !!issue) ? 'alert' : 'status'}>
    {loading && <span className="sr-only">{t('setup.preview.loading')}</span>}
    {active && result && <StatusLine id="save-result-status" tone={failed ? 'error' : hasFollowUp ? 'warning' : 'success'}
      text={t(result.message === 'setup.status.saved' ? 'setup.action.saved' : result.message)}>
      {(failed || hasFollowUp) && <Button className="text-button" aria-describedby="save-result-status" onClick={onResult}>{t('ui.details')}</Button>}
    </StatusLine>}
    {active && issue && <StatusLine id="operation-status" tone="error" text={t(issue.message)}>
      <Button className="text-button" aria-describedby="operation-status" onClick={() => onIssue(issue.error)}>{t('ui.details')}</Button>
    </StatusLine>}
    {actionable && invalid && <Button className="text-button status-input-link" aria-describedby="validation-message" onClick={onShowInput}>
      <CircleAlert size={14} aria-hidden="true" />{t('ui.input.review')}
    </Button>}
    {actionable && !invalid && warnBeforeSave && <StatusLine tone="info" text={t('setup.status.quitwarning')} />}
    {[...warningKeys].map(key => <StatusLine key={key} tone="warning" text={t(key)} />)}
  </div>
}
