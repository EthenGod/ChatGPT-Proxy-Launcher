import { useMemo, useState } from 'react'
import { createRoot } from 'react-dom/client'
import { SetupPage } from './features/setup/SetupPage'
import { MockSetupService, scenarios, scenarioLabels, type Scenario } from './platform/mock'
import { prototypeWindow } from './platform/prototype-window'
import { translator } from './i18n'
import './styles/theme.css'
import './styles/prototype.css'

function Prototype() {
  const initial = new URLSearchParams(window.location.search).get('scenario')
  const [scenario, setScenario] = useState<Scenario>(scenarios.includes(initial as Scenario) ? initial as Scenario : 'normal')
  const service = useMemo(() => new MockSetupService(scenario), [scenario])
  const [language, setLanguage] = useState('zh-CN')
  const t = translator(language)
  return <SetupPage key={scenario} service={service} host={prototypeWindow} onLanguageChange={setLanguage} scenarioControls={
    <label className="scenario-control"><span>{t('ui.scenarios')}</span><select aria-label={t('ui.scenarios')} value={scenario} onChange={event => setScenario(event.target.value as Scenario)}>
      {scenarios.map(value => <option key={value} value={value}>{t(scenarioLabels[value])}</option>)}
    </select></label>
  } />
}
const root = document.getElementById('root')
if (!root) throw new Error('Missing application root')
createRoot(root).render(<Prototype />)
