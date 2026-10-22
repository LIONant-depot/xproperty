#ifndef XPROPERTY_OK_COLOR_PICKER_H
#define XPROPERTY_OK_COLOR_PICKER_H
#pragma once

// The inspector's colour picker (color3_group / color4_group): picking in Okhsv or Okhsl (Bjorn Ottosson, https://bottosson.github.io/posts/colorpicker/),
// where equal steps look like equal steps and a hue keeps its lightness. A square (saturation x value, or saturation x lightness), a hue strip, an alpha
// strip for four channels, the colour before and now, and the numbers: RGB as stored, the Ok coordinates and hex.
//
// What the channels hold: float channels are linear (as xLION stores colours), uint8 channels are sRGB. Nothing is clamped: a float colour brighter
// than 1 is picked as its colour (its brightest channel brought to 1) times an Intensity, which is a number of its own.
#include "ok_color.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace xproperty::ui::ok_picker
{
    struct state
    {
        float   m_H = 0, m_S = 0, m_V = 0;      // Okhsv (h, s, v) or Okhsl (h, s, l): m_V is the third
        float   m_Intensity = 1;                 // float colours: what the colour is multiplied by
        float   m_Old[4] = {};                   // the colour when the picker opened
        bool    m_bHSL = false;
    };

    inline state& State() noexcept { static state S; return S; }

    inline float ToSRGB  (float x) noexcept { return x <= 0.0031308f ? 12.92f * x : 1.055f * std::pow(x, 1.0f / 2.4f) - 0.055f; }
    inline float ToLinear(float x) noexcept { return x <= 0.04045f ? x / 12.92f : std::pow((x + 0.055f) / 1.055f, 2.4f); }

    inline ImU32 U32(const ok_color::RGB& C, float A = 1.0f) noexcept
    {
        return ImGui::ColorConvertFloat4ToU32(ImVec4(std::clamp(C.r, 0.0f, 1.0f), std::clamp(C.g, 0.0f, 1.0f), std::clamp(C.b, 0.0f, 1.0f), A));
    }

    // The picker's coordinates -> sRGB (0..1)
    inline ok_color::RGB ToRGB(bool bHSL, float H, float S, float V) noexcept
    {
        return bHSL ? ok_color::okhsl_to_srgb({ H, S, V }) : ok_color::okhsv_to_srgb({ H, S, V });
    }

    // sRGB (0..1) -> the picker's coordinates; a grey keeps the hue it had (it has none of its own)
    inline void FromRGB(state& St, ok_color::RGB C) noexcept
    {
        const float Max = std::max({ C.r, C.g, C.b }), Min = std::min({ C.r, C.g, C.b });
        if (Max - Min < 1e-5f)
        {
            const float L = ok_color::linear_srgb_to_oklab({ ok_color::srgb_transfer_function_inv(C.r), ok_color::srgb_transfer_function_inv(C.g), ok_color::srgb_transfer_function_inv(C.b) }).L;
            St.m_S = 0;
            St.m_V = std::clamp(ok_color::toe(L), 0.0f, 1.0f);
            return;
        }
        if (St.m_bHSL) { const auto P = ok_color::srgb_to_okhsl(C); St.m_H = P.h; St.m_S = P.s; St.m_V = P.l; }
        else           { const auto P = ok_color::srgb_to_okhsv(C); St.m_H = P.h; St.m_S = P.s; St.m_V = P.v; }
        St.m_S = std::clamp(St.m_S, 0.0f, 1.0f);
        St.m_V = std::clamp(St.m_V, 0.0f, 1.0f);
    }

    // The stored channels -> sRGB (0..1) of the colour, its intensity apart
    inline ok_color::RGB StoredToRGB(const float C[4], bool bByte, float Intensity) noexcept
    {
        if (bByte) return { C[0], C[1], C[2] };
        const float k = Intensity > 0 ? 1.0f / Intensity : 0.0f;
        return { ToSRGB(std::max(0.0f, C[0] * k)), ToSRGB(std::max(0.0f, C[1] * k)), ToSRGB(std::max(0.0f, C[2] * k)) };
    }

    inline void RGBToStored(float C[4], ok_color::RGB P, bool bByte, float Intensity) noexcept
    {
        if (bByte) { C[0] = std::clamp(P.r, 0.0f, 1.0f); C[1] = std::clamp(P.g, 0.0f, 1.0f); C[2] = std::clamp(P.b, 0.0f, 1.0f); return; }
        C[0] = ToLinear(std::clamp(P.r, 0.0f, 1.0f)) * Intensity;
        C[1] = ToLinear(std::clamp(P.g, 0.0f, 1.0f)) * Intensity;
        C[2] = ToLinear(std::clamp(P.b, 0.0f, 1.0f)) * Intensity;
    }

    // Reads the colour into the picker's state (when it opens, or after its numbers were typed)
    inline void Load(state& St, const float C[4], bool bByte) noexcept
    {
        St.m_Intensity = bByte ? 1.0f : std::max(1.0f, std::max({ C[0], C[1], C[2] }));
        FromRGB(St, StoredToRGB(C, bByte, St.m_Intensity));
    }

    inline void DrawChecker(ImDrawList* pDL, ImVec2 A, ImVec2 B, float Cell) noexcept
    {
        pDL->AddRectFilled(A, B, IM_COL32(204, 204, 204, 255));
        for (float y = A.y; y < B.y; y += Cell)
            for (float x = A.x + (static_cast<int>((y - A.y) / Cell) % 2) * Cell; x < B.x; x += Cell * 2)
                pDL->AddRectFilled(ImVec2(x, y), ImVec2(std::min(x + Cell, B.x), std::min(y + Cell, B.y)), IM_COL32(128, 128, 128, 255));
    }

    // The picker, inside a popup the caller has begun. C: the stored channels (uint8 ones as 0..1); true when they changed this frame.
    inline bool Picker(float C[4], int nChannels, bool bByte) noexcept
    {
        auto&       St      = State();
        const bool  bAlpha  = nChannels == 4;
        bool        bChanged = false;
        bool        bFromState = false;                 // the square, the strips or the Ok numbers moved: the colour is made from the state

        if (ImGui::IsWindowAppearing())
        {
            std::copy(C, C + 4, St.m_Old);
            Load(St, C, bByte);
        }
        else if (!ImGui::IsAnyItemActive())             // the colour changed under the picker (an undo, its first frame): the state follows it
        {
            float Mine[4] = { C[0], C[1], C[2], C[3] };
            RGBToStored(Mine, ToRGB(St.m_bHSL, St.m_H, St.m_S, St.m_V), bByte, St.m_Intensity);
            for (int i = 0; i < 3; ++i) if (std::fabs(Mine[i] - C[i]) > (bByte ? 0.5f / 255.0f : 1e-4f)) { Load(St, C, bByte); break; }
        }

        // What the dialog draws: a float colour as it is, its intensity included (linear times the intensity, clamped only to be shown)
        const auto Shown = [&](ok_color::RGB P) noexcept -> ok_color::RGB
        {
            if (bByte || St.m_Intensity == 1.0f) return P;
            const auto f = [&](float x) { return ToSRGB(std::clamp(ToLinear(std::clamp(x, 0.0f, 1.0f)) * St.m_Intensity, 0.0f, 1.0f)); };
            return { f(P.r), f(P.g), f(P.b) };
        };

        ImDrawList* pDL   = ImGui::GetWindowDrawList();
        const float Line  = ImGui::GetFrameHeight();
        const float Size  = Line * 9.0f;
        const float Strip = Line * 0.9f;
        const auto& Style = ImGui::GetStyle();

        // Model
        if (ImGui::RadioButton("Okhsv", !St.m_bHSL)) { const auto P = ToRGB(St.m_bHSL, St.m_H, St.m_S, St.m_V); St.m_bHSL = false; FromRGB(St, P); }
        ImGui::SameLine();
        if (ImGui::RadioButton("Okhsl", St.m_bHSL))  { const auto P = ToRGB(St.m_bHSL, St.m_H, St.m_S, St.m_V); St.m_bHSL = true;  FromRGB(St, P); }

        // The square: saturation across, value (or lightness) up
        const ImVec2 SQ = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("##OkSquare", ImVec2(Size, Size));
        if (ImGui::IsItemActive())
        {
            const ImVec2 M = ImGui::GetIO().MousePos;
            St.m_S = std::clamp((M.x - SQ.x) / Size, 0.0f, 1.0f);
            St.m_V = std::clamp(1.0f - (M.y - SQ.y) / Size, 0.0f, 1.0f);
            bFromState = true;
        }
        {
            constexpr int N = 24;
            const float   d = Size / N;
            for (int j = 0; j < N; ++j)
                for (int i = 0; i < N; ++i)
                {
                    const float s0 = float(i) / N, s1 = float(i + 1) / N, v0 = 1.0f - float(j) / N, v1 = 1.0f - float(j + 1) / N;
                    pDL->AddRectFilledMultiColor(ImVec2(SQ.x + i * d, SQ.y + j * d), ImVec2(SQ.x + (i + 1) * d + 0.5f, SQ.y + (j + 1) * d + 0.5f)
                        , U32(Shown(ToRGB(St.m_bHSL, St.m_H, s0, v0))), U32(Shown(ToRGB(St.m_bHSL, St.m_H, s1, v0))), U32(Shown(ToRGB(St.m_bHSL, St.m_H, s1, v1))), U32(Shown(ToRGB(St.m_bHSL, St.m_H, s0, v1))));
                }
            const ImVec2 P(SQ.x + St.m_S * Size, SQ.y + (1.0f - St.m_V) * Size);
            pDL->AddCircle(P, Line * 0.3f, IM_COL32(0, 0, 0, 255), 0, 3.0f);
            pDL->AddCircle(P, Line * 0.3f, IM_COL32(255, 255, 255, 255), 0, 1.5f);
        }

        // The hue strip
        ImGui::SameLine();
        const ImVec2 HQ = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("##OkHue", ImVec2(Strip, Size));
        if (ImGui::IsItemActive()) { St.m_H = std::clamp((ImGui::GetIO().MousePos.y - HQ.y) / Size, 0.0f, 0.9999f); bFromState = true; }
        {
            constexpr int N = 48;
            const float   d = Size / N;
            const float   s = 1.0f, v = St.m_bHSL ? 0.65f : 1.0f;         // full saturation, the lightness each model shows hues best at
            for (int j = 0; j < N; ++j)
            {
                const ImU32 T = U32(ToRGB(St.m_bHSL, float(j) / N, s, v)), B = U32(ToRGB(St.m_bHSL, float(j + 1) / N, s, v));
                pDL->AddRectFilledMultiColor(ImVec2(HQ.x, HQ.y + j * d), ImVec2(HQ.x + Strip, HQ.y + (j + 1) * d + 0.5f), T, T, B, B);
            }
            const float y = HQ.y + St.m_H * Size;
            pDL->AddRect(ImVec2(HQ.x - 1, y - 2), ImVec2(HQ.x + Strip + 1, y + 2), IM_COL32(255, 255, 255, 255), 0, 0, 2.0f);
        }

        // The alpha strip
        const auto Now = ToRGB(St.m_bHSL, St.m_H, St.m_S, St.m_V);            // the colour (its intensity apart: the hex and the numbers)
        const auto NowShown = Shown(Now);
        if (bAlpha)
        {
            ImGui::SameLine();
            const ImVec2 AQ = ImGui::GetCursorScreenPos();
            ImGui::InvisibleButton("##OkAlpha", ImVec2(Strip, Size));
            if (ImGui::IsItemActive()) { C[3] = std::clamp(1.0f - (ImGui::GetIO().MousePos.y - AQ.y) / Size, 0.0f, 1.0f); bChanged = true; }
            DrawChecker(pDL, AQ, ImVec2(AQ.x + Strip, AQ.y + Size), Strip * 0.5f);
            pDL->AddRectFilledMultiColor(AQ, ImVec2(AQ.x + Strip, AQ.y + Size), U32(NowShown, 1), U32(NowShown, 1), U32(NowShown, 0), U32(NowShown, 0));
            const float y = AQ.y + (1.0f - std::clamp(C[3], 0.0f, 1.0f)) * Size;
            pDL->AddRect(ImVec2(AQ.x - 1, y - 2), ImVec2(AQ.x + Strip + 1, y + 2), IM_COL32(255, 255, 255, 255), 0, 0, 2.0f);
        }

        // Before and now: the colour (opaque), and under it, apart, the alpha (white over a checker). Clicking the one from before puts it back
        {
            const float  W   = (Size + Strip * (bAlpha ? 2 : 1) + Style.ItemSpacing.x * (bAlpha ? 2 : 1)) * 0.5f;
            const float  AH  = bAlpha ? Line * 0.5f : 0.0f;
            const ImVec2 P   = ImGui::GetCursorScreenPos();
            const auto   Old = StoredToRGB(St.m_Old, bByte, 1.0f);       // as it was, its brightness included (clamped to be shown)
            pDL->AddRectFilled(P, ImVec2(P.x + W, P.y + Line), U32(Old));
            pDL->AddRectFilled(ImVec2(P.x + W, P.y), ImVec2(P.x + W * 2, P.y + Line), U32(NowShown));
            if (bAlpha)
            {
                const ImVec2 A(P.x, P.y + Line);
                DrawChecker(pDL, A, ImVec2(A.x + W * 2, A.y + AH), AH * 0.5f);
                pDL->AddRectFilled(A, ImVec2(A.x + W, A.y + AH), IM_COL32(255, 255, 255, int(std::clamp(St.m_Old[3], 0.0f, 1.0f) * 255 + 0.5f)));
                pDL->AddRectFilled(ImVec2(A.x + W, A.y), ImVec2(A.x + W * 2, A.y + AH), IM_COL32(255, 255, 255, int(std::clamp(C[3], 0.0f, 1.0f) * 255 + 0.5f)));
            }
            ImGui::InvisibleButton("##OkOld", ImVec2(W, Line + AH));
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Before: click to put it back");
            if (ImGui::IsItemClicked()) { std::copy(St.m_Old, St.m_Old + 4, C); Load(St, C, bByte); bChanged = true; }
            ImGui::SameLine(0, 0);
            ImGui::Dummy(ImVec2(W, Line + AH));
        }

        // The numbers
        const float FieldW = Size + Strip * (bAlpha ? 2 : 1) + Style.ItemSpacing.x * (bAlpha ? 2 : 1);
        ImGui::PushItemWidth(FieldW - ImGui::CalcTextSize("Intensity").x - Style.ItemInnerSpacing.x);
        {
            float Ok[3] = { St.m_H * 360.0f, St.m_S * 100.0f, St.m_V * 100.0f };
            if (ImGui::DragFloat3(St.m_bHSL ? "H S L" : "H S V", Ok, 0.5f, 0.0f, 0.0f, "%.1f"))
            {
                St.m_H = std::clamp(Ok[0] / 360.0f, 0.0f, 0.9999f); St.m_S = std::clamp(Ok[1] / 100.0f, 0.0f, 1.0f); St.m_V = std::clamp(Ok[2] / 100.0f, 0.0f, 1.0f);
                bFromState = true;
            }
        }
        if (bByte)
        {
            int B[4] = { int(C[0] * 255 + 0.5f), int(C[1] * 255 + 0.5f), int(C[2] * 255 + 0.5f), int(C[3] * 255 + 0.5f) };
            if (bAlpha ? ImGui::DragInt4("RGBA", B, 0.5f, 0, 255) : ImGui::DragInt3("RGB", B, 0.5f, 0, 255))
            {
                for (int i = 0; i < nChannels; ++i) C[i] = std::clamp(B[i], 0, 255) / 255.0f;
                Load(St, C, bByte);
                bChanged = true;
            }
        }
        else if (bAlpha ? ImGui::DragFloat4("RGBA", C, 0.005f, 0.0f, 0.0f, "%.3f") : ImGui::DragFloat3("RGB", C, 0.005f, 0.0f, 0.0f, "%.3f"))
        {
            Load(St, C, bByte);
            bChanged = true;
        }
        {
            char Hex[16];
            const auto H = [](float x) { return int(std::clamp(x, 0.0f, 1.0f) * 255 + 0.5f); };
            if (bAlpha) std::snprintf(Hex, sizeof(Hex), "#%02X%02X%02X%02X", H(Now.r), H(Now.g), H(Now.b), H(C[3]));
            else        std::snprintf(Hex, sizeof(Hex), "#%02X%02X%02X", H(Now.r), H(Now.g), H(Now.b));
            if (ImGui::InputText("Hex (sRGB)", Hex, sizeof(Hex), ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CharsUppercase))
            {
                unsigned R = 0, G = 0, B = 0, A = 255;
                const char* p = Hex[0] == '#' ? Hex + 1 : Hex;
                const int n = std::sscanf(p, "%02x%02x%02x%02x", &R, &G, &B, &A);
                if (n >= 3)
                {
                    RGBToStored(C, { R / 255.0f, G / 255.0f, B / 255.0f }, bByte, St.m_Intensity);
                    if (bAlpha && n == 4) C[3] = A / 255.0f;
                    Load(St, C, bByte);
                    bChanged = true;
                }
            }
        }
        if (!bByte && ImGui::DragFloat("Intensity", &St.m_Intensity, 0.01f, 0.0f, FLT_MAX, "%.3f"))
        {
            St.m_Intensity = std::max(0.0f, St.m_Intensity);
            bFromState = true;
        }
        ImGui::PopItemWidth();

        if (bFromState)
        {
            RGBToStored(C, ToRGB(St.m_bHSL, St.m_H, St.m_S, St.m_V), bByte, St.m_Intensity);
            bChanged = true;
        }
        return bChanged;
    }
}

#endif
