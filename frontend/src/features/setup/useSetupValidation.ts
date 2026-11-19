import { useCallback, useEffect, useMemo, useState } from 'react'
import { translator } from '../../i18n'
import type { SetupOptions } from '../../platform/types'
import type { SetupService, Validation } from './contract'
import { nativeIssue } from './errors'
import { previewOptions } from './preview'

interface NativeCheck {
  service: SetupService
  options: SetupOptions
  language: string
  value: Validation
}

export function useSetupValidation(service: SetupService, options: SetupOptions | null, language: string, paused: boolean) {
  const [last, setLast] = useState<NativeCheck | null>(null)
  const resetValidation = useCallback(() => setLast(null), [])
  const check = last?.service === service ? last : null
  const match = check?.options === options ? check : null
  const confirmed = match?.value.valid === true
  const local = useMemo(() => options && !service.validate ? previewOptions(options) : null, [options, service])

  useEffect(() => {
    const validate = service.validate
    if (!options || !validate || paused || confirmed) return
    let current = true
    const timer = window.setTimeout(async () => {
      let value: Validation
      try {
        value = await validate.call(service, options)
      } catch (cause) {
        const problem = nativeIssue(cause)
        value = { valid: false, arguments: '', errors: {}, nativeError: problem.nativeError,
          detail: problem.detail || translator(language)('ui.save.invalid') }
      }
      if (current) setLast({ service, options, language, value })
    }, 180)
    return () => { current = false; window.clearTimeout(timer) }
  }, [service, options, language, paused, confirmed])

  // Successful arguments are language-independent. Errors can contain localized native text.
  const validation: Validation | null = local ?? (match && (confirmed || match.language === language) ? match.value : null)
  // Only the preview may keep old arguments while new input is being checked.
  const previewValue = validation ?? (check?.value.valid ? check.value : null)
  return { validation, previewValue, resetValidation }
}
