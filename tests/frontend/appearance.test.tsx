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
  it('releases a snapshot on resize and keeps the latest language', () => {
    const { commits, skip, start } = transitions()
    const { result, unmount } = renderHook(useAppearanceTransition)
    const oldLanguage = vi.fn(), newLanguage = vi.fn()
    act(() => result.current('language', oldLanguage))
    act(() => { window.dispatchEvent(new Event('resize')) })
    expect(skip).toHaveBeenCalledOnce()
    expect(document.documentElement.dataset.appearanceTransition).toBeUndefined()
    act(() => result.current('language', newLanguage))
    act(() => commits.forEach(commit => commit()))
    expect(start).toHaveBeenCalledOnce()
    expect(oldLanguage).not.toHaveBeenCalled()
    expect(newLanguage).toHaveBeenCalledOnce()
    unmount()
    expect(document.documentElement.dataset.windowResizing).toBeUndefined()
  })
  it('keeps detached labels at the opacity of their control until the snapshot is released', async () => {
    const { getByRole } = render(<div>
      <button className="text-button" disabled style={{ opacity: .5 }}><span data-motion-text>Restore default</span></button>
      <button className="text-button" style={{ opacity: 1 }}><span data-motion-text>Browse</span></button>
    </div>)
    const disabled = getByRole('button', { name: 'Restore default' })
    const enabled = getByRole('button', { name: 'Browse' })
    const { commits, finishes } = transitions()
    const { result } = renderHook(useAppearanceTransition)
    act(() => result.current('language', () => { disabled.firstElementChild!.textContent = '恢复默认' }))
    act(() => commits[0]!())
    const name = disabled.style.getPropertyValue('view-transition-name')
    const rules = Array.from(document.styleSheets).flatMap(sheet => Array.from(sheet.cssRules)) as CSSStyleRule[]
    expect(disabled.style.getPropertyValue('view-transition-group')).toBe('contain')
    expect(rules.find(rule => rule.selectorText === `::view-transition-group-children(${name})`)?.style.opacity).toBe('0.5')
    expect(enabled.style.getPropertyValue('view-transition-group')).toBe('')
    await act(async () => finishes[0]!())
    expect(disabled.style.getPropertyValue('view-transition-group')).toBe('')
    expect(disabled.style.opacity).toBe('0.5')
  })
  it('extracts visible program labels and leaves fully clipped rows in their viewport', () => {
    const { container } = render(<div className="program-scroll"><div className="scroll-viewport"><ul className="program-list">
      {['Above', 'Partial', 'Visible', 'Below'].map(label => <li key={label}><span data-motion-text>{label}</span></li>)}
    </ul></div></div>)
    const viewport = container.querySelector('.scroll-viewport')!
    vi.spyOn(viewport, 'getBoundingClientRect').mockReturnValue(new DOMRect(0, 50, 200, 80))
    const rows = Array.from(container.querySelectorAll('li'))
    rows.forEach((row, i) => vi.spyOn(row, 'getBoundingClientRect').mockReturnValue(new DOMRect(0, [10, 40, 80, 130][i], 200, 40)))
    transitions()
    const { result } = renderHook(useAppearanceTransition)
    act(() => result.current('language', () => {}))
    expect(rows.map(row => !!row.querySelector('span')!.style.getPropertyValue('view-transition-name'))).toEqual([false, true, true, false])
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
