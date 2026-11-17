import { Children, useId, useRef, type ComponentProps, type ReactElement, type ReactNode, type InputHTMLAttributes, type RefObject } from 'react'
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

export function Hint({ label, children }: { label: string; children: ReactElement }) {
  return <Tooltip.Root>
    <Tooltip.Trigger render={children} delay={500} />
    <Tooltip.Portal><Tooltip.Positioner sideOffset={8} className="tooltip-positioner">
      <Tooltip.Popup className="tooltip-popup">{label}</Tooltip.Popup>
    </Tooltip.Positioner></Tooltip.Portal>
  </Tooltip.Root>
}

export function SwitchControl({ id, label, describedBy, controls, checked, disabled, onChange }: {
  id?: string; label?: string; describedBy?: string; controls?: string;
  checked: boolean; disabled?: boolean; onChange: (checked: boolean) => void
}) {
  return <Switch.Root id={id} aria-label={label} aria-describedby={describedBy} aria-controls={controls}
    checked={checked} disabled={disabled} onCheckedChange={onChange} className="switch">
    <Switch.Thumb className="switch-thumb" />
  </Switch.Root>
}
export function Toggle({ label, checked, disabled, onChange, help, badge }: {
  label: string; checked: boolean; disabled?: boolean; onChange: (checked: boolean) => void; help?: ReactNode; badge?: string
}) {
  const id = useId()
  return <div className="setting-row">
    <div className="setting-label"><label htmlFor={id}>{label}</label>{help}{badge && <span id={`${id}-badge`} className="setting-badge"><span data-motion-text>{badge}</span></span>}</div>
    <SwitchControl id={id} describedBy={badge ? `${id}-badge` : undefined} checked={checked} disabled={disabled} onChange={onChange} />
  </div>
}
export function TextField({ label, error, suffix, labelActions, className = '', ...props }: InputHTMLAttributes<HTMLInputElement> & {
  label: string; error?: string; suffix?: ReactNode; labelActions?: ReactNode
}) {
  const generated = useId()
  const id = props.id ?? generated
  return <div className={`field ${className}`}>
    <div className="field-heading"><label htmlFor={id}>{label}</label>{labelActions}</div>
    <div className="input-wrap"><input {...props} id={id} aria-invalid={!!error} aria-describedby={error ? `${id}-error` : undefined} />{suffix}</div>
    {error && <span id={`${id}-error`} className="field-error">{error}</span>}
  </div>
}
export function ChoiceMenu({ label, value, items, onChange, disabled, icon, variant = 'default' }: {
  label: string; value: string; items: Array<{ value: string; label: string }>;
  onChange: (value: string) => void; disabled?: boolean; icon?: ReactNode; variant?: 'default' | 'icon' | 'quiet'
}) {
  return <Menu.Root>
    <Hint label={label}><Menu.Trigger className={`menu-trigger ${variant === 'icon' ? 'icon-button' : 'control-button'} ${variant === 'quiet' ? 'quiet-button' : ''}`} disabled={disabled} aria-label={label}>
      {icon}{variant !== 'icon' && <><span data-motion-text>{items.find(item => item.value === value)?.label}</span><ChevronDown size={14} /></>}
    </Menu.Trigger></Hint>
    <Menu.Portal><Menu.Positioner sideOffset={6} align="end" className="menu-positioner">
      <Menu.Popup className="menu-popup">
        <ScrollBox className="menu-scroll" tabIndex={-1} label={label}><Menu.RadioGroup value={value} onValueChange={onChange}>
          {items.map(item => <Menu.RadioItem className="menu-item" key={item.value} value={item.value} closeOnClick>
            <span>{item.label}</span><Menu.RadioItemIndicator><Check size={16} /></Menu.RadioItemIndicator>
          </Menu.RadioItem>)}
        </Menu.RadioGroup></ScrollBox>
      </Menu.Popup>
    </Menu.Positioner></Menu.Portal>
  </Menu.Root>
}
export function Modal({ open, title, description, children, actions, onClose, focusReturn, initialFocus }: {
  open: boolean; title: string; description?: string; children?: ReactNode; actions: ReactNode;
  onClose: () => void; focusReturn: RefObject<HTMLElement | null>; initialFocus?: RefObject<HTMLElement | null>
}) {
  const popup = useRef<HTMLDivElement | null>(null)
  return <Dialog.Root open={open} onOpenChange={open => { if (!open) onClose() }}>
    <Dialog.Portal><Dialog.Backdrop className="dialog-backdrop" />
      <Dialog.Popup ref={popup} className="dialog" finalFocus={focusReturn} initialFocus={initialFocus ?? (() =>
        popup.current?.querySelector<HTMLElement>('[role="radio"][aria-checked="true"], .dialog-actions button:not([disabled])') ?? false)}>
        <ScrollBox className="dialog-scroll" contentClassName="dialog-content" label={title}>
          <Dialog.Title className="dialog-title">{title}</Dialog.Title>
          {description && <Dialog.Description className="dialog-description">{description}</Dialog.Description>}
          {children && <div className="dialog-body">{children}</div>}
        </ScrollBox>
        <div className="dialog-actions">{actions}</div>
      </Dialog.Popup>
    </Dialog.Portal>
  </Dialog.Root>
}
