#include "engine/ui/widgets.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace vc::ui {
void WidgetTree::clear() { widgets_.clear();hovered_.clear(); }
void WidgetTree::add(Widget widget) {
    if(widgets_.size()>=256||widget.id.empty()||widget.bounds.width<0||widget.bounds.height<0||
        !std::isfinite(widget.bounds.x)||!std::isfinite(widget.bounds.y)) throw std::invalid_argument("Invalid or excessive UI widget");
    if(std::ranges::any_of(widgets_,[&](const auto& existing){return existing.id==widget.id;})) throw std::invalid_argument("Duplicate UI widget ID");
    widgets_.push_back(std::move(widget));
}
const Widget* WidgetTree::hit(float x,float y) const {
    for(auto it=widgets_.rbegin();it!=widgets_.rend();++it) if(it->enabled&&it->bounds.contains(x,y)&&it->kind!=WidgetKind::Label&&it->kind!=WidgetKind::Image) return &*it;
    return nullptr;
}
bool WidgetTree::hover(float x,float y) {
    const auto* widget=hit(x,y);const auto next=widget?widget->id:std::string{};
    if(next==hovered_) return false;
    hovered_=next;return true;
}
void WidgetTree::focus(std::string_view id) {
    const auto found=std::ranges::find_if(widgets_,[&](const auto& widget){return widget.id==id&&widget.enabled&&widget.focusable;});
    if(found!=widgets_.end()) focused_=found->id;
}
bool WidgetTree::navigate(int steps) {
    std::vector<std::string> choices;
    for(const auto& widget:widgets_) if(widget.enabled&&widget.focusable) choices.push_back(widget.id);
    if(choices.empty()||!steps) return false;
    const auto found=std::ranges::find(choices,focused_);auto index=found==choices.end()?-1:static_cast<int>(found-choices.begin());
    const auto count=static_cast<int>(choices.size());index=(index+steps%count+count)%count;
    const bool changed=focused_!=choices[static_cast<std::size_t>(index)];focused_=choices[static_cast<std::size_t>(index)];return changed;
}
bool ScrollState::scroll(int steps) {
    const auto maximum=count>visible?count-visible:0;
    const auto next=std::clamp(static_cast<std::int64_t>(offset)+steps,std::int64_t{0},static_cast<std::int64_t>(maximum));
    if(offset==static_cast<std::size_t>(next)) return false;
    offset=static_cast<std::size_t>(next);return true;
}
}
