import { useMemo, useState } from 'react'
import { createRoot } from 'react-dom/client'
import { SetupPage } from './features/setup/SetupPage'
import { ChoiceMenu } from './components/controls'
import { MockSetupService, programScenarios, scenarios, scenarioLabels, type Scenario } from './platform/mock'
import { desktopWindow } from './platform/desktop'
import { translator } from './i18n'
import './styles/theme.css'
import './styles/prototype.css'

function Prototype() {
  const initial = new URLSearchParams(window.location.search).get('scenario')
  const [scenario, setScenario] = useState<Scenario>(scenarios.includes(initial as Scenario) ? initial as Scenario : 'normal')
  const service = useMemo(() => new MockSetupService(scenario), [scenario])
  const [language, setLanguage] = useState('zh-CN')
  const t = translator(language)
  const sample = initial === 'programs'
  return <SetupPage key={scenario} service={service} host={desktopWindow} showPrograms={sample || scenario.startsWith('programs')} onLanguageChange={setLanguage} scenarioControls={
    <ChoiceMenu label={t('ui.scenarios')} value={scenario} onChange={value => setScenario(value as Scenario)} variant="quiet"
      items={(sample ? programScenarios : scenarios).map(value => ({ value, label: t(sample && value === 'first' ? 'ui.programs.empty' : scenarioLabels[value]) }))} />
  } />
}
const root = document.getElementById('root')
if (!root) throw new Error('Missing application root')
createRoot(root).render(<Prototype />)
