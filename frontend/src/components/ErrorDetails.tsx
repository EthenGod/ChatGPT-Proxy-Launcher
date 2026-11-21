import { observeTextReflow, observeWrappedRow } from './textReflow'
import { ChevronRight } from 'lucide-react'
import type { translator } from '../i18n'
import { errorPresentation, type ConflictLocation, type ErrorInfo } from '../features/setup/errors'
import { Button } from './controls'

export function ErrorDetails({ error, t, disabled, onOpenConflict }: {
  error: ErrorInfo; t: ReturnType<typeof translator>; disabled?: boolean; onOpenConflict?: (location: ConflictLocation) => void
}) {
  const info = errorPresentation(error)
  const location = error.conflictLocation
  return <div className="error-guide">
    <div ref={observeWrappedRow} className="error-heading"><h3 ref={observeTextReflow} data-motion-text>{t(info.meaning)}</h3>
      <code>{info.code ? t('ui.error.code', { code: info.code }) : t('ui.error.nocode')}</code></div>
    <p ref={observeTextReflow}>{t(info.explanation)}</p>
    <p className="error-advice"><strong>{t('ui.error.action')}</strong>{t(info.action)}</p>
    {error.conflictDirectory && <><p ref={observeTextReflow} className="error-detail">{error.conflictDirectory}</p>
      {location && onOpenConflict && <Button className="button secondary-button" disabled={disabled}
        onClick={() => onOpenConflict(location)}>{t('ui.openconflictfolder')}</Button>}</>}
    {error.detail && <details><summary><ChevronRight size={14} aria-hidden="true" /><span data-motion-text>{t('ui.error.technical')}</span></summary><pre>{error.detail}</pre></details>}
  </div>
}
