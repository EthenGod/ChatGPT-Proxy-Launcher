import { Children, useEffect, useId, useRef, type ComponentProps, type ReactElement, type ReactNode, type InputHTMLAttributes, type RefObject } from 'react'
import { Button as BaseButton } from '@base-ui/react/button'
import { Switch } from '@base-ui/react/switch'
import { Dialog } from '@base-ui/react/dialog'
import { Menu } from '@base-ui/react/menu'
import { Tooltip } from '@base-ui/react/tooltip'
import { Check, ChevronDown } from 'lucide-react'
import { ScrollBox } from './ScrollBox'

export function Button({ children, ...props }: ComponentProps<typeof BaseButton>) {
  return <BaseButton {...props}>{typeof children === 'function' ? children : Children.map(children, child =>
    typeof child === 'string' || typeof child === 'number' ? <span data-motion-text>{child}</span> : child)}</BaseButton>
}

export function HintPopup({ children }: { children: ReactNode }) {
  return <Tooltip.Portal><Tooltip.Positioner sideOffset={8} className="tooltip-positioner">
    <Tooltip.Popup className="tooltip-popup">{children}</Tooltip.Popup>
  </Tooltip.Positioner></Tooltip.Portal>
}

export function Hint({ label, children }: { label: string; children: ReactElement }) {
  return <Tooltip.Root>
    <Tooltip.Trigger render={children} delay={500} />
    <HintPopup>{label}</HintPopup>
  </Tooltip.Root>
}

export function SwitchControl({ id, label, labelledBy, describedBy, controls, checked, disabled, onChange }: {
  id?: string; label?: string; labelledBy?: string; describedBy?: string; controls?: string;
  checked: boolean; disabled?: boolean; onChange: (checked: boolean) => void
}) {
  return <Switch.Root id={id} aria-label={label} aria-labelledby={labelledBy} aria-describedby={describedBy} aria-controls={controls}
    checked={checked} disabled={disabled} onCheckedChange={onChange} className="switch">
    <Switch.Thumb className="switch-thumb" />
  </Switch.Root>
}
export function Toggle({ label, checked, disabled, onChange, help, badge, className = '' }: {
  label: string; checked: boolean; disabled?: boolean; onChange: (checked: boolean) => void; help?: ReactNode; badge?: string; className?: string
}) {
  const id = useId()
  return <div className={`setting-row ${className}`}>
    <div className="setting-label"><span id={`${id}-label`} data-motion-text>{label}</span>{help}{badge && <span id={`${id}-badge`} className="setting-badge"><span data-motion-text>{badge}</span></span>}</div>
    <SwitchControl id={id} labelledBy={`${id}-label`} describedBy={badge ? `${id}-badge` : undefined} checked={checked} disabled={disabled} onChange={onChange} />
  </div>
}
export function TextField({ label, error, errorDescriptionId, suffix, labelActions, className = '', ...props }: InputHTMLAttributes<HTMLInputElement> & {
  label: string; error?: string; errorDescriptionId?: string; suffix?: ReactNode; labelActions?: ReactNode
}) {
  const generated = useId()
  const id = props.id ?? generated
  return <div className={`field ${className}`}>
    <div className="field-heading"><label htmlFor={id}>{label}</label>{labelActions}</div>
    <div className="input-wrap"><input {...props} id={id} aria-invalid={!!error} aria-describedby={error ? errorDescriptionId ?? `${id}-error` : undefined} />{suffix}</div>
    {error && !errorDescriptionId && <span id={`${id}-error`} className="field-error">{error}</span>}
  </div>
}
export function ChoiceMenu({ label, value, items, onChange, disabled, icon, variant = 'default', changeAfterClose = false }: {
  label: string; value: string; items: Array<{ value: string; label: string }>;
  onChange: (value: string) => void; disabled?: boolean; icon?: ReactNode; variant?: 'default' | 'icon' | 'quiet'; changeAfterClose?: boolean
}) {
  const pendingChange = useRef<string | null>(null)
  const changeFrame = useRef<number | null>(null)
  useEffect(() => () => { if (changeFrame.current !== null) cancelAnimationFrame(changeFrame.current) }, [])
  return <Menu.Root onOpenChangeComplete={open => {
    // The completion callback precedes React's portal removal. Capture on the
    // next frame, after closing and focus restoration have both committed.
    if (!open && pendingChange.current !== null) {
      const next = pendingChange.current
      pendingChange.current = null
      if (changeFrame.current !== null) cancelAnimationFrame(changeFrame.current)
      changeFrame.current = requestAnimationFrame(() => { changeFrame.current = null; onChange(next) })
    }
  }}>
    <Hint label={label}><Menu.Trigger className={`menu-trigger ${variant === 'icon' ? 'icon-button' : 'control-button'} ${variant === 'quiet' ? 'quiet-button' : ''}`} disabled={disabled} aria-label={label}>
      {icon}{variant !== 'icon' && <><span data-motion-text>{items.find(item => item.value === value)?.label}</span><ChevronDown size={14} /></>}
    </Menu.Trigger></Hint>
    <Menu.Portal><Menu.Positioner sideOffset={6} align="end" className="menu-positioner">
      <Menu.Popup className="menu-popup">
        <ScrollBox className="menu-scroll" tabIndex={-1} label={label}><Menu.RadioGroup value={value} onValueChange={next => {
          if (changeAfterClose) pendingChange.current = next
          else onChange(next)
        }}>
          {items.map(item => <Menu.RadioItem className="menu-item" key={item.value} value={item.value} closeOnClick>
            <span>{item.label}</span><Menu.RadioItemIndicator><Check size={16} /></Menu.RadioItemIndicator>
          </Menu.RadioItem>)}
        </Menu.RadioGroup></ScrollBox>
      </Menu.Popup>
    </Menu.Positioner></Menu.Portal>
  </Menu.Root>
}
export function Modal({ open, title, description, children, actions, variant, onClose, focusReturn, initialFocus }: {
  open: boolean; title: string; description?: string; children?: ReactNode; actions: ReactNode; variant?: 'help';
  onClose: () => void; focusReturn: RefObject<HTMLElement | null>; initialFocus?: RefObject<HTMLElement | null>
}) {
  const popup = useRef<HTMLDivElement | null>(null)
  return <Dialog.Root open={open} onOpenChange={open => { if (!open) onClose() }}>
    <Dialog.Portal><Dialog.Backdrop className="dialog-backdrop" />
      <Dialog.Popup ref={popup} className="dialog" data-variant={variant} finalFocus={focusReturn} initialFocus={initialFocus ?? (() =>
        popup.current?.querySelector<HTMLElement>('[role="radio"][aria-checked="true"], .dialog-actions button:not([disabled])') ?? false)}>
        <Dialog.Title className="dialog-title">{title}</Dialog.Title>
        <ScrollBox className="dialog-scroll" contentClassName="dialog-content" label={title}>
          {description && (variant === 'help'
            ? <Dialog.Description render={<div />} className="dialog-description help-paragraphs">
              {description.split(/\r\n?|\n/).map(line => line.trim()).filter(Boolean).map((line, index) => <p key={index}>{line}</p>)}
            </Dialog.Description>
            : <Dialog.Description className="dialog-description">{description}</Dialog.Description>)}
          {children && <div className="dialog-body">{children}</div>}
        </ScrollBox>
        <div className="dialog-actions">{actions}</div>
      </Dialog.Popup>
    </Dialog.Portal>
  </Dialog.Root>
}
