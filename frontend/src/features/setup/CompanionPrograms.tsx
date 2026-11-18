import { useEffect, useReducer, useState, type ReactNode } from 'react'
import { BadgeCheck, File, FolderOpen, Plus, RefreshCw } from 'lucide-react'
import { Button, Hint } from '../../components/controls'
import { ScrollBox } from '../../components/ScrollBox'
import type { translator } from '../../i18n'
import type { ProgramList, SetupService } from './contract'
import { nativeIssue, type ErrorInfo } from './errors'

export function CompanionPrograms({ service, active, revision, locked, help, t, onAdd, onOpen, onError }: {
  service: SetupService; active: boolean; revision: number; locked: boolean; help: ReactNode; t: ReturnType<typeof translator>
  onAdd: () => void; onOpen: () => void; onError: (error: ErrorInfo) => void
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

  return <section className="programs-group" aria-labelledby="programs-title">
    <div className="programs-heading"><h2 id="programs-title">{t('ui.programs')}</h2>{help}
      <Hint label={t('ui.programs.refresh')}><Button className="icon-button program-refresh" disabled={locked || pending}
        focusableWhenDisabled={pending && !locked} aria-label={t('ui.programs.refresh')} onClick={refresh}><RefreshCw size={15} /></Button></Hint>
    </div>
    <div className="program-results" aria-busy={pending}>
      {!!list?.entries.length && <ScrollBox className="program-scroll" label={t('ui.programs')}>
        <ul className="program-list">{list.entries.map(program => {
          const adapted = program.status === 'adapted' || program.status === 'adapted-disabled'
          const disabled = program.status === 'disabled' || program.status === 'adapted-disabled'
          return <li key={program.id}><Hint label={[program.path, program.arguments].filter(Boolean).join('\n')}>
            <span className="program-row" tabIndex={0}><File size={15} aria-hidden="true" />
              <span className="program-name">{program.name}</span>
              {adapted && <span className="program-badge"><BadgeCheck size={13} aria-hidden="true" /><span data-motion-text>{t('ui.programs.adapted')}</span></span>}
              {disabled && <span className="program-state" data-motion-text>{t('ui.programs.disabled')}</span>}
              {(program.status === 'invalid' || program.status === 'unavailable') && <span className="program-state" data-warning data-motion-text>{t(program.status === 'invalid' ? 'ui.programs.invalid' : 'ui.programs.unavailable')}</span>}
            </span>
          </Hint></li>
        })}</ul>
      </ScrollBox>}
      {!list && !issue && <div className="program-placeholder" aria-hidden="true"><span className="skeleton-bar skeleton-label" /></div>}
      {list && !list.entries.length && !issue && !list.incomplete && <p className="program-message" data-motion-text>{t('ui.programs.empty')}</p>}
      {issue ? <div className="program-message" role="status"><span data-motion-text>{t('ui.programs.failed')}</span>
        <Button className="text-button" onClick={() => onError(issue)}>{t('ui.details')}</Button></div>
        : list?.incomplete && <p className="program-message" role="status" data-motion-text>{t('ui.programs.partial')}</p>}
    </div>
    <div className="field-actions"><Button className="control-button" disabled={locked} onClick={onAdd}><Plus size={16} />{t('setup.action.add')}</Button>
      <Hint label={t('setup.action.openstartup')}><Button className="icon-button" disabled={locked} aria-label={t('setup.action.openstartup')} onClick={onOpen}><FolderOpen size={16} /></Button></Hint>
    </div>
  </section>
}
