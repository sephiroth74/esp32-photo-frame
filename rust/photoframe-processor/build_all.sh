#!/bin/bash
# Build script for photoframe-processor (CLI + GUI)

set -e

echo "========================================"
echo "Building Photo Frame Processor"
echo "========================================"
echo ""

# Colors
GREEN='\033[0;32m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Build CLI
echo -e "${BLUE}Building CLI (photoframe-processor)...${NC}"
cargo build --release --bin photoframe-processor

echo -e "${GREEN}✓ CLI built successfully${NC}"
echo "  → target/release/photoframe-processor"
echo ""

# Build GUI
echo -e "${BLUE}Building GUI (photoframe-processor-gui)...${NC}"
cargo build --release --bin photoframe-processor-gui --features gui

echo -e "${GREEN}✓ GUI built successfully${NC}"
echo "  → target/release/photoframe-processor-gui"
echo ""

# Show file sizes
echo "========================================"
echo "Build Summary:"
echo "========================================"
ls -lh target/release/photoframe-processor target/release/photoframe-processor-gui | tail -2

echo ""
echo -e "${GREEN}✓ All builds complete!${NC}"
echo ""
echo "To run:"
echo "  CLI: ./target/release/photoframe-processor --help"
echo "  GUI: ./target/release/photoframe-processor-gui"
