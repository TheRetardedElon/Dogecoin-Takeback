#!/bin/bash
# Build script for Dogecoin with Modern UI
# Run this in WSL after setup_build_env.sh

set -e  # Exit on any error

echo "🏗️ Building Dogecoin with Modern UI"
echo "=================================="

# Check if we're in the right directory
if [ ! -f "autogen.sh" ]; then
    echo "❌ Error: autogen.sh not found. Make sure you're in the dogecoin-master directory"
    exit 1
fi

# Clean previous builds
echo "🧹 Cleaning previous builds..."
make clean 2>/dev/null || true
rm -f config.log config.status 2>/dev/null || true

# Generate build files
echo "📝 Generating build files..."
./autogen.sh

# Configure the build
echo "⚙️ Configuring build..."
./configure --with-gui=qt5 --enable-debug

# Build the project
echo "🔨 Building Dogecoin (this may take 30-60 minutes)..."
echo "Using $(nproc) parallel jobs"
make -j$(nproc)

# Check if build was successful
if [ -f "src/qt/dogecoin-qt" ]; then
    echo ""
    echo "🎉 Build successful!"
    echo "✅ Executable created: src/qt/dogecoin-qt"
    echo ""
    echo "To run the application:"
    echo "  ./src/qt/dogecoin-qt"
    echo ""
    echo "To test the modern UI:"
    echo "  1. Launch the application"
    echo "  2. Go to Settings → Theme tab"
    echo "  3. Try switching between Light and Dark themes"
    echo "  4. Test the custom theme creator"
    echo ""
    
    # Show file sizes
    echo "📊 Build artifacts:"
    ls -lh src/qt/dogecoin-qt 2>/dev/null || echo "  dogecoin-qt: Not found"
    ls -lh src/dogecoind 2>/dev/null || echo "  dogecoind: Not found"
    ls -lh src/dogecoin-cli 2>/dev/null || echo "  dogecoin-cli: Not found"
    
else
    echo "❌ Build failed!"
    echo "Check the output above for errors."
    exit 1
fi
