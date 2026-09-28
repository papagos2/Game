// Touch, mouse and keyboard input. Left side of the screen = floating joystick,
// right side = drag to turn the camera, pinch to zoom, tap to select a target.

interface Tracked { id: number; sx: number; sy: number; x: number; y: number; t: number; moved: number }

export class Input {
  /** Movement: x = strafe right, y = forward. Length <= 1. */
  move = { x: 0, y: 0 };
  yaw = 0;
  pitch = 0;
  zoom = 0;
  taps: { x: number; y: number }[] = [];
  pressed = new Set<string>();
  private keys = new Set<string>();
  private joy: Tracked | null = null;
  private cams = new Map<number, Tracked>();
  private pinchDist = 0;
  private readonly radius = 56;

  constructor(private layer: HTMLElement, private joyBase: HTMLElement, private joyKnob: HTMLElement) {
    layer.addEventListener('pointerdown', this.onDown);
    window.addEventListener('pointermove', this.onMove, { passive: false });
    window.addEventListener('pointerup', this.onUp);
    window.addEventListener('pointercancel', this.onUp);
    layer.addEventListener('wheel', (e) => {
      this.zoom += Math.sign(e.deltaY) * 1.5;
      e.preventDefault();
    }, { passive: false });
    layer.addEventListener('contextmenu', (e) => e.preventDefault());
    window.addEventListener('keydown', (e) => {
      if ((e.target as HTMLElement)?.tagName === 'INPUT') return;
      const k = e.key.toLowerCase();
      if (!this.keys.has(k)) this.pressed.add(k);
      this.keys.add(k);
      if (k === 'tab') e.preventDefault();
    });
    window.addEventListener('keyup', (e) => this.keys.delete(e.key.toLowerCase()));
    window.addEventListener('blur', () => this.reset());
  }

  reset() {
    this.keys.clear();
    this.cams.clear();
    this.endJoy();
  }

  private onDown = (e: PointerEvent) => {
    const tr: Tracked = { id: e.pointerId, sx: e.clientX, sy: e.clientY, x: e.clientX, y: e.clientY, t: performance.now(), moved: 0 };
    const leftZone = e.clientX < window.innerWidth * 0.42 && e.clientY > window.innerHeight * 0.25;
    if (e.pointerType !== 'mouse' && leftZone && !this.joy) {
      this.joy = tr;
      this.joyBase.style.display = 'block';
      this.joyBase.style.left = `${e.clientX}px`;
      this.joyBase.style.top = `${e.clientY}px`;
      this.joyKnob.style.transform = 'translate(-50%, -50%)';
    } else {
      this.cams.set(e.pointerId, tr);
      if (this.cams.size === 2) this.pinchDist = this.camDistance();
    }
    try {
      this.layer.setPointerCapture(e.pointerId);
    } catch {
      /* ignore */
    }
  };

  private camDistance() {
    const [a, b] = [...this.cams.values()];
    return Math.hypot(a.x - b.x, a.y - b.y);
  }

  private onMove = (e: PointerEvent) => {
    if (this.joy && e.pointerId === this.joy.id) {
      let dx = e.clientX - this.joy.sx;
      let dy = e.clientY - this.joy.sy;
      const len = Math.hypot(dx, dy);
      if (len > this.radius) {
        dx = (dx / len) * this.radius;
        dy = (dy / len) * this.radius;
      }
      this.joyKnob.style.transform = `translate(calc(-50% + ${dx}px), calc(-50% + ${dy}px))`;
      const m = Math.min(1, len / this.radius);
      const dead = 0.12;
      const f = m < dead ? 0 : (m - dead) / (1 - dead) / Math.max(m, 1e-6);
      this.move.x = (dx / this.radius) * f;
      this.move.y = (-dy / this.radius) * f;
      e.preventDefault();
      return;
    }
    const tr = this.cams.get(e.pointerId);
    if (!tr) return;
    const dx = e.clientX - tr.x;
    const dy = e.clientY - tr.y;
    tr.x = e.clientX;
    tr.y = e.clientY;
    tr.moved = Math.max(tr.moved, Math.hypot(tr.x - tr.sx, tr.y - tr.sy));
    if (this.cams.size >= 2) {
      const d = this.camDistance();
      this.zoom -= (d - this.pinchDist) * 0.05;
      this.pinchDist = d;
    } else if (e.pointerType !== 'mouse' || e.buttons > 0) {
      this.yaw -= dx * 0.0065;
      this.pitch += dy * 0.004;
    }
    e.preventDefault();
  };

  private onUp = (e: PointerEvent) => {
    if (this.joy && e.pointerId === this.joy.id) {
      this.endJoy();
      return;
    }
    const tr = this.cams.get(e.pointerId);
    if (!tr) return;
    this.cams.delete(e.pointerId);
    if (tr.moved < 12 && performance.now() - tr.t < 400 && this.cams.size === 0) this.taps.push({ x: tr.x, y: tr.y });
  };

  private endJoy() {
    this.joy = null;
    this.move.x = 0;
    this.move.y = 0;
    this.joyBase.style.display = 'none';
  }

  /** Movement combining joystick and keyboard. */
  moveVector() {
    let x = this.move.x;
    let y = this.move.y;
    if (this.keys.has('w') || this.keys.has('arrowup')) y += 1;
    if (this.keys.has('s') || this.keys.has('arrowdown')) y -= 1;
    if (this.keys.has('a') || this.keys.has('arrowleft')) x -= 1;
    if (this.keys.has('d') || this.keys.has('arrowright')) x += 1;
    const l = Math.hypot(x, y);
    if (l > 1) {
      x /= l;
      y /= l;
    }
    return { x, y };
  }

  keyDown(k: string) {
    return this.keys.has(k);
  }

  /** Returns and clears per-frame accumulated input. */
  consume() {
    const out = { yaw: this.yaw, pitch: this.pitch, zoom: this.zoom, taps: this.taps, pressed: this.pressed };
    this.yaw = 0;
    this.pitch = 0;
    this.zoom = 0;
    this.taps = [];
    this.pressed = new Set();
    return out;
  }
}
