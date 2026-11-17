// @vitest-environment jsdom
import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest'
import { act, cleanup, render, renderHook } from '@testing-library/react'
import { useAppearanceTransition } from '../../frontend/src/components/useAppearanceTransition'

beforeEach(() => { window.matchMedia = vi.fn().mockReturnValue({ matches: false }) })
afterEach(() => { cleanup(); Reflect.deleteProperty(document, 'startViewTransition'); vi.restoreAllMocks() })

function transitions() {
  const commits: Array<() => void> = []
  const finishes: Array<() => void> = []
  const skip = vi.fn()
  const start = vi.fn((commit: () => void) => {
    commits.push(commit)
    let finish!: () => void
    const finished = new Promise<void>(resolve => { finish = resolve })
    finishes.push(finish)
    return { ready: Promise.resolve(), finished, skipTransition: () => { skip(); finish() } }
  })
  Object.defineProperty(document, 'startViewTransition', { configurable: true, value: start })
  return { commits, finishes, skip, start }
}

describe('appearance changes', () => {
  it('keeps the latest language and a pending theme when snapshots are interrupted', () => {
    const { commits, skip } = transitions()
    const { result } = renderHook(useAppearanceTransition)
    const theme = vi.fn(), english = vi.fn(), japanese = vi.fn()
    act(() => { result.current('theme', theme); result.current('language', english); result.current('language', japanese) })
    expect(document.documentElement.dataset.appearanceTransition).toBe('theme')
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
  it.each(['finish', 'unmount', 'throw'])('restores surface styling and temporary rules after %s', async outcome => {
    const { container } = render(<div className="settings-card" style={{ opacity: 1, backgroundColor: 'rgb(20, 20, 20)', border: '1px solid rgb(60, 60, 60)', borderRadius: 12, transition: 'background-color 150ms ease-out' }}>
      <span data-motion-text>Before</span>
    </div>)
    const card = container.firstElementChild as HTMLElement
    const original = card.style.cssText, sheets = document.styleSheets.length
    const paintAtHandoff: string[][] = []
    vi.spyOn(document.documentElement, 'getBoundingClientRect').mockImplementation(() => {
      paintAtHandoff.push([card.style.backgroundColor, card.style.transition, card.style.getPropertyPriority('transition')])
      return new DOMRect()
    })
    const { commits, finishes, start } = transitions()
    if (outcome === 'throw') start.mockImplementationOnce(() => { throw new Error('snapshot unavailable') })
    const { result, unmount } = renderHook(useAppearanceTransition)
    const update = vi.fn(() => { card.firstElementChild!.textContent = 'After' })
    act(() => result.current('language', update))
    if (outcome !== 'throw') {
      // Preparing snapshot names must never expose an unpainted live frame.
      expect(card.style.backgroundColor).toBe('rgb(20, 20, 20)')
      expect(card.style.borderColor).toBe('rgb(60, 60, 60)')
      act(() => commits[0]!())
      expect(card.style.backgroundColor).toBe('transparent')
      expect(card.style.borderColor).toBe('transparent')
      if (outcome === 'unmount') unmount()
      else await act(async () => finishes[0]!())
    }
    expect(update).toHaveBeenCalledOnce()
    expect(paintAtHandoff).toEqual([['rgb(20, 20, 20)', 'none', 'important']])
    expect(card.style.cssText).toBe(original)
    expect(document.styleSheets).toHaveLength(sheets)
    expect(document.documentElement.dataset.appearanceTransition).toBeUndefined()
  })
})
