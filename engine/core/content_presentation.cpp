#include "engine/core/content_presentation.hpp"
#include "engine/core/utf8.hpp"
#include <array>
#include <stdexcept>

namespace vc {
namespace { constexpr std::array<std::string_view,7> categories{"all","building","natural","tools","materials","food","miscellaneous"}; }
std::string_view categoryName(ItemCategory category) {
    const auto index=static_cast<std::size_t>(category);if(index>=categories.size()) throw std::invalid_argument("Unknown item category");return categories[index];
}
ItemCategory parseCategory(std::string_view category) {
    for(std::size_t i=0;i<categories.size();++i) if(categories[i]==category) return static_cast<ItemCategory>(i);
    throw std::invalid_argument("Unknown item category");
}
void validatePresentation(const ContentPresentation& presentation) {
    if(presentation.name.size()>128||presentation.description.size()>512||presentation.icon.size()>256||
        !utf8::valid(presentation.name)||!utf8::valid(presentation.description)) throw std::invalid_argument("Invalid content UI metadata");
    for(const auto* text:{&presentation.name,&presentation.description}) for(const char byte:*text)
        if(const auto c=static_cast<unsigned char>(byte);c<32||c==127) throw std::invalid_argument("Content UI metadata requires single-line printable text");
    if(!presentation.category.empty()) (void)parseCategory(presentation.category);
}
}
