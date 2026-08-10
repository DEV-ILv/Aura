AURA.pages = AURA.pages || {};

AURA.errors = {
  filter: 'all',
  timer: null
};

AURA.errors.sevClass = function(sev) {
  return String(sev || 'INFO').toLowerCase();
};

AURA.errors.sevIcon = function(sev) {
  switch (String(sev || '').toUpperCase()) {
    case 'CRITICAL': return 'warning';
    case 'ERROR': return 'error';
    case 'WARNING': return 'warning';
    default: return 'info';
  }
};

AURA.errors.sevChip = function(sev) {
  switch (String(sev || '').toUpperCase()) {
    case 'CRITICAL': return 'Critical';
    case 'ERROR': return 'Error';
    case 'WARNING': return 'Warning';
    default: return 'Info';
  }
};

AURA.errors.load = function() {
  AURA.api.getErrors().then(function(data) {
    if (!data) return;
    AURA.state.errors = data;
    if (location.hash === '#errors') AURA.errors.render();
  });
};

AURA.errors.filtered = function() {
  var list = (AURA.state.errors && AURA.state.errors.errors) || [];
  if (AURA.errors.filter === 'all') return list;
  return list.filter(function(e) {
    return String(e.severity).toLowerCase() === AURA.errors.filter;
  });
};

AURA.errors.countBy = function(sev) {
  var list = (AURA.state.errors && AURA.state.errors.errors) || [];
  return list.filter(function(e) {
    return String(e.severity).toUpperCase() === String(sev).toUpperCase() && e.active;
  }).length;
};

AURA.errors.elapsed = function(ms) {
  if (!ms) return '--';
  var upSec = AURA.state.status && AURA.state.status.uptime;
  if (!upSec) return (Math.floor(ms / 1000)) + 's uptime';
  // Firmware timestamps are millis since boot; reference against current
  // boot uptime (seconds) so the label stays valid.
  if (ms > upSec * 1000 + 5000) return 'previous boot';
  var age = upSec - Math.floor(ms / 1000);
  if (age < 0) age = 0;
  if (age < 60) return age + 's ago';
  if (age < 3600) return Math.floor(age / 60) + 'm ago';
  if (age < 86400) return Math.floor(age / 3600) + 'h ago';
  return Math.floor(age / 86400) + 'd ago';
};

AURA.errors.render = function() {
  var data = AURA.state.errors || {};
  var meta = data.meta || {};
  var health = meta.health || 'HEALTHY';
  var active = meta.active || 0;
  var total = meta.total || 0;
  var crt = active === 0 ? 0 : (data.errors || []).filter(function(e) {
    return e.active && String(e.severity).toUpperCase() === 'CRITICAL';
  }).length;
  var err = active === 0 ? 0 : (data.errors || []).filter(function(e) {
    return e.active && String(e.severity).toUpperCase() === 'ERROR';
  }).length;
  var wrn = active === 0 ? 0 : (data.errors || []).filter(function(e) {
    return e.active && String(e.severity).toUpperCase() === 'WARNING';
  }).length;

  var heroIcon = health === 'HEALTHY' ? 'verified_user' : 'report';
  var heroSub = {
    HEALTHY: 'All systems nominal',
    WARNING: 'Non-critical faults active',
    ERROR: 'Errors require attention',
    CRITICAL: 'Critical faults detected'
  }[health] || '';

  var html =
    '<div class="page" id="page-errors">' +
    '<div class="header-row">' +
    '<div><div class="page-h1">Errors</div><div class="page-sub">Diagnostic events &amp; device health</div></div>' +
    '<button class="btn btn-ghost btn-sm" onclick="AURA.errors.clearAll()"><span class="material-symbols-outlined">delete_sweep</span>Clear</button>' +
    '</div>' +

    '<div class="health-banner health-hero ' + AURA.errors.sevClass(health) + '">' +
    '<div class="hb-icon"><span class="material-symbols-outlined">' + heroIcon + '</span></div>' +
    '<div><div class="hb-name">' + health + '</div><div class="hb-sub">' + heroSub + '</div></div>' +
    '<div style="margin-left:auto;text-align:right">' +
    '<div class="hb-sub">' + active + ' active / ' + total + ' total</div>' +
    '</div></div>' +

    '<div class="toolbar">' +
    '<div class="sev-chips">' +
    '<button class="chip' + (AURA.errors.filter === 'all' ? ' active' : '') + '" onclick="AURA.errors.setFilter(\'all\')">All<span class="n">' + total + '</span></button>' +
    '<button class="chip' + (AURA.errors.filter === 'critical' ? ' active' : '') + '" onclick="AURA.errors.setFilter(\'critical\')">Critical<span class="n">' + crt + '</span></button>' +
    '<button class="chip' + (AURA.errors.filter === 'error' ? ' active' : '') + '" onclick="AURA.errors.setFilter(\'error\')">Errors<span class="n">' + err + '</span></button>' +
    '<button class="chip' + (AURA.errors.filter === 'warning' ? ' active' : '') + '" onclick="AURA.errors.setFilter(\'warning\')">Warnings<span class="n">' + wrn + '</span></button>' +
    '</div></div>' +

    '<div class="err-list" id="err-list">' + AURA.errors.listHtml() + '</div>' +
    '</div>';

  AURA.components.render(html);
  AURA.components.showPage('page-errors');
};

AURA.errors.listHtml = function() {
  var list = AURA.errors.filtered();
  if (!list.length) {
    return '<div class="err-empty"><span class="material-symbols-outlined">check_circle</span>No events match this filter.</div>';
  }

  var out = '';
  list.forEach(function(e) {
    var sev = String(e.severity || 'INFO').toUpperCase();
    var scls = AURA.errors.sevClass(sev);
    var state = e.active ? 'active' : 'resolved';
    var stateLbl = e.active ? 'Active' : 'Resolved';
    var occ = e.occurrences || 1;
    out +=
      '<div class="err-item ' + scls + (e.acknowledged ? ' acked' : '') + '" data-id="' + e.id + '">' +
      '<div class="err-head" onclick="AURA.errors.toggle(this)">' +
      '<span class="err-dot ' + scls + '"></span>' +
      '<div style="min-width:0;flex:1">' +
      '<div class="err-title">' + (e.title || e.code || e.id) + '</div>' +
      '<div class="err-meta">' + (e.component || '?') + ' / ' + (e.code || '') + '</div>' +
      '</div>' +
      '<div class="err-badge ' + state + '">' + stateLbl + '</div>' +
      '<div class="err-meta">' + occ + 'x</div>' +
      '<span class="material-symbols-outlined err-chev">expand_more</span>' +
      '</div>' +
      '<div class="err-body">' +
      '<div class="ev-line"><strong>Title:</strong> ' + (e.title || '—') + '</div>' +
      (e.message ? '<div class="ev-line"><strong>Detail:</strong> ' + (e.message || '') + '</div>' : '') +
      '<div class="ev-line"><strong>First seen:</strong> ' + AURA.errors.elapsed(e.first_seen_ms) + ' &middot; <strong>Last seen:</strong> ' + AURA.errors.elapsed(e.last_seen_ms) + '</div>' +
      '<div class="ev-line"><strong>ID:</strong> <span style="font-family:var(--font-mono)">' + e.id + '</span> &middot; <strong>Occurrences:</strong> ' + occ + '</div>' +
      '</div>' +
      '<div class="err-actions">' +
      '<button class="btn btn-ghost btn-sm" onclick="AURA.errors.ack(\'' + e.id + '\')">' +
      (e.acknowledged
        ? '<span class="material-symbols-outlined">done</span>Acknowledged'
        : '<span class="material-symbols-outlined">done_all</span>Acknowledge') +
      '</button></div>' +
      '</div>';
  });
  return out;
};

AURA.errors.toggle = function(el) {
  el.parentElement.classList.toggle('open');
};

AURA.errors.setFilter = function(f) {
  AURA.errors.filter = f;
  AURA.errors.render();
};

AURA.errors.ack = function(id) {
  AURA.api.ackError(id).then(function(r) {
    if (r && r.ok) {
      AURA.components.toast('Event acknowledged', 'success');
      AURA.errors.load();
    } else {
      AURA.components.toast('Acknowledge failed', 'error');
    }
  });
};

AURA.errors.clearAll = function() {
  if (!confirm('Clear all diagnostic events? This cannot be undone.')) return;
  AURA.api.clearErrors().then(function(r) {
    if (r && r.ok) {
      AURA.components.toast('All events cleared', 'success');
      AURA.errors.load();
    } else {
      AURA.components.toast('Clear failed', 'error');
    }
  });
};

AURA.pages.errors = function() {
  if (AURA.errors.timer) {
    clearInterval(AURA.errors.timer);
    AURA.errors.timer = null;
  }
  AURA.errors.load();
  AURA.errors.timer = setInterval(function() {
    if (location.hash === '#errors') AURA.errors.load();
  }, 10000);
};