#include "everload_tags/config.hpp"

#include "common.hpp"

namespace everload_tags {

void StyleConfig::calcRects(QPoint& lt, std::vector<Tag>& tags, QFontMetrics const& fm, std::optional<QRect> const& fit,
                            bool has_cross) const {
    Style::calcRects(lt, tags, *this, fm, fit, has_cross);
}

void StyleConfig::drawTags(QPainter& p, std::vector<Tag> const& tags, QFontMetrics const& fm, QPoint const& offset,
                           bool has_cross) const {
    Style::drawTags(p, tags, *this, fm, offset, has_cross);
}

} // namespace everload_tags
