import { useCallback, useEffect, useMemo, useRef, useState } from 'react'
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
  const previousOptions = useRef<SetupOptions | null>(null)
  const resetValidation = useCallback(() => { previousOptions.current = null; setLast(null) }, [])
  const check = last?.service === service ? last : null
  const match = check?.options === options ? check : null
  const confirmed = match?.value.valid === true
  const local = useMemo(() => options && !service.validate ? previewOptions(options) : null, [options, service])

  useEffect(() => {
    const previous = previousOptions.current
    previousOptions.current = paused ? null : options
    const validate = service.validate
    if (!options || !validate || paused || confirmed) return
    // Coalesce text edits. Loading, toggles and protocol selection can validate on the next task.
    const textChanged = previous && (previous.host !== options.host || previous.port !== options.port
      || previous.directory !== options.directory || previous.bypass !== options.bypass || previous.extra !== options.extra)
    let request: AbortController | undefined
    const timer = window.setTimeout(async () => {
      request = new AbortController()
      const { signal } = request
      let value: Validation
      try {
        value = await validate.call(service, options, signal)
      } catch (cause) {
        if (signal.aborted) return
        const problem = nativeIssue(cause)
        value = { valid: false, arguments: '', errors: {}, nativeError: problem.nativeError,
          detail: problem.detail || translator(language)('ui.save.invalid') }
      }
      if (!signal.aborted) setLast({ service, options, language, value })
    }, textChanged ? 80 : 0)
    return () => { window.clearTimeout(timer); request?.abort() }
  }, [service, options, language, paused, confirmed])

  // Successful arguments are language-independent. Errors can contain localized native text.
  const validation: Validation | null = local ?? (match && (confirmed || match.language === language) ? match.value : null)
  // Only the preview may keep old arguments while new input is being checked.
  const previewValue = validation ?? (check?.value.valid ? check.value : null)
  return { validation, previewValue, resetValidation }
}
