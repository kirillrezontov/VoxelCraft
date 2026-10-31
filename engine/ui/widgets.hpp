#pragma once
#include "engine/ui/font.hpp"
#include <optional>
#include <string>
#include <vector>

namespace vc::ui {
struct Theme {
    glm::vec4 panel{0.055f,0.070f,0.085f,0.94f},surface{0.10f,0.125f,0.15f,0.96f},hover{0.16f,0.20f,0.23f,1};
    glm::vec4 border{0.25f,0.30f,0.34f,0.8f},accent{0.26f,0.76f,0.65f,1},text{0.93f,0.95f,0.96f,1},muted{0.57f,0.65f,0.69f,1};
    glm::vec4 error{0.95f,0.36f,0.34f,1},warning{0.94f,0.70f,0.31f,1},success{0.35f,0.82f,0.54f,1};
    float spacing{8},borderWidth{1},slotSize{48},fontSize{18},titleSize{26},transitionSeconds{0.16f};
};
enum class WidgetKind { Panel,Button,Label,Image,Slot,Scroll,TextInput };
struct Widget {
    std::string id;
    WidgetKind kind{};
    Rect bounds;
    bool enabled{true},focusable{};
    std::string tooltip{};
};
class WidgetTree {
public:
    void clear();
    void add(Widget widget);
    const Widget* hit(float x,float y) const;
    bool hover(float x,float y);
    void focus(std::string_view id);
    bool navigate(int steps);
    const std::string& hovered() const { return hovered_; }
    const std::string& focused() const { return focused_; }
    const std::vector<Widget>& widgets() const { return widgets_; }
private:
    std::vector<Widget> widgets_;
    std::string hovered_,focused_;
};
struct ScrollState {
    std::size_t offset{},visible{1},count{};
    bool scroll(int steps);
};
}
