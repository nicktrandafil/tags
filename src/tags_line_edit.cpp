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

// Invariant-1
// No empty tags except `editing_index`

namespace everload_tags {

struct TagsLineEdit::Impl : Common {
    explicit Impl(TagsLineEdit* const& ifce, bool unique)
        : Common{Style{},
                 Behavior{
                     .unique = unique,
                 },
                 {}},
          ifce{ifce} {}

    QPoint offset() const {
        return {hscroll, 0};
    }

    bool inCrossArea(size_t tag_index, QPoint const& point) const {
        return crossRect(tags[tag_index].rect).adjusted(-1, -1, 1, 1).translated(-offset()).contains(point) &&
               (!cursorVisible() || tag_index != editing_index);
    }

    template <class It> void drawTags(QPainter& p, std::pair<It, It> range) const {
        Style::drawTags(p, range, ifce->fontMetrics(), pill_thickness, tag_cross_size, -offset());
    }

    QRect contentsRect() const {
        QStyleOptionFrame panel;
        initStyleOption(&panel, ifce);
        return ifce->style()->subElementRect(QStyle::SE_LineEditContents, &panel, ifce) - magic_margins;
    }

    void calcRects() {
        auto const r = contentsRect();
        auto lt = r.topLeft();

        auto const middle = tags.begin() + static_cast<ptrdiff_t>(editing_index);

        calcRects(lt, r.height(), std::make_pair(tags.begin(), middle));

        if (cursorVisible()) {
            calcCurrentRect(lt, r.height());
        } else if (!currentText().isEmpty()) {
            calcRects(lt, r.height(), std::make_pair(middle, middle + 1));
        }

        calcRects(lt, r.height(), std::make_pair(middle + 1, tags.end()));
    }

    template <class It> void calcRects(QPoint& lt, int height, std::pair<It, It> range) {
        for (auto it = range.first; it != range.second; ++it) {
            const auto text_width = FONT_METRICS_WIDTH(ifce->fontMetrics(), it->text);
            it->rect = QRect(lt, QSize(pillWidth(text_width), height));
            lt.setX(it->rect.right() + pills_h_spacing);
        }
    }

    void calcCurrentRect(QPoint& lt, int height) {
        auto const text_width = FONT_METRICS_WIDTH(ifce->fontMetrics(), text_layout.text());
        currentRect(QRect(lt, QSize(pillWidth(text_width), height)));
        lt.setX(currentRect().right() + pills_h_spacing);
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

    bool cursorVisible() const {
        return blink_timer;
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

    void currentIndex(size_t i) {
        assert(i < tags.size());
        if (currentText().isEmpty() || (unique && 1 < std::count_if(tags.begin(), tags.end(), [this](auto const& x) {
                                                      return x.text == currentText();
                                                  }))) {
            tags.erase(tags.begin() + static_cast<ptrdiff_t>(editing_index));
            if (editing_index <= i) {
                --i;
            }
        }
        editing_index = i;
    }

    void currentText(QString const& text) {
        tags[editing_index].text = text;
        moveCursor(currentText().length(), false);
        updateDisplayText();
        calcRects();
        ifce->update();
    }

    QString const& currentText() const {
        return tags[editing_index].text;
    }

    // todo: remove
    QRect const& currentRect() const {
        return tags[editing_index].rect;
    }

    void currentRect(QRect rect) {
        tags[editing_index].rect = rect;
    }

    void editNewTag() {
        tags.push_back(Tag());
        currentIndex(tags.size() - 1);
        moveCursor(0, false);
    }

    void setupCompleter() {
        completer->setWidget(ifce);
        connect(completer.get(), static_cast<void (QCompleter::*)(QString const&)>(&QCompleter::activated), ifce,
                [this](QString const& text) { currentText(text); });
    }

    bool hasSelection() const noexcept {
        return select_size > 0;
    }

    void removeSelection() {
        cursor = select_start;
        tags[editing_index].text.remove(cursor, select_size);
        deselectAll();
    }

    void removeBackwardOne() {
        if (hasSelection()) {
            removeSelection();
        } else {
            tags[editing_index].text.remove(--cursor, 1);
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

    int pillsWidth() const {
        size_t f = 0;
        size_t l = tags.size() - 1;

        if (f == editing_index) {
            ++f;
        } else if (l == editing_index) {
            --l;
        }

        return f <= l ? tags[l].rect.right() - tags[f].rect.left() + 1 : 0;
    }

    qreal cursorToX() {
        return text_layout.lineAt(0).cursorToX(cursor);
    }

    void updateHScrollRange() {
        auto const contents_rect = contentsRect();
        auto const width_used = pillsWidth();

        if (contents_rect.width() < width_used) {
            hscroll_max = width_used - contents_rect.width();
        } else {
            hscroll_max = 0;
        }

        if (hscroll_max < hscroll) {
            hscroll = hscroll_max;
        }
    }

    void ensureCursorIsVisible() {
        auto const contents_rect = contentsRect().translated(hscroll, 0);
        int const cursor_x = (currentRect() - pill_thickness).left() + qRound(cursorToX());

        if (contents_rect.right() <
            cursor_x + tag_cross_spacing + tag_cross_size + pill_thickness.right() + pills_h_spacing) {
            hscroll = cursor_x - contents_rect.width() + tag_cross_spacing + tag_cross_size + pill_thickness.right() +
                      pills_h_spacing;
        } else if (cursor_x - pill_thickness.left() - pills_h_spacing < contents_rect.left()) {
            hscroll = cursor_x - pill_thickness.left() - pills_h_spacing;
        }

        hscroll = std::clamp(hscroll, hscroll_min, hscroll_max);
    }

    void editPreviousTag() {
        if (editing_index > 0) {
            currentIndex(editing_index - 1);
            moveCursor(currentText().size(), false);
        }
    }

    void editNextTag() {
        if (editing_index < tags.size() - 1) {
            currentIndex(editing_index + 1);
            moveCursor(0, false);
        }
    }

    void editTag(size_t i) {
        assert(i < tags.size());
        currentIndex(i);
        moveCursor(currentText().size(), false);
    }

    void removeTag(size_t i) {
        tags.erase(tags.begin() + static_cast<ptrdiff_t>(i));
        if (i <= editing_index) {
            --editing_index;
        }
    }

    TagsLineEdit* const ifce;

    int const hscroll_min = 0;
    int hscroll = 0;
    int hscroll_max = 0;
};

TagsLineEdit::TagsLineEdit(QWidget* parent, TagsConfig const& config)
    : QWidget(parent), impl(std::make_unique<Impl>(this, config.unique)) {
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

void TagsLineEdit::resizeEvent(QResizeEvent*) {
    impl->calcRects();
}

void TagsLineEdit::focusInEvent(QFocusEvent*) {
    impl->setCursorVisible(true);
    impl->updateDisplayText();
    impl->calcRects();
    update();
}

void TagsLineEdit::focusOutEvent(QFocusEvent*) {
    impl->setCursorVisible(false);
    impl->updateDisplayText();
    impl->calcRects();
    update();
}

void TagsLineEdit::paintEvent(QPaintEvent*) {
    QPainter p(this);

    // opt
    auto const panel = [this] {
        QStyleOptionFrame panel;
        initStyleOption(&panel, this);
        return panel;
    }();

    // draw frame
    style()->drawPrimitive(QStyle::PE_PanelLineEdit, &panel, &p, this);

    // clip
    auto const rect = impl->contentsRect();
    p.setClipRect(rect);

    auto const middle = impl->tags.cbegin() + static_cast<ptrdiff_t>(impl->editing_index);

    // tags
    impl->drawTags(p, std::make_pair(impl->tags.cbegin(), middle));

    // todo: draw in one round all if the editor is inactive else, have this 3-part drawing.
    if (impl->cursorVisible()) {
        impl->drawEditor(p, palette(), impl->offset());
    } else if (!impl->currentText().isEmpty()) {
        impl->drawTags(p, std::make_pair(middle, middle + 1));
    }

    // tags
    impl->drawTags(p, std::make_pair(middle + 1, impl->tags.cend()));
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
            impl->removeTag(i);
            found = true;
            break;
        }

        if (!impl->tags[i].rect.translated(-impl->hscroll, 0).contains(event->pos())) {
            continue;
        }

        if (impl->editing_index == i) {
            impl->moveCursor(impl->text_layout.lineAt(0).xToCursor(
                                 (event->pos() - impl->currentRect().translated(-impl->hscroll, 0).topLeft()).x()),
                             false);
        } else {
            impl->editTag(i);
        }

        found = true;
        break;
    }

    if (!found) {
        impl->editNewTag();
        event->accept();
    }

    if (event->isAccepted()) {
        impl->updateDisplayText();
        impl->calcRects();
        impl->updateHScrollRange();
        impl->ensureCursorIsVisible();
        impl->updateCursorBlinking();
        update();
    }
}

QSize TagsLineEdit::sizeHint() const {
    ensurePolished();

    auto const fm = fontMetrics();
    QRect rect(0, 0, impl->pillWidth(fm.boundingRect(QLatin1Char('x')).width() * 17), impl->pillHeight(fm.height()));
    rect += magic_margins;
    rect += magic_margins2;

    QStyleOptionFrame opt;
    initStyleOption(&opt, this);

    return (style()->sizeFromContents(QStyle::CT_LineEdit, &opt, rect.size(), this));
}

QSize TagsLineEdit::minimumSizeHint() const {
    ensurePolished();

    auto const fm = fontMetrics();
    QRect rect(0, 0, impl->pillWidth(fm.maxWidth()), impl->pillHeight(fm.height()));
    rect += magic_margins;
    rect += magic_margins2;

    QStyleOptionFrame opt;
    initStyleOption(&opt, this);

    return (style()->sizeFromContents(QStyle::CT_LineEdit, &opt, rect.size(), this));
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
                impl->tags.insert(impl->tags.begin() + static_cast<ptrdiff_t>(impl->editing_index + 1), Tag());
                impl->editNextTag();
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
        impl->tags[impl->editing_index].text.insert(impl->cursor, event->text());
        impl->cursor += event->text().length();
        event->accept();
    }

    if (event->isAccepted()) {
        // update content
        impl->updateDisplayText();
        impl->calcRects();
        impl->updateHScrollRange();
        impl->ensureCursorIsVisible();
        impl->updateCursorBlinking();

        // complete
        impl->completer->setCompletionPrefix(impl->currentText());
        impl->completer->complete();

        update();

        emit tagsEdited();
    }
}

void TagsLineEdit::completion(std::vector<QString> const& completions) {
    QStringList tmp;
    std::copy(begin(completions), end(completions), std::back_inserter(tmp));
    impl->completer = std::make_unique<QCompleter>(std::move(tmp));
    impl->setupCompleter();
}

void TagsLineEdit::tags(std::vector<QString> const& tags) {
    std::vector<Tag> t(tags.size());
    std::transform(tags.begin(), tags.end(), t.begin(), [](QString const& text) { return Tag{text, QRect()}; });

    // Ensure Invariant-1
    t.erase(std::remove_if(t.begin(), t.end(), [](auto const& x) { return x.text.isEmpty(); }), t.end());

    // Ensure `TagsConfig::unique`
    if (impl->unique) {
        t.erase(std::remove_if(t.begin(), t.end(),
                               [&tags](auto const& x) { return 1 < std::count(tags.begin(), tags.end(), x.text); }),
                t.end());
    }

    impl->tags = std::move(t);
    impl->tags.push_back(Tag{});
    impl->editing_index = impl->tags.size() - 1;
    impl->moveCursor(0, false);

    impl->updateDisplayText();
    impl->calcRects();
    impl->updateHScrollRange();
    impl->ensureCursorIsVisible();

    update();
}

std::vector<QString> TagsLineEdit::tags() const {
    std::vector<QString> ret(impl->tags.size());
    std::transform(impl->tags.begin(), impl->tags.end(), ret.begin(), [](auto const& tag) { return tag.text; });
    if (impl->currentText().isEmpty() ||
        (impl->unique && 1 < std::count(ret.begin(), ret.end(), impl->currentText()))) {
        ret.erase(ret.begin() + static_cast<ptrdiff_t>(impl->editing_index));
    }
    return ret;
}

void TagsLineEdit::mouseMoveEvent(QMouseEvent* event) {
    event->accept();
    for (size_t i = 0; i < impl->tags.size(); ++i) {
        if (impl->inCrossArea(i, event->pos())) {
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

} // namespace everload_tags
