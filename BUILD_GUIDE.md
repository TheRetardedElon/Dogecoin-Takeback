# 🚀 Building Dogecoin with Modern UI

This guide will help you compile and test the new modern UI system for Dogecoin Core.

## 📋 Prerequisites

### For Windows (Recommended: WSL)

1. **Install WSL2**:
   ```powershell
   # Run in PowerShell as Administrator
   wsl --install
   ```

2. **Install Ubuntu 22.04** from Microsoft Store

3. **Update and install dependencies**:
   ```bash
   sudo apt update
   sudo apt upgrade -y
   sudo apt install -y build-essential libtool autotools-dev automake pkg-config bsdmainutils python3
   sudo apt install -y libssl-dev libevent-dev libboost-system-dev libboost-filesystem-dev libboost-chrono-dev libboost-program-options-dev libboost-test-dev libboost-thread-dev
   sudo apt install -y libdb-dev libdb++-dev
   sudo apt install -y libqt5gui5 libqt5core5a libqt5dbus5 qttools5-dev qttools5-dev-tools libqt5widgets5-dev
   sudo apt install -y git curl
   ```

### For Linux (Ubuntu/Debian)

```bash
sudo apt update
sudo apt install -y build-essential libtool autotools-dev automake pkg-config bsdmainutils python3
sudo apt install -y libssl-dev libevent-dev libboost-system-dev libboost-filesystem-dev libboost-chrono-dev libboost-program-options-dev libboost-test-dev libboost-thread-dev
sudo apt install -y libdb-dev libdb++-dev
sudo apt install -y libqt5gui5 libqt5core5a libqt5dbus5 qttools5-dev qttools5-dev-tools libqt5widgets5-dev
```

### For macOS

```bash
# Install Xcode Command Line Tools
xcode-select --install

# Install Homebrew if not already installed
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# Install dependencies
brew install autoconf automake libtool pkg-config boost libevent db@4 qt5
```

## 🔨 Building the Project

### Step 1: Generate Build Files

```bash
# Navigate to the project directory
cd /path/to/dogecoin-master

# Generate configure script
./autogen.sh

# Configure the build
./configure --with-gui=qt5

# Alternative: Enable debug mode
./configure --with-gui=qt5 --enable-debug
```

### Step 2: Compile

```bash
# Build the project (this may take 30-60 minutes)
make -j$(nproc)

# Or on macOS
make -j$(sysctl -n hw.ncpu)
```

### Step 3: Test the Build

```bash
# Run the GUI
./src/qt/dogecoin-qt

# Or run the daemon
./src/dogecoind
```

## 🎨 Testing the Modern UI

### 1. Launch the Application
```bash
./src/qt/dogecoin-qt
```

### 2. Access Theme Settings
- Go to `Settings` → `Theme` tab
- Try switching between Light and Dark themes
- Test the custom theme creator
- Adjust font settings

### 3. Test Modern Components
- Check the overview page for card-based layout
- Test the sidebar navigation
- Verify smooth animations and transitions
- Test responsive design by resizing the window

## 🐛 Troubleshooting

### Common Issues

#### 1. Missing Qt5 Development Libraries
```bash
# Ubuntu/Debian
sudo apt install libqt5widgets5-dev libqt5gui5-dev libqt5core5-dev

# CentOS/RHEL
sudo yum install qt5-qtbase-devel qt5-qttools-devel
```

#### 2. Boost Library Issues
```bash
# Make sure boost is properly installed
sudo apt install libboost-all-dev
```

#### 3. Berkeley DB Issues
```bash
# Install Berkeley DB
sudo apt install libdb-dev libdb++-dev
```

#### 4. Compilation Errors
If you get compilation errors related to our new files:

1. **Check file permissions**:
   ```bash
   chmod +x autogen.sh
   ```

2. **Clean and rebuild**:
   ```bash
   make clean
   ./autogen.sh
   ./configure --with-gui=qt5
   make -j$(nproc)
   ```

3. **Check for missing includes**:
   - Verify all our new `.h` and `.cpp` files are in `src/qt/`
   - Check that `src/Makefile.qt.include` has been updated

### Debug Mode

To build with debug information:
```bash
./configure --with-gui=qt5 --enable-debug
make -j$(nproc)
```

## 🧪 Testing the Modern UI Features

### Theme System Testing
1. **Light Theme**: Should have white backgrounds, dark text
2. **Dark Theme**: Should have dark backgrounds, light text
3. **Custom Themes**: Test color picker and live preview
4. **Font Customization**: Test different fonts and sizes

### Navigation Testing
1. **Sidebar Navigation**: Click through all menu items
2. **Compact Mode**: Test the toggle button
3. **Responsive Design**: Resize window and check layout

### Overview Page Testing
1. **Balance Cards**: Check if balances display correctly
2. **Transaction List**: Verify recent transactions show
3. **Network Status**: Check connection and sync status
4. **Quick Actions**: Test Send, Receive, History buttons

## 📊 Performance Testing

### Memory Usage
```bash
# Monitor memory usage while running
htop
# or
top
```

### CPU Usage
```bash
# Check CPU usage during theme switching
iostat 1
```

## 🎯 Expected Results

After successful compilation and testing, you should see:

1. **Modern Interface**: Clean, card-based design
2. **Theme Switching**: Smooth transitions between themes
3. **Dark Mode**: Proper contrast and readability
4. **Responsive Design**: Layout adapts to window size
5. **Smooth Animations**: Polished user interactions

## 🚀 Next Steps

Once the basic build is working:

1. **Test all features** thoroughly
2. **Report any bugs** you encounter
3. **Suggest improvements** for the UI
4. **Contribute enhancements** to the codebase

## 📞 Getting Help

If you encounter issues:

1. **Check the logs**: Look for error messages in the terminal
2. **Verify dependencies**: Make sure all required libraries are installed
3. **Clean build**: Try a clean rebuild if you get strange errors
4. **Check file permissions**: Ensure all files are readable

---

*Happy building! 🐕✨*
