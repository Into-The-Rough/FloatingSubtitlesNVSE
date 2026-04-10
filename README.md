# FloatingSubtitlesNVSE

An xNVSE plugin for Fallout: New Vegas that displays NPC dialogue as floating
subtitles above speakers' heads instead of the vanilla subtitle bar.

## Features

- Subtitles track speaker position in 3D space
- Distance-based fade and line-of-sight handling
- Off-screen display modes (screen edge, circular, hide)
- Interrupt transitions and fade-in animations
- Overlap resolution for simultaneous speakers
- Narrator support (centered display)
- Per-speaker dialogue queuing
- Dialogue menu tail suppression

## Requirements

- Fallout: New Vegas
- xNVSE
- JohnnyGuitar NVSE
- itr-nvse (required for NPC dialogue subtitles)
- MCM (optional, for in-game settings)

## Configuration

Settings live in `Data/config/FloatingSubtitlesNVSE.ini`.

Reload config at runtime with:

```text
ReloadPluginConfig FloatingSubtitlesNVSE
```

## Building

Open `FloatingSubtitlesNVSE.vcxproj` in Visual Studio 2022 and build `Win32`
`Release`.

The project file contains local post-build copy steps. If you are building in a
different environment, update or remove those paths first.

## License

FloatingSubtitlesNVSE is licensed under the MIT license. See [LICENSE](LICENSE).
