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

#include "everload_tags/tags_line_edit.hpp"

#include "common.hpp"
#include "scope_exit.hpp"

#include <QApplication>
#include <QCompleter>
#include <QDebug>
#include <QPainter>
#include <QPainterPath>
#include <QStyle>
#include <QStyleHints>
#include <QStyleOptionFrame>
#include <QTextLayout>

#include <algorithm>
#include <cassert>

namespace everload_tags {

struct TagsLineEdit::Impl : Common {
    explicit Impl(TagsLineEdit* const& ifce, Config config)
        : Common{{config.style}, {config.behavior}, {}}, ifce{ifce} {}

    QPoint offset() const {
        return {hscroll, 0};
    }

    using Common::drawTags;

    template <std::ranges::input_range Range>
    void drawTags(QPainter& p, Range range) const {
        drawTags(p, range, ifce->fontMetrics(), -offset());
    }

    QRect contentsRect() const {
        return ifce->contentsRect() - magic_margins;
    }

    using Common::calcRects;

    void calcRects() {
        auto const r = contentsRect();
        auto lt = r.topLeft();

        auto const middle = tags.begin() + static_cast<ptrdiff_t>(editing_index);

        calcRects(lt, std::ranges::subrange(tags.begin(), middle), ifce->fontMetrics());

        if (cursorVisible() || !editorText().isEmpty()) {
            calcRects(lt, std::ranges::subrange(middle, middle + 1), ifce->fontMetrics());
        }

        calcRects(lt, std::ranges::subrange(middle + 1, tags.end()), ifce->fontMetrics());
    }

    void setEditorText(QString const& text) {
        tags[editing_index].text = text;
        moveCursor(editorText().length(), false);
        update1();
    }

    void setupCompleter() {
        completer->setWidget(ifce);
        connect(completer.get(), static_cast<void (QCompleter::*)(QString const&)>(&QCompleter::activated), ifce,
                [this](QString const& text) { setEditorText(text); });
    }

    int pillsWidth() const {
        if (tags.size() == 1 && tags.front().text.isEmpty()) {
            return 0;
        }

        int left = tags.front().rect.left();
        int right = tags.back().rect.right();

        if (editing_index == 0 && !(cursorVisible() || !editorText().isEmpty())) {
            left = tags[1].rect.left();
        } else if (editing_index == tags.size() - 1 && !(cursorVisible() || !editorText().isEmpty())) {
            right = tags[tags.size() - 2].rect.right();
        }

        return right - left + 1;
    }

    void updateHScrollRange() {
        auto const contents_rect = contentsRect();
        auto const width_used = pillsWidth();

        if (contents_rect.width() < width_used) {
            hscroll_max = width_used - contents_rect.width();
        } else {
            hscroll_max = 0;
        }

        hscroll = std::clamp(hscroll, hscroll_min, hscroll_max);
    }

    void ensureCursorIsVisible() {
        auto const contents_rect = contentsRect().translated(offset());
        int const cursor_x = (editorRect() - pill_thickness).left() + qRound(cursorToX());

        if (contents_rect.right() < cursor_x) {
            hscroll = cursor_x - contents_rect.width();
        } else if (cursor_x < contents_rect.left()) {
            hscroll = cursor_x - 1;
        }

        hscroll = std::clamp(hscroll, hscroll_min, hscroll_max);
    }

    void update1(bool keep_cursor_visible = true) {
        updateDisplayText();
        calcRects();
        updateHScrollRange();
        if (keep_cursor_visible) {
            ensureCursorIsVisible();
        }
        updateCursorBlinking(ifce);
        ifce->update();
    }

    void initStyleOption(QStyleOptionFrame* option) const {
        assert(option);
        option->initFrom(ifce);
        option->rect = ifce->contentsRect();
        option->lineWidth = ifce->style()->pixelMetric(QStyle::PM_DefaultFrameWidth, option, ifce);
        option->midLineWidth = 0;
        option->state |= QStyle::State_Sunken;
        option->features = QStyleOptionFrame::None;
    }

    TagsLineEdit* const ifce;

    int const hscroll_min = 0;
    int hscroll = 0;
    int hscroll_max = 0;
};

TagsLineEdit::TagsLineEdit(QWidget* parent, Config config)
    : QWidget(parent), impl(std::make_unique<Impl>(this, config)) {
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setFocusPolicy(Qt::StrongFocus);
    setCursor(Qt::IBeamCursor);
    setAttribute(Qt::WA_InputMethodEnabled, true);
    setMouseTracking(true);

    impl->setupCompleter();
    impl->setCursorVisible(hasFocus(), this);
    impl->updateDisplayText();
}

TagsLineEdit::~TagsLineEdit() = default;

void TagsLineEdit::resizeEvent(QResizeEvent*) {
    impl->calcRects();
}

void TagsLineEdit::focusInEvent(QFocusEvent* e) {
    QWidget::focusInEvent(e);
    impl->focused_at = std::chrono::steady_clock::now();
    impl->setCursorVisible(true, this);
    impl->updateDisplayText();
    impl->calcRects();
    impl->updateHScrollRange();
    impl->ensureCursorIsVisible();
    update();
}

void TagsLineEdit::focusOutEvent(QFocusEvent* e) {
    QWidget::focusOutEvent(e);
    impl->setCursorVisible(false, this);
    impl->updateDisplayText();
    impl->calcRects();
    impl->updateHScrollRange();
    update();
}

void TagsLineEdit::paintEvent(QPaintEvent* e) {
    QWidget::paintEvent(e);

    QPainter p(this);

    // opt
    auto const panel = [this] {
        QStyleOptionFrame panel;
        impl->initStyleOption(&panel);
        return panel;
    }();

    // draw frame
    style()->drawPrimitive(QStyle::PE_PanelLineEdit, &panel, &p, this);

    // clip
    auto const rect = impl->contentsRect();
    p.setClipRect(rect);

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

void TagsLineEdit::timerEvent(QTimerEvent* event) {
    if (event->timerId() == impl->blink_timer) {
        impl->blink_status = !impl->blink_status;
        update();
    }
}

void TagsLineEdit::mousePressEvent(QMouseEvent* event) {
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
        // find the closest spot
        if (event->pos().x() > it->rect.translated(-impl->offset()).left()) {
            continue;
        }

        impl->editNewTag(static_cast<size_t>(std::distance(begin(impl->tags), it)));
        return;
    }

    // append a new nag
    impl->editNewTag(impl->tags.size());
}

QSize TagsLineEdit::sizeHint() const {
    ensurePolished();

    auto const fm = fontMetrics();
    QRect rect(0, 0, impl->pillWidth(fm.boundingRect(QLatin1Char('x')).width() * 17, true),
               impl->pillHeight(fm.height()));
    rect += magic_margins;

    QStyleOptionFrame opt;
    impl->initStyleOption(&opt);

    return (style()->sizeFromContents(QStyle::CT_LineEdit, &opt, rect.size(), this));
}

QSize TagsLineEdit::minimumSizeHint() const {
    ensurePolished();

    auto const fm = fontMetrics();
    QRect rect(0, 0, impl->pillWidth(fm.maxWidth(), true), impl->pillHeight(fm.height()));
    rect += magic_margins;

    QStyleOptionFrame opt;
    impl->initStyleOption(&opt);

    return (style()->sizeFromContents(QStyle::CT_LineEdit, &opt, rect.size(), this));
}

void TagsLineEdit::keyPressEvent(QKeyEvent* event) {
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
                impl->tags[impl->editing_index].text.insert(impl->cursor, event->text());
                impl->cursor += event->text().length();
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

void TagsLineEdit::completion(std::vector<QString> const& completions) {
    QStringList tmp;
    std::copy(begin(completions), end(completions), std::back_inserter(tmp));
    impl->completer = std::make_unique<QCompleter>(std::move(tmp));
    impl->setupCompleter();
}

void TagsLineEdit::tags(std::vector<QString> const& tags) {
    impl->setTags(tags);
    impl->update1();
}

std::vector<QString> TagsLineEdit::tags() const {
    std::vector<QString> ret(impl->tags.size());
    std::transform(impl->tags.begin(), impl->tags.end(), ret.begin(), [](auto const& tag) { return tag.text; });
    if (impl->editorText().isEmpty() || (impl->unique && 1 < std::count(ret.begin(), ret.end(), impl->editorText()))) {
        ret.erase(ret.begin() + static_cast<ptrdiff_t>(impl->editing_index));
    }
    return ret;
}

void TagsLineEdit::mouseMoveEvent(QMouseEvent* event) {
    event->accept();
    for (size_t i = 0; i < impl->tags.size(); ++i) {
        if (impl->inCrossArea(i, event->pos(), impl->offset())) {
            setCursor(Qt::ArrowCursor);
            return;
        }
    }
    setCursor(Qt::IBeamCursor);
}

void TagsLineEdit::wheelEvent(QWheelEvent* event) {
    event->accept();
    impl->calcRects();
    impl->updateHScrollRange();
    impl->hscroll = std::clamp(impl->hscroll - event->pixelDelta().x(), impl->hscroll_min, impl->hscroll_max);
    update();
}

void TagsLineEdit::config(Config config) {
    static_cast<StyleConfig&>(*impl) = config.style;
    if (impl->unique && impl->unique != config.behavior.unique) {
        impl->removeDuplicates();
    }
    impl->update1();
}

Config TagsLineEdit::config() const {
    return Config{
        .style = static_cast<StyleConfig const&>(*impl),
        .behavior = static_cast<BehaviorConfig const&>(*impl),
    };
}

} // namespace everload_tags
