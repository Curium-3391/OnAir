// The tuner selector styles: the passband / tuning overlay drawn over the spectrum and the waterfall (Settings > Display > Tuner style).
// 0 is the classic band (plots.cpp draws it); 1 Slab, 2 Brackets, 3 Ruler flag are drawn here with the ImGui draw list, clipped to the plot,
// in the app's flat look: 1 px lines, monospace text, the accent colour, no glow, no gradients, no rounded corners.
// Only drawing: no App, no plot library calls (plots.cpp passes the plot's rectangle and the pixel columns of the band).
#pragma once
#include "imgui.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace tstyle {

constexpr int kNumStyles = 4;

struct In {
    ImVec2 pmin, psize;          // the plot area on the screen
    double xLo = 0, xHi = 0, xC = 0;   // the band's edges and the tuned frequency, in screen pixels
    float bottomReserve = 0;     // the room kept free at the bottom for the frequency labels (the spectrum has them inside the plot)
    bool spectrum = true;        // false: the waterfall (no readout text, the labels are not there)
    bool dragging = false;       // an edge is being dragged
    int hover = -1;              // the edge under the mouse: 0 lower, 1 upper, -1 none
    bool settable = false;       // the width can be changed by dragging the edges (the grips look live)
    float sc = 1.f;              // interface scale
    ImFont* mono = nullptr;
    ImU32 accent = IM_COL32(236, 165, 58, 255);
    char readout[96] = "";       // "98.100 MHz  12.5 kHz"
};

inline ImU32 withA(ImU32 c, float a) { return (c & 0x00FFFFFF) | ((ImU32)(std::min(1.f, std::max(0.f, a)) * 255.f) << 24); }

inline void dashV(ImDrawList* dl, float x, float y0, float y1, ImU32 col, float on, float off) {
    for (float y = y0; y < y1; y += on + off) dl->AddLine(ImVec2(x, y), ImVec2(x, std::min(y1, y + on)), col, 1.f);
}

// The readout in a small flat tag. Placed with its left edge at x, clamped into [lo, hi]; bg 0: no box.
inline void tag(ImDrawList* dl, const In& I, float x, float y, float lo, float hi, bool centre, ImU32 bg, ImU32 border) {
    ImFont* f = I.mono ? I.mono : ImGui::GetFont();
    const float fs = ImGui::GetFontSize() * 0.92f, pad = 4 * I.sc;
    const ImVec2 ts = f->CalcTextSizeA(fs, FLT_MAX, 0, I.readout);
    float bx = centre ? x - (ts.x + 2 * pad) / 2 : x;
    bx = std::max(lo, std::min(hi - ts.x - 2 * pad, bx));
    const ImVec2 a(std::floor(bx), std::floor(y)), b(std::floor(bx + ts.x + 2 * pad), std::floor(y + ts.y + 2 * pad * 0.5f));
    if (bg >> 24) dl->AddRectFilled(a, b, bg);
    if (border >> 24) dl->AddRect(a, b, border, 0.f, 0, 1.f);
    dl->AddText(f, fs, ImVec2(a.x + pad, a.y + pad * 0.25f), IM_COL32(232, 234, 236, 245), I.readout);
}

// Style 1, Slab. under: the fill only (drawn before the trace); the rest is drawn on top.
inline void slab(ImDrawList* dl, const In& I, bool under) {
    const float top = I.pmin.y, bot = I.pmin.y + I.psize.y - I.bottomReserve, s = I.sc;
    const float xl = std::floor((float)I.xLo) + 0.5f, xh = std::floor((float)I.xHi) + 0.5f, xc = std::floor((float)I.xC) + 0.5f;
    const ImU32 pale = IM_COL32(214, 218, 222, 255);
    if (under) { dl->AddRectFilled(ImVec2(xl, top), ImVec2(xh, bot), IM_COL32(200, 206, 212, I.spectrum ? 26 : 18)); return; }
    const float eA = I.dragging ? 0.95f : 0.62f;
    dl->AddLine(ImVec2(xl, top), ImVec2(xl, bot), withA(pale, I.hover == 0 ? 1.f : eA), 1.f);
    dl->AddLine(ImVec2(xh, top), ImVec2(xh, bot), withA(pale, I.hover == 1 ? 1.f : eA), 1.f);
    dashV(dl, xc, top, bot, withA(pale, 0.55f), 4 * s, 4 * s);
    const float g = std::round(7 * s);
    if (xh - xl >= 2 * g) {   // grip handles (square) at the top of both edges
        for (int e = 0; e < 2; e++) {
            const float x = e ? xh : xl;
            const bool hot = I.hover == e || (I.dragging && I.hover == e);
            const ImVec2 a(std::floor(x - g / 2), top + 1), b(std::floor(x - g / 2) + g, top + 1 + g);
            dl->AddRectFilled(a, b, withA(pale, hot ? 1.f : (I.settable ? 0.85f : 0.5f)));
            dl->AddRect(a, b, IM_COL32(20, 21, 23, 200), 0.f, 0, 1.f);
        }
    }
    if (I.spectrum) {
        const float hw = 5 * s;   // the accent pointer, its base on the axis line
        dl->AddTriangleFilled(ImVec2(xc - hw, bot), ImVec2(xc + hw, bot), ImVec2(xc, bot - 8 * s), I.accent);
        tag(dl, I, xc, top + g + 6 * s, I.pmin.x + 2, I.pmin.x + I.psize.x - 2, true, IM_COL32(18, 19, 21, 225), IM_COL32(214, 218, 222, 90));
    }
}

// Style 2, Brackets: no fill; corner brackets at the passband edges (top and bottom), the bandwidth as a solid bar on the axis.
inline void brackets(ImDrawList* dl, const In& I, bool under) {
    if (under) return;
    const float top = I.pmin.y, bot = I.pmin.y + I.psize.y - I.bottomReserve, s = I.sc;
    const float xl = std::floor((float)I.xLo) + 0.5f, xh = std::floor((float)I.xHi) + 0.5f, xc = std::floor((float)I.xC) + 0.5f;
    const ImU32 pale = IM_COL32(226, 229, 232, 255);
    const float arm = std::round(16 * s), lip = std::round(8 * s), w = 1.5f;
    for (int e = 0; e < 2; e++) {
        const float x = e ? xh : xl, d = e ? -1.f : 1.f;   // d: towards the centre
        const bool hot = I.hover == e || I.dragging;
        const ImU32 c = withA(pale, hot ? 1.f : 0.85f);
        dl->AddLine(ImVec2(x, top), ImVec2(x, top + arm), c, w); dl->AddLine(ImVec2(x, top), ImVec2(x + d * lip, top), c, w);   // top
        dl->AddLine(ImVec2(x, bot), ImVec2(x, bot - arm), c, w); dl->AddLine(ImVec2(x, bot), ImVec2(x + d * lip, bot), c, w);   // bottom
        dashV(dl, x, top + arm + 3 * s, bot - arm - 3 * s, withA(pale, hot ? 0.55f : 0.22f), 2 * s, 5 * s);   // the edge between the brackets, barely there
    }
    const float bh = std::max(2.f, std::round(3 * s));
    dl->AddRectFilled(ImVec2(xl, bot - bh), ImVec2(std::max(xh, xl + 2), bot), I.accent);   // the bandwidth along the axis
    const float tk = std::round(9 * s);
    dl->AddLine(ImVec2(xc, bot - bh - tk), ImVec2(xc, bot - bh), withA(pale, 0.95f), 1.f);   // centre tick
    dl->AddLine(ImVec2(xc, top), ImVec2(xc, top + tk), withA(pale, 0.95f), 1.f);
    if (I.spectrum) {
        ImFont* f = I.mono ? I.mono : ImGui::GetFont();
        const float tw = f->CalcTextSizeA(ImGui::GetFontSize() * 0.92f, FLT_MAX, 0, I.readout).x + 8 * s;
        const bool right = xh + 8 * s + tw <= I.pmin.x + I.psize.x - 2;
        const float th = f->CalcTextSizeA(ImGui::GetFontSize() * 0.92f, FLT_MAX, 0, I.readout).y + 4 * s;
        tag(dl, I, right ? xh + 8 * s : xl - 8 * s - tw, bot - th - 4 * s, I.pmin.x + 2, I.pmin.x + I.psize.x - 2, false, IM_COL32(18, 19, 21, 200), 0);
    }
}

// Style 3, Ruler flag: a thin strip along the top with a flag as wide as the band (arrows at both ends), faint lines through the plot.
inline void ruler(ImDrawList* dl, const In& I, bool under) {
    const float top = I.pmin.y, bot = I.pmin.y + I.psize.y - I.bottomReserve, s = I.sc;
    const float xl = std::floor((float)I.xLo) + 0.5f, xh = std::floor((float)I.xHi) + 0.5f, xc = std::floor((float)I.xC) + 0.5f;
    const float rh = std::round(13 * s), px0 = I.pmin.x, px1 = I.pmin.x + I.psize.x;
    const ImU32 pale = IM_COL32(220, 224, 228, 255);
    if (under) { dl->AddRectFilled(ImVec2(xl, top + rh), ImVec2(xh, bot), IM_COL32(200, 206, 212, I.spectrum ? 9 : 7)); return; }
    dl->AddRectFilled(ImVec2(px0, top), ImVec2(px1, top + rh), IM_COL32(16, 17, 19, 205));
    dl->AddLine(ImVec2(px0, top + rh), ImVec2(px1, top + rh), withA(pale, 0.22f), 1.f);
    const float eA = I.dragging ? 0.8f : 0.34f;
    dl->AddLine(ImVec2(xl, top + rh), ImVec2(xl, bot), withA(pale, I.hover == 0 ? 0.9f : eA), 1.f);
    dl->AddLine(ImVec2(xh, top + rh), ImVec2(xh, bot), withA(pale, I.hover == 1 ? 0.9f : eA), 1.f);
    dl->AddLine(ImVec2(xc, top + rh), ImVec2(xc, bot), withA(pale, 0.2f), 1.f);
    const float fl = std::max(xl, px0), fh = std::min(xh, px1), y0 = top + 2 * s, y1 = top + rh - 2 * s, ar = std::round(4 * s);
    if (fh - fl >= 1) dl->AddRectFilled(ImVec2(std::floor(fl), y0), ImVec2(std::floor(std::max(fh, fl + 2)), y1), withA(I.accent, 0.88f));
    const float ym = (y0 + y1) / 2;   // arrows at both ends, pointing outwards
    if (xl - ar >= px0) dl->AddTriangleFilled(ImVec2(xl, y0), ImVec2(xl, y1), ImVec2(xl - ar, ym), withA(I.accent, I.hover == 0 ? 1.f : 0.88f));
    if (xh + ar <= px1) dl->AddTriangleFilled(ImVec2(xh, y0), ImVec2(xh, y1), ImVec2(xh + ar, ym), withA(I.accent, I.hover == 1 ? 1.f : 0.88f));
    if (xc >= px0 && xc <= px1) dl->AddLine(ImVec2(xc, y0), ImVec2(xc, y1), IM_COL32(20, 21, 23, 230), 1.f);
    if (I.spectrum) {
        ImFont* f = I.mono ? I.mono : ImGui::GetFont();
        const float th = f->CalcTextSizeA(ImGui::GetFontSize() * 0.92f, FLT_MAX, 0, I.readout).y + 2 * s;
        tag(dl, I, xc, bot - th - 5 * s, px0 + 2, px1 - 2, true, IM_COL32(18, 19, 21, 170), 0);
    }
}

// under = true: the part that goes under the trace (the fills); false: the rest, over it. The waterfall calls it with both.
inline void draw(ImDrawList* dl, const In& I, int style, bool under) {
    if (I.xHi <= I.xLo || I.psize.x <= 0 || I.psize.y <= 0) return;
    dl->PushClipRect(I.pmin, ImVec2(I.pmin.x + I.psize.x, I.pmin.y + I.psize.y), true);
    if (style == 1) slab(dl, I, under);
    else if (style == 2) brackets(dl, I, under);
    else if (style == 3) ruler(dl, I, under);
    dl->PopClipRect();
}

}   // namespace tstyle
