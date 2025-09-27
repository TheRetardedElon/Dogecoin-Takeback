#!/bin/bash
# Setup script for building Dogecoin with Modern UI
# Run this in WSL (Ubuntu)

set -e  # Exit on any error

echo "🚀 Setting up Dogecoin Build Environment with Modern UI"
echo "=================================================="

# Update system
echo "📦 Updating system packages..."
sudo apt update
sudo apt upgrade -y

# Install essential build tools
echo "🔧 Installing build tools..."
sudo apt install -y \
    build-essential \
    libtool \
    autotools-dev \
    automake \
    pkg-config \
    bsdmainutils \
    python3 \
    python3-pip \
    git \
    curl \
    wget

# Install crypto and networking libraries
echo "🔐 Installing crypto libraries..."
sudo apt install -y \
    libssl-dev \
    libevent-dev \
    libboost-system-dev \
    libboost-filesystem-dev \
    libboost-chrono-dev \
    libboost-program-options-dev \
    libboost-test-dev \
    libboost-thread-dev

# Install database libraries
echo "🗄️ Installing database libraries..."
sudo apt install -y \
    libdb-dev \
    libdb++-dev

# Install Qt5 development libraries
echo "🎨 Installing Qt5 development libraries..."
sudo apt install -y \
    libqt5gui5 \
    libqt5core5a \
    libqt5dbus5 \
    qttools5-dev \
    qttools5-dev-tools \
    libqt5widgets5-dev \
    qtbase5-dev \
    qtchooser \
    qt5-qmake \
    qtbase5-dev-tools

# Install additional useful tools
echo "🛠️ Installing additional tools..."
sudo apt install -y \
    cmake \
    ninja-build \
    ccache \
    htop \
    tree \
    vim \
    nano

# Verify installations
echo "✅ Verifying installations..."
echo "GCC version: $(gcc --version | head -n1)"
echo "Make version: $(make --version | head -n1)"
echo "Qt version: $(qmake --version)"

# Check if we're in the right directory
if [ ! -f "autogen.sh" ]; then
    echo "❌ Error: autogen.sh not found. Make sure you're in the dogecoin-master directory"
    exit 1
fi

echo "🎉 Build environment setup complete!"
echo ""
echo "Next steps:"
echo "1. Run: ./autogen.sh"
echo "2. Run: ./configure --with-gui=qt5"
echo "3. Run: make -j\$(nproc)"
echo ""
echo "To start building, run: ./build_dogecoin.sh"
