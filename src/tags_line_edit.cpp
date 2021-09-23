/*
  MIT License

  Copyright (c) 2019 Nicolai Trandafil

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

#include "tags/tags_line_edit.hpp"

#include "tags_impl.hpp"

#include <QApplication>
#include <QDebug>
#include <QPainter>
#include <QPainterPath>
#include <QStyle>
#include <QStyleHints>
#include <QStyleOptionFrame>
#include <QTextLayout>

#include <cassert>

namespace yenxo_widgets {
namespace {

// Without this margin the frame is not highlighted if the item is focused.
QMargins const magic_margins{1, 1, 1, 1};

} // namespace

struct TagsLineEdit::Impl : TagsImpl<Impl> {
    explicit Impl(TagsLineEdit* ifce)
            : ifce(ifce) {
    }

    // CRTP method.
    QPoint offset() const {
        return QPoint{hscroll, 0};
    }

    // Invariant-2 ensures the empty current tag's rect:
    // * has always width 1;
    // * if it is the first index, then its `left()` equals to the `left()` of
    // the next tag;
    // * if it is not the first index, then its `right()` equals to the
    // `right()` of the previous tag;
    void calcRects() {
        auto lt = contentsRect().topLeft();
        auto const fm = ifce->fontMetrics();
        for (auto i = 0u; i < tags.size(); ++i) {
            auto& tag = tags[i];
            if (i == editing_index && !cursorVisible() && tag.text.isEmpty()) {
                QRect tmp(lt, QSize(1, pillHeight(fm.height())));
                if (0 < i) {
                    tmp.moveRight(tags[i - 1].rect.right());
                }
                tag.rect = tmp;
            } else {
                QRect const i_r(
                        lt,
                        QSize(pillWidth(FONT_METRICS_WIDTH(fm, tag.text)),
                              pillHeight(fm.height())));
                tag.rect = i_r;
                lt.setX(i_r.right() + pills_h_spacing);
            }
        }
    }

    // CRTP member function.
    void onCurrentTextUpdate() {
        calcRectsUpdateScrollRanges();
        ifce->update();
    }

    void initStyleOption(QStyleOptionFrame* option) const {
        assert(option);
        option->initFrom(ifce);
        option->rect = ifce->contentsRect();
        option->lineWidth = ifce->style()->pixelMetric(
                QStyle::PM_DefaultFrameWidth, option, ifce);
        option->midLineWidth = 0;
        option->state |= QStyle::State_Sunken;
        option->features = QStyleOptionFrame::None;
    }

    QRect contentsRect() const {
        QStyleOptionFrame panel;
        initStyleOption(&panel);
        return ifce->style()->subElementRect(
                       QStyle::SE_LineEditContents, &panel, ifce)
             - magic_margins;
    }

    int allPillsWidth() const {
        return tags.back().rect.right() - tags.front().rect.left() + 1;
    }

    void calcRectsUpdateScrollRanges() {
        calcRects();
        updateHScrollRange();
    }

    void updateHScrollRange() {
        auto const cr = contentsRect();
        auto const width_used = allPillsWidth();
        if (cr.width() < width_used) {
            hscroll_max = width_used - cr.width();
        } else {
            hscroll_max = 0;
        }
        if (hscroll_max < hscroll) {
            hscroll = hscroll_max;
        }
    }

    void ensureCursorIsVisibleH() {
        if (!cursorVisible()) {
            return;
        }
        auto const contents_rect = contentsRect().translated(offset());
        int const cursor_x =
                // cursor pos
                (currentRect() - pill_thickness).left()
                + qRound(cursorToX())
                // pill right side
                + tag_cross_spacing + tag_cross_width + pill_thickness.right();
        if (contents_rect.right() < cursor_x) {
            hscroll = cursor_x - contents_rect.width();
        } else if (cursor_x < contents_rect.left()) {
            hscroll = cursor_x - 1;
        }
        hscroll = qBound(hscroll_min, hscroll, hscroll_max);
    }

    TagsLineEdit* const ifce; // CRTP data member.

    int const hscroll_min{0};
    int hscroll{0};
    int hscroll_max{0};
};

TagsLineEdit::TagsLineEdit(QWidget* parent)
    : QWidget(parent), impl(std::make_unique<Impl>(this)) {
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setFocusPolicy(Qt::StrongFocus);
    setCursor(Qt::IBeamCursor);
    setAttribute(Qt::WA_InputMethodEnabled, true);
    setMouseTracking(true);

    impl->setupCompleter();
    impl->setCursorVisible(hasFocus());
    impl->updateDisplayText();
}

TagsLineEdit::~TagsLineEdit() = default;

void TagsLineEdit::resizeEvent(QResizeEvent* event) {
    impl->calcRects();
    QWidget::resizeEvent(event);
}

void TagsLineEdit::focusInEvent(QFocusEvent* event) {
    impl->setCursorVisible(true);
    impl->updateDisplayText();
    impl->calcRects();
    update();
    QWidget::focusInEvent(event);
}

void TagsLineEdit::focusOutEvent(QFocusEvent* event) {
    impl->setCursorVisible(false);
    impl->updateDisplayText();
    impl->calcRectsUpdateScrollRanges();
    update();
    QWidget::focusOutEvent(event);
}

void TagsLineEdit::paintEvent(QPaintEvent*) {
    QPainter p(this);
    auto const panel = [this] {
        QStyleOptionFrame panel;
        impl->initStyleOption(&panel);
        return panel;
    }();
    style()->drawPrimitive(QStyle::PE_PanelLineEdit, &panel, &p, this);
    p.setClipRect(impl->contentsRect());
    impl->drawContent(p);
}

void TagsLineEdit::timerEvent(QTimerEvent* event) {
    if (event->timerId() == impl->blink_timer) {
        impl->blink_status = !impl->blink_status;
        update();
    }
}

void TagsLineEdit::mousePressEvent(QMouseEvent* event) {
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
        impl->editNewTag(impl->tags.size());
        event->accept();
    }

    if (event->isAccepted()) {
        impl->updateDisplayText();
        impl->calcRectsUpdateScrollRanges();
        impl->ensureCursorIsVisibleH();
        impl->updateCursorBlinking();
        update();
    }
}

QSize TagsLineEdit::sizeHint() const {
    ensurePolished();
    QFontMetrics const fm = fontMetrics();

    QRect rect(0,
               0,
               impl->pillWidth(fm.boundingRect(QLatin1Char('x')).width() * 17),
               impl->pillHeight(fm.height()));
    rect += magic_margins;

    QStyleOptionFrame opt;
    impl->initStyleOption(&opt);

    return (style()->sizeFromContents(
            QStyle::CT_LineEdit,
            &opt,
            rect.size().expandedTo(QApplication::globalStrut()),
            this));
}

QSize TagsLineEdit::minimumSizeHint() const {
    ensurePolished();
    QFontMetrics const fm = fontMetrics();

    QRect rect(0,
               0,
               impl->pillWidth(fm.maxWidth()),
               impl->pillHeight(fm.height()));
    rect += magic_margins;

    QStyleOptionFrame opt;
    impl->initStyleOption(&opt);
    return (style()->sizeFromContents(
            QStyle::CT_LineEdit,
            &opt,
            rect.size().expandedTo(QApplication::globalStrut()),
            this));
}

void TagsLineEdit::keyPressEvent(QKeyEvent* event) {
    event->setAccepted(false);
    bool unknown = false;

    if (event == QKeySequence::SelectAll) {
        impl->selectAll();
        event->accept();
    } else if (event == QKeySequence::SelectPreviousChar) {
        impl->moveCursor(impl->text_layout.previousCursorPosition(impl->cursor), true);
        event->accept();
    } else if (event == QKeySequence::SelectNextChar) {
        impl->moveCursor(impl->text_layout.nextCursorPosition(impl->cursor), true);
        event->accept();
    } else {
        switch (event->key()) {
        case Qt::Key_Left:
            if (impl->cursor == 0) {
                impl->editPreviousTag();
            } else {
                impl->moveCursor(impl->text_layout.previousCursorPosition(impl->cursor), false);
            }
            event->accept();
            break;
        case Qt::Key_Right:
            if (impl->cursor == impl->currentText().size()) {
                impl->editNextTag();
            } else {
                impl->moveCursor(impl->text_layout.nextCursorPosition(impl->cursor), false);
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
                impl->tags.insert(impl->tags.begin() + std::ptrdiff_t(impl->editing_index + 1), Tag());
                impl->editNextTag();
            }
            event->accept();
            break;
        default:
            unknown = true;
        }
    }

    if (unknown && impl->isAcceptableInput(event)) {
        if (impl->hasSelection()) { impl->removeSelection(); }
        impl->currentText().insert(impl->cursor, event->text());
        impl->cursor += event->text().length();
        event->accept();
    }

    if (event->isAccepted()) {
        // update content
        impl->updateDisplayText();
        impl->calcRectsUpdateScrollRanges();
        impl->ensureCursorIsVisibleH();
        impl->updateCursorBlinking();

        // complete
        impl->completer->setCompletionPrefix(impl->currentText());
        impl->completer->complete();

        update();

        emit tagsEdited();
    }
}

void TagsLineEdit::completion(std::vector<QString> const& completions) {
    impl->completer = std::make_unique<QCompleter>(
        [&] {
            QStringList ret;
            std::copy(completions.begin(),
                      completions.end(), std::back_inserter(ret));
            return ret;
        }());
    impl->setupCompleter();
}

void TagsLineEdit::tags(std::vector<QString> const& tags) {
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
    impl->hscroll = 0;

    impl->editNewTag(impl->tags.size());
    impl->updateDisplayText();
    impl->calcRects();
    impl->updateHScrollRange();
    impl->ensureCursorIsVisibleH();
    update();
}

std::vector<QString> TagsLineEdit::tags() const {
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

void TagsLineEdit::mouseMoveEvent(QMouseEvent* event) {
    bool arrow_cursor{false};
    for (size_t i = 0; i < impl->tags.size(); ++i) {
        if (impl->inCrossArea(i, event->pos())) {
            arrow_cursor = true;
            break;
        }
    }
    setCursor(arrow_cursor ? Qt::ArrowCursor : Qt::IBeamCursor);
    QWidget::mouseMoveEvent(event);
}

} // namespace yenxo_widgets
