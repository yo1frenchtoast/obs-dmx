# End-to-end tests

These scripts check what the unit tests cannot reach: that switching scene in a
real OBS actually puts the right DMX values on the wire.

The idea: install a throwaway scene collection, launch OBS, drive its scene
changes over obs-websocket, and decode the Art-Net frames arriving on the
loopback.

Seven scenes cover the plugin's five build stages:

| Scene         | What it proves                                            |
|---------------|-----------------------------------------------------------|
| Plateau       | a white look: colour temperature and intensity             |
| Interview     | a coloured look: hue and saturation                        |
| Chaser        | the pattern really moves along the fixtures                |
| Strobe        | the hardware strobe channel, over a preserved background   |
| EffetIntegre  | the T4c's FX mode: effect selection and rate               |
| FxManuel      | hand-entered channels, and the footprint guard             |
| Musique       | a sound source brings the lights up                        |
| Silence       | the same programme without sound leaves them out           |

The last two form a control pair: without the Silence scene, nothing would prove
it is the sound doing the lighting.

## Running

    ./run_e2e.sh

## Warning

The script modifies your OBS configuration: it enables obs-websocket, writes a
scene collection named `obs-dmx-test`, drops a test tone into the configuration
directory, and points the DMX output at 127.0.0.1. Back up `user.ini`,
`global.ini` and `plugin_config/obs-websocket/config.json` before running it on a
machine whose configuration matters.

It also refuses to start if OBS is already running, and force-kills the instance
it launched. Do not run it while you are working in OBS.

Two traps met along the way, which explain some of its lines:

- it clears `.sentinel/run_*` before each launch, otherwise force-killing OBS at
  the end of the previous test brings up the safe-mode dialog, which blocks
  start-up;
- the test tone is written under `$HOME` rather than `/tmp`: OBS's sandbox has
  its own `/tmp` and would find nothing there.

## The other scripts

- `artnet_listener.py <seconds>` decodes ArtDMX frames on 127.0.0.1:6454 and
  checks the frame rate and sequencing.
- `sacn_listener.py <universe> <seconds>` does the same for sACN, over multicast.
- `make_tone.py <file.wav>` generates the test bass note.
