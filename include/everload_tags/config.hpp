/*
 * MIT License
 *
 * Copyright (c) 2025 Nicolai Trandafil
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#pragma once

#include <QColor>
#include <QMargins>

namespace everload_tags {

struct StyleConfig {
    /// Padding from the text to the the pill border
    QMargins pill_thickness = {3, 3, 4, 3};

    /// Space between pills
    int pills_h_spacing = 4;

    /// Size of cross side
    qreal tag_cross_size = 5;

    /// Distance between text and the cross
    int tag_cross_spacing = 2;

    QColor color{0, 96, 100, 150};

    /// Rounding of the pill
    qreal rounding_x_radius = 4;

    /// Rounding of the pill
    qreal rounding_y_radius = 4;
};

struct BehaviorConfig {
    /// Maintain only unique tags
    bool unique = true;
};

struct Config {
    StyleConfig style{};
    BehaviorConfig behavior{};
};

} // namespace everload_tags
