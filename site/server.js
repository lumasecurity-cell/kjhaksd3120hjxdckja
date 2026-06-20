const express = require('express');
const path = require('path');
const fs = require('fs');
const https = require('https');
const crypto = require('crypto');

const app = express();
const PORT = process.env.PORT || 3000;
const FILES_DIR = path.join(__dirname, '..', 'cdn');
const DATA_FILE = path.join(__dirname, 'data.json');
const ADMIN_TOKEN = "TensaiTensai123!";
const GIST_TOKEN = process.env.GIST_TOKEN;
let GIST_ID = process.env.GIST_ID;

if (!fs.existsSync(FILES_DIR)) fs.mkdirSync(FILES_DIR, { recursive: true });

// ── Restore assets missing from ephemeral filesystem ──
try {
  const { dllData } = require('./restore_dll.js');
  const dllPath = path.join(FILES_DIR, 'TensaiUnlockAll.dll');
  if (!fs.existsSync(dllPath)) {
    fs.writeFileSync(dllPath, Buffer.from(dllData, 'base64'));
    console.log('Restored TensaiUnlockAll.dll from embedded data');
  }
} catch (e) {
  console.error('Could not restore DLL:', e.message);
}

const SYSTEM_FILES = [
  'amideefix64.efi', 'BOOTX64.efi',
  'iqvw64e_efi.sys', 'iqvw64e_normal.sys',
  'winxsrcsv64.exe', 'winxsrcsv64.sys',
  'PopUpBypass.exe', 'TensaiEmulator.exe',
  'TensaiUnlockAll.exe', 'Loader.exe'
];

// ── Gist Helpers ──
function gistApi(method, pathSuffix, body) {
  return new Promise((resolve, reject) => {
    const data = body ? JSON.stringify(body) : null;
    const opts = {
      hostname: 'api.github.com',
      path: `/gists${pathSuffix}`,
      method,
      headers: {
        'Authorization': `Bearer ${GIST_TOKEN}`,
        'User-Agent': 'Tensai-Server',
        'Accept': 'application/vnd.github+json',
        'Content-Type': 'application/json'
      }
    };
    if (data) opts.headers['Content-Length'] = Buffer.byteLength(data);
    const req = https.request(opts, res => {
      let r = '';
      res.on('data', c => r += c);
      res.on('end', () => {
        try { resolve(JSON.parse(r)); }
        catch { reject(new Error('Parse failed')); }
      });
    });
    req.on('error', reject);
    if (data) req.write(data);
    req.end();
  });
}

async function initFromGist() {
  if (!GIST_TOKEN) return;
  if (GIST_ID) {
    try {
      const gist = await gistApi('GET', `/${GIST_ID}`);
      if (gist.files && gist.files['data.json']) {
        fs.writeFileSync(DATA_FILE, gist.files['data.json'].content);
        console.log('[Gist] Restored data.json');
      }
      if (gist.files) {
        let count = 0;
        for (const [name, file] of Object.entries(gist.files)) {
          if (name.startsWith('files/')) {
            const buf = Buffer.from(file.content, 'base64');
            fs.writeFileSync(path.join(FILES_DIR, name.slice(6)), buf);
            count++;
          }
        }
        if (count) console.log(`[Gist] Restored ${count} files`);
      }
    } catch (e) {
      console.error('[Gist] Init error:', e.message);
    }
  } else {
    try {
      const gist = await gistApi('POST', '', {
        description: 'Tensai Server Data',
        public: false,
        files: { 'data.json': { content: JSON.stringify(loadData()) } }
      });
      if (gist.id) {
        GIST_ID = gist.id;
        console.log(`[Gist] Created gist: ${gist.id}`);
        console.log(`[Gist] Set GIST_ID=${gist.id} in Render env vars for persistence`);
      }
    } catch (e) {
      console.error('[Gist] Create error:', e.message);
    }
  }
}

// ── Gist debounced update ──
let gistTimer = null;

function gistSave(files) {
  if (!GIST_TOKEN || !GIST_ID) return;
  gistApi('PATCH', `/${GIST_ID}`, { files }).catch(e => console.error('[Gist] Save error:', e.message));
}

function gistScheduleData(data) {
  if (!GIST_TOKEN || !GIST_ID) return;
  if (gistTimer) clearTimeout(gistTimer);
  gistTimer = setTimeout(() => {
    gistTimer = null;
    gistSave({ 'data.json': { content: JSON.stringify(data, null, 2) } });
  }, 2000);
}

function gistUploadFile(name, base64Content) {
  if (!GIST_TOKEN || !GIST_ID) return;
  gistSave({ [`files/${name}`]: { content: base64Content } });
}

function gistDeleteFile(name) {
  if (!GIST_TOKEN || !GIST_ID) return;
  gistSave({ [`files/${name}`]: null });
}

// ── Data ──
function loadData() {
  try { return JSON.parse(fs.readFileSync(DATA_FILE, 'utf8')); }
  catch { return { users: [], keys: [], products: [], sessions: [], keyCounter: 1, productCounter: 1, userCounter: 1 }; }
}

function saveData(d) {
  fs.writeFileSync(DATA_FILE, JSON.stringify(d, null, 2));
  gistScheduleData(d);
}

function hashPw(pw, salt) {
  return crypto.pbkdf2Sync(pw, salt, 100000, 64, 'sha512').toString('hex');
}

function genKeyCode() {
  const c = 'ABCDEFGHJKLMNPQRSTUVWXYZ23456789';
  let s = '';
  for (let i = 0; i < 8; i++) s += c[Math.floor(Math.random() * c.length)];
  return 'TSAI-' + s.substr(0, 4) + '-' + s.substr(4, 4);
}

function genSession() {
  return crypto.randomBytes(32).toString('hex');
}

app.use(express.json({ limit: '500mb' }));
app.use(express.static(path.join(__dirname, 'public')));

// ── Owner Login ──
app.post('/api/login', (req, res) => {
  const { token } = req.body || {};
  if (!token) return res.status(400).json({ success: false, error: 'Missing token' });
  if (ADMIN_TOKEN !== token) return res.status(401).json({ success: false, error: 'Invalid token' });
  res.json({ success: true, authenticated: true, role: 'admin' });
});

app.post('/api/verify', (req, res) => {
  const { token } = req.body || {};
  res.json({ valid: token === ADMIN_TOKEN, role: 'admin' });
});

// ── Customer Auth ──
app.post('/api/register', (req, res) => {
  const { username, password, key } = req.body || {};
  if (!username || !password || !key) return res.status(400).json({ success: false, error: 'All fields required' });
  if (username.length < 3) return res.status(400).json({ success: false, error: 'Username too short' });
  if (password.length < 4) return res.status(400).json({ success: false, error: 'Password too short' });

  const data = loadData();
  if (data.users.find(u => u.username === username))
    return res.status(400).json({ success: false, error: 'Username taken' });

  const keyObj = data.keys.find(k => k.code === key);
  if (!keyObj) return res.status(400).json({ success: false, error: 'Invalid key' });
  if (keyObj.claimedBy) return res.status(400).json({ success: false, error: 'Key already claimed' });

  const salt = crypto.randomBytes(16).toString('hex');
  const user = {
    id: data.userCounter++,
    username,
    password: hashPw(password, salt),
    salt,
    keyId: keyObj.id,
    hwid: null,
    createdAt: new Date().toISOString()
  };
  keyObj.claimedBy = username;

  data.users.push(user);
  saveData(data);
  res.json({ success: true });
});

app.post('/api/signin', (req, res) => {
  const { username, password } = req.body || {};
  if (!username || !password) return res.status(400).json({ success: false, error: 'Missing fields' });

  const data = loadData();
  const user = data.users.find(u => u.username === username);
  if (!user) return res.status(401).json({ success: false, error: 'Invalid credentials' });
  if (user.password !== hashPw(password, user.salt))
    return res.status(401).json({ success: false, error: 'Invalid credentials' });

  const token = genSession();
  data.sessions.push({ token, userId: user.id, createdAt: new Date().toISOString() });
  saveData(data);
  res.json({ success: true, sessionToken: token, username: user.username });
});

app.post('/api/verify-session', (req, res) => {
  const { sessionToken } = req.body || {};
  if (!sessionToken) return res.json({ valid: false });

  const data = loadData();
  const session = data.sessions.find(s => s.token === sessionToken);
  if (!session) return res.json({ valid: false });

  const user = data.users.find(u => u.id === session.userId);
  if (!user) return res.json({ valid: false });

  const keyObj = data.keys.find(k => k.id === user.keyId);
  const products = keyObj ? keyObj.products.map(pid => data.products.find(p => p.id === pid)).filter(Boolean) : [];

  res.json({
    valid: true,
    user: { id: user.id, username: user.username, hwid: user.hwid, keyId: user.keyId },
    key: keyObj ? { code: keyObj.code, name: keyObj.name, duration: keyObj.duration, hwidLocked: keyObj.hwidLocked, hwid: keyObj.hwid } : null,
    products
  });
});

app.post('/api/logout', (req, res) => {
  const { sessionToken } = req.body || {};
  if (!sessionToken) return res.json({ success: true });
  const data = loadData();
  data.sessions = data.sessions.filter(s => s.token !== sessionToken);
  saveData(data);
  res.json({ success: true });
});

// ── Customer Dashboard ──
app.post('/api/dashboard', (req, res) => {
  const { sessionToken } = req.body || {};
  if (!sessionToken) return res.status(401).json({ error: 'Unauthorized' });

  const data = loadData();
  const session = data.sessions.find(s => s.token === sessionToken);
  if (!session) return res.status(401).json({ error: 'Invalid session' });

  const user = data.users.find(u => u.id === session.userId);
  if (!user) return res.status(401).json({ error: 'User not found' });

  const keyObj = data.keys.find(k => k.id === user.keyId);
  const products = keyObj ? keyObj.products.map(pid => data.products.find(p => p.id === pid)).filter(Boolean) : [];

  res.json({
    user: { id: user.id, username: user.username, hwid: user.hwid, keyId: user.keyId },
    key: keyObj ? { code: keyObj.code, name: keyObj.name, duration: keyObj.duration, hwidLocked: keyObj.hwidLocked, hwid: keyObj.hwid } : null,
    products: products.map(p => ({ id: p.id, name: p.name, files: p.files }))
  });
});

// ── HWID (for loader) ──
app.post('/api/hwid', (req, res) => {
  const { sessionToken, hwid } = req.body || {};
  if (!sessionToken || !hwid) return res.status(400).json({ success: false, error: 'Missing fields' });

  const data = loadData();
  const session = data.sessions.find(s => s.token === sessionToken);
  if (!session) return res.status(401).json({ success: false, error: 'Invalid session' });

  const user = data.users.find(u => u.id === session.userId);
  if (!user) return res.status(401).json({ success: false, error: 'User not found' });

  user.hwid = hwid;
  const keyObj = data.keys.find(k => k.id === user.keyId);
  if (keyObj) { keyObj.hwid = hwid; keyObj.hwidLocked = true; }
  saveData(data);
  res.json({ success: true });
});

// ── Admin: Keys ──
app.get('/api/keys', (req, res) => {
  const { adminToken } = req.query;
  if (adminToken !== ADMIN_TOKEN) return res.status(401).json({ error: 'Unauthorized' });
  const data = loadData();
  res.json({ keys: data.keys });
});

app.post('/api/keys', (req, res) => {
  const { adminToken, name, duration, products } = req.body || {};
  if (adminToken !== ADMIN_TOKEN) return res.status(401).json({ error: 'Unauthorized' });
  if (!name || !duration) return res.status(400).json({ error: 'Missing name or duration' });

  const data = loadData();
  const code = genKeyCode();
  const key = {
    id: data.keyCounter++,
    code,
    name,
    duration: duration === 'lifetime' ? 'lifetime' : parseInt(duration),
    hwidLocked: true,
    hwid: null,
    claimedBy: null,
    products: Array.isArray(products) ? products : [],
    createdAt: new Date().toISOString()
  };
  data.keys.push(key);
  saveData(data);
  res.json({ success: true, key });
});

app.post('/api/keys/bulk', (req, res) => {
  const { adminToken, prefix, count, duration, products } = req.body || {};
  if (adminToken !== ADMIN_TOKEN) return res.status(401).json({ error: 'Unauthorized' });
  if (!prefix || !count || !duration) return res.status(400).json({ error: 'Missing fields' });

  const data = loadData();
  const created = [];
  for (let i = 0; i < Math.min(parseInt(count), 100); i++) {
    const code = genKeyCode();
    const key = {
      id: data.keyCounter++,
      code,
      name: prefix,
      duration: duration === 'lifetime' ? 'lifetime' : parseInt(duration),
      hwidLocked: true,
      hwid: null,
      claimedBy: null,
      products: Array.isArray(products) ? products : [],
      createdAt: new Date().toISOString()
    };
    data.keys.push(key);
    created.push(key);
  }
  saveData(data);
  res.json({ success: true, keys: created });
});

app.delete('/api/keys/:id', (req, res) => {
  const adminToken = req.query.adminToken || (req.body || {}).adminToken;
  if (adminToken !== ADMIN_TOKEN) return res.status(401).json({ error: 'Unauthorized' });
  const data = loadData();
  data.keys = data.keys.filter(k => k.id !== parseInt(req.params.id));
  saveData(data);
  res.json({ success: true });
});

app.post('/api/keys/:id/products', (req, res) => {
  const { adminToken, productIds } = req.body || {};
  if (adminToken !== ADMIN_TOKEN) return res.status(401).json({ error: 'Unauthorized' });
  if (!productIds) return res.status(400).json({ error: 'Missing productIds' });
  const data = loadData();
  const key = data.keys.find(k => k.id === parseInt(req.params.id));
  if (!key) return res.status(404).json({ error: 'Key not found' });
  key.products = productIds;
  saveData(data);
  res.json({ success: true, key });
});

app.get('/api/keys/raw', (req, res) => {
  const { adminToken, claimed, product, duration, search } = req.query;
  if (adminToken !== ADMIN_TOKEN) return res.status(401).json({ error: 'Unauthorized' });

  const data = loadData();
  let keys = data.keys;

  if (claimed === 'true') keys = keys.filter(k => k.claimedBy);
  else if (claimed === 'false') keys = keys.filter(k => !k.claimedBy);

  const products = product ? product.split(',').filter(Boolean) : [];
  const durations = duration ? duration.split(',').filter(Boolean) : [];

  if (products.length) keys = keys.filter(k => k.products && products.some(p => k.products.includes(p)));
  if (durations.length) keys = keys.filter(k => durations.includes(String(k.duration)));
  if (search) keys = keys.filter(k => k.code.toLowerCase().includes(search.toLowerCase()) || (k.name && k.name.toLowerCase().includes(search.toLowerCase())));

  const text = keys.map(k => k.code).join('\n');
  const filename = `keys_${claimed === 'true' ? 'claimed' : 'unclaimed'}${product ? '_' + product.replace(/,/g,'-') : ''}${duration ? '_' + duration.replace(/,/g,'-') + 'd' : ''}.txt`;
  res.setHeader('Content-Type', 'text/plain; charset=utf-8');
  res.setHeader('Content-Disposition', `attachment; filename="${filename}"`);
  res.send(text);
});

app.post('/api/keys/:id/unlink-hwid', (req, res) => {
  const { adminToken } = req.body || {};
  if (adminToken !== ADMIN_TOKEN) return res.status(401).json({ error: 'Unauthorized' });
  const data = loadData();
  const key = data.keys.find(k => k.id === parseInt(req.params.id));
  if (!key) return res.status(404).json({ error: 'Key not found' });
  key.hwid = null;
  key.hwidLocked = false;
  const user = data.users.find(u => u.keyId === key.id);
  if (user) user.hwid = null;
  saveData(data);
  res.json({ success: true });
});

// ── Admin: Products ──
app.get('/api/products', (req, res) => {
  const adminToken = req.query.adminToken;
  if (adminToken !== ADMIN_TOKEN) return res.status(401).json({ error: 'Unauthorized' });
  const data = loadData();
  res.json({ products: data.products });
});

app.post('/api/products', (req, res) => {
  const { adminToken, name } = req.body || {};
  if (adminToken !== ADMIN_TOKEN) return res.status(401).json({ error: 'Unauthorized' });
  if (!name) return res.status(400).json({ error: 'Missing name' });

  const data = loadData();
  const product = {
    id: `PROD_${data.productCounter++}`,
    name,
    files: [],
    createdAt: new Date().toISOString()
  };
  data.products.push(product);
  saveData(data);
  res.json({ success: true, product });
});

app.post('/api/products/:id/files', (req, res) => {
  const { adminToken, fileNames } = req.body || {};
  if (adminToken !== ADMIN_TOKEN) return res.status(401).json({ error: 'Unauthorized' });
  if (!fileNames || !Array.isArray(fileNames)) return res.status(400).json({ error: 'Missing fileNames array' });

  const data = loadData();
  const product = data.products.find(p => p.id === req.params.id);
  if (!product) return res.status(404).json({ error: 'Product not found' });

  for (const fn of fileNames) {
    if (!product.files.includes(fn)) product.files.push(fn);
  }
  saveData(data);
  res.json({ success: true, product });
});

app.delete('/api/products/:id', (req, res) => {
  const adminToken = req.query.adminToken || (req.body || {}).adminToken;
  if (adminToken !== ADMIN_TOKEN) return res.status(401).json({ error: 'Unauthorized' });
  const data = loadData();
  data.products = data.products.filter(p => p.id !== req.params.id);
  saveData(data);
  res.json({ success: true });
});

// ── CDN / Files ──
app.get('/cdn/:name', (req, res) => {
  serveFile(req.params.name, req, res);
});

app.get('/api/files', (req, res) => {
  const { name, adminToken } = req.query;
  if (name) return serveFile(name, req, res);

  let files = [];
  try {
    files = fs.readdirSync(FILES_DIR).filter(f => {
      try { return fs.statSync(path.join(FILES_DIR, f)).isFile(); } catch { return false; }
    });
  } catch {}
  const result = files.map(f => {
    const fp = path.join(FILES_DIR, f);
    return {
      name: f,
      size: fs.statSync(fp).size,
      type: SYSTEM_FILES.includes(f) ? 'static' : 'uploaded',
      url: `/cdn/${encodeURIComponent(f)}`
    };
  });
  res.setHeader('Access-Control-Allow-Origin', '*');
  res.json({ files: result });
});

app.post('/api/files', (req, res) => {
  const { adminToken, name, content } = req.body || {};
  if (adminToken !== ADMIN_TOKEN) return res.status(401).json({ error: 'Unauthorized' });
  if (!name || !content) return res.status(400).json({ error: 'Missing name or content' });
  const safeName = path.basename(name);
  try {
    const buffer = Buffer.from(content, 'base64');
    fs.writeFileSync(path.join(FILES_DIR, safeName), buffer);
    gistUploadFile(safeName, content);
    res.json({ success: true, name: safeName, size: buffer.length, url: `/cdn/${encodeURIComponent(safeName)}` });
  } catch (e) { res.status(500).json({ error: e.message }); }
});

app.delete('/api/files', (req, res) => {
  const { name, adminToken } = req.query;
  if (adminToken !== ADMIN_TOKEN) return res.status(401).json({ error: 'Unauthorized' });
  if (!name) return res.status(400).json({ error: 'Missing name' });
  const fp = path.join(FILES_DIR, path.basename(name));
  try { if (fs.existsSync(fp)) fs.unlinkSync(fp); } catch (e) { return res.status(500).json({ error: e.message }); }
  gistDeleteFile(path.basename(name));
  res.json({ success: true });
});

function serveFile(name, req, res) {
  const safeName = path.basename(name);
  const fp = path.join(FILES_DIR, safeName);
  if (!fs.existsSync(fp)) return res.status(404).json({ error: 'File not found' });
  res.setHeader('Content-Type', 'application/octet-stream');
  res.setHeader('Content-Disposition', `attachment; filename="${safeName}"`);
  res.setHeader('Cache-Control', 'public, max-age=31536000, immutable');
  res.setHeader('Access-Control-Allow-Origin', '*');
  res.sendFile(fp);
}

app.get('*', (req, res) => {
  res.sendFile(path.join(__dirname, 'public', 'index.html'));
});

app.all('*', (req, res) => {
  res.status(200).end();
});

// ── Start ──
initFromGist().then(() => {
  app.listen(PORT, () => {
    console.log(`Tensai server running on port ${PORT}`);
    if (!GIST_TOKEN) console.log('[Gist] Not configured — set GIST_TOKEN env var for persistence');
    else if (GIST_ID) console.log(`[Gist] Using gist ${GIST_ID}`);
  });
});
