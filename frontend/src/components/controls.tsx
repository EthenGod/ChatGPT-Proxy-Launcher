import { useId, type ReactNode, type InputHTMLAttributes, type RefObject } from 'react'
import { Button } from '@base-ui/react/button'
import { Switch } from '@base-ui/react/switch'
import { Dialog } from '@base-ui/react/dialog'
import { Menu } from '@base-ui/react/menu'
import { Check, ChevronDown } from 'lucide-react'

export function Toggle({ label, checked, disabled, onChange, help }: {
  label: string; checked: boolean; disabled?: boolean; onChange: (checked: boolean) => void; help?: ReactNode
}) {
  const id = useId()
  return <div className="setting-row">
    <div className="setting-label"><label htmlFor={id}>{label}</label>{help}</div>
    <Switch.Root id={id} checked={checked} disabled={disabled} onCheckedChange={onChange} className="switch">
      <Switch.Thumb className="switch-thumb" />
    </Switch.Root>
  </div>
}
export function TextField({ label, error, suffix, className = '', ...props }: InputHTMLAttributes<HTMLInputElement> & {
  label: string; error?: string; suffix?: ReactNode
}) {
  const generated = useId()
  const id = props.id ?? generated
  return <div className={`field ${className}`}>
    <label htmlFor={id}>{label}</label>
    <div className="input-wrap"><input {...props} id={id} aria-invalid={!!error} aria-describedby={error ? `${id}-error` : undefined} />{suffix}</div>
    {error && <span id={`${id}-error`} className="field-error">{error}</span>}
  </div>
}
export function ChoiceMenu({ label, value, items, onChange, disabled, icon }: {
  label: string; value: string; items: Array<{ value: string; label: string }>;
  onChange: (value: string) => void; disabled?: boolean; icon?: ReactNode
}) {
  return <Menu.Root>
    <Menu.Trigger className="control-button menu-trigger" disabled={disabled} aria-label={label}>
      {icon}<span>{items.find(item => item.value === value)?.label}</span><ChevronDown size={14} />
    </Menu.Trigger>
    <Menu.Portal><Menu.Positioner sideOffset={6} align="end" className="menu-positioner">
      <Menu.Popup className="menu-popup">
        <Menu.RadioGroup value={value} onValueChange={onChange}>
          {items.map(item => <Menu.RadioItem className="menu-item" key={item.value} value={item.value} closeOnClick>
            <span>{item.label}</span><Menu.RadioItemIndicator><Check size={15} /></Menu.RadioItemIndicator>
          </Menu.RadioItem>)}
        </Menu.RadioGroup>
      </Menu.Popup>
    </Menu.Positioner></Menu.Portal>
  </Menu.Root>
}
export function Modal({ open, title, description, children, actions, onClose, focusReturn, initialFocus }: {
  open: boolean; title: string; description?: string; children?: ReactNode; actions: ReactNode;
  onClose: () => void; focusReturn: RefObject<HTMLElement | null>; initialFocus?: RefObject<HTMLElement | null>
}) {
  return <Dialog.Root open={open} onOpenChange={open => { if (!open) onClose() }}>
    <Dialog.Portal><Dialog.Backdrop className="dialog-backdrop" />
      <Dialog.Popup className="dialog" finalFocus={focusReturn} initialFocus={initialFocus}>
        <Dialog.Title className="dialog-title">{title}</Dialog.Title>
        {description && <Dialog.Description className="dialog-description">{description}</Dialog.Description>}
        {children && <div className="dialog-body">{children}</div>}
        <div className="dialog-actions">{actions}</div>
      </Dialog.Popup>
    </Dialog.Portal>
  </Dialog.Root>
}
export { Button }
