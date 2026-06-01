#!/bin/bash
set -e

echo "=========================================="
echo "SCUT_WALK_TALL Kunpeng ARM64 Build Script"
echo "=========================================="

BUILD_TYPE=${1:-Release}

echo "Build type: $BUILD_TYPE"

echo ""
echo "Step 1: Installing SFML 3.0 dependencies..."
sudo apt-get update
sudo apt-get install -y cmake g++ pkg-config libudev1 libudev-dev libopengl0 libgl1-mesa-dev

echo ""
echo "Step 2: Downloading and building SFML 3.0 from source..."
cd /tmp

if [ ! -d "SFML" ]; then
    git clone https://github.com/SFML/SFML.git
fi

cd SFML
git checkout 3.0.0
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=TRUE -DSFML_BUILD_EXAMPLES=FALSE -DSFML_BUILD_TEST_SUITE=FALSE
make -j$(nproc)
sudo make install
sudo ldconfig

echo ""
echo "Step 3: Building SCUT_WALK_TALL..."
cd ~
cd kunpeng_release

if [ ! -d "build" ]; then
    mkdir -p build
fi

cd build

echo "Configuring with CMake..."
cmake .. -DCMAKE_BUILD_TYPE=$BUILD_TYPE

echo "Building..."
cmake --build . --config $BUILD_TYPE -j$(nproc)

echo ""
echo "=========================================="
echo "Build completed!"
echo "Executable: $(pwd)/SCUT_WALK_TALL_KUNPENG"
echo "=========================================="
echo ""
echo "To run the game:"
echo "  cd $(pwd)"
echo "  ./SCUT_WALK_TALL_KUNPENG"
echo ""
