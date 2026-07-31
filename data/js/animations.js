AURA.anim = AURA.anim || {};

AURA.anim.init = function() {
  var canvas = AURA.$('hud');
  if (!canvas) return;

  var ctx = canvas.getContext('2d');
  var W, H, particles = [], grid = [], circles = [], pulses = [];
  var frame = 0;
  var running = true;

  function resize() {
    W = canvas.width = window.innerWidth;
    H = canvas.height = window.innerHeight;

    var count = Math.min(80, Math.floor(W * H / 12000));

    while (particles.length < count) {
      particles.push({
        x: Math.random() * W,
        y: Math.random() * H,
        vx: (Math.random() - 0.5) * 0.4,
        vy: (Math.random() - 0.5) * 0.4,
        r: 1 + Math.random() * 1.5,
        o: 0.2 + Math.random() * 0.4
      });
    }
    if (particles.length > count) particles.length = count;

    grid = [];
    for (var i = 0; i < 6; i++) {
      grid.push(Math.random() * W);
    }

    circles = [];
    for (var j = 0; j < 4; j++) {
      circles.push({
        x: Math.random() * W,
        y: Math.random() * H,
        vx: (Math.random() - 0.5) * 0.15,
        vy: (Math.random() - 0.5) * 0.15,
        R: 60 + Math.random() * 80
      });
    }
  }

  window.addEventListener('resize', resize);
  resize();

  setInterval(function() {
    pulses.push({
      x: Math.random() * W,
      y: Math.random() * H,
      r: 0,
      o: 0.5
    });
  }, 3000);

  function draw() {
    if (!running) return;
    frame++;
    requestAnimationFrame(draw);
    ctx.clearRect(0, 0, W, H);

    // Update particles
    for (var i = 0; i < particles.length; i++) {
      var p = particles[i];
      p.x += p.vx;
      p.y += p.vy;
      if (p.x < 0) p.x = W;
      if (p.x > W) p.x = 0;
      if (p.y < 0) p.y = H;
      if (p.y > H) p.y = 0;
    }

    // Connections
    for (var a = 0; a < particles.length; a++) {
      for (var b = a + 1; b < particles.length; b++) {
        var dx = particles[a].x - particles[b].x;
        var dy = particles[a].y - particles[b].y;
        var ds = dx * dx + dy * dy;
        if (ds < 22500) {
          var opacity = (1 - ds / 22500) * 0.35 * Math.min(particles[a].o, particles[b].o);
          if (opacity > 0.01) {
            ctx.beginPath();
            ctx.moveTo(particles[a].x, particles[a].y);
            ctx.lineTo(particles[b].x, particles[b].y);
            ctx.strokeStyle = 'rgba(0,217,255,' + opacity + ')';
            ctx.lineWidth = 0.6;
            ctx.stroke();
          }
        }
      }
    }

    // Draw particles
    for (var c = 0; c < particles.length; c++) {
      ctx.beginPath();
      ctx.arc(particles[c].x, particles[c].y, particles[c].r, 0, Math.PI * 2);
      var po = particles[c].o + (Math.sin(frame * 0.01 + c) * 0.1);
      ctx.fillStyle = 'rgba(0,217,255,' + Math.max(0.1, Math.min(0.9, po)) + ')';
      ctx.fill();
    }

    // Grid lines
    ctx.strokeStyle = 'rgba(0,217,255,0.025)';
    ctx.lineWidth = 0.5;
    for (var d = 0; d < grid.length; d++) {
      var yOff = (frame * 0.08 + d * 30) % (H * 1.2);
      ctx.beginPath();
      ctx.moveTo(0, yOff);
      ctx.lineTo(W, yOff - H * 0.1);
      ctx.stroke();
    }

    // Radar pulses
    for (var e = pulses.length - 1; e >= 0; e--) {
      var pl = pulses[e];
      pl.r += 2.5;
      pl.o -= 0.008;
      if (pl.o <= 0 || pl.r > Math.max(W, H)) {
        pulses.splice(e, 1);
        continue;
      }
      ctx.beginPath();
      ctx.arc(pl.x, pl.y, pl.r, 0, Math.PI * 2);
      ctx.strokeStyle = 'rgba(0,217,255,' + Math.max(0, pl.o) + ')';
      ctx.lineWidth = 1.5;
      ctx.stroke();
    }

    // Floating circles
    for (var f = 0; f < circles.length; f++) {
      var cir = circles[f];
      cir.x += cir.vx;
      cir.y += cir.vy;
      if (cir.x < -200) cir.x = W + 200;
      if (cir.x > W + 200) cir.x = -200;
      if (cir.y < -200) cir.y = H + 200;
      if (cir.y > H + 200) cir.y = -200;
      var pulse = Math.sin(frame * 0.005 + f) * 0.2 + 0.8;
      ctx.beginPath();
      ctx.arc(cir.x, cir.y, cir.R * pulse, 0, Math.PI * 2);
      ctx.fillStyle = 'rgba(0,217,255,0.015)';
      ctx.fill();
      ctx.strokeStyle = 'rgba(0,217,255,0.04)';
      ctx.lineWidth = 0.5;
      ctx.stroke();
    }
  }

  draw();

  AURA.anim.stop = function() {
    running = false;
  };
};
