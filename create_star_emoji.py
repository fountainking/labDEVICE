#!/usr/bin/env python3
"""
Generate star.emoji file for LabChat system emojis
Creates a 16x16 pixel gold star emoji in .emoji format (512 bytes RGB565)
"""

import struct
import math

# RGB565 color conversion
def rgb_to_rgb565(r, g, b):
    """Convert RGB (0-255) to RGB565 (16-bit)"""
    r5 = (r >> 3) & 0x1F
    g6 = (g >> 2) & 0x3F
    b5 = (b >> 3) & 0x1F
    return (r5 << 11) | (g6 << 5) | b5

# Colors
TRANSPARENT = rgb_to_rgb565(255, 0, 255)  # Magenta = transparent
GOLD = rgb_to_rgb565(254, 160, 0)  # TFT_GOLD from M5 (0xFEA0)
DARK_GOLD = rgb_to_rgb565(200, 128, 0)  # Darker gold for outline

def point_in_star(x, y, cx, cy, outer_r, inner_r):
    """Check if point (x, y) is inside a 5-pointed star centered at (cx, cy)"""
    # Convert to polar coordinates relative to center
    dx = x - cx
    dy = y - cy
    dist = math.sqrt(dx * dx + dy * dy)
    angle = math.atan2(dy, dx)

    # Normalize angle to 0-2π
    if angle < 0:
        angle += 2 * math.pi

    # Rotate angle to start at top (like drawStar does with -PI/2)
    angle = angle + math.pi / 2
    if angle >= 2 * math.pi:
        angle -= 2 * math.pi

    # Find which segment we're in (10 segments: 5 outer, 5 inner)
    segment_angle = 2 * math.pi / 10  # 36 degrees
    segment = int(angle / segment_angle)
    local_angle = angle % segment_angle

    # Even segments are outer points, odd segments are inner points
    if segment % 2 == 0:
        # Outer point segment
        # Linear interpolation from outer to inner
        mid_angle = segment_angle / 2
        if local_angle < mid_angle:
            # Going from outer to inner
            t = local_angle / mid_angle
            threshold_r = outer_r * (1 - t) + inner_r * t
        else:
            # Going from inner to outer
            t = (local_angle - mid_angle) / mid_angle
            threshold_r = inner_r * (1 - t) + outer_r * t
    else:
        # Inner point segment (smaller radius)
        threshold_r = inner_r

    return dist <= threshold_r

def create_star_emoji():
    """Create 16x16 star emoji"""
    # Star parameters (centered at 8, 8 in 16x16 grid)
    cx, cy = 7.5, 7.5  # Center of 16x16 grid (between pixels 7 and 8)
    outer_r = 6.5  # Outer radius
    inner_r = 3.0  # Inner radius

    pixels = []

    for y in range(16):
        for x in range(16):
            # Check if pixel is inside star
            if point_in_star(x, y, cx, cy, outer_r, inner_r):
                # Add slight outline effect at edges
                edge_dist = 0
                for dx in [-1, 0, 1]:
                    for dy in [-1, 0, 1]:
                        if not point_in_star(x + dx, y + dy, cx, cy, outer_r, inner_r):
                            edge_dist += 1

                if edge_dist > 0:
                    pixels.append(DARK_GOLD)  # Edge pixels darker
                else:
                    pixels.append(GOLD)  # Inner pixels gold
            else:
                pixels.append(TRANSPARENT)  # Background transparent

    return pixels

def write_emoji_file(filename, pixels):
    """Write .emoji file (512 bytes, RGB565 little-endian)"""
    with open(filename, 'wb') as f:
        for pixel in pixels:
            # Write as little-endian 16-bit value
            f.write(struct.pack('<H', pixel))
    print(f"✓ Created {filename} ({len(pixels) * 2} bytes)")

if __name__ == "__main__":
    print("Generating star.emoji...")
    pixels = create_star_emoji()

    # Verify size
    if len(pixels) != 256:
        print(f"ERROR: Expected 256 pixels, got {len(pixels)}")
        exit(1)

    # Write to SD card emoji folder (change path if needed)
    write_emoji_file("star.emoji", pixels)

    print("\nTo use:")
    print("1. Copy star.emoji to your SD card at /labchat/emojis/star.emoji")
    print("2. The emoji will load automatically when entering LabChat")
    print("3. Type ':star' in chat to use it")
