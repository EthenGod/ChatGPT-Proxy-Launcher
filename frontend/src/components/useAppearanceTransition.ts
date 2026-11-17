import { useEffect, useRef } from 'react'
import { flushSync } from 'react-dom'

// Text gets its own snapshot, leaving the surrounding surface opaque while it moves.
function separateLanguageLayers() {
  const text = '[data-motion-text], h1, h2, label, .dialog-title, .dialog-description, .dialog-body > p, .followup p, .field-error, .deployment-status > span:last-child, .footer-status > span, .menu-item > span, .argument-preview, .notice-title, .notice-description, .error-heading code, .error-guide p, .error-guide summary'
  const boxes = '.settings-header, .settings-card, .setting-row, .argument-scroll, .advanced-section, .programs-group, .settings-footer, .prototype-bar, .button, .control-button, .icon-button, .text-button, .input-action, .input-wrap input, .segmented, .segment, .switch, .version-badge, .setting-badge, .dialog, .menu-popup, .notice-toast, .error-guide'
  const elements = [...document.querySelectorAll<HTMLElement>(`${boxes}, ${text}`)]
  const previous = elements.map(element => [element, element.style.getPropertyValue('view-transition-name'), element.style.getPropertyValue('view-transition-class')] as const)
  elements.forEach((element, index) => {
    element.style.setProperty('view-transition-name', `language-${index}`)
    element.style.setProperty('view-transition-class', element.matches(text) ? 'language-text' : 'language-frame')
  })
  return () => previous.forEach(([element, name, category]) => {
    if (name) element.style.setProperty('view-transition-name', name); else element.style.removeProperty('view-transition-name')
    if (category) element.style.setProperty('view-transition-class', category); else element.style.removeProperty('view-transition-class')
  })
}

/** Preserve the mounted form and focus while text/palette and geometry transition separately. */
export function useAppearanceTransition() {
  const active = useRef<ViewTransition | null>(null)
  const revision = useRef(0)
  const pending = useRef(new Map<'language' | 'theme', () => void>())
  const restoreLayers = useRef<(() => void) | null>(null)
  useEffect(() => () => {
    ++revision.current
    active.current?.skipTransition()
    active.current = null
    pending.current.clear()
    restoreLayers.current?.(); restoreLayers.current = null
    delete document.documentElement.dataset.appearanceTransition
  }, [])

  return (kind: 'language' | 'theme', update: () => void) => {
    const request = ++revision.current
    pending.current.set(kind, update)
    active.current?.skipTransition()
    active.current = null
    restoreLayers.current?.(); restoreLayers.current = null
    const commit = () => {
      if (request !== revision.current) return
      const updates = [...pending.current.values()]
      pending.current.clear()
      flushSync(() => updates.forEach(apply => apply()))
    }
    const root = document.documentElement
    if (!document.startViewTransition || (kind === 'language' && typeof CSS !== 'undefined' && CSS.supports && !CSS.supports('view-transition-class', 'language-text'))
      || window.matchMedia('(prefers-reduced-motion: reduce), (forced-colors: active)').matches) {
      delete root.dataset.appearanceTransition
      commit()
      return
    }
    root.dataset.appearanceTransition = kind
    if (kind === 'language') restoreLayers.current = separateLanguageLayers()
    try {
      const transition = document.startViewTransition(commit)
      active.current = transition
      // A hidden window or interrupted snapshot can reject ready without losing the update.
      void transition.ready.catch(() => {})
      void transition.finished.catch(() => {}).finally(() => {
        if (active.current === transition) {
          active.current = null
          restoreLayers.current?.(); restoreLayers.current = null
          delete root.dataset.appearanceTransition
        }
      })
    } catch {
      restoreLayers.current?.(); restoreLayers.current = null
      delete root.dataset.appearanceTransition
      commit()
    }
  }
}
