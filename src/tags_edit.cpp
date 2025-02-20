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

#include "everload_tags/tags_edit.hpp"

#include "common.hpp"

#include <QApplication>
#include <QDebug>
#include <QPainter>
#include <QPainterPath>
#include <QScrollBar>
#include <QStyle>
#include <QStyleHints>
#include <QStyleOptionFrame>
#include <QTextLayout>
#include <QKeyEvent>

#include <cassert>

namespace everload_tags {
namespace {

constexpr int tag_v_spacing = 2;

} // namespace

// Invariant-1 ensures no empty tags apart from currently being edited.
// Invariant-2 ensures tags uniqueness.
// Default-state is one empty tag which is currently editing.
struct TagsEdit::Impl : Style, Behavior, State {
    explicit Impl(TagsEdit* ifce, bool unique_)
            : Behavior{
                  .unique = unique_
              }
            , ifce{ifce} {
    }

    bool inCrossArea(size_t tag_index, QPoint point) const {
        return crossRect(tags[tag_index].rect)
        .adjusted(-1, -1, 1, 1)
        .translated(-offset())
        .contains(point) &&
        (!cursorVisible() || tag_index != editing_index);
    }

    void drawTags(QPainter& p) const {
        for (auto i = 0u; i < tags.size(); ++i) {
            auto const& tag = tags[i];
            if (i == editing_index && cursorVisible()) {
                auto const txt_p =
                currentRect().topLeft() + QPoint(pill_thickness.left(), pill_thickness.top());
                auto const f = formatting();
                text_layout.draw(&p, txt_p - offset(), f);
                if (blink_status) {
                    text_layout.drawCursor(&p, txt_p - offset(), cursor);
                }
            } else if (i != editing_index || !tag.text.isEmpty()) {
                QRect const& i_r = tag.rect.translated(-offset());
                auto const text_pos =
                i_r.topLeft() + QPointF(pill_thickness.left(),
                                        ifce->fontMetrics().ascent() + ((i_r.height() - ifce->fontMetrics().height()) / 2));

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
        return pill_thickness.left() + text_width + tag_cross_spacing + tag_cross_size + pill_thickness.right();
    }

    bool cursorVisible() const {
        return blink_timer;
    }

    void setCursorVisible(bool visible) {
        if (blink_timer) {
            ifce->killTimer(blink_timer);
            blink_timer = 0;
            blink_status = true;
        }

        if (visible) {
            int flashTime = QGuiApplication::styleHints()->cursorFlashTime();
            if (flashTime >= 2) {
                blink_timer = ifce->startTimer(flashTime / 2);
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
        return std::find_if(tags.begin(), mid, text_eq) != mid || std::find_if(mid + 1, tags.end(), text_eq) != tags.end();
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
        onCurrentTextUpdate();
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
        completer->setWidget(ifce);
        QObject::connect(
            completer.get(),
                         qOverload<QString const&>(&QCompleter::activated),
                         [this](QString const& text) { currentText(text); });
    }

    QVector<QTextLayout::FormatRange> formatting() const {
        if (select_size == 0) {
            return {};
        }

        QTextLayout::FormatRange selection;
        selection.start = select_start;
        selection.length = select_size;
        selection.format.setBackground(
            ifce->palette().brush(QPalette::Highlight));
        selection.format.setForeground(
            ifce->palette().brush(QPalette::HighlightedText));
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
            currentText().remove(--cursor, 1);
        }
    }

    void selectAll() {
        select_start = 0;
        select_size = currentText().size();
    }

    void deselectAll() {
        select_start = 0;
        select_size = 0;
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

    QPoint offset() const {
        return QPoint{ifce->horizontalScrollBar()->value(),
                      ifce->verticalScrollBar()->value()};
    }

    void calcRects(QPoint& lt,
                   std::vector<Tag>& val,
                   QRect r,
                   QFontMetrics const& fm) const {
        for (auto i = 0u; i < val.size(); ++i) {
            auto& tag = val[i];
            if (i == editing_index && !cursorVisible() && tag.text.isEmpty()) {
                QRect tmp(lt, QSize(1, pillHeight(fm.height())));
                if (0 < i) {
                    tmp.moveRight(tags[i - 1].rect.right());
                }
                tag.rect = tmp;
            } else {
                QRect i_r(lt,
                          QSize(pillWidth(FONT_METRICS_WIDTH(fm, tag.text)),
                                pillHeight(fm.height())));

                // line wrapping
                if (r.right() < i_r.right() && // doesn't fit in current line
                    i_r.left() != r.left() // doesn't occupy entire line already
                ) {
                    i_r.moveTo(r.left(), i_r.bottom() + tag_v_spacing);
                    lt = i_r.topLeft();
                }

                tag.rect = i_r;
                lt.setX(i_r.right() + pills_h_spacing);
            }
        }
    }

    // CRTP member function.
    void onCurrentTextUpdate() {
        calcRectsUpdateScrollRanges();
        ifce->viewport()->update();
    }

    QRect contentsRect() const {
        return ifce->viewport()->contentsRect();
    }

    QRect calcRects(std::vector<Tag>& val) const {
        return calcRects(val, contentsRect());
    }

    QRect calcRects(std::vector<Tag>& val, QRect r) const {
        auto lt = r.topLeft();
        auto const fm = ifce->fontMetrics();
        calcRects(lt, val, r, fm);
        r.setBottom(lt.y() + pillHeight(fm.height()) - 1);
        return r;
    }

    void calcRectsUpdateScrollRanges() {
        calcRects(tags);
        updateVScrollRange();
        assert(!tags.empty()); // Invariant-1
        auto const max_width =
                std::max_element(begin(tags),
                                 end(tags),
                                 [](auto const& x, auto const& y) {
                                     return x.rect.width() < y.rect.width();
                                 })
                        ->rect.width();
        updateHScrollRange(max_width);
    }

    void updateVScrollRange() {
        auto const fm = ifce->fontMetrics();
        auto const row_h = pillHeight(fm.height()) + tag_v_spacing;
        ifce->verticalScrollBar()->setPageStep(row_h);
        assert(!tags.empty()); // Invariant-1
        auto const h = tags.back().rect.bottom() - tags.front().rect.top() + 1;
        auto const contents_rect = contentsRect();
        if (contents_rect.height() < h) {
            ifce->verticalScrollBar()->setRange(0, h - contents_rect.height());
        } else {
            ifce->verticalScrollBar()->setRange(0, 0);
        }
    }

    void updateHScrollRange() {
        assert(!tags.empty()); // Invariant-1
        auto const max_width =
                std::max_element(begin(tags),
                                 end(tags),
                                 [](auto const& x, auto const& y) {
                                     return x.rect.width() < y.rect.width();
                                 })
                        ->rect.width();
        updateHScrollRange(max_width);
    }

    void updateHScrollRange(int width) {
        auto const contents_rect_width = contentsRect().width();
        if (contents_rect_width < width) {
            ifce->horizontalScrollBar()->setRange(0,
                                                  width - contents_rect_width);
        } else {
            ifce->horizontalScrollBar()->setRange(0, 0);
        }
    }

    void ensureCursorIsVisibleV() {
        if (!cursorVisible()) {
            return;
        }
        auto const fm = ifce->fontMetrics();
        auto const row_h = pillHeight(fm.height());
        auto const vscroll = ifce->verticalScrollBar()->value();
        auto const cursor_top =
                currentRect().topLeft() + QPoint(qRound(cursorToX()), 0);
        auto const cursor_bottom = cursor_top + QPoint(0, row_h - 1);
        auto const contents_rect = contentsRect().translated(0, vscroll);
        if (contents_rect.bottom() < cursor_bottom.y()) {
            ifce->verticalScrollBar()->setValue(cursor_bottom.y() - row_h);
        } else if (cursor_top.y() < contents_rect.top()) {
            ifce->verticalScrollBar()->setValue(cursor_top.y() - 1);
        }
    }

    void ensureCursorIsVisibleH() {
        if (!cursorVisible()) {
            return;
        }
        auto const contents_rect = contentsRect().translated(
                ifce->horizontalScrollBar()->value(), 0);
        auto const cursor_x =
                // cursor pos
                (currentRect() - pill_thickness).left()
                + qRound(cursorToX())
                // pill right side
                + tag_cross_spacing + tag_cross_size + pill_thickness.right();
        if (contents_rect.right() < cursor_x) {
            ifce->horizontalScrollBar()->setValue(cursor_x
                                                  - contents_rect.width());
        } else if (cursor_x < contents_rect.left()) {
            ifce->horizontalScrollBar()->setValue(cursor_x - 1);
        }
    }

    TagsEdit* const ifce;
};

TagsEdit::TagsEdit(bool unique, QWidget* parent)
        : QAbstractScrollArea(parent)
        , impl(std::make_unique<Impl>(this, unique)) {
    QSizePolicy size_policy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    size_policy.setHeightForWidth(true);
    setSizePolicy(size_policy);

    setFocusPolicy(Qt::StrongFocus);
    viewport()->setCursor(Qt::IBeamCursor);
    setAttribute(Qt::WA_InputMethodEnabled, true);
    setMouseTracking(true);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    impl->setupCompleter();
    impl->setCursorVisible(hasFocus());
    impl->updateDisplayText();

    viewport()->setContentsMargins(1, 1, 1, 1);
}

TagsEdit::~TagsEdit() = default;

void TagsEdit::resizeEvent(QResizeEvent* event) {
    impl->calcRects(impl->tags);
    impl->updateVScrollRange();
    impl->updateHScrollRange();
    QAbstractScrollArea::resizeEvent(event);
}

void TagsEdit::focusInEvent(QFocusEvent* event) {
    impl->setCursorVisible(true);
    impl->updateDisplayText();
    impl->calcRects(impl->tags);
    viewport()->update();
    QAbstractScrollArea::focusInEvent(event);
}

void TagsEdit::focusOutEvent(QFocusEvent* event) {
    impl->setCursorVisible(false);
    impl->updateDisplayText();
    impl->calcRects(impl->tags);
    viewport()->update();
    QAbstractScrollArea::focusOutEvent(event);
}

void TagsEdit::paintEvent(QPaintEvent*) {
    QPainter p(viewport());
    p.setClipRect(impl->contentsRect());
    impl->drawContent(p);
}

void TagsEdit::timerEvent(QTimerEvent* event) {
    if (event->timerId() == impl->blink_timer) {
        impl->blink_status = !impl->blink_status;
        viewport()->update();
    }
}

void TagsEdit::mousePressEvent(QMouseEvent* event) {
    bool found = false;
    for (size_t i = 0; i < impl->tags.size(); ++i) {
        if (impl->inCrossArea(i, event->pos())) {
            impl->tags.erase(impl->tags.begin() + std::ptrdiff_t(i));
            if (i <= impl->editing_index) {
                --impl->editing_index;
            }
            found = true;
            break;
        }

        if (!impl->tags[i]
                     .rect.translated(-impl->offset())
                     .contains(event->pos())) {
            continue;
        }

        if (impl->editing_index == i) {
            impl->moveCursor(impl->text_layout.lineAt(0).xToCursor(
                                     (event->pos()
                                      - impl->currentRect()
                                                .translated(-impl->offset())
                                                .topLeft())
                                             .x()),
                             false);
        } else {
            impl->editTag(i);
        }

        found = true;
        break;
    }

    if (!found) {
        for (auto it = begin(impl->tags); it != end(impl->tags); ++it) {
            // Click of a row.
            if (it->rect.translated(-impl->offset()).bottom()
                < event->pos().y()) {
                continue;
            }

            // Last tag of the row.
            auto const row = it->rect.top();
            while (it != end(impl->tags) && it->rect.top() == row) {
                ++it;
            }

            impl->editNewTag(
                    static_cast<size_t>(std::distance(begin(impl->tags), it)));
            break;
        }

        event->accept();
    }

    if (event->isAccepted()) {
        impl->updateDisplayText();
        impl->calcRectsUpdateScrollRanges();
        impl->ensureCursorIsVisibleV();
        impl->ensureCursorIsVisibleH();
        impl->updateCursorBlinking();
        viewport()->update();
    }
}

QSize TagsEdit::sizeHint() const {
    return minimumSizeHint();
}

QSize TagsEdit::minimumSizeHint() const {
    ensurePolished();
    QFontMetrics fm = fontMetrics();
    QRect rect(0,
               0,
               impl->pillWidth(fm.maxWidth()),
               impl->pillHeight(fm.height()));
    rect += contentsMargins() + viewport()->contentsMargins()
          + viewportMargins();
    return rect.size();
}

int TagsEdit::heightForWidth(int w) const {
    auto const content_width = w;
    QRect contents_rect(0, 0, content_width, 100);
    contents_rect -= contentsMargins() + viewport()->contentsMargins()
                   + viewportMargins();
    auto tags = impl->tags;
    contents_rect = impl->calcRects(tags, contents_rect);
    contents_rect += contentsMargins() + viewport()->contentsMargins()
                   + viewportMargins();
    return contents_rect.height();
}

void TagsEdit::keyPressEvent(QKeyEvent* event) {
    event->setAccepted(false);
    bool unknown = false;

    if (event == QKeySequence::SelectAll) {
        impl->selectAll();
        event->accept();
    } else if (event == QKeySequence::SelectPreviousChar) {
        impl->moveCursor(impl->text_layout.previousCursorPosition(impl->cursor),
                         true);
        event->accept();
    } else if (event == QKeySequence::SelectNextChar) {
        impl->moveCursor(impl->text_layout.nextCursorPosition(impl->cursor),
                         true);
        event->accept();
    } else {
        switch (event->key()) {
        case Qt::Key_Left:
            if (impl->cursor == 0) {
                impl->editPreviousTag();
            } else {
                impl->moveCursor(
                        impl->text_layout.previousCursorPosition(impl->cursor),
                        false);
            }
            event->accept();
            break;
        case Qt::Key_Right:
            if (impl->cursor == impl->currentText().size()) {
                impl->editNextTag();
            } else {
                impl->moveCursor(
                        impl->text_layout.nextCursorPosition(impl->cursor),
                        false);
            }
            event->accept();
            break;
        case Qt::Key_Home:
            if (impl->cursor == 0) {
                impl->editTag(0);
            } else {
                impl->moveCursor(0, false);
            }
            event->accept();
            break;
        case Qt::Key_End:
            if (impl->cursor == impl->currentText().size()) {
                impl->editTag(impl->tags.size() - 1);
            } else {
                impl->moveCursor(impl->currentText().length(), false);
            }
            event->accept();
            break;
        case Qt::Key_Backspace:
            if (!impl->currentText().isEmpty()) {
                impl->removeBackwardOne();
            } else if (impl->editing_index > 0) {
                impl->editPreviousTag();
            }
            event->accept();
            break;
        case Qt::Key_Space:
            if (!impl->currentText().isEmpty()) {
                impl->editNewTag(impl->editing_index + 1);
            }
            event->accept();
            break;
        default:
            unknown = true;
        }
    }

    if (unknown && isAcceptableInput(*event)) {
        if (impl->hasSelection()) {
            impl->removeSelection();
        }
        impl->currentText().insert(impl->cursor, event->text());
        impl->cursor = impl->cursor + event->text().length();
        event->accept();
    }

    if (event->isAccepted()) {
        // update content
        impl->updateDisplayText();
        impl->calcRectsUpdateScrollRanges();
        impl->ensureCursorIsVisibleV();
        impl->ensureCursorIsVisibleH();
        impl->updateCursorBlinking();

        // complete
        impl->completer->setCompletionPrefix(impl->currentText());
        impl->completer->complete();

        viewport()->update();

        emit tagsEdited();
    }
}

void TagsEdit::completion(std::vector<QString> const& completions) {
    impl->completer = std::make_unique<QCompleter>([&] {
        QStringList ret;
        std::copy(completions.begin(),
                  completions.end(),
                  std::back_inserter(ret));
        return ret;
    }());
    impl->setupCompleter();
}

void TagsEdit::tags(std::vector<QString> const& tags) {
    impl->setTags(tags);
    verticalScrollBar()->setValue(0);
    horizontalScrollBar()->setValue(0);
    impl->editNewTag(impl->tags.size());
    impl->updateDisplayText();
    impl->calcRects(impl->tags);
    impl->updateHScrollRange();
    impl->updateVScrollRange();
    impl->ensureCursorIsVisibleH();
    impl->ensureCursorIsVisibleV();
    viewport()->update();
}

std::vector<QString> TagsEdit::tags() const {
    std::vector<QString> ret(impl->tags.size());
    std::transform(impl->tags.begin(),
                   impl->tags.end(),
                   ret.begin(),
                   [](Tag const& tag) { return tag.text; });
    assert(!ret.empty()); // Invariant-1
    if (ret[impl->editing_index].isEmpty()
        || (impl->unique && impl->isCurrentTagADuplicate())) {
        ret.erase(ret.begin()
                  + static_cast<std::ptrdiff_t>(impl->editing_index));
    }
    return ret;
}

void TagsEdit::mouseMoveEvent(QMouseEvent* event) {
    for (size_t i = 0; i < impl->tags.size(); ++i) {
        if (impl->inCrossArea(i, event->pos())) {
            viewport()->setCursor(Qt::ArrowCursor);
            return;
        }
    }
    if (impl->contentsRect().contains(event->pos())) {
        viewport()->setCursor(Qt::IBeamCursor);
    } else {
        QAbstractScrollArea::mouseMoveEvent(event);
    }
}

} // namespace everload_tags
