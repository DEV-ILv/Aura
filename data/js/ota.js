AURA.pages = AURA.pages || {};
AURA.ota = AURA.ota || {};
AURA.ota.file = null;

AURA.ota.handleFile = function(f) {
  if (!f.name.endsWith('.bin')) {
    AURA.components.toast('Please select a .bin firmware file', 'error');
    return;
  }
  AURA.ota.file = f;
  AURA.$('file-name').textContent = f.name;
  AURA.$('file-size').textContent = (f.size / 1024).toFixed(1) + ' KB';
  AURA.$('file-info').classList.add('show');
  AURA.$('ota-btn').disabled = false;
};

AURA.ota.clearFile = function() {
  AURA.ota.file = null;
  AURA.$('file-info').classList.remove('show');
  AURA.$('file-input').value = '';
  AURA.$('ota-btn').disabled = true;
};

AURA.ota.start = function() {
  if (!AURA.ota.file) return;

  var btn = AURA.$('ota-btn');
  var prg = AURA.$('prg-wrap');
  var fill = AURA.$('prg-fill');
  var txt = AURA.$('prg-txt');
  var st = AURA.$('ota-status');

  btn.disabled = true;
  prg.classList.add('show');
  fill.style.width = '0%';
  txt.textContent = '0%';
  st.textContent = 'Uploading...';
  st.style.color = 'var(--text-secondary)';

  var fd = new FormData();
  fd.append('firmware', AURA.ota.file);

  var xhr = new XMLHttpRequest();

  xhr.upload.addEventListener('progress', function(e) {
    if (e.lengthComputable) {
      var p = Math.round(e.loaded / e.total * 100);
      fill.style.width = p + '%';
      txt.textContent = p + '%';
    }
  });

  xhr.addEventListener('load', function() {
    try {
      var r = JSON.parse(xhr.responseText);
      if (r.success) {
        st.textContent = r.message || 'Update successful! Restarting...';
        st.style.color = 'var(--success)';
        AURA.components.toast('Firmware updated successfully', 'success');
      } else {
        st.textContent = r.error || 'Update failed';
        st.style.color = 'var(--danger)';
        btn.disabled = false;
        AURA.components.toast(r.error || 'Update failed', 'error');
      }
    } catch (e) {
      st.textContent = 'Update complete. Restarting...';
      st.style.color = 'var(--success)';
    }
  });

  xhr.addEventListener('error', function() {
    st.textContent = 'Upload failed';
    st.style.color = 'var(--danger)';
    btn.disabled = false;
    AURA.components.toast('Upload failed', 'error');
  });

  xhr.open('POST', '/ota');
  xhr.send(fd);
};

AURA.pages.ota = function() {
  var se = AURA.state.settings;
  AURA.components.render(
    '<div class="page" id="page-ota">' +
    '<div class="page-h1">OTA Update</div>' +
    '<div class="page-sub">Firmware update via file upload</div>' +
    '<div class="crd">' +
    '<div class="crd-h"><div class="crd-lbl">Current Firmware</div><span style="font-family:var(--font-mono);font-size:14px;color:var(--text-secondary)" id="ota-ver">' + (se ? 'v' + se.version : '--') + '</span></div>' +
    '<div style="font-size:12px;color:var(--text-muted);margin-bottom:20px">Select a .bin firmware file to upload and update the device firmware.</div>' +
    '<div class="file-zone" id="file-zone" onclick="document.getElementById(\'file-input\').click()">' +
    '<span class="material-symbols-outlined">cloud_upload</span>' +
    '<p><strong>Click to select</strong> or drag a .bin file here</p>' +
    '</div>' +
    '<input type="file" id="file-input" accept=".bin" style="display:none">' +
    '<div class="file-info" id="file-info"><span class="material-symbols-outlined" style="color:var(--accent);font-size:20px">description</span><span class="name" id="file-name"></span><span class="size" id="file-size"></span><button class="btn btn-ghost btn-sm" onclick="AURA.ota.clearFile()">Remove</button></div>' +
    '<div class="prg-wrap" id="prg-wrap"><div class="prg-bar"><div class="prg-fill" id="prg-fill"></div></div><div class="prg-txt" id="prg-txt">0%</div></div>' +
    '<div class="ota-status" id="ota-status"></div>' +
    '<div class="btn-group"><button class="btn btn-primary" id="ota-btn" onclick="AURA.ota.start()" disabled>Upload Firmware</button></div>' +
    '</div></div>'
  );
  AURA.components.showPage('page-ota');

  AURA.$('file-input').addEventListener('change', function(e) {
    var f = e.target.files[0];
    if (f) AURA.ota.handleFile(f);
  });

  var zone = AURA.$('file-zone');
  if (zone) {
    zone.addEventListener('dragover', function(e) {
      e.preventDefault();
      zone.classList.add('drag');
    });
    zone.addEventListener('dragleave', function() {
      zone.classList.remove('drag');
    });
    zone.addEventListener('drop', function(e) {
      e.preventDefault();
      zone.classList.remove('drag');
      var f = e.dataTransfer.files[0];
      if (f) AURA.ota.handleFile(f);
    });
  }
};
