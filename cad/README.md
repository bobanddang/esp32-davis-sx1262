# Enclosure (FreeCAD)

Minimal FreeCAD model for a small box that fits this project when built with the **same hardware** used for the working ESP32-C3 + Waveshare Core1262-HF setup.

It is intentionally simple: a shell and a plate sized for that stack, not a universal case for every ESP32 variant.

## Files

| File | Description |
|------|-------------|
| `davis-relay.FCStd` | FreeCAD source |
| `davis-relay-shell.step` | Enclosure shell (STEP) |
| `davis-relay-plate.step` | Mounting / lid plate (STEP) |
| `davis-relay-plate.3mf` | Plate export for 3D printing |

Open `davis-relay.FCStd` in [FreeCAD](https://www.freecad.org/) to tweak dimensions.

## Hardware this box was designed around

These are affiliate links — I get a little money if you buy through them. Almost any similar parts should work. I also tested with an ESP32-S3 and ESP32-C5; both worked fine when wired to the pins in the main README.

- ESP32-C3 board: https://amzn.to/4hbI4Rr
- Waveshare Core1262-HF (SX1262): https://amzn.to/3RHByar (white-label version of the Waveshare board)
- ElectroCookie proto board: https://amzn.to/3S5fS8v (standoffs fit this specific board)
- Heat-set inserts: https://amzn.to/4pLMBvY (I used inserts from this kit; others should work, but the holes are fairly narrow, so you need thin-walled M3 inserts. Frankly the columns should be wider, but there isn’t much torque on these.)
- Antenna and U.FL-to-SMA adapter: https://amzn.to/4pQ1LjY (I used this; almost anything should work depending on how far you are from the weather station. Never power the radio on without an antenna connected.)

If you use a different MCU (S3/C5 Zero, etc.) or a taller antenna / different header stack-up, expect to edit the model.

## Notes

- Print orientation and clearances are up to you; verify fit before committing to a final print.
- Cable / antenna cutouts may need adjusting for your wiring and U.FL vs ceramic antenna choice.
