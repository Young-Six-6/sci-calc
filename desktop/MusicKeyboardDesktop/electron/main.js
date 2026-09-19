const { app, BrowserWindow, dialog, ipcMain, screen } = require('electron');
const { spawn } = require('child_process');
const fs = require('fs/promises');
const path = require('path');

let mainWindow = null;
let bridgeProcess = null;

function send(channel, payload) {
  if (mainWindow && !mainWindow.isDestroyed()) {
    mainWindow.webContents.send(channel, payload);
  }
}

function repoRoot() {
  return path.resolve(__dirname, '..', '..', '..');
}

function bridgePath() {
  return path.resolve(__dirname, '..', 'bridge', 'serial_bridge.py');
}

function converterPath() {
  return path.resolve(repoRoot(), 'games', 'MusicKeyboard', 'tools', 'midi_to_calc.py');
}

function lessonBuilderPath() {
  return path.resolve(__dirname, '..', 'bridge', 'midi_lesson.py');
}

function pythonCommand() {
  return process.env.SCI_CALC_PYTHON || 'python';
}

function stopBridge() {
  if (!bridgeProcess) return;
  bridgeProcess.kill();
  bridgeProcess = null;
}

function fitWindowToWorkArea() {
  if (!mainWindow || mainWindow.isDestroyed() || mainWindow.isMaximized()) return;
  const display = screen.getDisplayMatching(mainWindow.getBounds());
  const area = display.workArea;
  const margin = 24;
  const bounds = mainWindow.getBounds();
  const width = Math.min(bounds.width, Math.max(1, area.width - margin * 2));
  const height = Math.min(bounds.height, Math.max(1, area.height - margin * 2));
  const x = Math.max(area.x + margin, Math.min(bounds.x, area.x + area.width - width - margin));
  const y = Math.max(area.y + margin, Math.min(bounds.y, area.y + area.height - height - margin));
  if (width !== bounds.width || height !== bounds.height || x !== bounds.x || y !== bounds.y) {
    mainWindow.setBounds({ x, y, width, height }, false);
  }
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
}

function createWindow() {
  const display = screen.getPrimaryDisplay();
  const area = display.workArea;
  const margin = 24;
  const availableWidth = Math.max(1, area.width - margin * 2);
  const availableHeight = Math.max(1, area.height - margin * 2);
  const minWidth = Math.min(720, availableWidth);
  const minHeight = Math.min(460, availableHeight);
  mainWindow = new BrowserWindow({
    width: Math.min(1380, availableWidth),
    height: Math.min(900, availableHeight),
    minWidth,
    minHeight,
    center: true,
    useContentSize: true,
    backgroundColor: '#0d1115',
    autoHideMenuBar: true,
    titleBarStyle: 'hidden',
    titleBarOverlay: {
      color: '#11171b',
      symbolColor: '#9aa9ae',
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
  mainWindow.once('ready-to-show', () => {
    fitWindowToWorkArea();
    mainWindow.show();
  });
  mainWindow.setMenuBarVisibility(false);
  mainWindow.on('closed', () => { mainWindow = null; });
}

ipcMain.handle('serial:list', async () => new Promise((resolve) => {
  const child = spawn(pythonCommand(), [bridgePath(), '--list-ports'], {
    cwd: repoRoot(),
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
  child.on('close', (code) => resolve({
    ok: code === 0,
    error: code === 0 ? '' : stderr.trim() || 'Unable to enumerate serial ports',
    ports,
  }));
}));

ipcMain.handle('serial:connect', async (_event, rawPort) => {
  const port = String(rawPort || '').trim();
  if (!/^(COM\d+|\\\\\.\\COM\d+|\/dev\/[^\s]+)$/i.test(port)) {
    return { ok: false, error: 'Invalid serial port' };
  }

  stopBridge();
  const child = spawn(pythonCommand(), [bridgePath(), '--port', port], {
    cwd: repoRoot(),
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

ipcMain.handle('midi:choose', async () => {
  const result = await dialog.showOpenDialog(mainWindow, {
    title: 'Choose MIDI file',
    properties: ['openFile'],
    filters: [{ name: 'MIDI files', extensions: ['mid', 'midi'] }, { name: 'All files', extensions: ['*'] }],
  });
  if (result.canceled || !result.filePaths[0]) return { ok: true, canceled: true };
  return { ok: true, canceled: false, path: result.filePaths[0], name: path.basename(result.filePaths[0]) };
});

ipcMain.handle('midi:convert', async (_event, payload) => {
  const inputPath = String(payload?.path || '').trim();
  if (!inputPath || !path.isAbsolute(inputPath)) return { ok: false, error: 'A MIDI file is required', output: '' };
  try {
    const stat = await fs.stat(inputPath);
    if (!stat.isFile()) return { ok: false, error: 'The selected path is not a file', output: '' };
  } catch (error) {
    return { ok: false, error: `Cannot read MIDI file: ${error.message}`, output: '' };
  }

  const args = [converterPath(), inputPath, '--grid', String(payload?.grid === 16 ? 16 : 8)];
  const track = String(payload?.track ?? '').trim();
  if (/^\d+$/.test(track)) args.push('--track', track);
  if (payload?.fullTrack) args.push('--full-track');
  if (payload?.chords) args.push('--chords');
  const label = String(payload?.rpnKeyLabel || 'RPN').trim();
  if (label) args.push('--rpn-key-label', label);

  return new Promise((resolve) => {
    const child = spawn(pythonCommand(), args, {
      cwd: repoRoot(),
      windowsHide: true,
      stdio: ['ignore', 'pipe', 'pipe'],
    });
    let output = '';
    let errorOutput = '';
    const maxOutput = 4 * 1024 * 1024;
    child.stdout.setEncoding('utf8');
    child.stderr.setEncoding('utf8');
    child.stdout.on('data', (chunk) => {
      if (output.length < maxOutput) output += String(chunk).slice(0, maxOutput - output.length);
    });
    child.stderr.on('data', (chunk) => { errorOutput += String(chunk); });
    child.on('error', (error) => resolve({ ok: false, error: error.message, output }));
    child.on('close', (code) => resolve({
      ok: code === 0,
      error: code === 0 ? '' : errorOutput.trim() || `Converter exited with code ${code}`,
      output,
    }));
  });
});

ipcMain.handle('midi:lesson', async (_event, payload) => {
  const inputPath = String(payload?.path || '').trim();
  if (!inputPath || !path.isAbsolute(inputPath)) return { ok: false, error: 'A MIDI file is required' };
  try {
    const stat = await fs.stat(inputPath);
    if (!stat.isFile()) return { ok: false, error: 'The selected path is not a file' };
  } catch (error) {
    return { ok: false, error: `Cannot read MIDI file: ${error.message}` };
  }

  const complexity = ['melody', 'simple', 'full'].includes(payload?.complexity) ? payload.complexity : 'melody';
  const args = [lessonBuilderPath(), inputPath, '--complexity', complexity, '--grid', '16'];
  return new Promise((resolve) => {
    const child = spawn(pythonCommand(), args, {
      cwd: repoRoot(),
      windowsHide: true,
      stdio: ['ignore', 'pipe', 'pipe'],
    });
    let output = '';
    let errorOutput = '';
    const maxOutput = 16 * 1024 * 1024;
    child.stdout.setEncoding('utf8');
    child.stderr.setEncoding('utf8');
    child.stdout.on('data', (chunk) => {
      if (output.length < maxOutput) output += String(chunk).slice(0, maxOutput - output.length);
    });
    child.stderr.on('data', (chunk) => { errorOutput += String(chunk); });
    child.on('error', (error) => resolve({ ok: false, error: error.message }));
    child.on('close', (code) => {
      if (code !== 0) return resolve({ ok: false, error: errorOutput.trim() || `Lesson builder exited with code ${code}` });
      try {
        return resolve({ ok: true, lesson: JSON.parse(output) });
      } catch (error) {
        return resolve({ ok: false, error: `Invalid lesson data: ${error.message}` });
      }
    });
  });
});

ipcMain.handle('file:save-midi', async (_event, payload) => {
  const bytes = Array.isArray(payload?.bytes) ? payload.bytes : [];
  if (!bytes.length) return { ok: false, error: 'MIDI data is empty' };

  const recordingsDir = path.join(app.getPath('documents'), 'recordings');
  await fs.mkdir(recordingsDir, { recursive: true });
  const requestedName = String(payload?.fileName || 'KEYS000.MID')
    .replace(/[<>:"/\\|?*\x00-\x1f]/g, '_')
    .slice(0, 80) || 'KEYS000.MID';
  const result = await dialog.showSaveDialog(mainWindow, {
    title: 'Save recorded MIDI',
    defaultPath: path.join(recordingsDir, requestedName),
    filters: [{ name: 'MIDI files', extensions: ['mid', 'midi'] }],
  });
  if (result.canceled || !result.filePath) return { ok: true, canceled: true };

  await fs.writeFile(result.filePath, Buffer.from(bytes));
  return { ok: true, canceled: false, path: result.filePath, fileName: path.basename(result.filePath) };
});

app.whenReady().then(() => {
  createWindow();
  screen.on('display-metrics-changed', fitWindowToWorkArea);
  app.on('activate', () => {
    if (BrowserWindow.getAllWindows().length === 0) createWindow();
  });
});

app.on('window-all-closed', () => {
  stopBridge();
  if (process.platform !== 'darwin') app.quit();
});

app.on('before-quit', stopBridge);
