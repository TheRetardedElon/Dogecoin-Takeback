#!/usr/bin/env python3
"""
Verification script for Modern UI components
Checks that all required files exist and have proper structure
"""

import os
import sys

def check_file_exists(filepath, description):
    """Check if a file exists and report status"""
    if os.path.exists(filepath):
        print(f"✅ {description}: {filepath}")
        return True
    else:
        print(f"❌ {description}: {filepath} - MISSING")
        return False

def check_file_content(filepath, required_strings, description):
    """Check if file contains required strings"""
    if not os.path.exists(filepath):
        print(f"❌ {description}: {filepath} - FILE NOT FOUND")
        return False
    
    try:
        with open(filepath, 'r', encoding='utf-8') as f:
            content = f.read()
        
        missing = []
        for req_str in required_strings:
            if req_str not in content:
                missing.append(req_str)
        
        if missing:
            print(f"⚠️  {description}: {filepath} - Missing: {', '.join(missing)}")
            return False
        else:
            print(f"✅ {description}: {filepath}")
            return True
    except Exception as e:
        print(f"❌ {description}: {filepath} - ERROR: {e}")
        return False

def main():
    print("🔍 Verifying Modern UI Implementation")
    print("=" * 50)
    
    # Check core files
    core_files = [
        ("src/qt/thememanager.h", "Theme Manager Header"),
        ("src/qt/thememanager.cpp", "Theme Manager Implementation"),
        ("src/qt/themeswitcher.h", "Theme Switcher Header"),
        ("src/qt/themeswitcher.cpp", "Theme Switcher Implementation"),
        ("src/qt/moderncard.h", "Modern Card Header"),
        ("src/qt/moderncard.cpp", "Modern Card Implementation"),
        ("src/qt/modernoverviewpage.h", "Modern Overview Header"),
        ("src/qt/modernoverviewpage.cpp", "Modern Overview Implementation"),
        ("src/qt/modernnavigation.h", "Modern Navigation Header"),
        ("src/qt/modernnavigation.cpp", "Modern Navigation Implementation"),
        ("src/qt/modernmainwindow.h", "Modern Main Window Header"),
        ("src/qt/modernmainwindow.cpp", "Modern Main Window Implementation"),
    ]
    
    all_good = True
    for filepath, description in core_files:
        if not check_file_exists(filepath, description):
            all_good = False
    
    print("\n🔧 Checking Build Integration")
    print("-" * 30)
    
    # Check Makefile integration
    makefile_checks = [
        ("src/Makefile.qt.include", ["qt/thememanager.cpp", "qt/themeswitcher.cpp", "qt/moderncard.cpp"], "Makefile.qt.include - Source Files"),
        ("src/Makefile.qt.include", ["qt/moc_thememanager.cpp", "qt/moc_themeswitcher.cpp", "qt/moc_moderncard.cpp"], "Makefile.qt.include - MOC Files"),
    ]
    
    for filepath, required_strings, description in makefile_checks:
        if not check_file_content(filepath, required_strings, description):
            all_good = False
    
    print("\n🎨 Checking UI Integration")
    print("-" * 30)
    
    # Check UI integration
    ui_checks = [
        ("src/qt/forms/optionsdialog.ui", ["tabTheme", "ThemeSwitcher"], "Options Dialog - Theme Tab"),
        ("src/qt/optionsdialog.cpp", ["themeswitcher.h", "ThemeSwitcher"], "Options Dialog - Theme Integration"),
        ("src/qt/bitcoingui.cpp", ["thememanager.h", "ThemeManager"], "Main GUI - Theme Integration"),
    ]
    
    for filepath, required_strings, description in ui_checks:
        if not check_file_content(filepath, required_strings, description):
            all_good = False
    
    print("\n📚 Checking Documentation")
    print("-" * 30)
    
    # Check documentation
    doc_files = [
        ("MODERN_UI_README.md", "Modern UI Documentation"),
        ("BUILD_GUIDE.md", "Build Guide"),
    ]
    
    for filepath, description in doc_files:
        if not check_file_exists(filepath, description):
            all_good = False
    
    print("\n" + "=" * 50)
    if all_good:
        print("🎉 All checks passed! Modern UI implementation is ready.")
        print("\nNext steps:")
        print("1. Follow BUILD_GUIDE.md to compile the project")
        print("2. Test the theme switching functionality")
        print("3. Verify the modern UI components work correctly")
    else:
        print("❌ Some issues found. Please check the errors above.")
        print("\nCommon fixes:")
        print("1. Ensure all files are in the correct directories")
        print("2. Check that Makefile.qt.include has been updated")
        print("3. Verify UI file modifications are correct")
    
    return 0 if all_good else 1

if __name__ == "__main__":
    sys.exit(main())
