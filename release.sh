#!/bin/bash

# Release build script for murmdoom
# Builds M1 and M2 variants with auto-incrementing version

set -e

# Version file
VERSION_FILE="version.txt"

# Read current version or initialize
if [ -f "$VERSION_FILE" ]; then
    read MAJOR MINOR < "$VERSION_FILE"
else
    MAJOR=1
    MINOR=0
fi

# Increment minor version
MINOR=$((MINOR + 1))
if [ $MINOR -ge 100 ]; then
    MAJOR=$((MAJOR + 1))
    MINOR=0
fi

# Save new version
echo "$MAJOR $MINOR" > "$VERSION_FILE"

# Format version string (e.g., 1_00, 1_01, 2_00)
VERSION=$(printf "%d_%02d" $MAJOR $MINOR)

echo "================================================"
echo "Building murmdoom release v${MAJOR}.${MINOR}"
echo "================================================"

# Create release directory
mkdir -p release

# Build variants
VARIANTS="M1 M2"

for VARIANT in $VARIANTS; do
    echo ""
    echo "================================================"
    echo "Building ${VARIANT} variant..."
    echo "================================================"
    
    # Clean build directory
    rm -rf build
    mkdir build
    cd build
    
    # Configure with board variant
    cmake -DPICO_PLATFORM=rp2350 -DBOARD_VARIANT=${VARIANT} ..
    
    # Build
    make -j4
    
    # Copy UF2 to release directory with version
    VARIANT_LOWER=$(echo "$VARIANT" | tr '[:upper:]' '[:lower:]')
    OUTPUT_NAME="murmdoom_${VARIANT_LOWER}_${VERSION}.uf2"
    cp murmdoom.uf2 "../release/${OUTPUT_NAME}"
    
    echo "Created release/${OUTPUT_NAME}"
    
    cd ..
done

echo ""
echo "================================================"
echo "Release build complete!"
echo "Version: ${MAJOR}.${MINOR}"
echo "Files created in release/:"
ls -la release/murmdoom_*_${VERSION}.uf2
echo "================================================"
