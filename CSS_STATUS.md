# MacSurf CSS Status Report

Generated 2026-05-19.

This is a brutal, hedge-free audit of CSS support in MacSurf. The goal is to identify what works, what doesn't, and what to implement next.

---

## The honest summary

MacSurf parses **167 CSS properties** via libcss. The layout/redraw pipeline only **reads 87 of them**. The gap between "parsed" and "consumed" is where modern pages fall apart visually — libcss correctly computes the cascade, but the layout engine never asks for the value, so the property does nothing.

Of the 87 consumed properties, **most work**, but a handful are partial (limits or accuracy gaps), and several depend on subsystems (font selection, color resolution) that have their own gaps.

---

## What actually works on real pages

These have been verified on hardware or in screenshots and produce the correct visual result:

### Box model
- `width`, `height`, `min-width`, `max-width`, `max-height`
- `margin` (all sides, `auto` centering)
- `padding` (all sides)
- `border-width`, `border-color`, `border-style` (all sides)
- `border-radius` (rounded corners via `PaintRoundRect`/`FrameRoundRect` — fixes172)
- `box-sizing`
- `box-shadow` (independent h/v offsets + custom colour — fixes175/178)

### Display & positioning
- `display: block | inline | inline-block | none | flex | inline-flex | grid | table | table-cell | table-row | list-item`
- `position: static | relative | absolute | fixed`
- `top`, `right`, `bottom`, `left`
- `float: left | right | none`
- `clear: left | right | both | none`
- `visibility: visible | hidden`

### Flexbox
- `flex-direction: row | row-reverse | column | column-reverse`
- `flex-wrap: nowrap | wrap | wrap-reverse`
- `flex-grow`, `flex-shrink`, `flex-basis`
- `justify-content: flex-start | flex-end | center | space-between | space-around | space-evenly`
- `align-content` (all values)
- `align-items`, `align-self`
- `order` (stable bubble-sort before layout)
- `gap: N` single-value (both axes get N)
- `column-gap: N`

### Grid (V1 — fixes75 + fixes118)
- `display: grid`
- `-macsurf-grid: N` (MacSurf shorthand for N equal columns)
- `grid-template-columns: <length-list>` (real track widths — fixes118)

### Typography
- `color`
- `font-family` (name match: Geneva, Monaco, Chicago, Charcoal)
- `font-size`
- `font-weight: normal | bold` (via QuickDraw `face=1` bold smear)
- `font-style: normal | italic` (via QuickDraw `face=2`)
- `text-align: left | right | center | justify`
- `text-decoration: underline | overline | line-through`
- `text-indent`
- `text-transform: uppercase | lowercase | capitalize | none`
- `line-height`
- `letter-spacing`
- `white-space: normal | nowrap | pre`
- `vertical-align`

### Backgrounds
- `background-color`
- `background-image: url(...)` (PNG/GIF/BMP/TIFF/JPEG via lodepng + QT — fixes78-79b)
- `background-position`
- `background-repeat`
- `-macsurf-gradient: linear-gradient(...)` (multi-stop — fixes49)
- `-macsurf-gradient: radial-gradient(...)` (24-ring oval stack — fixes74d)

### Lists & content
- `list-style-type` (disc, decimal, etc. — but bullet glyph renders as `;` on G3, see Known Issues)
- `list-style-image`
- `content` for `::before` / `::after` pseudo-elements

### Custom & visual
- CSS Custom Properties / `var()` — full native resolution (fixes133-139)
- `opacity` (QuickDraw stipple pattern at 1.0/0.80/0.50/0.25)
- `cursor` parsed; on hover, the cursor changes via `SetThemeCursor` (fixes131)
- `outline`, `outline-color`, `outline-style`, `outline-width` (parsed and consumed in redraw)
- `clip` (CSS 2 `clip: rect(...)`)
- `-macsurf-transform: rotate() translate() scale()` (fixes73)
- `-macsurf-text-shadow` (fixes50)
- `object-fit: fill | contain | cover | none | scale-down` (fixes116)
- `overflow: visible | hidden | scroll | auto` (clipping applies on block / inline-block / table-cell / flex / inline-flex / grid — fixes131 added flex/grid)
- `border-collapse`, `border-spacing` (tables)

### Pseudo-classes (fixes130 + fixes130e)
- `:hover` (re-cascade on mouse-track)
- `:active`
- `:focus`
- (Static pseudo-classes always worked: `:first-child`, `:nth-child(n)`, etc. via libcss selector engine)

---

## What is PARSED but NOT CONSUMED (the silent-fail category)

These accept author CSS without complaint but have zero effect on rendering. Every one of these is a probable visual bug on real pages.

| Property | Parsed | Layout reads? | Redraw reads? | Impact |
|---|---|---|---|---|
| `background-attachment` | yes | no | no | parallax / fixed bg won't stick |
| `caption-side` | yes | no | no | table caption position ignored |
| `column-count` | yes | no | no | multi-column text layout broken |
| `column-fill` | yes | no | no | |
| `column-rule-*` | yes | no | no | rule between columns ignored |
| `column-span` | yes | no | no | |
| `column-width` | yes | no | no | |
| `counter-increment` | yes | no | no | numbered counters don't increment |
| `counter-reset` | yes | no | no | |
| `quotes` | yes | no | no | `q { quotes: ... }` ignored |
| `empty-cells` | yes | no | no | empty table cell handling |
| `table-layout` | yes | no | no | fixed vs auto table layout |
| `unicode-bidi` | yes | no | no | bidi text |
| `writing-mode` | yes | no | no | vertical-writing pages broken |
| `word-spacing` | yes | no | no | typography accuracy |
| `z-index` | yes | no | no | **stacking order ignored — overlaps wrong** |
| `min-height` | yes | no | no | layout constraint missing |
| `break-after`, `break-before`, `break-inside` | yes | no | no | print/column breaks |
| `page-break-*` | yes | no | no | print breaks |
| `orphans`, `widows` | yes | no | no | print typography |
| `fill-opacity`, `stroke-opacity` | yes | no | no | SVG-only, low priority |

**Highest-impact silent fails on real pages, ranked:**

1. **`z-index`** — Modal dialogs, dropdowns, fixed nav bars, tooltips all rely on z-index to stack above page content. Without it, the cascade falls back to DOM tree order. Visible symptom: dropdowns paint UNDER the content below them; modals paint UNDER the page that called them.
2. **`min-height`** — Hero sections (`min-height: 100vh`) collapse to text height. Visible symptom: large coloured hero blocks appear tiny.
3. **`background-attachment: fixed`** — Parallax sites scroll their backgrounds. Visible only on specific sites.
4. **`counter-increment` / `counter-reset`** — Auto-numbered lists, table-of-contents, footnotes. Visible on documentation sites.
5. **`column-count`** — Magazine-style multi-column text. Visible on news / blog reading layouts.

---

## What is BROKEN or PARTIAL on consumed properties

### `gap: A B` two-value form (fixes148 limitation)
Single-value `gap: N` works (both axes get N). Two-value `gap: A B` loses A and stores only B as column-gap. Fix requires adding `CSS_PROP_ROW_GAP` as an independent property with its own bit slot in `css_computed_style_i.bits[]`. Bit budget audit shows word 15 has 27 free bits, word 14 bottom 5 bits are full. ~17 files to touch. Real-world impact: 97% of MacTrove pages use single-value form, deferred.

### `font-family` matching
Currently only matches `Geneva`, `Monaco`, `Chicago`, `Charcoal` by name. Any other family name falls through to the OS 9 default font for the resolved generic. Modern sites specifying `"Helvetica Neue", system-ui, sans-serif` get the system font, not their preferred. Not strictly broken — there are no other fonts installed by default — but the matching is narrow.

### `font-weight` granularity
Only `bold` (>= 600) vs `normal` (< 600). Numeric weights 100/200/300/400/500/600/700/800/900 all collapse to two values. Acceptable for QuickDraw which only has bold/non-bold.

### `text-overflow: ellipsis`
Not implemented anywhere. Long text in narrow containers wraps or overflows; never truncates with `…`. Modern UI cards rely on this for clean truncation.

### `word-break` / `overflow-wrap`
Not implemented. Long unbreakable strings (URLs, hashes) overflow their containers instead of breaking. Visible on URL-display pages.

### `clip-path` and `mask`
Not implemented. Decorative shape clipping silently ignored. Pages degrade to rectangular fallback (usually fine).

### `transition` and `animation`
Deferred to v0.4.5 (already noted in CLAUDE.md). MacSurf has a one-shot `-macsurf-animation-rotate` extension but no real CSS animation property support.

### Bitmap rendering
- PNG transparency: 1-bit `CopyMask` threshold (anti-aliased edges become binary)
- TIFF: opaque rendering only (`QTNewGWorld(k32ARGBPixelFormat)` returns `cDepthErr -157` on OS 9)
- Path A1.5 (`CopyDeepMask` + 8-bit mask) is queued

### List bullets render as `;`
Known issue from fixes33 era. `list-style-type: disc` resolves correctly in cascade but the glyph rendering shows `;` instead of `•` on G3 hardware. Documented in [docs/css-milestone-2026-05-13.md](docs/css-milestone-2026-05-13.md).

### Inline boxes occasionally duplicate
Known issue post-fixes33. Some inline-box runs render twice. Not blocking comprehension; cause unknown.

### URL bar input on initial window
Probably fixed by fixes77g, needs verification. Workaround: File → New Window.

---

## Implementation priorities

Ranked by visual impact on real-world pages. Each priority is a discrete shippable round.

### P0 — `z-index` and stacking contexts (HIGH impact, MEDIUM effort)

Modern pages cannot render correctly without z-index. Every dropdown, modal, tooltip, sticky header relies on this.

**Scope:**
- Add `css_computed_z_index` read in `redraw.c` `html_redraw_box`
- Sort children by `(z-index, dom-order)` before paint when a box establishes a stacking context (any positioned element with `z-index != auto`, opacity < 1, transform != none)
- The stacking context rules are involved — see CSS Painting Order spec for the full 7-level paint order

**Files:** `redraw.c`, possibly a new `redraw_stacking.c` helper.
**Estimated rounds:** 1-2 (one for `z-index: N` on positioned elements, second for stacking contexts proper).

### P1 — `min-height` (HIGH impact, LOW effort)

Hero sections everywhere use `min-height: 400px` or `min-height: 100vh`. Without it, the hero collapses to text height and the page looks broken.

**Scope:**
- Find `layout_find_dimensions` callers that read `max_height` and `min_width`
- Add parallel read of `css_computed_min_height`
- Apply same way `min_width` is currently applied (`if (min_height > 0 && min_height > *height) *height = min_height`)

**Files:** `layout.c` (one site, mirroring the existing min_width logic at line ~158-205).
**Estimated rounds:** 1.

### P2 — `text-overflow: ellipsis` (MEDIUM-HIGH impact, MEDIUM effort)

Card components on every modern UI use this to truncate long titles. Combined with fixes131's overflow-clipping on flex containers, ellipsis would finally make cards look polished.

**Scope:**
- Parse `text-overflow` property (new parser + selector files)
- In `layout_inline.c` (or wherever line boxes are finalized), when overflow:hidden is set and a line would exceed its container, truncate at the last fitting character and append `…`
- Mac OS 9 `…` glyph = `0xC9` in MacRoman, which we already convert from UTF-8

**Files:** new `p_text_overflow.c`, `s_text_overflow.c`, modifications to `layout_inline.c`, `propstrings.h`, dispatch tables.
**Estimated rounds:** 1-2.

### P3 — Viewport units `vw` / `vh` (HIGH impact on responsive sites, LOW effort)

Author CSS uses `100vh` for hero sections constantly. libcss already parses unit suffixes; the unit conversion in `css_unit_len2px` is where this lands.

**Scope:**
- In `css_unit_len2px` (or our wrapper), when unit == `CSS_UNIT_VW` return `viewport_width * value / 100`; same for VH
- Plumb viewport dimensions through the unit-context. Already partially done — viewport width is in `unit_len_ctx`.

**Files:** `browser/libcss/src/select/calc.c` or our calc shim, possibly `select.c` in the html handler.
**Estimated rounds:** 1.

### P4 — `counter-increment` / `counter-reset` (MEDIUM impact, MEDIUM effort)

Numbered headings, ordered lists with custom numbering, table-of-contents formats. NetSurf's `content_item` already has counter types in the cascade; we just don't drive them.

**Scope:**
- Walk DOM in document order, maintaining a counter table per scope
- Resolve `counter(name)` in `content: counter(chapter)` to the current value
- Reset on `counter-reset` declarations

**Files:** new `redraw_counters.c` helper invoked from `html_redraw_box` for boxes with `content` containing counter items.
**Estimated rounds:** 2-3.

### P5 — `word-break: break-all` / `overflow-wrap: break-word` (LOW-MEDIUM impact, LOW effort)

Long URLs in body text overflow containers. Word-break forces character-level wrapping when normal word breaks won't fit.

**Scope:**
- In `layout_inline.c`, when measuring a line and a single word exceeds the line width, allow mid-word breaks
- Property already parsed by NetSurf's libcss

**Files:** `layout_inline.c`.
**Estimated rounds:** 1.

### P6 — `transition` / `animation` (deferred to v0.4.5)

Animation framework would touch event loop, paint scheduling, time tracking. Substantial scope. Defer.

### P7 — `column-count` for multi-column text (LOW-MEDIUM impact, HIGH effort)

Magazine-style layout. Genuinely complex (text balancing across columns). Defer until proven needed.

### P8 — `clip-path`, `mask`, `filter` (LOW impact, HIGH effort)

Decorative. Pages degrade to rectangular fallback which is acceptable. Defer indefinitely.

---

## Test plan

Without dedicated regression tests, every CSS round needs a real-page verification step. Suggested gauntlet:

1. **`tests/css/z_index.html`** — three boxes with overlapping `position: absolute` and explicit z-index — paint order should match z-index, not DOM order.
2. **`tests/css/min_height.html`** — a `div` with `min-height: 300px` containing one short line of text — should be 300px tall.
3. **`tests/css/text_overflow.html`** — card with `width: 200px; overflow: hidden; text-overflow: ellipsis; white-space: nowrap` and 500px of text — should show `Very long title…`
4. **`tests/css/viewport_units.html`** — full-screen hero with `height: 100vh` — should fill the viewport.
5. **`tests/css/counters.html`** — `<h2>` styled with `counter-increment` and `content: counter(...)` — should auto-number.
6. **Real-page regression: MacTrove home, MacTrove app page, DuckDuckGo Lite, Wikipedia article** — visual diff against previous shipped state.

---

## Out-of-CSS items affecting page rendering

These are not CSS properties but affect whether pages "load properly":

1. **HTTP fetcher reliability** — fixes98-105 closed the major leaks; current state is stable across many-page browsing.
2. **TLS** — handled by proxy (out of scope for the browser).
3. **JavaScript** — Duktape ES5 in base build; modern JS still needs proxy render-and-flatten.
4. **Forms** — `<input>` rendering works; form submission path is wired. Style cascade reaches form controls.
5. **Tables** — table layout works for simple tables; complex tables (colspan/rowspan with auto-layout) may have gaps.

---

## What I would ship next

Top of stack: **P0 (z-index) and P1 (min-height) in one round.** P1 is a 5-line change; P0 needs a paint-order pass but is the biggest visual fix on the table. Together they would close the two most visible structural gaps on modern pages.

After that, P3 (viewport units) is the smallest valuable round, and P2 (text-overflow ellipsis) the polish round.
