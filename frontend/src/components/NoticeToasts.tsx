import { Toast } from '@base-ui/react/toast'
import { CheckCircle2, Info, TriangleAlert, X } from 'lucide-react'
import type { MessageKey, translator } from '../i18n'
import type { ErrorInfo } from '../features/setup/errors'
import { Button } from './controls'

type NoticeText = { message: MessageKey; values?: Record<string, string> }
export interface NoticeData extends NoticeText { detail?: string | NoticeText; error?: ErrorInfo }
export const createNoticeManager = () => Toast.createToastManager<NoticeData>()
type Manager = ReturnType<typeof createNoticeManager>

function NoticeList({ t, onError }: { t: ReturnType<typeof translator>; onError: (error: ErrorInfo) => void }) {
  const { toasts, close } = Toast.useToastManager<NoticeData>()
  return <Toast.Viewport className="notice-viewport" aria-label={t('ui.notice.label')}>
    {toasts.map(toast => toast.data && <Toast.Root key={toast.id} toast={toast} className="notice-toast" swipeDirection={[]}>
      {toast.type === 'error' ? <TriangleAlert size={18} /> : toast.type === 'success' ? <CheckCircle2 size={18} /> : <Info size={18} />}
      <Toast.Content className="notice-content">
        <Toast.Title className="notice-title">{t(toast.data.message, toast.data.values)}</Toast.Title>
        {toast.data.detail && <Toast.Description className="notice-description">{typeof toast.data.detail === 'string'
          ? toast.data.detail : t(toast.data.detail.message, toast.data.detail.values)}</Toast.Description>}
        {toast.data.error && <Button className="text-button" onClick={() => { onError(toast.data!.error!); close(toast.id) }}>{t('ui.details')}</Button>}
      </Toast.Content>
      <Toast.Close className="icon-button notice-close" aria-label={t('ui.notice.dismiss')}><X size={16} /></Toast.Close>
    </Toast.Root>)}
  </Toast.Viewport>
}
export function NoticeToasts({ manager, t, onError }: { manager: Manager; t: ReturnType<typeof translator>; onError: (error: ErrorInfo) => void }) {
  return <Toast.Provider toastManager={manager} timeout={6000} limit={1}><NoticeList t={t} onError={onError} /></Toast.Provider>
}
