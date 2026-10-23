// The docking layout tree of the new interface (issue #242), pure: no ImGui. ui2_dock.cpp draws it.
// The vendored Dear ImGui is the master branch (no docking), so the app docks its own windows: the tree says where each docked
// window goes inside the dock space; the window keeps drawing itself with ImGui::Begin and is placed into its rectangle.
// A node is either a leaf (one or more windows as tabs, one of them shown) or a split into two children, side by side or one
// above the other. A window is named by its ImGui window name ("Map", "Memory###Memory", ...).
#pragma once
#include <functional>
#include <string>
#include <vector>

namespace dock {

enum class Zone { None, Left, Right, Top, Bottom, Centre };

struct Rect {
    float x = 0, y = 0, w = 0, h = 0;
    bool contains(float px, float py) const { return px >= x && py >= y && px < x + w && py < y + h; }
};

struct Node {
    enum Kind { Leaf, SplitH, SplitV } kind = Leaf;   // SplitH: kids[0] left, kids[1] right; SplitV: kids[0] above kids[1]
    float ratio = 0.5f;                               // the first child's share of the split (0.05 .. 0.95)
    std::vector<Node> kids;                           // two for a split, none for a leaf
    std::vector<std::string> tabs;                    // a leaf's windows, in tab order
    int active = 0;                                   // the tab shown
    bool empty() const { return kind == Leaf && tabs.empty(); }
};

bool contains(const Node& root, const std::string& name);
std::vector<std::string> panels(const Node& root);                       // every docked window, depth first
const Node* leafOf(const Node& root, const std::string& name);          // the leaf that holds the window, or null
bool remove(Node& root, const std::string& name);                       // undock; an emptied leaf goes and its sibling takes the space
// Dock `name` next to the leaf holding `target` (Centre: as a tab of it). An empty target or a target that is not docked docks
// against the whole tree (an edge of the dock space). `share` is the new window's part of the split. Docking a window
// already in the tree moves it. Docking a window onto itself does nothing. Returns false when nothing changed.
bool dockAt(Node& root, const std::string& target, Zone z, const std::string& name, float share = 0.5f);
bool setActive(Node& root, const std::string& name);                    // shows that tab of its leaf
std::string cycleTab(const Node& root, const std::string& name, int dir, const std::function<bool(const std::string&)>& alive = {});   // the next / previous tab in the leaf (wraps)

// Layout of the tree in a rectangle. Only windows for which `alive` returns true take space (a closed window keeps its place
// in the tree for when it opens again; a subtree with nothing alive gives its whole space to its sibling).
struct LeafBox {
    Rect r;
    std::vector<std::string> tabs;   // the alive tabs
    int active = 0;                  // index into tabs
};
struct SplitBar {
    Rect r;                  // the bar between the two children (the gap)
    bool vertical = false;   // true: a vertical bar between left and right children (dragged sideways)
    float origin = 0, len = 0;   // the split node's extent along the drag axis
    std::vector<int> path;   // child indices from the root to the split node
};
void layout(const Node& root, Rect r, float gap, const std::function<bool(const std::string&)>& alive,
            std::vector<LeafBox>& leaves, std::vector<SplitBar>& bars);
// A splitter dragged to pixel `pos`: the new ratio for a split of extent [origin, origin + len], keeping both sides at
// least minPx (or half each when there is not room for that).
float ratioAt(float pos, float origin, float len, float minPx);
bool setRatio(Node& root, const std::vector<int>& path, float ratio);

// Drop targets. Inside a leaf: five squares of side `size` in a cross at its centre. On the dock space: four squares at the
// middle of each edge (`outer`). hitTest gives the zone under the point, previewRect where the window would go.
Rect zoneRect(Rect r, Zone z, float size, bool outer);
Zone hitTest(Rect r, float px, float py, float size, bool outer);
Rect previewRect(Rect r, Zone z, float share);

// Text form for the settings file: a leaf is T(active|name|name), a split H(ratio,first,second) or V(...), the ratio in
// ten-thousandths (no locale-dependent decimals). '|', '(', ')', ',' and '\' in names are escaped with '\'. "" is an empty tree.
std::string serialise(const Node& root);
bool parse(const std::string& s, Node& out);   // false (and out empty) on malformed text

// Named layouts: one line each, "modeKey<TAB>name<TAB>tree".
struct Named { std::string mode, name, tree; };
std::vector<Named> parseNamed(const std::string& text);
std::string writeNamed(const std::vector<Named>& v);
void putNamed(std::vector<Named>& v, const std::string& mode, const std::string& name, const std::string& tree);   // add or replace
bool dropNamed(std::vector<Named>& v, const std::string& mode, const std::string& name);

}  // namespace dock
