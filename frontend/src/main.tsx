import { createRoot } from 'react-dom/client'
import { SetupPage } from './features/setup/SetupPage'
import { setupService } from './platform/setup-service'
import { prototypeWindow } from './platform/prototype-window'
import './styles/theme.css'
import './styles/prototype.css'

const root = document.getElementById('root')
if (!root) throw new Error('Missing application root')
createRoot(root).render(<SetupPage service={setupService} host={prototypeWindow} />)
