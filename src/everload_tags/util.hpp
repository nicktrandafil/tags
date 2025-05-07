#pragma once

#include <QRect>
#include <QString>

#include <everload_tags/config.hpp>
#include <ranges>
#include <unordered_map>
#include <vector>

namespace everload_tags {

inline void removeDuplicates(std::vector<Tag>& tags) {
    std::unordered_map<QString, size_t> unique;
    for (auto const i : std::views::iota(size_t{0}, tags.size())) {
        unique.emplace(tags[i].text, i);
    }

    for (auto b = tags.rbegin(), it = b, e = tags.rend(); it != e;) {
        if (auto const i = static_cast<size_t>(std::distance(it, e) - 1); unique.at(it->text) != i) {
            tags.erase(it++.base() - 1);
        } else {
            ++it;
        }
    }
}

} // namespace everload_tags
