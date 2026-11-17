import { useLayoutEffect, useRef, type ReactElement, type ReactNode } from 'react'
import { ScrollArea } from '@base-ui/react/scroll-area'

/** The browser keeps wheel, touch and keyboard scrolling; Base UI owns the thumb. */
export function ScrollBox({ children, className = '', contentClassName = '', label, render, tabIndex }: {
  children: ReactNode; className?: string; contentClassName?: string; label?: string; render?: ReactElement; tabIndex?: number
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
  return <ScrollArea.Root className={`scroll-area ${className}`} render={render}>
    <ScrollArea.Viewport className="scroll-viewport" aria-label={label} tabIndex={tabIndex} style={{ overflowX: 'hidden' }}>
      <ScrollArea.Content className={`scroll-content ${contentClassName}`} style={{ minWidth: 0 }}>{children}</ScrollArea.Content>
    </ScrollArea.Viewport>
    <ScrollArea.Scrollbar ref={track} keepMounted className="scroll-track" orientation="vertical">
      <ScrollArea.Thumb ref={thumb} className="scroll-thumb" />
    </ScrollArea.Scrollbar>
  </ScrollArea.Root>
}
