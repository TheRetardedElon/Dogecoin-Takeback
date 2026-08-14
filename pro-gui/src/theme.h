#pragma once

enum class ProTheme {
    GoldDark = 0,
    Matrix,
    Dim
};

void ApplyProTheme(ProTheme theme);
const char* ProThemeName(ProTheme theme);
