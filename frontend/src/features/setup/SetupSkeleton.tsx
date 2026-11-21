import { useEffect, useState } from 'react'

// Reserve the geometry immediately. Only the visual cue waits, never the result.
export function useDelayedLoading(pending: boolean) {
  const [visible, setVisible] = useState(false)
  useEffect(() => {
    if (!pending) { setVisible(false); return }
    const timer = window.setTimeout(() => setVisible(true), 180)
    return () => window.clearTimeout(timer)
  }, [pending])
  return pending && visible
}

export function SetupSkeleton({ visible }: { visible: boolean }) {
  return <div className="setup-skeleton" data-skeleton-wait={!visible || undefined} aria-hidden="true">
    <div className="deployment-status"><span className="skeleton-bar skeleton-dot" /><span className="skeleton-bar skeleton-label" /></div>
    <div className="settings-section">
      <div className="skeleton-heading"><span className="skeleton-bar skeleton-label" /></div>
      <div className="settings-card">{[0, 1, 2].map(row =>
        <div className="setting-row" key={row}><span className="skeleton-bar skeleton-label" /><span className="skeleton-bar skeleton-switch" /></div>)}</div>
    </div>
    <div className="settings-section section-heading"><span className="skeleton-bar skeleton-label" /><span className="skeleton-bar skeleton-switch" /></div>
    {/* The saved proxy setting determines where the preview starts. Reserve the
        collapsed layout, but paint its placeholder only once that position is known. */}
    <div className="skeleton-remainder">
      <div className="settings-section">
        <div className="skeleton-heading" />
        <div className="argument-scroll"><div className="argument-preview" /></div>
      </div>
      <div className="advanced-section"><div className="advanced-trigger" /></div>
    </div>
  </div>
}

export function ArgumentSkeleton() {
  return <span className="skeleton-bar skeleton-argument" aria-hidden="true" />
}
