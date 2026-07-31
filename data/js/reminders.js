AURA.pages = AURA.pages || {};

AURA.pages.reminders = function() {
  AURA.components.render(
    '<div class="page" id="page-reminders">' +
    '<div class="page-h1">Reminders</div>' +
    '<div class="page-sub">Upcoming and completed reminders</div>' +
    '<div class="cta-placeholder"><span class="material-symbols-outlined">notifications</span><h3>Reminders</h3><p>Set and manage reminders with calendar integration. Coming soon.</p></div>' +
    '</div>'
  );
  AURA.components.showPage('page-reminders');
};
