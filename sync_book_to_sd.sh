#!/bin/bash

# Laboratory M5 - The Book SD Card Sync Script
# Syncs all 2,137 articles to SD card

echo "🔬 Laboratory M5 - The Book SD Card Sync"
echo "========================================"
echo ""

# Colors for terminal output
RED='\033[0;31m'
YELLOW='\033[1;33m'
GREEN='\033[0;32m'
NC='\033[0m' # No Color

# Check if SD card is mounted
echo "Looking for SD card..."
SD_CARD=""

# Common SD card mount points on macOS
for mount_point in /Volumes/NO\ NAME /Volumes/LABORATORY /Volumes/M5CARD /Volumes/SD*; do
    if [ -d "$mount_point" ]; then
        SD_CARD="$mount_point"
        echo -e "${GREEN}✓ Found SD card at: $SD_CARD${NC}"
        break
    fi
done

if [ -z "$SD_CARD" ]; then
    echo -e "${RED}✗ SD card not found${NC}"
    echo ""
    echo "Available volumes:"
    ls -1 /Volumes/
    echo ""
    read -p "Enter SD card mount point (e.g., /Volumes/NO NAME): " SD_CARD

    if [ ! -d "$SD_CARD" ]; then
        echo -e "${RED}✗ Invalid path${NC}"
        exit 1
    fi
fi

echo ""

# Source directory
SRC_DIR="./the_book"
DEST_DIR="$SD_CARD/the_book"

# Check if source exists
if [ ! -d "$SRC_DIR" ]; then
    echo -e "${RED}✗ Source directory not found: $SRC_DIR${NC}"
    exit 1
fi

# Count files
TOTAL_FILES=$(find "$SRC_DIR" -type f -name "*.txt" | wc -l | xargs)
echo "Files to sync: $TOTAL_FILES"
echo ""

# Ask for confirmation
read -p "Sync $TOTAL_FILES articles to $SD_CARD? (y/n): " -n 1 -r
echo ""

if [[ ! $REPLY =~ ^[Yy]$ ]]; then
    echo "Cancelled."
    exit 0
fi

echo ""
echo "Syncing..."

# Create destination directory if it doesn't exist
mkdir -p "$DEST_DIR"

# Sync with rsync (preserves structure, only copies new/changed files)
rsync -av --progress \
    --include="*/" \
    --include="*.txt" \
    --include="index.txt" \
    --exclude="*" \
    "$SRC_DIR/" "$DEST_DIR/"

SYNC_STATUS=$?

echo ""
if [ $SYNC_STATUS -eq 0 ]; then
    SYNCED_FILES=$(find "$DEST_DIR" -type f -name "*.txt" | wc -l | xargs)
    echo -e "${GREEN}✓ Sync complete!${NC}"
    echo "Files on SD card: $SYNCED_FILES"

    # Show disk usage
    echo ""
    echo "SD Card Usage:"
    df -h "$SD_CARD" | tail -1

    # List categories
    echo ""
    echo "Categories synced:"
    find "$DEST_DIR" -maxdepth 1 -mindepth 1 -type d -exec sh -c 'echo "  • $(basename "{}"): $(find "{}" -type f -name "*.txt" | wc -l | xargs) articles"' \; | sort
else
    echo -e "${RED}✗ Sync failed${NC}"
    exit 1
fi

echo ""
echo -e "${GREEN}Ready for offline apocalypse knowledge 🔬${NC}"
