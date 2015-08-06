// Copyright (C) 2015 Kohei Takahashi
// Distributed under the Boost Software License, Version 1.0
// (see accompanying file LICENSE_1_0.txt or a copy at
// http://www.boost.org/LICENSE_1_0.txt)
// Home at http://www.boost.org/libs/functional/overloaded_function

#ifndef BOOST_FUNCTIONAL_MULTIMETHOD_HPP_
#define BOOST_FUNCTIONAL_MULTIMETHOD_HPP_

#include <utility>
#include <memory>
#include <type_traits>
#include <typeinfo>
#include <stdexcept>
#include <functional>
#include <tuple>
#include <vector>
#include <iterator>
#include <algorithm>

namespace boost {

template <typename FT> class multimethod;

template <typename R, typename ...T>
class multimethod<R(T...)>
{
    struct method_base
    {
        std::tuple<decltype(typeid(T)) const&...> ti_tuple;

        explicit
        method_base(decltype(ti_tuple) ti_tuple) noexcept : ti_tuple(ti_tuple) {}

        virtual ~method_base() noexcept = default;

        virtual R operator()(T&&...) const = 0;

        bool is_eligible(decltype(typeid(T)) const&... info) const noexcept
        {
            return ti_tuple == std::tie(info...);
        }
    };

    template <typename> struct method;

    template <typename Rm, typename ...Tm>
    struct method<Rm(Tm...)> : method_base
    {
        static_assert(std::is_convertible<Rm, R>::value, "");

        std::function<Rm(Tm...)> f;

        template <typename Functor>
        method(Functor&& f)
          : method_base(std::tie(typeid(Tm)...)), f(std::forward<Functor>(f)) {}

        R operator()(T&&... a) const override
        {
            return f(static_cast<Tm&&>(a)...);
        }
    };

    std::vector<std::unique_ptr<method_base>> methods;

public:
    R operator()(T&&... a) const
    {
        const auto filter = [&a...](std::unique_ptr<method_base> const& mp)
        {
            return mp->is_eligible(typeid(a)...);
        };
        const auto itr = std::find_if(std::begin(methods), std::end(methods), filter);
        if (itr == std::end(methods))
        {
            throw std::runtime_error("No candidates found");
        }
        return (**itr)(std::forward<T>(a)...);
    }

    template <typename FT, typename Functor>
    void add_rule(Functor&& f)
    {
        methods.push_back(std::unique_ptr<method_base>(new method<FT>{std::forward<Functor>(f)}));
    }
};

} // namespace boost

#endif // BOOST_FUNCTIONAL_MULTIMETHOD_HPP_

