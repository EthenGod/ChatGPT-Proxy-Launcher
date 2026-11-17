import { useEffect, useRef } from 'react'
import { flushSync } from 'react-dom'

/** Crossfade the rendered text/palette without remounting fields or moving focus. */
export function useAppearanceTransition() {
  const active = useRef<ViewTransition | null>(null)
  const revision = useRef(0)
  const pending = useRef(new Map<'language' | 'theme', () => void>())
  useEffect(() => () => {
    ++revision.current
    active.current?.skipTransition()
    active.current = null
    pending.current.clear()
    delete document.documentElement.dataset.appearanceTransition
  }, [])

  return (kind: 'language' | 'theme', update: () => void) => {
    const request = ++revision.current
    pending.current.set(kind, update)
    active.current?.skipTransition()
    active.current = null
    const commit = () => {
      if (request !== revision.current) return
      const updates = [...pending.current.values()]
      pending.current.clear()
      flushSync(() => updates.forEach(apply => apply()))
    }
    const root = document.documentElement
    if (!document.startViewTransition || window.matchMedia('(prefers-reduced-motion: reduce), (forced-colors: active)').matches) {
      delete root.dataset.appearanceTransition
      commit()
      return
    }
    root.dataset.appearanceTransition = kind
    try {
      const transition = document.startViewTransition(commit)
      active.current = transition
      // A hidden window or interrupted snapshot can reject ready without losing the update.
      void transition.ready.catch(() => {})
      void transition.finished.catch(() => {}).finally(() => {
        if (active.current === transition) {
          active.current = null
          delete root.dataset.appearanceTransition
        }
      })
    } catch {
      delete root.dataset.appearanceTransition
      commit()
    }
  }
}
