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

#include <tags/tags_area.hpp>

#include <QApplication>
#include <QCompleter>
#include <QDebug>
#include <QPainter>
#include <QPainterPath>
#include <QStyle>
#include <QStyleHints>
#include <QStyleOptionFrame>
#include <QTextLayout>
#include <QtGui/private/qinputcontrol_p.h>

#include <cassert>

#if QT_VERSION < QT_VERSION_CHECK(5, 11, 0)
#define FONT_METRICS_WIDTH(fmt, ...) fmt.width(__VA_ARGS__)
#else
#define FONT_METRICS_WIDTH(fmt, ...) fmt.horizontalAdvance(__VA_ARGS__)
#endif

namespace {

constexpr int tag_v_spacing = 2;
constexpr int tag_h_spacing = 3;
constexpr int tag_inner_top_padding = 3;
constexpr int tag_inner_bottom_padding = 3;
constexpr int tag_inner_left_padding = 3;
constexpr int tag_inner_right_padding = 4;
constexpr int tag_cross_width = 4;
constexpr int tag_cross_spacing = 2;

constexpr int tags_h_margin = 1;
constexpr int tags_v_margin = 1;

struct Tag {
    QString text;
    QRect rect;
    size_t row;
};

/// Non empty string filtering iterator
template <class It>
struct EmptySkipIterator {
    EmptySkipIterator() = default;

    // skip until `end`
    explicit EmptySkipIterator(It it, It end) : it(it), end(end) {
        while (this->it != end && this->it->text.isEmpty()) {
            ++this->it;
        }
    }

    explicit EmptySkipIterator(It it) : it(it) {}

    using difference_type = typename std::iterator_traits<It>::difference_type;
    using value_type = typename std::iterator_traits<It>::value_type;
    using pointer = typename std::iterator_traits<It>::pointer;
    using reference = typename std::iterator_traits<It>::reference;
    using iterator_category = std::output_iterator_tag;

    EmptySkipIterator& operator++() {
        while (++it != end && it->text.isEmpty())
            ;
        return *this;
    }

    value_type& operator*() {
        return *it;
    }

    pointer operator->() {
        return &(*it);
    }

    bool operator!=(EmptySkipIterator const& rhs) const {
        return it != rhs.it;
    }

    bool operator==(EmptySkipIterator const& rhs) const {
        return it == rhs.it;
    }

private:
    It it;
    It end;
};

template <class It>
EmptySkipIterator(It, It) -> EmptySkipIterator<It>;

} // namespace

struct TagsArea::Impl {
    explicit Impl(TagsArea* ifce)
        : ifce(ifce),
          tags{Tag()},
          editing_index(0),
          cursor(0),
          blink_timer(0),
          blink_status(true),
          select_start(0),
          select_size(0),
          ctrl(QInputControl::LineEdit),
          completer(std::make_unique<QCompleter>()) {}

    void initStyleOption(QStyleOptionFrame* frame) const {
        assert(frame);
        frame->initFrom(ifce);
        frame->rect = ifce->contentsRect();
        frame->lineWidth = ifce->style()->pixelMetric(QStyle::PM_DefaultFrameWidth, frame, ifce);
        frame->midLineWidth = 0;
        frame->state |= QStyle::State_Sunken;
        frame->features = QStyleOptionFrame::None;
    }

    inline QRectF crossRect(QRectF const& r) const {
        QRectF cross(QPointF{0, 0}, QSizeF{tag_cross_width, tag_cross_width});
        cross.moveCenter(QPointF(r.right() - tag_cross_width, r.center().y()));
        return cross;
    }

    bool inCrossArea(size_t tag_index, QPoint point) const {
        return crossRect(tags[tag_index].rect).adjusted(-2, 0, 0, 0).translated(-hscroll, 0).contains(point) &&
               (!cursorVisible() || tag_index != editing_index);
    }

    template <class It>
    void drawTags(QPainter& p, std::pair<It, It> range) const {
        for (auto it = range.first; it != range.second; ++it) {
            QRect const& i_r = it->rect.translated(-hscroll, 0);
            auto const text_pos = i_r.topLeft() +
                                  QPointF(tag_inner_left_padding,
                                          ifce->fontMetrics().ascent() +
                                              ((i_r.height() - ifce->fontMetrics().height()) / 2));

            // draw tag rect
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

    QRect tagsRect(QStyleOptionFrame const& frame) const {
        return ifce->style()
            ->subElementRect(QStyle::SE_LineEditContents, &frame, ifce)
            .adjusted(tags_h_margin, tags_v_margin, -tags_h_margin, -tags_v_margin);
    }

    QRect tagsRect() const {
        QStyleOptionFrame frame;
        initStyleOption(&frame);
        return tagsRect(frame);
    }

    QRect calcRects(std::vector<Tag>& tags) const {
        return calcRects(tags, tagsRect());
    }

    QRect calcRects(std::vector<Tag>& tags, QRect r) const {
        size_t row = 0;
        auto lt = r.topLeft();
        QFontMetrics fm = ifce->fontMetrics();

        auto const b = begin(tags);
        auto const e = end(tags);
        if (cursorVisible()) {
            auto const m = b + static_cast<std::ptrdiff_t>(editing_index);
            calcRects(lt, row, r, fm, std::make_pair(b, m));
            calcEditorRect(lt, row, r, fm, m);
            calcRects(lt, row, r, fm, std::make_pair(m + 1, e));
        } else {
            calcRects(lt, row, r, fm, std::make_pair(EmptySkipIterator(b, e), EmptySkipIterator(e)));
        }

        r.setBottom(lt.y() + fm.height() + fm.leading() + tag_inner_top_padding + tag_inner_bottom_padding - 1);
        return r;
    }

    template <class It>
    void calcRects(QPoint& lt, size_t& row, QRect r, QFontMetrics const& fm, std::pair<It, It> range) const {
        for (auto it = range.first; it != range.second; ++it) {
            // calc text rect
            const auto text_w = FONT_METRICS_WIDTH(fm, it->text);
            auto const text_h = fm.height() + fm.leading();
            auto const w = tag_inner_left_padding + tag_inner_right_padding + tag_cross_spacing + tag_cross_width;
            auto const h = tag_inner_top_padding + tag_inner_bottom_padding;
            QRect i_r(lt, QSize(text_w + w, text_h + h));

            // line wrapping
            if (i_r.right() > r.right()) { // next line
                i_r.moveTo(r.left(), i_r.bottom() + tag_v_spacing);
                ++row;
                lt = i_r.topLeft();
            }

            it->rect = i_r;
            it->row = row;
            lt.setX(i_r.right() + tag_h_spacing);
        }
    }

    template <class It>
    void calcEditorRect(QPoint& lt, size_t& row, QRect r, QFontMetrics const& fm, It it) const {
        auto const text_w = FONT_METRICS_WIDTH(fm, text_layout.text());
        auto const text_h = fm.height() + fm.leading();
        auto const w = tag_inner_left_padding + tag_inner_right_padding;
        auto const h = tag_inner_top_padding + tag_inner_bottom_padding;
        QRect i_r(lt, QSize(text_w + w, text_h + h));
        if (r.right() < i_r.right()) {
            i_r.moveTo(r.left(), i_r.bottom() + tag_v_spacing);
            ++row;
            lt = i_r.topLeft();
        }
        it->rect = i_r;
        it->row = row;
        lt.setX(i_r.right() + tag_h_spacing);
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

    void setEditingIndex(size_t i) {
        assert(i <= tags.size());
        if (currentText().isEmpty()) {
            tags.erase(std::next(tags.begin(), std::ptrdiff_t(editing_index)));
            if (editing_index <= i) {
                --i;
            }
        }
        editing_index = i;
    }

    void currentText(QString const& text) {
        currentText() = text;
        moveCursor(currentText().length(), false);
        updateDisplayText();
        calcRects(tags);
        ifce->update();
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

    void editNewTag() {
        tags.push_back(Tag());
        setEditingIndex(tags.size() - 1);
        moveCursor(0, false);
    }

    void setupCompleter() {
        completer->setWidget(ifce);
        connect(completer.get(), qOverload<QString const&>(&QCompleter::activated),
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

    qreal natrualWidth() const {
        return tags.back().rect.right() - tags.front().rect.left();
    }

    qreal cursorToX() {
        return text_layout.lineAt(0).cursorToX(cursor);
    }

    void calcHScroll(QRect r) {
        auto const rect = tagsRect();
        auto const width_used = qRound(natrualWidth()) + 1;
        int const cix = r.x() + qRound(cursorToX());
        if (width_used <= rect.width()) {
            // text fit
            hscroll = 0;
        } else if (cix - hscroll >= rect.width()) {
            // text doesn't fit, cursor is to the right of lineRect (scroll right)
            hscroll = cix - rect.width() + 1;
        } else if (cix - hscroll < 0 && hscroll < width_used) {
            // text doesn't fit, cursor is to the left of lineRect (scroll left)
            hscroll = cix;
        } else if (width_used - hscroll < rect.width()) {
            // text doesn't fit, text document is to the left of lineRect; align
            // right
            hscroll = width_used - rect.width() + 1;
        } else {
            //in case the text is bigger than the lineedit, the hscroll can never be negative
            hscroll = qMax(0, hscroll);
        }
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
        assert(i >= 0 && i < tags.size());
        setEditingIndex(i);
        moveCursor(currentText().size(), false);
    }

    TagsArea* const ifce;
    std::vector<Tag> tags;
    size_t editing_index;
    int cursor;
    int blink_timer;
    bool blink_status;
    QTextLayout text_layout;
    int select_start;
    int select_size;
    QInputControl ctrl;
    std::unique_ptr<QCompleter> completer;
    int hscroll{0};
};

TagsArea::TagsArea(QWidget* parent)
    : QWidget(parent), impl(std::make_unique<Impl>(this)) {
    QSizePolicy size_policy(QSizePolicy::Ignored, QSizePolicy::Minimum);
    size_policy.setHeightForWidth(true);
    setSizePolicy(size_policy);

    setFocusPolicy(Qt::StrongFocus);
    setCursor(Qt::IBeamCursor);
    setAttribute(Qt::WA_InputMethodEnabled, true);
    setMouseTracking(true);

    impl->setupCompleter();
    impl->setCursorVisible(hasFocus());
    impl->updateDisplayText();
}

TagsArea::~TagsArea() = default;

void TagsArea::resizeEvent(QResizeEvent*) {
    impl->calcRects(impl->tags);
}

void TagsArea::focusInEvent(QFocusEvent*) {
    impl->setCursorVisible(true);
    impl->updateDisplayText();
    impl->calcRects(impl->tags);
    update();
}

void TagsArea::focusOutEvent(QFocusEvent*) {
    impl->setCursorVisible(false);
    impl->updateDisplayText();
    impl->calcRects(impl->tags);
    update();
}

void TagsArea::paintEvent(QPaintEvent*) {
    QPainter p(this);

    // frame
    auto const frame = [this] {
        QStyleOptionFrame frame;
        impl->initStyleOption(&frame);
        return frame;
    }();

    // draw frame
    style()->drawPrimitive(QStyle::PE_PanelLineEdit, &frame, &p, this);

    // clip
    auto const rect = impl->tagsRect(frame);
    p.setClipRect(rect);

    if (impl->cursorVisible()) {
        // not terminated tag pos
        auto const& r = impl->currentRect();
        auto const& txt_p = r.topLeft() + QPointF(tag_inner_left_padding,
                                                  ((r.height() - fontMetrics().height()) / 2));

        // scroll
        impl->calcHScroll(r);

        // tags
        impl->drawTags(p, std::make_pair(impl->tags.cbegin(), std::next(impl->tags.cbegin(), std::ptrdiff_t(impl->editing_index))));

        // draw not terminated tag
        auto const formatting = impl->formatting();
        impl->text_layout.draw(&p, txt_p - QPointF(impl->hscroll, 0), formatting);

        // draw cursor
        if (impl->blink_status) {
            impl->text_layout.drawCursor(&p, txt_p - QPointF(impl->hscroll, 0), impl->cursor);
        }

        // tags
        impl->drawTags(p, std::make_pair(std::next(impl->tags.cbegin(), std::ptrdiff_t(impl->editing_index + 1)), impl->tags.cend()));
    } else {
        impl->drawTags(p, std::make_pair(EmptySkipIterator(impl->tags.begin(), impl->tags.end()), EmptySkipIterator(impl->tags.end())));
    }
}

void TagsArea::timerEvent(QTimerEvent* event) {
    if (event->timerId() == impl->blink_timer) {
        impl->blink_status = !impl->blink_status;
        update();
    }
}

void TagsArea::mousePressEvent(QMouseEvent* event) {
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
        impl->calcRects(impl->tags);
        impl->updateCursorBlinking();
        update();
    }
}

QSize TagsArea::sizeHint() const {
    return minimumSizeHint();
}

QSize TagsArea::minimumSizeHint() const {
    ensurePolished();
    QFontMetrics fm = fontMetrics();

    QStyleOptionFrame frame;
    impl->initStyleOption(&frame);

    int const h = fm.height() + fm.leading() +                       // text
                  tag_inner_top_padding + tag_inner_bottom_padding + // tag rect
                  2 * tags_v_margin;                                 // all tags rect

    int w = fm.maxWidth() +                                                                          // text
            tag_inner_left_padding + tag_inner_right_padding + tag_cross_spacing + tag_cross_width + // tag rect
            2 * tags_h_margin;                                                                       // all tags rect

    return style()->sizeFromContents(QStyle::CT_LineEdit, &frame, QSize(w, h), this);
}

int TagsArea::heightForWidth(int w) const {
    auto const content_width = w - contentsMargins().left() - contentsMargins().right();

    QStyleOptionFrame frame;
    impl->initStyleOption(&frame);
    frame.rect.setWidth(content_width);

    auto tags_rect = impl->tagsRect(frame);

    auto tags = impl->tags;
    tags_rect = impl->calcRects(tags, tags_rect);
    tags_rect.adjust(0, 0, 0, 2 * tags_v_margin);

    return style()
        ->sizeFromContents(QStyle::CT_LineEdit, &frame, QSize(content_width, tags_rect.height()), this)
        .height();
}

void TagsArea::keyPressEvent(QKeyEvent* event) {
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

    if (unknown && impl->ctrl.isAcceptableInput(event)) {
        if (impl->hasSelection()) { impl->removeSelection(); }
        impl->currentText().insert(impl->cursor, event->text());
        impl->cursor += event->text().length();
        event->accept();
    }

    if (event->isAccepted()) {
        // update content
        impl->updateDisplayText();
        impl->calcRects(impl->tags);
        impl->updateCursorBlinking();

        // complete
        impl->completer->setCompletionPrefix(impl->currentText());
        impl->completer->complete();

        update();

        emit tagsEdited();
    }
}

void TagsArea::completion(std::vector<QString> const& completions) {
    impl->completer = std::make_unique<QCompleter>(
        [&] {
            QStringList ret;
            std::copy(completions.begin(),
                      completions.end(), std::back_inserter(ret));
            return ret;
        }());
    impl->setupCompleter();
}

void TagsArea::tags(std::vector<QString> const& tags) {
    std::vector<Tag> t{Tag()};
    std::transform(tags.begin(), tags.end(), std::back_inserter(t),
                   [](QString const& text) {
                       return Tag{text, QRect(), 0};
                   });
    impl->tags = std::move(t);
    impl->editing_index = 0;
    impl->moveCursor(0, false);

    impl->editNewTag();
    impl->updateDisplayText();
    impl->calcRects(impl->tags);

    update();
}

std::vector<QString> TagsArea::tags() const {
    std::vector<QString> ret;
    std::transform(EmptySkipIterator(impl->tags.begin(), impl->tags.end()),
                   EmptySkipIterator(impl->tags.end()),
                   std::back_inserter(ret),
                   [](Tag const& tag) {
                       return tag.text;
                   });
    return ret;
}

void TagsArea::mouseMoveEvent(QMouseEvent* event) {
    for (size_t i = 0; i < impl->tags.size(); ++i) {
        if (impl->inCrossArea(i, event->pos())) {
            setCursor(Qt::ArrowCursor);
            return;
        }
    }
    setCursor(Qt::IBeamCursor);
}
