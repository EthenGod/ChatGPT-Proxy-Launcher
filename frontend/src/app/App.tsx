import { useEffect, useState } from 'react'
import { Button } from '@base-ui/react/button'
import { CheckCircle2, RefreshCw } from 'lucide-react'
import { desktop } from '../platform/desktop'
import type { SetupSnapshot } from '../platform/types'
import { translator } from '../i18n'

export function App() {
  const [snapshot, setSnapshot] = useState<SetupSnapshot | null>(null)
  const [error, setError] = useState('')
  const [loading, setLoading] = useState(true)
  const t = translator(snapshot?.preferences.resolvedLanguage ?? 'zh-CN')

  async function load() {
    setLoading(true)
    setError('')
    try { setSnapshot(await desktop.load()) }
    catch (cause) {
      const message = cause && typeof cause === 'object' && 'message' in cause ? String(cause.message) : ''
      setError(message || t('setup.error.readstate'))
    } finally { setLoading(false) }
  }

  useEffect(() => { void load() }, [])
  useEffect(() => {
    document.documentElement.classList.toggle('light', snapshot?.preferences.dark === false)
    document.documentElement.classList.toggle('dark', snapshot?.preferences.dark !== false)
    document.documentElement.lang = snapshot?.preferences.resolvedLanguage ?? 'zh-CN'
    if (!loading) void desktop.ready().catch(() => setError(t('setup.error.webviewstart')))
  }, [loading, snapshot])

  const deployment = snapshot?.deployed
    ? snapshot.deployedVersion ? t('setup.state.version', { version: snapshot.deployedVersion }) : t('setup.state.unknownversion')
    : snapshot?.hasSettings ? t('setup.state.missing') : t('setup.state.absent')

  return <div className="app-shell">
    <header>
      <h1>{t('common.launcher')}</h1>
      {snapshot && <span className="secondary small">{t('setup.version', { version: snapshot.setupVersion })}</span>}
    </header>
    <main aria-busy={loading}>
      <p className="preview-note">{t('setup.preview.readonly')}</p>
      {error && <div className="error" role="alert">{error}</div>}
      {snapshot && <>
        <section className="card">
          <div className="status"><CheckCircle2 size={18} aria-hidden="true" /><strong>{deployment}</strong></div>
          {snapshot.notice && <p className="secondary">{snapshot.notice}</p>}
        </section>
        <section className="card">
          <h2>{t('setup.section.startup')}</h2>
          <dl>
            <div><dt>{t('setup.option.startup')}</dt><dd>{t(snapshot.options.startup ? 'setup.preview.on' : 'setup.preview.off')}</dd></div>
            <div><dt>{t('setup.option.desktop')}</dt><dd>{t(snapshot.options.desktop ? 'setup.preview.on' : 'setup.preview.off')}</dd></div>
            <div><dt>{t('setup.option.proxy')}</dt><dd>{t(snapshot.options.proxy ? 'setup.preview.on' : 'setup.preview.off')}</dd></div>
          </dl>
        </section>
      </>}
      {loading && <p role="status" className="secondary">{t('setup.preview.loading')}</p>}
    </main>
    <footer>
      <Button className="button secondary-button" disabled={loading} onClick={() => void load()}>
        <RefreshCw size={16} aria-hidden="true" />{t('setup.action.refresh')}
      </Button>
      <Button className="button primary-button" onClick={() => void desktop.close()}>{t('common.close')}</Button>
    </footer>
  </div>
}
