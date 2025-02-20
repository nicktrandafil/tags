/*
 * MIT License
 *
 * Copyright (c) 2021 Nicolai Trandafil
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

#include <QKeyEvent>
#include <QRect>
#include <QString>
#include <QTextLayout>
#include <QCompleter>
#include <QStyleOptionFrame>

#if QT_VERSION < QT_VERSION_CHECK(5, 11, 0)
#define FONT_METRICS_WIDTH(fmt, ...) fmt.width(__VA_ARGS__)
#else
#define FONT_METRICS_WIDTH(fmt, ...) fmt.horizontalAdvance(__VA_ARGS__)
#endif

namespace everload_tags {

struct Tag {
    QString text;
    QRect rect;
};

struct Style {
    /// Width of painting from text to the the pill border
    QMargins pill_thickness = {3, 3, 4, 3};
    /// Space between pills
    int pills_h_spacing = 3;
    /// Size of cross side
    qreal tag_cross_size = 4;
    /// Distance between text and cross
    int tag_cross_spacing = 2;

    QRectF crossRect(QRectF const& r) const {
        QRectF cross(QPointF{0, 0}, QSizeF{tag_cross_size, tag_cross_size});
        cross.moveCenter(QPointF(r.right() - tag_cross_size, r.center().y()));
        return cross;
    }

    int pillWidth(int text_width) const {
        return text_width + pill_thickness.left() + tag_cross_spacing + tag_cross_size + pill_thickness.right();
    }

    int pillHeight(int text_height) const {
        return text_height + pill_thickness.top() + pill_thickness.bottom();
    }
};

struct Behavior {
    /// Turn on/off Invariant-2
    bool unique;
};

struct State {
    std::vector<Tag> tags{Tag{}};
    size_t editing_index{0};
    int blink_timer{0};
    bool blink_status{true};
    int cursor{0};
    int select_start{0};
    int select_size{0};
    QTextLayout text_layout;
    std::unique_ptr<QCompleter> completer{new QCompleter{}};
};

// Without this margin the frame is not highlighted if the item is focused
inline constexpr QMargins magic_margins = {1, 1, 1, 1};

// ?
inline constexpr QMargins magic_margins2 = {1, 1, 1, 1};

/// \ref `bool QInputControl::isAcceptableInput(QKeyEvent const* event) const`
inline bool isAcceptableInput(QKeyEvent const& event) {
    auto const text = event.text();
    if (text.isEmpty()) {
        return false;
    }

    auto const c = text.at(0);

    if (c.category() == QChar::Other_Format) {
        return true;
    }

    if (event.modifiers() == Qt::ControlModifier || event.modifiers() == (Qt::ShiftModifier | Qt::ControlModifier)) {
        return false;
    }

    if (c.isPrint()) {
        return true;
    }

    if (c.category() == QChar::Other_PrivateUse) {
        return true;
    }

    return false;
}

inline void initStyleOption(QStyleOptionFrame* option, QWidget const* widget) {
    assert(option);
    option->initFrom(widget);
    option->rect = widget->contentsRect();
    option->lineWidth = widget->style()->pixelMetric(QStyle::PM_DefaultFrameWidth, option, widget);
    option->midLineWidth = 0;
    option->state |= QStyle::State_Sunken;
    option->features = QStyleOptionFrame::None;
}

} // namespace everload_tags
