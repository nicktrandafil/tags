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

#include <QCompleter>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QPainter>
#include <QPainterPath>
#include <QRect>
#include <QString>
#include <QStyleHints>
#include <QStyleOptionFrame>
#include <QTextLayout>

#include <chrono>

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

    static QRectF crossRect(QRectF const& r, qreal cross_size) {
        QRectF cross(QPointF{0, 0}, QSizeF{cross_size, cross_size});
        cross.moveCenter(QPointF(r.right() - cross_size, r.center().y()));
        return cross;
    }

    QRectF crossRect(QRectF const& r) const {
        return crossRect(r, tag_cross_size);
    }

    template <class It>
    static void drawTags(QPainter& p, std::pair<It, It> range, QFontMetrics const& fm, QMargins const& pill_thickness,
                         qreal cross_size, QPoint const& offset) {
        for (auto it = range.first; it != range.second; ++it) {
            QRect const& i_r = it->rect.translated(offset);
            auto const text_pos =
                i_r.topLeft() + QPointF(pill_thickness.left(), fm.ascent() + ((i_r.height() - fm.height()) / 2));

            // drag tag rect
            QColor const blue(0, 96, 100, 150);
            QPainterPath path;
            path.addRoundedRect(i_r, 4, 4);
            p.fillPath(path, blue);

            // draw text
            p.drawText(text_pos, it->text);

            // calc cross rect
            auto const i_cross_r = Style::crossRect(i_r, cross_size);

            QPen pen = p.pen();
            pen.setWidth(2);

            p.save();
            p.setPen(pen);
            p.setRenderHint(QPainter::Antialiasing);
            p.drawLine(QLineF(i_cross_r.topLeft(), i_cross_r.bottomRight()));
            p.drawLine(QLineF(i_cross_r.bottomLeft(), i_cross_r.topRight()));
            p.restore();
        }
    }

    int pillWidth(int text_width) const {
        return text_width + pill_thickness.left() + tag_cross_spacing + tag_cross_size + pill_thickness.right();
    }

    int pillHeight(int text_height) const {
        return text_height + pill_thickness.top() + pill_thickness.bottom();
    }
};

struct Behavior {
    bool unique; ///<! Turn on/off Invariant-2
};

// Invariant-1 no empty tags apart from currently being edited.
// Invariant-2 tags are unique.
// Default-state is one empty tag which is editing.
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
    std::chrono::steady_clock::time_point focused_at{};

    QRect const& editorRect() const {
        return tags[editing_index].rect;
    }

    void setEditorRect(QRect rect) {
        tags[editing_index].rect = rect;
    }

    QString const& editorText() const {
        return tags[editing_index].text;
    }

    QString& editorText() {
        return tags[editing_index].text;
    }

    bool cursorVisible() const {
        return blink_timer;
    }

    void updateCursorBlinking(QObject* ifce) {
        setCursorVisible(cursorVisible(), ifce);
    }

    void updateDisplayText() {
        text_layout.clearLayout();
        text_layout.setText(editorText());
        text_layout.beginLayout();
        text_layout.createLine();
        text_layout.endLayout();
    }

    void setCursorVisible(bool visible, QObject* ifce) {
        if (blink_timer) {
            ifce->killTimer(blink_timer);
            blink_timer = 0;
        }

        if (visible) {
            blink_status = true;
            int flashTime = QGuiApplication::styleHints()->cursorFlashTime();
            if (flashTime >= 2) {
                blink_timer = ifce->startTimer(flashTime / 2);
            }
        } else {
            blink_status = false;
        }
    }

    QVector<QTextLayout::FormatRange> formatting(QPalette const& palette) const {
        if (select_size == 0) {
            return {};
        }

        QTextLayout::FormatRange selection;
        selection.start = select_start;
        selection.length = select_size;
        selection.format.setBackground(palette.brush(QPalette::Highlight));
        selection.format.setForeground(palette.brush(QPalette::HighlightedText));
        return {selection};
    }

    qreal cursorToX() {
        return text_layout.lineAt(0).cursorToX(cursor);
    }

    void moveCursor(int pos, bool mark) {
        if (mark) {
            auto e = select_start + select_size;
            int anchor = select_size > 0 && cursor == select_start ? e
                         : select_size > 0 && cursor == e          ? select_start
                                                                   : cursor;
            select_start = qMin(anchor, pos);
            select_size = qMax(anchor, pos) - select_start;
        } else {
            deselectAll();
        }
        cursor = pos;
    }

    void deselectAll() {
        select_start = 0;
        select_size = 0;
    }

    bool hasSelection() const noexcept {
        return select_size > 0;
    }

    void selectAll() {
        select_start = 0;
        select_size = editorText().size();
    }

    void removeSelection() {
        assert(cursor + select_size <= editorText().size());
        cursor = select_start;
        editorText().remove(cursor, select_size);
        deselectAll();
    }

    void removeBackwardOne() {
        if (hasSelection()) {
            removeSelection();
        } else {
            editorText().remove(--cursor, 1);
        }
    }
};

struct Common : Style, Behavior, State {
    void drawEditor(QPainter& p, QPalette const& palette, QPoint const& offset) const {
        auto const& r = editorRect();
        auto const& txt_p = r.topLeft() + QPointF(pill_thickness.left(), pill_thickness.top());
        auto const f = formatting(palette);
        text_layout.draw(&p, txt_p - offset, f);
        if (blink_status) {
            text_layout.drawCursor(&p, txt_p - offset, cursor);
        }
    }

    bool inCrossArea(size_t tag_index, QPoint const& point, QPoint const& offset) const {
        return crossRect(tags[tag_index].rect).adjusted(-1, -1, 1, 1).translated(-offset).contains(point) &&
               (!cursorVisible() || tag_index != editing_index);
    }

    bool isCurrentTagADuplicate() const {
        assert(editing_index < tags.size());
        auto const mid = tags.begin() + static_cast<std::ptrdiff_t>(editing_index);
        auto const text_eq = [this](auto const& x) { return x.text == editorText(); };
        return std::find_if(tags.begin(), mid, text_eq) != mid ||
               std::find_if(mid + 1, tags.end(), text_eq) != tags.end();
    }

    /// Makes the tag at `i` currently editing, and ensures Invariant-1 and Invariant-2`.
    void setEditorIndex(size_t i) {
        assert(i < tags.size());
        if (editorText().isEmpty() || (unique && isCurrentTagADuplicate())) {
            tags.erase(std::next(begin(tags), static_cast<std::ptrdiff_t>(editing_index)));
            if (editing_index <= i) { // Did we shift `i`?
                --i;
            }
        }
        editing_index = i;
    }

    // Inserts a new tag at `i`, makes the tag currently editing, and ensures Invariant-1.
    void editNewTag(size_t i) {
        assert(i <= tags.size());
        tags.insert(begin(tags) + static_cast<std::ptrdiff_t>(i), Tag{});
        if (i <= editing_index) { // Did we shift `editing_index`?
            ++editing_index;
        }
        setEditorIndex(i);
        moveCursor(0, false);
    }

    void editPreviousTag() {
        if (editing_index > 0) {
            setEditorIndex(editing_index - 1);
            moveCursor(editorText().size(), false);
        }
    }

    void editNextTag() {
        if (editing_index < tags.size() - 1) {
            setEditorIndex(editing_index + 1);
            moveCursor(0, false);
        }
    }

    void editTag(size_t i) {
        assert(i < tags.size());
        setEditorIndex(i);
        moveCursor(editorText().size(), false);
    }

    void removeTag(size_t i) {
        tags.erase(tags.begin() + static_cast<ptrdiff_t>(i));
        if (i <= editing_index) {
            --editing_index;
        }
    }
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

inline auto elapsed(std::chrono::steady_clock::time_point const& ts) {
    return std::chrono::steady_clock::now() - ts;
}

} // namespace everload_tags
