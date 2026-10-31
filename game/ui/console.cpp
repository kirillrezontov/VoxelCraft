#include "game/ui/console.hpp"
#include "engine/core/utf8.hpp"
#include <algorithm>
#include <utility>

namespace vc {
void CommandConsole::begin(bool slash,bool expanded) {
    expanded_=expanded;edit_.set(slash?"/":"");historyIndex_=history_.size();draft_.clear();cycling_=false;suggestions_.clear();outputOffset_=0;
}
void CommandConsole::message(std::string text,NotificationSeverity severity,double now) {
    output_.push_back({utf8::singleLine(text,commands::maxResultBytes),severity,now});while(output_.size()>128) output_.pop_front();outputOffset_=0;
}
void CommandConsole::post(const commands::Result& result,double now) {
    for(const auto& line:result.lines) message(line,result.success()?NotificationSeverity::Success:NotificationSeverity::Error,now);
}
bool CommandConsole::process(const ui::TextInput& input,int scroll,const commands::Registry& registry,
        const commands::CompletionContext& context,double now) {
    bool dirty=false;
    if(input.copy||input.cut) clipboard_=edit_.selected();
    if(scroll) {
        const auto next=std::clamp(static_cast<std::int64_t>(outputOffset_)+scroll,std::int64_t{0},static_cast<std::int64_t>(output_.size()));
        dirty=outputOffset_!=static_cast<std::size_t>(next);outputOffset_=static_cast<std::size_t>(next);
    }
    if(input.up||input.down) {
        if(historyIndex_==history_.size()) draft_=edit_.text();
        if(input.up&&historyIndex_>0) --historyIndex_;
        if(input.down&&historyIndex_<history_.size()) ++historyIndex_;
        edit_.set(historyIndex_==history_.size()?draft_:history_[historyIndex_]);cycling_=false;dirty=true;
    }
    if(input.tab) {
        if(!cycling_) {
            completionPrefix_=edit_.text().substr(0,edit_.cursor());completionSuffix_=edit_.text().substr(edit_.cursor());
            suggestions_=registry.complete(completionPrefix_,context);selectedSuggestion_=0;cycling_=true;
        } else if(!suggestions_.empty()) selectedSuggestion_=(selectedSuggestion_+1)%suggestions_.size();
        if(!suggestions_.empty()) { edit_.set(suggestions_[selectedSuggestion_].replacement+completionSuffix_);dirty=true; }
    } else {
        try { if(edit_.process(input)) { cycling_=false;dirty=true; } }
        catch(const std::invalid_argument&) { message("Invalid UTF-8 input was ignored",NotificationSeverity::Error,now);dirty=true; }
        if(dirty||suggestions_.empty()) suggestions_=registry.complete(edit_.text().substr(0,edit_.cursor()),context);
    }
    if(input.enter&&!edit_.text().empty()) {
        const auto text=edit_.text();
        if(history_.empty()||history_.back()!=text) history_.push_back(text);
        while(history_.size()>64) history_.pop_front();
        historyIndex_=history_.size();
        if(pending_.size()<8) {
            if(text.starts_with('/')) pending_.push_back(text);
            else {
                std::string quoted="/say \"";for(const auto c:text) { if(c=='"'||c=='\\') quoted+='\\';quoted+=c; }quoted+='"';
                if(quoted.size()<=commands::maxCommandBytes) pending_.push_back(std::move(quoted));
                else message("Message is too long",NotificationSeverity::Error,now);
            }
            message("> "+text,NotificationSeverity::Info,now);
        } else message("Command queue is full",NotificationSeverity::Warning,now);
        edit_.set("");cycling_=false;suggestions_.clear();dirty=true;
    }
    return dirty;
}
std::vector<std::string> CommandConsole::takeCommands() { return std::exchange(pending_,{}); }
std::string CommandConsole::takeClipboard() { return std::exchange(clipboard_,{}); }
}
