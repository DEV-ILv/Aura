AURA.components = AURA.components || {};

AURA.components.toast = function(msg, type) {
  var t = AURA.$('toast');
  if (!t) return;
  t.textContent = msg;
  t.className = 'toast show ' + type;
  clearTimeout(t._h);
  t._h = setTimeout(function() {
    t.classList.remove('show');
  }, 4000);
};

AURA.components.toggleSidebar = function() {
  var s = AURA.$('sidebar');
  var o = AURA.$('overlay');
  if (s) s.classList.toggle('open');
  if (o) o.classList.toggle('show');
};

AURA.components.showPage = function(id) {
  AURA.$$('.page').forEach(function(p) {
    p.classList.remove('active');
  });
  var el = AURA.$(id);
  if (el) el.classList.add('active');

  AURA.$$('.sidebar a').forEach(function(a) {
    a.classList.remove('active');
  });
  var link = document.querySelector('.sidebar a[data-page="' + id.replace('page-', '') + '"]');
  if (link) link.classList.add('active');
};

AURA.components.render = function(html) {
  var el = AURA.$('content');
  if (el) el.innerHTML = html;
};
