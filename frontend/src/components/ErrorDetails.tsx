import type { translator } from '../i18n'
import { errorPresentation, type ErrorInfo } from '../features/setup/errors'

export function ErrorDetails({ error, t }: { error: ErrorInfo; t: ReturnType<typeof translator> }) {
  const info = errorPresentation(error)
  return <div className="error-guide">
    <div className="error-heading"><h3 data-motion-text>{t(info.meaning)}</h3>
      <code>{info.code ? t('ui.error.code', { code: info.code }) : t('ui.error.nocode')}</code></div>
    <p>{t(info.explanation)}</p>
    <p className="error-advice"><strong>{t('ui.error.action')}</strong>{t(info.action)}</p>
    {error.detail && <details><summary>{t('ui.error.technical')}</summary><pre>{error.detail}</pre></details>}
  </div>
}
