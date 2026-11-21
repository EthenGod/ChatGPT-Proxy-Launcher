type Glyph = { x: number; y: number; start: number; end: number }

// Measure the original text node so the browser still chooses all line breaks.
function glyphPositions(node: Text, range: Range, origin: DOMRect): Glyph[] {
  let start = 0
  return Array.from(node.data, char => {
    const end = start + char.length
    range.setStart(node, start)
    range.setEnd(node, end)
    const rect = range.getBoundingClientRect()
    const glyph = { x: rect.x - origin.x, y: rect.y - origin.y, start, end }
    start = end
    return glyph
  })
}

function reflowDuration(style: CSSStyleDeclaration) {
  const time = style.getPropertyValue('--motion-fast').trim()
  return parseFloat(time) * (time.endsWith('ms') ? 1 : 1000)
}

// React callback ref. Keep the original text node for shaping, selection and accessibility.
export function observeTextReflow(label: HTMLElement | null) {
  if (!label || !window.matchMedia || typeof ResizeObserver === 'undefined') return
  const footer = label.closest<HTMLElement>('.status-caption')?.closest<HTMLElement>('.settings-footer')
  const scroll = footer && label.closest<HTMLElement>('.status-scroll')
  const anchor = footer ?? label.closest<HTMLElement>('.scroll-content, .dialog')
  if (!anchor) return
  const motion = window.matchMedia('(prefers-reduced-motion: reduce), (forced-colors: active)')
  const range = document.createRange()
  const layer = document.createElement('span')
  layer.className = footer ? 'text-reflow-layer status-reflow-layer' : 'text-reflow-layer'
  layer.setAttribute('aria-hidden', 'true')
  layer.inert = true
  let previous: Glyph[] = [], painted: Glyph[] = []
  let lastText = '', previousWidth = 0, previousScrollHeight = 0, frame = 0
  let finishAt = 0
  let resize: Animation | undefined
  const stopDrawing = () => {
    cancelAnimationFrame(frame)
    frame = 0
    label.removeAttribute('data-reflowing')
    layer.remove()
    painted = []
  }
  const clear = () => { stopDrawing(); resize?.cancel() }
  const selected = () => {
    const selection = window.getSelection()
    return !!selection?.rangeCount && !selection.isCollapsed && selection.getRangeAt(0).intersectsNode(label)
  }
  const observer = new ResizeObserver(([entry]) => {
    const node = label.firstChild
    // Keep long command lines and logs cheap to resize. Offscreen text needs no overlay.
    if (!entry || !entry.contentRect.height || !(node instanceof Text) || label.childNodes.length !== 1 ||
        node.length > 2048 || motion.matches) {
      clear()
      previous = []
      return
    }
    const { width } = entry.contentRect
    const text = node.data
    const bounds = label.getBoundingClientRect()
    const local = glyphPositions(node, range, bounds)
    const endPositions = () => {
      const box = label.getBoundingClientRect(), edge = anchor.getBoundingClientRect()
      return { x: box.x - edge.left - anchor.clientLeft, y: box.y - (footer ? edge.bottom : edge.top + anchor.clientTop) }
    }
    const origin = endPositions()
    const next = local.map(g => ({ ...g, x: g.x + origin.x, y: g.y + origin.y }))
    const before = painted.length ? painted : previous
    const sameText = text === lastText
    if (!sameText) stopDrawing()
    const rewrapped = sameText && previous.length === next.length && width !== previousWidth &&
      next.some((g, i) => Math.abs((g.y - next[0]!.y) - (previous[i]!.y - previous[0]!.y)) > 1)
    // The status row has a minimum height. Losing a text line can shrink the footer by less than a line.
    const scrollHeight = scroll?.getBoundingClientRect().height ?? 0
    const heightChange = previousScrollHeight - scrollHeight
    previous = next; previousWidth = width; previousScrollHeight = scrollHeight; lastText = text
    // During an interrupted slide, even a resize that keeps the same breaks moves its destination.
    if (document.documentElement.hasAttribute('data-appearance-transition') ||
        selected() || label.closest('[inert], [data-swapping]')) { clear(); return }
    if (!rewrapped && !(frame && before.length === local.length)) return
    if (scroll && heightChange) {
      const start = heightChange / 2 + parseFloat(getComputedStyle(scroll).marginBlockStart)
      resize?.cancel()
      resize = scroll.animate([{ marginBlock: start + 'px' }, { marginBlock: '0px' }], { duration: 250, easing: 'ease-out' })
    }
    // Adjust the destination as the gutter settles without extending the slide.
    const started = performance.now()
    const style = getComputedStyle(label)
    if (rewrapped) finishAt = started + reflowDuration(style)
    const duration = finishAt - started
    stopDrawing()
    if (duration <= 0) return
    layer.replaceChildren()
    layer.style.font = style.font
    layer.style.lineHeight = style.lineHeight
    layer.style.letterSpacing = style.letterSpacing
    layer.style.color = style.color
    anchor.append(layer)
    // Neighbours moving together stay in one shaped text run.
    const runs: { el: HTMLElement; index: number; inkTop: number }[] = []
    let index = 0
    while (index < local.length) {
      const first = index++
      while (index < local.length && local[index]!.y === local[first]!.y &&
        Math.abs(before[index]!.y - before[first]!.y) < 0.1 &&
        Math.abs((before[index]!.x - before[first]!.x) - (local[index]!.x - local[first]!.x)) < 0.1) index++
      const value = text.slice(local[first]!.start, local[index - 1]!.end)
      if (!value.trim()) continue
      const el = document.createElement('span')
      el.textContent = value
      layer.append(el)
      range.selectNodeContents(el)
      runs.push({ el, index: first, inkTop: range.getBoundingClientRect().top - el.getBoundingClientRect().top })
    }
    label.dataset.reflowing = ''
    const draw = (now: number) => {
      if (node.data !== text || node.parentNode !== label || selected() || document.documentElement.hasAttribute('data-appearance-transition')) {
        clear()
        return
      }
      const end = endPositions()
      const remaining = Math.pow(1 - Math.min(1, Math.max(0, now - started) / duration), 3)
      painted = local.map((g, i) => {
        const x = g.x + end.x, y = g.y + end.y
        return { ...g, x: x + (before[i]!.x - x) * remaining, y: y + (before[i]!.y - y) * remaining }
      })
      for (const run of runs) {
        const g = painted[run.index]!
        run.el.style.transform = 'translate(' + g.x + 'px,' + (g.y - run.inkTop) + 'px)'
      }
      if (remaining) frame = requestAnimationFrame(draw)
      else { previous = painted; stopDrawing() }
    }
    draw(started)
  })
  let visible = true
  const refresh = () => {
    clear()
    previous = []
    observer.unobserve(label)
    if (visible) observer.observe(label)
  }
  // Text can change without changing the box. Scrolling a paragraph into view
  // also needs a current starting layout before the next horizontal resize.
  const edits = new MutationObserver(refresh)
  edits.observe(label, { childList: true, characterData: true, subtree: true })
  const visibility = typeof IntersectionObserver === 'undefined' ? null : new IntersectionObserver(([entry]) => {
    if (entry && visible !== entry.isIntersecting) { visible = entry.isIntersecting; refresh() }
  })
  visibility?.observe(label)
  observer.observe(label)
  motion.addEventListener('change', refresh)
  return () => {
    visibility?.disconnect()
    edits.disconnect()
    observer.disconnect()
    motion.removeEventListener('change', refresh)
    clear()
  }
}

// Flex rows keep their native breaks. Move a whole item, including its background
// and hit target, only when it crosses between rows.
export function observeWrappedRow(group: HTMLElement | null) {
  if (!group || !window.matchMedia || typeof ResizeObserver === 'undefined') return
  const motion = window.matchMedia('(prefers-reduced-motion: reduce), (forced-colors: active)')
  type Position = { x: number; y: number; row: number }
  let previous = new Map<HTMLElement, Position>()
  const moving = new Map<HTMLElement, Animation>()
  let width = 0
  const clear = () => { moving.forEach(animation => animation.cancel()); moving.clear() }
  const observer = new ResizeObserver(() => {
    const bounds = group.getBoundingClientRect()
    const next = new Map<HTMLElement, Position>()
    const enabled = width !== 0 && width !== bounds.width && !motion.matches &&
      !document.documentElement.hasAttribute('data-appearance-transition') && !group.closest('[inert]')
    let row = 0, bottom = -Infinity
    for (const item of group.children) {
      if (!(item instanceof HTMLElement) || !item.offsetHeight) continue
      const box = item.getBoundingClientRect()
      const active = moving.get(item)
      const shift = active ? getComputedStyle(item).translate.split(' ').map(parseFloat) : []
      const dx = shift[0] || 0, dy = shift[1] || 0
      const x = box.x - bounds.x - dx, y = box.y - bounds.y - dy
      if (y >= bottom) row++
      bottom = Math.max(bottom, y + box.height)
      next.set(item, { x, y, row })
      const old = previous.get(item)
      if (!enabled || !old || row === old.row) continue
      active?.cancel()
      const animation = item.animate([{ translate: (old.x + dx - x) + 'px ' + (old.y + dy - y) + 'px' },
        { translate: '0px 0px' }], { duration: reflowDuration(getComputedStyle(group)), easing: 'ease-out' })
      moving.set(item, animation)
      const finish = () => { if (moving.get(item) === animation) moving.delete(item) }
      void animation.finished.then(finish, finish)
    }
    previous = next
    width = bounds.width
  })
  observer.observe(group)
  motion.addEventListener('change', clear)
  return () => { observer.disconnect(); motion.removeEventListener('change', clear); clear() }
}
