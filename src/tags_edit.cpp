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

#include "tags/tags_edit.hpp"

#include "tags_impl.hpp"

#include <QApplication>
#include <QDebug>
#include <QPainter>
#include <QPainterPath>
#include <QScrollBar>
#include <QStyle>
#include <QStyleHints>
#include <QStyleOptionFrame>
#include <QTextLayout>

#include <cassert>

namespace yenxo_widgets {
namespace {

constexpr int tag_v_spacing = 2;

} // namespace

struct TagsEdit::Impl : TagsImpl<Impl> {
    explicit Impl(TagsEdit* ifce)
            : ifce(ifce) {
    }

    // CRTP member function.
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
                + tag_cross_spacing + tag_cross_width + pill_thickness.right();
        if (contents_rect.right() < cursor_x) {
            ifce->horizontalScrollBar()->setValue(cursor_x
                                                  - contents_rect.width());
        } else if (cursor_x < contents_rect.left()) {
            ifce->horizontalScrollBar()->setValue(cursor_x - 1);
        }
    }

    TagsEdit* const ifce;  // CRTP data member.
};

TagsEdit::TagsEdit(QWidget* parent)
        : QAbstractScrollArea(parent)
        , impl(std::make_unique<Impl>(this)) {
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

    if (unknown && impl->isAcceptableInput(event)) {
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
    std::vector<Tag> t(tags.size());
    std::transform(
            tags.begin(), tags.end(), t.begin(), [](QString const& text) {
                return Tag{text, QRect()};
            });

    // Ensure Invariant-1.
    t.erase(std::remove_if(t.begin(),
                           t.end(),
                           [](auto const& x) { return x.text.isEmpty(); }),
            t.end());
    if (t.empty()) {
        t.push_back(Tag{});
    }

    // Set to Default-state.
    impl->editing_index = 0;
    impl->cursor = 0;
    impl->select_size = 0;
    impl->select_start = 0;
    impl->tags = std::move(t);
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

    // Invariant-1
    assert(!ret.empty());
    if (ret[impl->editing_index].isEmpty()) {
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

} // namespace yenxo_widgets
