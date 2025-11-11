#!/bin/bash
set -e

# Get version from user or use timestamp
VERSION=${1:-$(date +%Y%m%d-%H%M%S)}
TAG="v${VERSION}"

echo "Building firmware..."
pio run -e m5stack-cardputer

echo "Creating release ${TAG}..."
gh release create ${TAG} \
  .pio/build/m5stack-cardputer/firmware.bin \
  --title "Laboratory M5 ${TAG}" \
  --notes "Automated release from VPS" \
  --repo fountainking/LaboratoryM5

echo "✓ Release created: https://github.com/fountainking/LaboratoryM5/releases/tag/${TAG}"
