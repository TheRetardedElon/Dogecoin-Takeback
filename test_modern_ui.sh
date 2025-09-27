#!/bin/bash
# Test script for Modern UI features
# Run this after successful build

echo "🧪 Testing Modern UI Features"
echo "============================="

# Check if the executable exists
if [ ! -f "src/qt/dogecoin-qt" ]; then
    echo "❌ Error: dogecoin-qt not found. Please build first with ./build_dogecoin.sh"
    exit 1
fi

echo "✅ Found dogecoin-qt executable"

# Test 1: Check if the executable runs without crashing
echo "🔍 Test 1: Basic executable check"
if ./src/qt/dogecoin-qt --version > /dev/null 2>&1; then
    echo "✅ Executable runs and shows version"
else
    echo "⚠️  Executable may have issues (this is normal for GUI apps in headless mode)"
fi

# Test 2: Check for our new source files
echo "🔍 Test 2: Checking for Modern UI source files"
modern_files=(
    "src/qt/thememanager.h"
    "src/qt/thememanager.cpp"
    "src/qt/themeswitcher.h"
    "src/qt/themeswitcher.cpp"
    "src/qt/moderncard.h"
    "src/qt/moderncard.cpp"
    "src/qt/modernoverviewpage.h"
    "src/qt/modernoverviewpage.cpp"
    "src/qt/modernnavigation.h"
    "src/qt/modernnavigation.cpp"
    "src/qt/modernmainwindow.h"
    "src/qt/modernmainwindow.cpp"
)

all_files_exist=true
for file in "${modern_files[@]}"; do
    if [ -f "$file" ]; then
        echo "✅ $file"
    else
        echo "❌ $file - MISSING"
        all_files_exist=false
    fi
done

if [ "$all_files_exist" = true ]; then
    echo "✅ All Modern UI source files present"
else
    echo "❌ Some Modern UI source files are missing"
fi

# Test 3: Check Makefile integration
echo "🔍 Test 3: Checking build system integration"
if grep -q "qt/thememanager.cpp" src/Makefile.qt.include; then
    echo "✅ Theme manager integrated in build system"
else
    echo "❌ Theme manager not found in build system"
fi

if grep -q "qt/moc_thememanager.cpp" src/Makefile.qt.include; then
    echo "✅ Theme manager MOC files integrated"
else
    echo "❌ Theme manager MOC files not found"
fi

# Test 4: Check UI integration
echo "🔍 Test 4: Checking UI integration"
if grep -q "tabTheme" src/qt/forms/optionsdialog.ui; then
    echo "✅ Theme tab added to options dialog"
else
    echo "❌ Theme tab not found in options dialog"
fi

if grep -q "thememanager.h" src/qt/bitcoingui.cpp; then
    echo "✅ Theme manager integrated in main GUI"
else
    echo "❌ Theme manager not found in main GUI"
fi

# Test 5: Check documentation
echo "🔍 Test 5: Checking documentation"
if [ -f "MODERN_UI_README.md" ]; then
    echo "✅ Modern UI documentation present"
else
    echo "❌ Modern UI documentation missing"
fi

if [ -f "BUILD_GUIDE.md" ]; then
    echo "✅ Build guide present"
else
    echo "❌ Build guide missing"
fi

echo ""
echo "🎯 Manual Testing Instructions:"
echo "================================"
echo "1. Run: ./src/qt/dogecoin-qt"
echo "2. Go to Settings → Theme tab"
echo "3. Test theme switching (Light/Dark/Custom)"
echo "4. Check the overview page for modern card layout"
echo "5. Test the sidebar navigation"
echo "6. Verify responsive design by resizing the window"
echo ""
echo "Expected features:"
echo "- 🎨 Theme switching with live preview"
echo "- 🌙 Dark mode with proper contrast"
echo "- 🃏 Card-based balance display"
echo "- 📱 Responsive sidebar navigation"
echo "- ✨ Smooth animations and transitions"
echo ""
echo "🐕 Much modern, very theme, so sleek!"
