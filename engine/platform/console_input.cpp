#include "engine/platform/console_input.hpp"
#include "engine/core/utf8.hpp"
#include <algorithm>
#include <array>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <poll.h>
#include <unistd.h>
#include <cerrno>
#endif

namespace vc {
void ConsoleLines::line() {
    if(!buffer_.empty()&&buffer_.back()=='\r') buffer_.pop_back();
    if(ready_.size()<32) ready_.push_back({std::move(buffer_),overflow_});
    buffer_.clear();overflow_=false;
}
void ConsoleLines::feed(std::string_view bytes) {
    for(const char c:bytes) {
        if(c=='\n') line();
        else if(buffer_.size()<maximum_) buffer_+=c;
        else overflow_=true;
    }
}
void ConsoleLines::finish() { if(!buffer_.empty()||overflow_) line(); }
std::vector<ConsoleLine> ConsoleLines::take(std::size_t budget) {
    std::vector<ConsoleLine> result;while(!ready_.empty()&&result.size()<std::min(budget,std::size_t{32})) { result.push_back(std::move(ready_.front()));ready_.pop_front(); }return result;
}
struct ConsoleInput::Impl {
    ConsoleLines lines;
    bool active{true};
#ifdef _WIN32
    HANDLE input{GetStdHandle(STD_INPUT_HANDLE)};
    std::wstring editing;
    UINT outputPage{GetConsoleOutputCP()};
    Impl() { if(outputPage) SetConsoleOutputCP(CP_UTF8); }
    ~Impl() { if(outputPage) SetConsoleOutputCP(outputPage); }
#endif
};
ConsoleInput::ConsoleInput():impl_(std::make_unique<Impl>()) {}
ConsoleInput::~ConsoleInput()=default;
std::vector<ConsoleLine> ConsoleInput::poll() {
    if(!impl_->active) return impl_->lines.take();
    std::array<char,4096> buffer{};
#ifdef _WIN32
    const auto input=impl_->input;
    if(input==nullptr||input==INVALID_HANDLE_VALUE) { impl_->active=false;return {}; }
    DWORD mode{};
    if(GetConsoleMode(input,&mode)) {
        // Console readiness is a key event, not necessarily a complete line.
        // ReadConsoleInput avoids blocking ReadFile on unfinished canonical input.
        for(unsigned n=0;n<64;++n) {
            DWORD count{};if(!GetNumberOfConsoleInputEvents(input,&count)||!count) break;
            INPUT_RECORD record{};DWORD read{};
            if(!ReadConsoleInputW(input,&record,1,&read)||!read) break;
            if(record.EventType!=KEY_EVENT||!record.Event.KeyEvent.bKeyDown) continue;
            const auto c=record.Event.KeyEvent.uChar.UnicodeChar;
            if(c==L'\r') {
                const auto& line=impl_->editing;
                const int length=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,line.data(),static_cast<int>(line.size()),nullptr,0,nullptr,nullptr);
                std::string text(length>0?static_cast<std::size_t>(length):0,'\0');
                if(length>0) WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,line.data(),static_cast<int>(line.size()),text.data(),length,nullptr,nullptr);
                impl_->lines.feed(text);impl_->lines.feed("\n");impl_->editing.clear();
                DWORD written{};WriteConsoleW(GetStdHandle(STD_OUTPUT_HANDLE),L"\r\n",2,&written,nullptr);
            } else if(c==L'\b') {
                if(!impl_->editing.empty()) {
                    impl_->editing.pop_back();if(!impl_->editing.empty()&&impl_->editing.back()>=0xd800&&impl_->editing.back()<=0xdbff) impl_->editing.pop_back();
                    DWORD written{};WriteConsoleW(GetStdHandle(STD_OUTPUT_HANDLE),L"\b \b",3,&written,nullptr);
                }
            } else if(c>=32&&impl_->editing.size()<2048) {
                impl_->editing+=c;DWORD written{};WriteConsoleW(GetStdHandle(STD_OUTPUT_HANDLE),&c,1,&written,nullptr);
            }
        }
    } else {
        DWORD available=static_cast<DWORD>(buffer.size());
        if(GetFileType(input)==FILE_TYPE_PIPE&&!PeekNamedPipe(input,nullptr,0,nullptr,&available,nullptr)) { impl_->active=false;impl_->lines.finish(); }
        if(impl_->active&&available) {
            DWORD read{};
            if(!ReadFile(input,buffer.data(),std::min(available,static_cast<DWORD>(buffer.size())),&read,nullptr)||!read) { impl_->active=false;impl_->lines.finish(); }
            else impl_->lines.feed(std::string_view(buffer.data(),read));
        }
    }
#else
    pollfd input{STDIN_FILENO,POLLIN,0};
    if(::poll(&input,1,0)>0) {
        if(input.revents&(POLLIN|POLLHUP)) {
            const auto count=::read(STDIN_FILENO,buffer.data(),buffer.size());
            if(count>0) impl_->lines.feed(std::string_view(buffer.data(),static_cast<std::size_t>(count)));
            else if(count==0||(errno!=EINTR&&errno!=EAGAIN)) { impl_->active=false;impl_->lines.finish(); }
        } else if(input.revents&(POLLERR|POLLNVAL)) impl_->active=false;
    }
#endif
    return impl_->lines.take();
}
}
