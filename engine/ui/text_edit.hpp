#pragma once
#include <string>
#include <string_view>
#include <utility>

namespace vc::ui {
struct TextInput {
    std::string text{};
    bool backspace{},erase{},left{},right{},home{},end{},selectAll{},copy{},cut{},paste{},enter{},escape{},tab{},up{},down{},shift{};
};
class TextEdit {
public:
    explicit TextEdit(std::size_t maximum=1024):maximum_(maximum) {}
    const std::string& text() const { return text_; }
    std::size_t cursor() const { return cursor_; }
    std::pair<std::size_t,std::size_t> selection() const;
    std::string selected() const;
    void set(std::string_view text);
    bool insert(std::string_view text);
    bool process(const TextInput& input);
    void select(std::size_t anchor,std::size_t cursor);
private:
    bool eraseSelection();
    void move(std::size_t byte,bool selecting);
    std::string text_;
    std::size_t cursor_{},anchor_{},maximum_;
};
}
