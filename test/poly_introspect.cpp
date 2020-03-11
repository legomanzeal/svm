/*   Support Vector Machine Library Wrappers
 *   Copyright (C) 2018-2019  Jonas Greitemann
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program, see the file entitled "LICENCE" in the
 *   repository's root directory, or see <http://www.gnu.org/licenses/>.
 */

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include "doctest/doctest.h"

#include <array>
#include <iostream>
#include <tuple>
#include <utility>

#include <svm/dataset.hpp>
#include <svm/model.hpp>
#include <svm/parameters.hpp>
#include <svm/problem.hpp>
#include <svm/kernel/polynomial.hpp>


using kernel_t = svm::kernel::polynomial<2>;
using model_t = svm::model<kernel_t>;
using problem_t = svm::problem<kernel_t>;
using param_t = svm::parameters<kernel_t>;
using array_t = std::array<double, 4>;

static const array_t ys = {+1, +1, -1, -1};
static const std::array<array_t,4> xs {
    array_t {  1,  2,  3,  4 },
    array_t { -8, -6, -4, -2 },
    array_t {  4,  3,  2,  1 },
    array_t { -2, -4, -6, -8 }
};

static const model_t model = [] {
    problem_t prob(4);
    auto itX = xs.begin();
    auto itY = ys.begin();
    for (; itX != xs.end(); ++itX, ++itY)
        prob.add_sample(svm::dataset(*itX), *itY);
    param_t params;
    params.gamma() = 1;
    params.coef0() = 0.5;
    return model_t(std::move(prob), params);
} ();

static const array_t ya = [] {
    array_t ya;
    auto it = ya.begin();
    for (auto p : model.classifier()) {
        std::tie(*(it), std::ignore) = p;
        std::cout << *it << std::endl;
        ++it;
    }
    return ya;
} ();

TEST_CASE("polynomial-introspect-scalar") {
    // https://xtensor.readthedocs.io/en/latest/compilers.html
    // original code svm::model<kernel_t,double>::classifier_type
    //auto introspector2 = svm::tensor_introspect <model_t::classifier_type, 0, model_t::kernel_type::Degree>(model.classifier());
    //CHECK(introspector2.tensor() == doctest::Approx(0.25));

     auto introspector = svm::tensor_introspector < model_t::classifier_type, 0, model_t::kernel_type::Degree>(model.classifier());
   
    CHECK(introspector.tensor() == doctest::Approx(0.25));
}

TEST_CASE("polynomial-introspect-vector") {
    //auto introspector = svm::tensor_introspect<1>(model.classifier());
    auto introspector = svm::tensor_introspector < model_t::classifier_type, 1, model_t::kernel_type::Degree>(model.classifier());
    array_t u = {0, 0, 0, 0};
    auto itX = xs.begin();
    auto itYA = ya.begin();
    for (; itX != xs.end(); ++itX, ++itYA) {
        auto itU = u.begin();
        auto itXX = itX->begin();
        for (; itXX != itX->end(); ++itXX, ++itU)
            *itU += *itYA * *itXX;
    }
    for (size_t i = 0; i < 4; ++i)
        CHECK(u[i] == doctest::Approx(introspector.tensor({i})));
}

TEST_CASE("polynomial-introspect-matrix") {
    //auto introspector = svm::tensor_introspect<2>(model.classifier());
    auto introspector = svm::tensor_introspector < model_t::classifier_type, 2, model_t::kernel_type::Degree>(model.classifier());
    std::array<array_t, 4> u {
        array_t {0, 0, 0, 0},
        array_t {0, 0, 0, 0},
        array_t {0, 0, 0, 0},
        array_t {0, 0, 0, 0}
    };
    auto itX = xs.begin();
    auto itYA = ya.begin();
    for (; itX != xs.end(); ++itX, ++itYA) {
        for (size_t i = 0; i < 4; ++i) {
            for (size_t j = 0; j < 4; ++j) {
                u[i][j] += *itYA * (*itX)[i] * (*itX)[j];
            }
        }
    }
    for (size_t i = 0; i < 4; ++i)
        for (size_t j = 0; j < 4; ++j)
            CHECK(u[i][j] == doctest::Approx(introspector.tensor({i, j})));
}
