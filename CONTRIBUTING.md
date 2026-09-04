# Contributing

## Adding a fixture profile

This is the most useful contribution, and it needs no C++ at all: profiles are
JSON files in `data/fixtures/`, one per model.

Each mode describes its channels **by role** — `dimmer`, `red`, `hue`, `cct`,
`strobe` and so on — rather than by number. That typing is what lets the
interface offer a colour wheel, and lets the engine translate a wanted colour
onto whatever the fixture actually has.

```json
{
  "id": "maker-model",
  "manufacturer": "Maker",
  "model": "Model",
  "default_mode": "6ch",
  "note": { "en": "Anything the user must know before patching it.",
            "fr": "Ce que l'utilisateur doit savoir avant de le patcher." },
  "modes": [
    {
      "id": "6ch",
      "label": { "en": "Mode 6Ch — Intensity, strobe, RGB", "fr": "Mode 6Ch — …" },
      "channels": [
        { "role": "dimmer", "label": { "en": "Intensity", "fr": "Intensité" } },
        { "role": "strobe", "label": { "en": "Strobe", "fr": "Strobe" },
          "range_min": 128, "range_max": 250, "off": 0,
          "physical_min": 0, "physical_max": 30 }
      ]
    }
  ]
}
```

A few things the format exists to capture, and which are easy to get wrong:

- **Ranges are not always 0-255.** Describe them in the profile with
  `range_min`, `range_max`, `off`, `neutral`, `physical_min` and `physical_max`,
  rather than expecting the engine to know. See
  `aputure-amaran-t4c.json`, whose strobe channel is off from 0 to 19 and then
  covers 1 to 25 Hz from 20 to 255, and whose green/magenta neutral sits at 132.
- **The same role can sit at different ranges in different modes.** The Stairville
  Wild Wash strobe runs 11-250 in some modes but 128-250 in others, because 11 to
  127 is taken by random effects there. Getting that wrong fires a random effect
  instead of a strobe.
- **Labels can be a plain string or an object keyed by language.** Profiles carry
  their own labels and so cannot go through the module's translation file, which
  only knows fixed keys. An unknown language falls back to English, then to
  anything, rather than showing an empty field.
- **A default value that leaves the fixture dark is a trap.** If a channel the
  engine does not drive means "blackout" at zero — a colour macro, typically —
  give it a `default` that lets the fixture actually light.

Add a test in `tests/test-library-files.cpp` pinning the values you took from the
manual. That is what stops a later edit quietly breaking them.

## Building

The plugin builds as a Flatpak extension of OBS, so the OBS headers and Qt6 come
from the OBS runtime itself and nothing has to be installed on the host:

    flatpak install --user flathub org.flatpak.Builder
    flatpak run org.flatpak.Builder --force-clean --user --install \
        build-dir flatpak/com.obsproject.Studio.Plugin.ObsDmx.yaml

## Running the tests

`src/core` and `src/output` depend on neither libobs nor Qt, which is what makes
them testable outside OBS. You need only a compiler and nlohmann/json:

    cmake -B build -DBUILD_PLUGIN=OFF -DBUILD_TESTING_OBS_DMX=ON
    cmake --build build
    ctest --test-dir build --output-on-failure

Two further checks run in CI and are worth running by hand:

    ./tools/check-locales.sh   # every UI string translated, in both languages
    ./tools/check-slots.sh     # every Qt slot connected, every signal received

The second exists because a Qt slot that nothing connects compiles without the
slightest warning: the button is there, it is enabled, and it does nothing. That
happened once during development and no amount of unit testing would have caught
it.

`tools/e2e/` drives a real OBS over obs-websocket and decodes the DMX it emits.
Read its README before running it: it modifies your OBS configuration.

## Code conventions

- Comments, identifiers and log messages are in English. The `fr-FR` translation
  of the interface is a product feature and stays in French.
- Comments explain **why**, not what. The what is in the code; the why is the
  part that gets lost.
- `src/core` must stay free of libobs and Qt. Anything touching OBS lives in
  `src/obs`, `src/audio` or `src/ui`.
- The audio thread is real time: it must not allocate, take a lock, or log. It
  publishes only atomics.
