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
#include "scope_exit.hpp"

#include <QApplication>
#include <QDebug>
#include <QKeyEvent>
#include <QPainter>
#include <QPainterPath>
#include <QScrollBar>
#include <QStyle>
#include <QStyleHints>
#include <QStyleOptionFrame>
#include <QTextLayout>

#include <cassert>

namespace everload_tags {

struct TagsEdit::Impl : Common {
    explicit Impl(TagsEdit* ifce, Config config) : Common{{config.style}, {config.behavior}, {}}, ifce{ifce} {}

    QPoint offset() const {
        return QPoint{ifce->horizontalScrollBar()->value(), ifce->verticalScrollBar()->value()};
    }

    using Common::drawTags;

    template <std::ranges::input_range Range>
    void drawTags(QPainter& p, Range range) const {
        drawTags(p, range, ifce->fontMetrics(), -offset());
    }

    void setEditorText(QString const& text) {
        editorText() = text;
        moveCursor(editorText().length(), false);
        update1();
    }

    void setupCompleter() {
        completer->setWidget(ifce);
        QObject::connect(completer.get(), qOverload<QString const&>(&QCompleter::activated),
                         [this](QString const& text) { setEditorText(text); });
    }

    using Common::calcRects;

    void calcRects(QRect r, QPoint& lt, QFontMetrics const& fm) {
        auto const middle = tags.begin() + static_cast<ptrdiff_t>(editing_index);

        calcRects(lt, std::ranges::subrange(tags.begin(), middle), fm, r);

        if (cursorVisible() || !editorText().isEmpty()) {
            calcRects(lt, std::ranges::subrange(middle, middle + 1), fm, r);
        }

        calcRects(lt, std::ranges::subrange(middle + 1, tags.end()), fm, r);
    }

    QRect calcRects(QRect r) {
        auto lt = r.topLeft();
        auto const fm = ifce->fontMetrics();
        calcRects(r, lt, fm);
        r.setBottom(lt.y() + pillHeight(fm.height()) - 1);
        return r;
    }

    QRect calcRects() {
        return calcRects(contentsRect());
    }

    QRect contentsRect() const {
        return ifce->viewport()->contentsRect();
    }

    void calcRectsUpdateScrollRanges() {
        calcRects();
        updateVScrollRange();
        updateHScrollRange();
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
        auto const width = std::max_element(begin(tags), end(tags), [](auto const& x, auto const& y) {
                               return x.rect.width() < y.rect.width();
                           })->rect.width();

        auto const contents_rect_width = contentsRect().width();

        if (contents_rect_width < width) {
            ifce->horizontalScrollBar()->setRange(0, width - contents_rect_width);
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
        auto const cursor_top = editorRect().topLeft() + QPoint(qRound(cursorToX()), 0);
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
        auto const contents_rect = contentsRect().translated(ifce->horizontalScrollBar()->value(), 0);
        auto const cursor_x = (editorRect() - pill_thickness).left() + qRound(cursorToX());
        if (contents_rect.right() < cursor_x) {
            ifce->horizontalScrollBar()->setValue(cursor_x - contents_rect.width());
        } else if (cursor_x < contents_rect.left()) {
            ifce->horizontalScrollBar()->setValue(cursor_x - 1);
        }
    }

    void update1(bool keep_cursor_visible = true) {
        updateDisplayText();
        calcRectsUpdateScrollRanges();
        if (keep_cursor_visible) {
            ensureCursorIsVisibleV();
            ensureCursorIsVisibleH();
        }
        updateCursorBlinking(ifce);
        ifce->viewport()->update();
    }

    TagsEdit* const ifce;
};

TagsEdit::TagsEdit(QWidget* parent, Config config)
    : QAbstractScrollArea(parent), impl(std::make_unique<Impl>(this, config)) {
    QSizePolicy size_policy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    size_policy.setHeightForWidth(true);
    setSizePolicy(size_policy);

    setFocusPolicy(Qt::StrongFocus);
    viewport()->setCursor(Qt::IBeamCursor);
    setAttribute(Qt::WA_InputMethodEnabled, true);
    setMouseTracking(true);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    impl->setupCompleter();
    impl->setCursorVisible(hasFocus(), this);
    impl->updateDisplayText();

    viewport()->setContentsMargins(1, 1, 1, 1);
}

TagsEdit::~TagsEdit() = default;

void TagsEdit::resizeEvent(QResizeEvent* event) {
    impl->calcRects();
    impl->updateVScrollRange();
    impl->updateHScrollRange();
    QAbstractScrollArea::resizeEvent(event);
}

void TagsEdit::focusInEvent(QFocusEvent* event) {
    QAbstractScrollArea::focusInEvent(event);
    impl->focused_at = std::chrono::steady_clock::now();
    impl->setCursorVisible(true, this);
    impl->updateDisplayText();
    impl->calcRects();
    impl->ensureCursorIsVisibleH();
    impl->ensureCursorIsVisibleV();
    viewport()->update();
}

void TagsEdit::focusOutEvent(QFocusEvent* event) {
    QAbstractScrollArea::focusOutEvent(event);
    impl->setCursorVisible(false, this);
    impl->updateDisplayText();
    impl->calcRects();
    viewport()->update();
}

void TagsEdit::paintEvent(QPaintEvent* e) {
    QAbstractScrollArea::paintEvent(e);

    QPainter p(viewport());

    p.setClipRect(impl->contentsRect());

    auto const middle = impl->tags.cbegin() + static_cast<ptrdiff_t>(impl->editing_index);

    // tags
    impl->drawTags(p, std::ranges::subrange(impl->tags.cbegin(), middle));

    // todo: draw in one round all if the editor is inactive else, have this 3-part drawing.
    if (impl->cursorVisible()) {
        impl->drawEditor(p, palette(), impl->offset());
    } else if (!impl->editorText().isEmpty()) {
        impl->drawTags(p, std::ranges::subrange(middle, middle + 1));
    }

    // tags
    impl->drawTags(p, std::ranges::subrange(middle + 1, impl->tags.cend()));
}

void TagsEdit::timerEvent(QTimerEvent* event) {
    if (event->timerId() == impl->blink_timer) {
        impl->blink_status = !impl->blink_status;
        viewport()->update();
    }
}

void TagsEdit::mousePressEvent(QMouseEvent* event) {
    // we don't want to change cursor position if this event is part of focusIn
    using namespace std::chrono_literals;
    if (elapsed(impl->focused_at) < 1ms) {
        return;
    }

    bool keep_cursor_visible = true;
    EVERLOAD_TAGS_SCOPE_EXIT {
        impl->update1(keep_cursor_visible);
    };

    // remove or edit a tag
    for (size_t i = 0; i < impl->tags.size(); ++i) {
        if (!impl->tags[i].rect.translated(-impl->offset()).contains(event->pos())) {
            continue;
        }

        if (impl->inCrossArea(i, event->pos(), impl->offset())) {
            impl->removeTag(i);
            keep_cursor_visible = false;
        } else if (impl->editing_index == i) {
            impl->moveCursor(
                impl->text_layout.lineAt(0).xToCursor(
                    (event->pos() - (impl->editorRect() - impl->pill_thickness).translated(-impl->offset()).topLeft())
                        .x()),
                false);
        } else {
            impl->editTag(i);
        }

        return;
    }

    // add new tag closed to the cursor
    for (auto it = begin(impl->tags); it != end(impl->tags); ++it) {
        // find the row
        if (it->rect.translated(-impl->offset()).bottom() < event->pos().y()) {
            continue;
        }

        // find the closest spot
        auto const row = it->rect.translated(-impl->offset()).top();
        while (it != end(impl->tags) && it->rect.translated(-impl->offset()).top() == row &&
               event->pos().x() > it->rect.translated(-impl->offset()).left()) {
            ++it;
        }

        impl->editNewTag(static_cast<size_t>(std::distance(begin(impl->tags), it)));
        return;
    }

    // append a new nag
    impl->editNewTag(impl->tags.size());
}

QSize TagsEdit::sizeHint() const {
    return minimumSizeHint();
}

QSize TagsEdit::minimumSizeHint() const {
    ensurePolished();
    QFontMetrics fm = fontMetrics();
    QRect rect(0, 0, impl->pillWidth(fm.maxWidth(), true), impl->pillHeight(fm.height()));
    rect += contentsMargins() + viewport()->contentsMargins() + viewportMargins();
    return rect.size();
}

int TagsEdit::heightForWidth(int w) const {
    auto const content_width = w;
    QRect contents_rect(0, 0, content_width, 100);
    contents_rect -= contentsMargins() + viewport()->contentsMargins() + viewportMargins();
    auto tags = impl->tags;
    contents_rect = impl->calcRects(contents_rect);
    contents_rect += contentsMargins() + viewport()->contentsMargins() + viewportMargins();
    return contents_rect.height();
}

void TagsEdit::keyPressEvent(QKeyEvent* event) {
    if (event == QKeySequence::SelectAll) {
        impl->selectAll();
    } else if (event == QKeySequence::SelectPreviousChar) {
        impl->moveCursor(impl->text_layout.previousCursorPosition(impl->cursor), true);
    } else if (event == QKeySequence::SelectNextChar) {
        impl->moveCursor(impl->text_layout.nextCursorPosition(impl->cursor), true);
    } else {
        switch (event->key()) {
        case Qt::Key_Left:
            if (impl->cursor == 0) {
                impl->editPreviousTag();
            } else {
                impl->moveCursor(impl->text_layout.previousCursorPosition(impl->cursor), false);
            }
            break;
        case Qt::Key_Right:
            if (impl->cursor == impl->editorText().size()) {
                impl->editNextTag();
            } else {
                impl->moveCursor(impl->text_layout.nextCursorPosition(impl->cursor), false);
            }
            break;
        case Qt::Key_Home:
            if (impl->cursor == 0) {
                impl->editTag(0);
            } else {
                impl->moveCursor(0, false);
            }
            break;
        case Qt::Key_End:
            if (impl->cursor == impl->editorText().size()) {
                impl->editTag(impl->tags.size() - 1);
            } else {
                impl->moveCursor(impl->editorText().length(), false);
            }
            break;
        case Qt::Key_Backspace:
            if (!impl->editorText().isEmpty()) {
                impl->removeBackwardOne();
            } else if (impl->editing_index > 0) {
                impl->editPreviousTag();
            }
            break;
        case Qt::Key_Space:
            if (!impl->editorText().isEmpty()) {
                impl->editNewTag(impl->editing_index + 1);
            }
            break;
        default:
            if (isAcceptableInput(*event)) {
                if (impl->hasSelection()) {
                    impl->removeSelection();
                }
                impl->editorText().insert(impl->cursor, event->text());
                impl->cursor = impl->cursor + event->text().length();
                break;
            } else {
                event->setAccepted(false);
                return;
            }
        }
    }

    impl->update1();

    impl->completer->setCompletionPrefix(impl->editorText());
    impl->completer->complete();

    emit tagsEdited();
}

void TagsEdit::completion(std::vector<QString> const& completions) {
    impl->completer = std::make_unique<QCompleter>([&] {
        QStringList ret;
        std::copy(completions.begin(), completions.end(), std::back_inserter(ret));
        return ret;
    }());
    impl->setupCompleter();
}

void TagsEdit::tags(std::vector<QString> const& tags) {
    impl->setTags(tags);
    impl->update1();
}

std::vector<QString> TagsEdit::tags() const {
    std::vector<QString> ret(impl->tags.size());
    std::transform(impl->tags.begin(), impl->tags.end(), ret.begin(), [](Tag const& tag) { return tag.text; });
    assert(!ret.empty()); // Invariant-1
    if (ret[impl->editing_index].isEmpty() || (impl->unique && impl->isCurrentTagADuplicate())) {
        ret.erase(ret.begin() + static_cast<std::ptrdiff_t>(impl->editing_index));
    }
    return ret;
}

void TagsEdit::mouseMoveEvent(QMouseEvent* event) {
    for (size_t i = 0; i < impl->tags.size(); ++i) {
        if (impl->inCrossArea(i, event->pos(), impl->offset())) {
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

void TagsEdit::config(Config config) {
    static_cast<StyleConfig&>(*impl) = config.style;
    if (impl->unique && impl->unique != config.behavior.unique) {
        impl->removeDuplicates();
    }
    impl->update1();
}

Config TagsEdit::config() const {
    return Config{
        .style = static_cast<StyleConfig const&>(*impl),
        .behavior = static_cast<BehaviorConfig const&>(*impl),
    };
}

} // namespace everload_tags
