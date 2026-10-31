#pragma once
#include "engine/ui/text_edit.hpp"
#include "game/commands/commands.hpp"
#include <deque>

namespace vc {
enum class NotificationSeverity { Info,Success,Warning,Error };
struct ConsoleMessage { std::string text;NotificationSeverity severity{};double time{}; };
class CommandConsole {
public:
    void begin(bool slash,bool expanded);
    bool process(const ui::TextInput& input,int scroll,const commands::Registry& registry,
        const commands::CompletionContext& context,double now);
    void post(const commands::Result& result,double now);
    void message(std::string text,NotificationSeverity severity,double now);
    const ui::TextEdit& editor() const { return edit_; }
    ui::TextEdit& editor() { return edit_; }
    const auto& output() const { return output_; }
    const auto& suggestions() const { return suggestions_; }
    std::size_t selectedSuggestion() const { return selectedSuggestion_; }
    std::size_t outputOffset() const { return outputOffset_; }
    bool expanded() const { return expanded_; }
    std::vector<std::string> takeCommands();
    std::string takeClipboard();
private:
    ui::TextEdit edit_;
    std::deque<std::string> history_;
    std::deque<ConsoleMessage> output_;
    std::vector<std::string> pending_;
    std::vector<commands::Completion> suggestions_;
    std::size_t historyIndex_{},selectedSuggestion_{},outputOffset_{};
    std::string draft_,clipboard_,completionPrefix_,completionSuffix_;
    bool expanded_{},cycling_{};
};
}
