# DMX Lighting for OBS

An OBS plugin that attaches a lighting programme to each scene: switching scene
switches the light. Static looks, chases, strobes, and effects that follow the
sound.

Everything is set up by hand in a panel docked inside OBS. There is no
configuration file to write.

## What you need

- **OBS Studio 32** or newer.
- **A DMX interface** between the computer and your fixtures. Three families are
  supported:
  - an **Art-Net** or **sACN** node on the network, which outputs wired DMX;
  - an **Enttec DMX USB Pro** (or a clone speaking the same protocol) over USB.

  On Linux, reaching a USB interface requires membership of the `dialout` group:

      sudo usermod -aG dialout $USER

  then log out and back in.

- **If your fixtures are Aputure amaran T2c or T4c**: the *amaran USB-C to DMX*
  adapter, sold separately, one per lamp. These tubes have no DMX input of their
  own.

## Building and installing

The plugin is a plain CMake project. Nothing about it is tied to a particular
packaging: pick whichever route matches how OBS is installed on your machine.

### Linux, system OBS

Install the OBS development files (`obs-studio-devel` on Fedora,
`libobs-dev` on Debian and Ubuntu) along with Qt 6, then:

    cmake -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
    cmake --build build
    sudo cmake --install build --prefix /usr

This lands `obs-dmx.so` in `/usr/lib/obs-plugins` (or `lib64` where that is the
convention) and its data in `/usr/share/obs/obs-plugins/obs-dmx`.

To install for yourself rather than system-wide, point OBS at the user plugin
directory instead:

    cmake --install build --prefix ~/.config/obs-studio/plugins/obs-dmx

### Linux, OBS from Flatpak

A Flatpak app runs against its own libobs, Qt and C++ runtime, so a plugin has to
be built inside that same runtime. The manifest does exactly that, and installs
the result as an OBS plugin extension:

    flatpak install --user flathub org.flatpak.Builder
    flatpak run org.flatpak.Builder --force-clean --user --install \
        build-dir flatpak/com.obsproject.Studio.Plugin.ObsDmx.yaml

### Windows

A build is published on the
[releases page](https://github.com/yo1frenchtoast/obs-dmx/releases), one archive
per OBS major version. Extract it over your OBS installation directory — usually
`C:\Program Files\obs-studio` — or into `%APPDATA%\obs-studio\plugins\obs-dmx`
to install it for yourself alone. Restart OBS and look under
**Docks → DMX Lighting**.

To build it yourself, the CI workflow in `.github/workflows/windows.yml` is the
worked example: OBS publishes no Windows SDK, so libobs is built from its sources
against the prebuilt dependencies the project ships. With those in place:

    cmake -B build -A x64 "-DCMAKE_PREFIX_PATH=<deps>"
    cmake --build build --config RelWithDebInfo
    cmake --install build --prefix "<OBS install>"

### macOS

A universal build, covering both Apple silicon and Intel, is published on the
[releases page](https://github.com/yo1frenchtoast/obs-dmx/releases), one archive
per OBS major version. Unpack it into your plugins directory:

    unzip obs-dmx-macos-universal-obs32.zip \
      -d ~/Library/Application\ Support/obs-studio/plugins

macOS quarantines anything downloaded from a browser, and an unsigned plugin will
be refused silently. Clear the flag once:

    xattr -dr com.apple.quarantine \
      ~/Library/Application\ Support/obs-studio/plugins/obs-dmx.plugin

Restart OBS and look under **Docks → DMX Lighting**.

To build it yourself, `.github/workflows/macos.yml` is the worked example. Note
that OBS insists on the Xcode generator for its own build, and that it installs
libobs as a framework, so `libobs_DIR` has to point inside the bundle.

### A note on versions

A native plugin is bound to the OBS release it was compiled against, and must be
rebuilt when OBS makes a major version jump.

**What has actually been run:** Windows with OBS 32, and Linux both through the
Flatpak against OBS 32 and natively against libobs 31 on Fedora — the Linux
routes verified down to real DMX frames on the wire.

Not yet exercised by anyone: the Windows build for OBS 31, and macOS, whose
universal bundle is built and inspected by CI but has never been loaded into a
running OBS. Reports welcome, working or not.

The Enttec serial protocol is written from its documentation and has never met a
real interface, on any platform. Art-Net and sACN were verified frame by frame
against a software receiver.

## Getting started

### 1. Declare your fixtures

**Fixtures → Add a light…** Pick the model, say which **DMX mode** it is set to,
and give it a start address. The plugin suggests the first free one.

> **The DMX mode is chosen on the fixture's own screen**, not from the software.
> The mode named here must match the one set on the device. Nothing can verify
> it: if the two disagree, the fixture will behave oddly with no error message
> whatsoever. This is by far the most common cause of "it doesn't work".

The table flags address overlaps, naming both fixtures involved.

### 2. Check the wiring

**Output** tab. Choose the protocol, give the node's address or the serial port,
and tick **Send DMX**.

The **test bench** at the bottom of that tab sends a value on a channel of your
choosing. It is the quickest way to answer the real question: is my fixture
actually at the address I think it is?

### 3. Build your programmes

**Programmes → New…** Tick the fixtures involved, select them, set the light. The
setting applies to the whole selection at once.

While that tab is open, the programme being edited takes over the output: you see
what you are setting. **On leaving the tab, the active OBS scene takes over
again** — if it is attached to no programme, the lights go out. A warning at the
top of the tab says so, with a button to attach the programme to the current
scene in one click.

### 4. Attach the OBS scenes

At the bottom of the Programmes tab, a table lists your OBS scenes with, against
each one, the programme to trigger and the fade duration. This is the central
screen.

A scene left without a programme puts the lights out.

## The effects

A programme can carry effects, which stack on top of its base light. Unticking an
effect switches it off without losing it.

- **Chase** — a sequence of colours running along the fixtures, in the order they
  are listed. Step duration or tempo, fade between steps, forwards, backwards,
  back and forth, or random.
- **Strobe** — rate and flash length. Fixtures with a strobe channel of their own
  handle it themselves, which is far crisper. The others are flashed by the
  plugin, which becomes uneven above roughly ten flashes per second: that is a
  limit of DMX, not of your hardware.
- **Follows the sound** — brightness follows the level, colour follows the
  frequencies, or a flash lands on every beat. The analysis listens to OBS's
  audio mix, so exactly what the audience hears. A level meter shows live what
  the plugin is hearing, so the sensitivity can be set by eye.

  Note that the mix includes your global audio devices, microphone included, and
  that whatever scene is showing. If the light twitches in silence, that is room
  noise coming through the microphone: raise the **threshold**.

  By default the effect takes the programme's colour; untick *Keep the
  programme's colour* to give it one of its own.

- **Effect built into the fixture** — for devices that offer them, such as the FX
  mode of the amaran T4c: lightning, fire, TV, cop car, fireworks… They are
  played by the lamp itself, so they are far finer than anything that could be
  sent over the network. In exchange, the lamp must be in its effects mode on its
  own screen, and can then no longer show a plain colour.

  **If the plugin does not know your fixture's effects**, tick *Enter the
  channels myself*: you then copy the channel chart from its manual, line by
  line. Channel 1 is the fixture's first channel as numbered in its
  documentation — not a DMX address — and the values follow the fixture if you
  readdress it. A channel past the fixture's channel count is not sent, and the
  interface says so: writing there would drive the next fixture along.

  This direct entry is not only for effects. It is also the way to drive any
  channel the interface does not expose: a gobo wheel, a rotation motor, some
  setting peculiar to your model.

By default, when several things drive the same fixture, the brightest wins. That
is what lets a strobe flash over a coloured background without erasing it between
flashes. The **Advanced** fold offers Replace instead.

One consequence worth knowing: in that mode **an effect can only brighten, never
dim**. An effect whose intensity follows the sound will therefore never fall
below the programme's own level. That is why sound effects are created in
*Replace*, and why the editor warns when the combination would make an effect
inert.

## Keyboard shortcut

A **blackout** is offered among OBS's hotkeys (*Settings → Hotkeys*). It kills
and restores everything with the same gesture.

## Where the settings live

- The lighting rig (fixtures, programmes, scene attachments) lives in the OBS
  **scene collection**. It therefore follows your project and is backed up with
  it.
- The output settings (protocol, node address, serial port) depend on the machine
  and live apart, in the plugin's own configuration.

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md). Adding a fixture profile is the most
likely contribution and needs no C++.

## Licence

GPL-2.0-or-later, as required by anything linking against libobs.
