AURA.ws = AURA.ws || {};

AURA.ws.connected = false;
AURA.ws.socket = null;
AURA.ws.reconnectTimer = null;

AURA.ws.connect = function() {
  if (AURA.ws.socket && AURA.ws.socket.readyState === WebSocket.OPEN) {
    return;
  }

  var protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
  var host = window.location.host;
  var url = protocol + '//' + host + '/ws';

  try {
    AURA.ws.socket = new WebSocket(url);

    AURA.ws.socket.onopen = function() {
      AURA.ws.connected = true;
    };

    AURA.ws.socket.onclose = function() {
      AURA.ws.connected = false;
      AURA.ws.scheduleReconnect();
    };

    AURA.ws.socket.onerror = function() {
      AURA.ws.connected = false;
    };

    AURA.ws.socket.onmessage = function(event) {
      try {
        var data = JSON.parse(event.data);
        AURA.ws.dispatch(data);
      } catch (e) {
        // ignore parse errors
      }
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
