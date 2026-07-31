AURA.pages = AURA.pages || {};

AURA.pages.logs = function() {
  AURA.components.render(
    '<div class="page" id="page-logs">' +
    '<div class="page-h1">Logs</div>' +
    '<div class="page-sub">System activity log</div>' +
    '<div class="cta-placeholder"><span class="material-symbols-outlined">article</span><h3>System Logs</h3><p>Live streaming logs with color-coded levels, filtering, and export. Coming soon.</p></div>' +
    '</div>'
  );
  AURA.components.showPage('page-logs');
};
