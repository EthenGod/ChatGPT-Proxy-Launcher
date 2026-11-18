import { useEffect, useRef } from 'react'
import { flushSync } from 'react-dom'

// Paint flat surfaces on the animated bounds, not inside a stretched bitmap.
// This keeps one-pixel borders and rounded corners stable as labels change width.
function separateLanguageLayers() {
  const text = '[data-motion-text], h1, h2, label, .dialog-title, .dialog-description:not(.help-paragraphs), .help-paragraphs > p, .dialog-body > p, .followup p, .field-error, .deployment-status > span:last-child, .menu-item > span, .argument-preview, .notice-title, .notice-description, .notice-detail > p, .error-heading code, .error-guide p, .error-guide summary'
  const boxes = '.appearance-controls, .settings-card, .argument-scroll, .advanced-trigger, .settings-footer, .prototype-bar, .button, .control-button, .icon-button, .text-button, .input-action, .input-wrap input, .segmented, .segment, .switch, .version-badge, .setting-badge, .dialog, .menu-popup, .notice-toast, .error-guide, .program-results, .program-badge'
  const elements = [...document.querySelectorAll<HTMLElement>(`${boxes}, ${text}`)].filter(element => !element.matches('.appearance-controls .icon-button') && !element.closest('[inert]'))
  const properties = ['view-transition-name', 'view-transition-class', 'background-color', 'border-color', 'box-shadow']
  const previous = elements.map(element => ({ element, text: element.textContent, values: properties.map(property => [property, element.style.getPropertyValue(property), element.style.getPropertyPriority(property)] as const), transition: [element.style.getPropertyValue('transition'), element.style.getPropertyPriority('transition')] as const, paint: getComputedStyle(element) }))
  const decorations = document.createElement('style')
  document.head.append(decorations)
  const surfaces: HTMLElement[] = []
  const gutters = [...document.querySelectorAll<HTMLElement>('.scroll-content')].map(element => ({
    element, value: element.style.getPropertyValue('margin-inline-end'), priority: element.style.getPropertyPriority('margin-inline-end'),
  }))
  previous.forEach(({ element, paint }, index) => {
    const isText = element.matches(text)
    // Keep the guard independent of the snapshot name, including during cleanup.
    element.style.setProperty('transition', 'none', 'important')
    element.style.setProperty('view-transition-name', `language-${index}`)
    element.style.setProperty('view-transition-class', isText ? 'language-text' : element.matches('input') ? 'language-input' : 'language-frame')
    if (isText || paint.opacity !== '1') return
    const background = paint.backgroundColor, radius = paint.borderRadius, shadow = paint.boxShadow
    const edges = ['top', 'right', 'bottom', 'left'].map(side => ({
      width: parseFloat(paint.getPropertyValue(`border-${side}-width`)),
      color: paint.getPropertyValue(`border-${side}-color`),
      style: paint.getPropertyValue(`border-${side}-style`),
    }))
    const borders = edges.map(({ width, color, style }, edge) => width && style === 'solid' && color !== 'rgba(0, 0, 0, 0)'
      ? `inset ${edge === 1 ? -width : edge === 3 ? width : 0}px ${edge === 0 ? width : edge === 2 ? -width : 0}px 0 ${color}` : '').filter(Boolean)
    const first = edges[0]
    if (first && borders.length === 4 && edges.every(edge => edge.width === first.width && edge.color === first.color)) {
      borders.splice(0, 4, `inset 0 0 0 ${first.width}px ${first.color}`)
    }
    if (background === 'rgba(0, 0, 0, 0)' && !borders.length && shadow === 'none') return
    const sheet = decorations.sheet!
    const rule = sheet.cssRules[sheet.insertRule(`::view-transition-group(language-${index}) {}`)] as CSSStyleRule
    rule.style.backgroundColor = background
    rule.style.borderRadius = radius
    rule.style.boxShadow = [...borders, ...(shadow === 'none' ? [] : [shadow])].join(', ') || 'none'
    surfaces.push(element)
  })
  return {
    settle() {
      // Keep the live page painted until the old snapshot has been captured.
      // This callback runs with rendering suspended, so only the new snapshots
      // lose their paint; the animated geometry layers replace it without a gap.
      surfaces.forEach(element => {
        element.style.setProperty('background-color', 'transparent', 'important')
        element.style.setProperty('border-color', 'transparent', 'important')
        element.style.setProperty('box-shadow', 'none', 'important')
      })
      previous.forEach(({ element, text: before }) => {
        if (element.matches(text) && element.textContent === before) element.style.setProperty('view-transition-class', 'language-static')
      })
      // Rendering is suspended during the update callback: waiting for rAF would
      // time out. Resolve gutters synchronously, then let ResizeObserver catch up.
      for (let pass = 0; pass < 2; pass++) gutters.forEach(({ element }) => {
        const viewport = element.parentElement!
        const space = viewport.scrollHeight > viewport.clientHeight ? getComputedStyle(element).getPropertyValue('--scrollbar-space') : '0px'
        element.style.setProperty('margin-inline-end', space, 'important')
      })
    },
    restore() {
      decorations.remove()
      previous.forEach(({ element, values }) => values.forEach(([property, value, priority]) => {
        if (value) element.style.setProperty(property, value, priority); else element.style.removeProperty(property)
      }))
      gutters.forEach(({ element, value, priority }) => {
        if (value) element.style.setProperty('margin-inline-end', value, priority); else element.style.removeProperty('margin-inline-end')
      })
      // Commit the restored paint while transitions are still disabled. Removing
      // both in one style update would fade the live controls in from transparent.
      document.documentElement.getBoundingClientRect()
      previous.forEach(({ element, transition: [value, priority] }) => {
        if (value) element.style.setProperty('transition', value, priority); else element.style.removeProperty('transition')
      })
    },
  }
}

/** Preserve the mounted form and focus while text/palette and geometry transition separately. */
export function useAppearanceTransition() {
  const active = useRef<ViewTransition | null>(null)
  const revision = useRef(0)
  const pending = useRef(new Map<'language' | 'theme', () => void>())
  const layers = useRef<ReturnType<typeof separateLanguageLayers> | null>(null)
  useEffect(() => () => {
    ++revision.current
    active.current?.skipTransition()
    active.current = null
    pending.current.clear()
    layers.current?.restore(); layers.current = null
    delete document.documentElement.dataset.appearanceTransition
  }, [])

  return (kind: 'language' | 'theme', update: () => void) => {
    const request = ++revision.current
    pending.current.set(kind, update)
    // A queued palette change needs the full theme crossfade, not old-color surfaces.
    const mode = pending.current.has('theme') ? 'theme' : kind
    active.current?.skipTransition()
    active.current = null
    layers.current?.restore(); layers.current = null
    const commit = () => {
      if (request !== revision.current) return
      const updates = [...pending.current.values()]
      pending.current.clear()
      flushSync(() => updates.forEach(apply => apply()))
      layers.current?.settle()
    }
    const root = document.documentElement
    if (!document.startViewTransition || (mode === 'language' && typeof CSS !== 'undefined' && CSS.supports && !CSS.supports('view-transition-class', 'language-text'))
      || window.matchMedia('(prefers-reduced-motion: reduce), (forced-colors: active)').matches) {
      delete root.dataset.appearanceTransition
      commit()
      return
    }
    root.dataset.appearanceTransition = mode
    try {
      if (mode === 'language') layers.current = separateLanguageLayers()
      const transition = document.startViewTransition(commit)
      active.current = transition
      // A hidden window or interrupted snapshot can reject ready without losing the update.
      void transition.ready.catch(() => {})
      void transition.finished.catch(() => {}).finally(() => {
        if (active.current === transition) {
          active.current = null
          layers.current?.restore(); layers.current = null
          delete root.dataset.appearanceTransition
        }
      })
    } catch {
      layers.current?.restore(); layers.current = null
      delete root.dataset.appearanceTransition
      commit()
    }
  }
}
