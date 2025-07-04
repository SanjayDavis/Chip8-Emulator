# CHIP-8 Emulator

A simple CHIP-8 emulator written in C using the SDL2 library. Supports display, keyboard input, and square wave audio output.

## 🎮 Features

- Full CHIP-8 instruction set support
- SDL2-based graphics rendering (scalable)
- Customizable screen, color, and audio settings
- Sound emulation using square waves
- Pause/resume functionality
- Key remapping to QWERTY layout


## 🛠️ Build Instructions

1. Ensure you're in the project root directory.
2. Compile the emulator:
   ```bash
   make
   ```
   This generates the `chip8` executable.

## ▶️ Run the Emulator

Run the emulator with a CHIP-8 ROM:
```bash
./chip8 path/to/rom.ch8
```

## ⌨️ Key Mapping

The emulator maps QWERTY keys to the CHIP-8 keypad as follows:

```
 CHIP-8 Keypad        Keyboard
 ┌────┬────┬────┬────┐        ┌────┬────┬────┬────┐
 │ 1  │ 2  │ 3  │ C  │        │ 1  │ 2  │ 3  │ 4  │
 ├────┼────┼────┼────┤        ├────┼────┼────┼────┤
 │ 4  │ 5  │ 6  │ D  │   =>   │ Q  │ W  │ E  │ R  │
 ├────┼────┼────┼────┤        ├────┼────┼────┼────┤
 │ 7  │ 8  │ 9  │ E  │        │ A  │ S  │ D  │ F  │
 ├────┼────┼────┼────┤        ├────┼────┼────┼────┤
 │ A  │ 0  │ B  │ F  │        │ Z  │ X  │ C  │ V  │
 └────┴────┴────┴────┘        └────┴────┴────┴────┘
```

## 🎨 Configuration

Default configuration values (set in code):
- **Window**: 64×32 pixels, scaled by 20× (1280×640)
- **Background Color**: Black (0x00000000)
- **Foreground Color**: White (0xFFFFFFFF)
- **Instructions per Second**: 700
- **Audio**: Square wave at 440 Hz, 44100 Hz sample rate, volume 12000
- **Pixel Outlines**: Enabled

## ⏸️ Controls

| Key        | Action           |
|------------|------------------|
| `Esc`      | Quit emulator    |
| `Spacebar` | Pause / Resume   |
| `Up arrow` | Increase Volume  |
|`Down arrow`| Decrease Volume  |
## 📁 ROMs

This emulator does not include ROMs. You can find public domain CHIP-8 ROMs online, such as:
- [CHIP-8 ROM Pack](https://github.com/chip8/roms)

## 📦 Makefile Targets

- `make`: Builds the `chip8` executable
- `make clean`: Removes the `chip8` binary
