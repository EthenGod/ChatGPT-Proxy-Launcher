import { Toast } from '@base-ui/react/toast'
import { CheckCircle2, Info, TriangleAlert, X } from 'lucide-react'
import type { MessageKey, translator } from '../i18n'
import type { ErrorInfo } from '../features/setup/errors'
import { Button } from './controls'

type NoticeText = { message: MessageKey; values?: Record<string, string> }
export interface NoticeData extends NoticeText { detail?: string | NoticeText; fullDetail?: string | NoticeText; error?: ErrorInfo }
export const createNoticeManager = Toast.createToastManager<NoticeData>
type Manager = ReturnType<typeof createNoticeManager>

function NoticeList({ t, onOpen }: { t: ReturnType<typeof translator>; onOpen: (notice: NoticeData) => void }) {
  const { toasts, close, update } = Toast.useToastManager<NoticeData>()
  // Base UI owns pause/resume. Supplying timeout again resets the full duration,
  // including when the pointer leaves while keyboard focus still pauses the timer.
  const restart = () => toasts.forEach(toast => update(toast.id, { timeout: toast.timeout ?? 3500 }))
  return <Toast.Viewport className="notice-viewport" aria-label={t('ui.notice.label')}
    onMouseLeave={restart} onBlur={event => { if (!event.currentTarget.contains(event.relatedTarget)) restart() }}>
    {toasts.map(toast => toast.data && <Toast.Root key={toast.id} toast={toast} className="notice-toast" swipeDirection={[]}>
      {toast.type === 'error' ? <TriangleAlert size={18} /> : toast.type === 'success' ? <CheckCircle2 size={18} /> : <Info size={18} />}
      <Toast.Content className="notice-content">
        <Toast.Title className="notice-title">{t(toast.data.message, toast.data.values)}</Toast.Title>
        {toast.data.detail && <Toast.Description className="notice-description">{typeof toast.data.detail === 'string'
          ? toast.data.detail : t(toast.data.detail.message, toast.data.detail.values)}</Toast.Description>}
      </Toast.Content>
      <Button className="notice-open" aria-label={`${t('ui.details')}: ${t(toast.data.message, toast.data.values)}`}
        onClick={() => { onOpen(toast.data!); close(toast.id) }} />
      <Toast.Close className="icon-button notice-close" aria-label={t('ui.notice.dismiss')}><X size={16} /></Toast.Close>
    </Toast.Root>)}
  </Toast.Viewport>
}
export function NoticeToasts({ manager, t, onOpen }: { manager: Manager; t: ReturnType<typeof translator>; onOpen: (notice: NoticeData) => void }) {
  return <Toast.Provider toastManager={manager} timeout={3500} limit={1}><NoticeList t={t} onOpen={onOpen} /></Toast.Provider>
}
