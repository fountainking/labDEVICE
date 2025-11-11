# CHIP-8 Per-Game Key Configuration

## Overview

Each CHIP-8 game uses different keys for controls. This system lets you use **standardized physical keys** that map to the correct CHIP-8 hex keys for each game.

**Standard physical keys on M5Cardputer:**
- `;` = Up
- `.` = Down
- `,` = Left
- `/` = Right
- `Enter` or `Space` = Action

## How It Works

1. Each ROM can have a `.cfg` file with the same name
2. Example: `tetris.ch8` → `tetris.ch8.cfg`
3. The config tells the emulator which CHIP-8 hex keys (0-F) each physical key should trigger
4. No config = default mapping (up=5, down=2, left=4, right=6, action=0)

## Quick Start

### 1. Generate Configs

Run the Python script in your CHIP-8 ROMs directory:

```bash
cd /path/to/your/roms
python3 /path/to/chip8_config_generator.py
```

Or place `chip8_config_generator.py` in your ROMs directory and run:

```bash
python3 chip8_config_generator.py
```

### 2. Choose Presets

The script offers common presets:

1. **Tetris** - 4=left, 5=rotate, 6=right, 2=drop
2. **Space Invaders** - 4=left, 5=shoot, 6=right
3. **Pong** - 1=up, 4=down
4. **Maze/Adventure** - 2=up, 8=down, 4=left, 6=right
5. **Custom** - specify each key manually

### 3. Transfer to SD Card

Copy both the `.ch8` ROM and its `.ch8.cfg` file to your M5Cardputer SD card.

## Config File Format

Example `tetris.ch8.cfg`:

```ini
# CHIP-8 Key Configuration for tetris.ch8
up=5
down=2
left=4
right=6
action=0
```

- Values are CHIP-8 hex keys (0-F)
- Lines starting with `#` are comments
- Emulator loads this automatically when ROM starts

## Manual Editing

You can edit `.cfg` files by hand:

```ini
up=2      # ; key → CHIP-8 key 2
down=8    # . key → CHIP-8 key 8
left=4    # , key → CHIP-8 key 4
right=6   # / key → CHIP-8 key 6
action=5  # Enter/Space → CHIP-8 key 5
```

Hex values can use `0x` prefix or not:
- `up=5` or `up=0x5` (both work)
- `up=F` or `up=0xF` (both work)

## CHIP-8 Keypad Reference

CHIP-8 has a 16-key hexadecimal keypad:

```
1 2 3 C
4 5 6 D
7 8 9 E
A 0 B F
```

Different games use different keys:
- **Tetris**: 4, 5, 6 (left, rotate, right)
- **Space Invaders**: 4, 5, 6 (left, shoot, right)
- **Pong**: 1, 4 (up, down)
- **Cave**: 2, 4, 6, 8 (movement)

## Tips

- **Number keys always work** - You can always press 0-9, B, C, D, E, F directly
- **Test and tweak** - If controls feel wrong, edit the `.cfg` and reload the ROM
- **Serial monitor** - Watch for `"Loading config: ..."` messages when ROM starts
- **No config?** - Defaults work for many games (especially Tetris-style)

## Troubleshooting

**ROM loads but controls don't work:**
- Check `.cfg` file is in same directory as `.ch8` file
- Check `.cfg` has correct values (0-F)
- Watch serial monitor for config loading messages

**Controls are backwards/wrong:**
- Edit `.cfg` file and swap values
- Example: If left/right are swapped, swap the `left=` and `right=` values

**ROM won't load:**
- Ensure `.ch8` file is valid CHIP-8 ROM
- Check SD card is mounted correctly

## Examples

### Tetris
```ini
up=5      # Rotate piece
down=2    # Drop faster
left=4    # Move left
right=6   # Move right
action=0  # (not used)
```

### Space Invaders
```ini
up=5      # Shoot (same as action)
down=5    # Shoot (same as action)
left=4    # Move left
right=6   # Move right
action=5  # Shoot
```

### Pong
```ini
up=1      # Paddle up
down=4    # Paddle down
left=0    # (not used)
right=0   # (not used)
action=0  # (not used)
```
