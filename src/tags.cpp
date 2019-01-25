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


// ifce
#include <tags/tags.hpp>

// qt
#include <QPainter>
#include <QStyleOptionFrame>
#include <QStyle>
#include <QApplication>
#include <QStyleHints>
#include <QTextLayout>
#include <QtGui/private/qinputcontrol_p.h>
#include <QCompleter>


namespace {


constexpr int verticalMargin = 3;
constexpr int topTextMargin = 1;
constexpr int bottomTextMargin = 1;
constexpr int bottommargin = 1;
constexpr int topmargin = 1;

constexpr int horizontalMargin = 3;
constexpr int leftmargin = 1;
constexpr int rightmargin = 1;

constexpr int tag_spacing = 3;
constexpr int tag_inner_left_spacing = 3;
constexpr int tag_inner_right_spacing = 4;
constexpr int tag_cross_width = 4;
constexpr int tag_cross_spacing = 2;


struct Tag {
    QString text;
    QRect rect;
};


/// Non empty string filtering iterator
template <class It>
struct EmptySkipIterator {
    EmptySkipIterator() = default;

    // skip until `end`
    explicit EmptySkipIterator(It it, It end) : it(it), end(end) {
        while (this->it != end && this->it->text.isEmpty()) { ++this->it; }
    }

    explicit EmptySkipIterator(It it) : it(it) {}

    using difference_type = typename std::iterator_traits<It>::difference_type;
    using value_type = typename std::iterator_traits<It>::value_type;
    using pointer = typename std::iterator_traits<It>::pointer;
    using reference = typename std::iterator_traits<It>::reference;
    using iterator_category = std::output_iterator_tag;

    EmptySkipIterator& operator++() {
        while (++it != end && it->text.isEmpty());
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

private:
    It it;
    It end;
};


}


struct Tags::Impl {
    explicit Impl(Tags* const& ifce)
        : ifce(ifce)
        , tags{Tag()}
        , editing_index(0)
        , cursor(0)
        , blink_timer(0)
        , blink_status(true)
        , ctrl(QInputControl::LineEdit)
        , completer(std::make_unique<QCompleter>())
    {}

    void initStyleOption(QStyleOptionFrame* option) const {
        assert(option);
        option->initFrom(ifce);
        option->rect = ifce->contentsRect();
        option->lineWidth = ifce->style()->pixelMetric(QStyle::PM_DefaultFrameWidth, option, ifce);
        option->midLineWidth = 0;
        option->state |= QStyle::State_Sunken;
        option->features = QStyleOptionFrame::None;
    }

    template <class It>
    void drawTags(QPainter& p, std::pair<It, It> range) {
        for (auto it = range.first; it != range.second; ++it) {
            QRect const& i_r = it->rect;
            auto const text_pos = i_r.topLeft() +
                    QPointF(tag_inner_left_spacing,
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
            QRectF i_cross_r(QPointF{0, 0}, QSizeF{tag_cross_width, tag_cross_width});
            i_cross_r.moveCenter(QPointF(i_r.right() - tag_cross_width, i_r.center().y()));

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

    QRect cRect() const {
        QStyleOptionFrame panel;
        initStyleOption(&panel);
        QRect r = ifce->style()->subElementRect(QStyle::SE_LineEditContents, &panel, ifce);
        r.setY(r.y() + topTextMargin);
        r.setBottom(r.bottom() - bottomTextMargin);
        return r;
    }

    void calcRects() {
        auto const r = cRect();
        auto lt = r.topLeft();

        if (cursorVisible()) {
            calcRects(lt, r.height(), std::make_pair(
                          tags.begin(),
                          tags.begin() + std::ptrdiff_t(editing_index)));
            calcEditorRect(lt, r.height());
            calcRects(lt, r.height(), std::make_pair(
                          tags.begin() + std::ptrdiff_t(editing_index + 1),
                          tags.end()));
        } else {
            calcRects(lt, r.height(), std::make_pair(
                          EmptySkipIterator(tags.begin(), tags.end()),
                          EmptySkipIterator(tags.end())));
        }
    }

    template <class It>
    void calcRects(QPoint& lt, int height, std::pair<It, It> range) {
        for (auto it = range.first; it != range.second; ++it) {
            // calc text rect
            const auto i_width = ifce->fontMetrics().width(it->text);
            QRect i_r(lt, QSize(i_width, height));
            i_r.translate(tag_inner_left_spacing, 0);
            i_r.adjust(-tag_inner_left_spacing, 0,
                       tag_inner_right_spacing + tag_cross_spacing + tag_cross_width, 0);
            it->rect = i_r;
            lt.setX(i_r.right() + tag_spacing);
        }
    }

    void calcEditorRect(QPoint& lt, int height) {
        auto const w = ifce->fontMetrics().width(text_layout.text()) +
                tag_inner_left_spacing + tag_inner_right_spacing;
        currentRect() = QRect(lt, QSize(w, height));
        lt += QPoint(w + tag_spacing, 0);
    }

    void setCursorVisible(bool visible) {
        if (blink_timer) {
            ifce->killTimer(blink_timer);
            blink_timer = 0;
            blink_status = true;
        }

        if (visible) {
            int flashTime = QGuiApplication::styleHints()->cursorFlashTime();
            if (flashTime >= 2) { blink_timer = ifce->startTimer(flashTime / 2); }
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
            if (editing_index <= i) { --i; }
        }
        editing_index = i;
    }

    void currentText(QString const& text) {
        currentText() = text;
        cursor = currentText().length();
        updateDisplayText();
        calcRects();
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

    QRect& currentRect() {
        return tags[editing_index].rect;
    }

    void appendTag() {
        tags.push_back(Tag());
        setEditingIndex(tags.size() - 1);
        cursor = 0;
    }

    void setupCompleter() {
        completer->setWidget(ifce);
        connect(completer.get(), qOverload<QString const&>(&QCompleter::activated),
                [this] (QString const& text) {
            currentText(text);
        });
    }

    Tags* const ifce;
    std::vector<Tag> tags;
    size_t editing_index;
    int cursor;
    int blink_timer;
    bool blink_status;
    QTextLayout text_layout;
    QInputControl ctrl;
    std::unique_ptr<QCompleter> completer;
};


Tags::Tags(QWidget* parent)
    : QWidget(parent)
    , impl(std::make_unique<Impl>(this))
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setFocusPolicy(Qt::StrongFocus);
    setCursor(Qt::IBeamCursor);
    setAttribute(Qt::WA_InputMethodEnabled, true);

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

    if (impl->cursorVisible()) {
        // tags
        impl->drawTags(p, std::make_pair(impl->tags.cbegin(),
                                         std::next(impl->tags.cbegin(),
                                                   std::ptrdiff_t(impl->editing_index))));

        // draw not terminated tag
        auto const& r = impl->currentRect();
        auto const& txt_p = r.topLeft() + QPointF(tag_inner_left_spacing,
                                                  ((r.height() - fontMetrics().height()) / 2));
        impl->text_layout.draw(&p, txt_p);

        // draw cursor
        if (impl->blink_status) {
            impl->text_layout.drawCursor(&p, txt_p, impl->cursor);
        }

        // tags
        impl->drawTags(p, std::make_pair(
                           std::next(impl->tags.cbegin(),
                                     std::ptrdiff_t(impl->editing_index + 1)),
                           impl->tags.cend()));
    } else {
        impl->drawTags(p, std::make_pair(
                           EmptySkipIterator(impl->tags.begin(), impl->tags.end()),
                           EmptySkipIterator(impl->tags.end())));
    }
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
        if (!impl->tags[i].rect.contains(event->pos())) { continue; }

        if (impl->editing_index == i) {
            impl->cursor = impl->text_layout.lineAt(0).xToCursor(
                        (event->pos() - impl->currentRect().topLeft()).x());
        } else {
            impl->setEditingIndex(i);
            impl->cursor = impl->currentText().size();
        }

        found = true;
        event->accept();

        break;
    }

    if (!found) {
        impl->appendTag();
        event->accept();
    }

    if (event->isAccepted()) {
        impl->updateDisplayText();
        impl->calcRects();
        impl->updateCursorBlinking();
        update();
    }
}


QSize Tags::sizeHint() const {
    ensurePolished();
    QFontMetrics fm(font());
    int h = fm.height() + 2 * verticalMargin + topTextMargin + bottomTextMargin
            + topmargin + bottommargin;
    int w = fm.horizontalAdvance(QLatin1Char('x')) * 17 + 2 * horizontalMargin
            + leftmargin + rightmargin; // "some"
    QStyleOptionFrame opt;
    impl->initStyleOption(&opt);
    return (style()->sizeFromContents(QStyle::CT_LineEdit, &opt,
                                      QSize(w, h).expandedTo(QApplication::globalStrut()), this));
}


QSize Tags::minimumSizeHint() const {
    ensurePolished();
    QFontMetrics fm = fontMetrics();
    int h = fm.height() + qMax(2*verticalMargin, fm.leading())
            + topTextMargin + bottomTextMargin
            + topmargin + bottommargin;
    int w = fm.maxWidth() + leftmargin + rightmargin;
    QStyleOptionFrame opt;
    impl->initStyleOption(&opt);
    return (style()->sizeFromContents(QStyle::CT_LineEdit, &opt,
                                      QSize(w, h).expandedTo(QApplication::globalStrut()), this));
}


void Tags::keyPressEvent(QKeyEvent* event) {
    bool unknown = false;

    if (event->key() == Qt::Key_Left) {
        impl->cursor = impl->text_layout.previousCursorPosition(impl->cursor);
        event->accept();
    } else if (event->key() == Qt::Key_Right) {
        impl->cursor = impl->text_layout.nextCursorPosition(impl->cursor);
        event->accept();
    } else if (event->key() == Qt::Key_Home) {
        impl->cursor = 0;
        event->accept();
    } else if (event->key() == Qt::Key_End) {
        impl->cursor = impl->currentText().length();
        event->accept();
    } else {
        switch (event->key()) {
        case Qt::Key_Backspace:
            if (!impl->currentText().isEmpty()) {
                impl->currentText().remove(--impl->cursor, 1);
            } else if (impl->editing_index > 0) {
                impl->setEditingIndex(impl->editing_index - 1);
                impl->cursor = impl->currentText().size();
            }
            event->accept();
            break;
        case Qt::Key_Space:
            if (!impl->currentText().isEmpty()) {
                impl->tags.insert(impl->tags.begin() + std::ptrdiff_t(impl->editing_index + 1), Tag());
                impl->setEditingIndex(impl->editing_index + 1);
                impl->cursor = 0;
            }
            event->accept();
            break;
        default:
            unknown = true;
        }
    }

    if (unknown && impl->ctrl.isAcceptableInput(event)) {
        impl->currentText().insert(impl->cursor, event->text());
        impl->cursor += event->text().length();
        event->accept();
        unknown = false;
    }

    if (event->isAccepted()) {
        // update content
        impl->updateDisplayText();
        impl->calcRects();
        impl->updateCursorBlinking();

        // complete
        impl->completer->setCompletionPrefix(impl->currentText());
        impl->completer->complete();

        update();
    }

    if (unknown) {
        event->ignore();
    }
}


void Tags::completion(std::vector<QString> const& completions) {
    impl->completer = std::make_unique<QCompleter>(
        [&] {
            QStringList ret;
            std::copy(completions.begin(),
                      completions.end(), std::back_inserter(ret));
            return ret;
    }());
    impl->setupCompleter();
}


void Tags::tags(std::vector<QString> const& tags) {
    std::vector<Tag> t{Tag()};
    std::transform(tags.begin(), tags.end(), std::back_inserter(t),
                   [](QString const& text) {
        return Tag{text, QRect()};
    });
    impl->tags = std::move(t);
    impl->editing_index = 0;
    impl->cursor = 0;

    impl->appendTag();
    impl->updateDisplayText();
    impl->calcRects();

    update();
}


std::vector<QString> Tags::tags() const {
    std::vector<QString> ret;
    std::transform(EmptySkipIterator(impl->tags.begin(), impl->tags.end()),
                   EmptySkipIterator(impl->tags.end()),
                   std::back_inserter(ret),
                   [](Tag const& tag) {
        return tag.text;
    });
    return ret;
}
