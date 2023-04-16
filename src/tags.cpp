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

#include "everload_tags/tags.hpp"

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
// No empty tags except `current_index`

#if QT_VERSION < QT_VERSION_CHECK(5, 11, 0)
#define FONT_METRICS_WIDTH(fmt, ...) fmt.width(__VA_ARGS__)
#else
#define FONT_METRICS_WIDTH(fmt, ...) fmt.horizontalAdvance(__VA_ARGS__)
#endif

namespace everload_tags {
namespace {

// Without this margin the frame is not highlighted if the item is focused
constexpr QMargins magic_margins = {1, 1, 1, 1};

// ?
constexpr QMargins magic_margins2 = {1, 1, 1, 1};

/// Width of painting from text to the the pill border
constexpr QMargins pill_thickness = {3, 3, 4, 3};

/// Space between pills
constexpr int pills_h_spacing = 3;

/// Size of cross side
constexpr int tag_cross_size = 4;

/// Distance between text and cross
constexpr int tag_cross_spacing = 2;

constexpr int pillWidth(int text_width) {
    return text_width + pill_thickness.left() + tag_cross_spacing + tag_cross_size + pill_thickness.right();
}

constexpr int pillHeight(int text_height) {
    return text_height + pill_thickness.top() + pill_thickness.bottom();
}

/// \ref `bool QInputControl::isAcceptableInput(QKeyEvent const* event) const`
bool isAcceptableInput(QKeyEvent const& event) {
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

struct Tag {
    QString text;
    QRect rect;
};

} // namespace

struct Tags::Impl {
    explicit Impl(Tags* const& ifce, bool unique)
        : ifce(ifce),
          unique(unique),
          tags{Tag()},
          current_index(0),
          cursor(0),
          blink_timer(0),
          blink_status(true),
          select_start(0),
          select_size(0),
          completer(std::make_unique<QCompleter>()) {}

    void initStyleOption(QStyleOptionFrame* option) const {
        assert(option);
        option->initFrom(ifce);
        option->rect = ifce->contentsRect();
        option->lineWidth = ifce->style()->pixelMetric(QStyle::PM_DefaultFrameWidth, option, ifce);
        option->midLineWidth = 0;
        option->state |= QStyle::State_Sunken;
        option->features = QStyleOptionFrame::None;
    }

    inline QRectF crossRect(QRectF const& r) const {
        QRectF cross(QPointF{0, 0}, QSizeF{tag_cross_size, tag_cross_size});
        cross.moveCenter(QPointF(r.right() - tag_cross_size, r.center().y()));
        return cross;
    }

    bool inCrossArea(size_t tag_index, QPoint const& point) const {
        return crossRect(tags[tag_index].rect).adjusted(-1, -1, 1, 1).translated(-hscroll, 0).contains(point) &&
               (!cursorVisible() || tag_index != current_index);
    }

    template <class It>
    void drawTags(QPainter& p, std::pair<It, It> range) const {
        for (auto it = range.first; it != range.second; ++it) {
            QRect const& i_r = it->rect.translated(-hscroll, 0);
            auto const text_pos = i_r.topLeft() +
                                  QPointF(pill_thickness.left(),
                                          ifce->fontMetrics().ascent() +
                                              ((i_r.height() - ifce->fontMetrics().height()) / 2));

            // drag tag rect
            QColor const blue(0, 96, 100, 150);
            QPainterPath path;
            path.addRoundedRect(i_r, 4, 4);
            p.fillPath(path, blue);

            // draw text
            p.drawText(text_pos, it->text);

            // calc cross rect
            auto const i_cross_r = crossRect(i_r);

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

    QRect contentsRect() const {
        QStyleOptionFrame panel;
        initStyleOption(&panel);
        return ifce->style()->subElementRect(QStyle::SE_LineEditContents, &panel, ifce) - magic_margins;
    }

    void calcRects() {
        auto const r = contentsRect();
        auto lt = r.topLeft();

        auto const middle = tags.begin() + static_cast<ptrdiff_t>(current_index);

        calcRects(lt, r.height(), std::make_pair(tags.begin(), middle));

        if (cursorVisible()) {
            calcCurrentRect(lt, r.height());
        } else if (!currentText().isEmpty()) {
            calcRects(lt, r.height(), std::make_pair(middle, middle));
        }

        calcRects(lt, r.height(), std::make_pair(middle + 1, tags.end()));
    }

    template <class It>
    void calcRects(QPoint& lt, int height, std::pair<It, It> range) {
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
        if (currentText().isEmpty() || (unique && 1 < std::count_if(tags.begin(), tags.end(),
                                                                    [this](auto const& x) {
                                                                        return x.text == currentText();
                                                                    }))) {
            tags.erase(tags.begin() + static_cast<ptrdiff_t>(current_index));
            if (current_index <= i) {
                --i;
            }
        }
        current_index = i;
    }

    void currentText(QString const& text) {
        tags[current_index].text = text;
        moveCursor(currentText().length(), false);
        updateDisplayText();
        calcRects();
        ifce->update();
    }

    QString const& currentText() const {
        return tags[current_index].text;
    }

    QRect const& currentRect() const {
        return tags[current_index].rect;
    }

    void currentRect(QRect rect) {
        tags[current_index].rect = rect;
    }

    void editNewTag() {
        tags.push_back(Tag());
        currentIndex(tags.size() - 1);
        moveCursor(0, false);
    }

    void setupCompleter() {
        completer->setWidget(ifce);
        connect(completer.get(), static_cast<void (QCompleter::*)(QString const&)>(&QCompleter::activated), ifce,
                [this](QString const& text) {
                    currentText(text);
                });
    }

    QVector<QTextLayout::FormatRange> formatting() const {
        if (select_size == 0) {
            return {};
        }

        QTextLayout::FormatRange selection;
        selection.start = select_start;
        selection.length = select_size;
        selection.format.setBackground(ifce->palette().brush(QPalette::Highlight));
        selection.format.setForeground(ifce->palette().brush(QPalette::HighlightedText));
        return {selection};
    }

    bool hasSelection() const noexcept {
        return select_size > 0;
    }

    void removeSelection() {
        cursor = select_start;
        tags[current_index].text.remove(cursor, select_size);
        deselectAll();
    }

    void removeBackwardOne() {
        if (hasSelection()) {
            removeSelection();
        } else {
            tags[current_index].text.remove(--cursor, 1);
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
            int anchor = select_size > 0 && cursor == select_start
                             ? e
                         : select_size > 0 && cursor == e
                             ? select_start
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

        if (f == current_index) {
            ++f;
        } else if (l == current_index) {
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

        if (contents_rect.right() < cursor_x + tag_cross_spacing + tag_cross_size + pill_thickness.right() + pills_h_spacing) {
            hscroll = cursor_x - contents_rect.width() + tag_cross_spacing + tag_cross_size + pill_thickness.right() + pills_h_spacing;
        } else if (cursor_x - pill_thickness.left() - pills_h_spacing < contents_rect.left()) {
            hscroll = cursor_x - pill_thickness.left() - pills_h_spacing;
        }

        hscroll = std::clamp(hscroll, hscroll_min, hscroll_max);
    }

    void editPreviousTag() {
        if (current_index > 0) {
            currentIndex(current_index - 1);
            moveCursor(currentText().size(), false);
        }
    }

    void editNextTag() {
        if (current_index < tags.size() - 1) {
            currentIndex(current_index + 1);
            moveCursor(0, false);
        }
    }

    void editTag(size_t i) {
        assert(i >= 0 && i < tags.size());
        currentIndex(i);
        moveCursor(currentText().size(), false);
    }

    void removeTag(size_t i) {
        tags.erase(tags.begin() + static_cast<ptrdiff_t>(i));
        if (i <= current_index) {
            --current_index;
        }
    }

    Tags* const ifce;
    bool unique;
    std::vector<Tag> tags;
    size_t current_index;
    int cursor;
    int blink_timer;
    bool blink_status;
    QTextLayout text_layout;
    int select_start;
    int select_size;
    std::unique_ptr<QCompleter> completer;
    int const hscroll_min = 0;
    int hscroll = 0;
    int hscroll_max = 0;
};

Tags::Tags(QWidget* parent, TagsConfig const& config)
    : QWidget(parent),
      impl(std::make_unique<Impl>(this, config.unique)) {
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setFocusPolicy(Qt::StrongFocus);
    setCursor(Qt::IBeamCursor);
    setAttribute(Qt::WA_InputMethodEnabled, true);
    setMouseTracking(true);

    impl->setupCompleter();
    impl->setCursorVisible(hasFocus());
    impl->updateDisplayText();
}

Tags::~Tags() = default;

void Tags::resizeEvent(QResizeEvent*) {
    impl->calcRects();
}

void Tags::focusInEvent(QFocusEvent*) {
    impl->setCursorVisible(true);
    impl->updateDisplayText();
    impl->calcRects();
    update();
}

void Tags::focusOutEvent(QFocusEvent*) {
    impl->setCursorVisible(false);
    impl->updateDisplayText();
    impl->calcRects();
    update();
}

void Tags::paintEvent(QPaintEvent*) {
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

    auto const middle = impl->tags.cbegin() + static_cast<ptrdiff_t>(impl->current_index);

    // tags
    impl->drawTags(p, std::make_pair(impl->tags.cbegin(), middle));

    if (impl->cursorVisible()) {
        // draw not terminated tag
        auto const& r = impl->currentRect();
        auto const& txt_p = r.topLeft() + QPointF(pill_thickness.left(), (r.height() - fontMetrics().height()) / 2);
        auto const formatting = impl->formatting();
        impl->text_layout.draw(&p, txt_p - QPointF(impl->hscroll, 0), formatting);

        // draw cursor
        if (impl->blink_status) {
            impl->text_layout.drawCursor(&p, txt_p - QPointF(impl->hscroll, 0), impl->cursor);
        }
    } else if (!impl->currentText().isEmpty()) {
        impl->drawTags(p, std::make_pair(middle, middle));
    }

    // tags
    impl->drawTags(p, std::make_pair(middle + 1, impl->tags.cend()));
}

void Tags::timerEvent(QTimerEvent* event) {
    if (event->timerId() == impl->blink_timer) {
        impl->blink_status = !impl->blink_status;
        update();
    }
}

void Tags::mousePressEvent(QMouseEvent* event) {
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

        if (impl->current_index == i) {
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

QSize Tags::sizeHint() const {
    ensurePolished();

    auto const fm = fontMetrics();
    QRect rect(0, 0, pillWidth(fm.boundingRect(QLatin1Char('x')).width() * 17), pillHeight(fm.height()));
    rect += magic_margins;
    rect += magic_margins2;

    QStyleOptionFrame opt;
    impl->initStyleOption(&opt);

    return (style()->sizeFromContents(
        QStyle::CT_LineEdit,
        &opt,
        rect.size().expandedTo(QApplication::globalStrut()),
        this));
}

QSize Tags::minimumSizeHint() const {
    ensurePolished();

    auto const fm = fontMetrics();
    QRect rect(0, 0, pillWidth(fm.maxWidth()), pillHeight(fm.height()));
    rect += magic_margins;
    rect += magic_margins2;

    QStyleOptionFrame opt;
    impl->initStyleOption(&opt);

    return (style()->sizeFromContents(
        QStyle::CT_LineEdit,
        &opt,
        rect.size().expandedTo(QApplication::globalStrut()),
        this));
}

void Tags::keyPressEvent(QKeyEvent* event) {
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
            } else if (impl->current_index > 0) {
                impl->editPreviousTag();
            }
            event->accept();
            break;
        case Qt::Key_Space:
            if (!impl->currentText().isEmpty()) {
                impl->tags.insert(impl->tags.begin() + static_cast<ptrdiff_t>(impl->current_index + 1), Tag());
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
        impl->tags[impl->current_index].text.insert(impl->cursor, event->text());
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

void Tags::completion(std::vector<QString> const& completions) {
    impl->completer = std::make_unique<QCompleter>(QStringList(completions.begin(), completions.end()));
    impl->setupCompleter();
}

void Tags::tags(std::vector<QString> const& tags) {
    std::vector<Tag> t(tags.size());
    std::transform(tags.begin(), tags.end(), t.begin(),
                   [](QString const& text) {
                       return Tag{text, QRect()};
                   });

    // Ensure Invariant-1
    t.erase(std::remove_if(t.begin(), t.end(),
                           [](auto const& x) {
                               return x.text.isEmpty();
                           }),
            t.end());

    // Ensure `TagsConfig::unique`
    if (impl->unique) {
        t.erase(std::remove_if(t.begin(), t.end(),
                               [&tags](auto const& x) {
                                   return 1 < std::count(tags.begin(), tags.end(), x.text);
                               }),
                t.end());
    }

    impl->tags = std::move(t);
    impl->tags.push_back(Tag{});
    impl->current_index = impl->tags.size() - 1;
    impl->moveCursor(0, false);

    impl->updateDisplayText();
    impl->calcRects();
    impl->updateHScrollRange();
    impl->ensureCursorIsVisible();

    update();
}

std::vector<QString> Tags::tags() const {
    std::vector<QString> ret(impl->tags.size());
    std::transform(impl->tags.begin(), impl->tags.end(), ret.begin(),
                   [](auto const& tag) {
                       return tag.text;
                   });
    if (impl->currentText().isEmpty() || (impl->unique && 1 < std::count(ret.begin(), ret.end(), impl->currentText()))) {
        ret.erase(ret.begin() + static_cast<ptrdiff_t>(impl->current_index));
    }
    return ret;
}

void Tags::mouseMoveEvent(QMouseEvent* event) {
    event->accept();
    for (size_t i = 0; i < impl->tags.size(); ++i) {
        if (impl->inCrossArea(i, event->pos())) {
            setCursor(Qt::ArrowCursor);
            return;
        }
    }
    setCursor(Qt::IBeamCursor);
}

void Tags::wheelEvent(QWheelEvent* event) {
    event->accept();
    impl->calcRects();
    impl->updateHScrollRange();
    impl->hscroll = std::clamp(impl->hscroll - event->pixelDelta().x(), impl->hscroll_min, impl->hscroll_max);
    update();
}

} // namespace everload_tags
