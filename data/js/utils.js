const AURA = window.AURA || {};

AURA.$ = function(id) {
  return document.getElementById(id);
};

AURA.$$ = function(sel) {
  return document.querySelectorAll(sel);
};

AURA.fmtUptime = function(s) {
  if (!s) return '--';
  const d = Math.floor(s / 86400);
  const h = Math.floor((s % 86400) / 3600);
  const m = Math.floor((s % 3600) / 60);
  if (d > 0) return d + 'd ' + h + 'h ' + m + 'm';
  if (h > 0) return h + 'h ' + m + 'm';
  return m + 'm';
};

AURA.fmtBytes = function(b) {
  if (b > 1048576) return (b / 1048576).toFixed(1) + ' MB';
  if (b > 1024) return (b / 1024).toFixed(0) + ' KB';
  return b + ' B';
};

AURA.fmtSignal = function(r) {
  const a = Math.abs(r);
  if (a < 50) return 'Excellent';
  if (a < 60) return 'Good';
  if (a < 70) return 'Fair';
  return 'Weak';
};
