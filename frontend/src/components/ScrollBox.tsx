import type { ReactElement, ReactNode } from 'react'
import { ScrollArea } from '@base-ui/react/scroll-area'

/** The browser keeps wheel, touch and keyboard scrolling; Base UI owns the thumb. */
export function ScrollBox({ children, className = '', contentClassName = '', label, render, tabIndex }: {
  children: ReactNode; className?: string; contentClassName?: string; label?: string; render?: ReactElement; tabIndex?: number
}) {
  return <ScrollArea.Root className={`scroll-area ${className}`} render={render}>
    <ScrollArea.Viewport className="scroll-viewport" aria-label={label} tabIndex={tabIndex} style={{ overflowX: 'hidden' }}>
      <ScrollArea.Content className={`scroll-content ${contentClassName}`} style={{ minWidth: 0 }}>{children}</ScrollArea.Content>
    </ScrollArea.Viewport>
    <ScrollArea.Scrollbar keepMounted className="scroll-track" orientation="vertical">
      <ScrollArea.Thumb className="scroll-thumb" />
    </ScrollArea.Scrollbar>
  </ScrollArea.Root>
}
