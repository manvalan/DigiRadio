# UI_ARCHITECTURE — igiRadio

## iPhone

`TabView` con tab: Home, Radio, Presets, Audio, Settings.

## iPad

`NavigationSplitView`:
- Sidebar: Home, FM, DAB, Presets, Bluetooth, Audio, Settings
- Detail: contenuto selezionato

## Schermate

| Schermata | ViewModel |
|-----------|-----------|
| Connection | `ConnectionViewModel` |
| Home | `HomeViewModel` |
| FM | `FMViewModel` |
| DAB | `DABViewModel` |
| Presets | `PresetsViewModel` |
| Audio | `AudioViewModel` |
| Bluetooth | `BluetoothViewModel` |
| Settings | `SettingsViewModel` |
| Device Info | `DeviceViewModel` |
| Diagnostics | `DiagnosticsViewModel` |
| Firmware | `FirmwareViewModel` |

## Design System

`Components/DesignSystem/` — colori semantici, tipografia, card, slider, status dot.
