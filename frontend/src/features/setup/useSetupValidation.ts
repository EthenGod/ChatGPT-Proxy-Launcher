import { useCallback, useEffect, useMemo, useRef, useState } from 'react'
import { translator } from '../../i18n'
import type { SetupOptions } from '../../platform/types'
import type { SetupService, Validation } from './contract'
import { nativeIssue } from './errors'

interface CheckedOptions {
  service: SetupService
  options: SetupOptions
  language: string
  value: Validation
}

export function useSetupValidation(service: SetupService, options: SetupOptions | null, language: string, paused: boolean) {
  const [last, setLast] = useState<CheckedOptions | null>(null)
  const previousOptions = useRef<SetupOptions | null>(null)
  const resetValidation = useCallback(() => { previousOptions.current = null; setLast(null) }, [])
  const check = last?.service === service ? last : null
  const match = check?.options === options ? check : null
  const confirmed = match?.value.valid === true
  const local = useMemo(() => options && !('validate' in service) ? service.preview(options) : null, [options, service])

  useEffect(() => {
    const previous = previousOptions.current
    previousOptions.current = paused ? null : options
    if (!options || !('validate' in service) || paused || confirmed) return
    // Coalesce text edits. Loading, toggles and protocol selection can validate on the next task.
    const textChanged = previous && (previous.host !== options.host || previous.port !== options.port
      || previous.directory !== options.directory || previous.bypass !== options.bypass || previous.extra !== options.extra)
    let request: AbortController | undefined
    const timer = window.setTimeout(async () => {
      request = new AbortController()
      const { signal } = request
      let value: Validation
      try {
        value = await service.validate(options, signal)
      } catch (cause) {
        if (signal.aborted) return
        const problem = nativeIssue(cause)
        value = { valid: false, arguments: '', errors: {}, nativeError: problem.nativeError,
          detail: problem.detail || translator(language)('ui.save.invalid') }
      }
      request = undefined
      if (!signal.aborted) setLast({ service, options, language, value })
    }, textChanged ? 80 : 0)
    return () => { window.clearTimeout(timer); request?.abort() }
  }, [service, options, language, paused, confirmed])

  // Successful arguments are language-independent. Errors can contain localized native text.
  const validation = local ?? (match && (confirmed || match.language === language) ? match.value : null)
  // Keep errors for identical inputs while their translation is refreshed. Only a
  // successful preview may follow an edit, and it never authorizes a save.
  const previewValue = validation ?? match?.value ?? (check?.value.valid ? check.value : null)
  return { validation, previewValue, resetValidation }
}
