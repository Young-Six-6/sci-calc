(function () {
  'use strict';

  const { parsePTC, mapLanes } = window.FallingKeysChart;
  const { parseMIDI } = window.FallingKeysMidi;
  const { laneForKey, maskHas } = window.FallingKeysInput;
  const $ = (id) => document.getElementById(id);
  const MODES = {
    flow: { key: 'modeFlow', hit: 180, perfect: 60, approach: 2350, grace: 160, speed: 1, autoGain: .30, mirror: false },
    arcade: { key: 'modeArcade', hit: 140, perfect: 45, approach: 1900, grace: 120, speed: 1, autoGain: .40, mirror: false },
    rush: { key: 'modeRush', hit: 105, perfect: 32, approach: 1450, grace: 90, speed: 1.13, autoGain: .48, mirror: false },
    mirror: { key: 'modeMirror', hit: 135, perfect: 42, approach: 1850, grace: 115, speed: 1, autoGain: .42, mirror: true },
  };
  const I18N = {
    en: {
      brandSub: 'Calculator rhythm lab', language: 'Language', controllerOffline: 'Controller offline', desktopMode: 'DESKTOP RHYTHM MODE',
      heroA: 'Play the chart.', heroB: 'Feel every beat.', heroCopy: 'Your calculator is the controller. The computer handles the stage, sound, timing, and score.', readyRoom: 'READY ROOM', setupRun: 'Set up a run', serialController: 'Serial controller', serialHelp: 'USB Key Share at 115200 baud', chart: 'Chart', loadSong: 'Load song', useDemo: 'Use demo', laneCount: 'Lane count', mode: 'Mode', startRun: 'Start run', connectFirst: 'Connect the calculator to begin.', liveJudgment: 'Live judgment', judgmentSub: 'Perfect, good, miss', generatedSound: 'Generated sound', soundSub: 'MIDI accompaniment plays automatically', localBests: 'Local bests', bestSub: 'Saved on this computer', scan: 'Scan', connect: 'Connect', pause: 'Pause', resume: 'Resume', runPaused: 'RUN PAUSED', takeBreath: 'Take a breath', resumeHint: 'Press Esc or the button to resume', waitingController: 'Waiting for controller', runComplete: 'RUN COMPLETE', accuracy: 'Accuracy', maxCombo: 'Max combo', perfect: 'Perfect', misses: 'Misses', setup: 'Setup', retry: 'Retry', score: 'SCORE', combo: 'COMBO',
      modeFlow: 'Flow', modeArcade: 'Arcade', modeRush: 'Rush', modeMirror: 'Mirror', disconnect: 'Disconnect', best: 'BEST', watchDemo: 'Watch demo', autoPilot: 'AUTO PILOT', builtIn: 'Built-in warm-up chart', imported: 'Imported chart', chartSummary: '{notes} notes · {lanes} lanes · {bpm} BPM · {duration}',
      controllerReady: 'Controller ready. Choose a chart and start.', serialFound: '{count} serial port(s) found.', noPorts: 'No serial ports found.', loaded: 'Loaded {name}.', demoReady: 'Demo chart ready.', connectFirstShort: 'Connect the calculator first.', enterPort: 'Enter a serial port such as COM7.', invalidPort: 'Invalid serial port.', disconnected: 'Controller disconnected.', getReady: 'Get ready', paused: 'Paused', controllerInput: 'calculator input', controllerDisconnected: 'Controller disconnected', brilliant: 'Brilliant run.', nice: 'Nice run.', keepBeat: 'Keep the beat.', offbeat: 'OFFBEAT', holdMiss: 'HOLD MISS', holdPlus: 'HOLD+', perfectWord: 'PERFECT', goodWord: 'GOOD', missWord: 'MISS',
      timingCalibration: 'TIMING CALIBRATION', calibrationTitle: 'Calibrate controller latency', calibrationLead: 'Listen for the click, then press any lane key once on every beat. The result corrects the game judgment clock.', calibrationTempo: 'Calibration tempo', calibrationBeats: 'Measured beats', calibrationStart: 'Start calibration', calibrationStop: 'Stop calibration', calibrationReset: 'Reset', calibrationReady: 'Ready. Connect the calculator, then start calibration.', calibrationConnect: 'Connect the calculator before calibrating.', calibrationAudioError: 'Audio could not start; calibration was not started.', calibrationCountIn: 'Count-in: get ready...', calibrationMeasuring: 'Tap one lane key on every beat.', calibrationDone: 'Calibration complete', calibrationTooFew: 'Not enough valid taps; no setting was changed.', calibrationSamples: 'samples', calibrationSpread: 'spread', calibrationNote: 'A positive value means the serial event arrived late, so game judgment moves the input earlier by that amount. Live sound cannot start before the key event arrives.',
    },
    zh: {
      brandSub: '计算器节奏实验室', language: '语言', controllerOffline: '控制器未连接', desktopMode: '桌面音游模式',
      heroA: '跟着谱面落键。', heroB: '感受每一拍。', heroCopy: '计算器负责输入，电脑负责舞台、音效、时序和计分。', readyRoom: '准备区', setupRun: '设置一局游戏', serialController: '串口控制器', serialHelp: 'USB 按键共享，115200 波特率', chart: '谱面', loadSong: '加载音乐', useDemo: '使用演示', laneCount: '轨道数量', mode: '模式', startRun: '开始游戏', connectFirst: '请先连接计算器。', liveJudgment: '实时判定', judgmentSub: '完美、良好、漏击', generatedSound: '自动伴奏', soundSub: 'MIDI 其他声部由电脑自动播放', localBests: '本地最高分', bestSub: '保存在这台电脑上', scan: '扫描', connect: '连接', pause: '暂停', resume: '继续', runPaused: '游戏暂停', takeBreath: '休息一下', resumeHint: '按 Esc 或按钮继续', waitingController: '等待控制器', runComplete: '本局结束', accuracy: '准确率', maxCombo: '最高连击', perfect: '完美', misses: '漏击', setup: '设置', retry: '重试', score: '得分', combo: '连击',
      modeFlow: '流畅', modeArcade: '街机', modeRush: '疾速', modeMirror: '镜像', disconnect: '断开', best: '最高分', watchDemo: '观看演示', autoPilot: '自动演示', builtIn: '内置热身谱面', imported: '导入谱面', chartSummary: '{notes} 个音符 · {lanes} 轨 · {bpm} BPM · {duration}',
      controllerReady: '控制器已就绪，请选择谱面并开始。', serialFound: '找到 {count} 个串口。', noPorts: '没有找到串口。', loaded: '已加载 {name}。', demoReady: '演示谱面已准备好。', connectFirstShort: '请先连接计算器。', enterPort: '请输入串口，例如 COM7。', invalidPort: '串口格式无效。', disconnected: '控制器已断开。', getReady: '准备', paused: '已暂停', controllerInput: '计算器输入', controllerDisconnected: '控制器已断开', brilliant: '精彩演奏。', nice: '不错的演奏。', keepBeat: '继续保持节奏。', offbeat: '偏拍', holdMiss: '长按漏击', holdPlus: '长按完成', perfectWord: '完美', goodWord: '良好', missWord: '漏击',
      timingCalibration: '时序校准', calibrationTitle: '校准控制器延迟', calibrationLead: '听到节拍后，每拍按一次任意轨道按键。结果会修正音游的判定时钟。', calibrationTempo: '校准速度', calibrationBeats: '测量节拍数', calibrationStart: '开始校准', calibrationStop: '停止校准', calibrationReset: '清零', calibrationReady: '准备就绪。连接计算器后开始校准。', calibrationConnect: '请先连接计算器再校准。', calibrationAudioError: '音频无法启动，未开始校准。', calibrationCountIn: '预备拍：请准备……', calibrationMeasuring: '每拍按一次轨道按键。', calibrationDone: '校准完成', calibrationTooFew: '有效按键太少，未修改设置。', calibrationSamples: '个样本', calibrationSpread: '离散', calibrationNote: '正数表示串口事件到达较晚，因此游戏判定会将输入时间提前相应毫秒。实时声音无法在按键事件到达前播放。',
    },
  };
  const COLORS = ['#ff7565', '#54d8dc', '#8de7b2', '#f5c96b', '#c99cff', '#ff9c68'];
  const KEY_LABELS = ['ESC', '/', '*', '-', 'SIN', 'SEC', '7', '8', '9', '+', 'COS', 'CSC', '4', '5', '6', '^', 'TAN', 'COT', '1', '2', '3', 'ENTER', 'EXP', 'RPN', 'SHIFT', '0', '.', 'BACK', 'ANGLE', 'TAB'];
  const DEMO_PTC = `PTC1
title=Demo Circuit
artist=Falling Keys
lanes=6
bpm=118
offset=0
N,time_ms,lane,note,duration_ms
N,500,0,60,0
N,750,1,64,0
N,1000,2,67,0
N,1250,3,72,0
N,1500,2,67,0
N,1750,1,64,0
N,2000,0,60,0
N,2250,3,72,0
N,2500,4,76,0
N,2750,5,79,0
N,3000,4,76,0
N,3250,3,72,0
N,3500,2,67,0
N,3750,1,64,0
N,4000,0,60,650
N,5000,3,72,0
N,5250,5,79,0
N,5500,4,76,0
N,5750,2,67,0
N,6000,1,64,0
N,6250,3,72,0
N,6500,0,60,0
N,6750,2,67,0
N,7000,4,76,0
N,7250,5,79,0
N,7500,3,72,650
N,8500,1,64,0
N,8750,2,67,0
N,9000,0,60,0
N,9250,5,79,0
N,9500,4,76,0
N,9750,3,72,0
N,10000,0,60,0
N,10250,1,64,0
N,10500,2,67,0
N,10750,3,72,0`.replace(/^\+/gm, '');

  const ui = {
    home: $('home-screen'), game: $('game-screen'), result: $('result-screen'),
    connection: $('connection-state'), portInput: $('port-input'), portList: $('port-list'),
    scan: $('scan-button'), connect: $('connect-button'), start: $('start-button'), watchDemo: $('watch-demo-button'),
    file: $('chart-file'), loadChart: $('load-chart-button'), demo: $('demo-button'),
    laneSelect: $('lane-select'), chartTitle: $('chart-title'),
    chartArtist: $('chart-artist'), chartSummary: $('chart-summary'), setupMessage: $('setup-message'),
    best: $('best-chip'), canvas: $('game-canvas'), playTitle: $('play-title'), playMeta: $('play-meta'),
    pause: $('pause-button'), back: $('back-button'), countdown: $('countdown'), pauseCard: $('pause-card'),
    inputLabel: $('input-label'), progressLabel: $('progress-label'), progressBar: $('progress-bar'),
    score: $('hud-score'), combo: $('hud-combo'), accuracy: $('hud-accuracy'), judgement: $('hud-judgement'),
    timing: $('hud-timing'), resultTitle: $('result-title'), resultScore: $('result-score'),
    resultAccuracy: $('result-accuracy'), resultCombo: $('result-combo'), resultPerfect: $('result-perfect'),
    resultMisses: $('result-misses'), resultHome: $('result-home'), retry: $('retry-button'),
    language: $('language-select'), mode: $('mode-select'),
    calibrationBpm: $('calibration-bpm'), calibrationBeats: $('calibration-beats'), calibrationStart: $('calibration-start'), calibrationReset: $('calibration-reset'), calibrationOffset: $('calibration-offset'), calibrationProgress: $('calibration-progress'), calibrationBeatCount: $('calibration-beat-count'), calibrationSampleCount: $('calibration-sample-count'), calibrationStatus: $('calibration-status'),
  };

  let currentChart;
  let chartIsDemo = true;
  let serialConnected = false;
  let bridgePort = '';
  let audioContext = null;
  let session = null;
  let animationFrame = 0;
  let pressedMask = 0;
  let laneCounts = [0, 0, 0, 0, 0, 0];
  let language = localStorage.getItem('fallingKeys.language') || 'en';
  let latencyMs = clamp(Number(localStorage.getItem('fallingKeys.latencyMs') || 0), -250, 250);
  const savedCalibrationBpm = Number(localStorage.getItem('fallingKeys.calibrationBpm') || 90);
  const savedCalibrationBeats = Number(localStorage.getItem('fallingKeys.calibrationBeats') || 12);
  const calibration = {
    active: false, starting: false, phase: 'idle', bpm: [60, 90, 120].includes(savedCalibrationBpm) ? savedCalibrationBpm : 90, measuredBeats: [8, 12, 16].includes(savedCalibrationBeats) ? savedCalibrationBeats : 12, leadInBeats: 4,
    beatIndex: 0, nextBeatAt: 0, timer: null, beats: [], samples: [], missed: 0, result: null,
  };

  function tr(key, values = {}) {
    const table = I18N[language] || I18N.en;
    return String(table[key] || I18N.en[key] || key).replace(/\{(\w+)\}/g, (_match, name) => values[name] ?? '');
  }

  function applyLanguage() {
    language = ui.language.value;
    localStorage.setItem('fallingKeys.language', language);
    document.documentElement.lang = language === 'zh' ? 'zh-CN' : 'en';
    document.querySelectorAll('[data-i18n]').forEach((element) => {
      const key = element.dataset.i18n;
      if (key) element.textContent = tr(key);
    });
    ui.connect.textContent = serialConnected ? tr('disconnect') : tr('connect');
    ui.scan.textContent = tr('scan');
    updateChartInfo();
    if (session) {
      ui.playMeta.textContent = `${tr(session.mode.key)} · ${session.chart.lanes} ${language === 'zh' ? '轨' : 'lanes'}`;
      ui.pause.innerHTML = `${session.status === 'paused' ? tr('resume') : tr('pause')} <kbd>Esc</kbd>`;
    }
    updateCalibrationUI();
  }

  function setMessage(message, kind = '') {
    ui.setupMessage.textContent = message;
    ui.setupMessage.className = `message ${kind}`.trim();
  }

  function setConnection(kind, text) {
    ui.connection.className = `connection ${kind}`;
    ui.connection.querySelector('span:last-child').textContent = text;
  }

  function scoreText(value) { return String(Math.max(0, Math.round(value))).padStart(6, '0'); }
  function timeText(ms) {
    const seconds = Math.max(0, Math.floor(ms / 1000));
    return `${Math.floor(seconds / 60)}:${String(seconds % 60).padStart(2, '0')}`;
  }
  function noteName(note) {
    const names = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'];
    return `${names[note % 12]}${Math.floor(note / 12) - 1}`;
  }
  function clamp(value, min, max) { return Math.max(min, Math.min(max, value)); }
  function judgmentLabel(text) {
    return ({ READY: tr('getReady'), OFFBEAT: tr('offbeat'), 'HOLD MISS': tr('holdMiss'), 'HOLD+': tr('holdPlus'), PERFECT: tr('perfectWord'), GOOD: tr('goodWord'), MISS: tr('missWord') })[text] || text;
  }

  function updateChartInfo() {
    ui.chartTitle.textContent = currentChart.title;
    ui.chartArtist.textContent = chartIsDemo ? tr('builtIn') : (currentChart.artist || tr('imported'));
    ui.laneSelect.disabled = !currentChart.flexibleLanes;
    ui.laneSelect.value = currentChart.flexibleLanes ? (localStorage.getItem('fallingKeys.lanes') || '4') : String(currentChart.lanes);
    ui.chartSummary.textContent = tr('chartSummary', { notes: currentChart.events.length, lanes: ui.laneSelect.value, bpm: currentChart.bpm, duration: timeText(currentChart.duration) });
    updateBest();
  }

  function bestKey() { return `fallingKeys.best.${currentChart.id}.${ui.laneSelect.value}.${ui.mode.value}`; }
  function updateBest() {
    const best = Number(localStorage.getItem(bestKey()) || 0);
    ui.best.textContent = best ? `${tr('best')} ${scoreText(best)}` : `${tr('best')} --`;
  }

  function resizeCanvas() {
    const rect = ui.canvas.getBoundingClientRect();
    const dpr = Math.min(2, window.devicePixelRatio || 1);
    const width = Math.max(1, Math.round(rect.width * dpr));
    const height = Math.max(1, Math.round(rect.height * dpr));
    if (ui.canvas.width !== width || ui.canvas.height !== height) {
      ui.canvas.width = width;
      ui.canvas.height = height;
    }
  }

  function ensureAudio() {
    if (!audioContext) audioContext = new AudioContext();
    if (audioContext.state === 'suspended') audioContext.resume();
    return audioContext;
  }

  function playToneAt(note, velocity = 0.7, duration = 0.16, when = audioContext?.currentTime || 0, gainScale = .11) {
    if (!audioContext) return;
    const start = Math.max(audioContext.currentTime, when);
    const oscillator = audioContext.createOscillator();
    const overtone = audioContext.createOscillator();
    const gain = audioContext.createGain();
    const overtoneGain = audioContext.createGain();
    const frequency = 440 * (2 ** ((note - 69) / 12));
    oscillator.type = 'triangle';
    overtone.type = 'sine';
    oscillator.frequency.value = frequency;
    overtone.frequency.value = frequency * 2.01;
    gain.gain.setValueAtTime(0.0001, start);
    gain.gain.exponentialRampToValueAtTime(Math.max(.008, gainScale * velocity), start + .008);
    gain.gain.exponentialRampToValueAtTime(.0001, start + duration);
    overtoneGain.gain.setValueAtTime(.0001, start);
    overtoneGain.gain.exponentialRampToValueAtTime(Math.max(.002, gainScale * .23 * velocity), start + .01);
    overtoneGain.gain.exponentialRampToValueAtTime(.0001, start + duration * .75);
    oscillator.connect(gain).connect(audioContext.destination);
    overtone.connect(overtoneGain).connect(audioContext.destination);
    oscillator.start(start); overtone.start(start);
    oscillator.stop(start + duration + .03); overtone.stop(start + duration + .03);
  }

  function playCalibrationClickAt(performanceTimeMs, accent = false) {
    if (!audioContext || audioContext.state !== 'running') return;
    const now = audioContext.currentTime;
    const scheduled = Math.max(now + .003, now + (performanceTimeMs - performance.now()) / 1000);
    const oscillator = audioContext.createOscillator();
    const gain = audioContext.createGain();
    oscillator.type = 'square';
    oscillator.frequency.value = accent ? 1500 : 1000;
    gain.gain.setValueAtTime(.0001, scheduled);
    gain.gain.exponentialRampToValueAtTime(accent ? .055 : .035, scheduled + .004);
    gain.gain.exponentialRampToValueAtTime(.0001, scheduled + .075);
    oscillator.connect(gain).connect(audioContext.destination);
    oscillator.start(scheduled);
    oscillator.stop(scheduled + .09);
  }

  function signed(value) { return `${Number(value) >= 0 ? '+' : ''}${Math.round(Number(value) || 0)}`; }
  function median(values) {
    if (!values.length) return 0;
    const ordered = [...values].sort((left, right) => left - right);
    const middle = Math.floor(ordered.length / 2);
    return ordered.length % 2 ? ordered[middle] : (ordered[middle - 1] + ordered[middle]) / 2;
  }
  function calibrationTotalBeats() { return calibration.leadInBeats + calibration.measuredBeats; }
  function setLatencyMs(value) {
    latencyMs = clamp(Math.round(Number(value) || 0), -250, 250);
    localStorage.setItem('fallingKeys.latencyMs', String(latencyMs));
    updateCalibrationUI();
  }

  function updateCalibrationUI() {
    if (!ui.calibrationStart) return;
    const measured = clamp(calibration.beatIndex - calibration.leadInBeats, 0, calibration.measuredBeats);
    const progress = calibration.measuredBeats ? measured / calibration.measuredBeats * 100 : 0;
    ui.calibrationBpm.value = String(calibration.bpm);
    ui.calibrationBeats.value = String(calibration.measuredBeats);
    ui.calibrationBpm.disabled = calibration.active || calibration.starting;
    ui.calibrationBeats.disabled = calibration.active || calibration.starting;
    ui.calibrationStart.disabled = calibration.starting;
    ui.calibrationStart.textContent = calibration.active ? tr('calibrationStop') : tr('calibrationStart');
    ui.calibrationReset.disabled = calibration.active || calibration.starting;
    ui.calibrationOffset.textContent = `${signed(latencyMs)} ms`;
    ui.calibrationProgress.style.width = `${calibration.active ? progress : calibration.result ? 100 : 0}%`;
    ui.calibrationBeatCount.textContent = `${measured} / ${calibration.measuredBeats}`;
    ui.calibrationSampleCount.textContent = `${calibration.samples.length} ${tr('calibrationSamples')}`;
    if (calibration.active) ui.calibrationStatus.textContent = calibration.phase === 'countin' ? tr('calibrationCountIn') : tr('calibrationMeasuring');
    else if (calibration.phase === 'error') ui.calibrationStatus.textContent = tr('calibrationTooFew');
    else if (calibration.result) ui.calibrationStatus.textContent = `${tr('calibrationDone')}: ${signed(calibration.result.offset)} ms · ${tr('calibrationSpread')} ±${calibration.result.spread} ms`;
    else ui.calibrationStatus.textContent = tr('calibrationReady');
    ui.start.disabled = !serialConnected || calibration.active || calibration.starting;
  }

  function finishLatencyCalibration() {
    if (!calibration.active) return;
    if (calibration.timer) window.clearTimeout(calibration.timer);
    calibration.timer = null;
    calibration.active = false;
    calibration.phase = 'done';
    calibration.missed = calibration.beats.filter((beat) => !beat.matched).length;
    if (calibration.samples.length < 4) {
      calibration.result = null;
      calibration.phase = 'error';
      setMessage(tr('calibrationTooFew'), 'error');
      updateCalibrationUI();
      return;
    }
    const ordered = [...calibration.samples].sort((left, right) => left - right);
    const trim = ordered.length >= 8 ? Math.floor(ordered.length * .15) : 0;
    const robust = ordered.slice(trim, ordered.length - trim || undefined);
    const offset = clamp(Math.round(median(robust)), -250, 250);
    const center = median(robust);
    const spread = Math.round(1.4826 * median(robust.map((value) => Math.abs(value - center))));
    calibration.result = { offset, spread };
    setLatencyMs(offset);
    setMessage(`${tr('calibrationDone')}: ${signed(offset)} ms`, 'ok');
    updateCalibrationUI();
  }

  function scheduleCalibrationBeat() {
    if (!calibration.active) return;
    const targetAt = calibration.nextBeatAt;
    const index = calibration.beatIndex;
    const measured = index >= calibration.leadInBeats;
    if (measured) calibration.beats.push({ at: targetAt, matched: false });
    calibration.phase = measured ? 'measure' : 'countin';
    playCalibrationClickAt(targetAt, index % 4 === 0);
    calibration.beatIndex += 1;
    calibration.nextBeatAt += 60000 / calibration.bpm;
    updateCalibrationUI();
    if (calibration.beatIndex >= calibrationTotalBeats()) {
      calibration.timer = window.setTimeout(finishLatencyCalibration, Math.max(0, calibration.nextBeatAt - performance.now() + 120));
      return;
    }
    calibration.timer = window.setTimeout(scheduleCalibrationBeat, Math.max(1, targetAt - performance.now() + 60000 / calibration.bpm - 25));
  }

  async function startLatencyCalibration() {
    if (calibration.active || calibration.starting) return;
    if (!serialConnected) {
      setMessage(tr('calibrationConnect'), 'error');
      return;
    }
    calibration.starting = true;
    updateCalibrationUI();
    try {
      const context = ensureAudio();
      if (context.state !== 'running') await context.resume();
      if (context.state !== 'running') throw new Error('Audio context is not running');
    } catch (_error) {
      calibration.starting = false;
      setMessage(tr('calibrationAudioError'), 'error');
      updateCalibrationUI();
      return;
    }
    calibration.bpm = Number(ui.calibrationBpm.value) || 90;
    calibration.measuredBeats = Number(ui.calibrationBeats.value) || 12;
    localStorage.setItem('fallingKeys.calibrationBpm', String(calibration.bpm));
    localStorage.setItem('fallingKeys.calibrationBeats', String(calibration.measuredBeats));
    calibration.starting = false;
    calibration.active = true;
    calibration.phase = 'countin';
    calibration.beatIndex = 0;
    calibration.nextBeatAt = performance.now() + 850;
    calibration.beats = [];
    calibration.samples = [];
    calibration.missed = 0;
    calibration.result = null;
    calibration.timer = window.setTimeout(scheduleCalibrationBeat, 1);
    setMessage(tr('calibrationCountIn'), '');
    updateCalibrationUI();
  }

  function stopLatencyCalibration(showMessage = true) {
    if (calibration.timer) window.clearTimeout(calibration.timer);
    calibration.timer = null;
    calibration.active = false;
    calibration.starting = false;
    calibration.phase = 'idle';
    calibration.beats = [];
    calibration.samples = [];
    calibration.missed = 0;
    calibration.result = null;
    if (showMessage) setMessage(tr('calibrationReady'));
    updateCalibrationUI();
  }

  function resetLatencyCalibration() {
    stopLatencyCalibration(false);
    setLatencyMs(0);
    setMessage(tr('calibrationReady'));
  }

  function handleCalibrationTap(keyId, action, timestamp) {
    if (!calibration.active || action !== 1 || laneForKey(keyId, 6) < 0) return;
    const beatMs = 60000 / calibration.bpm;
    const candidate = calibration.beats
      .filter((beat) => !beat.matched)
      .map((beat) => ({ beat, distance: Math.abs(timestamp - beat.at) }))
      .sort((left, right) => left.distance - right.distance)[0];
    if (!candidate || candidate.distance > Math.max(160, beatMs * .45)) return;
    candidate.beat.matched = true;
    calibration.samples.push(timestamp - candidate.beat.at);
    updateCalibrationUI();
  }

  function getChartForRun() {
    const requested = Number(ui.laneSelect.value);
    return currentChart.flexibleLanes ? mapLanes(currentChart, requested) : currentChart;
  }

  class GameSession {
    constructor(chart, difficultyKey, autoplay = false) {
      this.chart = chart;
      this.autoplay = autoplay;
      this.modeKey = difficultyKey;
      this.mode = MODES[difficultyKey] || MODES.arcade;
      this.config = this.mode;
      this.events = chart.events.map((event) => ({ ...event, state: 'upcoming', holding: false, released: false }));
      this.accompaniment = (chart.accompaniment || []).map((event) => ({ ...event })).sort((left, right) => left.time - right.time);
      this.duration = chart.duration;
      this.score = 0; this.combo = 0; this.maxCombo = 0; this.perfect = 0; this.good = 0; this.misses = 0; this.wrong = 0; this.accuracyPoints = 0; this.judgedParts = 0;
      this.status = 'countdown'; this.startedAtPerf = performance.now() + 1800; this.startedAtAudio = audioContext.currentTime + 1.8; this.pausedSongMs = 0; this.lastFrame = performance.now();
      this.lastJudgement = { text: 'READY', kind: '', until: 0, delta: 0 }; this.particles = []; this.laneDown = Array(chart.lanes).fill(false); this.audioIndex = 0;
      this.modeBeatLength = 60000 / Math.max(20, chart.bpm) * 4;
      if (this.mode.speed !== 1) {
        this.events.forEach((event) => { event.time /= this.mode.speed; event.duration /= this.mode.speed; });
        this.accompaniment.forEach((event) => { event.time /= this.mode.speed; event.duration /= this.mode.speed; });
        this.duration /= this.mode.speed;
      }
      if (this.mode.mirror) this.events.forEach((event) => { if (Math.floor(event.time / this.modeBeatLength) % 2) event.lane = chart.lanes - 1 - event.lane; });
      this.audioNotes = [
        ...this.events.map((event) => ({ time: event.time, note: event.note, duration: event.duration, velocity: 112, lead: true })),
        ...this.accompaniment.map((event) => ({ ...event, lead: false })),
      ].sort((left, right) => left.time - right.time || Number(right.lead) - Number(left.lead));
    }

    nowFromPerf(timestamp = performance.now(), applyCalibration = true) { return timestamp - this.startedAtPerf - (applyCalibration ? latencyMs : 0); }
    nowFromAudio() { return (audioContext.currentTime - this.startedAtAudio) * 1000; }
    now() { return this.status === 'paused' ? this.pausedSongMs : this.nowFromAudio(); }
    activeCount() { return this.events.reduce((count, event) => count + (event.state === 'upcoming' || event.state === 'holding' ? 1 : 0), 0); }
    setLaneDown(lane, down) { if (lane >= 0 && lane < this.laneDown.length) this.laneDown[lane] = down; }

    pressLane(lane, timestamp, applyCalibration = true) {
      if (this.status !== 'playing' || lane < 0 || lane >= this.chart.lanes) return;
      const now = this.nowFromPerf(timestamp, applyCalibration);
      let candidate = null; let distance = Infinity;
      for (const event of this.events) {
        if (event.lane !== lane || event.state !== 'upcoming') continue;
        const nextDistance = Math.abs(event.time - now);
        if (nextDistance < distance) { candidate = event; distance = nextDistance; }
        if (event.time > now + this.config.hit) break;
      }
      if (!candidate || distance > this.config.hit) {
        this.wrong += 1; this.combo = 0; this.lastJudgement = { text: 'OFFBEAT', kind: 'miss', until: performance.now() + 500, delta: 0 }; return;
      }
      const delta = now - candidate.time;
      const perfect = Math.abs(delta) <= this.config.perfect;
      candidate.state = candidate.duration > 0 ? 'holding' : 'hit';
      candidate.headHit = true; candidate.holding = candidate.duration > 0; candidate.holdStarted = now;
      this.combo += 1; this.maxCombo = Math.max(this.maxCombo, this.combo);
      if (perfect) { this.perfect += 1; this.accuracyPoints += 100; this.score += 1000 * this.multiplier(); this.lastJudgement = { text: 'PERFECT', kind: '', until: performance.now() + 520, delta }; }
      else { this.good += 1; this.accuracyPoints += 70; this.score += 500 * this.multiplier(); this.lastJudgement = { text: 'GOOD', kind: 'good', until: performance.now() + 520, delta }; }
      this.judgedParts += 1; this.spawnParticles(lane, perfect ? 14 : 7);
    }

    releaseLane(lane, timestamp, applyCalibration = true) {
      if (this.status !== 'playing') return;
      const now = this.nowFromPerf(timestamp, applyCalibration);
      const hold = this.events.find((event) => event.lane === lane && event.state === 'holding');
      if (!hold) return;
      if (now >= hold.time + hold.duration - this.config.grace) this.completeHold(hold);
      else this.failHold(hold);
    }

    completeHold(event) { if (event.state !== 'holding') return; event.state = 'hit'; event.holding = false; this.accuracyPoints += 100; this.judgedParts += 1; this.score += 400 * this.multiplier(); this.lastJudgement = { text: 'HOLD+', kind: '', until: performance.now() + 420, delta: 0 }; }
    failHold(event) { if (event.state !== 'holding') return; event.state = 'miss'; event.holding = false; this.misses += 1; this.combo = 0; this.lastJudgement = { text: 'HOLD MISS', kind: 'miss', until: performance.now() + 560, delta: 0 }; }
    multiplier() { return 1 + Math.min(3, Math.floor(this.combo / 10)); }

    autoplayStep(now) {
      if (!this.autoplay) return;
      for (const event of this.events) {
        if (event.state === 'upcoming' && now >= event.time) {
          this.setLaneDown(event.lane, true);
          this.pressLane(event.lane, this.startedAtPerf + event.time, false);
          if (!event.duration) this.setLaneDown(event.lane, false);
        } else if (event.state === 'holding' && now >= event.time + event.duration) {
          this.setLaneDown(event.lane, false);
          this.releaseLane(event.lane, this.startedAtPerf + event.time + event.duration, false);
        }
      }
    }

    update() {
      if (this.status === 'countdown') {
        const remaining = this.startedAtPerf - performance.now();
        if (remaining <= 0) { this.status = 'playing'; ui.countdown.classList.add('hidden'); }
        else { ui.countdown.classList.remove('hidden'); ui.countdown.textContent = remaining > 1100 ? '3' : remaining > 500 ? '2' : '1'; }
        this.scheduleAudio();
        return;
      }
      if (this.status !== 'playing') return;
      const now = this.now();
      this.scheduleAudio();
      this.autoplayStep(now);
      for (const event of this.events) {
        if (event.state === 'upcoming' && now > event.time + this.config.hit) this.missEvent(event);
        if (event.state === 'holding') {
          if (now >= event.time + event.duration - this.config.grace && this.laneDown[event.lane]) this.completeHold(event);
          else if (now >= event.time + event.duration + this.config.grace) this.failHold(event);
        }
      }
      if (now > this.duration + 1000 && this.activeCount() === 0) this.finish();
    }

    missEvent(event) { event.state = 'miss'; this.misses += 1; this.combo = 0; this.lastJudgement = { text: 'MISS', kind: 'miss', until: performance.now() + 500, delta: 0 }; }
    spawnParticles(lane, amount) { for (let i = 0; i < amount; i += 1) this.particles.push({ lane, x: Math.random(), y: 0.82, life: 1, speed: .15 + Math.random() * .4, size: 2 + Math.random() * 4 }); }
    scheduleAudio() {
      if (!audioContext || !this.audioNotes.length) return;
      const horizon = (audioContext.currentTime - this.startedAtAudio) * 1000 + 260;
      while (this.audioIndex < this.audioNotes.length && this.audioNotes[this.audioIndex].time <= horizon) {
        const event = this.audioNotes[this.audioIndex++];
        const gainScale = event.lead ? .11 : this.mode.autoGain * .23;
        if (event.time >= -20) playToneAt(event.note, Math.min(1, (event.velocity || 80) / 112), Math.max(.06, Math.min(1.4, event.duration / 1000 || .16)), this.startedAtAudio + event.time / 1000, gainScale);
      }
    }
    togglePause() { if (this.status === 'playing') this.pause(); else if (this.status === 'paused') this.resume(); }
    pause() { this.pausedSongMs = this.now(); this.status = 'paused'; ui.pauseCard.classList.remove('hidden'); ui.pause.textContent = tr('resume'); audioContext.suspend(); }
    resume() { this.status = 'playing'; this.startedAtPerf = performance.now() - this.pausedSongMs; ui.pause.innerHTML = `${tr('pause')} <kbd>Esc</kbd>`; audioContext.resume(); }
    finish() { this.status = 'finished'; showResult(this); }
  }

  function drawSession(game) {
    resizeCanvas();
    const ctx = ui.canvas.getContext('2d');
    const rect = ui.canvas.getBoundingClientRect(); const dpr = Math.min(2, window.devicePixelRatio || 1);
    const w = Math.max(1, rect.width); const h = Math.max(1, rect.height);
    ctx.setTransform(dpr, 0, 0, dpr, 0, 0); ctx.clearRect(0, 0, w, h);
    const lanes = game.chart.lanes; const left = Math.max(58, w * .13); const right = Math.min(w - 58, w * .87); const playWidth = Math.max(160, right - left); const laneWidth = playWidth / lanes; const top = 26; const hitY = h - 72; const now = game.now();
    ctx.fillStyle = '#0b1012'; ctx.fillRect(0, 0, w, h);
    ctx.fillStyle = '#111a1d'; ctx.fillRect(left, 0, playWidth, h);
    ctx.strokeStyle = '#253237'; ctx.lineWidth = 1;
    for (let lane = 0; lane <= lanes; lane += 1) { const x = left + lane * laneWidth; ctx.beginPath(); ctx.moveTo(x, 0); ctx.lineTo(x, h); ctx.stroke(); }
    ctx.strokeStyle = '#405158'; ctx.lineWidth = 2; ctx.beginPath(); ctx.moveTo(left, hitY); ctx.lineTo(right, hitY); ctx.stroke();
    ctx.fillStyle = '#63747a'; ctx.font = '600 11px Segoe UI, sans-serif'; ctx.textAlign = 'center';
    for (let lane = 0; lane < lanes; lane += 1) { const x = left + laneWidth * lane + laneWidth / 2; ctx.fillStyle = game.laneDown[lane] ? COLORS[lane] : '#63747a'; ctx.fillText(`L${lane + 1}`, x, hitY + 30); }
    const firstVisible = now - 160; const lastVisible = now + game.config.approach;
    for (const event of game.events) {
      if (event.time + event.duration < firstVisible || event.time > lastVisible) continue;
      const x = left + event.lane * laneWidth + 8; const noteWidth = laneWidth - 16;
      const y = hitY - ((event.time - now) / game.config.approach) * (hitY - top);
      const color = COLORS[event.lane % COLORS.length];
      if (event.duration > 0) {
        const endY = hitY - ((event.time + event.duration - now) / game.config.approach) * (hitY - top);
        ctx.globalAlpha = event.state === 'miss' ? .15 : event.state === 'hit' ? .3 : .7;
        ctx.fillStyle = color; ctx.fillRect(x + noteWidth * .28, Math.min(y, endY), noteWidth * .44, Math.max(4, Math.abs(endY - y)));
        ctx.globalAlpha = 1;
      }
      if (event.state !== 'hit' || event.duration > 0) {
        ctx.globalAlpha = event.state === 'miss' ? .16 : event.state === 'hit' ? .45 : 1;
        ctx.fillStyle = color; ctx.beginPath(); ctx.roundRect(x, y - 7, noteWidth, 14, 4); ctx.fill();
        ctx.fillStyle = '#ffffffaa'; ctx.fillRect(x + 6, y - 3, Math.max(2, noteWidth * .28), 2); ctx.globalAlpha = 1;
      }
    }
    for (let index = game.particles.length - 1; index >= 0; index -= 1) {
      const particle = game.particles[index]; particle.life -= .035; particle.y -= particle.speed * .012;
      if (particle.life <= 0) { game.particles.splice(index, 1); continue; }
      const x = left + particle.lane * laneWidth + laneWidth * particle.x; const y = hitY + (particle.y - .82) * h;
      ctx.globalAlpha = particle.life; ctx.fillStyle = COLORS[particle.lane % COLORS.length]; ctx.fillRect(x, y, particle.size, particle.size); ctx.globalAlpha = 1;
    }
    const accent = COLORS[Math.floor((now / 400) % COLORS.length)]; ctx.globalAlpha = .16; ctx.fillStyle = accent; ctx.fillRect(left, hitY - 2, playWidth, 4); ctx.globalAlpha = 1;
    if (game.status === 'paused') { ctx.fillStyle = '#ffffff10'; ctx.fillRect(left, 0, playWidth, h); }
  }

  function updateHud(game) {
    ui.score.textContent = scoreText(game.score); ui.combo.textContent = String(game.combo); ui.accuracy.textContent = game.judgedParts ? `${((game.accuracyPoints / (game.judgedParts * 100)) * 100).toFixed(1)}%` : '--';
    const judgment = game.lastJudgement; ui.judgement.textContent = performance.now() < judgment.until ? judgmentLabel(judgment.text) : (game.status === 'countdown' ? judgmentLabel('READY') : ''); ui.judgement.className = `judgement ${judgment.kind}`;
    ui.timing.textContent = judgment.delta ? `${judgment.delta > 0 ? '+' : ''}${Math.round(judgment.delta)} ms` : game.combo >= 10 ? `${game.multiplier()}x` : '';
    const now = clamp(game.now(), 0, game.duration); ui.progressLabel.textContent = `${timeText(now)} / ${timeText(game.duration)}`; ui.progressBar.style.width = `${(now / Math.max(1, game.duration)) * 100}%`;
  }

  function loop() {
    if (session) { session.update(); drawSession(session); updateHud(session); if (session.status === 'countdown') ui.inputLabel.textContent = tr('getReady'); else if (session.status === 'paused') ui.inputLabel.textContent = tr('paused'); else if (session.autoplay) ui.inputLabel.textContent = tr('autoPilot'); else ui.inputLabel.textContent = serialConnected ? `${bridgePort} · ${tr('controllerInput')}` : tr('controllerDisconnected'); }
    animationFrame = requestAnimationFrame(loop);
  }

  function showScreen(name) { ui.home.classList.toggle('hidden', name !== 'home'); ui.game.classList.toggle('hidden', name !== 'game'); ui.result.classList.toggle('hidden', name !== 'result'); }
  function startRun(autoplay = false) {
    if (!autoplay && !serialConnected) { setMessage(tr('connectFirstShort'), 'error'); return; }
    stopLatencyCalibration(false);
    ensureAudio(); if (session) session = null; session = new GameSession(getChartForRun(), ui.mode.value, autoplay); ui.playTitle.textContent = session.chart.title; ui.playMeta.textContent = `${tr(session.mode.key)} · ${session.chart.lanes} ${language === 'zh' ? '轨' : 'lanes'}${autoplay ? ` · ${tr('autoPilot')}` : ''} · ${session.chart.bpm} BPM`; showScreen('game'); ui.countdown.classList.remove('hidden'); ui.pauseCard.classList.add('hidden');
  }
  function showResult(game) {
    const accuracy = game.judgedParts ? (game.accuracyPoints / (game.judgedParts * 100)) * 100 : 0; const best = Number(localStorage.getItem(bestKey()) || 0); if (game.score > best) localStorage.setItem(bestKey(), String(game.score));
    ui.resultTitle.textContent = accuracy >= 95 ? tr('brilliant') : accuracy >= 80 ? tr('nice') : tr('keepBeat'); ui.resultScore.textContent = scoreText(game.score); ui.resultAccuracy.textContent = `${accuracy.toFixed(1)}%`; ui.resultCombo.textContent = String(game.maxCombo); ui.resultPerfect.textContent = String(game.perfect); ui.resultMisses.textContent = String(game.misses + game.wrong); showScreen('result'); updateBest();
  }
  function backHome() { stopLatencyCalibration(false); if (session) { session = null; } ui.countdown.classList.add('hidden'); ui.pauseCard.classList.add('hidden'); showScreen('home'); updateChartInfo(); }

  function reconcileState(mask) {
    pressedMask = Number(mask) >>> 0; laneCounts = Array(6).fill(0);
    for (let id = 0; id < 30; id += 1) if (maskHas(pressedMask, id)) { const lane = laneForKey(id, session ? session.chart.lanes : 6); if (lane >= 0) laneCounts[lane] += 1; }
    if (session && !session.autoplay) for (let lane = 0; lane < session.chart.lanes; lane += 1) session.setLaneDown(lane, laneCounts[lane] > 0);
  }
  function handleKey(event) {
    const id = Number(event.key_id); const action = Number(event.action_id); const timestamp = performance.now();
    handleCalibrationTap(id, action, timestamp);
    pressedMask = action === 1 ? (pressedMask | (2 ** id)) >>> 0 : action === 2 ? (pressedMask & ~(2 ** id)) >>> 0 : pressedMask;
    if (id === 0 && action === 1) { if (session) session.togglePause(); return; }
    if (session?.autoplay) return;
    if (action === 3 || id === 24) return;
    const lane = laneForKey(id, session ? session.chart.lanes : 6); if (lane < 0) return;
    if (action === 1) { laneCounts[lane] += 1; if (session && laneCounts[lane] === 1) { session.setLaneDown(lane, true); session.pressLane(lane, timestamp); } }
    if (action === 2) { laneCounts[lane] = Math.max(0, laneCounts[lane] - 1); if (session && laneCounts[lane] === 0) { session.setLaneDown(lane, false); session.releaseLane(lane, timestamp); } }
    ui.inputLabel.textContent = `${event.key || KEY_LABELS[id] || 'key'} · ${action === 1 ? 'down' : 'up'}`;
  }
  function handleSerialData(data) { if (data.type === 'device_info') { serialConnected = true; setConnection('online', language === 'zh' ? `计算器已连接 · 固件 ${data.firmware}` : `Calculator online · FW ${data.firmware}`); ui.connect.textContent = tr('disconnect'); ui.start.disabled = false; setMessage(tr('controllerReady'), 'ok'); updateCalibrationUI(); }
    if (data.type === 'key') handleKey(data); if (data.type === 'state') reconcileState(data.state_mask); }
  function handleSerialStatus(status) { if (status.kind === 'connecting') { setConnection('busy', language === 'zh' ? `正在连接 ${status.port}...` : `Connecting ${status.port}...`); ui.connect.textContent = language === 'zh' ? '连接中' : 'Connecting'; }
    if (status.kind === 'error') { serialConnected = false; ui.start.disabled = true; setConnection('offline', tr('controllerOffline')); ui.connect.textContent = tr('connect'); setMessage(status.message, 'error'); }
    if (status.kind === 'disconnected') { stopLatencyCalibration(false); serialConnected = false; ui.start.disabled = true; pressedMask = 0; laneCounts = Array(6).fill(0); setConnection('offline', tr('controllerOffline')); ui.connect.textContent = tr('connect'); if (session && session.status === 'playing' && !session.autoplay) session.pause(); setMessage(tr('disconnected'), 'error'); updateCalibrationUI(); } }

  async function scanPorts() { ui.scan.disabled = true; ui.scan.textContent = language === 'zh' ? '扫描中' : 'Scanning'; const result = await window.fallingKeys.listPorts(); ui.portList.replaceChildren(); if (result.ok) { for (const port of result.ports) { const option = document.createElement('option'); option.value = port.device; option.label = port.description || port.device; ui.portList.appendChild(option); } setMessage(result.ports.length ? tr('serialFound', { count: result.ports.length }) : tr('noPorts'), result.ports.length ? 'ok' : ''); } else setMessage(result.error || 'Could not scan serial ports.', 'error'); ui.scan.disabled = false; ui.scan.textContent = tr('scan'); }
  async function connectPort() { if (serialConnected) { await window.fallingKeys.disconnect(); return; } bridgePort = ui.portInput.value.trim(); if (!bridgePort) { setMessage(tr('enterPort'), 'error'); return; } ui.connect.disabled = true; const result = await window.fallingKeys.connect(bridgePort); ui.connect.disabled = false; if (!result.ok) setMessage(result.error || tr('invalidPort'), 'error'); }
  async function loadFile(file) { if (!file) return; try {
    const isMidi = /\.(mid|midi)$/i.test(file.name);
    currentChart = isMidi ? parseMIDI(await file.arrayBuffer(), { source: file.name, lanes: Number(ui.laneSelect.value) }) : parsePTC(await file.text(), { source: file.name });
    chartIsDemo = false; updateChartInfo(); const suffix = isMidi && currentChart.stats ? ` · ${currentChart.stats.melodyNotes} melody / ${currentChart.stats.accompanimentNotes} auto` : ''; setMessage(`${tr('loaded', { name: file.name })}${suffix}`, 'ok');
  } catch (error) { setMessage(error.message, 'error'); } }

  currentChart = parsePTC(DEMO_PTC, { id: 'demo-circuit', source: 'built-in', flexibleLanes: true });
  ui.language.value = language; ui.mode.value = localStorage.getItem('fallingKeys.mode') || 'arcade'; applyLanguage();
  ui.language.addEventListener('change', applyLanguage);
  ui.laneSelect.addEventListener('change', () => { localStorage.setItem('fallingKeys.lanes', ui.laneSelect.value); updateChartInfo(); });
  ui.calibrationBpm.addEventListener('change', () => { calibration.bpm = Number(ui.calibrationBpm.value) || 90; localStorage.setItem('fallingKeys.calibrationBpm', String(calibration.bpm)); updateCalibrationUI(); }); ui.calibrationBeats.addEventListener('change', () => { calibration.measuredBeats = Number(ui.calibrationBeats.value) || 12; localStorage.setItem('fallingKeys.calibrationBeats', String(calibration.measuredBeats)); updateCalibrationUI(); });
  ui.mode.addEventListener('change', () => { localStorage.setItem('fallingKeys.mode', ui.mode.value); updateBest(); }); ui.scan.addEventListener('click', scanPorts); ui.connect.addEventListener('click', connectPort); ui.start.addEventListener('click', () => startRun(false)); ui.watchDemo.addEventListener('click', () => startRun(true));
  ui.loadChart.addEventListener('click', () => ui.file.click()); ui.file.addEventListener('change', () => loadFile(ui.file.files[0])); ui.demo.addEventListener('click', () => { currentChart = parsePTC(DEMO_PTC, { id: 'demo-circuit', source: 'built-in', flexibleLanes: true }); chartIsDemo = true; updateChartInfo(); setMessage('Demo chart ready.', 'ok'); });
  ui.pause.addEventListener('click', () => { if (session) session.togglePause(); }); ui.back.addEventListener('click', backHome); ui.resultHome.addEventListener('click', backHome); ui.retry.addEventListener('click', () => startRun(Boolean(session?.autoplay)));
  ui.calibrationStart.addEventListener('click', () => calibration.active ? stopLatencyCalibration() : startLatencyCalibration()); ui.calibrationReset.addEventListener('click', resetLatencyCalibration);
  const canvasObserver = typeof ResizeObserver === 'function' ? new ResizeObserver(resizeCanvas) : null;
  if (canvasObserver) canvasObserver.observe(ui.canvas);
  window.addEventListener('resize', resizeCanvas); window.addEventListener('keydown', (event) => { if (event.key === 'Escape' && session) { event.preventDefault(); session.togglePause(); } });
  if (window.fallingKeys) { window.fallingKeys.onSerialData(handleSerialData); window.fallingKeys.onSerialStatus(handleSerialStatus); }
  scanPorts(); animationFrame = requestAnimationFrame(loop);
})();
