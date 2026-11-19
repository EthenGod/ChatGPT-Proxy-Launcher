// @vitest-environment jsdom
import { afterEach, describe, expect, it } from 'vitest'
import { cleanup, render, screen } from '@testing-library/react'
import '@testing-library/jest-dom/vitest'
import { ErrorDetails } from '../../frontend/src/components/ErrorDetails'
import { errorPresentation, nativeIssue } from '../../frontend/src/features/setup/errors'
import { translator } from '../../frontend/src/i18n'

afterEach(cleanup)
describe('user-facing native errors', () => {
  it('retains the full HRESULT while explaining its Win32 cause', () => {
    const issue = nativeIssue({nativeError: 0x80070005, message: 'C:\\Example\\locked'})
    render(<ErrorDetails error={issue} t={translator('zh-CN')} />)
    expect(screen.getByText('错误码 0x80070005')).toBeInTheDocument()
    expect(screen.getByRole('heading', {name:'访问被拒绝'})).toBeInTheDocument()
    expect(screen.getByText('当前账户无法读取或修改所选位置。')).toBeInTheDocument()
    expect(screen.getByText(/改用你有写入权限的目录/)).toBeInTheDocument()
    expect(screen.getByText('C:\\Example\\locked')).toBeInTheDocument()
  })
  it('does not guess a cause from message text or an unrelated HRESULT low word', () => {
    expect(errorPresentation(nativeIssue({nativeError:0x80004005})).meaning).toBe('ui.error.unknown.meaning')
    expect(errorPresentation(nativeIssue(new Error('HTTP 403 / error 5'))).code).toBe('')
    expect(errorPresentation({nativeError:0}).code).toBe('')
    for (const code of [-1, 0, NaN, 2.5, 0x100000000, '5']) expect(nativeIssue({nativeError:code}).nativeError).toBeUndefined()
  })
  it.each(['zh-CN','en-US','ja-JP'])('renders a concise explanation and action in %s', language => {
    const t = translator(language)
    render(<ErrorDetails error={{nativeError:32}} t={t} />)
    expect(screen.getByText(t('ui.error.code',{code:'32'}))).toBeInTheDocument()
    expect(screen.getByText(t('ui.error.locked.meaning'))).toBeInTheDocument()
    expect(screen.getByText(t('ui.error.locked.explanation'))).toBeInTheDocument()
    expect(screen.getByText(new RegExp(t('ui.error.locked.action').slice(0,12)))).toBeInTheDocument()
  })
})
