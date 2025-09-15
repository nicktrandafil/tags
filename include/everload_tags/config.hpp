/*
 * MIT License
 *
 * Copyright (c) 2025 Nicolai Trandafil
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#pragma once

#include <QColor>
#include <QFontMetrics>
#include <QMargins>
#include <QPoint>
#include <QRect>
#include <QSize>

#include <optional>
#include <vector>

namespace everload_tags {

struct Tag {
    QString text;
    QRect rect;

    bool operator==(Tag const& rhs) const {
        return text == rhs.text && rect == rhs.rect;
    }
};

struct StyleConfig {
    /// Padding from the text to the the pill border
    QMargins pill_thickness = {7, 7, 8, 7};

    /// Space between pills
    int pills_h_spacing = 7;

    /// Space between rows of pills (for multi line tags)
    int tag_v_spacing = 2;

    /// Size of cross side
    qreal tag_cross_size = 8;

    /// Distance between text and the cross
    int tag_cross_spacing = 3;

    QColor color{255, 164, 100, 100};

    /// Rounding of the pill
    qreal rounding_x_radius = 5;

    /// Rounding of the pill
    qreal rounding_y_radius = 5;

    /// Calculate the width that a tag would have with the given text width
    int pillWidth(int text_width, bool has_cross) const {
        return text_width + pill_thickness.left() + (has_cross ? (tag_cross_spacing + tag_cross_size) : 0) +
               pill_thickness.right();
    }

    /// Calculate the height that a tag would have with the given text height
    int pillHeight(int text_height) const {
        return text_height + pill_thickness.top() + pill_thickness.bottom();
    }

    /// \param fit When nullopt arranges the tags in a line
    void calcRects(QPoint& lt, std::vector<Tag>& tags, QFontMetrics const& fm, std::optional<QRect> const& fit,
                   bool has_cross) const;

    void drawTags(QPainter& p, std::vector<Tag> const& tags, QFontMetrics const& fm, QPoint const& translate,
                  bool has_cross) const;
};

struct BehaviorConfig {
    /// Maintain only unique tags
    bool unique = true;
    bool restore_cursor_position_on_focus_click = false;
};

struct Config {
    StyleConfig style{};
    BehaviorConfig behavior{};
};

} // namespace everload_tags
