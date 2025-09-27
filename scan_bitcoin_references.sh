#!/bin/bash

echo "=== DOGECOIN BITCOIN REFERENCE SCANNER ==="
echo "Scanning all source files for Bitcoin references that need migration..."
echo ""

# Initialize counters
total_files=0
files_with_bitcoin=0
issues_found=0

echo "📁 SCANNING INCLUDE STATEMENTS..."
echo "=================================="

# Scan for bitcoin includes in header files
echo "🔍 Checking .h files for bitcoin includes:"
find src -name "*.h" -type f | while read file; do
    total_files=$((total_files + 1))
    if grep -q "#include.*bitcoin" "$file"; then
        files_with_bitcoin=$((files_with_bitcoin + 1))
        issues_found=$((issues_found + 1))
        echo "❌ ISSUE FOUND: $file"
        echo "   Contains: $(grep "#include.*bitcoin" "$file")"
        echo ""
    fi
done

# Scan for bitcoin includes in C/C++ files
echo "🔍 Checking .c/.cpp files for bitcoin includes:"
find src -name "*.c" -o -name "*.cpp" | while read file; do
    total_files=$((total_files + 1))
    if grep -q "#include.*bitcoin" "$file"; then
        files_with_bitcoin=$((files_with_bitcoin + 1))
        issues_found=$((issues_found + 1))
        echo "❌ ISSUE FOUND: $file"
        echo "   Contains: $(grep "#include.*bitcoin" "$file")"
        echo ""
    fi
done

echo ""
echo "📋 SCANNING CLASS/STRUCT REFERENCES..."
echo "======================================"

# Scan for Bitcoin class references
echo "🔍 Checking for Bitcoin class references:"
find src -name "*.h" -o -name "*.cpp" -o -name "*.c" | while read file; do
    if grep -q "Bitcoin[A-Z][a-zA-Z]*" "$file"; then
        issues_found=$((issues_found + 1))
        echo "❌ ISSUE FOUND: $file"
        echo "   Contains Bitcoin classes: $(grep -o "Bitcoin[A-Z][a-zA-Z]*" "$file" | sort -u | tr '\n' ' ')"
        echo ""
    fi
done

echo "🔍 Checking for CBitcoin references:"
find src -name "*.h" -o -name "*.cpp" -o -name "*.c" | while read file; do
    if grep -q "CBitcoin[A-Z][a-zA-Z]*" "$file"; then
        issues_found=$((issues_found + 1))
        echo "❌ ISSUE FOUND: $file"
        echo "   Contains CBitcoin classes: $(grep -o "CBitcoin[A-Z][a-zA-Z]*" "$file" | sort -u | tr '\n' ' ')"
        echo ""
    fi
done

echo ""
echo "📝 SCANNING FUNCTION/METHOD REFERENCES..."
echo "========================================="

# Scan for Bitcoin function references
echo "🔍 Checking for Bitcoin function references:"
find src -name "*.h" -o -name "*.cpp" -o -name "*.c" | while read file; do
    if grep -q "bitcoin[A-Z][a-zA-Z]*\|Bitcoin[A-Z][a-zA-Z]*" "$file"; then
        bitcoin_refs=$(grep -o "bitcoin[A-Z][a-zA-Z]*\|Bitcoin[A-Z][a-zA-Z]*" "$file" | sort -u)
        if [ ! -z "$bitcoin_refs" ]; then
            issues_found=$((issues_found + 1))
            echo "❌ ISSUE FOUND: $file"
            echo "   Contains Bitcoin references: $bitcoin_refs"
            echo ""
        fi
    fi
done

echo ""
echo "🔧 SCANNING BUILD SYSTEM REFERENCES..."
echo "======================================"

# Scan Makefiles and build files
echo "🔍 Checking Makefiles for bitcoin references:"
find . -name "Makefile*" -o -name "*.mk" -o -name "*.am" -o -name "*.in" | while read file; do
    if grep -q "bitcoin" "$file"; then
        issues_found=$((issues_found + 1))
        echo "❌ ISSUE FOUND: $file"
        echo "   Contains bitcoin references: $(grep "bitcoin" "$file" | head -3)"
        echo ""
    fi
done

echo ""
echo "📊 SCANNING SUMMARY..."
echo "====================="

echo "Total files scanned: $total_files"
echo "Files with Bitcoin references: $files_with_bitcoin"
echo "Total issues found: $issues_found"

echo ""
echo "🎯 PRIORITY INVESTIGATION LIST:"
echo "==============================="

# Create a priority list of the most critical files to fix
echo "🔴 CRITICAL - Include statements that will cause compilation errors:"
find src -name "*.h" -o -name "*.cpp" -o -name "*.c" | xargs grep -l "#include.*bitcoin" 2>/dev/null | head -10

echo ""
echo "🟡 HIGH - Bitcoin class references that may cause linking errors:"
find src -name "*.h" -o -name "*.cpp" -o -name "*.c" | xargs grep -l "Bitcoin[A-Z][a-zA-Z]*" 2>/dev/null | head -10

echo ""
echo "🟢 MEDIUM - Build system references:"
find . -name "Makefile*" -o -name "*.mk" -o -name "*.am" -o -name "*.in" | xargs grep -l "bitcoin" 2>/dev/null | head -5

echo ""
if [ $issues_found -eq 0 ]; then
    echo "✅ SUCCESS: No Bitcoin references found! Migration appears complete."
else
    echo "⚠️  WARNING: $issues_found issues found that need attention."
    echo "   Review the files above and fix the Bitcoin references."
fi

echo ""
echo "🔧 QUICK FIX COMMANDS:"
echo "======================"
echo "# To fix all bitcoin includes:"
echo "find src -name '*.h' -o -name '*.cpp' -o -name '*.c' | xargs sed -i 's/#include \"bitcoin/#include \"dogecoin/g'"
echo ""
echo "# To fix Bitcoin class references:"
echo "find src -name '*.h' -o -name '*.cpp' -o -name '*.c' | xargs sed -i 's/Bitcoin[A-Z][a-zA-Z]*/Dogecoin&/g'"
echo ""
echo "# To fix CBitcoin class references:"
echo "find src -name '*.h' -o -name '*.cpp' -o -name '*.c' | xargs sed -i 's/CBitcoin/CDogecoin/g'"

echo ""
echo "=== SCAN COMPLETE ==="
