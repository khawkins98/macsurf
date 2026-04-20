# Mouse wheel scrolling in MacSurf (USB Overdrive)

MacSurf runs on Mac OS 9, which predates native scroll-wheel support.
Apple's CarbonLib on OS 9 does not carry the `kEventMouseWheelMoved`
Carbon event class — that API was added in Mac OS X 10.0 and never
back-ported. A browser running on OS 9 therefore cannot receive Carbon
wheel events no matter what it does.

The community-standard workaround is [USB
Overdrive](https://www.usboverdrive.com/) — Alessandro Levi
Montalcini's shareware USB HID driver that virtually every modern
OS 9 user already has installed for basic USB mouse support.

**Recommended configuration for MacSurf:**

1. Open the **USB Overdrive** control panel.
2. In the **Mouse** tab, find the **Scroll Wheel** action.
3. Set **Scroll Wheel Up** to **Up Arrow** (or **Page Up**).
4. Set **Scroll Wheel Down** to **Down Arrow** (or **Page Down**).
5. Apply. No restart required.

With this configuration, spinning the wheel sends classic keyDown
events that MacSurf's keyboard handler already routes to the active
window's scroll path — the same code path arrow keys and Page Up/Down
use when typed on the keyboard. Scrolling feel is indistinguishable
from keyboard-driven scrolling because it is keyboard-driven
scrolling.

**Alternative: leave USB Overdrive in "Scroll Up / Scroll Down" mode.**
This sends synthetic scrollbar-manipulation messages. MacSurf's
scroll bar is a standard Toolbox control and will respond correctly,
but the handler path is less direct than arrow keys. Use whichever
feels better.

**What to avoid:** do not try to install USB Overdrive's Mac OS X
"native wheel" modes in an OS 9 environment — those rely on Carbon
event classes that CarbonLib does not dispatch. See
[browser/netsurf/frontends/macos9/macos9_wheel.c](../browser/netsurf/frontends/macos9/macos9_wheel.c)
for the engineering history: MacSurf previously attempted a Carbon
`kEventMouseWheelMoved` handler, which crashed the Mac with an
illegal-instruction fault because CarbonLib's dispatcher
destabilizes when asked to deliver events whose class was never
back-ported. The handler has been disabled since fixes140; arrow-key
emulation is the correct OS 9 path.

**Scrolling UX that works today, with or without the wheel:**

- Scroll bar drag and thumb track
- Keyboard arrow keys (Up / Down)
- Page Up / Page Down
- Home / End
- Mouse wheel via USB Overdrive in arrow-key mode (per this doc)

That is a complete set of scroll inputs for an OS 9 browser.
