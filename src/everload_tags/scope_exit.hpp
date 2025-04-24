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

#include <QDebug>

#include <exception>
#include <utility>

namespace everload_tags {

template <class Fn>
struct ScopeExit : Fn {
    ~ScopeExit() {
        try {
            (*this)();
        } catch (...) {
            qDebug() << "exception durring scope exit";
        }
    }
};

struct MakeScopeExit {
    template <class Fn>
    auto operator->*(Fn&& fn) const {
        return ScopeExit<Fn>{std::forward<Fn>(fn)};
    }
};

template <class Fn>
struct ScopeFail : Fn {
    ~ScopeFail() {
        if (std::current_exception()) {
            try {
                (*this)();
            } catch (...) {
                qDebug() << "exception during scope fail";
            }
        }
    }
};

struct MakeScopeFail {
    template <class Fn>
    auto operator->*(Fn&& fn) const {
        return ScopeFail<Fn>{std::forward<Fn>(fn)};
    }
};

} // namespace everload_tags

#define EVERLOAD_TAGS_CONCATENATE_IMPL(s1, s2) s1##s2

#define EVERLOAD_TAGS_CONCATENATE(s1, s2) EVERLOAD_TAGS_CONCATENATE_IMPL(s1, s2)

#define EVERLOAD_TAGS_UNIQUE_IDENTIFIER EVERLOAD_TAGS_CONCATENATE(UNIQUE_IDENTIFIER_, __LINE__)

#define EVERLOAD_TAGS_SCOPE_EXIT auto const EVERLOAD_TAGS_UNIQUE_IDENTIFIER = everload_tags::MakeScopeExit{}->*[&]

#define EVERLOAD_TAGS_SCOPE_FAIL auto const EVERLOAD_TAGS_UNIQUE_IDENTIFIER = everload_tags::MakeScopeFail{}->*[&]
