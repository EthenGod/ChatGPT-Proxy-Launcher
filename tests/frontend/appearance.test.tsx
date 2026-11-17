// @vitest-environment jsdom
import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest'
import { act, cleanup, renderHook } from '@testing-library/react'
import { useAppearanceTransition } from '../../frontend/src/components/useAppearanceTransition'

beforeEach(() => { window.matchMedia = vi.fn().mockReturnValue({ matches: false }) })
afterEach(() => { cleanup(); Reflect.deleteProperty(document, 'startViewTransition'); vi.restoreAllMocks() })

function transitions() {
  const commits: Array<() => void> = []
  const skip = vi.fn()
  const start = vi.fn((commit: () => void) => {
    commits.push(commit)
    let finish!: () => void
    const finished = new Promise<void>(resolve => { finish = resolve })
    return { ready: Promise.resolve(), finished, skipTransition: () => { skip(); finish() } }
  })
  Object.defineProperty(document, 'startViewTransition', { configurable: true, value: start })
  return { commits, skip, start }
}

describe('appearance changes', () => {
  it('keeps the latest language and a pending theme when snapshots are interrupted', () => {
    const { commits, skip } = transitions()
    const { result } = renderHook(useAppearanceTransition)
    const theme = vi.fn(), english = vi.fn(), japanese = vi.fn()
    act(() => { result.current('theme', theme); result.current('language', english); result.current('language', japanese) })
    act(() => commits.forEach(commit => commit()))
    expect(theme).toHaveBeenCalledOnce()
    expect(japanese).toHaveBeenCalledOnce()
    expect(english).not.toHaveBeenCalled()
    expect(skip).toHaveBeenCalledTimes(2)
  })
  it('applies changes immediately when motion is reduced or snapshots are unavailable', () => {
    const { start } = transitions()
    vi.mocked(window.matchMedia).mockReturnValue({ matches: true } as MediaQueryList)
    const { result } = renderHook(useAppearanceTransition)
    const update = vi.fn()
    act(() => result.current('language', update))
    expect(update).toHaveBeenCalledOnce()
    expect(start).not.toHaveBeenCalled()
    Reflect.deleteProperty(document, 'startViewTransition')
    vi.mocked(window.matchMedia).mockReturnValue({ matches: false } as MediaQueryList)
    act(() => result.current('theme', update))
    expect(update).toHaveBeenCalledTimes(2)
  })
  it('does not apply a queued change after unmount', () => {
    const { commits, skip } = transitions()
    const { result, unmount } = renderHook(useAppearanceTransition)
    const update = vi.fn()
    act(() => result.current('theme', update))
    unmount()
    act(() => commits.forEach(commit => commit()))
    expect(update).not.toHaveBeenCalled()
    expect(skip).toHaveBeenCalledOnce()
    expect(document.documentElement.dataset.appearanceTransition).toBeUndefined()
  })
})
