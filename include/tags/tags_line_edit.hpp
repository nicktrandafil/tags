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

#pragma once

#include <QWidget>

#include <memory>
#include <vector>

namespace yenxo_widgets {

/// Tag line editor widget
/// `Space` commits a tag and initiates a new tag edition
class TagsLineEdit : public QWidget {
    Q_OBJECT

public:
    /// \param unique Ensure tags uniqueness
    explicit TagsLineEdit(bool unique = true, QWidget* parent = nullptr);
    ~TagsLineEdit() override;

    // QWidget
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

    /// Set completions
    void completion(std::vector<QString> const& completions);

    /// Set tags
    void tags(std::vector<QString> const& tags);

    /// Get tags
    std::vector<QString> tags() const;

signals:
    void tagsEdited();

protected:
    // QWidget
    void paintEvent(QPaintEvent* event) override;
    void timerEvent(QTimerEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void focusInEvent(QFocusEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

} // namespace yenxo_widgets
