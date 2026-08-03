AURA.ws = AURA.ws || {};

AURA.ws.connected = false;
AURA.ws.authed = false;
AURA.ws.socket = null;
AURA.ws.reconnectTimer = null;

AURA.ws.close = function() {
  AURA.ws.connected = false;
  AURA.ws.authed = false;
  if (AURA.ws.reconnectTimer) {
    clearTimeout(AURA.ws.reconnectTimer);
    AURA.ws.reconnectTimer = null;
  }
  if (AURA.ws.socket) {
    try { AURA.ws.socket.close(); } catch (e) {}
    AURA.ws.socket = null;
  }
};

AURA.ws.connect = function() {
  if (!AURA.session || !AURA.session.authenticated) return;
  if (AURA.ws.socket &&
      (AURA.ws.socket.readyState === WebSocket.OPEN ||
       AURA.ws.socket.readyState === WebSocket.CONNECTING)) {
    return;
  }

  var protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
  var host = window.location.hostname;
  var url = protocol + '//' + host + ':81/ws';
  AURA.ws.authed = false;

  try {
    AURA.ws.socket = new WebSocket(url);

    AURA.ws.socket.onopen = function() {
      AURA.ws.connected = true;
    };

    AURA.ws.socket.onmessage = function(event) {
      try {
        var data = JSON.parse(event.data);
        if (data.type === 'auth_required') {
          // Firmware asks for proof of session before sending any telemetry.
          AURA.ws.send({ type: 'auth', token: AURA.session.token });
        } else if (data.type === 'error') {
          if (!AURA.ws.authed) {
            // Server rejected the handshake/token — the session is not valid.
            AURA.ws.authed = true; // avoid recursion via handleExpired->close
            AURA.session.handleExpired();
          }
        } else {
          AURA.ws.dispatch(data);
        }
      } catch (e) {
        // ignore parse errors
      }
    };

    AURA.ws.socket.onclose = function() {
      AURA.ws.connected = false;
      AURA.ws.authed = false;
      AURA.ws.scheduleReconnect();
    };

    AURA.ws.socket.onerror = function() {
      AURA.ws.connected = false;
    };
  } catch (e) {
    AURA.ws.scheduleReconnect();
  }
};

AURA.ws.dispatch = function(data) {
  if (data.type === 'status' && AURA.onStatus) {
    AURA.onStatus(data);
  }
};

AURA.ws.scheduleReconnect = function() {
  if (!AURA.session || !AURA.session.authenticated) return;
  if (AURA.ws.reconnectTimer) {
    clearTimeout(AURA.ws.reconnectTimer);
  }
  AURA.ws.reconnectTimer = setTimeout(function() {
    AURA.ws.connect();
  }, 5000);
};

AURA.ws.send = function(data) {
  if (AURA.ws.socket && AURA.ws.socket.readyState === WebSocket.OPEN) {
    AURA.ws.socket.send(JSON.stringify(data));
  }
};
