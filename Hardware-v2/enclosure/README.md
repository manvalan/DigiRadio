# igiRadio v2 case

Two-part snap-fit case for the v2 board, for a Bambu Lab X1C with AMS. Outside size 65.6 x 84.6 x 25 mm.

| File | Contents |
|---|---|
| `igiradio_v2_case.3mf` | Both shells on one plate, with a filament on each part |
| `stl/` | Single-colour shells in print orientation |
| `step/` | Every part, in the coordinates of `../3d/digiradio-v2.step` |
| `renders/` | Previews |
| `digiradio_v2_case.py` | Parametric CadQuery model that generates all of the above |
| `check_fit.py` | Clash check against the board STEP and the battery |

## Printing

Open the 3MF in Bambu Studio and load four filaments:

| Slot | Part | Suggested |
|---|---|---|
| 1 | Upper shell | White |
| 2 | Lower shell | Navy |
| 3 | Lettering, line, reset cap | Teal |
| 4 | LED light pipe | Translucent (or white) |

Use PETG if you can, because the snap arms flex by about 1.2 % when they close. Print with 0.2 mm layers on the textured PEI plate, no supports. Avoid carbon-filled or metallic filament, which would detune the Wi-Fi and Bluetooth antennas.

## Assembly

1. Stick a 1 mm foam pad on the battery if you want it held firmly. The pocket leaves 0.9 mm of the height for swelling, so keep the pad soft.
2. Put the battery in the pocket with its lead towards the J3 end. The 755060 cell fits with either side along x.
3. Plug the lead into J3 and fold the slack into the space at the end of the case.
4. Lower the board onto the ledges, with the SMA in its square seat.
5. Press the upper shell on until all six arms click. To open it, lift each arm with a small flat screwdriver.

BOOT is not reachable from outside, and needs the case open.

## Rebuilding

```bash
pip install cadquery
python digiradio_v2_case.py
python check_fit.py
```

The lettering uses Arial Bold from the macOS system fonts; change `FONT` on other systems. `NAME` sets the name on the case.
