import { createRoot } from 'react-dom/client'
import { App } from './app/App'
import './styles/theme.css'

const root = document.getElementById('root')
if (!root) throw new Error('Missing application root')
createRoot(root).render(<App />)
