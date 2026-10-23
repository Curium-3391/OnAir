// View > Performance overlay (perf_overlay.cpp)
#pragma once
struct App;

// every frame: the time the interface took to build, the redraw pace chosen; `wanted`: the overlay is on (else nothing is measured)
void perfFrame(double now, double buildSec, double interval, bool battery, bool occluded, bool wanted);
void perfOverlayDraw(App& a);   // inside the ImGui frame, after the main window
