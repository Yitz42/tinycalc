// graph.c - sampling and drawing graphs (see src/c/ovl_graph.h). Built into
// the "graph" overlay on the watch; the PC simulator links it directly.
#include "../c/ovl_graph.h"
#include "../c/mmath.h"
#include <stddef.h>
#include <stdlib.h>

// Colours (the watch's 64: 0b11rrggbb)
#define C_BG     0xC0      // black
#define C_GRID   0xC5      // dark teal
#define C_AXIS   0xEA      // light grey
#define C_LABEL  0xEA
#define C_CURVE1 0xF8      // chrome yellow
#define C_CURVE2 0xCF      // cyan
#define C_ROOT   0xCC      // green
#define C_WHITE  0xFF

static bool fnan(float v) { return v != v; }

static int px(const GraphState *g, double x) {
  double v = g->x + (x - g->x0) / (g->x1 - g->x0) * g->w;
  return v < -30000 ? -30000 : v > 30000 ? 30000 : (int)v;
}
static int py(const GraphState *g, double y) {
  double v = g->y + g->h - 1 - (y - g->y0) / (g->y1 - g->y0) * (g->h - 1);
  return v < -30000 ? -30000 : v > 30000 ? 30000 : (int)v;
}
static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

// ------------------------------------------------------------------ sample

static void sample(GraphState *g, const TokBuf *f, const CalcVars *v) {
  if (g->kind != GK_IMPLICIT) {
    int w = g->w > GR_COLS ? GR_COLS : g->w;
    for (int c = 0; c < w; c++) {
      double x = g->x0 + (g->x1 - g->x0) * (c + 0.5) / g->w, l, r;
      if (calc_sides(f, v, x, &l, &r) && l > -1e30 && l < 1e30 && r > -1e30 && r < 1e30) {
        g->ca[c] = (float)l; g->cb[c] = (float)r;
      } else {
        g->ca[c] = g->cb[c] = (float)m_nan();
      }
    }
    return;
  }
  int gw = g->w / GR_CELL + 2, gh = g->h / GR_CELL + 2;
  if (!g->grid || g->gw != gw || g->gh != gh) {
    free(g->grid);
    g->grid = malloc((size_t)gw * gh * sizeof(float));
    g->gw = g->grid ? gw : 0;
    g->gh = g->grid ? gh : 0;
    if (!g->grid) return;
  }
  g->gx0 = g->x0;
  g->gy1 = g->y1;
  g->gdx = (g->x1 - g->x0) / g->w * GR_CELL;
  g->gdy = (g->y1 - g->y0) / (g->h - 1) * GR_CELL;
  CalcVars w = *v;
  for (int j = 0; j < gh; j++)
    for (int i = 0; i < gw; i++) {
      double l, r;
      w.x = g->gx0 + i * g->gdx;
      w.y = g->gy1 - j * g->gdy;
      float val = (float)m_nan();
      if (calc_eval_eq(f, &w, &l, &r) == CALC_OK) { double d = l - r; if (d > -1e30 && d < 1e30) val = (float)d; }
      g->grid[j * gw + i] = val;
    }
}

// Fit the y range to the bulk of the curve (5th..95th percentile), so a
// pole doesn't flatten everything else. Implicit curves get equal scales.
static void fit(GraphState *g) {
  if (g->kind == GK_IMPLICIT) {
    double half = (g->x1 - g->x0) * (g->h - 1) / g->w / 2, m = (g->y0 + g->y1) / 2;
    g->y0 = m - half; g->y1 = m + half;
    return;
  }
  int w = g->w > GR_COLS ? GR_COLS : g->w;
  float *s = malloc(2 * w * sizeof(float));
  if (!s) return;
  int m = 0;
  for (int c = 0; c < w; c++) {
    float v[2] = { g->ca[c], g->cb[c] };
    for (int k = 0; k < (g->kind == GK_SIDES ? 2 : 1); k++) {
      if (fnan(v[k])) continue;
      int j = m++;
      while (j > 0 && s[j - 1] > v[k]) { s[j] = s[j - 1]; j--; }
      s[j] = v[k];
    }
  }
  double lo = -1, hi = 1;
  if (m) { lo = s[m / 20]; hi = s[m - 1 - m / 20]; }
  free(s);
  if (g->kind == GK_FX || !m) { if (lo > 0) lo = 0; if (hi < 0) hi = 0; }   // keep y = 0 in view
  double span = hi - lo, mag = hi > -lo ? hi : -lo;
  if (span <= 1e-9 * (mag > 1 ? mag : 1)) { lo -= 1; hi += 1; span = 2; }
  if (lo > 0 && lo < span) lo = 0;
  if (hi < 0 && -hi < span) hi = 0;
  span = hi - lo;
  g->y0 = lo - span * 0.12;
  g->y1 = hi + span * 0.12;
}

// ------------------------------------------------------------------ draw

static double nice_step(double range) {
  double raw = range / 4, p = m_pow10i((int)m_floor(m_log10(raw)));
  double m = raw / p;
  return (m < 1.5 ? 1 : m < 3.5 ? 2 : m < 7.5 ? 5 : 10) * p;
}

static void grid_lines(const GraphState *g, const GfxApi *a) {
  char buf[24];
  int top = g->y, bot = g->y + g->h - 1, left = g->x, right = g->x + g->w - 1;
  int ax = clampi(py(g, 0), top, bot - 14);             // where x labels go
  int ay = clampi(px(g, 0), left, right - 30);          // where y labels go
  double st = nice_step(g->x1 - g->x0);
  for (double t = m_floor(g->x0 / st) * st; t <= g->x1; t += st) {
    int x = px(g, t);
    if (x <= left || x >= right) continue;
    a->line(a->ctx, x, top, x, bot, C_GRID, 1);
    if (m_fabs(t) < st / 2) continue;
    calc_format(m_fabs(t) < st * 1e-6 ? 0 : t, 4, buf, sizeof buf);
    a->text(a->ctx, buf, x - 20, ax, 40, false, C_LABEL, 1);
  }
  st = nice_step(g->y1 - g->y0);
  for (double t = m_floor(g->y0 / st) * st; t <= g->y1; t += st) {
    int y = py(g, t);
    if (y <= top || y >= bot) continue;
    a->line(a->ctx, left, y, right, y, C_GRID, 1);
    if (m_fabs(t) < st / 2) continue;
    calc_format(t, 4, buf, sizeof buf);
    a->text(a->ctx, buf, ay + 3, y - 9, 40, false, C_LABEL, 0);
  }
}

static void curve(const GraphState *g, const GfxApi *a, const float *v, uint8_t col) {
  int top = g->y + 1, bot = g->y + g->h - 2, w = g->w > GR_COLS ? GR_COLS : g->w;
  for (int c = 1; c < w; c++) {
    if (fnan(v[c - 1]) || fnan(v[c])) continue;
    int y0 = py(g, v[c - 1]), y1 = py(g, v[c]);
    if ((y0 < top && y1 < top) || (y0 > bot && y1 > bot)) continue;
    if ((y0 < top && y1 > bot) || (y0 > bot && y1 < top)) continue;   // jump across a pole
    a->line(a->ctx, g->x + c - 1, clampi(y0, top, bot), g->x + c, clampi(y1, top, bot), col, 2);
  }
}

// Implicit curve: in each grid cell where F changes sign, join the points
// where it crosses zero on the cell's edges (marching squares).
static void contour(const GraphState *g, const GfxApi *a) {
  if (!g->grid) return;
  int gw = g->gw, top = g->y, bot = g->y + g->h - 1, left = g->x, right = g->x + g->w - 1;
  for (int j = 0; j + 1 < g->gh; j++)
    for (int i = 0; i + 1 < gw; i++) {
      float v[4] = { g->grid[j * gw + i], g->grid[j * gw + i + 1],
                     g->grid[(j + 1) * gw + i + 1], g->grid[(j + 1) * gw + i] };
      if (fnan(v[0]) || fnan(v[1]) || fnan(v[2]) || fnan(v[3])) continue;
      double cx[4] = { 0, 1, 1, 0 }, cy[4] = { 0, 0, 1, 1 }, ex[4], ey[4];
      int n = 0;
      for (int e = 0; e < 4; e++) {
        float p = v[e], q = v[(e + 1) & 3];
        if ((p < 0) == (q < 0)) continue;
        double t = p / (p - q);
        ex[n] = cx[e] + (cx[(e + 1) & 3] - cx[e]) * t;
        ey[n] = cy[e] + (cy[(e + 1) & 3] - cy[e]) * t;
        n++;
      }
      // a sign change across a pole (huge values both sides) isn't a curve
      if (n >= 2) {
        float big = 0;
        for (int k = 0; k < 4; k++) if (m_fabs(v[k]) > big) big = (float)m_fabs(v[k]);
        float near = big;
        for (int k = 0; k < 4; k++) if (m_fabs(v[k]) < near) near = (float)m_fabs(v[k]);
        if (near > 1e3f && big > 1e6f) continue;
      }
      for (int k = 0; k + 1 < n; k += 2) {
        int x0 = px(g, g->gx0 + (i + ex[k]) * g->gdx), y0 = py(g, g->gy1 - (j + ey[k]) * g->gdy);
        int x1 = px(g, g->gx0 + (i + ex[k + 1]) * g->gdx), y1 = py(g, g->gy1 - (j + ey[k + 1]) * g->gdy);
        if ((x0 < left && x1 < left) || (x0 > right && x1 > right) || (y0 < top && y1 < top) || (y0 > bot && y1 > bot))
          continue;
        a->line(a->ctx, clampi(x0, left, right), clampi(y0, top, bot), clampi(x1, left, right), clampi(y1, top, bot),
                C_CURVE1, 2);
      }
    }
}

static void draw(const GraphState *g, const GfxApi *a) {
  a->fill(a->ctx, g->x, g->y, g->w, g->h, C_BG);
  int top = g->y, bot = g->y + g->h - 1, left = g->x, right = g->x + g->w - 1;
  if (g->labels) grid_lines(g, a);
  if (g->y0 < 0 && g->y1 > 0) a->line(a->ctx, left, py(g, 0), right, py(g, 0), C_AXIS, 1);
  if (g->x0 < 0 && g->x1 > 0) a->line(a->ctx, px(g, 0), top, px(g, 0), bot, C_AXIS, 1);
  if (g->kind == GK_IMPLICIT) contour(g, a);
  else {
    if (g->kind == GK_SIDES) curve(g, a, g->cb, C_CURVE2);
    curve(g, a, g->ca, C_CURVE1);
  }
  if (!g->roots_stale)
    for (int i = 0; i < g->nroots; i++) {
      int x = px(g, g->rx[i]), y = clampi(py(g, g->ry ? g->ry[i] : 0), top + 3, bot - 3);
      if (x < left || x > right) continue;
      if (i == g->sel) a->circle(a->ctx, x, y, 6, C_WHITE, false);
      a->circle(a->ctx, x, y, 3, C_ROOT, true);
    }
  if (g->trace && m_isfinite(g->ty)) {
    int x = px(g, g->tx), y = py(g, g->ty);
    if (x >= left && x <= right && y >= top && y <= bot) {
      a->line(a->ctx, x, top, x, bot, 0xD5, 1);
      a->circle(a->ctx, x, y, 5, C_WHITE, false);
      a->circle(a->ctx, x, y, 2, C_WHITE, true);
    }
    char buf[56], n1[20], n2[20];
    calc_format(g->tx, 6, n1, sizeof n1);
    calc_format(g->ty, 6, n2, sizeof n2);
    int k = 0;
    const char *parts[4] = { "x=", n1, "  y=", n2 };
    for (int p = 0; p < 4; p++) for (const char *s = parts[p]; *s && k < 55; s++) buf[k++] = *s;
    buf[k] = 0;
    a->fill(a->ctx, left + 2, top + 2, 150, 18, C_BG);
    a->text(a->ctx, buf, left + 5, top + 1, 150, false, C_WHITE, 0);
  }
}

// ---------------------------------------------------------------- graph sheet
// (moved here from main.c to keep the app under 64 KB)

static void trace(GrArgs *a) {
  GraphState *g = a->g;
  double l, r;
  g->ty = m_nan();
  if (g->kind != GK_IMPLICIT && calc_sides(a->f, a->v, g->tx, &l, &r)) g->ty = g->tcurve ? r : l;
}
static void trace_update(GrArgs *a) { if (a->g->trace && a->g->kind != GK_IMPLICIT) trace(a); }

static void view(GrArgs *a, bool reset) {
  GraphState *g = a->g;
  if (reset) { g->x0 = -10; g->x1 = 10; g->y0 = -10; g->y1 = 10; }
  if (g->kind == GK_IMPLICIT) { fit(g); sample(g, a->f, a->v); }   // equal scales, then sample
  else { sample(g, a->f, a->v); fit(g); }
  trace_update(a);
}

static bool only_y(const uint8_t *t, int n) { return n == 1 && t[0] == T_Y; }

// What to graph from what's typed: "y = f(x)" or "f(x)" -> f; an equation in
// x -> both sides; anything with y -> the curve where it holds.
static int setup(GrArgs *a) {
  const TokBuf *src = a->src;
  GraphState *g = a->g;
  int e = -1;
  for (int i = 0; i < src->n; i++) if (src->t[i] == '=') { e = i; break; }
  bool y = calc_has_y(src);
  *a->out = *src;
  if (e < 0) g->kind = y ? GK_IMPLICIT : GK_FX;
  else {
    const uint8_t *l = src->t, *r = src->t + e + 1;
    int nl = e, nr = src->n - e - 1;
    TokBuf rest = { .n = 0 };
    bool ry = false, ly = false;
    for (int i = 0; i < nr; i++) ry |= r[i] == T_Y;
    for (int i = 0; i < nl; i++) ly |= l[i] == T_Y;
    if (only_y(l, nl) && !ry) { for (int i = 0; i < nr; i++) rest.t[rest.n++] = r[i]; }
    else if (only_y(r, nr) && !ly) { for (int i = 0; i < nl; i++) rest.t[rest.n++] = l[i]; }
    if (rest.n) { *a->out = rest; g->kind = GK_FX; }
    else g->kind = y ? GK_IMPLICIT : GK_SIDES;
  }
  CalcVars v = *a->v;
  v.x = 1.234; v.y = 0.567;
  double l, r;
  return calc_eval_eq(a->out, &v, &l, &r) != CALC_ERR_SYNTAX;
}

static int zoom(GrArgs *a) {
  GraphState *g = a->g;
  double cx = (g->x0 + g->x1) / 2, cy = (g->y0 + g->y1) / 2;
  if (g->trace && m_isfinite(g->ty)) { cx = g->tx; cy = g->ty; }
  double hx = (g->x1 - g->x0) / 2 * a->d, hy = (g->y1 - g->y0) / 2 * a->d;
  if (hx < 1e-9 || hx > 1e9) return 0;
  g->x0 = cx - hx; g->x1 = cx + hx; g->y0 = cy - hy; g->y1 = cy + hy;
  sample(g, a->f, a->v);
  return 1;
}

// Mark the point at screen (sx, sy): on the nearer curve, or (implicit) the point itself.
static void trace_at(GrArgs *a) {
  GraphState *g = a->g;
  g->tx = g->x0 + (g->x1 - g->x0) * (a->sx - g->x) / g->w;
  double wy = g->y1 - (g->y1 - g->y0) * (a->sy - g->y) / (g->h - 1);
  g->trace = true;
  if (g->kind == GK_IMPLICIT) { g->ty = wy; return; }
  g->tcurve = 0;
  trace(a);
  if (g->kind == GK_SIDES) {
    double y0 = g->ty;
    g->tcurve = 1;
    trace(a);
    if (!(m_fabs(g->ty - wy) < m_fabs(y0 - wy))) { g->tcurve = 0; trace(a); }
  }
}

static void step(GrArgs *a) {                   // Up/Down: move the point (implicit: pan)
  GraphState *g = a->g;
  int d = a->n;
  if (g->kind == GK_IMPLICIT) {
    double h = (g->y1 - g->y0) / 8 * -d;
    g->y0 += h; g->y1 += h;
    sample(g, a->f, a->v);
    return;
  }
  if (!g->trace) { g->trace = true; g->tx = (g->x0 + g->x1) / 2; g->tcurve = 0; }
  else g->tx += (g->x1 - g->x0) / 40 * d;
  if (g->tx < g->x0 || g->tx > g->x1) {          // follow it
    double sh = g->tx < g->x0 ? g->tx - g->x0 - (g->x1 - g->x0) / 10 : g->tx - g->x1 + (g->x1 - g->x0) / 10;
    g->x0 += sh; g->x1 += sh;
    sample(g, a->f, a->v);
  }
  trace(a);
}

int graph_run(int cmd, GrArgs *a) {
  GraphState *g = a->g;
  switch (cmd) {
    case GR_SAMPLE: sample(g, a->f, a->v); return 1;
    case GR_FIT: fit(g); return 1;
    case GR_DRAW: draw(g, a->gfx); return 1;
    case GR_TRACE: trace(a); return 1;
    case GR_FREE: free(g->grid); g->grid = NULL; g->gw = g->gh = 0; return 1;
    case GR_SETUP: return setup(a);
    case GR_VIEW: view(a, a->n); return 1;
    case GR_ZOOM: return zoom(a);
    case GR_TRACE_AT: trace_at(a); return 1;
    case GR_STEP: step(a); return 1;
  }
  return 0;
}
