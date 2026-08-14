#include "theme.h"

#include "imgui.h"

const char* ProThemeName(ProTheme theme)
{
    switch (theme) {
    case ProTheme::GoldDark: return "Gold Dark";
    case ProTheme::Matrix: return "Matrix";
    case ProTheme::Dim: return "Dim";
    }
    return "Gold Dark";
}

void ApplyProTheme(ProTheme theme)
{
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding = 8.0f;
    s.ChildRounding = 6.0f;
    s.FrameRounding = 4.0f;
    s.PopupRounding = 6.0f;
    s.ScrollbarRounding = 6.0f;
    s.GrabRounding = 4.0f;
    s.TabRounding = 4.0f;
    s.WindowPadding = ImVec2(10, 10);
    s.FramePadding = ImVec2(8, 5);
    s.ItemSpacing = ImVec2(8, 6);
    s.WindowBorderSize = 1.0f;
    s.FrameBorderSize = 0.0f;

    ImVec4* c = s.Colors;
    auto set = [&](ImGuiCol idx, float r, float g, float b, float a = 1.f) {
        c[idx] = ImVec4(r, g, b, a);
    };

    if (theme == ProTheme::Matrix) {
        set(ImGuiCol_WindowBg, 0.02f, 0.05f, 0.02f, 0.96f);
        set(ImGuiCol_ChildBg, 0.03f, 0.07f, 0.03f, 0.90f);
        set(ImGuiCol_PopupBg, 0.04f, 0.08f, 0.04f, 0.98f);
        set(ImGuiCol_Border, 0.1f, 0.55f, 0.2f, 0.55f);
        set(ImGuiCol_Text, 0.55f, 1.0f, 0.55f, 1.f);
        set(ImGuiCol_TextDisabled, 0.3f, 0.55f, 0.3f, 1.f);
        set(ImGuiCol_Header, 0.08f, 0.35f, 0.12f, 0.85f);
        set(ImGuiCol_HeaderHovered, 0.12f, 0.5f, 0.18f, 0.9f);
        set(ImGuiCol_HeaderActive, 0.15f, 0.6f, 0.22f, 1.f);
        set(ImGuiCol_Button, 0.08f, 0.3f, 0.12f, 0.85f);
        set(ImGuiCol_ButtonHovered, 0.12f, 0.45f, 0.18f, 0.95f);
        set(ImGuiCol_ButtonActive, 0.18f, 0.65f, 0.28f, 1.f);
        set(ImGuiCol_FrameBg, 0.05f, 0.12f, 0.06f, 0.9f);
        set(ImGuiCol_FrameBgHovered, 0.08f, 0.2f, 0.1f, 0.95f);
        set(ImGuiCol_TitleBg, 0.02f, 0.08f, 0.03f, 1.f);
        set(ImGuiCol_TitleBgActive, 0.05f, 0.18f, 0.07f, 1.f);
        set(ImGuiCol_Tab, 0.05f, 0.15f, 0.07f, 0.9f);
        set(ImGuiCol_TabHovered, 0.12f, 0.4f, 0.18f, 0.95f);
        set(ImGuiCol_TabActive, 0.1f, 0.35f, 0.15f, 1.f);
        set(ImGuiCol_CheckMark, 0.4f, 1.f, 0.45f, 1.f);
        set(ImGuiCol_SliderGrab, 0.3f, 0.85f, 0.35f, 1.f);
        set(ImGuiCol_Separator, 0.15f, 0.45f, 0.2f, 0.6f);
    } else if (theme == ProTheme::Dim) {
        ImGui::StyleColorsDark();
        set(ImGuiCol_WindowBg, 0.08f, 0.08f, 0.1f, 0.96f);
        set(ImGuiCol_Border, 0.35f, 0.3f, 0.2f, 0.4f);
        set(ImGuiCol_Header, 0.25f, 0.2f, 0.1f, 0.8f);
        set(ImGuiCol_Button, 0.22f, 0.18f, 0.1f, 0.85f);
    } else {
        // Gold Dark — default Core Pro spike
        set(ImGuiCol_Text, 0.95f, 0.93f, 0.88f, 1.f);
        set(ImGuiCol_TextDisabled, 0.5f, 0.48f, 0.4f, 1.f);
        set(ImGuiCol_WindowBg, 0.06f, 0.06f, 0.09f, 0.97f);
        set(ImGuiCol_ChildBg, 0.07f, 0.07f, 0.1f, 0.92f);
        set(ImGuiCol_PopupBg, 0.08f, 0.08f, 0.11f, 0.98f);
        set(ImGuiCol_Border, 0.72f, 0.55f, 0.18f, 0.45f);
        set(ImGuiCol_BorderShadow, 0, 0, 0, 0);
        set(ImGuiCol_FrameBg, 0.12f, 0.11f, 0.14f, 0.95f);
        set(ImGuiCol_FrameBgHovered, 0.18f, 0.15f, 0.12f, 0.95f);
        set(ImGuiCol_FrameBgActive, 0.22f, 0.18f, 0.1f, 1.f);
        set(ImGuiCol_TitleBg, 0.08f, 0.07f, 0.05f, 1.f);
        set(ImGuiCol_TitleBgActive, 0.16f, 0.12f, 0.05f, 1.f);
        set(ImGuiCol_TitleBgCollapsed, 0.06f, 0.05f, 0.04f, 0.8f);
        set(ImGuiCol_MenuBarBg, 0.09f, 0.08f, 0.07f, 1.f);
        set(ImGuiCol_ScrollbarBg, 0.05f, 0.05f, 0.06f, 0.8f);
        set(ImGuiCol_ScrollbarGrab, 0.55f, 0.42f, 0.15f, 0.7f);
        set(ImGuiCol_ScrollbarGrabHovered, 0.75f, 0.58f, 0.2f, 0.85f);
        set(ImGuiCol_ScrollbarGrabActive, 0.9f, 0.7f, 0.25f, 1.f);
        set(ImGuiCol_CheckMark, 0.95f, 0.78f, 0.25f, 1.f);
        set(ImGuiCol_SliderGrab, 0.85f, 0.65f, 0.2f, 1.f);
        set(ImGuiCol_SliderGrabActive, 1.f, 0.8f, 0.3f, 1.f);
        set(ImGuiCol_Button, 0.2f, 0.16f, 0.08f, 0.9f);
        set(ImGuiCol_ButtonHovered, 0.35f, 0.26f, 0.1f, 0.95f);
        set(ImGuiCol_ButtonActive, 0.55f, 0.4f, 0.12f, 1.f);
        set(ImGuiCol_Header, 0.25f, 0.18f, 0.06f, 0.85f);
        set(ImGuiCol_HeaderHovered, 0.4f, 0.3f, 0.1f, 0.9f);
        set(ImGuiCol_HeaderActive, 0.55f, 0.4f, 0.12f, 1.f);
        set(ImGuiCol_Separator, 0.55f, 0.42f, 0.15f, 0.5f);
        set(ImGuiCol_Tab, 0.14f, 0.12f, 0.08f, 0.9f);
        set(ImGuiCol_TabHovered, 0.4f, 0.3f, 0.1f, 0.95f);
        set(ImGuiCol_TabActive, 0.28f, 0.2f, 0.08f, 1.f);
        set(ImGuiCol_TabUnfocused, 0.1f, 0.09f, 0.07f, 0.9f);
        set(ImGuiCol_TabUnfocusedActive, 0.18f, 0.14f, 0.07f, 1.f);
        set(ImGuiCol_DockingPreview, 0.85f, 0.65f, 0.2f, 0.35f);
        set(ImGuiCol_DockingEmptyBg, 0.04f, 0.04f, 0.06f, 1.f);
        set(ImGuiCol_PlotLines, 0.9f, 0.7f, 0.2f, 1.f);
        set(ImGuiCol_PlotHistogram, 0.85f, 0.6f, 0.15f, 1.f);
        set(ImGuiCol_TableHeaderBg, 0.12f, 0.1f, 0.06f, 1.f);
        set(ImGuiCol_TableBorderStrong, 0.45f, 0.35f, 0.12f, 0.7f);
        set(ImGuiCol_TableBorderLight, 0.3f, 0.25f, 0.1f, 0.5f);
        set(ImGuiCol_NavHighlight, 0.9f, 0.7f, 0.2f, 1.f);
    }
}
