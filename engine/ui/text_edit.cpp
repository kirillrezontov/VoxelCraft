#include "engine/ui/text_edit.hpp"
#include "engine/core/utf8.hpp"
#include <algorithm>
#include <stdexcept>

namespace vc::ui {
std::pair<std::size_t,std::size_t> TextEdit::selection() const { return std::minmax(cursor_,anchor_); }
std::string TextEdit::selected() const { const auto [a,b]=selection();return text_.substr(a,b-a); }
void TextEdit::set(std::string_view text) { text_=utf8::singleLine(text,maximum_);cursor_=anchor_=text_.size(); }
bool TextEdit::eraseSelection() {
    const auto [a,b]=selection();if(a==b) return false;
    text_.erase(a,b-a);cursor_=anchor_=a;return true;
}
bool TextEdit::insert(std::string_view text) {
    const auto clean=utf8::singleLine(text,maximum_);const auto [a,b]=selection();
    const auto available=maximum_-(text_.size()-(b-a));const auto accepted=utf8::truncate(clean,available);
    if(accepted.empty()) return false;
    eraseSelection();text_.insert(cursor_,accepted);cursor_+=accepted.size();anchor_=cursor_;return true;
}
void TextEdit::move(std::size_t byte,bool selecting) { cursor_=byte;if(!selecting) anchor_=cursor_; }
void TextEdit::select(std::size_t anchor,std::size_t cursor) {
    const auto boundary=[&](std::size_t b){return b<=text_.size()&&(b==text_.size()||(static_cast<unsigned char>(text_[b])&0xc0)!=0x80);};
    if(!boundary(anchor)||!boundary(cursor)) throw std::invalid_argument("Text selection must use UTF-8 boundaries");
    anchor_=anchor;cursor_=cursor;
}
bool TextEdit::process(const TextInput& input) {
    const auto before=text_;const auto old=std::pair(cursor_,anchor_);
    if(input.selectAll) { anchor_=0;cursor_=text_.size(); }
    if(input.cut) eraseSelection();
    if(input.left) { const auto [a,b]=selection();move(!input.shift&&a!=b?a:utf8::previous(text_,cursor_),input.shift); }
    if(input.right) { const auto [a,b]=selection();move(!input.shift&&a!=b?b:utf8::next(text_,cursor_),input.shift); }
    if(input.home) move(0,input.shift);
    if(input.end) move(text_.size(),input.shift);
    if(input.backspace&&!eraseSelection()&&cursor_) { const auto previous=utf8::previous(text_,cursor_);text_.erase(previous,cursor_-previous);cursor_=anchor_=previous; }
    if(input.erase&&!eraseSelection()&&cursor_<text_.size()) text_.erase(cursor_,utf8::next(text_,cursor_)-cursor_);
    if(!input.text.empty()) (void)insert(input.text);
    return before!=text_||old!=std::pair(cursor_,anchor_);
}
}
