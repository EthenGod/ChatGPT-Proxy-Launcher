import { useLayoutEffect, useRef, type ReactElement, type ReactNode } from 'react'
import { ScrollArea } from '@base-ui/react/scroll-area'

/** The browser keeps wheel, touch and keyboard scrolling; Base UI owns the thumb. */
export function ScrollBox({ children, className = '', contentClassName = '', label, render, tabIndex, persistent = false }: {
  children: ReactNode; className?: string; contentClassName?: string; label?: string; render?: ReactElement; tabIndex?: number; persistent?: boolean
}) {
  const track = useRef<HTMLDivElement | null>(null)
  const thumb = useRef<HTMLDivElement | null>(null)
  useLayoutEffect(() => {
    const rail = track.current, handle = thumb.current
    if (!rail || !handle) return
    // Freeze the last valid geometry while fading out. Base UI resets it when overflow ends.
    const remember = () => {
      if (!rail.hasAttribute('data-has-overflow-y')) return
      const height = rail.style.getPropertyValue('--scroll-area-thumb-height')
      if (!(parseFloat(height) > 0)) return
      for (const [key, value] of [['--scroll-thumb-last-height', height], ['--scroll-thumb-last-transform', handle.style.transform || 'none']] as const) {
        if (rail.style.getPropertyValue(key) !== value) rail.style.setProperty(key, value)
      }
    }
    remember()
    const observer = new MutationObserver(remember)
    observer.observe(rail, { attributes: true, attributeFilter: ['style', 'data-has-overflow-y'] })
    observer.observe(handle, { attributes: true, attributeFilter: ['style'] })
    return () => observer.disconnect()
  }, [])
  useLayoutEffect(() => {
    const rail = track.current
    if (persistent || !rail) return
    // The rail remains a drag target while its thumb and the content gutter fade away.
    let timeout: number | undefined
    const end = () => { window.clearTimeout(timeout); rail.removeAttribute('data-pointer-moving') }
    const move = (event: PointerEvent) => {
      if (event.pointerType === 'touch') return
      rail.setAttribute('data-pointer-moving', '')
      window.clearTimeout(timeout)
      timeout = window.setTimeout(end, 500)
    }
    rail.addEventListener('pointermove', move)
    rail.addEventListener('pointerleave', end)
    return () => { end(); rail.removeEventListener('pointermove', move); rail.removeEventListener('pointerleave', end) }
  }, [persistent])
  return <ScrollArea.Root className={`scroll-area ${className}`} render={render} data-auto-hide={persistent ? undefined : ''}>
    <ScrollArea.Viewport className="scroll-viewport" aria-label={label} tabIndex={tabIndex} style={{ overflowX: 'hidden' }}>
      <ScrollArea.Content className={`scroll-content ${contentClassName}`} style={{ minWidth: 0 }}>{children}</ScrollArea.Content>
    </ScrollArea.Viewport>
    <ScrollArea.Scrollbar ref={track} keepMounted className="scroll-track" orientation="vertical">
      <ScrollArea.Thumb ref={thumb} className="scroll-thumb" />
    </ScrollArea.Scrollbar>
  </ScrollArea.Root>
}
