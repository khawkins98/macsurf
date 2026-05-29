# macLink — system-wide TLS bridge for Classic Mac OS

**Status:** scoping doc, no code yet.
**Goal:** A Classic Mac OS extension that makes every TCP-using application able to talk to modern TLS-protected services — Gmail, iCloud, Fastmail, ProtonMail Bridge, modern FTPS, modern web — without changing the applications themselves. The work already done in **macTLS** (TLS 1.2/1.3 with multi-curve ECDHE) and **macEntropy** (HMAC-DRBG-backed randomness) is the cryptographic core; macLink is the system-integration and proxy work that exposes that core to every app on the machine.

Codename: **macLink**.

Sister-product framing:
- **MacSurf** — the browser
- **macTLS** — the TLS engine (library)
- **macEntropy** — the random number source (library)
- **macLink** — the system bridge that exposes macTLS + macEntropy to every other app

---

## The product in one paragraph

Drop macLink into the System Folder. Reboot. Run a one-time per-app setup that imports macLink's root CA certificate. From that point forward, every classic Mac app that respects Internet Config's proxy setting — Eudora, Outlook Express 5, Internet Explorer 5, Netscape 7, Fetch, Anarchie, NCSA Telnet, Sherlock, dozens of others — speaks modern TLS to modern servers. The TLS handshake happens on the local Mac (so servers see the user's own IP, not a shared proxy origin) using macTLS's already-shipping engine. No remote service. No telemetry. No subscription. No external dependency past the first install.

---

## Architecture

### Topology

```
┌────────────────────────────────────┐    ┌───────────────────────────┐
│       Classic Mac OS 9.x           │    │    The modern internet    │
│                                    │    │                           │
│  ┌──────────┐  HTTP CONNECT  ┌─────┴────┴──┐    macTLS 1.3   ┌─────┴───────┐
│  │ Eudora   ├───────────────►│             ├────────────────►│ IMAP server │
│  │ IE 5     │  127.0.0.1:    │  macLink    │  X25519/        │ (Gmail,     │
│  │ OE 5     │  8765 (HTTPS)  │  background │  P-256/P-384    │  iCloud,    │
│  │ Netscape │  8587 (SMTP)   │  proxy      │  ChaCha20/AES  │  Fastmail,  │
│  │ Fetch    │  8143 (IMAP)   │             │                 │  …)         │
│  └──────────┘                └─────┬────┬──┘                 └─────┬───────┘
│       │                            │    │                          │
│       └─── trust macLink root ─────┘    └── trust 121 anchors ─────┘
│           (one-time CA import)            (Mozilla CCADB bundle)
└────────────────────────────────────┘    └───────────────────────────┘
```

The Mac runs a small background process (Faceless Background Application — well-established Classic Mac pattern) that exposes one or more loopback TCP ports. Apps connect to those ports either via Internet Config's standard proxy setting or via manual per-app configuration.

On the loopback side, macLink terminates TLS using its own internally-generated root CA, presenting a leaf cert that pretends to be the upstream server. On the upstream side, macLink uses macTLS — the same code MacSurf already runs — to do the real TLS handshake to the real server, with the real 121-anchor trust bundle.

### Trust model

Three trust relationships, each clear:

1. **User trusts macLink** — installs macLink's root CA in each TLS-using app's trust store, once per app. macLink can now present synthetic certs to that app.
2. **macLink trusts the modern web** — ships the 121-anchor Mozilla CCADB bundle (same one MacSurf has used since v0.6) and verifies real upstream certs against it.
3. **The upstream server trusts the user's Mac** — because the TLS connection originates from the user's actual IP, with the user's real network identity. No shared proxy IP. No fingerprint clumping.

Crucially: **macLink is the local TLS authority for each user's own machine**. Not a remote service. Not a shared identity. Each user is their own CA, scoped to their own apps.

### Why this works on apps that bring their own TLS

Netscape 7, IE 5, and similar apps don't use the OS's TLS layer — they ship their own. We can't patch them to use macTLS directly. But every one of those apps DOES support HTTP CONNECT proxy mode (because that's how corporate firewall setups in 2001-2003 worked). In that mode:

1. App opens TCP to macLink's loopback port
2. App sends `CONNECT realserver.com:443 HTTP/1.0\r\n\r\n`
3. macLink replies `200 Connection established\r\n\r\n`
4. App expects to do TLS through the tunnel
5. macLink presents its own cert claiming to be `realserver.com`, signed by macLink's root
6. App validates the cert against its own trust store (which now includes macLink's root) — passes
7. App's TLS handshake completes against macLink's leaf cert
8. macLink simultaneously runs a separate, real macTLS handshake to `realserver.com` upstream
9. macLink bridges plaintext bytes between the two TLS sessions

This is the same architecture as Burp Suite, mitmproxy, and corporate TLS inspection. Well-engineered, well-understood, well-debugged elsewhere.

### Why this works on apps with their own TLS that ALSO don't respect HTTP CONNECT

Some apps might not speak CONNECT but might speak a plain HTTP proxy. For those, macLink also accepts plain HTTP proxy requests on the same port (the standard `GET http://...` form) and upgrades the URL to HTTPS upstream when desired. This is the original macsurf-proxy pattern. Fewer modern apps need this, but Eudora's older versions did.

### Why this works on STARTTLS-using mail clients

STARTTLS is the protocol pattern where SMTP/IMAP/POP3 connections start plaintext on port 587/143/110, then negotiate an upgrade with a `STARTTLS` command. We can't tunnel STARTTLS via HTTP CONNECT because the upgrade is in-band to the application protocol.

Solution: protocol-aware proxies on dedicated loopback ports. macLink listens on:

- `127.0.0.1:8587` — SMTP STARTTLS proxy
- `127.0.0.1:8143` — IMAP STARTTLS proxy
- `127.0.0.1:8110` — POP3 STARTTLS proxy
- `127.0.0.1:8765` — HTTP/HTTPS CONNECT proxy
- `127.0.0.1:8121` — FTP control proxy (for FTPS)

For each protocol port, macLink runs a small state machine that knows the protocol's STARTTLS dance. The state machine waits for the client to issue `STARTTLS` (or equivalent), responds with the success line in the protocol's format, then transitions the connection to TLS-terminated mode with our leaf cert. Upstream, macLink does the equivalent STARTTLS upgrade to the real server using macTLS.

This is more work than HTTP CONNECT but well-bounded. Each protocol is ~150 lines of state machine.

---

## Components

| Component | Type | LOC est. | Purpose |
|---|---:|---:|---|
| `macLink-Proxy` | Faceless Background App (Carbon CFM) | ~1500 | The actual TLS-bridging server. Hosts all loopback listeners, drives macTLS upstream, runs leaf-cert minting, processes connections. |
| `macLink-CA` | Library inside macLink-Proxy | ~600 | Root CA generation on first boot, per-host leaf cert minting, cert caching, key material storage. Uses BearSSL primitives (RSA-2048 + ECDSA P-256 already linked from macTLS). |
| `macLink-IC` | Library inside macLink-Proxy | ~150 | Reads/writes Internet Config preferences (`kICHTTPProxy`, `kICUseHTTPProxy`). Auto-configures the system proxy at first boot. |
| `macLink-CDEV` | Control Panel (CDEV/cdev resource) | ~800 | UI: on/off toggle, list of intercepted protocols, root CA fingerprint display, export-CA button, connection stats, per-app integration help. |
| `macLink-Installer` | One-shot Carbon app + AppleScript glue | ~400 | First-run wizard: generates root CA if not present, drops .cer file on Desktop, walks user through per-app trust import. |
| `macLink-STARTTLS` | Protocol state machines (4 files) | ~600 total | SMTP / IMAP / POP3 / FTP STARTTLS handlers. Each ~150 LOC. |
| **Total net-new** | | **~4050** | Plus the macTLS + macEntropy + BearSSL files that are already in the project. |

### Reuse breakdown

| Already in repo / shipped | New for macLink |
|---|---|
| macTLS 1.2/1.3 client | TLS server-side (BearSSL primitives) |
| BearSSL primitives (AES, ChaCha20, SHA-2, X.509 verify, ECDSA verify) | X.509 generation + ECDSA signing |
| macEntropy (HMAC-DRBG) | Cert serial number entropy + key gen entropy |
| OT async wrapper (from macTLS) | Multi-listener accept loop |
| 121-anchor Mozilla CCADB bundle | macLink internal root CA cert |
| CW8 C89 build infrastructure | Faceless Background App project file |
| HTTP/1.1 parser (from MacSurf fetchers) | HTTP CONNECT method handler |
| Carbon UI patterns (CDEV unique) | Control Panel resource layout |

The cryptographic load-bearing work is **almost entirely already shipped**. macLink is mostly system-integration glue + a new server-side TLS path.

---

## The technically hard parts (and how each is handled)

### 1. X.509 leaf cert generation

BearSSL ships **verification** primitives (we use them today in macTLS) but does not ship a high-level **cert minting** API. Generating a valid X.509 leaf cert requires:

- DER encoding of a TBSCertificate structure (CN, SAN, validity dates, public key, serial)
- ECDSA-P256 signature over the TBS with macLink's root key
- Wrapping in the outer Certificate SEQUENCE

This is well-documented work. RFC 5280 + BearSSL's `br_ecdsa_sign_asn1` + a small hand-rolled DER encoder (~300 lines). Reference implementations exist in Caddy, mitmproxy's `certauthority`, and several Go ecosystem libraries — we can borrow structure.

Performance is the question, not feasibility. ECDSA signing on a G3 PPC at 233 MHz is roughly 30-60ms per cert (rough order-of-magnitude). Mitigation: aggressive caching — generate a leaf cert once per host, cache for 30 days, mint a new one only on first contact with a previously-unseen host. After a few weeks of normal use, the cache covers nearly all hits.

### 2. Internal root CA storage

macLink generates its own root CA private key on first boot using macEntropy. That key MUST be:

- Persisted (so the leaf certs we mint today remain valid tomorrow)
- Protected (it's the master key — anyone with it can forge certs that any of the user's apps will trust)

Storage options on Classic Mac OS:

- **Keychain Manager API** (the OS keychain) — exists on OS 9, supports password-protected key storage. Would prompt the user for a password on each macLink boot. Adds friction but is the security-best answer.
- **Resource fork of a system-owned file** — opaque to casual inspection, no password prompt, but the key is recoverable by any other process running on the Mac.
- **Plain file in macLink's Preferences folder with file-system-level restrictions** — minimal protection.

**Recommended:** Keychain Manager API, with an option (clearly documented as less secure) to skip the password for users who prioritize convenience over the marginal risk on a single-user machine. Classic Mac OS keychain is well-documented and macLink would be the kind of app that benefits from it.

### 3. Faceless Background Application lifecycle

A Carbon "background-only" app (FBA, `'BNDL'` with `kCFBundleAllowMixedLocalizations` analog) is launched at startup, has no menu bar, no Dock presence, but runs its event loop forever. Several long-lived Classic Mac apps used this pattern: Stuffit Browser, FaxSTF background, OneStop's various servers, etc.

The macLink-Proxy app would be installed in `System Folder/Startup Items/`, which the Finder launches on every boot in user-friendly order. The Process Manager keeps it running until shutdown or explicit quit.

Risks: if macLink crashes, it doesn't relaunch on its own. We'd want a small Watchdog INIT that monitors the FBA and relaunches if it dies — adds complexity. First version can skip this and rely on the user to manually relaunch (or reboot) if it crashes. Once stable, watchdog is straightforward to add.

### 4. Per-app trust store integration

Each TLS-using app stores trusted CAs in its own format:

| App | Trust store format | Import path |
|---|---|---|
| Netscape 7 / Mozilla Mail | NSS cert8.db (BerkeleyDB) | Preferences → Privacy → Certificates → Authorities → Import |
| IE 5 Mac | Resource fork of "Internet Explorer Preferences" | Preferences → Web Browser → Security → Certificate Authorities → Import |
| OE 5 Mac | Same store as IE 5 (shared) | Same UI |
| Eudora 6.x | PKCS#7 chain in `Eudora SSL Certs` file | Preferences → Checking Mail → SSL → Manual import |
| Fetch 4+ | Apple Keychain | Standard Keychain import |
| OS 9 Keychain Manager | System-wide PKCS#12 import | Keychain Access utility |

The installer drops the macLink root cert as `macLink Root.cer` on the Desktop (PEM or DER, both formats included). Each app gets a documented "how to import" page in the Help section of the Control Panel and on the project website. **One-time per-app friction**. Not trivial, but not insurmountable for users motivated enough to install a system extension in the first place.

### 5. STARTTLS protocol parsing

Each STARTTLS proxy is a small state machine that needs to:

- Accept plaintext TCP from the app
- Parse just enough of the protocol to find the STARTTLS exchange
- Pass other commands through to the upstream
- At STARTTLS time: respond to the client with success-in-protocol-format, transition our local socket to TLS-terminated mode, upgrade our upstream connection via macTLS, bridge cleartext between the two

The state machines are small but per-protocol. Estimated:

- SMTP STARTTLS: ~150 LOC (RFC 3207)
- IMAP STARTTLS: ~150 LOC (RFC 3501)
- POP3 STLS: ~120 LOC (RFC 2595)
- FTP AUTH TLS: ~200 LOC (RFC 4217) — more complex due to dual-channel

### 6. macTLS server-side (NEW work in macTLS)

macTLS today is a **client**. It speaks TLS *out*. For macLink to terminate TLS for incoming connections, macTLS needs to also speak TLS *in*. BearSSL has full server-side support (`br_ssl_server_init_*`) — the primitives are there. macTLS needs a new public API surface:

- `OSTLS_AcceptNew(...)` parallel to `OSTLS_New(...)`
- `OSTLS_Accept(...)` parallel to `OSTLS_Start(...)`
- Same `Pump` / `Read` / `Write` / `Close` for both sides

Estimated work in macTLS: ~600 LOC for the server-side wrapper. The handshake state machine is BearSSL's; we just wire it. Symmetric to the client work we already have.

---

## What macLink does NOT do (boundaries)

- **Does not provide OAuth 2.0** for mail clients. Gmail/iCloud/Outlook still require OAuth, and that's an application-protocol concern above TLS. macLink unblocks the TLS layer for clients using basic auth or app passwords — sufficient for iCloud (app passwords), Fastmail, self-hosted mail, ProtonMail Bridge, several others. NOT sufficient for Gmail or Outlook personal accounts. (OAuth bridging is a follow-on product, possibly **macAuth**.)
- **Does not modify any application's binary.** All compatibility comes via Internet Config, HTTP proxy mode, or STARTTLS interception.
- **Does not provide a service to other Macs over the network.** macLink is loopback-only by default. A "share with my LAN" mode is possible later (one option in the Control Panel) but adds operational and security complexity; not in v1.
- **Does not modify the OS's TLS stack.** OS 9's own SSL implementation stays untouched. Apps that explicitly use it (rare) continue to use it. macLink coexists.
- **Does not provide auto-update for macLink itself.** Initial version is install-once, replace-manually like MacSurf today. Auto-update via the same mechanism MacSurf is planning (issue #154) is a clean follow-on.

---

## Distribution & install story

### What ships

A single Mac OS 9-format `.sit` archive named `macLink-X.Y.sit`. Contents:

```
macLink/
├── Read Me First.rtf            ← Welcome, what to do, trust-model explanation
├── Install macLink              ← One-shot Carbon installer app
├── System Folder/
│   ├── Startup Items/
│   │   └── macLink              ← The FBA background process
│   ├── Control Panels/
│   │   └── macLink              ← The CDEV
│   └── Extensions/
│       └── (nothing in v1 — pure FBA approach; future INIT optional)
├── Apps/
│   └── macLink Setup Wizard     ← First-run cert installation walkthrough
└── docs/
    ├── Trust Model.html
    ├── App Integration Guides.html
    └── Privacy.html
```

The Install macLink app:
1. Copies files to System Folder locations
2. Verifies CarbonLib 1.5+ is installed (warns if not)
3. Asks the user whether they want auto-configure of Internet Config's proxy setting (default yes)
4. Asks whether they want to launch macLink immediately or on next reboot
5. Optionally launches the Setup Wizard to walk through trust-store imports

### First boot

The macLink-Proxy FBA:
1. Reads its preferences
2. If no root CA exists yet, generates one via macEntropy (one-time, ~1 second on G3)
3. Persists root key encrypted (Keychain Manager or fallback)
4. Drops the root cert as `macLink Root.cer` (PEM and DER both) on the user's Desktop
5. Starts listening on configured loopback ports
6. Idle; waits for connections

### Per-app trust import

The Setup Wizard (or Control Panel's Help button) opens a step-by-step guide for each major app. Screenshots, exact menu paths, expected outcomes. Worked examples for the top 5-10 apps; brief notes for others; community-maintained wiki for the long tail.

### Update story

For v1: replace the .sit, re-run the Installer, root CA preserved across upgrades. For v2+: integrate the auto-update checker MacSurf is scoping (issue #154) so macLink can self-update.

---

## Privacy & security model

**What macLink can see:** every byte of every connection it proxies. It's the TLS endpoint for both legs.

**What macLink does with that data:** processes it in memory, bridges it between sessions, forgets it. No disk logging beyond standard error/diagnostic logs (which the user can disable). No telemetry. No analytics. No third-party network calls except the user's own intended destinations.

**What an attacker who steals macLink's root key can do:** impersonate any server to any of the user's TLS-using apps, on that single Mac. They cannot impersonate to other Macs or to apps that don't trust macLink's root. Defense: standard system security plus optional Keychain password on the root key.

**What an attacker who compromises the FBA process can do:** read/write data flowing through any proxied connection on that single Mac. Same risk profile as compromising the user's browser.

**What an attacker on the network sees:** the user's Mac making normal TLS handshakes from the user's IP, with macTLS's normal fingerprint (X25519/P-256/P-384, ChaCha20-Poly1305 or AES-128-GCM). Indistinguishable from MacSurf doing the same. No "proxy origin" reputational risk.

**Privacy property worth highlighting:** Unlike a remote proxy service, macLink **cannot accidentally leak which sites you visit** because no other machine has that information. The user's traffic touches one piece of software the user installed; the world sees normal Mac traffic.

**Update integrity:** macLink ships with a code signature (CW8 has no native code signing, but we can attach a separately-distributed SHA-256 hash of the .sit) so users can verify they got the real macLink rather than a tampered copy. This matters because macLink is system-trust-establishing software.

---

## Phasing

Each phase is independently shippable. Phases 1-6 land before macLink is genuinely useful; 7-8 are quality of life and outreach.

| Phase | What ships | LOC | Lead time | Outcome |
|---|---|---:|---|---|
| **0. Spec & feasibility** | This doc + a hello-world FBA + verify macTLS server-side primitives exist in BearSSL | n/a | ~1 week | Go/no-go decision |
| **1. macTLS server side** | New `OSTLS_AcceptNew/Accept` API in macTLS. Symmetric to existing client API. Verified with a self-test against macTLS client. | ~600 | ~2 weeks | macTLS can terminate TLS as well as initiate it |
| **2. Cert generation** | `macLink-CA` library. Generates root + leaf certs via BearSSL primitives. Validates against MacSurf as TLS client. | ~600 | ~2 weeks | Can mint valid certs that BearSSL itself verifies |
| **3. HTTP CONNECT proxy** | FBA project skeleton. Listens on `127.0.0.1:8765`, terminates TLS with leaf certs, bridges to macTLS upstream. No GUI yet. End-to-end manual test with cURL or MacSurf configured as proxy client. | ~700 | ~2 weeks | First real macLink connection completes |
| **4. Internet Config integration** | Sets `kICHTTPProxy` at boot. Apps respect it. Manual test with IE 5 talking to a real HTTPS site. | ~150 | ~1 week | Configured apps work without manual proxy entry |
| **5. Control Panel CDEV** | On/off toggle, root CA fingerprint display, export-CA button, basic stats. | ~800 | ~3 weeks | Users can actually configure and trust macLink |
| **6. STARTTLS proxies** | SMTP/IMAP/POP3 state machines. Eudora-talking-to-Fastmail demo. | ~600 | ~2 weeks | Mail clients work |
| **7. Setup wizard + installer** | First-run guide, per-app integration walkthroughs, installer app. Documentation. | ~400 | ~2 weeks | Non-expert users can install and configure |
| **8. Polish & ship** | Bug fixes from internal testing, performance tuning, documentation polish, release. | n/a | ~2 weeks | macLink v1.0 |

**Total estimated work:** ~3850 LOC of net-new code, plus ~600 LOC into macTLS, plus a CDEV resource layout. Wall-clock estimate at the same intensity macTLS shipped at: 3-4 months for v1.0.

---

## Risks & open questions

### Will it actually work in the field?

**Best evidence we have today:** the existing Go macsurf-proxy ran identically-architected logic in production for several months pre-macTLS. The TLS-termination-and-rebridging pattern is proven on the application level. The new variables are (a) running it on the Mac itself, (b) the per-app trust-import friction.

**The variable I'm least sure about** is whether enough real users will actually do the per-app trust imports. The friction is one-time but it's there. Mitigation: make the Setup Wizard genuinely good. Walk-through video. Maybe an automated "I'll click the buttons for you" mode that scripts the Trust Authority dialog (possible via AppleScript in some cases).

### Will Classic apps actually respect Internet Config?

Most do, as a matter of historical fact, because that was the standard protocol-design for the era. But some apps (especially OE 5's "POP3/IMAP" settings vs IC's "Mail" settings — yes, different) override IC with per-app config. Each major app may need its own one-page integration guide showing the user where to set the proxy.

### Will BearSSL's cert minting be acceptably fast on a G3?

Educated guess: 30-60ms per leaf cert on G3 @ 233 MHz, dominated by ECDSA-P256 signing. With 30-day caching that's a one-time-per-host cost. For a user visiting 100 distinct hosts a day, the amortized cost is well below human perception. **Worth measuring early in phase 2 to confirm before committing to the architecture.**

### What about apps that don't support proxies at all?

There's a long tail of obscure software that hardcodes `connect(socket, AF_INET, port_443)`. Those apps can't be helped by a proxy-based design. Two options:

- **System-level TCP redirect** — intercept all TCP connects to specific ports at the kernel/Open Transport level and rewrite to localhost. Possible via an INIT that patches OT's `OTConnect`. Higher risk, higher reward. Probably v2 work.
- **App-by-app patches** — modify specific apps to use macLink. Niche, fragile, doesn't generalize.

**Recommendation:** start with proxy-only in v1. Add OT-level interception in v2 if there's demand.

### Compatibility with macTLS's existing keep-alive / connection pool

MacSurf's HTTPS fetcher pools macTLS connections to reduce handshake overhead. macLink would do the same, scoped to per-(client, upstream-host) pair. Need to confirm there's no global-state assumption in macTLS that breaks when multiple consumers use it concurrently. **Worth verifying in phase 0.**

### Should macLink share macTLS's TLS session cache across processes?

Probably not in v1. Each macLink-proxied connection is its own session. Future optimization: cross-process resumption via a small daemon that holds session tickets.

---

## Out of scope (v1)

- **OAuth 2.0 bridging.** Gmail/Outlook personal accounts won't work via macLink alone. That's macAuth's job (future product).
- **Local CA enrollment automation.** Apple Keychain import can be scripted via AppleScript; per-app stores cannot. Manual user step is required.
- **LAN sharing.** macLink listens on loopback only. Multi-Mac household sharing is a v2 feature.
- **OS 9 TCP/IP system patches.** No INIT-level OT modification in v1.
- **macLink for OS X.** Different platform entirely; would be a separate project.

---

## Adjacent products this scope unlocks

Once macLink ships, the surrounding ecosystem expands without further heavy lifting:

- **macAuth** — companion OAuth-bridging service for Gmail/Outlook personal accounts. Shares macTLS, macEntropy, and macLink's process model. Probably ~2000 LOC.
- **macFTP** — a native modern FTP/FTPS client. Could be built on top of macLink's FTP proxy state machine. Or could just be a normal Carbon app using macTLS directly.
- **macMail** (the email client we discussed) — native IMAP/SMTP client with HTML rendering via NetSurf engine. Wouldn't need macLink because it would use macTLS directly. But it benefits from the same code paths macLink develops.

The shared infrastructure tree:

```
                                              (macSurf)
                                                   │
   (macEntropy) ─── (macTLS) ─────────────────────┴── (macMail)
                       │
                       └──── (macLink) ─────── (macAuth)
                                                   │
                                                (macFTP)
```

Each downstream product is independent, but the cryptographic and protocol layers below are shared across the whole tree. macLink is the systemic-integration node; downstream products either use it (macAuth) or share its infrastructure (macFTP, macMail).

---

## Recommended next steps

1. **Phase 0 spike (this week):** verify BearSSL has the X.509 generation + ECDSA signing primitives we need, and write a 50-line hello-world FBA to confirm the background-app pattern works under CarbonLib. Both should answer cleanly. No commitment to ship yet.
2. **Decide on a release codename and version** for the macLink series (v0.1 alpha first, matching how MacSurf and macTLS rolled).
3. **Reserve `macLink` namespace on GitHub** (`mplsllc/macLink`?) and start the repo with this scoping doc.
4. **Phase 1 (macTLS server side)** is the longest-pole dependency and can start immediately, in parallel with phase 0 spike work — it's pure macTLS work and doesn't need macLink to exist yet.

---

## One-line summary

**macLink is the system extension that turns macTLS from "MacSurf's library" into "Classic Mac OS's TLS for everyone," letting every old app reach the modern web without a code change.** Built on what already ships, scoped to ~4 KLOC of net-new code, distributable as a single .sit, with no remote dependency and no server to operate.
