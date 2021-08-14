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

#include <tags/tags_edit.hpp>

#include <QApplication>
#include <QCompleter>
#include <QDebug>
#include <QPainter>
#include <QPainterPath>
#include <QScrollBar>
#include <QStyle>
#include <QStyleHints>
#include <QStyleOptionFrame>
#include <QTextLayout>

#include <cassert>

#if QT_VERSION < QT_VERSION_CHECK(5, 11, 0)
#define FONT_METRICS_WIDTH(fmt, ...) fmt.width(__VA_ARGS__)
#else
#define FONT_METRICS_WIDTH(fmt, ...) fmt.horizontalAdvance(__VA_ARGS__)
#endif

namespace {

constexpr int tag_v_spacing = 2;
constexpr int tag_h_spacing = 3;

constexpr QMargins tag_inner(3, 3, 4, 3);

constexpr int tag_cross_width = 4;
constexpr int tag_cross_spacing = 2;

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
        begin = it;
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
    It begin;
    It it;
    It end;
};

template <class It>
EmptySkipIterator(It, It) -> EmptySkipIterator<It>;

} // namespace

struct TagsEdit::Impl {
    explicit Impl(TagsEdit* ifce)
        : ifce(ifce),
          tags{Tag()},
          editing_index(0),
          cursor(0),
          blink_timer(0),
          blink_status(true),
          select_start(0),
          select_size(0),
          completer(std::make_unique<QCompleter>()) {}

    inline QRectF crossRect(QRectF const& r) const {
        QRectF cross(QPointF{0, 0}, QSizeF{tag_cross_width, tag_cross_width});
        cross.moveCenter(QPointF(r.right() - tag_cross_width, r.center().y()));
        return cross;
    }

    bool inCrossArea(size_t tag_index, QPoint point) const {
        return crossRect(tags[tag_index].rect)
                       .adjusted(-tag_cross_spacing, 0, 0, 0)
                       .translated(-ifce->horizontalScrollBar()->value(), -ifce->verticalScrollBar()->value())
                       .contains(point)
            && (!cursorVisible() || tag_index != editing_index);
    }

    template <class It>
    void drawTags(QPainter& p, std::pair<It, It> range) const {
        for (auto it = range.first; it != range.second; ++it) {
            QRect const& i_r =
                    it->rect.translated(-ifce->horizontalScrollBar()->value(), -ifce->verticalScrollBar()->value());
            auto const text_pos =
                    i_r.topLeft()
                    + QPointF(tag_inner.left(),
                              ifce->fontMetrics().ascent() + ((i_r.height() - ifce->fontMetrics().height()) / 2));

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

    QRect contentsRect() const {
        return ifce->viewport()->contentsRect();
    }

    QRect calcRects(std::vector<Tag>& tags) const {
        return calcRects(tags, contentsRect());
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

        r.setBottom(lt.y() + fm.height() + fm.leading() + tag_inner.top() + tag_inner.bottom() - 1);
        return r;
    }

    template <class It>
    static void calcRects(QPoint& lt, size_t& row, QRect r, QFontMetrics const& fm, std::pair<It, It> range) {
        for (auto it = range.first; it != range.second; ++it) {
            // calc text rect
            const auto text_w = FONT_METRICS_WIDTH(fm, it->text);
            auto const text_h = fm.height() + fm.leading();
            auto const w = tag_inner.left() + tag_inner.right() + tag_cross_spacing + tag_cross_width;
            auto const h = tag_inner.top() + tag_inner.bottom();
            QRect i_r(lt, QSize(text_w + w, text_h + h));

            // line wrapping
            if (r.right() < i_r.right() && // doesn't fit in current line
                i_r.left() != r.left()     // doesn't occupy entire line already
            ) {
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
        auto const w = tag_inner.left() + tag_inner.right();
        auto const h = tag_inner.top() + tag_inner.bottom();
        QRect i_r(lt, QSize(text_w + w, text_h + h));

        // line wrapping
        if (r.right() < i_r.right() && // doesn't fit in current line
            i_r.left() != r.left()     // doesn't occupy entire line already
        ) {
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

    void calcRectsAndUpdateScrollRanges() {
        auto const row = tags.back().row;
        auto const max_width = std::max_element(begin(tags), end(tags), [](auto const& x, auto const& y) {
                                   return x.rect.width() < y.rect.width();
                               })->rect.width();

        calcRects(tags);

        if (row != tags.back().row) {
            updateVScrollRange();
        }

        auto const new_max_width = std::max_element(begin(tags), end(tags), [](auto const& x, auto const& y) {
                                       return x.rect.width() < y.rect.width();
                                   })->rect.width();

        if (max_width != new_max_width) {
            updateHScrollRange(new_max_width);
        }
    }

    void currentText(QString const& text) {
        currentText() = text;
        moveCursor(currentText().length(), false);
        updateDisplayText();
        calcRectsAndUpdateScrollRanges();
        ifce->viewport()->update();
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

    void editNewTag(size_t i) {
        tags.insert(std::next(begin(tags), static_cast<std::ptrdiff_t>(i)), Tag());
        if (editing_index >= i) {
            ++editing_index;
        }
        setEditingIndex(i);
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

    qreal cursorToX() {
        return text_layout.lineAt(0).cursorToX(cursor);
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

    void updateVScrollRange() {
        auto fm = ifce->fontMetrics();
        auto const row_h = fm.height() + fm.leading() + tag_inner.top() + tag_inner.bottom() + tag_v_spacing;
        ifce->verticalScrollBar()->setPageStep(row_h);
        auto const h = tags.back().rect.bottom() - tags.front().rect.top() + 1;
        auto const contents_rect = contentsRect();
        if (h > contents_rect.height()) {
            ifce->verticalScrollBar()->setRange(0, h - contents_rect.height());
        } else {
            ifce->verticalScrollBar()->setRange(0, 0);
        }
    }

    void updateHScrollRange() {
        auto const max_width = std::max_element(begin(tags), end(tags), [](auto const& x, auto const& y) {
                                   return x.rect.width() < y.rect.width();
                               })->rect.width();
        updateHScrollRange(max_width);
    }

    void updateHScrollRange(int width) {
        auto const contents_rect_width = contentsRect().width();
        if (width > contents_rect_width) {
            ifce->horizontalScrollBar()->setRange(0, width - contents_rect_width);
        } else {
            ifce->horizontalScrollBar()->setRange(0, 0);
        }
    }

    void ensureCursorIsVisibleV() {
        auto fm = ifce->fontMetrics();
        auto const row_h = fm.height() + fm.leading() + tag_inner.top() + tag_inner.bottom();
        auto const vscroll = ifce->verticalScrollBar()->value();
        auto const cursor_top = currentRect().topLeft() + QPoint(qRound(cursorToX()), 0);
        auto const cursor_bottom = cursor_top + QPoint(0, row_h - 1);
        auto const contents_rect = contentsRect().translated(0, vscroll);
        if (contents_rect.bottom() < cursor_bottom.y()) {
            ifce->verticalScrollBar()->setValue(cursor_bottom.y() - row_h);
        } else if (cursor_top.y() < contents_rect.top()) {
            ifce->verticalScrollBar()->setValue(cursor_top.y() - 1);
        }
    }

    void ensureCursorIsVisibleH() {
        auto const hscroll = ifce->horizontalScrollBar()->value();
        auto const contents_rect = contentsRect().translated(hscroll, 0);
        auto const cursor_x = (currentRect() - tag_inner).left() + qRound(cursorToX());
        if (contents_rect.right() < cursor_x) {
            ifce->horizontalScrollBar()->setValue(cursor_x - contents_rect.width());
        } else if (cursor_x < contents_rect.left()) {
            ifce->horizontalScrollBar()->setValue(cursor_x - 1);
        }
    }

    TagsEdit* const ifce;
    std::vector<Tag> tags;
    size_t editing_index;
    int cursor;
    int blink_timer;
    bool blink_status;
    QTextLayout text_layout;
    int select_start;
    int select_size;
    std::unique_ptr<QCompleter> completer;
    int hscroll{0};
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

void TagsEdit::resizeEvent(QResizeEvent*) {
    impl->calcRects(impl->tags);
    impl->updateVScrollRange();
    impl->updateHScrollRange();
}

void TagsEdit::focusInEvent(QFocusEvent*) {
    impl->setCursorVisible(true);
    impl->updateDisplayText();
    impl->calcRects(impl->tags);
    viewport()->update();
}

void TagsEdit::focusOutEvent(QFocusEvent*) {
    impl->setCursorVisible(false);
    impl->updateDisplayText();
    impl->calcRects(impl->tags);
    viewport()->update();
}

void TagsEdit::paintEvent(QPaintEvent*) {
    QPainter p(viewport());

    // clip
    auto const rect = impl->contentsRect();
    p.setClipRect(rect);

    if (impl->cursorVisible()) {
        // not terminated tag pos
        auto const& r = impl->currentRect();
        auto const& txt_p = r.topLeft() + QPointF(tag_inner.left(), ((r.height() - fontMetrics().height()) / 2));

        // tags
        impl->drawTags(p, std::make_pair(impl->tags.cbegin(), std::next(impl->tags.cbegin(), std::ptrdiff_t(impl->editing_index))));

        // draw not terminated tag
        auto const formatting = impl->formatting();
        impl->text_layout.draw(
                &p, txt_p - QPointF(horizontalScrollBar()->value(), verticalScrollBar()->value()), formatting);

        // draw cursor
        if (impl->blink_status) {
            impl->text_layout.drawCursor(
                    &p, txt_p - QPointF(horizontalScrollBar()->value(), verticalScrollBar()->value()), impl->cursor);
        }

        // tags
        impl->drawTags(p, std::make_pair(std::next(impl->tags.cbegin(), std::ptrdiff_t(impl->editing_index + 1)), impl->tags.cend()));
    } else {
        impl->drawTags(p, std::make_pair(EmptySkipIterator(impl->tags.begin(), impl->tags.end()), EmptySkipIterator(impl->tags.end())));
    }
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
                     .rect.translated(-horizontalScrollBar()->value(), -verticalScrollBar()->value())
                     .contains(event->pos())) {
            continue;
        }

        if (impl->editing_index == i) {
            impl->moveCursor(
                    impl->text_layout.lineAt(0).xToCursor(
                            (event->pos()
                             - impl->currentRect()
                                       .translated(-horizontalScrollBar()->value(), -verticalScrollBar()->value())
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
            if (it->rect.translated(-horizontalScrollBar()->value(), -verticalScrollBar()->value()).bottom()
                < event->pos().y()) {
                continue;
            }

            // Last tag of the row.
            auto const row = it->row;
            while (it->row == row && it != end(impl->tags)) {
                ++it;
            }

            impl->editNewTag(static_cast<size_t>(std::distance(begin(impl->tags), it)));
            break;
        }

        event->accept();
    }

    if (event->isAccepted()) {
        impl->updateDisplayText();
        impl->calcRectsAndUpdateScrollRanges();
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
    QRect rect(0, 0, fm.maxWidth() + tag_cross_spacing + tag_cross_width, fm.height() + fm.leading());
    rect += tag_inner + contentsMargins() + viewport()->contentsMargins() + viewportMargins();
    return rect.size();
}

int TagsEdit::heightForWidth(int w) const {
    auto const content_width = w;
    QRect contents_rect(0, 0, content_width, 100);
    contents_rect -= contentsMargins() + viewport()->contentsMargins() + viewportMargins();
    auto tags = impl->tags;
    contents_rect = impl->calcRects(tags, contents_rect);
    contents_rect += contentsMargins() + viewport()->contentsMargins() + viewportMargins();
    return contents_rect.height();
}

void TagsEdit::keyPressEvent(QKeyEvent* event) {
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
                impl->editNewTag(impl->editing_index + 1);
            }
            event->accept();
            break;
        default:
            unknown = true;
        }
    }

    if (unknown && isAcceptableInput(event)) {
        if (impl->hasSelection()) { impl->removeSelection(); }
        impl->currentText().insert(impl->cursor, event->text());
        impl->cursor = impl->cursor + event->text().length();
        event->accept();
    }

    if (event->isAccepted()) {
        // update content
        impl->updateDisplayText();
        impl->calcRectsAndUpdateScrollRanges();
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
    impl->completer = std::make_unique<QCompleter>(
        [&] {
            QStringList ret;
            std::copy(completions.begin(),
                      completions.end(), std::back_inserter(ret));
            return ret;
        }());
    impl->setupCompleter();
}

void TagsEdit::tags(std::vector<QString> const& tags) {
    std::vector<Tag> t{Tag()};
    std::transform(tags.begin(), tags.end(), std::back_inserter(t),
                   [](QString const& text) {
                       return Tag{text, QRect(), 0};
                   });
    impl->tags = std::move(t);
    impl->editNewTag(impl->tags.size());
    impl->updateDisplayText();
    impl->calcRectsAndUpdateScrollRanges();
    viewport()->update();
}

std::vector<QString> TagsEdit::tags() const {
    std::vector<QString> ret;
    std::transform(EmptySkipIterator(impl->tags.begin(), impl->tags.end()),
                   EmptySkipIterator(impl->tags.end()),
                   std::back_inserter(ret),
                   [](Tag const& tag) {
                       return tag.text;
                   });
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

bool TagsEdit::isAcceptableInput(const QKeyEvent* event) const
{
    const QString text = event->text();
    if (text.isEmpty())
        return false;

    const QChar c = text.at(0);

    // Formatting characters such as ZWNJ, ZWJ, RLM, etc. This needs to go before the
    // next test, since CTRL+SHIFT is sometimes used to input it on Windows.
    if (c.category() == QChar::Other_Format)
        return true;

    // QTBUG-35734: ignore Ctrl/Ctrl+Shift; accept only AltGr (Alt+Ctrl) on German keyboards
    if (event->modifiers() == Qt::ControlModifier || event->modifiers() == (Qt::ShiftModifier | Qt::ControlModifier)) {
        return false;
    }

    if (c.isPrint())
        return true;

    if (c.category() == QChar::Other_PrivateUse)
        return true;

    return false;
}
