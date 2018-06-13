/*   Support Vector Machine Library Wrappers
 *   Copyright (C) 2018  Jonas Greitemann
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

#pragma once

#include "dataset.hpp"
#include "problem.hpp"
#include "parameters.hpp"
#include "serializer.hpp"
#include "svm.h"

#include <array>
#include <cmath>
#include <stdexcept>
#include <type_traits>
#include <utility>


namespace svm {

    template <class Kernel, class Label = double>
    class model {
    public:
        typedef problem<Kernel, Label> problem_t;
        typedef parameters<Kernel> parameters_t;
        typedef typename problem_t::input_container_type input_container_type;
        typedef Label label_type;

        struct classifier_type {

            struct const_iterator {
                using support_vec_type = std::conditional_t<problem_t::is_precomputed,
                                                            input_container_type,
                                                            data_view>;
                using support_vec_ref = std::conditional_t<problem_t::is_precomputed,
                                                           input_container_type const&,
                                                           data_view>;
                using value_type = std::pair<double, support_vec_type>;
                using reference = std::pair<double const&, support_vec_ref> const;
                using pointer = std::pair<double, support_vec_ref> const *;
                using iterator_category = std::bidirectional_iterator_tag;

                const_iterator & operator++ () {
                    ++yalpha;
                    ++sv;
                    if (yalpha == yalpha_1_end) {
                        yalpha = yalpha_2;
                        sv = sv_2;
                    }
                    return *this;
                }
                const_iterator operator++ (int) {
                    const_iterator old(*this);
                    ++(*this);
                    return old;
                }
                const_iterator & operator-- () {
                    if (yalpha == yalpha_2) {
                        yalpha = yalpha_1_end;
                        sv = sv_1_end;
                    }
                    --yalpha;
                    --sv;
                    return *this;
                }
                const_iterator operator-- (int) {
                    const_iterator old(*this);
                    --(*this);
                    return old;
                }
                double coef () const {
                    return *yalpha;
                }

                template <typename Problem = problem_t,
                        typename = std::enable_if_t<!Problem::is_precomputed>>
                data_view support_vec () const {
                    return data_view(*sv);
                }

                template <typename Problem = problem_t,
                        typename = std::enable_if_t<Problem::is_precomputed>,
                        bool dummy = false>
                input_container_type const& support_vec () const {
                    data_view permutation_index(*sv, 0);
                    return prob[permutation_index.front()-1].first;
                }

                auto operator* () const {
                    return std::make_pair(coef(), support_vec());
                }

                friend bool operator== (const_iterator lhs, const_iterator rhs) {
                    return lhs.yalpha == rhs.yalpha && lhs.sv == rhs.sv;
                }
                friend bool operator!= (const_iterator lhs, const_iterator rhs) {
                    return lhs.yalpha != rhs.yalpha || lhs.sv != rhs.sv;
                }

                friend struct classifier_type;

            private:
                const_iterator (double * yalpha_1, struct svm_node ** sv_1,
                                double * yalpha_1_end, struct svm_node ** sv_1_end,
                                double * yalpha_2, struct svm_node ** sv_2,
                                problem_t const& prob)
                    : yalpha(yalpha_1), sv(sv_1),
                      yalpha_1_end(yalpha_1_end), sv_1_end(sv_1_end),
                      yalpha_2(yalpha_2), sv_2(sv_2), prob(prob) {}
                double * yalpha, * yalpha_1_end, * yalpha_2;
                struct svm_node ** sv, **sv_1_end, **sv_2;
                problem_t const& prob;
            };

            using iterator = const_iterator;

            classifier_type (model const& parent, size_t k1, size_t k2)
                : parent(parent), k1(k1), k2(k2) {
                if (k1 > k2) {
                    std::swap(k1, k2);
                    std::swap(this->k1, this->k2);
                }
                size_t sum = 0;
                for (size_t k = 0; k < model::nr_labels; ++k) {
                    if (k == k1)
                        k1_offset = sum;
                    if (k == k2)
                        k2_offset = sum;
                    sum += parent.m->nSV[k];
                }
            }

            const_iterator begin () const {
                return const_iterator {
                    parent.m->sv_coef[k2-1] + k1_offset,
                    parent.m->SV + k1_offset,
                    parent.m->sv_coef[k2-1] + k1_offset + parent.m->nSV[k1],
                    parent.m->SV + k1_offset + parent.m->nSV[k1],
                    parent.m->sv_coef[k1] + k2_offset,
                    parent.m->SV + k2_offset,
                    parent.prob
                };
            }

            const_iterator end () const {
                return const_iterator {
                    parent.m->sv_coef[k1] + k2_offset + parent.m->nSV[k2],
                    parent.m->SV + k2_offset + parent.m->nSV[k2],
                    parent.m->sv_coef[k2-1] + k1_offset + parent.m->nSV[k1],
                    parent.m->SV + k1_offset + parent.m->nSV[k1],
                    parent.m->sv_coef[k1] + k2_offset,
                    parent.m->SV + k2_offset,
                    parent.prob
                };
            }

        private:
            size_t k1, k2;
            size_t k1_offset, k2_offset;
            model const& parent;
        };

        static const size_t nr_labels = detail::label_traits<Label>::nr_labels;
        static const size_t nr_classifiers = nr_labels * (nr_labels - 1) / 2;

        using decision_type = std::conditional_t<nr_classifiers == 1,
                                                 double,
                                                 std::array<double, nr_classifiers>>;

        using nSV_type = std::conditional_t<nr_labels == 2,
                                            std::pair<size_t, size_t>,
                                            std::array<size_t, nr_labels>>;
        using label_arr_t = std::array<label_type, nr_labels>;
        using classifier_arr_t = std::array<classifier_type, nr_classifiers>;

        model () : prob(0), m(nullptr) {}

        model (problem_t && problem, parameters_t const& parameters)
            : prob(std::move(problem)),
              params_(parameters)
        {
            struct svm_problem svm_prob = prob.generate();
            const char * err = svm_check_parameter(&svm_prob, params_.svm_params_ptr());
            if (err) {
                std::string err_str(err);
                throw std::runtime_error(err_str);
            }
            m = svm_train(&svm_prob, params_.svm_params_ptr());
            if (std::isnan(rho()))
                throw std::runtime_error("SVM returned NaN. Specified nu is infeasible.");
            if (m->nr_class != nr_labels)
                throw std::runtime_error("inconsistent number of label values");
        }

        model (model const&) = delete;
        model & operator= (model const&) = delete;

        model (model && other)
            : prob(std::move(other.prob)),
              params_(other.params_),
              m(other.m)
        {
            other.m = nullptr;
        }

        model & operator= (model && other) {
            prob = std::move(other.prob);
            params_ = std::move(other.params_);
            if (m)
                svm_free_and_destroy_model(&m);
            m = other.m;
            other.m = nullptr;
            return *this;
        }

        ~model () noexcept {
            if (m)
                svm_free_and_destroy_model(&m); // WTF
        }

        label_arr_t labels () const {
            label_arr_t ret;
            std::copy(m->label, m->label + nr_labels, ret.begin());
            return ret;
        }

        classifier_arr_t classifiers () const {
            classifier_arr_t ret;
            auto ls = labels();
            auto it = ret.begin();
            for (size_t k1 = 0; k1 < nr_labels - 1; ++k1) {
                for (size_t k2 = k1 + 1; k2 < nr_labels; ++k2, ++it) {
                    *it = classifier_type(*this, k1, k2);
                }
            }
        }

        classifier_type classifier (Label l1, Label l2) const {
            auto ls = labels();
            size_t k1, k2;
            for (size_t k = 0; k < nr_labels; ++k) {
                if (l1 == ls[k])
                    k1 = k;
                if (l2 == ls[k])
                    k2 = k;
            }
            return classifier_type {*this, k1, k2};
        }

        template <typename Problem = problem_t,
                  typename = std::enable_if_t<!Problem::is_precomputed>>
        std::pair<Label, decision_type> operator() (input_container_type const& xj) {
            decision_type dec;
            Label label(svm_predict_values(m, xj.ptr(), reinterpret_cast<double*>(&dec)));
            return std::make_pair(label, dec);
        }

        template <typename Problem = problem_t,
                  typename = std::enable_if_t<Problem::is_precomputed>,
                  bool dummy = false>
        std::pair<Label, decision_type> operator() (input_container_type const& xj) {
            dataset kernelized = prob.kernelize(xj);
            decision_type dec;
            Label label(svm_predict_values(m, kernelized.ptr(), reinterpret_cast<double*>(&dec)));
            return std::make_pair(label, dec);
        }

        template <typename..., size_t NC = nr_classifiers,
                  typename = std::enable_if_t<NC == 1>>
        typename classifier_type::const_iterator begin () const {
            return classifier_type {*this, 0, 1}.begin();
        }

        template <typename..., size_t NC = nr_classifiers,
                  typename = std::enable_if_t<NC == 1>>
        typename classifier_type::const_iterator end () const {
            return classifier_type {*this, 0, 1}.end();
        }

        double rho () const {
            return m->rho[0];
        }

        nSV_type nSV () const {
            if (nr_labels == 2) {
                return {m->nSV[0], m->nSV[1]};
            } else {
                nSV_type n;
                std::copy(m->nSV, m->nSV + nr_labels, n.begin());
                return n;
            }
        }

        size_t dim () const {
            return prob.dim();
        }

        parameters_t const& params () const {
            return params_;
        }

        template <typename Tag, typename Model>
        friend struct model_serializer;

    private:
        problem_t prob;
        parameters_t params_;
        struct svm_model * m;
    };

}
