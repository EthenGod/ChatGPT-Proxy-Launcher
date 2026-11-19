export function SetupSkeleton() {
  return <div className="setup-skeleton" aria-hidden="true">
    <div className="deployment-status"><span className="skeleton-bar skeleton-dot" /><span className="skeleton-bar skeleton-label" /></div>
    <div className="settings-section">
      <div className="skeleton-heading"><span className="skeleton-bar skeleton-label" /></div>
      <div className="settings-card">{[0, 1, 2].map(row =>
        <div className="setting-row" key={row}><span className="skeleton-bar skeleton-label" /><span className="skeleton-bar skeleton-switch" /></div>)}</div>
    </div>
    <div className="settings-section section-heading"><span className="skeleton-bar skeleton-label" /><span className="skeleton-bar skeleton-switch" /></div>
    <div className="settings-section">
      <div className="skeleton-heading"><span className="skeleton-bar skeleton-label" /></div>
      <div className="argument-scroll"><div className="argument-preview"><ArgumentSkeleton /></div></div>
    </div>
    <div className="advanced-section skeleton-disclosure"><span className="skeleton-bar skeleton-label" /></div>
  </div>
}

export function ArgumentSkeleton() {
  return <span className="skeleton-bar skeleton-argument" aria-hidden="true" />
}
