/*
  MIT License

  Copyright (c) 2021 Nicolai Trandafil

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
*/

#pragma once

#include <QCompleter>
#include <QGuiApplication>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QRectF>
#include <QStyleHints>
#include <QTextLayout>

#include <iterator>

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

// Invariant-1 ensures no empty tags apart from currently being edited.
// Invariant-2 ensures tags uniqueness.
// Default-state is one empty tag which is currently editing.
template <class Derived>
struct TagsImpl {
    explicit TagsImpl(bool unique)
            : unique(unique) {
    }

    Derived const& self() const noexcept {
        return static_cast<Derived const&>(*this);
    }

    Derived& self() noexcept {
        return static_cast<Derived&>(*this);
    }

    QRect crossRect(QRectF const& r) const {
        QRect cross(QPoint{0, 0}, QSize{tag_cross_width, tag_cross_width});
        cross.moveCenter(QPoint(r.right() - tag_cross_width, r.center().y()));
        return cross;
    }

    bool cursorVisible() const {
        return blink_timer;
    }

    bool inCrossArea(size_t tag_index, QPoint point) const {
        return crossRect(tags[tag_index].rect)
                       .adjusted(-1, -1, 1, 1)
                       .translated(-self().offset())
                       .contains(point)
            && (!cursorVisible() || tag_index != editing_index);
    }

    void drawTags(QPainter& p) const {
        for (auto i = 0u; i < tags.size(); ++i) {
            auto const& tag = tags[i];
            if (i == editing_index && cursorVisible()) {
                auto const txt_p =
                        currentRect().topLeft()
                        + QPoint(pill_thickness.left(), pill_thickness.top());
                auto const f = formatting();
                text_layout.draw(&p, txt_p - self().offset(), f);
                if (blink_status) {
                    text_layout.drawCursor(&p, txt_p - self().offset(), cursor);
                }
            } else if (i != editing_index || !tag.text.isEmpty()) {
                QRect const& i_r = tag.rect.translated(-self().offset());
                auto const text_pos =
                        i_r.topLeft()
                        + QPointF(pill_thickness.left(),
                                  self().ifce->fontMetrics().ascent()
                                          + ((i_r.height()
                                              - self().ifce->fontMetrics()
                                                        .height())
                                             / 2));

                // draw tag rect
                QColor const blue(0, 96, 100, 150);
                QPainterPath path;
                path.addRoundedRect(i_r, 4, 4);
                p.fillPath(path, blue);

                // draw text
                p.drawText(text_pos, tag.text);

                // calc cross rect
                auto const i_cross_r = crossRect(i_r);

                QPen pen = p.pen();
                pen.setWidth(2);

                p.save();
                p.setPen(pen);
                p.setRenderHint(QPainter::Antialiasing);
                p.drawLine(
                        QLineF(i_cross_r.topLeft(), i_cross_r.bottomRight()));
                p.drawLine(
                        QLineF(i_cross_r.bottomLeft(), i_cross_r.topRight()));
                p.restore();
            }
        }
    }

    void drawContent(QPainter& p) {
        drawTags(p);
    }

    int pillHeight(int text_height) const {
        return text_height + pill_thickness.top() + pill_thickness.bottom();
    }

    int pillWidth(int text_width) const {
        return pill_thickness.left() + text_width + tag_cross_spacing
             + tag_cross_width + pill_thickness.right();
    }

    void setCursorVisible(bool visible) {
        if (blink_timer) {
            self().ifce->killTimer(blink_timer);
            blink_timer = 0;
            blink_status = true;
        }

        if (visible) {
            int flashTime = QGuiApplication::styleHints()->cursorFlashTime();
            if (flashTime >= 2) {
                blink_timer = self().ifce->startTimer(flashTime / 2);
            }
        } else {
            blink_status = false;
        }
    }

    void updateCursorBlinking() {
        setCursorVisible(cursorVisible());
    }

    void updateDisplayText() {
        text_layout.clearLayout();
        text_layout.setText(currentText());
        text_layout.beginLayout();
        text_layout.createLine();
        text_layout.endLayout();
    }

    bool isCurrentTagADuplicate() const {
        assert(editing_index < tags.size());
        auto const mid =
                tags.begin() + static_cast<std::ptrdiff_t>(editing_index);
        auto const text_eq = [this](auto const& x) {
            return x.text == currentText();
        };
        return std::find_if(tags.begin(), mid, text_eq) != mid
            || std::find_if(mid + 1, tags.end(), text_eq) != tags.end();
    }

    /// Makes the tag at `i` currently editing, and ensures Invariant-1`.
    void setEditingIndex(size_t i) {
        assert(i < tags.size());
        if (currentText().isEmpty() || (unique && isCurrentTagADuplicate())) {
            tags.erase(std::next(begin(tags),
                                 static_cast<std::ptrdiff_t>(editing_index)));
            if (editing_index <= i) { // Did we shift `i`?
                --i;
            }
        }
        editing_index = i;
    }

    QString const& currentText() const {
        return tags[editing_index].text;
    }

    QString& currentText() {
        return tags[editing_index].text;
    }

    QRect const& currentRect() const {
        return tags[editing_index].rect;
    }

    qreal cursorToX() {
        return text_layout.lineAt(0).cursorToX(cursor);
    }

    void currentText(QString const& text) {
        currentText() = text;
        moveCursor(currentText().length(), false);
        updateDisplayText();
        self().onCurrentTextUpdate();
    }

    // Inserts a new tag at `i`, makes the tag currently editing,
    // and ensures Invariant-1.
    void editNewTag(size_t i) {
        assert(i <= tags.size());
        tags.insert(begin(tags) + static_cast<std::ptrdiff_t>(i), Tag{});
        if (i <= editing_index) { // Did we shift `editing_index`?
            ++editing_index;
        }
        setEditingIndex(i);
        moveCursor(0, false);
    }

    void editPreviousTag() {
        if (editing_index > 0) {
            setEditingIndex(editing_index - 1);
            moveCursor(currentText().size(), false);
        }
    }

    void editNextTag() {
        if (editing_index < tags.size() - 1) {
            setEditingIndex(editing_index + 1);
            moveCursor(0, false);
        }
    }

    void editTag(size_t i) {
        setEditingIndex(i);
        moveCursor(currentText().size(), false);
    }

    void setupCompleter() {
        completer->setWidget(self().ifce);
        QObject::connect(
                completer.get(),
                qOverload<QString const&>(&QCompleter::activated),
                [this](QString const& text) { self().currentText(text); });
    }

    QVector<QTextLayout::FormatRange> formatting() const {
        if (select_size == 0) {
            return {};
        }

        QTextLayout::FormatRange selection;
        selection.start = select_start;
        selection.length = select_size;
        selection.format.setBackground(
                self().ifce->palette().brush(QPalette::Highlight));
        selection.format.setForeground(
                self().ifce->palette().brush(QPalette::HighlightedText));
        return {selection};
    }

    bool hasSelection() const noexcept {
        return select_size > 0;
    }

    void removeSelection() {
        assert(cursor + select_size <= currentText().size());
        cursor = select_start;
        currentText().remove(cursor, select_size);
        deselectAll();
    }

    void removeBackwardOne() {
        if (hasSelection()) {
            removeSelection();
        } else {
            currentText().remove(--self().cursor, 1);
        }
    }

    void selectAll() {
        self().select_start = 0;
        self().select_size = currentText().size();
    }

    void deselectAll() {
        self().select_start = 0;
        self().select_size = 0;
    }

    // Copy of `bool QInputControl::isAcceptableInput(QKeyEvent const* event)
    // const`
    bool isAcceptableInput(QKeyEvent const* event) const {
        const QString text = event->text();
        if (text.isEmpty())
            return false;

        const QChar c = text.at(0);

        // Formatting characters such as ZWNJ, ZWJ, RLM, etc. This needs to go
        // before the next test, since CTRL+SHIFT is sometimes used to input it
        // on Windows.
        if (c.category() == QChar::Other_Format)
            return true;

        // QTBUG-35734: ignore Ctrl/Ctrl+Shift; accept only AltGr (Alt+Ctrl) on
        // German keyboards
        if (event->modifiers() == Qt::ControlModifier
            || event->modifiers()
                       == (Qt::ShiftModifier | Qt::ControlModifier)) {
            return false;
        }

        if (c.isPrint())
            return true;

        if (c.category() == QChar::Other_PrivateUse)
            return true;

        return false;
    }

    void moveCursor(int pos, bool mark) {
        if (mark) {
            auto e = select_start + select_size;
            int anchor = select_size > 0 && cursor == select_start ? e
                       : select_size > 0 && cursor == e ? select_start
                                                        : cursor;
            select_start = qMin(anchor, pos);
            select_size = qMax(anchor, pos) - select_start;
        } else {
            deselectAll();
        }

        cursor = pos;
    }

    void setTags(std::vector<QString> const& tags) {
        std::vector<Tag> t;
        t.reserve(tags.size());
        for (auto const& tag : tags) {
            if (!tag.isEmpty() // Ensure Invariant-1
                && (!unique    // Ensure Invariant-2
                    || std::find_if(t.begin(), t.end(), [tag](auto const& x) {
                           return x.text == tag;
                       }) == t.end())) {
                t.push_back(Tag{tag, QRect()});
            }
        }
        if (t.empty()) { // Ensure Invariant-1
            t.push_back(Tag{});
        }

        // Set to Default-state.
        editing_index = 0;
        cursor = 0;
        select_size = 0;
        select_start = 0;
        this->tags = std::move(t);
    }

    /// Width of painting from text to the the pill border.
    QMargins const pill_thickness{3, 3, 4, 3};

    /// Space between pills
    int const pills_h_spacing{3};

    int const tag_cross_width{4};
    int const tag_cross_spacing{2};

    std::vector<Tag> tags{Tag{}};

    int blink_timer{0};
    bool blink_status{true};

    size_t editing_index{0};
    QTextLayout text_layout;
    int cursor{0};
    int select_start{0};
    int select_size{0};

    std::unique_ptr<QCompleter> completer{std::make_unique<QCompleter>()};

    bool const unique; // Turn on/off Invariant-2
};

} // namespace everload_tags
