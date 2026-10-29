// Math helpers shared by the engine. Angles in the public API are degrees; y is up.

export const PI = Math.PI;
export const PI2 = Math.PI * 2;
export const degRad = Math.PI / 180;
export const radDeg = 180 / Math.PI;

export const clamp = (v, lo, hi) => (v < lo ? lo : v > hi ? hi : v);
export const lerp = (a, b, t) => a + (b - a) * t;
export const cosDeg = (d) => Math.cos(d * degRad);
export const sinDeg = (d) => Math.sin(d * degRad);
export const atan2Deg = (y, x) => Math.atan2(y, x) * radDeg;

/** Wraps degrees into (-180, 180]. */
export function wrapDeg(d) {
  d = d % 360;
  if (d > 180) d -= 360;
  else if (d <= -180) d += 360;
  return d;
}

/** Wraps radians into [-PI, PI]. */
export function wrapRad(r) {
  if (r > PI) return r - PI2;
  if (r < -PI) return r + PI2;
  return r;
}

/**
 * Evaluates a cubic Bezier segment used by animation curves. Control points are absolute
 * (time, value) pairs, matching the 4.x JSON curve format. Returns the value at `time`.
 */
export function bezierValue(time, t0, v0, cx1, cy1, cx2, cy2, t1, v1) {
  if (time <= t0) return v0;
  if (time >= t1) return v1;
  // Solve x(s) = time for s in [0,1] with Newton steps, falling back to bisection.
  const x = (s) => {
    const u = 1 - s;
    return u * u * u * t0 + 3 * u * u * s * cx1 + 3 * u * s * s * cx2 + s * s * s * t1;
  };
  const dx = (s) => {
    const u = 1 - s;
    return 3 * u * u * (cx1 - t0) + 6 * u * s * (cx2 - cx1) + 3 * s * s * (t1 - cx2);
  };
  let s = (time - t0) / (t1 - t0);
  for (let i = 0; i < 8; i++) {
    const d = dx(s);
    if (Math.abs(d) < 1e-9) break;
    const ns = s - (x(s) - time) / d;
    if (ns < 0 || ns > 1) break;
    s = ns;
  }
  if (Math.abs(x(s) - time) > 1e-6) {
    let lo = 0, hi = 1;
    for (let i = 0; i < 40; i++) {
      s = (lo + hi) / 2;
      if (x(s) < time) lo = s; else hi = s;
    }
  }
  const u = 1 - s;
  return u * u * u * v0 + 3 * u * u * s * cy1 + 3 * u * s * s * cy2 + s * s * s * v1;
}

/** 2x3 affine matrix [a, b, c, d, tx, ty] mapping (x,y) -> (a*x + b*y + tx, c*x + d*y + ty). */
export class Affine {
  constructor(a = 1, b = 0, c = 0, d = 1, tx = 0, ty = 0) {
    this.a = a; this.b = b; this.c = c; this.d = d; this.tx = tx; this.ty = ty;
  }
  apply(x, y) {
    return { x: this.a * x + this.b * y + this.tx, y: this.c * x + this.d * y + this.ty };
  }
  invert() {
    const det = this.a * this.d - this.b * this.c;
    if (Math.abs(det) < 1e-12) return new Affine();
    const id = 1 / det;
    const a = this.d * id, b = -this.b * id, c = -this.c * id, d = this.a * id;
    return new Affine(a, b, c, d, -(a * this.tx + b * this.ty), -(c * this.tx + d * this.ty));
  }
}

/** Distance from point p to segment ab. */
export function distToSegment(px, py, ax, ay, bx, by) {
  const dx = bx - ax, dy = by - ay;
  const l2 = dx * dx + dy * dy;
  let t = l2 > 0 ? ((px - ax) * dx + (py - ay) * dy) / l2 : 0;
  t = clamp(t, 0, 1);
  const x = ax + dx * t - px, y = ay + dy * t - py;
  return Math.sqrt(x * x + y * y);
}

/** Point-in-polygon test (even-odd) for flat [x0,y0,x1,y1,...] arrays. */
export function pointInPolygon(x, y, poly) {
  let inside = false;
  const n = poly.length / 2;
  for (let i = 0, j = n - 1; i < n; j = i++) {
    const xi = poly[i * 2], yi = poly[i * 2 + 1], xj = poly[j * 2], yj = poly[j * 2 + 1];
    if ((yi > y) !== (yj > y) && x < ((xj - xi) * (y - yi)) / (yj - yi) + xi) inside = !inside;
  }
  return inside;
}

export class Color {
  constructor(r = 1, g = 1, b = 1, a = 1) { this.r = r; this.g = g; this.b = b; this.a = a; }
  set(c) { this.r = c.r; this.g = c.g; this.b = c.b; this.a = c.a; return this; }
  clone() { return new Color(this.r, this.g, this.b, this.a); }
  equals(c) { return c && this.r === c.r && this.g === c.g && this.b === c.b && this.a === c.a; }
  static fromHex(hex, fallback = 'ffffffff') {
    hex = (hex || fallback).replace('#', '');
    if (hex.length === 6) hex += 'ff';
    const n = (i) => parseInt(hex.substr(i, 2), 16) / 255;
    return new Color(n(0), n(2), n(4), n(6));
  }
  toHex(withAlpha = true) {
    const h = (v) => Math.round(clamp(v, 0, 1) * 255).toString(16).padStart(2, '0');
    return h(this.r) + h(this.g) + h(this.b) + (withAlpha ? h(this.a) : '');
  }
}
