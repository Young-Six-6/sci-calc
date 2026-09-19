(() => {
  'use strict';

  const { KEY_NAMES, TONE_NAMES, GM_PROGRAMS, ROOT_NAMES, LAYOUT_NAMES, keyboardBaseNote, keyToNotes, clamp } = window.MusicKeymap;
  const { encodeTake } = window.MusicMidiWriter;
  const { holdEnvelopeAtTime } = window.MusicAudioEnvelope;
  const { ARP_PATTERNS, ARP_RATES, DRUM_LANES, PATTERN_LENGTH, buildArpeggio, sanitizeGrooveSettings, stepIntervalMs } = window.MusicGroove;
  const { difficultyConfig, buildChallengePhrase, timingWindowMs, previewDurationMs, targetKeyIds, scoreForStep, normalizeDifficulty } = window.MusicChallenge;
  const { buildChordVoicing, normalizeChordType } = window.MusicChord;
  const { DEFAULT_KEY_IDS: SAMPLER_KEY_IDS, buildSlices, sliceForKey } = window.MusicSampler;
  const MAX_VOICES = 16;
  const MAX_RECORDED_EVENTS = 2048;
  const PAGE_ORDER = ['keys', 'groove', 'synth', 'rec', 'learn', 'convert', 'scope', 'settings'];

  const I18N = {
    en: {
      workspace: 'WORKSPACE', inputDevice: 'INPUT DEVICE', panic: 'Panic / silence', liveInstrument: 'LIVE INSTRUMENT',
      calculatorKeys: 'Calculator keys', calculatorKeysLead: 'Every sound comes from the connected calculator. Play naturally, or hold SHIFT for controls.',
      tone: 'Tone', range: 'Range', activeNotes: 'Active notes', recordState: 'Record', pianoRoll: 'PIANO ROLL', twoOctaveRange: 'Two-octave visual range',
      pianoView: 'Piano', calculatorView: 'Calculator', calculatorViewKicker: 'CALCULATOR VIEW', calculatorPreview: 'Calculator keyboard',
      pressed: 'Pressed', sounding: 'Sounding', rootNote: 'Root', physicalMatrix: 'PHYSICAL MATRIX', keyState: 'Key state',
      instrumentTelemetry: 'INSTRUMENT TELEMETRY', scopeTitle: 'Scope', scopeLead: 'A live view of the desktop synth output and calculator input load.',
      waveform: 'WAVEFORM', audioOutput: 'Audio output', voices: 'Voices', peak: 'Peak', renderLoad: 'Render load', scopeNotes: 'NOTES',
      scopeRate: 'Refresh', fps: 'FPS',
      scopeNotesBody: 'The desktop engine keeps the same 16-voice ceiling as the calculator firmware. The scope is diagnostic only and never changes the sound path.',
      capture: 'CAPTURE', recorderTitle: 'Recorder', recorderLead: 'Record the calculator performance, replay it, and export a standard Format 0 MIDI file.',
      takeDetails: 'TAKE DETAILS', events: 'Events', duration: 'Duration', initialTone: 'Initial tone', initialTempo: 'Initial tempo', savedFile: 'Saved file',
      recentEvents: 'RECENT EVENTS', eventMonitor: 'Event monitor', noEvents: 'No recorded events yet.', startRecording: 'Start recording', stopRecording: 'Stop recording',
      playTake: 'Play take', stopPlayback: 'Stop playback', clearTake: 'Clear take', saveMidi: 'Save MIDI',
      instrumentSetup: 'INSTRUMENT SETUP', settingsTitle: 'Settings', settingsLead: "The controls mirror the calculator's MusicKeyboard settings page.",
      sound: 'SOUND', volume: 'Volume', velocity: 'Velocity', sustain: 'Sustain', metronome: 'Metronome', pitch: 'PITCH', octave: 'Octave', root: 'Root',
      transpose: 'Transpose', layout: 'Layout', tempo: 'TEMPO', bpm: 'BPM', tempoHint: 'Tempo is recorded into MIDI when it changes during a take.',
      latencyCalibration: 'LATENCY CALIBRATION', latencyCalibrationTitle: 'Input timing calibration',
      latencyCalibrationLead: 'Listen to the click, then press one note key on every beat. The measured timing offset becomes the global correction for recorded and timed performance data.',
      calibrationTempo: 'Calibration tempo', calibrationBeats: 'Measured beats', startCalibration: 'Start calibration', stopCalibration: 'Stop calibration', resetCalibration: 'Reset',
      calibrationReady: 'Ready. Enable audio before starting.', calibrationCountIn: 'Count-in: get ready...', calibrationMeasuring: 'Tap one note key on every beat.', calibrationDone: 'Calibration complete', calibrationTooFew: 'Not enough valid taps; no setting was changed.',
      calibrationNote: 'A positive value means the key event arrived late, so recording timestamps are moved earlier by that amount. Live audio cannot be played before the physical key arrives.',
      samples: 'samples', sampleSpread: 'spread',
      quickControls: 'QUICK CONTROLS', shiftControls: 'Shift controls', changeTone: 'Change tone', changeOctave: 'Change octave', changeTranspose: 'Transpose',
      changeTempo: 'Change BPM', changeLayout: 'Change layout', recordShortcut: 'Record', navPlay: 'PLAY', navCreate: 'CREATE', navLearn: 'LEARN', navTools: 'TOOLS', keys: 'Keys', learn: 'Learn', scope: 'Scope', recorder: 'Recorder', settings: 'Settings', synth: 'Synth',
      synthTitle: 'Analog synthesizer', synthLead: 'Shape a desktop patch while the calculator remains the controller. Changes affect notes started after the edit.', synthMode: 'Engine', factoryTone: 'Factory tone', customPatch: 'Custom patch', synthPreset: 'Preset', applyPreset: 'Apply preset', resetSynth: 'Reset patch', presetApplied: 'Preset applied', customReady: 'Custom patch active', factoryReady: 'Factory tone active', oscillatorOne: 'OSCILLATOR 1', oscillatorTwo: 'OSCILLATOR 2', waveform: 'Waveform', waveSine: 'Sine', waveTriangle: 'Triangle', waveSaw: 'Saw', waveSquare: 'Square', oscLevel: 'Level', detune: 'Detune', noise: 'Noise', filter: 'FILTER', cutoff: 'Cutoff', resonance: 'Resonance', envelope: 'AMPLITUDE ENVELOPE', attack: 'Attack', decay: 'Decay', sustainLevel: 'Sustain', release: 'Release', modulation: 'MODULATION', lfoRate: 'LFO rate', lfoDepth: 'Pitch depth', synthHint: 'The custom patch uses two oscillators, filtered noise, a low-pass filter, ADSR, and pitch LFO within the same 16-voice ceiling.',
      groove: 'Groove', grooveTitle: 'Drum machine + arpeggiator', grooveLead: 'Build a loop with the calculator as the controller: drums provide the pulse and the arpeggiator turns held notes into a pattern.', grooveTransport: 'GROOVE TRANSPORT', groovePlay: 'Play groove', grooveStop: 'Stop groove', grooveClear: 'Clear pattern', grooveRandom: 'Randomize', grooveSteps: 'Steps', grooveSixteen: '16 steps', grooveThirtyTwo: '32 steps', grooveBpm: 'Groove BPM', grooveResolution: 'Grid', grooveEighth: '1/8', grooveSixteenth: '1/16', drumMachine: 'DRUM MACHINE', drumMachineTitle: 'Step rhythm', drumMachineLead: 'Click a lane to add a hit. The pattern is saved locally and can run beside live playing.', pattern: 'Pattern', kick: 'Kick', snare: 'Snare', hat: 'Hi-hat', clap: 'Clap', arpTitle: 'Arpeggiator', arpLead: 'Hold one or more calculator notes. The desktop engine cycles through them while you keep the keys down.', arpEnable: 'Arpeggiator', arpOn: 'On', arpOff: 'Off', arpPattern: 'Pattern', arpUp: 'Up', arpDown: 'Down', arpUpDown: 'Up / down', arpRandom: 'Random', arpRate: 'Rate', arpOctaves: 'Octaves', arpGate: 'Gate', arpHint: 'The arpeggiator uses the same tone, transpose, velocity, and 16-voice ceiling as live notes. Disable it to return to direct playing.', grooveCalcKeys: 'Calculator keys', grooveCalcKeysHint: 'Drums and live notes', grooveShift: 'SHIFT', grooveShiftHint: 'Existing tone and transport controls', grooveLocal: 'Groove', grooveLocalHint: 'Tempo and pattern stay local to this desktop app', looper: 'LOOPER', looperTitle: 'Layered loop', looperLead: 'Record a fixed-length base layer, then add more layers one cycle at a time. Loop playback does not capture itself.', looperRecordBase: 'Record base', looperPlay: 'Play loop', looperRecordLayer: 'Record layer', looperCancelArm: 'Cancel overdub', looperArmed: 'Next loop armed', looperRecording: 'Recording', looperOverdub: 'Overdub', looperStop: 'Stop looper', looperClear: 'Clear layers', looperBars: 'Bars', looperBar: 'bars', looperNoLayers: 'No layers yet.', looperLayer: 'Layer', looperNotes: 'notes', looperMute: 'Mute', looperUnmute: 'Unmute', looperRemove: 'Remove', looperPlaying: 'Looping', grooveReady: 'Ready', groovePlaying: 'Playing',
      chordAssist: 'CHORD ASSIST', chordAssistTitle: 'Chord accompaniment', chordAssistLead: 'Hold one calculator note to add a chord and a timed bass pulse.', chordAssistEnable: 'Chord assistant', chordType: 'Chord type', chordTriad: 'Major triad', chordMinor: 'Minor triad', chordSeventh: 'Seventh', chordSus4: 'Suspended 4', chordBass: 'Auto bass', chordPulse: 'Bass pulse', chordOn: 'On', chordOff: 'Off', chordHint: 'The assistant uses the current tone and stays within the desktop 16-voice ceiling. Turn it off to return to direct notes.',
      sampler: 'SAMPLER', samplerTitle: 'Sample keyboard', samplerLead: 'Import one audio file and divide it across the calculator keys for quick one-shot playback.', samplerChoose: 'Choose audio', samplerNoFile: 'No sample loaded', samplerReady: 'Sample ready', samplerLoading: 'Decoding sample', samplerError: 'Sample error', samplerEnable: 'Sample mode', samplerSlices: 'Slices', samplerSliceCount: 'Slice count', samplerStart: 'Start', samplerEnd: 'End', samplerSpeed: 'Speed', samplerKeys: '1 / 2 / 3 / 4 / 5 / 6 / 7 / 8 / 9 / 0 / . / SIN', samplerHint: 'Use the calculator number keys 1-9, 0, ., and SIN. Samples are kept in memory for this session and do not touch the calculator firmware.',
      soundLab: 'SOUND LAB', soundLabTitle: 'Waveform + spectrum', soundLabOpen: 'Open for analysis', soundLabLead: 'Compare the live waveform, a lightweight filtered trace, and the output spectrum.', soundLabRaw: 'Live waveform', soundLabFiltered: 'Filtered trace', soundLabSpectrum: 'Spectrum', soundLabFilter: 'Trace smoothing', soundLabGain: 'Display gain', soundLabHint: 'The lab is diagnostic only. It does not add another audio path or change the synth voice limit.',
      calculator: 'Calculator', chords: 'Chords', chromatic: 'Chromatic', ready: 'Ready', noTake: 'No take', audioEnable: 'Enable audio', audioReady: 'Audio ready',
      refresh: 'Refresh', connect: 'Connect', disconnect: 'Disconnect', noPort: 'No port selected', waitingCalculator: 'Waiting for calculator',
      connected: 'Connected', connecting: 'Connecting', disconnected: 'Disconnected', serialError: 'Serial error', serialHint: 'Connect the calculator by USB serial to begin.',
      audioHint: 'Enable audio before playing.', deviceReady: 'Calculator ready', deviceDetail: 'SKB1 / 5 x 6 matrix', packets: 'packets', readyToRecord: 'Ready to record',
      recording: 'Recording', playing: 'Playing take', takeInMemory: 'Take in memory', noTakeToPlay: 'No take to play', full: 'Event limit reached', saved: 'Saved',
      canceled: 'Save canceled', midiError: 'MIDI save failed', silence: 'Silenced', keyboard: 'Keyboard', scopeAudio: 'Web Audio',
      convert: 'Convert', midiTools: 'MIDI TOOLS', convertTitle: 'Calculator sheet', convertLead: 'Turn a MIDI melody into a readable SCI-CALC key sheet, with the same key names used by the instrument.',
      calcSheet: 'CALC SHEET', sourceMidi: 'SOURCE MIDI', noMidiSelected: 'No MIDI selected', chooseMidi: 'Choose MIDI', chooseMidiHint: 'Choose a MIDI file to begin.',
      rhythmGrid: 'Rhythm grid', melodyTrack: 'Melody track', extraKeyLabel: 'Extra key label', fullTrack: 'Use full selected track', includeChords: 'Include chord suggestions',
      convertMidi: 'Convert MIDI', report: 'REPORT', sheetPreview: 'Key sheet preview', reportEmpty: 'The converted report will appear here.',
      converterReady: 'Ready to convert', converting: 'Converting MIDI', convertDone: 'Conversion complete',
      lessonNow: 'LESSON / NEXT STEP', previous: 'Previous', skip: 'Skip', stopLesson: 'Stop lesson', guidedPractice: 'GUIDED PRACTICE', lessonTitle: 'MIDI lesson',
      lessonLead: 'Import a MIDI file and follow the highlighted calculator keys at your own pace.', waitForYou: 'WAIT FOR YOU', lessonMidi: 'LESSON MIDI',
      lessonChooseHint: 'Choose a MIDI file to build a lesson.', chordComplexity: 'Chord complexity', melodyOnly: 'Melody only', simpleChord: 'Simple chord', fullChord: 'Full chord',
      melodyOnlyHint: 'One highlighted note at a time.', simpleChordHint: 'The melody plus one playable harmony note.', fullChordHint: 'Up to four notes that can be held together.',
      startLesson: 'Start lesson', restartLesson: 'Restart', lessonSteps: 'Steps', recommendedRange: 'Recommended', nextStep: 'NEXT STEP', mistakes: 'mistakes',
      lessonLoading: 'Analyzing MIDI and building the lesson...', lessonReady: 'Lesson ready', lessonComplete: 'Lesson complete', lessonError: 'Could not build lesson',
      lessonActive: 'Follow the highlighted keys', enterFirst: 'Hold ENTER first', lessonStopped: 'Lesson paused',
      challenge: 'CHALLENGE', challengeTitle: 'Follow challenge', challengeLead: 'The desktop previews a short phrase, then waits for you to replay it on the calculator. Difficulty grows as you clear rounds.', challengeDifficulty: 'Difficulty', challengeEasy: 'Easy', challengeNormal: 'Normal', challengeHard: 'Hard', challengeStart: 'Start challenge', challengeStop: 'Stop challenge', challengeRestart: 'Restart challenge', challengeReady: 'Build a MIDI lesson first.', challengePreview: 'Preview the phrase', challengeInput: 'Your turn', challengeRound: 'Round', challengeScore: 'Score', challengeCombo: 'Combo', challengeAccuracy: 'Accuracy', challengeHits: 'Hits', challengeMistakes: 'Mistakes', challengeComplete: 'Phrase complete', challengeMissed: 'Missed step', challengeStopped: 'Challenge stopped', challengeRoundDone: 'Round complete', challengeNoData: 'Build a MIDI lesson before starting the challenge.', challengeKeyboardHint: 'Gold keys are the current target. The preview is local to this desktop app; no keyboard input is sent to Windows.',
    },
    zh: {
      workspace: '工作区', inputDevice: '输入设备', panic: '紧急静音', liveInstrument: '实时乐器', calculatorKeys: '计算器按键',
      calculatorKeysLead: '所有声音都来自已连接的计算器。直接演奏，或按住 SHIFT 使用控制功能。', tone: '音色', range: '音域', activeNotes: '当前音符', recordState: '录音',
      pianoRoll: '钢琴卷帘', twoOctaveRange: '双八度可视音域', pianoView: '钢琴', calculatorView: '计算器', calculatorViewKicker: '计算器视图', calculatorPreview: '计算器按键', pressed: '按下', sounding: '发声中', rootNote: '根音', physicalMatrix: '实体矩阵', keyState: '按键状态',
      instrumentTelemetry: '乐器监视', scopeTitle: '示波器', scopeLead: '实时查看桌面合成器输出和计算器输入负载。', waveform: '波形', audioOutput: '音频输出', voices: '复音数',
      peak: '峰值', renderLoad: '绘制负载', scopeNotes: '说明', scopeNotesBody: '桌面引擎与计算器固件一样限制为 16 复音。示波器只用于诊断，不会改变声音路径。',
      capture: '录制', recorderTitle: '录音机', recorderLead: '录下计算器演奏，回放，并导出标准 Format 0 MIDI 文件。', takeDetails: '录音详情', events: '事件', duration: '时长',
      initialTone: '初始音色', initialTempo: '初始速度', savedFile: '保存文件', recentEvents: '最近事件', eventMonitor: '事件监视', noEvents: '还没有录音事件。',
      startRecording: '开始录音', stopRecording: '停止录音', playTake: '播放录音', stopPlayback: '停止播放', clearTake: '清除录音', saveMidi: '保存 MIDI',
      instrumentSetup: '乐器设置', settingsTitle: '设置', settingsLead: '这里的控制项对应计算器 MusicKeyboard 设置页面。', sound: '声音', volume: '音量', velocity: '力度',
      sustain: '延音', metronome: '节拍器', pitch: '音高', octave: '八度', root: '根音', transpose: '移调', layout: '布局', tempo: '速度', bpm: 'BPM',
      tempoHint: '录音过程中改变速度时，速度变化会写入 MIDI。',
      latencyCalibration: '延迟校准', latencyCalibrationTitle: '输入时序校准',
      latencyCalibrationLead: '听到节拍后，在每个节拍上按一次音符键。测得的时序偏移会作为录音和有时间基准功能的全局校正。',
      calibrationTempo: '校准速度', calibrationBeats: '测量节拍数', startCalibration: '开始校准', stopCalibration: '停止校准', resetCalibration: '清零',
      calibrationReady: '准备就绪。开始前请启用音频。', calibrationCountIn: '预备拍：请准备……', calibrationMeasuring: '每个节拍按一次音符键。', calibrationDone: '校准完成', calibrationTooFew: '有效按键太少，未修改设置。',
      calibrationNote: '正数表示按键事件晚于节拍到达，因此录音时间戳会提前相应毫秒。实时声音无法在实体按键到达前播放。',
      samples: '个样本', sampleSpread: '离散',
      changeTranspose: '移调', changeTempo: '改变 BPM', changeLayout: '改变布局', recordShortcut: '录音', navPlay: '演奏', navCreate: '创作', navLearn: '学习', navTools: '工具', keys: '按键', learn: '教学', scope: '示波器', recorder: '录音', settings: '设置', synth: '合成器',
      synthTitle: '模拟合成器', synthLead: '在计算器继续作为控制器的同时调整桌面音色。参数修改会作用于之后开始的音符。', synthMode: '引擎', factoryTone: '内置音色', customPatch: '自定义音色', synthPreset: '预设', applyPreset: '应用预设', resetSynth: '重置音色', presetApplied: '预设已应用', customReady: '自定义音色已启用', factoryReady: '内置音色已启用', oscillatorOne: '振荡器 1', oscillatorTwo: '振荡器 2', waveform: '波形', waveSine: '正弦', waveTriangle: '三角', waveSaw: '锯齿', waveSquare: '方波', oscLevel: '电平', detune: '微调', noise: '噪声', filter: '滤波器', cutoff: '截止频率', resonance: '共振', envelope: '振幅包络', attack: '起音', decay: '衰减', sustainLevel: '保持', release: '释音', modulation: '调制', lfoRate: 'LFO 速率', lfoDepth: '音高深度', synthHint: '自定义音色使用双振荡器、滤波噪声、低通滤波器、ADSR 和音高 LFO，并保持原有 16 复音上限。',
      groove: '律动', grooveTitle: '鼓机 + 琶音器', grooveLead: '让计算器继续作为控制器：鼓机提供节拍，琶音器把按住的音符变成伴奏型。', grooveTransport: '律动控制', groovePlay: '播放律动', grooveStop: '停止律动', grooveClear: '清空节奏', grooveRandom: '随机生成', grooveSteps: '步数', grooveSixteen: '16 步', grooveThirtyTwo: '32 步', grooveBpm: '律动 BPM', grooveResolution: '网格', grooveEighth: '1/8', grooveSixteenth: '1/16', drumMachine: '鼓机', drumMachineTitle: '步进节奏', drumMachineLead: '点击格子加入击打。节奏会保存在本机，并可与实时演奏同时运行。', pattern: '节奏型', kick: '底鼓', snare: '军鼓', hat: '踩镲', clap: '拍手', arpTitle: '琶音器', arpLead: '按住一个或多个计算器音符，桌面端会在按住期间循环演奏。', arpEnable: '琶音器', arpOn: '开启', arpOff: '关闭', arpPattern: '方向', arpUp: '上行', arpDown: '下行', arpUpDown: '上行 / 下行', arpRandom: '随机', arpRate: '速率', arpOctaves: '八度数', arpGate: '占空', arpHint: '琶音器沿用当前音色、移调、力度和 16 复音上限。关闭后恢复直接演奏。', grooveCalcKeys: '计算器按键', grooveCalcKeysHint: '鼓机和实时音符', grooveShift: 'SHIFT', grooveShiftHint: '原有音色和控制功能', grooveLocal: '律动', grooveLocalHint: '速度和节奏型只保存在桌面端', looper: '循环叠录', looperTitle: '分层循环', looperLead: '先录固定长度的基础层，再按循环逐层加入。循环播放不会把自己的声音再次录进去。', looperRecordBase: '录制基础层', looperPlay: '播放循环', looperRecordLayer: '录制新层', looperCancelArm: '取消待叠录', looperArmed: '下一循环已准备', looperRecording: '录制中', looperOverdub: '叠录中', looperStop: '停止循环', looperClear: '清空各层', looperBars: '小节数', looperBar: '小节', looperNoLayers: '还没有循环层。', looperLayer: '第', looperNotes: '个音符', looperMute: '静音', looperUnmute: '取消静音', looperRemove: '删除', looperPlaying: '循环播放中', grooveReady: '就绪', groovePlaying: '播放中',
      chordAssist: '和弦助手', chordAssistTitle: '和弦伴奏', chordAssistLead: '按住一个计算器音符，自动加入和弦和有节拍的低音。', chordAssistEnable: '和弦助手', chordType: '和弦类型', chordTriad: '大三和弦', chordMinor: '小三和弦', chordSeventh: '七和弦', chordSus4: '挂四和弦', chordBass: '自动低音', chordPulse: '低音脉冲', chordOn: '开启', chordOff: '关闭', chordHint: '助手沿用当前音色，并保持桌面端 16 复音上限。关闭后恢复直接音符。',
      sampler: '采样器', samplerTitle: '采样键盘', samplerLead: '导入一段音频，自动切成多个片段分配给计算器按键，适合快速触发人声、音效和鼓点。', samplerChoose: '选择音频', samplerNoFile: '还没有采样', samplerReady: '采样已就绪', samplerLoading: '正在解码采样', samplerError: '采样错误', samplerEnable: '采样模式', samplerSlices: '片段', samplerSliceCount: '片段数', samplerStart: '起点', samplerEnd: '终点', samplerSpeed: '速度', samplerKeys: '1 / 2 / 3 / 4 / 5 / 6 / 7 / 8 / 9 / 0 / . / SIN', samplerHint: '使用计算器数字键 1-9、0、. 和 SIN 触发。采样只保存在本次桌面会话内，不会修改计算器固件。',
      soundLab: '声音实验台', soundLabTitle: '波形 + 频谱', soundLabOpen: '展开查看', soundLabLead: '并排查看实时波形、轻量滤波轨迹和输出频谱。', soundLabRaw: '实时波形', soundLabFiltered: '滤波轨迹', soundLabSpectrum: '频谱', soundLabFilter: '轨迹平滑', soundLabGain: '显示增益', soundLabHint: '实验台只用于诊断，不增加另一条音频路径，也不改变合成器复音上限。',
      calculator: '计算器', chords: '和弦', chromatic: '半音', ready: '就绪', noTake: '没有录音', audioEnable: '启用音频', audioReady: '音频已就绪', refresh: '刷新',
      connect: '连接', disconnect: '断开', noPort: '未选择端口', waitingCalculator: '等待计算器', connected: '已连接', connecting: '连接中', disconnected: '已断开',
      serialError: '串口错误', serialHint: '通过 USB 串口连接计算器后开始。', audioHint: '演奏前请先启用音频。', deviceReady: '计算器已就绪', deviceDetail: 'SKB1 / 5 x 6 矩阵',
      packets: '个数据包', readyToRecord: '准备录音', recording: '录音中', playing: '播放中', takeInMemory: '录音在内存中', noTakeToPlay: '没有可播放的录音', full: '事件数量已达上限',
      saved: '已保存', canceled: '已取消保存', midiError: 'MIDI 保存失败', silence: '已静音', keyboard: '键盘', scopeAudio: 'Web Audio',
      scopeRate: '刷新率', fps: '帧/秒', convert: '转谱', midiTools: 'MIDI 工具', convertTitle: '计算器谱', convertLead: '把 MIDI 旋律转换为易读的 SCI-CALC 按键谱，按键名称与乐器保持一致。',
      calcSheet: '计算器谱', sourceMidi: '源 MIDI', noMidiSelected: '未选择 MIDI', chooseMidi: '选择 MIDI', chooseMidiHint: '选择 MIDI 文件开始。', rhythmGrid: '节奏网格',
      melodyTrack: '旋律轨道', extraKeyLabel: '额外键标记', fullTrack: '使用选中轨道全部音符', includeChords: '包含和弦建议', convertMidi: '转换 MIDI', report: '报告',
      sheetPreview: '按键谱预览', reportEmpty: '转换后的报告会显示在这里。', converterReady: '准备转换', converting: '正在转换 MIDI', convertDone: '转换完成',
      lessonNow: '教学 / 下一步', previous: '上一步', skip: '跳过', stopLesson: '停止教学', guidedPractice: '引导演奏', lessonTitle: 'MIDI 教学',
      lessonLead: '导入 MIDI，按自己的速度跟随高亮的计算器按键练习。', waitForYou: '等待弹奏', lessonMidi: '教学 MIDI', lessonChooseHint: '选择 MIDI 文件来生成教学步骤。',
      chordComplexity: '和弦复杂度', melodyOnly: '仅旋律', simpleChord: '简单和弦', fullChord: '完整和弦', melodyOnlyHint: '每一步只高亮一个旋律音。',
      simpleChordHint: '旋律加一个可以同时按下的伴奏音。', fullChordHint: '最多四个能够同时按下的音。', startLesson: '开始教学', restartLesson: '重新开始',
      lessonSteps: '步骤', recommendedRange: '推荐设置', nextStep: '下一步', mistakes: '次错误', lessonLoading: '正在分析 MIDI 并生成教学步骤……', lessonReady: '教学已就绪',
      lessonComplete: '教学完成', lessonError: '无法生成教学', lessonActive: '请跟随高亮按键', enterFirst: '先按住 ENTER', lessonStopped: '教学已暂停',
      challenge: '跟弹挑战', challengeTitle: '跟弹挑战', challengeLead: '桌面端先预览一小段旋律，再等待你用计算器复现。完成轮次后会逐步增加长度和难度。', challengeDifficulty: '难度', challengeEasy: '简单', challengeNormal: '普通', challengeHard: '困难', challengeStart: '开始挑战', challengeStop: '停止挑战', challengeRestart: '重新挑战', challengeReady: '请先生成 MIDI 教学。', challengePreview: '正在预览片段', challengeInput: '轮到你', challengeRound: '轮次', challengeScore: '分数', challengeCombo: '连击', challengeAccuracy: '命中率', challengeHits: '命中', challengeMistakes: '错误', challengeComplete: '片段完成', challengeMissed: '错过步骤', challengeStopped: '挑战已停止', challengeRoundDone: '本轮完成', challengeNoData: '开始挑战前请先生成 MIDI 教学。', challengeKeyboardHint: '金色按键是当前目标。预览和挑战只在桌面端运行，不会向 Windows 发送键盘输入。',
    },
  };

  const SYNTH_DEFAULTS = {
    mode: 'factory', preset: 'init', osc1Wave: 'sawtooth', osc1Level: .58, osc2Wave: 'square', osc2Level: .24, osc2Detune: 7, noiseLevel: 0,
    cutoff: 2600, resonance: .18, attack: .01, decay: .28, sustainLevel: .68, release: .42, lfoRate: 5.2, lfoDepth: 3,
  };
  const SYNTH_PRESETS = {
    init: { osc1Wave: 'sawtooth', osc1Level: .58, osc2Wave: 'square', osc2Level: .24, osc2Detune: 7, noiseLevel: 0, cutoff: 2600, resonance: .18, attack: .01, decay: .28, sustainLevel: .68, release: .42, lfoRate: 5.2, lfoDepth: 3 },
    warm: { osc1Wave: 'sawtooth', osc1Level: .48, osc2Wave: 'triangle', osc2Level: .34, osc2Detune: -9, noiseLevel: .02, cutoff: 1450, resonance: .12, attack: .22, decay: .85, sustainLevel: .62, release: 1.35, lfoRate: .32, lfoDepth: 7 },
    bright: { osc1Wave: 'sawtooth', osc1Level: .62, osc2Wave: 'square', osc2Level: .2, osc2Detune: 12, noiseLevel: .015, cutoff: 5200, resonance: .3, attack: .008, decay: .2, sustainLevel: .55, release: .28, lfoRate: 5.4, lfoDepth: 14 },
    bass: { osc1Wave: 'triangle', osc1Level: .7, osc2Wave: 'sine', osc2Level: .3, osc2Detune: -12, noiseLevel: .01, cutoff: 720, resonance: .42, attack: .012, decay: .32, sustainLevel: .58, release: .34, lfoRate: 2.1, lfoDepth: 2 },
    bell: { osc1Wave: 'sine', osc1Level: .58, osc2Wave: 'sine', osc2Level: .25, osc2Detune: 1200, noiseLevel: 0, cutoff: 6800, resonance: .08, attack: .002, decay: 1.8, sustainLevel: .05, release: 1.1, lfoRate: 6.2, lfoDepth: 2 },
  };
  function loadSynthSettings() {
    const waves = ['sine', 'triangle', 'sawtooth', 'square'];
    try {
      const saved = JSON.parse(localStorage.getItem('musicKeyboardSynth') || '{}');
      const settings = { ...SYNTH_DEFAULTS, ...saved };
      settings.mode = settings.mode === 'custom' ? 'custom' : 'factory';
      settings.preset = SYNTH_PRESETS[settings.preset] ? settings.preset : 'init';
      settings.osc1Wave = waves.includes(settings.osc1Wave) ? settings.osc1Wave : SYNTH_DEFAULTS.osc1Wave;
      settings.osc2Wave = waves.includes(settings.osc2Wave) ? settings.osc2Wave : SYNTH_DEFAULTS.osc2Wave;
      settings.osc1Level = clamp(Number(settings.osc1Level) || 0, 0, 1);
      settings.osc2Level = clamp(Number(settings.osc2Level) || 0, 0, 1);
      settings.osc2Detune = clamp(Number(settings.osc2Detune) || 0, -1200, 1200);
      settings.noiseLevel = clamp(Number(settings.noiseLevel) || 0, 0, .35);
      settings.cutoff = clamp(Number(settings.cutoff) || SYNTH_DEFAULTS.cutoff, 120, 12000);
      settings.resonance = clamp(Number(settings.resonance) || 0, 0, 1);
      settings.attack = clamp(Number(settings.attack) || SYNTH_DEFAULTS.attack, .001, 2);
      settings.decay = clamp(Number(settings.decay) || SYNTH_DEFAULTS.decay, .001, 3);
      settings.sustainLevel = clamp(Number(settings.sustainLevel) || 0, 0, 1);
      settings.release = clamp(Number(settings.release) || SYNTH_DEFAULTS.release, .08, 3);
      settings.lfoRate = clamp(Number(settings.lfoRate) || SYNTH_DEFAULTS.lfoRate, .05, 12);
      settings.lfoDepth = clamp(Number(settings.lfoDepth) || 0, 0, 60);
      return settings;
    } catch (_error) {
      return { ...SYNTH_DEFAULTS };
    }
  }

  const ARP_DEFAULTS = { pattern: 'up', rate: 8, octaves: 1, gate: .72 };

  function loadGrooveSettings() {
    try {
      return sanitizeGrooveSettings(JSON.parse(localStorage.getItem('musicKeyboardGroove') || '{}'));
    } catch (_error) {
      return sanitizeGrooveSettings({});
    }
  }

  function loadArpSettings() {
    try {
      const saved = JSON.parse(localStorage.getItem('musicKeyboardArp') || '{}');
      return {
        pattern: ARP_PATTERNS.includes(saved.pattern) ? saved.pattern : ARP_DEFAULTS.pattern,
        rate: ARP_RATES.includes(Number(saved.rate)) ? Number(saved.rate) : ARP_DEFAULTS.rate,
        octaves: clamp(Math.round(Number(saved.octaves) || ARP_DEFAULTS.octaves), 1, 3),
        gate: clamp(Number(saved.gate) || ARP_DEFAULTS.gate, .35, 1),
      };
    } catch (_error) {
      return { ...ARP_DEFAULTS };
    }
  }

  function loadLooperBars() {
    try {
      const saved = Number(localStorage.getItem('musicKeyboardLooperBars') || 4);
      return clamp(Math.round(saved) || 4, 1, 8);
    } catch (_error) {
      return 4;
    }
  }

  function loadChordSettings() {
    try {
      const saved = JSON.parse(localStorage.getItem('musicKeyboardChordAssist') || '{}');
      return {
        type: normalizeChordType(saved.type),
        bass: saved.bass !== false,
        pulse: saved.pulse !== false,
      };
    } catch (_error) {
      return { type: 'triad', bass: true, pulse: true };
    }
  }

  function loadSamplerSettings() {
    try {
      const saved = JSON.parse(localStorage.getItem('musicKeyboardSampler') || '{}');
      return {
        sliceCount: clamp(Math.round(Number(saved.sliceCount) || 6), 2, 12),
        start: clamp(Number(saved.start) || 0, 0, .98),
        end: clamp(Number(saved.end) || 1, .02, 1),
        speed: clamp(Number(saved.speed) || 1, .5, 2),
        volume: clamp(Number(saved.volume) || .8, .1, 1),
      };
    } catch (_error) {
      return { sliceCount: 6, start: 0, end: 1, speed: 1, volume: .8 };
    }
  }

  const state = {
    language: localStorage.getItem('musicKeyboardLanguage') || 'en',
    page: 'keys',
    settings: { tone: 0, octave: 4, root: 0, transpose: 0, velocity: 100, volume: 7, bpm: 120, sustain: false, metronome: false, layout: 'calculator', latencyMs: clamp(Number(localStorage.getItem('musicKeyboardLatencyMs') || 0), -250, 250) },
    synth: loadSynthSettings(),
    groove: { ...loadGrooveSettings(), playing: false, currentStep: -1, nextStepAt: 0 },
    arp: { ...loadArpSettings(), enabled: false, held: new Map(), currentNote: null, currentOffAt: 0, nextAt: 0, stepIndex: 0 },
    chordAssist: { ...loadChordSettings(), enabled: false, held: new Map(), nextAt: 0 },
    sampler: { ...loadSamplerSettings(), enabled: false, loading: false, name: '', buffer: null, slices: [], sources: new Set() },
    soundLab: { smooth: 3, gain: 1.5 },
    looper: {
      bars: loadLooperBars(), mode: 'idle', playing: false, loopDurationMs: 0, bpm: 120,
      recordStartAt: 0, recordAnchorAt: 0, recordEvents: [], capturedHeld: new Map(),
      layers: [], nextLayerId: 1, cycleStartAt: 0, playIndex: 0, schedule: [], activeNotes: new Map(), revision: 0,
    },
    pressed: new Set(),
    bindings: new Map(),
    packets: 0,
    dropped: 0,
    device: null,
    serialStatus: 'disconnected',
    notice: '',
    take: null,
    recording: null,
    playback: null,
    saving: false,
    saveIndex: Number(localStorage.getItem('musicKeyboardSaveIndex') || 0),
    frameCost: 0,
    scopeDisplayPeak: 0,
    scopeRate: 60,
    lastScopeDraw: 0,
    visualMode: localStorage.getItem('musicKeyboardVisualMode') === 'calculator' ? 'calculator' : 'piano',
    converter: { path: '', name: '', busy: false },
    lesson: {
      path: '', name: '', complexity: 'melody', loading: false, data: null, error: '',
      index: 0, active: false, completed: false, mistakes: 0, matched: new Set(),
    },
    challenge: {
      active: false, phase: 'idle', difficulty: 'normal', data: null, phrase: [], index: 0, round: 1,
      previewIndex: 0, nextAt: 0, deadlineAt: 0, advanceAt: 0, bpm: 120, previewNotes: new Set(), matched: new Set(),
      score: 0, combo: 0, bestCombo: 0, hits: 0, mistakes: 0,
    },
    calibration: {
      active: false, phase: 'idle', bpm: 90, measuredBeats: 12, leadInBeats: 4,
      beatIndex: 0, nextBeatAt: 0, timer: null, beats: [], samples: [], missed: 0,
      result: null, pulse: false,
    },
  };

  function t(key) { return (I18N[state.language] && I18N[state.language][key]) || I18N.en[key] || key; }
  function qs(selector) { return document.querySelector(selector); }
  function qsa(selector) { return [...document.querySelectorAll(selector)]; }
  function formatTime(milliseconds) {
    const total = Math.max(0, milliseconds || 0);
    const minutes = Math.floor(total / 60000);
    const seconds = Math.floor((total % 60000) / 1000);
    const tenths = Math.floor((total % 1000) / 100);
    return `${String(minutes).padStart(2, '0')}:${String(seconds).padStart(2, '0')}.${tenths}`;
  }
  function signed(value) { return `${value >= 0 ? '+' : ''}${value}`; }
  function noteName(note) {
    const names = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'];
    return `${names[((note % 12) + 12) % 12]}${Math.floor(note / 12) - 1}`;
  }
  function toneLabel(index) {
    if (state.language === 'zh') return ['钢琴', '电钢琴', '风琴', '温暖铺底', '主音', '钟声'][index];
    return TONE_NAMES[index];
  }
  function layoutLabel(layout) {
    const key = layout === 'calculator' ? 'calculator' : layout;
    return t(key);
  }
  function setNotice(message) {
    state.notice = message;
    syncUI();
  }

  class DesktopSynth {
    constructor() {
      this.context = null;
      this.master = null;
      this.analyser = null;
      this.waveform = null;
      this.frequency = null;
      this.voices = [];
      this.nextId = 1;
      this.sustain = false;
      this.noiseBuffer = null;
    }

    async enable() {
      if (!this.context) {
        const AudioContext = window.AudioContext || window.webkitAudioContext;
        if (!AudioContext) throw new Error('Web Audio is unavailable in this Electron runtime');
        this.context = new AudioContext();
        this.master = this.context.createGain();
        this.analyser = this.context.createAnalyser();
        this.analyser.fftSize = 1024;
        this.analyser.smoothingTimeConstant = .28;
        this.master.connect(this.analyser);
        this.analyser.connect(this.context.destination);
        this.waveform = new Uint8Array(this.analyser.fftSize);
        this.frequency = new Uint8Array(this.analyser.frequencyBinCount);
      }
      if (this.context.state !== 'running') await this.context.resume();
      this.setVolume(state.settings.volume);
    }

    setVolume(value) {
      if (!this.master || !this.context) return;
      const gain = clamp(Number(value) / 10, 0, 1) * 0.28;
      this.master.gain.setTargetAtTime(gain, this.context.currentTime, 0.025);
    }

    patchForCustom(note) {
      const settings = state.synth;
      const cutoff = clamp(Number(settings.cutoff) || 2600, 120, 18000);
      const resonance = clamp(Number(settings.resonance) || 0, 0, 1);
      const layers = [];
      if (Number(settings.osc1Level) > 0) layers.push([settings.osc1Wave, 1, clamp(Number(settings.osc1Level), 0, 1), 0]);
      if (Number(settings.osc2Level) > 0) layers.push([settings.osc2Wave, 1, clamp(Number(settings.osc2Level), 0, 1), Number(settings.osc2Detune) || 0]);
      if (!layers.length) layers.push(['sine', 1, .05, 0]);
      return {
        kind: 'custom', layers, filter: { start: Math.max(80, cutoff * (.45 + resonance * .2)), frequency: cutoff, q: .35 + resonance * 17 },
        attack: clamp(Number(settings.attack) || .001, .001, 4), decay: clamp(Number(settings.decay) || .001, .001, 6), sustain: clamp(Number(settings.sustainLevel) || 0, .001, 1), release: clamp(Number(settings.release) || .08, .08, 6), level: .2,
        vibrato: Number(settings.lfoDepth) > 0 ? { rate: clamp(Number(settings.lfoRate) || 0, .05, 20), cents: clamp(Number(settings.lfoDepth), 0, 80) } : null,
        noise: Number(settings.noiseLevel) > 0 ? { level: clamp(Number(settings.noiseLevel), 0, .35), frequency: Math.min(12000, cutoff * 1.35) } : null,
      };
    }

    patchForTone(tone, note) {
      if (state.synth.mode === 'custom') return this.patchForCustom(note);
      const pianoDecay = clamp(1.65 - Math.max(0, note - 48) * .008, .72, 1.65);
      return [
        {
          kind: 'piano', layers: [['triangle', 1, .72], ['sine', 2.01, .22], ['sine', 3.01, .08], ['sine', 4.08, .025]],
          filter: { frequency: 5200, q: .7 }, attack: .004, decay: pianoDecay, sustain: .035, release: .48, level: .23,
          hammer: { frequency: 2200, q: 2.5, level: .08, duration: .075 },
        },
        {
          kind: 'electric-piano', layers: [['sine', 1, .18], ['sine', 3.99, .055]],
          fm: { ratio: 2.01, index: 2.2, endIndex: .35 }, filter: { frequency: 4800, q: 1.1 },
          attack: .003, decay: .78, sustain: .16, release: .36, level: .22,
        },
        {
          kind: 'organ', layers: [['sine', .5, .10], ['sine', 1, .72], ['sine', 2, .3], ['sine', 3, .17], ['sine', 4, .07]],
          filter: { frequency: 6200, q: .4 }, attack: .022, decay: .16, sustain: .78, release: .52, level: .12,
          vibrato: { rate: 5.2, cents: 3.5 },
        },
        {
          kind: 'warm-pad', layers: [['sawtooth', .997, .23], ['sawtooth', 1.003, .23], ['triangle', .5, .15], ['sine', 1, .1]],
          filter: { start: 620, frequency: 2400, q: .65 }, attack: .24, decay: .95, sustain: .68, release: 1.55, level: .11,
          vibrato: { rate: .32, cents: 7 },
        },
        {
          kind: 'lead', layers: [['sawtooth', 1, .44], ['square', 2, .14], ['sine', 1, .12]],
          filter: { start: 950, frequency: 2700, q: 1.7 }, attack: .014, decay: .28, sustain: .62, release: .34, level: .13,
          vibrato: { rate: 5.4, cents: 16 },
        },
        {
          kind: 'bell', layers: [['sine', 3.99, .18], ['sine', 5.42, .07]],
          fm: { ratio: 2.72, index: 3.1, endIndex: .08 }, filter: { frequency: 7200, q: .55 },
          attack: .002, decay: 2.05, sustain: .008, release: .7, level: .18, oneShot: true, tail: 2.35,
        },
      ][tone] || null;
    }

    ensureNoiseBuffer() {
      if (this.noiseBuffer || !this.context) return;
      const length = Math.floor(this.context.sampleRate * .12);
      this.noiseBuffer = this.context.createBuffer(1, length, this.context.sampleRate);
      const data = this.noiseBuffer.getChannelData(0);
      for (let index = 0; index < data.length; index += 1) data[index] = (Math.random() * 2 - 1) * (1 - index / data.length);
    }

    noteOn(note, velocity, tone) {
      if (!this.context || this.context.state !== 'running') return;
      const existing = this.voices.find((voice) => voice.note === note && voice.held && !voice.ended);
      if (existing && !existing.oneShot) {
        existing.overlap += 1;
        existing.velocity = velocity;
        return;
      }
      if (this.voices.filter((voice) => !voice.ended).length >= MAX_VOICES) {
        const victim = this.voices.find((voice) => !voice.held && !voice.ended) || this.voices[0];
        if (victim) this.releaseVoice(victim, true);
      }
      const patch = this.patchForTone(tone, note);
      if (!patch) return;
      const now = this.context.currentTime;
      const frequency = 440 * Math.pow(2, (note - 69) / 12);
      const gain = this.context.createGain();
      const amp = Math.max(.06, Math.min(1, velocity / 127));
      const peak = amp * patch.level;
      const sustainGain = Math.max(.0002, peak * patch.sustain);
      gain.gain.setValueAtTime(.0001, now);
      gain.gain.linearRampToValueAtTime(peak, now + patch.attack);
      gain.gain.exponentialRampToValueAtTime(sustainGain, now + patch.attack + patch.decay);
      let output = gain;
      let filter = null;
      if (patch.filter) {
        filter = this.context.createBiquadFilter();
        filter.type = 'lowpass';
        filter.frequency.setValueAtTime(patch.filter.start || patch.filter.frequency, now);
        if (patch.filter.start) filter.frequency.exponentialRampToValueAtTime(patch.filter.frequency, now + Math.max(.04, patch.attack + patch.decay));
        filter.Q.value = patch.filter.q || .7;
        filter.connect(gain);
        output = filter;
      }
      gain.connect(this.master);
      const nodes = [];
      const pitchNodes = [];
      for (const [type, ratio, level, detune = 0] of patch.layers || []) {
        const oscillator = this.context.createOscillator();
        const oscillatorGain = this.context.createGain();
        oscillator.type = type;
        oscillator.frequency.value = frequency * ratio;
        oscillator.detune.value = detune;
        oscillatorGain.gain.value = level;
        oscillator.connect(oscillatorGain);
        oscillatorGain.connect(output);
        oscillator.start(now);
        nodes.push(oscillator);
        pitchNodes.push(oscillator);
      }
      if (patch.fm) {
        const carrier = this.context.createOscillator();
        const modulator = this.context.createOscillator();
        const modGain = this.context.createGain();
        carrier.type = 'sine';
        modulator.type = 'sine';
        carrier.frequency.value = frequency;
        modulator.frequency.value = frequency * patch.fm.ratio;
        modGain.gain.setValueAtTime(frequency * patch.fm.index, now);
        modGain.gain.exponentialRampToValueAtTime(Math.max(1, frequency * patch.fm.endIndex), now + patch.attack + patch.decay);
        modulator.connect(modGain);
        modGain.connect(carrier.frequency);
        carrier.connect(output);
        carrier.start(now);
        modulator.start(now);
        nodes.push(carrier, modulator);
        pitchNodes.push(carrier);
      }
      if (patch.vibrato && pitchNodes.length) {
        const lfo = this.context.createOscillator();
        const lfoGain = this.context.createGain();
        lfo.type = 'sine';
        lfo.frequency.value = patch.vibrato.rate;
        lfoGain.gain.setValueAtTime(0, now);
        lfoGain.gain.linearRampToValueAtTime(patch.vibrato.cents, now + Math.min(.2, patch.attack));
        lfo.connect(lfoGain);
        pitchNodes.forEach((oscillator) => lfoGain.connect(oscillator.detune));
        lfo.start(now);
        nodes.push(lfo);
      }
      if (patch.hammer) {
        this.ensureNoiseBuffer();
        if (this.noiseBuffer) {
          const source = this.context.createBufferSource();
          const hammerFilter = this.context.createBiquadFilter();
          const hammerGain = this.context.createGain();
          source.buffer = this.noiseBuffer;
          hammerFilter.type = 'bandpass';
          hammerFilter.frequency.value = patch.hammer.frequency;
          hammerFilter.Q.value = patch.hammer.q;
          hammerGain.gain.setValueAtTime(.0001, now);
          hammerGain.gain.exponentialRampToValueAtTime(amp * patch.hammer.level, now + .002);
          hammerGain.gain.exponentialRampToValueAtTime(.0001, now + patch.hammer.duration);
          source.connect(hammerFilter);
          hammerFilter.connect(hammerGain);
          hammerGain.connect(output);
          source.start(now);
          source.stop(now + patch.hammer.duration + .01);
          nodes.push(source);
        }
      }
      if (patch.noise?.level > 0) {
        this.ensureNoiseBuffer();
        if (this.noiseBuffer) {
          const source = this.context.createBufferSource();
          const noiseFilter = this.context.createBiquadFilter();
          const noiseGain = this.context.createGain();
          source.buffer = this.noiseBuffer;
          source.loop = true;
          noiseFilter.type = 'lowpass';
          noiseFilter.frequency.value = patch.noise.frequency || 6000;
          noiseFilter.Q.value = .35;
          noiseGain.gain.value = patch.noise.level;
          source.connect(noiseFilter).connect(noiseGain).connect(output);
          source.start(now);
          nodes.push(source);
        }
      }
      const envelope = {
        startTime: now,
        floor: .0001,
        peak,
        sustainGain,
        attack: patch.attack,
        decay: patch.decay,
        oneShotTail: patch.oneShot ? (patch.tail || 2.2) : 0,
      };
      const voice = { id: this.nextId++, note, nodes, gain, filter, envelope, release: patch.release, held: true, sustained: false, overlap: 1, oneShot: Boolean(patch.oneShot), ended: false, createdAt: performance.now(), velocity };
      this.voices.push(voice);
      if (voice.oneShot) {
        const endAt = now + patch.attack + patch.decay + (patch.tail || 2.2);
        gain.gain.exponentialRampToValueAtTime(.0001, endAt);
        nodes.forEach((node) => node.stop(endAt + .05));
        window.setTimeout(() => this.removeVoice(voice), (endAt - now + .2) * 1000);
      }
    }

    noteOff(note) {
      const voice = [...this.voices].reverse().find((candidate) => candidate.note === note && candidate.held && !candidate.ended);
      if (!voice) return;
      if (voice.overlap > 1) {
        voice.overlap -= 1;
        return;
      }
      voice.held = false;
      if (this.sustain && !voice.oneShot) {
        voice.sustained = true;
        return;
      }
      this.releaseVoice(voice, false);
    }

    releaseVoice(voice, immediate) {
      if (!voice || voice.ended) return;
      voice.ended = true;
      const now = this.context ? this.context.currentTime : 0;
      if (this.context && voice.gain) {
        holdEnvelopeAtTime(voice.gain.gain, voice.envelope, now);
        if (immediate) {
          voice.gain.gain.setValueAtTime(.0001, now);
          voice.nodes.forEach((node) => { try { node.stop(now + .015); } catch (_) {} });
        } else {
          const release = Math.max(.08, voice.release || .35);
          const stopAt = now + Math.max(.45, Math.min(2.6, release * 3));
          voice.gain.gain.setTargetAtTime(.0001, now, Math.min(.55, release / 2.5));
          voice.nodes.forEach((node) => { try { node.stop(stopAt); } catch (_) {} });
        }
      }
      window.setTimeout(() => this.removeVoice(voice), immediate ? 40 : 1000);
    }

    removeVoice(voice) {
      const index = this.voices.indexOf(voice);
      if (index >= 0) this.voices.splice(index, 1);
    }

    setSustain(enabled) {
      this.sustain = Boolean(enabled);
      if (!this.sustain) {
        this.voices.filter((voice) => voice.sustained && !voice.held).forEach((voice) => {
          voice.sustained = false;
          this.releaseVoice(voice, false);
        });
      }
    }

    allNotesOff(immediate = false) {
      this.voices.filter((voice) => !voice.ended).forEach((voice) => this.releaseVoice(voice, immediate));
    }

    triggerDrum(kind, velocity = 1) {
      if (!this.context || this.context.state !== 'running' || !this.master) return;
      const now = this.context.currentTime;
      const amp = clamp(Number(velocity) || 1, .1, 1);
      const connectEnvelope = (node, gain, peak, duration) => {
        node.connect(gain);
        gain.connect(this.master);
        gain.gain.setValueAtTime(.0001, now);
        gain.gain.exponentialRampToValueAtTime(Math.max(.0002, peak * amp), now + .0015);
        gain.gain.exponentialRampToValueAtTime(.0001, now + duration);
      };
      if (kind === 'kick') {
        const oscillator = this.context.createOscillator();
        const gain = this.context.createGain();
        oscillator.type = 'sine';
        oscillator.frequency.setValueAtTime(150, now);
        oscillator.frequency.exponentialRampToValueAtTime(46, now + .16);
        connectEnvelope(oscillator, gain, .78, .22);
        oscillator.start(now);
        oscillator.stop(now + .24);
        return;
      }
      this.ensureNoiseBuffer();
      if (!this.noiseBuffer) return;
      const source = this.context.createBufferSource();
      const filter = this.context.createBiquadFilter();
      const gain = this.context.createGain();
      source.buffer = this.noiseBuffer;
      filter.type = kind === 'hat' ? 'highpass' : 'bandpass';
      filter.frequency.value = kind === 'hat' ? 5600 : kind === 'clap' ? 1450 : 1900;
      filter.Q.value = kind === 'hat' ? .6 : .8;
      const duration = kind === 'hat' ? .055 : kind === 'clap' ? .12 : .14;
      source.connect(filter);
      connectEnvelope(filter, gain, kind === 'hat' ? .25 : kind === 'clap' ? .42 : .58, duration);
      source.start(now);
      source.stop(now + duration + .015);
      if (kind === 'snare') {
        const body = this.context.createOscillator();
        const bodyGain = this.context.createGain();
        body.type = 'triangle';
        body.frequency.setValueAtTime(190, now);
        body.frequency.exponentialRampToValueAtTime(110, now + .08);
        connectEnvelope(body, bodyGain, .22, .1);
        body.start(now);
        body.stop(now + .12);
      }
    }

    clickAt(performanceTimeMs, accent = false) {
      if (!this.context || this.context.state !== 'running') return;
      const now = this.context.currentTime;
      const scheduled = Math.max(now + .003, now + (performanceTimeMs - performance.now()) / 1000);
      const oscillator = this.context.createOscillator();
      const gain = this.context.createGain();
      oscillator.type = 'square';
      oscillator.frequency.value = accent ? 1500 : 1000;
      gain.gain.setValueAtTime(.0001, scheduled);
      gain.gain.exponentialRampToValueAtTime(accent ? .055 : .035, scheduled + .004);
      gain.gain.exponentialRampToValueAtTime(.0001, scheduled + .075);
      oscillator.connect(gain);
      gain.connect(this.master);
      oscillator.start(scheduled);
      oscillator.stop(scheduled + .09);
    }

    click(accent = false) {
      this.clickAt(performance.now(), accent);
    }

    peak() {
      if (!this.analyser || !this.waveform) return 0;
      this.analyser.getByteTimeDomainData(this.waveform);
      let peak = 0;
      for (const sample of this.waveform) peak = Math.max(peak, Math.abs(sample - 128) / 128);
      return peak;
    }
  }

  const synth = new DesktopSynth();

  function applyTranslations() {
    document.documentElement.lang = state.language;
    qsa('[data-i18n]').forEach((element) => { element.textContent = t(element.dataset.i18n); });
    qs('#languageSelect').value = state.language;
    qs('#portSelect').options[0].textContent = t('noPort');
    qs('#refreshPorts').textContent = t('refresh');
    qs('#connectButton').textContent = state.serialStatus === 'connected' ? t('disconnect') : t('connect');
    qs('#audioButton').textContent = synth.context && synth.context.state === 'running' ? t('audioReady') : t('audioEnable');
    renderToneOptions();
    renderLayoutOptions();
    renderPhysicalGrid();
    renderCalculatorVisual();
    renderDrumGrid();
    looperLayerRevision = -1;
    renderLooperLayers();
    samplerPadRevision = -1;
    renderSamplerPads();
    syncUI();
  }

  function renderToneOptions() {
    const select = qs('#toneSelect');
    const current = state.settings.tone;
    select.innerHTML = TONE_NAMES.map((_, index) => `<option value="${index}">${toneLabel(index)}</option>`).join('');
    select.value = String(current);
  }

  function renderLayoutOptions() {
    const select = qs('#layoutSelect');
    [...select.options].forEach((option) => { option.textContent = layoutLabel(option.value); });
    select.value = state.settings.layout;
  }

  function visibleKeyName(id) {
    if (id === 23) return 'EXTRA';
    return KEY_NAMES[id];
  }

  function renderPhysicalGrid() {
    const grid = qs('#physicalGrid');
    if (grid.children.length !== KEY_NAMES.length) {
      grid.innerHTML = KEY_NAMES.map((_, id) => `<div class="physical-key" data-key-id="${id}"><span>${visibleKeyName(id)}</span><span class="key-index">${String(id).padStart(2, '0')}</span></div>`).join('');
    }
    qsa('.physical-key').forEach((element) => { element.querySelector('span').textContent = visibleKeyName(Number(element.dataset.keyId)); });
  }

  function calculatorKeyClass(id) {
    if ([1, 2, 3, 9, 15].includes(id)) return 'calc-operator';
    if ([6, 7, 8, 12, 13, 14, 18, 19, 20, 25, 26].includes(id)) return 'calc-number';
    if ([21, 24, 27, 28, 29].includes(id)) return 'calc-control';
    return 'calc-function';
  }

  function renderCalculatorVisual() {
    const grid = qs('#calculatorKeyGrid');
    if (grid.children.length !== KEY_NAMES.length) {
      grid.innerHTML = KEY_NAMES.map((_, id) => `<div class="calculator-key ${calculatorKeyClass(id)}" data-key-id="${id}"><span class="calc-key-label">${visibleKeyName(id)}</span><small class="calc-key-code">${String(id).padStart(2, '0')}</small></div>`).join('');
    }
    qsa('.calc-key-label').forEach((element) => { element.textContent = visibleKeyName(Number(element.parentElement.dataset.keyId)); });
  }

  function setVisualMode(mode) {
    state.visualMode = mode === 'calculator' ? 'calculator' : 'piano';
    localStorage.setItem('musicKeyboardVisualMode', state.visualMode);
    syncUI();
    if (state.visualMode === 'piano') drawPiano();
  }

  function setPage(page) {
    state.page = page;
    qsa('.page-tab').forEach((button) => button.classList.toggle('active', button.dataset.page === page));
    qsa('.page').forEach((section) => section.classList.toggle('active', section.id === `page-${page}`));
    if (page === 'keys' && state.visualMode === 'piano') drawPiano();
  }

  function currentLessonStep() {
    return state.lesson.data?.steps?.[state.lesson.index] || null;
  }

  function lessonKeyNames(step = currentLessonStep()) {
    if (!step) return [];
    return step.key_ids.map((id) => visibleKeyName(id));
  }

  function lessonComplexityHint() {
    if (state.lesson.complexity === 'full') return t('fullChordHint');
    if (state.lesson.complexity === 'simple') return t('simpleChordHint');
    return t('melodyOnlyHint');
  }

  async function chooseLessonMidi() {
    const result = await window.musicKeyboard.chooseMidi();
    if (!result?.ok || result.canceled) return;
    state.lesson.path = result.path;
    state.lesson.name = result.name;
    qs('#lessonFileName').textContent = result.name;
    qs('#lessonFilePath').textContent = result.path;
    await buildLesson();
  }

  async function buildLesson() {
    const lesson = state.lesson;
    if (!lesson.path || lesson.loading) return;
    if (state.challenge.active) stopChallenge(false);
    if (lessonAdvanceTimer) window.clearTimeout(lessonAdvanceTimer);
    lessonAdvanceTimer = null;
    lesson.loading = true;
    lesson.active = false;
    lesson.completed = false;
    lesson.data = null;
    lesson.error = '';
    lesson.index = 0;
    lesson.mistakes = 0;
    lesson.matched.clear();
    setNotice(t('lessonLoading'));
    syncUI();
    try {
      const result = await window.musicKeyboard.buildLesson({ path: lesson.path, complexity: lesson.complexity });
      if (!result?.ok || !result.lesson?.steps?.length) throw new Error(result?.error || t('lessonError'));
      lesson.data = result.lesson;
      lesson.name = result.lesson.file_name || lesson.name;
      setNotice(`${t('lessonReady')}: ${result.lesson.step_count} ${t('lessonSteps').toLowerCase()}`);
    } catch (error) {
      lesson.data = null;
      lesson.error = error.message || String(error);
      setNotice(`${t('lessonError')}: ${lesson.error}`);
    } finally {
      lesson.loading = false;
      syncUI();
    }
  }

  function applyLessonSettings() {
    const data = state.lesson.data;
    if (!data) return;
    state.settings.root = clamp(Number(data.root) || 0, 0, 11);
    state.settings.octave = clamp(Number(data.octave) || 4, 1, 7);
    state.settings.transpose = clamp(Number(data.transpose) || 0, -12, 12);
    state.settings.layout = 'calculator';
  }

  function startLesson(restart = false) {
    if (!state.lesson.data?.steps?.length) return;
    if (state.challenge.active) stopChallenge(false);
    if (lessonAdvanceTimer) window.clearTimeout(lessonAdvanceTimer);
    lessonAdvanceTimer = null;
    if (restart || state.lesson.completed || state.lesson.index >= state.lesson.data.steps.length) state.lesson.index = 0;
    state.lesson.active = true;
    state.lesson.completed = false;
    state.lesson.matched.clear();
    if (restart) state.lesson.mistakes = 0;
    applyLessonSettings();
    setPage('keys');
    setNotice(t('lessonActive'));
    syncUI();
  }

  function stopLesson() {
    if (lessonAdvanceTimer) window.clearTimeout(lessonAdvanceTimer);
    lessonAdvanceTimer = null;
    state.lesson.active = false;
    state.lesson.matched.clear();
    setPage('learn');
    setNotice(t('lessonStopped'));
    syncUI();
  }

  let lessonAdvanceTimer = null;
  function moveLesson(step) {
    const total = state.lesson.data?.steps?.length || 0;
    if (!total) return;
    if (lessonAdvanceTimer) window.clearTimeout(lessonAdvanceTimer);
    lessonAdvanceTimer = null;
    state.lesson.index = clamp(state.lesson.index + step, 0, total - 1);
    state.lesson.completed = false;
    state.lesson.matched.clear();
    syncUI();
  }

  function handleLessonKeyPress(keyId) {
    const lesson = state.lesson;
    const step = currentLessonStep();
    if (!lesson.active || !step || lessonAdvanceTimer) return;
    if (step.key_ids.includes(keyId)) lesson.matched.add(keyId);
    else lesson.mistakes += 1;
    syncUI();
    if (!step.key_ids.every((id) => lesson.matched.has(id))) return;
    lessonAdvanceTimer = window.setTimeout(() => {
      lessonAdvanceTimer = null;
      lesson.matched.clear();
      if (lesson.index + 1 >= lesson.data.steps.length) {
        lesson.active = false;
        lesson.completed = true;
        setPage('learn');
        setNotice(t('lessonComplete'));
      } else {
        lesson.index += 1;
        syncUI();
      }
    }, 180);
  }

  function currentChallengeStep() {
    return state.challenge.phrase?.[state.challenge.index] || null;
  }

  function challengePhaseLabel() {
    const challenge = state.challenge;
    if (challenge.phase === 'preview') return t('challengePreview');
    if (challenge.phase === 'input') return t('challengeInput');
    if (challenge.phase === 'round') return t('challengeRoundDone');
    return challenge.data ? t('challengeReady') : t('challengeNoData');
  }

  function challengeAccuracy() {
    const total = state.challenge.hits + state.challenge.mistakes;
    return total ? Math.round(state.challenge.hits / total * 100) : 0;
  }

  function releaseChallengePreview() {
    const challenge = state.challenge;
    challenge.previewNotes.forEach((note) => synth.noteOff(note));
    challenge.previewNotes.clear();
  }

  function previewChallengeStep(step) {
    releaseChallengePreview();
    (step?.notes || []).forEach((item) => {
      const note = Number(item.pitch);
      if (!Number.isFinite(note)) return;
      synth.noteOn(note, state.settings.velocity, state.settings.tone);
      state.challenge.previewNotes.add(note);
    });
  }

  function prepareChallengeRound(now = performance.now()) {
    const challenge = state.challenge;
    if (!challenge.data?.steps?.length) return stopChallenge(false);
    releaseChallengePreview();
    challenge.phrase = buildChallengePhrase(challenge.data, challenge.difficulty, challenge.round);
    challenge.index = 0;
    challenge.previewIndex = 0;
    challenge.matched.clear();
    challenge.phase = challenge.phrase.length ? 'preview' : 'idle';
    challenge.nextAt = now;
    challenge.deadlineAt = 0;
    challenge.advanceAt = 0;
    setNotice(challenge.phrase.length ? t('challengePreview') : t('challengeNoData'));
    syncUI();
  }

  function startChallengeRound(now) {
    const challenge = state.challenge;
    challenge.round += 1;
    prepareChallengeRound(now);
  }

  function finishChallengeStep(success, now = performance.now()) {
    const challenge = state.challenge;
    if (success) {
      challenge.hits += 1;
      challenge.combo += 1;
      challenge.bestCombo = Math.max(challenge.bestCombo, challenge.combo);
      challenge.score += scoreForStep(challenge.difficulty, challenge.round, challenge.combo - 1);
    } else {
      challenge.mistakes += 1;
      challenge.combo = 0;
    }
    challenge.index += 1;
    challenge.matched.clear();
    if (challenge.index >= challenge.phrase.length) {
      releaseChallengePreview();
      challenge.phase = 'round';
      challenge.advanceAt = now + 850;
      setNotice(success ? t('challengeComplete') : t('challengeMissed'));
    } else {
      challenge.deadlineAt = now + timingWindowMs(currentChallengeStep(), challenge.bpm, challenge.difficulty, challenge.round);
      setNotice(success ? t('challengeInput') : t('challengeMissed'));
    }
    syncUI();
  }

  function handleChallengeKeyPress(keyId) {
    const challenge = state.challenge;
    const step = currentChallengeStep();
    if (!challenge.active || challenge.phase !== 'input' || !step) return;
    const targets = targetKeyIds(step);
    if (targets.includes(keyId)) {
      challenge.matched.add(keyId);
      if (targets.every((id) => challenge.matched.has(id))) finishChallengeStep(true);
    } else {
      challenge.mistakes += 1;
      challenge.combo = 0;
      setNotice(t('challengeMissed'));
      syncUI();
    }
  }

  async function startChallenge(restart = false) {
    const challenge = state.challenge;
    if (!state.lesson.data?.steps?.length) return setNotice(t('challengeNoData'));
    if (challenge.active && !restart) restart = true;
    if (lessonAdvanceTimer) window.clearTimeout(lessonAdvanceTimer);
    lessonAdvanceTimer = null;
    state.lesson.active = false;
    state.lesson.matched.clear();
    releaseChallengePreview();
    challenge.active = true;
    challenge.phase = 'preview';
    challenge.data = state.lesson.data;
    challenge.difficulty = normalizeDifficulty(qs('#challengeDifficulty').value);
    challenge.bpm = clamp(Number(challenge.data.bpm) || state.settings.bpm, 40, 240);
    if (restart || !challenge.score) {
      challenge.round = 1;
      challenge.score = 0;
      challenge.combo = 0;
      challenge.bestCombo = 0;
      challenge.hits = 0;
      challenge.mistakes = 0;
    }
    applyLessonSettings();
    setPage('keys');
    if (!synth.context || synth.context.state !== 'running') await enableAudio();
    prepareChallengeRound(performance.now());
  }

  function stopChallenge(showNotice = true) {
    const challenge = state.challenge;
    releaseChallengePreview();
    challenge.active = false;
    challenge.phase = 'idle';
    challenge.phrase = [];
    challenge.index = 0;
    challenge.previewIndex = 0;
    challenge.nextAt = 0;
    challenge.deadlineAt = 0;
    challenge.advanceAt = 0;
    challenge.matched.clear();
    if (showNotice) {
      setPage('learn');
      setNotice(t('challengeStopped'));
    }
    syncUI();
  }

  function serviceChallenge(now) {
    const challenge = state.challenge;
    if (!challenge.active) return;
    if (challenge.phase === 'preview') {
      let processed = 0;
      while (now >= challenge.nextAt && processed < 8) {
        if (challenge.previewIndex >= challenge.phrase.length) {
          releaseChallengePreview();
          challenge.phase = 'input';
          challenge.index = 0;
          challenge.matched.clear();
          challenge.deadlineAt = now + timingWindowMs(currentChallengeStep(), challenge.bpm, challenge.difficulty, challenge.round);
          setNotice(t('challengeInput'));
          syncUI();
          break;
        }
        const step = challenge.phrase[challenge.previewIndex++];
        previewChallengeStep(step);
        challenge.nextAt += previewDurationMs(step, challenge.difficulty);
        processed += 1;
      }
    } else if (challenge.phase === 'input' && now >= challenge.deadlineAt) {
      finishChallengeStep(false, now);
    } else if (challenge.phase === 'round' && now >= challenge.advanceAt) {
      startChallengeRound(now);
    }
    if (challenge.phase === 'input') {
      const step = currentChallengeStep();
      const total = timingWindowMs(step, challenge.bpm, challenge.difficulty, challenge.round);
      const remaining = Math.max(0, challenge.deadlineAt - now);
      challenge.inputProgress = total ? 100 - remaining / total * 100 : 0;
    }
    syncChallengeUI();
  }

  function syncChallengeUI() {
    const challenge = state.challenge;
    const hasData = Boolean(state.lesson.data?.steps?.length);
    const step = challenge.phase === 'preview'
      ? challenge.phrase[Math.max(0, Math.min(challenge.previewIndex - 1, challenge.phrase.length - 1))]
      : currentChallengeStep();
    const total = challenge.phrase.length;
    const position = challenge.phase === 'preview' ? Math.max(1, Math.min(challenge.previewIndex, total)) : Math.min(challenge.index + 1, total);
    const progress = total ? clamp((challenge.phase === 'preview' ? challenge.previewIndex : challenge.index) / total * 100, 0, 100) : 0;
    qs('#challengeDifficulty').value = challenge.difficulty;
    qs('#challengeDifficulty').disabled = challenge.active;
    qs('#challengeStartButton').disabled = !hasData;
    qs('#challengeStartButton').textContent = challenge.active ? t('challengeRestart') : t('challengeStart');
    qs('#challengeStopButtonPage').disabled = !challenge.active;
    qs('#challengeStatusChip').textContent = challenge.active ? challengePhaseLabel() : hasData ? t('challengeReady') : t('challengeNoData');
    qs('#challengePhraseReadout').textContent = challenge.active ? `${position} / ${total} · ${challengePhaseLabel()}` : '-';
    qs('#challengeRoundValue').textContent = challenge.active || challenge.round > 1 ? String(challenge.round) : '0';
    qs('#challengeScoreValue').textContent = String(challenge.score);
    qs('#challengeComboValue').textContent = String(challenge.combo);
    qs('#challengeAccuracyValue').textContent = `${challengeAccuracy()}%`;
    qs('#challengeLive').hidden = !challenge.active;
    qs('#challengeLiveTitle').textContent = challenge.active ? `${t('challengeRound')} ${challenge.round} · ${position} / ${total}` : '-';
    qs('#challengeLiveKeys').textContent = challenge.active && step ? (challenge.phase === 'preview' ? step.notes.map((note) => note.name).join(' + ') : `${challenge.phase === 'input' && step.requires_enter ? `${t('enterFirst')} · ` : ''}${lessonKeyNames(step).join(' + ')}`) : '-';
    qs('#challengeLiveProgress').textContent = `${Math.round(progress)}%`;
    qs('#challengeLiveBar').style.width = `${progress}%`;
  }

  async function chooseMidi() {
    const result = await window.musicKeyboard.chooseMidi();
    if (!result?.ok || result.canceled) return;
    state.converter.path = result.path;
    state.converter.name = result.name;
    qs('#midiFileName').textContent = result.name;
    qs('#midiFilePath').textContent = result.path;
    qs('#converterStatus').textContent = t('converterReady');
    setNotice(t('converterReady'));
  }

  async function convertMidi() {
    if (!state.converter.path || state.converter.busy) {
      if (!state.converter.path) setNotice(t('chooseMidiHint'));
      return;
    }
    state.converter.busy = true;
    qs('#convertMidiButton').disabled = true;
    qs('#converterStatus').textContent = t('converting');
    setNotice(t('converting'));
    try {
      const result = await window.musicKeyboard.convertMidi({
        path: state.converter.path,
        grid: Number(qs('#gridSelect').value),
        track: qs('#trackInput').value,
        rpnKeyLabel: qs('#rpnInput').value,
        fullTrack: qs('#fullTrackToggle').checked,
        chords: qs('#chordsToggle').checked,
      });
      if (result?.ok) {
        const output = result.output || '';
        qs('#converterOutput').textContent = output || t('reportEmpty');
        qs('#reportReadout').textContent = `${output ? output.split(/\r?\n/).length : 0} lines`;
        qs('#converterStatus').textContent = t('convertDone');
        setNotice(t('convertDone'));
      } else {
        qs('#converterStatus').textContent = result?.error || 'Converter error';
        setNotice(result?.error || 'Converter error');
      }
    } catch (error) {
      qs('#converterStatus').textContent = error.message || 'Converter error';
      setNotice(error.message || 'Converter error');
    } finally {
      state.converter.busy = false;
      qs('#convertMidiButton').disabled = false;
    }
  }

  async function enableAudio() {
    try {
      await synth.enable();
      qs('#audioButton').classList.add('ready');
      setNotice(t('audioReady'));
    } catch (error) {
      setNotice(error.message || 'Web Audio unavailable');
    }
  }

  function setTone(value, record = true) {
    const next = (Number(value) + TONE_NAMES.length) % TONE_NAMES.length;
    if (next === state.settings.tone) return;
    state.settings.tone = next;
    if (record && state.recording?.active) recordEvent({ kind: 'program', program: GM_PROGRAMS[next] });
    syncUI();
  }

  function persistSynthSettings() {
    localStorage.setItem('musicKeyboardSynth', JSON.stringify(state.synth));
  }

  function synthTimeText(value) {
    const seconds = Number(value) || 0;
    return seconds < 1 ? `${Math.round(seconds * 1000)} ms` : `${seconds.toFixed(2)} s`;
  }

  function syncSynthUI() {
    const settings = state.synth;
    const custom = settings.mode === 'custom';
    qs('#synthModeSelect').value = custom ? 'custom' : 'factory';
    qs('#synthPresetSelect').value = SYNTH_PRESETS[settings.preset] ? settings.preset : 'init';
    qs('#synthModeChip').textContent = custom ? t('customPatch') : t('factoryTone');
    qs('#synthOsc1Wave').value = settings.osc1Wave;
    qs('#synthOsc2Wave').value = settings.osc2Wave;
    const ranges = {
      synthOsc1Level: settings.osc1Level, synthOsc2Level: settings.osc2Level, synthOsc2Detune: settings.osc2Detune, synthNoiseLevel: settings.noiseLevel,
      synthCutoff: settings.cutoff, synthResonance: settings.resonance, synthAttack: settings.attack, synthDecay: settings.decay, synthSustain: settings.sustainLevel, synthRelease: settings.release, synthLfoRate: settings.lfoRate, synthLfoDepth: settings.lfoDepth,
    };
    Object.entries(ranges).forEach(([id, value]) => { qs(`#${id}`).value = String(value); });
    qs('#synthOsc1LevelValue').textContent = `${Math.round(settings.osc1Level * 100)}%`;
    qs('#synthOsc2LevelValue').textContent = `${Math.round(settings.osc2Level * 100)}%`;
    qs('#synthOsc2DetuneValue').textContent = `${signed(settings.osc2Detune)} ct`;
    qs('#synthNoiseLevelValue').textContent = `${Math.round(settings.noiseLevel * 100)}%`;
    qs('#synthCutoffValue').textContent = `${Math.round(settings.cutoff)} Hz`;
    qs('#synthResonanceValue').textContent = `${Math.round(settings.resonance * 100)}%`;
    qs('#synthAttackValue').textContent = synthTimeText(settings.attack);
    qs('#synthDecayValue').textContent = synthTimeText(settings.decay);
    qs('#synthSustainValue').textContent = `${Math.round(settings.sustainLevel * 100)}%`;
    qs('#synthReleaseValue').textContent = synthTimeText(settings.release);
    qs('#synthLfoRateValue').textContent = `${Number(settings.lfoRate).toFixed(2)} Hz`;
    qs('#synthLfoDepthValue').textContent = `${Math.round(settings.lfoDepth)} ct`;
    ['synthOsc1Wave', 'synthOsc1Level', 'synthOsc2Wave', 'synthOsc2Level', 'synthOsc2Detune', 'synthNoiseLevel', 'synthCutoff', 'synthResonance', 'synthAttack', 'synthDecay', 'synthSustain', 'synthRelease', 'synthLfoRate', 'synthLfoDepth'].forEach((id) => { qs(`#${id}`).disabled = !custom; });
  }

  function setSynthMode(mode) {
    state.synth.mode = mode === 'custom' ? 'custom' : 'factory';
    persistSynthSettings();
    setNotice(t(state.synth.mode === 'custom' ? 'customReady' : 'factoryReady'));
  }

  function setSynthParameter(name, value) {
    const numeric = Number(value);
    if (name === 'osc1Wave' || name === 'osc2Wave') state.synth[name] = ['sine', 'triangle', 'sawtooth', 'square'].includes(value) ? value : 'sine';
    else if (name === 'osc1Level' || name === 'osc2Level') state.synth[name] = clamp(numeric, 0, 1);
    else if (name === 'osc2Detune') state.synth[name] = clamp(Math.round(numeric), -1200, 1200);
    else if (name === 'noiseLevel') state.synth[name] = clamp(numeric, 0, .35);
    else if (name === 'cutoff') state.synth[name] = clamp(Math.round(numeric / 10) * 10, 120, 12000);
    else if (name === 'resonance' || name === 'sustainLevel') state.synth[name] = clamp(numeric, 0, 1);
    else if (name === 'attack') state.synth[name] = clamp(numeric, .001, 2);
    else if (name === 'decay') state.synth[name] = clamp(numeric, .001, 3);
    else if (name === 'release') state.synth[name] = clamp(numeric, .08, 3);
    else if (name === 'lfoRate') state.synth[name] = clamp(numeric, .05, 12);
    else if (name === 'lfoDepth') state.synth[name] = clamp(Math.round(numeric), 0, 60);
    else return;
    state.synth.mode = 'custom';
    state.synth.preset = 'init';
    persistSynthSettings();
    syncUI();
  }

  function applySynthPreset() {
    const name = SYNTH_PRESETS[qs('#synthPresetSelect').value] ? qs('#synthPresetSelect').value : 'init';
    Object.assign(state.synth, SYNTH_PRESETS[name], { mode: 'custom', preset: name });
    persistSynthSettings();
    setNotice(t('presetApplied'));
  }

  function resetSynthPatch() {
    state.synth = { ...SYNTH_DEFAULTS };
    persistSynthSettings();
    setNotice(t('factoryReady'));
  }

  function setSetting(name, value, record = true) {
    const settings = state.settings;
    if (name === 'octave') settings.octave = clamp(Math.round(value), 1, 7);
    else if (name === 'root') settings.root = (Math.round(value) + 12) % 12;
    else if (name === 'transpose') settings.transpose = clamp(Math.round(value), -12, 12);
    else if (name === 'velocity') settings.velocity = clamp(Math.round(value / 8) * 8, 8, 127);
    else if (name === 'volume') settings.volume = clamp(Math.round(value), 0, 10);
    else if (name === 'bpm') settings.bpm = clamp(Math.round(value / 5) * 5, 40, 240);
    else return;
    if (name === 'volume') synth.setVolume(settings.volume);
    if (name === 'bpm') {
      restartMetronome();
      if (record && state.recording?.active) recordEvent({ kind: 'tempo', bpm: settings.bpm });
    }
    syncUI();
  }

  function setSustain(enabled, record = true) {
    const next = Boolean(enabled);
    if (next === state.settings.sustain) return;
    state.settings.sustain = next;
    synth.setSustain(next);
    if (record && state.recording?.active) recordEvent({ kind: 'sustain', enabled: next });
    syncUI();
  }

  function setMetronome(enabled) {
    state.settings.metronome = Boolean(enabled);
    restartMetronome();
    syncUI();
  }

  let metronomeTimer = null;
  let metronomeBeat = 0;
  function restartMetronome() {
    if (metronomeTimer) window.clearInterval(metronomeTimer);
    metronomeTimer = null;
    if (!state.settings.metronome) return;
    const interval = 60000 / state.settings.bpm;
    metronomeTimer = window.setInterval(() => {
      synth.click(metronomeBeat++ % 4 === 0);
    }, interval);
  }

  const CALIBRATION_NOTE_KEYS = new Set([1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 22, 23, 25, 26]);

  function setLatencyCalibration(value) {
    state.settings.latencyMs = clamp(Math.round(Number(value) || 0), -250, 250);
    localStorage.setItem('musicKeyboardLatencyMs', String(state.settings.latencyMs));
    syncUI();
  }

  function median(values) {
    if (!values.length) return 0;
    const sorted = [...values].sort((a, b) => a - b);
    const middle = Math.floor(sorted.length / 2);
    return sorted.length % 2 ? sorted[middle] : (sorted[middle - 1] + sorted[middle]) / 2;
  }

  function calibrationTotalBeats() {
    return state.calibration.leadInBeats + state.calibration.measuredBeats;
  }

  function finishLatencyCalibration() {
    const calibration = state.calibration;
    if (!calibration.active) return;
    if (calibration.timer) window.clearTimeout(calibration.timer);
    calibration.timer = null;
    calibration.active = false;
    calibration.phase = 'done';
    calibration.missed = calibration.beats.filter((beat) => !beat.matched).length;
    const samples = calibration.samples;
    if (samples.length < 4) {
      calibration.result = null;
      calibration.phase = 'error';
      setNotice(t('calibrationTooFew'));
      syncUI();
      return;
    }
    const ordered = [...samples].sort((a, b) => a - b);
    const trim = ordered.length >= 8 ? Math.floor(ordered.length * .15) : 0;
    const robust = ordered.slice(trim, ordered.length - trim || undefined);
    const offset = clamp(Math.round(median(robust)), -250, 250);
    const spread = Math.round(1.4826 * median(robust.map((value) => Math.abs(value - median(robust)))));
    calibration.result = { offset, spread };
    setLatencyCalibration(offset);
    setNotice(`${t('calibrationDone')}: ${signed(offset)} ms`);
  }

  function scheduleCalibrationBeat() {
    const calibration = state.calibration;
    if (!calibration.active) return;
    const targetAt = calibration.nextBeatAt;
    const index = calibration.beatIndex;
    const isMeasured = index >= calibration.leadInBeats;
    if (isMeasured) calibration.beats.push({ at: targetAt, matched: false });
    calibration.phase = isMeasured ? 'measure' : 'countin';
    synth.clickAt(targetAt, index % 4 === 0);
    calibration.beatIndex += 1;
    calibration.nextBeatAt += 60000 / calibration.bpm;
    calibration.pulse = true;
    syncUI();
    window.setTimeout(() => {
      if (!calibration.active) return;
      calibration.pulse = false;
      syncUI();
    }, Math.max(0, targetAt - performance.now() + 110));
    if (calibration.beatIndex >= calibrationTotalBeats()) {
      calibration.timer = window.setTimeout(finishLatencyCalibration, Math.max(0, calibration.nextBeatAt - performance.now() + 120));
      return;
    }
    calibration.timer = window.setTimeout(scheduleCalibrationBeat, Math.max(1, targetAt - performance.now() + 60000 / calibration.bpm - 25));
  }

  function startLatencyCalibration() {
    if (!synth.context || synth.context.state !== 'running') {
      setNotice(t('calibrationReady'));
      return;
    }
    stopLatencyCalibration(false);
    const calibration = state.calibration;
    calibration.bpm = Number(qs('#calibrationBpmSelect').value) || 90;
    calibration.measuredBeats = Number(qs('#calibrationBeatsSelect').value) || 12;
    calibration.beatIndex = 0;
    calibration.nextBeatAt = performance.now() + 850;
    calibration.beats = [];
    calibration.samples = [];
    calibration.missed = 0;
    calibration.result = null;
    calibration.phase = 'countin';
    calibration.active = true;
    calibration.timer = window.setTimeout(scheduleCalibrationBeat, 1);
    setNotice(t('calibrationCountIn'));
    syncUI();
  }

  function stopLatencyCalibration(showNotice = true) {
    const calibration = state.calibration;
    if (calibration.timer) window.clearTimeout(calibration.timer);
    calibration.timer = null;
    calibration.active = false;
    calibration.pulse = false;
    if (calibration.phase !== 'done') calibration.phase = 'idle';
    if (showNotice) setNotice(t('calibrationReady'));
    else syncUI();
  }

  function resetLatencyCalibration() {
    stopLatencyCalibration(false);
    state.calibration.phase = 'idle';
    state.calibration.result = null;
    state.calibration.beats = [];
    state.calibration.samples = [];
    state.calibration.missed = 0;
    setLatencyCalibration(0);
    setNotice(t('calibrationReady'));
  }

  function handleCalibrationTap(keyId, record) {
    const calibration = state.calibration;
    if (!calibration.active || Number(record.action_id) === 2 || Number(record.action_id) === 3 || record.action === 'RELEASE' || record.action === 'REPEAT' || !CALIBRATION_NOTE_KEYS.has(keyId)) return;
    const now = performance.now();
    const beatMs = 60000 / calibration.bpm;
    const candidate = calibration.beats
      .filter((beat) => !beat.matched)
      .map((beat) => ({ beat, distance: Math.abs(now - beat.at) }))
      .sort((a, b) => a.distance - b.distance)[0];
    if (!candidate || candidate.distance > Math.max(160, beatMs * .45)) return;
    candidate.beat.matched = true;
    calibration.samples.push(now - candidate.beat.at);
    syncUI();
  }

  function persistGrooveSettings() {
    localStorage.setItem('musicKeyboardGroove', JSON.stringify({
      steps: state.groove.steps,
      resolution: state.groove.resolution,
      pattern: state.groove.pattern,
    }));
  }

  function persistArpSettings() {
    localStorage.setItem('musicKeyboardArp', JSON.stringify({
      pattern: state.arp.pattern,
      rate: state.arp.rate,
      octaves: state.arp.octaves,
      gate: state.arp.gate,
    }));
  }

  function persistChordSettings() {
    localStorage.setItem('musicKeyboardChordAssist', JSON.stringify({
      type: state.chordAssist.type,
      bass: state.chordAssist.bass,
      pulse: state.chordAssist.pulse,
    }));
  }

  function persistSamplerSettings() {
    localStorage.setItem('musicKeyboardSampler', JSON.stringify({
      sliceCount: state.sampler.sliceCount,
      start: state.sampler.start,
      end: state.sampler.end,
      speed: state.sampler.speed,
      volume: state.sampler.volume,
    }));
  }

  function setChordAssistEnabled(enabled) {
    const next = Boolean(enabled);
    if (next === state.chordAssist.enabled) return syncUI();
    if (next && state.arp.enabled) setArpEnabled(false);
    if (next && state.sampler.enabled) setSamplerEnabled(false);
    if (state.bindings.size) releaseAllBindings(false);
    state.chordAssist.enabled = next;
    state.chordAssist.held.clear();
    state.chordAssist.nextAt = 0;
    syncUI();
  }

  function setChordAssistParameter(name, value) {
    if (name === 'type') state.chordAssist.type = normalizeChordType(value);
    else if (name === 'bass') state.chordAssist.bass = Boolean(value);
    else if (name === 'pulse') state.chordAssist.pulse = Boolean(value);
    else return;
    if (state.chordAssist.enabled && state.bindings.size) releaseAllBindings(false);
    persistChordSettings();
    syncUI();
  }

  function serviceChordAssist(now) {
    const assistant = state.chordAssist;
    if (!assistant.enabled || !assistant.bass || !assistant.pulse || !assistant.held.size || !synth.context || synth.context.state !== 'running') {
      assistant.nextAt = 0;
      return;
    }
    const interval = 60000 / Math.max(30, state.settings.bpm);
    if (!assistant.nextAt) assistant.nextAt = now;
    let processed = 0;
    while (now >= assistant.nextAt && processed < 4) {
      assistant.held.forEach((root) => {
        const note = clamp(root - 12, 24, 96);
        synth.noteOn(note, Math.max(48, state.settings.velocity - 12), state.settings.tone);
        const event = { kind: 'noteOn', note, velocity: Math.max(48, state.settings.velocity - 12) };
        captureLooperEvent(event);
        if (state.recording?.active) recordEvent(event);
        window.setTimeout(() => {
          synth.noteOff(note);
          const offEvent = { kind: 'noteOff', note };
          captureLooperEvent(offEvent);
          if (state.recording?.active) recordEvent(offEvent);
        }, Math.max(70, interval * .42));
      });
      assistant.nextAt += interval;
      processed += 1;
    }
    if (processed === 4 && now >= assistant.nextAt) assistant.nextAt = now + interval;
  }

  function renderSamplerPads() {
    const grid = qs('#samplerPadGrid');
    if (!grid) return;
    const ids = SAMPLER_KEY_IDS.slice(0, state.sampler.sliceCount);
    grid.innerHTML = ids.map((id, index) => `<button class="sampler-pad" type="button" data-sampler-index="${index}" data-sampler-key="${id}"><span>${visibleKeyName(id)}</span><small>${index + 1}</small></button>`).join('');
    samplerPadRevision = state.sampler.sliceCount;
  }

  function syncChordAssistUI() {
    const assistant = state.chordAssist;
    qs('#chordAssistEnabled').checked = assistant.enabled;
    qs('#chordAssistType').value = assistant.type;
    qs('#chordAssistBass').checked = assistant.bass;
    qs('#chordAssistPulse').checked = assistant.pulse;
    qs('#chordAssistModeChip').textContent = t(assistant.enabled ? 'chordOn' : 'chordOff');
  }

  function stopSamplerSources() {
    state.sampler.sources.forEach((source) => { try { source.stop(); } catch (_) {} });
    state.sampler.sources.clear();
  }

  function updateSamplerSlices() {
    state.sampler.slices = state.sampler.buffer
      ? buildSlices(state.sampler.buffer.duration, state.sampler.sliceCount, state.sampler.start, state.sampler.end)
      : [];
    samplerPadRevision = -1;
  }

  function syncSamplerUI() {
    const sampler = state.sampler;
    if (samplerPadRevision !== sampler.sliceCount) renderSamplerPads();
    const status = sampler.loading ? t('samplerLoading') : sampler.buffer ? `${t('samplerReady')}: ${sampler.name}` : t('samplerNoFile');
    qs('#samplerStatusChip').textContent = status;
    qs('#samplerStatus').textContent = status;
    qs('#samplerEnabled').checked = sampler.enabled;
    qs('#samplerEnabled').disabled = !sampler.buffer || sampler.loading;
    qs('#samplerSliceCount').value = String(sampler.sliceCount);
    qs('#samplerStart').value = String(sampler.start);
    qs('#samplerEnd').value = String(sampler.end);
    qs('#samplerSpeed').value = String(sampler.speed);
    qs('#samplerStartValue').textContent = `${Math.round(sampler.start * 100)}%`;
    qs('#samplerEndValue').textContent = `${Math.round(sampler.end * 100)}%`;
    qs('#samplerSpeedValue').textContent = `${sampler.speed.toFixed(2)}x`;
    qsa('.sampler-pad').forEach((button) => {
      const keyId = Number(button.dataset.samplerKey);
      button.classList.toggle('disabled', !sampler.buffer);
      button.classList.toggle('active', state.bindings.has(keyId));
    });
  }

  async function chooseSamplerFile() {
    const file = qs('#samplerFileInput').files?.[0];
    if (!file) return;
    const sampler = state.sampler;
    if (sampler.enabled && state.bindings.size) releaseAllBindings(false);
    stopSamplerSources();
    sampler.enabled = false;
    sampler.loading = true;
    sampler.name = file.name;
    sampler.buffer = null;
    sampler.slices = [];
    syncUI();
    try {
      await synth.enable();
      if (!synth.context) throw new Error('Web Audio is unavailable');
      const bytes = await file.arrayBuffer();
      sampler.buffer = await synth.context.decodeAudioData(bytes);
      updateSamplerSlices();
      setNotice(`${t('samplerReady')}: ${file.name}`);
    } catch (error) {
      sampler.buffer = null;
      sampler.slices = [];
      setNotice(`${t('samplerError')}: ${error.message || error}`);
    } finally {
      sampler.loading = false;
      syncUI();
    }
  }

  function setSamplerParameter(name, value) {
    const sampler = state.sampler;
    if (name === 'sliceCount') sampler.sliceCount = clamp(Math.round(Number(value) || 6), 2, 12);
    else if (name === 'start') sampler.start = clamp(Number(value) || 0, 0, Math.max(0, sampler.end - .02));
    else if (name === 'end') sampler.end = clamp(Number(value) || 1, Math.min(1, sampler.start + .02), 1);
    else if (name === 'speed') sampler.speed = clamp(Number(value) || 1, .5, 2);
    else return;
    updateSamplerSlices();
    persistSamplerSettings();
    syncUI();
  }

  function setSamplerEnabled(enabled) {
    const next = Boolean(enabled);
    if (next && !state.sampler.buffer) {
      state.sampler.enabled = false;
      setNotice(t('samplerNoFile'));
      return;
    }
    if (next === state.sampler.enabled) return syncUI();
    if (next && state.arp.enabled) setArpEnabled(false);
    if (next && state.chordAssist.enabled) {
      state.chordAssist.enabled = false;
      state.chordAssist.held.clear();
      state.chordAssist.nextAt = 0;
    }
    if (state.bindings.size) releaseAllBindings(false);
    if (!next) stopSamplerSources();
    state.sampler.enabled = next;
    syncUI();
  }

  function playSamplerSlice(index) {
    const sampler = state.sampler;
    const slice = sampler.slices[index];
    if (!slice || !sampler.buffer || !synth.context || synth.context.state !== 'running' || !synth.master) return false;
    const source = synth.context.createBufferSource();
    const gain = synth.context.createGain();
    const now = synth.context.currentTime;
    const duration = Math.max(.025, (slice.end - slice.start) / sampler.speed);
    const release = Math.min(.08, duration * .3);
    source.buffer = sampler.buffer;
    source.playbackRate.value = sampler.speed;
    gain.gain.setValueAtTime(.0001, now);
    gain.gain.linearRampToValueAtTime(sampler.volume, now + Math.min(.008, duration * .15));
    gain.gain.setTargetAtTime(.0001, now + Math.max(.012, duration - release), Math.max(.008, release / 3));
    source.connect(gain);
    gain.connect(synth.master);
    sampler.sources.add(source);
    source.onended = () => sampler.sources.delete(source);
    try {
      source.start(now, slice.start);
      source.stop(now + duration + .04);
      return true;
    } catch (_) {
      sampler.sources.delete(source);
      return false;
    }
  }

  function triggerSamplerKey(keyId) {
    const index = sliceForKey(keyId, SAMPLER_KEY_IDS.slice(0, state.sampler.sliceCount));
    return index >= 0 && playSamplerSlice(index);
  }

  function renderDrumGrid() {
    const grid = qs('#drumGrid');
    if (!grid) return;
    const header = Array.from({ length: PATTERN_LENGTH }, (_, step) => `<span class="drum-header-step${step % 4 === 0 ? ' bar-start' : ''}" data-step="${step}">${step + 1}</span>`).join('');
    const lanes = DRUM_LANES.map((lane) => {
      const buttons = Array.from({ length: PATTERN_LENGTH }, (_, step) => `<button class="drum-step${step % 4 === 0 ? ' bar-start' : ''}" data-lane="${lane.id}" data-step="${step}" type="button" aria-label="${t(lane.labelKey)} ${step + 1}"></button>`).join('');
      return `<div class="drum-lane"><div class="drum-lane-label" data-i18n="${lane.labelKey}">${t(lane.labelKey)}</div><div class="drum-steps">${buttons}</div></div>`;
    }).join('');
    grid.innerHTML = `<div class="drum-grid-header"><span>${t('pattern')}</span><div class="drum-header-steps">${header}</div></div>${lanes}`;
    syncGrooveUI();
  }

  function syncGroovePlayhead() {
    const currentStep = state.groove.playing ? state.groove.currentStep : -1;
    qsa('#drumGrid .drum-step').forEach((button) => {
      button.classList.toggle('playhead', Number(button.dataset.step) === currentStep);
    });
  }

  function syncGrooveUI() {
    const groove = state.groove;
    const arp = state.arp;
    qs('#grooveBpmRange').value = String(state.settings.bpm);
    qs('#grooveBpmValue').textContent = String(state.settings.bpm);
    qs('#grooveStepsSelect').value = String(groove.steps);
    qs('#grooveResolutionSelect').value = String(groove.resolution);
    qs('#grooveTransportChip').textContent = t(groove.playing ? 'groovePlaying' : 'grooveReady');
    qs('#groovePlayButton').textContent = t(groove.playing ? 'grooveStop' : 'groovePlay');
    qs('#groovePatternReadout').textContent = t(groove.steps === 32 ? 'grooveThirtyTwo' : 'grooveSixteen');
    qs('#drumGrid').style.setProperty('--groove-columns', String(groove.steps));
    qsa('#drumGrid .drum-header-step').forEach((step) => step.classList.toggle('step-hidden', Number(step.dataset.step) >= groove.steps));
    qsa('#drumGrid .drum-step').forEach((button) => {
      const lane = button.dataset.lane;
      const step = Number(button.dataset.step);
      button.classList.toggle('active', Boolean(groove.pattern[lane]?.[step]));
      button.classList.toggle('step-hidden', step >= groove.steps);
    });
    syncGroovePlayhead();
    qs('#arpEnabled').checked = arp.enabled;
    qs('#arpModeChip').textContent = t(arp.enabled ? 'arpOn' : 'arpOff');
    qs('#arpPatternSelect').value = arp.pattern;
    qs('#arpRateSelect').value = String(arp.rate);
    qs('#arpOctavesSelect').value = String(arp.octaves);
    qs('#arpGateRange').value = String(Math.round(arp.gate * 100));
    qs('#arpGateValue').textContent = `${Math.round(arp.gate * 100)}%`;
  }

  function toggleGrooveStep(lane, step) {
    if (!state.groove.pattern[lane] || step < 0 || step >= PATTERN_LENGTH) return;
    state.groove.pattern[lane][step] = !state.groove.pattern[lane][step];
    persistGrooveSettings();
    syncGrooveUI();
  }

  function clearGroovePattern() {
    DRUM_LANES.forEach((lane) => state.groove.pattern[lane.id].fill(false));
    persistGrooveSettings();
    syncUI();
  }

  function randomizeGroovePattern() {
    const steps = state.groove.steps;
    DRUM_LANES.forEach((lane) => state.groove.pattern[lane.id].fill(false));
    for (let step = 0; step < steps; step += 1) {
      state.groove.pattern.kick[step] = step % 8 === 0 || Math.random() < .18;
      state.groove.pattern.snare[step] = step % 8 === 4 || Math.random() < .1;
      state.groove.pattern.hat[step] = step % 2 === 0 || Math.random() < .2;
      state.groove.pattern.clap[step] = step % 16 === 12 || Math.random() < .08;
    }
    persistGrooveSettings();
    syncUI();
  }

  function startGroove() {
    state.groove.playing = true;
    state.groove.currentStep = -1;
    state.groove.nextStepAt = performance.now();
    syncUI();
  }

  function stopGroove() {
    state.groove.playing = false;
    state.groove.currentStep = -1;
    state.groove.nextStepAt = 0;
    syncUI();
  }

  async function toggleGroove() {
    if (state.groove.playing) return stopGroove();
    if (!synth.context || synth.context.state !== 'running') await enableAudio();
    if (!synth.context || synth.context.state !== 'running') return;
    startGroove();
  }

  function serviceGroove(now) {
    const groove = state.groove;
    if (!groove.playing) return;
    const interval = stepIntervalMs(state.settings.bpm, groove.resolution);
    if (!groove.nextStepAt) groove.nextStepAt = now;
    let processed = 0;
    while (now >= groove.nextStepAt && processed < 8) {
      groove.currentStep = (groove.currentStep + 1 + groove.steps) % groove.steps;
      DRUM_LANES.forEach((lane) => {
        if (!groove.pattern[lane.id][groove.currentStep]) return;
        const accent = groove.currentStep % 4 === 0;
        const velocity = lane.id === 'hat' ? (accent ? .58 : .38) : accent ? 1 : .78;
        synth.triggerDrum(lane.id, velocity);
      });
      groove.nextStepAt += interval;
      processed += 1;
      syncGroovePlayhead();
    }
    if (processed === 8 && now >= groove.nextStepAt) groove.nextStepAt = now + interval;
  }

  function arpHeldNotes() {
    return [...state.arp.held.keys()];
  }

  function stopArpNote(record = true) {
    const arp = state.arp;
    if (arp.currentNote === null) return;
    synth.noteOff(arp.currentNote);
    const event = { kind: 'noteOff', note: arp.currentNote };
    captureLooperEvent(event);
    if (record && state.recording?.active) recordEvent(event);
    arp.currentNote = null;
    arp.currentOffAt = 0;
  }

  function addArpNotes(notes) {
    if (!state.arp.enabled) return;
    notes.forEach((note) => state.arp.held.set(note, (state.arp.held.get(note) || 0) + 1));
    if (!state.arp.nextAt) state.arp.nextAt = performance.now();
  }

  function removeArpNotes(notes) {
    if (!state.arp.enabled) return;
    notes.forEach((note) => {
      const count = state.arp.held.get(note) || 0;
      if (count <= 1) state.arp.held.delete(note);
      else state.arp.held.set(note, count - 1);
    });
    if (!state.arp.held.size) {
      stopArpNote();
      state.arp.nextAt = 0;
      state.arp.stepIndex = 0;
    }
  }

  function setArpEnabled(enabled) {
    const next = Boolean(enabled);
    const arp = state.arp;
    if (next === arp.enabled) return;
    if (next) {
      const held = new Map();
      state.bindings.forEach((notes) => notes.forEach((note) => {
        synth.noteOff(note);
        held.set(note, (held.get(note) || 0) + 1);
      }));
      arp.enabled = true;
      arp.held = held;
      arp.currentNote = null;
      arp.currentOffAt = 0;
      arp.stepIndex = 0;
      arp.nextAt = held.size ? performance.now() : 0;
    } else {
      stopArpNote();
      arp.enabled = false;
      arp.held.clear();
      arp.nextAt = 0;
      arp.stepIndex = 0;
      state.bindings.forEach((notes) => notes.forEach((note) => synth.noteOn(note, state.settings.velocity, state.settings.tone)));
    }
    syncUI();
  }

  async function toggleArpeggiator(enabled) {
    if (enabled && (!synth.context || synth.context.state !== 'running')) await enableAudio();
    if (enabled && (!synth.context || synth.context.state !== 'running')) return syncUI();
    if (enabled && state.sampler.enabled) setSamplerEnabled(false);
    if (enabled && state.chordAssist.enabled) setChordAssistEnabled(false);
    setArpEnabled(enabled);
  }

  function setArpParameter(name, value) {
    if (name === 'pattern') state.arp.pattern = ARP_PATTERNS.includes(value) ? value : 'up';
    else if (name === 'rate') state.arp.rate = ARP_RATES.includes(Number(value)) ? Number(value) : 8;
    else if (name === 'octaves') state.arp.octaves = clamp(Math.round(Number(value) || 1), 1, 3);
    else if (name === 'gate') state.arp.gate = clamp(Number(value) / 100, .35, 1);
    else return;
    persistArpSettings();
    state.arp.nextAt = state.arp.held.size ? performance.now() : 0;
    syncUI();
  }

  function serviceArpeggiator(now) {
    const arp = state.arp;
    if (!arp.enabled) return;
    if (arp.currentNote !== null && now >= arp.currentOffAt) stopArpNote();
    const notes = arpHeldNotes();
    if (!notes.length) {
      arp.nextAt = 0;
      return;
    }
    const interval = stepIntervalMs(state.settings.bpm, arp.rate);
    if (!arp.nextAt) arp.nextAt = now;
    const sequence = buildArpeggio(notes, arp.pattern, arp.octaves);
    if (!sequence.length) return;
    let processed = 0;
    while (now >= arp.nextAt && processed < 8) {
      stopArpNote();
      const index = arp.pattern === 'random' ? Math.floor(Math.random() * sequence.length) : arp.stepIndex % sequence.length;
      const note = sequence[index];
      synth.noteOn(note, state.settings.velocity, state.settings.tone);
      const event = { kind: 'noteOn', note, velocity: state.settings.velocity };
      captureLooperEvent(event);
      if (state.recording?.active) recordEvent(event);
      arp.currentNote = note;
      arp.currentOffAt = now + interval * arp.gate;
      arp.stepIndex = (index + 1) % sequence.length;
      arp.nextAt += interval;
      processed += 1;
    }
    if (processed === 8 && now >= arp.nextAt) arp.nextAt = now + interval;
  }

  let looperLayerRevision = -1;
  let samplerPadRevision = -1;

  function looperBarDurationMs() {
    return 60000 / state.looper.bpm * 4 * state.looper.bars;
  }

  function looperStatusKey() {
    if (state.looper.mode === 'armed') return 'looperArmed';
    if (state.looper.mode === 'recording') return 'looperRecording';
    if (state.looper.mode === 'overdubbing') return 'looperOverdub';
    if (state.looper.playing) return 'looperPlaying';
    return 'grooveReady';
  }

  function looperPosition(now = performance.now()) {
    const looper = state.looper;
    if (looper.mode === 'recording' || looper.mode === 'overdubbing') return clamp(now - looper.recordStartAt, 0, looper.loopDurationMs);
    if (looper.playing && looper.loopDurationMs) {
      const elapsed = now - looper.cycleStartAt;
      return ((elapsed % looper.loopDurationMs) + looper.loopDurationMs) % looper.loopDurationMs;
    }
    return 0;
  }

  function looperPositionText(position) {
    return `${formatTime(position)} / ${state.looper.bars} ${t('looperBar')}`;
  }

  function renderLooperLayers() {
    const list = qs('#looperLayerList');
    if (!list) return;
    if (!state.looper.layers.length) {
      list.innerHTML = `<div class="empty-state">${t('looperNoLayers')}</div>`;
      looperLayerRevision = state.looper.revision;
      return;
    }
    list.innerHTML = state.looper.layers.map((layer, index) => {
      const notes = layer.events.filter((event) => event.kind === 'noteOn').length;
      return `<div class="looper-layer${layer.muted ? ' muted' : ''}" data-layer-id="${layer.id}"><div><strong>${t('looperLayer')} ${index + 1}</strong><small>${notes} ${t('looperNotes')}</small></div><button class="button ghost" data-looper-mute="${layer.id}" type="button">${t(layer.muted ? 'looperUnmute' : 'looperMute')}</button><button class="button ghost remove-layer" data-looper-remove="${layer.id}" type="button" aria-label="${t('looperRemove')}">${t('looperRemove')}</button></div>`;
    }).join('');
    looperLayerRevision = state.looper.revision;
  }

  function syncLooperProgress(now = performance.now()) {
    const looper = state.looper;
    const position = looperPosition(now);
    const progress = looper.loopDurationMs ? position / looper.loopDurationMs * 100 : 0;
    qs('#looperPositionReadout').textContent = looper.loopDurationMs ? looperPositionText(position) : `0:00 / ${looper.bars} ${t('looperBar')}`;
    qs('#looperProgressBar').style.width = `${clamp(progress, 0, 100)}%`;
    qs('#looperProgressText').textContent = t(looperStatusKey());
  }

  function syncLooperUI() {
    const looper = state.looper;
    if (looperLayerRevision !== looper.revision) renderLooperLayers();
    qs('#looperBarsSelect').value = String(looper.bars);
    qs('#looperStatusChip').textContent = t(looperStatusKey());
    const recordButton = qs('#looperRecordButton');
    if (looper.mode === 'recording' || looper.mode === 'overdubbing') recordButton.textContent = t('looperRecording');
    else if (looper.mode === 'armed') recordButton.textContent = t('looperCancelArm');
    else if (looper.playing) recordButton.textContent = t('looperRecordLayer');
    else if (looper.layers.length) recordButton.textContent = t('looperPlay');
    else recordButton.textContent = t('looperRecordBase');
    recordButton.disabled = looper.mode === 'recording' || looper.mode === 'overdubbing';
    qs('#looperStopButton').disabled = !looper.playing && looper.mode === 'idle';
    qs('#looperClearButton').disabled = !looper.layers.length && looper.mode === 'idle';
    qs('#looperBarsSelect').disabled = looper.mode !== 'idle' || looper.layers.length > 0;
    syncLooperProgress();
  }

  function appendLooperEvent(event, timeMs) {
    const looper = state.looper;
    const note = Number(event.note);
    if (!Number.isFinite(note)) return;
    if (event.kind === 'noteOff' && !looper.capturedHeld.has(note)) return;
    const normalized = clamp(Math.round(timeMs), 0, looper.loopDurationMs);
    looper.recordEvents.push({ ...event, note, timeMs: normalized, tone: state.settings.tone });
    if (event.kind === 'noteOn') looper.capturedHeld.set(note, (looper.capturedHeld.get(note) || 0) + 1);
    else if (event.kind === 'noteOff') {
      const count = looper.capturedHeld.get(note) || 0;
      if (count <= 1) looper.capturedHeld.delete(note);
      else looper.capturedHeld.set(note, count - 1);
    }
  }

  function captureLooperEvent(event) {
    const looper = state.looper;
    if (looper.mode !== 'recording' && looper.mode !== 'overdubbing') return;
    appendLooperEvent(event, performance.now() - looper.recordStartAt);
  }

  function beginLooperCapture(startAt) {
    const looper = state.looper;
    looper.mode = looper.layers.length ? 'overdubbing' : 'recording';
    looper.recordStartAt = startAt;
    looper.recordEvents = [];
    looper.capturedHeld.clear();
    if (!state.arp.enabled) state.bindings.forEach((notes) => notes.forEach((note) => appendLooperEvent({ kind: 'noteOn', note, velocity: state.settings.velocity }, 0)));
    syncUI();
  }

  function releaseLooperLayerNotes(layerId) {
    const active = state.looper.activeNotes.get(layerId);
    if (!active) return;
    active.forEach((count, note) => {
      for (let index = 0; index < count; index += 1) synth.noteOff(note);
    });
    state.looper.activeNotes.delete(layerId);
  }

  function releaseLooperPlayback() {
    [...state.looper.activeNotes.keys()].forEach((layerId) => releaseLooperLayerNotes(layerId));
  }

  function rebuildLooperSchedule() {
    const looper = state.looper;
    looper.schedule = [];
    looper.layers.forEach((layer) => {
      if (layer.muted) return;
      layer.events.forEach((event) => looper.schedule.push({ ...event, layerId: layer.id }));
    });
    looper.schedule.sort((left, right) => left.timeMs - right.timeMs || (left.kind === 'noteOff' ? -1 : 1));
    if (!looper.playing || !looper.loopDurationMs) {
      looper.playIndex = 0;
      return;
    }
    const position = looperPosition();
    looper.playIndex = looper.schedule.findIndex((event) => event.timeMs > position);
    if (looper.playIndex < 0) looper.playIndex = looper.schedule.length;
  }

  function playLooperEvent(event) {
    const tone = Number.isInteger(event.tone) ? event.tone : state.settings.tone;
    const active = state.looper.activeNotes.get(event.layerId) || new Map();
    if (event.kind === 'noteOn') {
      synth.noteOn(event.note, event.velocity || state.settings.velocity, tone);
      active.set(event.note, (active.get(event.note) || 0) + 1);
      state.looper.activeNotes.set(event.layerId, active);
    } else if (event.kind === 'noteOff') {
      synth.noteOff(event.note);
      const count = active.get(event.note) || 0;
      if (count <= 1) active.delete(event.note);
      else active.set(event.note, count - 1);
      if (!active.size) state.looper.activeNotes.delete(event.layerId);
    }
  }

  function finishLooperCapture() {
    const looper = state.looper;
    if (looper.mode !== 'recording' && looper.mode !== 'overdubbing') return;
    const boundary = looper.recordStartAt + looper.loopDurationMs;
    looper.capturedHeld.forEach((count, note) => {
      for (let index = 0; index < count; index += 1) appendLooperEvent({ kind: 'noteOff', note }, looper.loopDurationMs);
    });
    const hasNotes = looper.recordEvents.some((event) => event.kind === 'noteOn');
    if (hasNotes) {
      looper.layers.push({ id: looper.nextLayerId++, muted: false, events: looper.recordEvents.map((event) => ({ ...event })) });
      looper.revision += 1;
    }
    looper.recordEvents = [];
    looper.capturedHeld.clear();
    if (!looper.layers.length) {
      looper.mode = 'idle';
      looper.playing = false;
      looper.loopDurationMs = 0;
      syncUI();
      return;
    }
    looper.mode = 'playing';
    looper.playing = true;
    looper.cycleStartAt = boundary;
    looper.playIndex = 0;
    rebuildLooperSchedule();
    looper.playIndex = 0;
    syncUI();
  }

  function startLooperPlayback() {
    const looper = state.looper;
    if (!looper.layers.length) return;
    looper.bpm = state.settings.bpm;
    if (!looper.loopDurationMs) looper.loopDurationMs = looperBarDurationMs();
    looper.playing = true;
    looper.mode = 'playing';
    looper.cycleStartAt = performance.now();
    looper.playIndex = 0;
    rebuildLooperSchedule();
    looper.playIndex = 0;
    syncUI();
  }

  async function startLooperAction() {
    const looper = state.looper;
    if (looper.mode === 'recording' || looper.mode === 'overdubbing') return;
    if (!synth.context || synth.context.state !== 'running') await enableAudio();
    if (!synth.context || synth.context.state !== 'running') return;
    if (looper.mode === 'armed') {
      looper.mode = 'playing';
      looper.recordAnchorAt = 0;
      looper.recordEvents = [];
      looper.capturedHeld.clear();
      syncUI();
      return;
    }
    if (looper.playing) {
      const now = performance.now();
      const position = looperPosition(now);
      const untilBoundary = looper.loopDurationMs - position || looper.loopDurationMs;
      looper.mode = 'armed';
      looper.recordAnchorAt = now + untilBoundary;
      looper.recordEvents = [];
      looper.capturedHeld.clear();
      syncUI();
      return;
    }
    if (looper.layers.length) return startLooperPlayback();
    looper.bpm = state.settings.bpm;
    looper.loopDurationMs = looperBarDurationMs();
    beginLooperCapture(performance.now());
  }

  function stopLooper() {
    const looper = state.looper;
    releaseLooperPlayback();
    looper.playing = false;
    looper.mode = 'idle';
    looper.recordEvents = [];
    looper.capturedHeld.clear();
    looper.recordAnchorAt = 0;
    looper.nextLayerId = Math.max(1, ...looper.layers.map((layer) => layer.id + 1));
    syncUI();
  }

  function clearLooperLayers() {
    stopLooper();
    state.looper.layers = [];
    state.looper.schedule = [];
    state.looper.loopDurationMs = 0;
    state.looper.revision += 1;
    syncUI();
  }

  function toggleLooperLayer(layerId) {
    const layer = state.looper.layers.find((candidate) => candidate.id === Number(layerId));
    if (!layer) return;
    layer.muted = !layer.muted;
    if (layer.muted) releaseLooperLayerNotes(layer.id);
    state.looper.revision += 1;
    rebuildLooperSchedule();
    syncUI();
  }

  function removeLooperLayer(layerId) {
    const id = Number(layerId);
    releaseLooperLayerNotes(id);
    state.looper.layers = state.looper.layers.filter((layer) => layer.id !== id);
    state.looper.revision += 1;
    if (!state.looper.layers.length) {
      stopLooper();
      state.looper.loopDurationMs = 0;
    } else {
      rebuildLooperSchedule();
      syncUI();
    }
  }

  function serviceLooper(now) {
    const looper = state.looper;
    if (looper.mode === 'armed' && now >= looper.recordAnchorAt) beginLooperCapture(looper.recordAnchorAt);
    if ((looper.mode === 'recording' || looper.mode === 'overdubbing') && now >= looper.recordStartAt + looper.loopDurationMs) finishLooperCapture();
    if (!looper.playing || !looper.loopDurationMs) return;
    while (now >= looper.cycleStartAt + looper.loopDurationMs) {
      releaseLooperPlayback();
      looper.cycleStartAt += looper.loopDurationMs;
      looper.playIndex = 0;
    }
    const position = looperPosition(now);
    while (looper.playIndex < looper.schedule.length && looper.schedule[looper.playIndex].timeMs <= position) playLooperEvent(looper.schedule[looper.playIndex++]);
    syncLooperProgress(now);
  }

  function startBinding(keyId, notes) {
    if (!notes.length) return;
    if (state.sampler.enabled) {
      const samplerIndex = sliceForKey(keyId, SAMPLER_KEY_IDS.slice(0, state.sampler.sliceCount));
      if (samplerIndex < 0) return;
      state.bindings.set(keyId, []);
      playSamplerSlice(samplerIndex);
      syncUI();
      return;
    }
    let playableNotes = notes;
    if (state.chordAssist.enabled && !state.arp.enabled) {
      const root = notes[0];
      playableNotes = buildChordVoicing(root, state.chordAssist.type, state.chordAssist.bass && !state.chordAssist.pulse);
      state.chordAssist.held.set(keyId, root);
      if (!state.chordAssist.nextAt) state.chordAssist.nextAt = performance.now();
    }
    state.bindings.set(keyId, playableNotes);
    if (state.arp.enabled) addArpNotes(playableNotes);
    else playableNotes.forEach((note) => {
      synth.noteOn(note, state.settings.velocity, state.settings.tone);
      const event = { kind: 'noteOn', note, velocity: state.settings.velocity };
      captureLooperEvent(event);
      if (state.recording?.active) recordEvent(event);
    });
    syncUI();
  }

  function releaseBinding(keyId) {
    const notes = state.bindings.get(keyId);
    if (!notes) return;
    if (state.chordAssist.held.has(keyId)) {
      state.chordAssist.held.delete(keyId);
      if (!state.chordAssist.held.size) state.chordAssist.nextAt = 0;
    }
    if (state.arp.enabled) removeArpNotes(notes);
    else notes.forEach((note) => {
      synth.noteOff(note);
      const event = { kind: 'noteOff', note };
      captureLooperEvent(event);
      if (state.recording?.active) recordEvent(event);
    });
    state.bindings.delete(keyId);
    syncUI();
  }

  function releaseAllBindings(record = false) {
    const previous = state.recording?.active;
    if (!record && previous) state.recording.active = false;
    [...state.bindings.keys()].forEach((keyId) => releaseBinding(keyId));
    if (!record && previous) state.recording.active = true;
    stopArpNote(false);
    state.arp.held.clear();
    state.arp.nextAt = 0;
    state.arp.stepIndex = 0;
    state.chordAssist.held.clear();
    state.chordAssist.nextAt = 0;
    synth.allNotesOff(true);
    state.bindings.clear();
  }

  function panic() {
    releaseAllBindings(true);
    stopSamplerSources();
    synth.allNotesOff(true);
    setNotice(t('silence'));
    syncUI();
  }

  function handleShiftControl(keyId, repeat) {
    const noRepeat = new Set([18, 20, 13, 26, 25, 21, 27, 29, 28, 23]);
    if (repeat && noRepeat.has(keyId)) return;
    if (keyId === 18) setTone(state.settings.tone - 1);
    else if (keyId === 20) setTone(state.settings.tone + 1);
    else if (keyId === 12) setSetting('octave', state.settings.octave - 1);
    else if (keyId === 14) setSetting('octave', state.settings.octave + 1);
    else if (keyId === 19) setSetting('transpose', state.settings.transpose - 1);
    else if (keyId === 7) setSetting('transpose', state.settings.transpose + 1);
    else if (keyId === 1) setSetting('root', state.settings.root - 1);
    else if (keyId === 2) setSetting('root', state.settings.root + 1);
    else if (keyId === 3) setSetting('volume', state.settings.volume - 1);
    else if (keyId === 9) setSetting('volume', state.settings.volume + 1);
    else if (keyId === 6) setSetting('bpm', state.settings.bpm - 5);
    else if (keyId === 8) setSetting('bpm', state.settings.bpm + 5);
    else if (keyId === 15) setSetting('velocity', state.settings.velocity - 8);
    else if (keyId === 22) setSetting('velocity', state.settings.velocity + 8);
    else if (keyId === 13) setSustain(!state.settings.sustain);
    else if (keyId === 26) setMetronome(!state.settings.metronome);
    else if (keyId === 25) toggleRecording();
    else if (keyId === 27) clearTake();
    else if (keyId === 29) setLayoutByStep(1);
    else if (keyId === 28) setPage('settings');
    else if (keyId === 23 || keyId === 0) panic();
  }

  function setLayoutByStep(step) {
    const current = LAYOUT_NAMES.findIndex((name) => name.toLowerCase() === state.settings.layout);
    const index = (current + step + LAYOUT_NAMES.length) % LAYOUT_NAMES.length;
    state.settings.layout = LAYOUT_NAMES[index].toLowerCase();
    syncUI();
  }

  function handleKeyPress(record) {
    const id = Number(record.key_id);
    if (!Number.isInteger(id) || id < 0 || id >= KEY_NAMES.length) return;
    const repeat = Number(record.action_id) === 3 || record.action === 'REPEAT';
    const isNew = !state.pressed.has(id);
    state.pressed.add(id);
    if (isNew) handleCalibrationTap(id, record);
    if (isNew) handleLessonKeyPress(id);
    if (isNew) handleChallengeKeyPress(id);
    if (id === 24) return syncUI();
    const shiftHeld = state.pressed.has(24);
    if (shiftHeld) {
      if (state.lesson.active) return syncUI();
      if (isNew || repeat) handleShiftControl(id, repeat);
      return syncUI();
    }
    if (repeat || !isNew) return syncUI();
    if (state.lesson.active && [28, 29].includes(id) && !currentLessonStep()?.key_ids.includes(id)) return syncUI();
    if (id === 0 || id === 27) return panic();
    if (id === 29) {
      const current = Math.max(0, PAGE_ORDER.indexOf(state.page));
      return setPage(PAGE_ORDER[(current + 1) % PAGE_ORDER.length]);
    }
    if (id === 28) return setSustain(!state.settings.sustain);
    if (id === 24) return;
    const notes = keyToNotes(id, { ...state.settings }, state.pressed.has(21));
    startBinding(id, notes);
  }

  function handleKeyRelease(record) {
    const id = Number(record.key_id);
    if (!Number.isInteger(id)) return;
    releaseBinding(id);
    state.pressed.delete(id);
    syncUI();
  }

  function reconcileMask(mask) {
    const next = new Set();
    for (let id = 0; id < KEY_NAMES.length; id += 1) if (Number(mask) & (1 << id)) next.add(id);
    [...state.pressed].filter((id) => !next.has(id)).forEach((id) => releaseBinding(id));
    state.pressed = next;
    syncUI();
  }

  function handleSerialRecord(record) {
    state.packets += 1;
    if (record.type === 'device_info') {
      state.device = record;
      state.serialStatus = 'connected';
      setNotice(t('deviceReady'));
    } else if (record.type === 'key') {
      if (Number(record.action_id) === 2 || record.action === 'RELEASE') handleKeyRelease(record);
      else handleKeyPress(record);
      if (record.state_mask !== undefined) reconcileMask(record.state_mask);
    } else if (record.type === 'state') {
      state.dropped = Number(record.dropped_events) || 0;
      reconcileMask(record.state_mask || 0);
    }
    syncUI();
  }

  function recordEvent(event) {
    if (!state.recording?.active) return;
    if (state.recording.events.length >= MAX_RECORDED_EVENTS) {
      state.recording.full = true;
      return;
    }
    const timeMs = Math.max(0, Math.round(performance.now() - state.recording.startedAt - state.settings.latencyMs));
    state.recording.events.push({ ...event, timeMs });
    if (event.kind === 'noteOn') state.recording.hasNotes = true;
    renderEventTable();
  }

  function startRecording() {
    stopPlayback();
    releaseAllBindings(false);
    state.take = null;
    state.recording = { active: true, startedAt: performance.now(), durationMs: 0, events: [], hasNotes: false, full: false, initialBpm: state.settings.bpm, initialTone: state.settings.tone, initialProgram: GM_PROGRAMS[state.settings.tone] };
    setPage('rec');
    setNotice(t('recording'));
    renderEventTable();
    syncUI();
  }

  async function stopRecording() {
    if (!state.recording?.active) return;
    releaseAllBindings(true);
    if (state.settings.sustain) setSustain(false);
    state.recording.active = false;
    state.recording.durationMs = Math.max(0, Math.round(performance.now() - state.recording.startedAt));
    if (state.recording.hasNotes) {
      state.take = { ...state.recording, events: state.recording.events.map((event) => ({ ...event })) };
      setNotice(state.recording.full ? t('full') : t('takeInMemory'));
      await saveCurrentTake(true);
    } else {
      setNotice(t('noTake'));
    }
    syncUI();
  }

  function toggleRecording() {
    if (state.recording?.active) stopRecording();
    else startRecording();
  }

  function clearTake() {
    if (state.recording?.active) {
      releaseAllBindings(false);
      state.recording = null;
    }
    stopPlayback();
    state.take = null;
    setNotice(t('noTake'));
    renderEventTable();
    syncUI();
  }

  function playbackEvent(event) {
    if (event.kind === 'noteOn') synth.noteOn(event.note, event.velocity || 100, state.settings.tone);
    else if (event.kind === 'noteOff') synth.noteOff(event.note);
    else if (event.kind === 'program') {
      const tone = TONE_NAMES.findIndex((_, index) => GM_PROGRAMS[index] === event.program);
      if (tone >= 0) setTone(tone, false);
    }
    else if (event.kind === 'sustain') { state.settings.sustain = Boolean(event.enabled); synth.setSustain(event.enabled); }
  }

  function startPlayback() {
    if (!state.take || !state.take.hasNotes) return setNotice(t('noTakeToPlay'));
    if (state.recording?.active) stopRecording();
    stopPlayback();
    releaseAllBindings(false);
    synth.allNotesOff(true);
    state.settings.tone = state.take.initialTone;
    state.playback = { startedAt: performance.now(), index: 0, take: state.take };
    setPage('rec');
    setNotice(t('playing'));
    syncUI();
  }

  function stopPlayback() {
    if (!state.playback) return;
    state.playback = null;
    synth.allNotesOff(true);
    setNotice(t('ready'));
    syncUI();
  }

  function servicePlayback(now) {
    const playback = state.playback;
    if (!playback) return;
    const elapsed = now - playback.startedAt;
    while (playback.index < playback.take.events.length && playback.take.events[playback.index].timeMs <= elapsed) playbackEvent(playback.take.events[playback.index++]);
    if (playback.index >= playback.take.events.length && elapsed >= playback.take.durationMs + 900) stopPlayback();
  }

  async function saveCurrentTake(autoSave = false) {
    if (!state.take || state.saving) return;
    state.saving = true;
    const index = state.saveIndex % 1000;
    const fileName = `KEYS${String(index).padStart(3, '0')}.MID`;
    const midi = encodeTake(state.take);
    try {
      const result = await window.musicKeyboard.saveMidi(fileName, Array.from(midi));
      if (result?.ok && !result.canceled) {
        state.take.savedFile = result.path;
        state.saveIndex = (index + 1) % 1000;
        localStorage.setItem('musicKeyboardSaveIndex', String(state.saveIndex));
        setNotice(`${t('saved')}: ${result.fileName || fileName}`);
      } else if (result?.canceled) {
        setNotice(t('canceled'));
      } else if (result?.error) {
        setNotice(`${t('midiError')}: ${result.error}`);
      }
    } catch (error) {
      setNotice(`${t('midiError')}: ${error.message}`);
    } finally {
      state.saving = false;
      syncUI();
    }
  }

  function renderEventTable() {
    const table = qs('#eventTable');
    const events = state.recording?.events || state.take?.events || [];
    const visible = events.slice(-18).reverse();
    if (!visible.length) {
      table.innerHTML = `<div class="empty-state">${t('noEvents')}</div>`;
      return;
    }
    table.innerHTML = visible.map((event) => {
      const detail = event.kind === 'noteOn' || event.kind === 'noteOff' ? noteName(event.note) : event.kind === 'program' ? toneLabel(GM_PROGRAMS.indexOf(event.program)) : event.kind === 'tempo' ? `${event.bpm} BPM` : (event.enabled ? t('sustain') : t('silence'));
      return `<div class="event-row"><span>${formatTime(event.timeMs)}</span><strong>${event.kind}</strong><span>${detail}</span><span>${event.velocity || ''}</span></div>`;
    }).join('');
  }

  function syncStatusDot(dot, status) {
    dot.className = `status-dot ${status === 'connected' ? 'connected' : status === 'connecting' ? 'connecting' : status === 'error' ? 'error' : ''}`;
  }

  function syncLessonUI() {
    const lesson = state.lesson;
    const data = lesson.data;
    const step = currentLessonStep();
    const total = data?.steps?.length || 0;
    const position = total ? lesson.index + 1 : 0;
    const progress = lesson.completed ? 100 : total ? lesson.index / total * 100 : 0;
    const keyNames = lessonKeyNames(step);
    const noteNames = step?.notes?.map((note) => note.name) || [];

    qs('#lessonFileName').textContent = lesson.name || t('noMidiSelected');
    qs('#lessonFilePath').textContent = lesson.path || t('lessonChooseHint');
    qs('#lessonComplexity').value = lesson.complexity;
    qs('#lessonComplexity').disabled = lesson.loading;
    qs('#lessonComplexityHint').textContent = lessonComplexityHint();
    qs('#lessonStartButton').disabled = lesson.loading || !data || lesson.active;
    qs('#lessonRestartButton').disabled = lesson.loading || !data;
    qs('#lessonMetrics').hidden = !data;
    qs('#lessonPreview').hidden = !data;
    qs('#lessonLive').hidden = !lesson.active || !step;

    if (lesson.loading) qs('#lessonStatus').textContent = t('lessonLoading');
    else if (lesson.error) qs('#lessonStatus').textContent = `${t('lessonError')}: ${lesson.error}`;
    else if (lesson.completed) qs('#lessonStatus').textContent = t('lessonComplete');
    else if (data) qs('#lessonStatus').textContent = `${t('lessonReady')} · ${data.step_count} ${t('lessonSteps').toLowerCase()}`;
    else qs('#lessonStatus').textContent = t('lessonChooseHint');

    if (!data) return;
    qs('#lessonStepCount').textContent = String(data.step_count);
    qs('#lessonTrack').textContent = `#${data.track} ${data.track_name}`;
    qs('#lessonRange').textContent = `${ROOT_NAMES[data.root]}${data.octave} / MIDI ${data.base_note}`;
    qs('#lessonTempo').textContent = `${data.bpm} BPM / ${data.time_signature}`;
    qs('#lessonPreviewNotes').textContent = noteNames.join(' + ') || '-';
    qs('#lessonPreviewIndex').textContent = `${position} / ${total}`;
    qs('#lessonPreviewBar').style.width = `${progress}%`;
    qs('#lessonMistakes').textContent = `${lesson.mistakes} ${t('mistakes')}`;
    qs('#lessonKeyPreview').innerHTML = keyNames.map((name, index) => {
      const id = step.key_ids[index];
      const hit = lesson.matched.has(id) ? ' hit' : '';
      return `<span class="lesson-preview-key${hit}">${name}</span>`;
    }).join('');
    qs('#lessonLiveTitle').textContent = `${position} / ${total} · ${noteNames.join(' + ')}`;
    qs('#lessonLiveKeys').textContent = `${step.requires_enter ? `${t('enterFirst')} · ` : ''}${keyNames.join(' + ')}`;
    qs('#lessonLiveProgress').textContent = `${Math.round(progress)}%`;
    qs('#lessonLiveBar').style.width = `${progress}%`;
    qs('#lessonLivePrev').disabled = lesson.index <= 0;
    qs('#lessonLiveNext').disabled = lesson.index >= total - 1;
  }

  function syncRenderLoad() {
    const load = clamp(state.frameCost / (1000 / 60) * 100, 0, 100);
    qs('#loadStat').textContent = `${Math.round(load)}%`;
    qs('#loadBar').style.width = `${load}%`;
  }

  function syncSoundLabUI() {
    qs('#soundLabSmooth').value = String(state.soundLab.smooth);
    qs('#soundLabGain').value = String(state.soundLab.gain);
  }

  function syncUI() {
    const settings = state.settings;
    const activeVoices = synth.voices.filter((voice) => !voice.ended);
    const base = keyboardBaseNote(settings);
    const lessonStep = state.lesson.active ? currentLessonStep() : null;
    const challengeStep = state.challenge.active && state.challenge.phase === 'input' ? currentChallengeStep() : null;
    const targetStep = challengeStep || lessonStep;
    const targetMatched = challengeStep ? state.challenge.matched : state.lesson.matched;
    const lessonTargets = new Set(targetStep?.key_ids || []);
    const statusText = state.serialStatus === 'connected' ? t('connected') : state.serialStatus === 'connecting' ? t('connecting') : state.serialStatus === 'error' ? t('serialError') : t('disconnected');
    qs('#toneMetric').textContent = toneLabel(settings.tone);
    qs('#rangeMetric').textContent = `${noteName(base)} / ${signed(settings.transpose)}`;
    qs('#activeMetric').textContent = `${new Set(activeVoices.map((voice) => voice.note)).size} / ${MAX_VOICES}`;
    qs('#recordMetric').textContent = state.recording?.active ? t('recording') : state.playback ? t('playing') : state.take ? t('takeInMemory') : t('ready');
    qs('#layoutChip').textContent = layoutLabel(settings.layout);
    qs('#visualKicker').textContent = state.visualMode === 'calculator' ? t('calculatorViewKicker') : t('pianoRoll');
    qs('#visualHeading').textContent = state.visualMode === 'calculator' ? t('calculatorPreview') : t('twoOctaveRange');
    qs('#pianoCanvas').hidden = state.visualMode !== 'piano';
    qs('#calculatorVisual').hidden = state.visualMode !== 'calculator';
    qsa('.view-option').forEach((button) => {
      const selected = button.dataset.visualMode === state.visualMode;
      button.classList.toggle('active', selected);
      button.setAttribute('aria-selected', String(selected));
    });
    const activeNames = [...new Set(activeVoices.map((voice) => noteName(voice.note)))];
    qs('#noteReadout').textContent = activeNames.length ? activeNames.slice(0, 5).join('  ') : targetStep ? `${state.challenge.active ? t('challengeInput') : t('nextStep')}: ${lessonKeyNames(targetStep).join(' + ')}` : t('ready');
    qs('#calculatorScreenTone').textContent = toneLabel(settings.tone).toUpperCase();
    qs('#calculatorScreenNotes').textContent = activeNames.length ? activeNames.slice(0, 5).join('  ') : targetStep ? lessonKeyNames(targetStep).join(' + ') : t('ready').toUpperCase();
    qs('#calculatorScreenMeta').textContent = state.challenge.active ? `${t('challenge').toUpperCase()} ${state.challenge.round} / ${state.challenge.phrase.length}` : lessonStep ? `${t('lessonSteps').toUpperCase()} ${state.lesson.index + 1}/${state.lesson.data.steps.length}` : `${layoutLabel(settings.layout).toUpperCase()} / ${noteName(base)} / ${signed(settings.transpose)}`;
    qs('#packetReadout').textContent = `${state.packets} ${t('packets')}${state.dropped ? ` / ${state.dropped} dropped` : ''}`;
    qs('#voiceStat').textContent = `${activeVoices.length} / ${MAX_VOICES}`;
    qs('#voiceBar').style.width = `${Math.min(100, activeVoices.length / MAX_VOICES * 100)}%`;
    const peak = synth.peak();
    qs('#peakStat').textContent = `${Math.round(peak * 100)}%`;
    qs('#peakBar').style.width = `${Math.round(peak * 100)}%`;
    syncRenderLoad();
    qs('#scopeReadout').textContent = `${activeVoices.length} ${t('voices').toLowerCase()}`;
    qs('#scopeMode').textContent = t('scopeAudio');
    qs('#scopeRateSelect').value = String(state.scopeRate);
    qs('#baseNoteChip').textContent = `MIDI ${base}`;
    qs('#volumeValue').textContent = settings.volume;
    qs('#velocityValue').textContent = settings.velocity;
    qs('#octaveValue').textContent = settings.octave;
    qs('#rootValue').textContent = ROOT_NAMES[settings.root];
    qs('#transposeValue').textContent = signed(settings.transpose);
    qs('#bpmValue').textContent = settings.bpm;
    qs('#bpmRange').value = settings.bpm;
    qs('#toneSelect').value = settings.tone;
    qs('#layoutSelect').value = settings.layout;
    qs('#sustainToggle').checked = settings.sustain;
    qs('#metronomeToggle').checked = settings.metronome;
    syncSynthUI();
    syncGrooveUI();
    syncChordAssistUI();
    syncSamplerUI();
    syncSoundLabUI();
    syncLooperUI();
    syncChallengeUI();
    const calibration = state.calibration;
    const measuredBeatCount = clamp(calibration.beatIndex - calibration.leadInBeats, 0, calibration.measuredBeats);
    const calibrationProgress = calibration.measuredBeats ? measuredBeatCount / calibration.measuredBeats * 100 : 0;
    qs('#calibrationBpmSelect').value = String(calibration.bpm);
    qs('#calibrationBeatsSelect').value = String(calibration.measuredBeats);
    qs('#calibrationBpmSelect').disabled = calibration.active;
    qs('#calibrationBeatsSelect').disabled = calibration.active;
    qs('#calibrationStartButton').textContent = calibration.active ? t('stopCalibration') : t('startCalibration');
    qs('#calibrationResetButton').disabled = calibration.active;
    qs('#calibrationDownButton').disabled = calibration.active;
    qs('#calibrationUpButton').disabled = calibration.active;
    qs('#calibrationOffsetReadout').textContent = `${signed(settings.latencyMs)} ms`;
    qs('#calibrationOffsetValue').textContent = `${signed(settings.latencyMs)} ms`;
    qs('#calibrationProgressBar').style.width = `${calibration.active ? calibrationProgress : calibration.result ? 100 : 0}%`;
    qs('#calibrationBeatReadout').textContent = `${measuredBeatCount} / ${calibration.measuredBeats}`;
    qs('#calibrationSampleReadout').textContent = `${calibration.samples.length} ${t('samples')}`;
    qs('.calibration-panel').classList.toggle('active', calibration.active);
    if (calibration.active) qs('#calibrationStatus').textContent = calibration.phase === 'countin' ? t('calibrationCountIn') : t('calibrationMeasuring');
    else if (calibration.phase === 'error') qs('#calibrationStatus').textContent = t('calibrationTooFew');
    else if (calibration.result) qs('#calibrationStatus').textContent = `${t('calibrationDone')}: ${signed(calibration.result.offset)} ms · ${t('sampleSpread')} ±${calibration.result.spread} ms`;
    else qs('#calibrationStatus').textContent = t('calibrationReady');
    qs('#recordOrb').className = `record-orb${state.recording?.active ? ' recording' : state.playback ? ' playing' : ''}`;
    qs('#recState').textContent = state.recording?.active ? t('recording') : state.playback ? t('playing') : state.take ? t('takeInMemory') : t('readyToRecord');
    const liveDuration = state.recording?.active ? performance.now() - state.recording.startedAt : state.take?.durationMs || 0;
    qs('#recTime').textContent = formatTime(liveDuration);
    qs('#recProgress').style.width = `${Math.min(100, liveDuration / 600000 * 100)}%`;
    qs('#eventDetail').textContent = `${(state.recording?.events || state.take?.events || []).length} / ${MAX_RECORDED_EVENTS}`;
    qs('#eventCountReadout').textContent = String((state.recording?.events || state.take?.events || []).length);
    qs('#durationDetail').textContent = formatTime(state.take?.durationMs || (state.recording?.active ? liveDuration : 0));
    qs('#initialToneDetail').textContent = state.take ? toneLabel(state.take.initialTone) : '-';
    qs('#initialTempoDetail').textContent = state.take ? `${state.take.initialBpm} BPM` : '-';
    qs('#savedDetail').textContent = state.take?.savedFile || (state.take ? t('takeInMemory') : t('noTake'));
    qs('#recNotice').textContent = state.recording?.full ? t('full') : state.notice || (state.take ? t('takeInMemory') : t('ready'));
    qs('#recordButton').textContent = state.recording?.active ? t('stopRecording') : t('startRecording');
    qs('#playButton').textContent = state.playback ? t('stopPlayback') : t('playTake');
    qs('#playButton').disabled = !state.playback && !state.take;
    qs('#clearButton').disabled = !state.take && !state.recording;
    qs('#saveButton').disabled = !state.take || state.saving;
    qs('#takeChip').textContent = state.take ? t('takeInMemory') : t('noTake');
    qs('#convertMidiButton').disabled = state.converter.busy;
    qs('#deviceName').textContent = state.device ? `${t('deviceReady')} #${Number(state.device.boot_id || 0).toString(16).toUpperCase().padStart(8, '0')}` : t('waitingCalculator');
    qs('#deviceDetail').textContent = state.device ? `FW ${state.device.firmware} / ${state.device.rows} x ${state.device.cols}` : t('deviceDetail');
    qs('#footerStatus').textContent = state.notice || statusText;
    qs('#footerHint').textContent = state.serialStatus === 'connected' ? `${statusText} / ${toneLabel(settings.tone)} / ${layoutLabel(settings.layout)}` : t('serialHint');
    qs('#connectButton').textContent = state.serialStatus === 'connected' || state.serialStatus === 'connecting' ? t('disconnect') : t('connect');
    syncStatusDot(qs('#statusDot'), state.serialStatus);
    syncStatusDot(qs('#footerDot'), state.serialStatus);
    qsa('.physical-key').forEach((element) => {
      const id = Number(element.dataset.keyId);
      element.classList.toggle('down', state.pressed.has(id));
      element.classList.toggle('lesson-target', lessonTargets.has(id));
      element.classList.toggle('lesson-hit', targetMatched.has(id));
    });
    qsa('.calculator-key').forEach((element) => {
      const id = Number(element.dataset.keyId);
      element.classList.toggle('pressed', state.pressed.has(id));
      element.classList.toggle('sounding', state.bindings.has(id));
      element.classList.toggle('shift-active', id === 24 && state.pressed.has(24));
      element.classList.toggle('lesson-target', lessonTargets.has(id));
      element.classList.toggle('lesson-hit', targetMatched.has(id));
    });
    syncLessonUI();
    renderEventTable();
  }

  function drawPiano() {
    const canvas = qs('#pianoCanvas');
    const rect = canvas.getBoundingClientRect();
    if (!rect.width || !rect.height) return;
    const dpr = Math.min(2, window.devicePixelRatio || 1);
    const width = Math.round(rect.width * dpr);
    const height = Math.round(rect.height * dpr);
    if (canvas.width !== width || canvas.height !== height) { canvas.width = width; canvas.height = height; }
    const ctx = canvas.getContext('2d');
    ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
    const w = rect.width;
    const h = rect.height;
    ctx.clearRect(0, 0, w, h);
    ctx.fillStyle = '#10161a'; ctx.fillRect(0, 0, w, h);
    const base = keyboardBaseNote(state.settings);
    const lessonStep = state.lesson.active ? currentLessonStep() : null;
    const challengeStep = state.challenge.active && state.challenge.phase === 'input' ? currentChallengeStep() : null;
    const targetStep = challengeStep || lessonStep;
    const targetPitches = targetStep?.notes?.map((note) => note.pitch) || [];
    const span = targetPitches.length && Math.max(...targetPitches) - Math.min(...targetPitches) > 24 ? 36 : 24;
    let start = clamp(base - (base % 12), 24, 108 - span);
    if (targetPitches.length && Math.max(...targetPitches) > start + span) {
      start = clamp(Math.floor((Math.max(...targetPitches) - span) / 12) * 12, 24, 108 - span);
    }
    if (targetPitches.length && Math.min(...targetPitches) < start) {
      start = clamp(Math.floor(Math.min(...targetPitches) / 12) * 12, 24, 108 - span);
    }
    const end = start + span;
    const whitePcs = [0, 2, 4, 5, 7, 9, 11];
    const blackPcs = new Set([1, 3, 6, 8, 10]);
    const whiteNotes = [];
    for (let midi = start; midi <= end; midi += 1) if (whitePcs.includes(midi % 12)) whiteNotes.push(midi);
    const whiteW = w / whiteNotes.length;
    const active = new Set(synth.voices.filter((voice) => !voice.ended).map((voice) => voice.note));
    const pressedNotes = new Set();
    state.bindings.forEach((notes) => notes.forEach((note) => pressedNotes.add(note)));
    const lessonNotes = new Map();
    targetStep?.notes?.forEach((note) => lessonNotes.set(note.pitch, note.key_id));
    whiteNotes.forEach((midi, index) => {
      const x = index * whiteW;
      const isActive = active.has(midi);
      const isPressed = pressedNotes.has(midi);
      const isTarget = lessonNotes.has(midi);
      const isHit = isTarget && (challengeStep ? state.challenge.matched : state.lesson.matched).has(lessonNotes.get(midi));
      ctx.fillStyle = isPressed ? '#c8534d' : isActive ? '#2c8990' : isHit ? '#a4e7b7' : isTarget ? '#f2c66d' : '#e9efed';
      ctx.fillRect(x + 1, 0, whiteW - 2, h - 1);
      ctx.strokeStyle = '#364348'; ctx.strokeRect(x + .5, .5, whiteW - 1, h - 1);
      ctx.fillStyle = isPressed || isActive ? '#f5fbfa' : isTarget ? '#332b18' : '#56666b';
      ctx.font = '11px Cascadia Mono, Consolas, monospace';
      ctx.fillText(noteName(midi), x + 7, h - 12);
    });
    for (let midi = start; midi < end; midi += 1) {
      if (!blackPcs.has(midi % 12)) continue;
      const nextWhite = whiteNotes.filter((value) => value <= midi).length;
      const x = nextWhite * whiteW - whiteW * .31;
      const bw = whiteW * .62;
      const isActive = active.has(midi); const isPressed = pressedNotes.has(midi);
      const isTarget = lessonNotes.has(midi);
      const isHit = isTarget && (challengeStep ? state.challenge.matched : state.lesson.matched).has(lessonNotes.get(midi));
      ctx.fillStyle = isPressed ? '#ff7869' : isActive ? '#64d9df' : isHit ? '#a4e7b7' : isTarget ? '#f2c66d' : '#202a2f';
      ctx.fillRect(x, 0, bw, h * .59);
      ctx.strokeStyle = '#071014'; ctx.strokeRect(x + .5, .5, bw - 1, h * .59);
    }
    ctx.fillStyle = '#f2c66d';
    const rootPc = (state.settings.root + start) % 12;
    whiteNotes.forEach((midi, index) => { if (midi % 12 === state.settings.root) ctx.fillRect(index * whiteW + 4, h - 5, whiteW - 8, 2); });
    void rootPc;
  }

  function drawScope() {
    const canvas = qs('#scopeCanvas');
    const rect = canvas.getBoundingClientRect();
    if (!rect.width || !rect.height) return;
    const peak = synth.peak();
    state.scopeDisplayPeak = state.scopeDisplayPeak ? state.scopeDisplayPeak * .82 + peak * .18 : peak;
    const displayGain = clamp(.66 / Math.max(state.scopeDisplayPeak, .025), 1, 12);
    qs('#peakStat').textContent = `${Math.round(peak * 100)}%`;
    qs('#peakBar').style.width = `${Math.round(peak * 100)}%`;
    const dpr = Math.min(2, window.devicePixelRatio || 1);
    const width = Math.round(rect.width * dpr); const height = Math.round(rect.height * dpr);
    if (canvas.width !== width || canvas.height !== height) { canvas.width = width; canvas.height = height; }
    const ctx = canvas.getContext('2d'); ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
    const w = rect.width; const h = rect.height;
    ctx.fillStyle = '#10161a'; ctx.fillRect(0, 0, w, h);
    ctx.strokeStyle = 'rgba(100,217,223,.10)'; ctx.lineWidth = 1;
    for (let x = 0; x < w; x += 40) { ctx.beginPath(); ctx.moveTo(x, 0); ctx.lineTo(x, h); ctx.stroke(); }
    for (let y = 20; y < h; y += 40) { ctx.beginPath(); ctx.moveTo(0, y); ctx.lineTo(w, y); ctx.stroke(); }
    ctx.strokeStyle = 'rgba(242,198,109,.32)'; ctx.beginPath(); ctx.moveTo(0, h / 2); ctx.lineTo(w, h / 2); ctx.stroke();
    ctx.strokeStyle = '#64d9df'; ctx.lineWidth = 1.6; ctx.beginPath();
    if (synth.waveform) {
      const pointCount = Math.min(synth.waveform.length, Math.max(2, Math.floor(w)));
      const lastSample = synth.waveform.length - 1;
      for (let index = 0; index < pointCount; index += 1) {
        const sampleIndex = Math.round(index / (pointCount - 1) * lastSample);
        const sample = synth.waveform[sampleIndex];
        const x = index / (pointCount - 1) * w;
        const normalized = clamp((sample - 128) / 128 * displayGain, -.48, .48);
        const y = h / 2 + normalized * h;
        if (index === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y);
      }
    } else { ctx.moveTo(0, h / 2); ctx.lineTo(w, h / 2); }
    ctx.stroke();
  }

  function drawSoundLab() {
    const waveCanvas = qs('#soundLabWaveCanvas');
    const spectrumCanvas = qs('#soundLabSpectrumCanvas');
    const waveRect = waveCanvas.getBoundingClientRect();
    const spectrumRect = spectrumCanvas.getBoundingClientRect();
    if (!waveRect.width || !waveRect.height || !spectrumRect.width || !spectrumRect.height) return;
    const dpr = Math.min(2, window.devicePixelRatio || 1);
    const drawGrid = (ctx, width, height) => {
      ctx.fillStyle = '#10161a'; ctx.fillRect(0, 0, width, height);
      ctx.strokeStyle = 'rgba(100,217,223,.10)'; ctx.lineWidth = 1;
      for (let x = 0; x < width; x += 40) { ctx.beginPath(); ctx.moveTo(x, 0); ctx.lineTo(x, height); ctx.stroke(); }
      for (let y = 20; y < height; y += 40) { ctx.beginPath(); ctx.moveTo(0, y); ctx.lineTo(width, y); ctx.stroke(); }
      ctx.strokeStyle = 'rgba(242,198,109,.32)'; ctx.beginPath(); ctx.moveTo(0, height / 2); ctx.lineTo(width, height / 2); ctx.stroke();
    };
    const waveWidth = waveRect.width; const waveHeight = waveRect.height;
    const wavePixelWidth = Math.round(waveWidth * dpr); const wavePixelHeight = Math.round(waveHeight * dpr);
    if (waveCanvas.width !== wavePixelWidth || waveCanvas.height !== wavePixelHeight) { waveCanvas.width = wavePixelWidth; waveCanvas.height = wavePixelHeight; }
    const waveCtx = waveCanvas.getContext('2d'); waveCtx.setTransform(dpr, 0, 0, dpr, 0, 0); drawGrid(waveCtx, waveWidth, waveHeight);
    const waveform = synth.waveform;
    const pointCount = waveform ? Math.min(waveform.length, Math.max(2, Math.floor(waveWidth))) : 0;
    const sampleAt = (index) => ((waveform?.[clamp(index, 0, (waveform?.length || 1) - 1)] || 128) - 128) / 128;
    const radius = Math.max(0, Math.floor((Number(state.soundLab.smooth) || 1) - 1) / 2);
    const gain = clamp(Number(state.soundLab.gain) || 1, .5, 6);
    const drawTrace = (color, filtered) => {
      waveCtx.strokeStyle = color; waveCtx.lineWidth = filtered ? 1.5 : 1.05; waveCtx.beginPath();
      if (!pointCount) { waveCtx.moveTo(0, waveHeight / 2); waveCtx.lineTo(waveWidth, waveHeight / 2); }
      else for (let index = 0; index < pointCount; index += 1) {
        const sampleIndex = Math.round(index / (pointCount - 1) * (waveform.length - 1));
        let value = sampleAt(sampleIndex);
        if (filtered && radius > 0) {
          let sum = 0; let count = 0;
          for (let offset = -radius; offset <= radius; offset += 1) { sum += sampleAt(sampleIndex + offset); count += 1; }
          value = sum / count;
        }
        const x = index / (pointCount - 1) * waveWidth;
        const y = waveHeight / 2 - clamp(value * gain, -.48, .48) * waveHeight;
        if (index === 0) waveCtx.moveTo(x, y); else waveCtx.lineTo(x, y);
      }
      waveCtx.stroke();
    };
    drawTrace('rgba(100,217,223,.55)', false);
    drawTrace('#f2c66d', true);

    const spectrumWidth = spectrumRect.width; const spectrumHeight = spectrumRect.height;
    const spectrumPixelWidth = Math.round(spectrumWidth * dpr); const spectrumPixelHeight = Math.round(spectrumHeight * dpr);
    if (spectrumCanvas.width !== spectrumPixelWidth || spectrumCanvas.height !== spectrumPixelHeight) { spectrumCanvas.width = spectrumPixelWidth; spectrumCanvas.height = spectrumPixelHeight; }
    const spectrumCtx = spectrumCanvas.getContext('2d'); spectrumCtx.setTransform(dpr, 0, 0, dpr, 0, 0); drawGrid(spectrumCtx, spectrumWidth, spectrumHeight);
    if (synth.analyser && synth.frequency) synth.analyser.getByteFrequencyData(synth.frequency);
    const frequency = synth.frequency;
    const bins = frequency ? Math.min(frequency.length, Math.max(8, Math.floor(spectrumWidth / 3))) : 0;
    const barWidth = bins ? spectrumWidth / bins : spectrumWidth;
    spectrumCtx.fillStyle = '#c8534d';
    for (let index = 0; index < bins; index += 1) {
      const sourceIndex = Math.min(frequency.length - 1, Math.floor(index / bins * frequency.length));
      const height = clamp((frequency[sourceIndex] / 255) * gain, 0, 1) * (spectrumHeight - 8);
      spectrumCtx.fillRect(index * barWidth + 1, spectrumHeight - height, Math.max(1, barWidth - 2), height);
    }
  }

  function animationFrame(now) {
    const workStarted = performance.now();
    serviceLooper(now);
    serviceGroove(now);
    serviceArpeggiator(now);
    serviceChordAssist(now);
    serviceChallenge(now);
    let scopeDrawn = false;
    servicePlayback(now);
    if (state.recording?.active) syncUI();
    if (state.page === 'keys' && state.visualMode === 'piano') drawPiano();
    if (state.page === 'scope' && (!state.lastScopeDraw || now - state.lastScopeDraw >= 1000 / state.scopeRate)) {
      state.lastScopeDraw = now;
      drawScope();
      drawSoundLab();
      scopeDrawn = true;
    }
    const workMs = performance.now() - workStarted;
    state.frameCost = state.frameCost * .88 + Math.min(32, workMs) * .12;
    if (scopeDrawn) syncRenderLoad();
    requestAnimationFrame(animationFrame);
  }

  async function refreshPorts() {
    const select = qs('#portSelect');
    const result = await window.musicKeyboard.listPorts();
    select.innerHTML = '';
    if (!result?.ports?.length) {
      const option = document.createElement('option'); option.value = ''; option.textContent = t('noPort'); select.appendChild(option);
      if (result?.error) setNotice(result.error);
      return;
    }
    result.ports.forEach((port) => {
      const option = document.createElement('option'); option.value = port.device; option.textContent = `${port.device} - ${port.description || 'Serial device'}`; select.appendChild(option);
    });
    setNotice(`${result.ports.length} serial port${result.ports.length === 1 ? '' : 's'} found`);
  }

  async function toggleConnection() {
    if (state.serialStatus === 'connected' || state.serialStatus === 'connecting') {
      await window.musicKeyboard.disconnect();
      state.serialStatus = 'disconnected';
      panic();
      return;
    }
    const port = qs('#portSelect').value;
    if (!port) return setNotice(t('noPort'));
    await enableAudio();
    const result = await window.musicKeyboard.connect(port);
    if (!result?.ok) {
      state.serialStatus = 'error';
      setNotice(result?.error || t('serialError'));
    } else {
      state.serialStatus = 'connecting';
      setNotice(t('connecting'));
    }
    syncUI();
  }

  function setupEvents() {
    qs('#languageSelect').addEventListener('change', (event) => {
      state.language = event.target.value;
      localStorage.setItem('musicKeyboardLanguage', state.language);
      applyTranslations();
    });
    qs('#audioButton').addEventListener('click', enableAudio);
    qs('#refreshPorts').addEventListener('click', refreshPorts);
    qs('#connectButton').addEventListener('click', toggleConnection);
    qs('#panicButton').addEventListener('click', panic);
    qsa('.view-option').forEach((button) => button.addEventListener('click', () => setVisualMode(button.dataset.visualMode)));
    qsa('.page-tab').forEach((button) => button.addEventListener('click', () => setPage(button.dataset.page)));
    qs('#chooseLessonMidiButton').addEventListener('click', chooseLessonMidi);
    qs('#lessonComplexity').addEventListener('change', async (event) => {
      state.lesson.complexity = event.target.value;
      state.lesson.error = '';
      if (state.lesson.path) await buildLesson();
      else syncUI();
    });
    qs('#lessonStartButton').addEventListener('click', () => startLesson(false));
    qs('#lessonRestartButton').addEventListener('click', () => startLesson(true));
    qs('#lessonLivePrev').addEventListener('click', () => moveLesson(-1));
    qs('#lessonLiveNext').addEventListener('click', () => moveLesson(1));
    qs('#lessonStopButton').addEventListener('click', stopLesson);
    qs('#challengeDifficulty').addEventListener('change', (event) => {
      state.challenge.difficulty = normalizeDifficulty(event.target.value);
      if (state.challenge.active) startChallenge(true);
      else syncUI();
    });
    qs('#challengeStartButton').addEventListener('click', () => startChallenge(true));
    qs('#challengeStopButtonPage').addEventListener('click', () => stopChallenge());
    qs('#challengeLiveRestart').addEventListener('click', () => startChallenge(true));
    qs('#challengeStopButton').addEventListener('click', () => stopChallenge());
    qsa('.step-button').forEach((button) => button.addEventListener('click', () => {
      const name = button.dataset.setting; const step = Number(button.dataset.step);
      if (!name) return;
      setSetting(name, state.settings[name] + step);
    }));
    qs('#toneSelect').addEventListener('change', (event) => setTone(Number(event.target.value)));
    qs('#layoutSelect').addEventListener('change', (event) => { state.settings.layout = event.target.value; syncUI(); });
    qs('#sustainToggle').addEventListener('change', (event) => setSustain(event.target.checked));
    qs('#metronomeToggle').addEventListener('change', (event) => setMetronome(event.target.checked));
    qs('#bpmRange').addEventListener('input', (event) => setSetting('bpm', Number(event.target.value)));
    qs('#calibrationStartButton').addEventListener('click', () => state.calibration.active ? stopLatencyCalibration() : startLatencyCalibration());
    qs('#calibrationResetButton').addEventListener('click', resetLatencyCalibration);
    qs('#calibrationDownButton').addEventListener('click', () => setLatencyCalibration(state.settings.latencyMs - 5));
    qs('#calibrationUpButton').addEventListener('click', () => setLatencyCalibration(state.settings.latencyMs + 5));
    qs('#synthModeSelect').addEventListener('change', (event) => setSynthMode(event.target.value));
    qs('#synthPresetSelect').addEventListener('change', (event) => { state.synth.preset = event.target.value; persistSynthSettings(); syncSynthUI(); });
    qs('#applySynthPreset').addEventListener('click', applySynthPreset);
    qs('#resetSynthButton').addEventListener('click', resetSynthPatch);
    ['synthOsc1Wave', 'synthOsc2Wave'].forEach((id) => qs(`#${id}`).addEventListener('change', (event) => setSynthParameter(id === 'synthOsc1Wave' ? 'osc1Wave' : 'osc2Wave', event.target.value)));
    qsa('[data-synth-range]').forEach((input) => input.addEventListener('input', (event) => setSynthParameter(event.target.dataset.synthRange, event.target.value)));
    qs('#groovePlayButton').addEventListener('click', toggleGroove);
    qs('#grooveClearButton').addEventListener('click', clearGroovePattern);
    qs('#grooveRandomButton').addEventListener('click', randomizeGroovePattern);
    qs('#grooveBpmRange').addEventListener('input', (event) => setSetting('bpm', Number(event.target.value)));
    qs('#grooveStepsSelect').addEventListener('change', (event) => {
      state.groove.steps = Number(event.target.value) === 32 ? 32 : 16;
      if (state.groove.currentStep >= state.groove.steps) state.groove.currentStep = -1;
      persistGrooveSettings();
      renderDrumGrid();
      syncUI();
    });
    qs('#grooveResolutionSelect').addEventListener('change', (event) => {
      state.groove.resolution = Number(event.target.value) === 8 ? 8 : 16;
      state.groove.nextStepAt = performance.now();
      persistGrooveSettings();
      syncUI();
    });
    qs('#drumGrid').addEventListener('click', (event) => {
      const button = event.target.closest('.drum-step');
      if (button) toggleGrooveStep(button.dataset.lane, Number(button.dataset.step));
    });
    qs('#arpEnabled').addEventListener('change', (event) => toggleArpeggiator(event.target.checked));
    qs('#arpPatternSelect').addEventListener('change', (event) => setArpParameter('pattern', event.target.value));
    qs('#arpRateSelect').addEventListener('change', (event) => setArpParameter('rate', event.target.value));
    qs('#arpOctavesSelect').addEventListener('change', (event) => setArpParameter('octaves', event.target.value));
    qs('#arpGateRange').addEventListener('input', (event) => setArpParameter('gate', event.target.value));
    qs('#chordAssistEnabled').addEventListener('change', async (event) => {
      if (event.target.checked) await enableAudio();
      if (event.target.checked && (!synth.context || synth.context.state !== 'running')) { event.target.checked = false; return; }
      setChordAssistEnabled(event.target.checked);
    });
    qs('#chordAssistType').addEventListener('change', (event) => setChordAssistParameter('type', event.target.value));
    qs('#chordAssistBass').addEventListener('change', (event) => setChordAssistParameter('bass', event.target.checked));
    qs('#chordAssistPulse').addEventListener('change', (event) => setChordAssistParameter('pulse', event.target.checked));
    qs('#samplerChooseButton').addEventListener('click', () => qs('#samplerFileInput').click());
    qs('#samplerFileInput').addEventListener('change', chooseSamplerFile);
    qs('#samplerEnabled').addEventListener('change', (event) => setSamplerEnabled(event.target.checked));
    qs('#samplerSliceCount').addEventListener('change', (event) => setSamplerParameter('sliceCount', event.target.value));
    qs('#samplerStart').addEventListener('input', (event) => setSamplerParameter('start', event.target.value));
    qs('#samplerEnd').addEventListener('input', (event) => setSamplerParameter('end', event.target.value));
    qs('#samplerSpeed').addEventListener('input', (event) => setSamplerParameter('speed', event.target.value));
    qs('#samplerPadGrid').addEventListener('click', (event) => {
      const button = event.target.closest('.sampler-pad');
      if (button) playSamplerSlice(Number(button.dataset.samplerIndex));
    });
    qs('#looperRecordButton').addEventListener('click', startLooperAction);
    qs('#looperStopButton').addEventListener('click', stopLooper);
    qs('#looperClearButton').addEventListener('click', clearLooperLayers);
    qs('#looperBarsSelect').addEventListener('change', (event) => {
      if (state.looper.mode !== 'idle' || state.looper.layers.length) return syncUI();
      state.looper.bars = clamp(Number(event.target.value) || 4, 1, 8);
      localStorage.setItem('musicKeyboardLooperBars', String(state.looper.bars));
      syncUI();
    });
    qs('#looperLayerList').addEventListener('click', (event) => {
      const mute = event.target.closest('[data-looper-mute]');
      const remove = event.target.closest('[data-looper-remove]');
      if (mute) toggleLooperLayer(mute.dataset.looperMute);
      else if (remove) removeLooperLayer(remove.dataset.looperRemove);
    });
    qs('#scopeRateSelect').addEventListener('change', (event) => {
      state.scopeRate = Number(event.target.value) || 60;
      state.lastScopeDraw = 0;
      drawScope();
      drawSoundLab();
      syncUI();
    });
    qs('#soundLabSmooth').addEventListener('change', (event) => { state.soundLab.smooth = [1, 3, 5, 9].includes(Number(event.target.value)) ? Number(event.target.value) : 3; syncSoundLabUI(); drawSoundLab(); });
    qs('#soundLabGain').addEventListener('input', (event) => { state.soundLab.gain = clamp(Number(event.target.value) || 1.5, .5, 6); syncSoundLabUI(); drawSoundLab(); });
    qs('#recordButton').addEventListener('click', toggleRecording);
    qs('#playButton').addEventListener('click', () => state.playback ? stopPlayback() : startPlayback());
    qs('#clearButton').addEventListener('click', clearTake);
    qs('#saveButton').addEventListener('click', () => saveCurrentTake(false));
    qs('#chooseMidiButton').addEventListener('click', chooseMidi);
    qs('#convertMidiButton').addEventListener('click', convertMidi);
    window.musicKeyboard.onSerialData(handleSerialRecord);
    window.musicKeyboard.onSerialStatus((status) => {
      if (status.kind === 'connecting') state.serialStatus = 'connecting';
      else if (status.kind === 'disconnected') { state.serialStatus = 'disconnected'; panic(); }
      else if (status.kind === 'error') { state.serialStatus = 'error'; setNotice(status.message || t('serialError')); }
      if (status.kind === 'bridge_log') setNotice(status.message);
      syncUI();
    });
    window.addEventListener('resize', () => { if (state.visualMode === 'piano') drawPiano(); drawScope(); drawSoundLab(); });
    if (window.ResizeObserver) {
      const observer = new ResizeObserver(() => { if (state.visualMode === 'piano') drawPiano(); drawScope(); drawSoundLab(); });
      observer.observe(qs('#pianoCanvas')); observer.observe(qs('#scopeCanvas'));
      observer.observe(qs('#soundLabWaveCanvas')); observer.observe(qs('#soundLabSpectrumCanvas'));
    }
  }

  renderPhysicalGrid();
  renderCalculatorVisual();
  setupEvents();
  applyTranslations();
  refreshPorts();
  syncUI();
  requestAnimationFrame(animationFrame);
})();
