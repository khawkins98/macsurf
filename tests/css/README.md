# MacSurf CSS test pages

Every feature shipped to the CSS engine has a probe page here. Probes
are flat HTML, no external assets, designed to render against the
proxy at `<redacted-proxy>:8765` on a real OS 9 device.

## Conventions

- Filename matches the feature: `flex_intrinsic.html`, `grid_areas.html`,
  `multicolumn.html`, `selectors_nth_child.html`.
- Top of file: human-readable description of what passes vs fails.
- Each card is labelled with a PROBE id (e.g. `PROBE F1`) so screenshots
  can be matched 1:1 against this file across rounds.
- Cards are mutually independent — no card depends on layout from another.
- Cards include at least one positive case, one edge case, one regression
  guard (the bug the feature was originally chasing).
- No JS. No external CSS. No images that need network.

## What a passing probe looks like

- Visible text matches the "expected" line in the probe header.
- No layout regressions on MacTrove, Wikipedia, DuckDuckGo, simple.html
  after the feature ships.

## Index

See `CSS_SUPPORT_MATRIX.md` at the repo root for the canonical
feature → test mapping. A feature without a probe here is not
considered shipped.
