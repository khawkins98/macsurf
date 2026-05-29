# MacSurf 1.3.1 — Forward, refined

**Released:** 2026-05-29
**Codename:** Forward (point release)
**Engine HEAD:** fixes318 (MacSurf side); macTLS at `2725837` (multi-curve ECDHE)
**Verified on:** Power Macintosh G3 iMac, Mac OS 9.2.2

---

## The headline

**MacSurf 1.3.1 negotiates TLS 1.3 over three ECDHE curves**, not just X25519. That unblocks a real chunk of the modern web that 1.3 alone didn't cover.

The 68kmla.org forums — one of the largest active Classic Mac communities, running XenForo on nginx with NIST-only curve config — refused every TLS 1.3 handshake from MacSurf 1.3 because we only offered X25519 in `key_share`. They now load completely on a 233 MHz G3 iMac running Mac OS 9.2.2.

![68kmla.org rendered on a G3 iMac via MacSurf 1.3.1](https://raw.githubusercontent.com/mplsllc/macsurf/master/screenshots/macsurf-1.3.1-68kmla.jpg)

Full forum index. 2,759 box-tree nodes, 19 images, zero handshake failures across the browsing session. TLS 1.3 over **P-384 via HelloRetryRequest**.

---

## What's new under the hood

The work is entirely inside macTLS — MacSurf picks it up as a transparent engine upgrade.

**Three ECDHE curves now offered in TLS 1.3 ClientHello `supported_groups`:**

- `0x001D` X25519 — preferred, all-purpose
- `0x0017` secp256r1 (NIST P-256)
- `0x0018` secp384r1 (NIST P-384)

The `key_share` extension carries an X25519 public key by default. If the server's `supported_groups` excludes X25519 (FIPS-compliant zones, XenForo on certain nginx configs, some Cloudflare zones in strict mode), the server sends a **HelloRetryRequest** naming the curve it actually wants. macTLS now resends ClientHello exactly once with a fresh key share on the requested curve.

The earlier HRR path infinite-looped on `WantRead` because it tried to handle the rekey inline. The new path is a clean state transition: `hrr_pending` set by ServerHello parser, consumed by the next pump step, ClientHello rebuilt and sent. Single retry, no recursion.

BearSSL primitives in use (already linked since v1.3):

- `ec_c25519_m15.c` (X25519)
- `ec_p256_m15.c` (P-256)
- `ec_prime_i15.c` (P-384)

No new BearSSL files. No MacSurf project file changes. Just rebuild against the new macTLS tree.

---

## Regression status

X25519-default sites all verified unchanged on host and on G3 hardware:

- mactrove.com (TLS 1.3, ChaCha20-Poly1305, X25519)
- google.com (TLS 1.3, X25519)
- cloudflare.com `/cdn-cgi/trace` (TLS 1.3, X25519)
- howsmyssl.com (TLS 1.3, X25519)

MacTLSTest unaffected.

---

## MacSurf-side companion fixes (fixes317-318)

Shipped alongside the macTLS update:

- **fixes317** — universal HTTPS↔HTTP fallback with per-host bounce-loop guard. Whichever scheme the user types is tried first; on failure the other scheme is attempted exactly once. HSTS sites that 301 HTTP back to HTTPS no longer spin in a redirect loop when the underlying TLS fails.
- **fixes317a** — URL parser repairs single-slash schemes (`https:/host/` typo → `https://host/`) so a slipped keystroke doesn't double-prepend and produce a nonsense URL.
- **fixes318** — diagnostic instrumentation for chunked-transfer-encoded responses (`CHUNKDIAG`), in service of running down a Google Fonts edge case.

---

## What's NOT in this release

For honest accounting:

- **Google Fonts (`fonts.googleapis.com`)** occasionally stalls on a chunked + keep-alive response that doesn't self-terminate cleanly. Diagnostic instrumentation shipped in fixes318; root cause is browser-side fetcher, not macTLS. Capture pending.
- **Perf headroom** on the G3 — most sites "struggle a little" as expected for the hardware. Speed work continues but isn't gated on a release.
- **TLS 1.3 session resumption** (PSK / tickets) still deferred.
- **Post-quantum key agreement** still deferred.

---

## Credits

Multi-curve ECDHE work landed in macTLS by the dedicated TLS agent. [BearSSL](https://www.bearssl.org/) by Thomas Pornin continues to provide the cryptographic primitives — the EC curve implementations used here (`ec_c25519_m15`, `ec_p256_m15`, `ec_prime_i15`) have been in BearSSL since its earliest releases.

---

## Pacing note

v1.2 closed the entropy hole on 2026-05-28. v1.3 landed TLS 1.3 on 2026-05-29. v1.3.1 ships later the same day with multi-curve ECDHE.

Two days from "modern entropy" to "modern TLS that works on real-world strict-mode servers." The site that triggered this round — 68kmla.org — is also one of the largest 68k/PPC retro-Mac communities on the web. The browser now loads the forum where the people most likely to actually use it hang out.

---

*MacSurf is at [github.com/mplsllc/macsurf](https://github.com/mplsllc/macsurf). macTLS is at [github.com/mplsllc/macTLS](https://github.com/mplsllc/macTLS). Bug reports and screenshots from real hardware are exactly what this project wants.*
