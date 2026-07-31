AURA.pages = AURA.pages || {};

AURA.pages.chat = function() {
  AURA.components.render(
    '<div class="page" id="page-chat">' +
    '<div class="page-h1">Chat</div>' +
    '<div class="page-sub">Conversation with AURA AI</div>' +
    '<div class="cta-placeholder"><span class="material-symbols-outlined">chat</span><h3>Chat Interface</h3><p>Send messages and receive AI-powered responses. Coming soon in a future update.</p></div>' +
    '</div>'
  );
  AURA.components.showPage('page-chat');
};
