import { Component, createRef, type RefObject } from 'react'
import { ScrollBox } from '../../components/ScrollBox'
import { ArgumentSkeleton } from './SetupSkeleton'

interface Props {
  fieldRef: RefObject<HTMLPreElement | null>
  label: string
  value: string | null
  text: string
  invalid: boolean
  skeleton: boolean
}

type BeforeChange = { height: number; scrollTop: number; width: number } | { anchor: number; focus: number; length: number } | null

export class ArgumentPreview extends Component<Props, object, BeforeChange> {
  private currentText = createRef<HTMLSpanElement>()
  private previousText = createRef<HTMLSpanElement>()
  private fade: Animation | undefined
  private resize: Animation | undefined
  private staticMotion = window.matchMedia('(prefers-reduced-motion: reduce), (forced-colors: active)')

  // React is about to replace the text. This is the last point at which the
  // selection offsets and the visible (possibly mid-transition) height are valid.
  getSnapshotBeforeUpdate(previous: Props): BeforeChange {
    if (previous.text === this.props.text) return null
    const field = this.props.fieldRef.current!
    const selection = window.getSelection()
    if (selection?.rangeCount && !selection.isCollapsed && selection.getRangeAt(0).intersectsNode(field)) {
      const node = this.currentText.current?.firstChild
      if (node?.nodeType === Node.TEXT_NODE && selection.anchorNode === node && selection.focusNode === node) {
        return { anchor: selection.anchorOffset, focus: selection.focusOffset, length: previous.text.length }
      }
      return null
    }
    const box = field.closest<HTMLElement>('.argument-scroll')!
    if (previous.value === null || this.props.value === null || previous.value === this.props.value ||
        this.staticMotion.matches || box.matches(':focus-within') ||
        document.documentElement.matches('[data-appearance-transition], [data-window-resizing]')) return null
    return {
      height: box.getBoundingClientRect().height,
      scrollTop: field.closest('.scroll-viewport')!.scrollTop,
      width: field.getBoundingClientRect().width,
    }
  }

  componentDidUpdate(previous: Props, _state: object, before: BeforeChange) {
    if (previous.text === this.props.text && previous.value === this.props.value) return
    const live = this.currentText.current!
    if (!before || 'anchor' in before) {
      this.stop()
      const node = live.firstChild
      if (before && node?.nodeType === Node.TEXT_NODE) {
        const length = this.props.text.length
        const offset = (n: number) => n === before.length ? length : Math.min(n, length)
        window.getSelection()?.setBaseAndExtent(node, offset(before.anchor), node, offset(before.focus))
      }
      return
    }
    const field = this.props.fieldRef.current!
    if (!this.fade) {
      // Paint the old view outside the scroller. Even a scrolled, multiline
      // snapshot must never extend the new text's scroll range.
      const snapshot = this.previousText.current!
      snapshot.style.setProperty('--argument-top', -before.scrollTop + 'px')
      snapshot.style.setProperty('--argument-width', before.width + 'px')
      snapshot.dataset.text = previous.text
      field.dataset.swapping = ''
      const enter = live.getAnimations?.().find(a => 'animationName' in a && a.animationName === 'argument-enter')
      if (!enter) { this.stop(); return }
      this.fade = enter
      const done = () => { if (this.fade === enter) this.clearTextFade() }
      void enter.finished.then(done, done)
    }
    // During a burst, the outgoing text and its clock stay put. Only the live
    // value changes. Smooth a real change in the bounded scroll area's height.
    const box = field.closest<HTMLElement>('.argument-scroll')!
    this.resize?.cancel()
    const height = box.getBoundingClientRect().height
    const duration = this.fade.effect?.getComputedTiming().duration
    if (Math.abs(before.height - height) > .5 && typeof duration === 'number') {
      this.resize = box.animate([{ height: before.height + 'px' }, { height: height + 'px' }], { duration, easing: 'ease-out' })
    }
  }

  private clearTextFade = () => {
    this.fade = undefined
    this.props.fieldRef.current?.removeAttribute('data-swapping')
    this.previousText.current?.removeAttribute('data-text')
  }
  private stop = () => {
    this.clearTextFade()
    this.resize?.cancel()
    this.resize = undefined
  }
  private onSelection = () => {
    if (!this.fade) return
    const selection = window.getSelection()
    const field = this.props.fieldRef.current
    if (field && selection?.rangeCount && !selection.isCollapsed && selection.getRangeAt(0).intersectsNode(field)) this.stop()
  }
  componentDidMount() {
    document.addEventListener('selectionchange', this.onSelection)
    this.staticMotion.addEventListener('change', this.stop)
  }
  componentWillUnmount() {
    document.removeEventListener('selectionchange', this.onSelection)
    this.staticMotion.removeEventListener('change', this.stop)
    this.stop()
  }

  render() {
    const { fieldRef, label, text, invalid, skeleton } = this.props
    return <ScrollBox className="argument-scroll" label={label}
      overlay={<span className="argument-previous" ref={this.previousText} aria-hidden="true" inert />}>
      <pre ref={fieldRef} id="validation-message" tabIndex={-1} aria-labelledby="preview-title"
        className={`argument-preview ${invalid ? 'invalid-preview' : ''}`} onFocus={this.stop} onPointerDown={this.stop} onWheel={this.stop}>
        <span className="argument-current" ref={this.currentText}>{skeleton ? <ArgumentSkeleton /> : text}</span>
      </pre>
    </ScrollBox>
  }
}
