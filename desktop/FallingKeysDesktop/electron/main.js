const { app, BrowserWindow, ipcMain } = require('electron');
const { spawn } = require('child_process');
const path = require('path');

let mainWindow = null;
let bridgeProcess = null;

function send(channel, payload) {
  if (mainWindow && !mainWindow.isDestroyed()) {
    mainWindow.webContents.send(channel, payload);
  }
}

function bridgePath() {
  return path.resolve(__dirname, '..', 'bridge', 'serial_bridge.py');
}

function pythonCommand() {
  return process.env.SCI_CALC_PYTHON || 'python';
}

function stopBridge() {
  if (!bridgeProcess) return;
  bridgeProcess.kill();
  bridgeProcess = null;
}

function readJsonLines(stream, onRecord) {
  let buffer = '';
  stream.setEncoding('utf8');
  stream.on('data', (chunk) => {
    buffer += chunk;
    const lines = buffer.split(/\r?\n/);
    buffer = lines.pop() || '';
    for (const line of lines) {
      if (!line.trim()) continue;
      try {
        onRecord(JSON.parse(line));
      } catch (error) {
        send('serial:status', { kind: 'bridge_parse_error', message: error.message });
      }
    }
  });
  return () => {
    if (buffer.trim()) {
      try { onRecord(JSON.parse(buffer)); } catch (_) { /* process is ending */ }
    }
  };
}

function createWindow() {
  mainWindow = new BrowserWindow({
    width: 1440,
    height: 900,
    minWidth: 860,
    minHeight: 580,
    useContentSize: true,
    backgroundColor: '#101417',
    autoHideMenuBar: true,
    titleBarStyle: 'hidden',
    titleBarOverlay: {
      color: '#12181a',
      symbolColor: '#91a1a8',
      height: 36,
    },
    webPreferences: {
      preload: path.join(__dirname, 'preload.js'),
      contextIsolation: true,
      nodeIntegration: false,
      sandbox: false,
    },
  });

  mainWindow.loadFile(path.resolve(__dirname, '..', 'renderer', 'index.html'));
  mainWindow.once('ready-to-show', () => mainWindow.show());
  mainWindow.setMenuBarVisibility(false);
  mainWindow.on('closed', () => { mainWindow = null; });
}

ipcMain.handle('serial:list', async () => new Promise((resolve) => {
  const child = spawn(pythonCommand(), [bridgePath(), '--list-ports'], {
    cwd: path.resolve(__dirname, '..', '..', '..'),
    windowsHide: true,
    stdio: ['ignore', 'pipe', 'pipe'],
  });
  const ports = [];
  let stderr = '';
  readJsonLines(child.stdout, (record) => {
    if (record.type === 'port') ports.push(record);
  });
  child.stderr.setEncoding('utf8');
  child.stderr.on('data', (chunk) => { stderr += chunk; });
  child.on('error', (error) => resolve({ ok: false, error: error.message, ports: [] }));
  child.on('close', (code) => resolve({ ok: code === 0, error: code === 0 ? '' : stderr.trim(), ports }));
}));

ipcMain.handle('serial:connect', async (_event, rawPort) => {
  const port = String(rawPort || '').trim();
  if (!/^(COM\d+|\\\\\.\\COM\d+|\/dev\/[^\s]+)$/i.test(port)) {
    return { ok: false, error: 'Invalid serial port' };
  }

  stopBridge();
  const child = spawn(pythonCommand(), [bridgePath(), '--port', port], {
    cwd: path.resolve(__dirname, '..', '..', '..'),
    windowsHide: true,
    stdio: ['ignore', 'pipe', 'pipe'],
  });
  bridgeProcess = child;
  send('serial:status', { kind: 'connecting', port });
  readJsonLines(child.stdout, (record) => {
    if (bridgeProcess !== child) return;
    if (record.type === 'error') {
      send('serial:status', { kind: 'error', message: record.message || 'Serial bridge error' });
    } else {
      send('serial:data', record);
    }
  });
  child.stderr.setEncoding('utf8');
  child.stderr.on('data', (chunk) => {
    if (bridgeProcess !== child) return;
    const message = String(chunk).trim();
    if (message) send('serial:status', { kind: 'bridge_log', message });
  });
  child.on('error', (error) => {
    if (bridgeProcess !== child) return;
    send('serial:status', { kind: 'error', message: error.message });
    bridgeProcess = null;
  });
  child.on('close', (code) => {
    if (bridgeProcess !== child) return;
    send('serial:status', { kind: 'disconnected', code });
    bridgeProcess = null;
  });
  return { ok: true, port };
});

ipcMain.handle('serial:disconnect', () => {
  stopBridge();
  send('serial:status', { kind: 'disconnected', code: 0 });
  return { ok: true };
});

app.whenReady().then(() => {
  createWindow();
  app.on('activate', () => { if (BrowserWindow.getAllWindows().length === 0) createWindow(); });
});

app.on('window-all-closed', () => {
  stopBridge();
  if (process.platform !== 'darwin') app.quit();
});

app.on('before-quit', stopBridge);
