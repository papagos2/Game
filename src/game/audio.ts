// Tiny synthesised sound effects (no audio files needed).
type Sfx = 'hit' | 'crit' | 'swing' | 'cast' | 'fire' | 'heal' | 'levelup' | 'quest' | 'loot' | 'death' | 'click' | 'error' | 'boom' | 'shock';

let ctx: AudioContext | null = null;
let master: GainNode | null = null;
let enabled = true;
let noiseBuf: AudioBuffer | null = null;

export function setSoundEnabled(on: boolean) {
  enabled = on;
  if (master) master.gain.value = on ? 0.5 : 0;
}

export function isSoundEnabled() {
  return enabled;
}

/** Must be called from a user gesture (browsers and iOS require it). */
export function unlockAudio() {
  if (!ctx) {
    const AC = window.AudioContext || (window as unknown as { webkitAudioContext: typeof AudioContext }).webkitAudioContext;
    if (!AC) return;
    ctx = new AC();
    master = ctx.createGain();
    master.gain.value = enabled ? 0.5 : 0;
    master.connect(ctx.destination);
    noiseBuf = ctx.createBuffer(1, ctx.sampleRate * 0.5, ctx.sampleRate);
    const d = noiseBuf.getChannelData(0);
    for (let i = 0; i < d.length; i++) d[i] = Math.random() * 2 - 1;
  }
  if (ctx.state === 'suspended') void ctx.resume();
}

function tone(freq: number, dur: number, type: OscillatorType, vol: number, slide = 0, delay = 0) {
  if (!ctx || !master) return;
  const t = ctx.currentTime + delay;
  const o = ctx.createOscillator();
  const g = ctx.createGain();
  o.type = type;
  o.frequency.setValueAtTime(freq, t);
  if (slide) o.frequency.exponentialRampToValueAtTime(Math.max(20, freq + slide), t + dur);
  g.gain.setValueAtTime(0.0001, t);
  g.gain.exponentialRampToValueAtTime(vol, t + 0.01);
  g.gain.exponentialRampToValueAtTime(0.0001, t + dur);
  o.connect(g).connect(master);
  o.start(t);
  o.stop(t + dur + 0.02);
}

function noise(dur: number, vol: number, filterFreq: number, delay = 0) {
  if (!ctx || !master || !noiseBuf) return;
  const t = ctx.currentTime + delay;
  const src = ctx.createBufferSource();
  src.buffer = noiseBuf;
  const f = ctx.createBiquadFilter();
  f.type = 'lowpass';
  f.frequency.value = filterFreq;
  const g = ctx.createGain();
  g.gain.setValueAtTime(vol, t);
  g.gain.exponentialRampToValueAtTime(0.0001, t + dur);
  src.connect(f).connect(g).connect(master);
  src.start(t);
  src.stop(t + dur);
}

export function play(s: Sfx) {
  if (!enabled || !ctx) return;
  switch (s) {
    case 'hit': noise(0.12, 0.35, 1200); tone(140, 0.1, 'square', 0.08, -60); break;
    case 'crit': noise(0.18, 0.5, 2000); tone(220, 0.15, 'sawtooth', 0.12, -120); break;
    case 'swing': noise(0.1, 0.12, 3000); break;
    case 'cast': tone(520, 0.18, 'triangle', 0.1, 300); break;
    case 'fire': noise(0.3, 0.3, 900); tone(200, 0.25, 'sawtooth', 0.06, -100); break;
    case 'shock': tone(900, 0.12, 'square', 0.06, -600); noise(0.15, 0.25, 4000); break;
    case 'heal': tone(660, 0.25, 'sine', 0.1, 220); tone(990, 0.3, 'sine', 0.07, 0, 0.08); break;
    case 'levelup': [523, 659, 784, 1046].forEach((f, i) => tone(f, 0.35, 'triangle', 0.12, 0, i * 0.09)); break;
    case 'quest': [392, 523, 659].forEach((f, i) => tone(f, 0.3, 'sine', 0.12, 0, i * 0.1)); break;
    case 'loot': tone(880, 0.08, 'sine', 0.08); tone(1320, 0.1, 'sine', 0.06, 0, 0.06); break;
    case 'death': tone(220, 0.8, 'sawtooth', 0.08, -150); break;
    case 'click': tone(700, 0.05, 'sine', 0.05); break;
    case 'error': tone(180, 0.15, 'square', 0.06); break;
    case 'boom': noise(0.6, 0.6, 500); tone(80, 0.5, 'sine', 0.2, -40); break;
  }
}
