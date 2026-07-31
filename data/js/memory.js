AURA.pages = AURA.pages || {};

AURA.pages.memory = function() {
  AURA.components.render(
    '<div class="page" id="page-memory">' +
    '<div class="page-h1">Memory</div>' +
    '<div class="page-sub">Search and manage stored information</div>' +
    '<div class="cta-placeholder"><span class="material-symbols-outlined">memory</span><h3>Memory Manager</h3><p>Browse categories, search past conversations, and manage favorites. Coming soon.</p></div>' +
    '</div>'
  );
  AURA.components.showPage('page-memory');
};
