// Copyright (C) 2015 Kohei Takahashi
// Distributed under the Boost Software License, Version 1.0
// (see accompanying file LICENSE_1_0.txt or a copy at
// http://www.boost.org/LICENSE_1_0.txt)
// Home at http://www.boost.org/libs/functional/overloaded_function

#ifndef BOOST_FUNCTIONAL_MULTIMETHOD_HPP_
#define BOOST_FUNCTIONAL_MULTIMETHOD_HPP_

#include <utility>
#include <type_traits>
#include <typeinfo>
#include <stdexcept>
#include <functional>
#include <tuple>
#include <vector>
#include <iterator>
#include <algorithm>

namespace boost {

namespace mm_detail {

template <typename ...T>
using type_tuple = std::tuple<T...>*;

template <typename FT> struct arg_tuple;
template <typename R, typename ...T>
struct arg_tuple<R(T...)>
{
    static constexpr type_tuple<T...> value = nullptr;
};

} // namespace boost::mm_detail

namespace functional {

template <typename FT> struct strict;

template <typename R, typename ...T>
struct strict<R(T...)>
{
    std::tuple<decltype(typeid(T))...> _ti_tuple;

    template <typename ...Tm>
    explicit
    strict(mm_detail::type_tuple<Tm...>) noexcept
      : _ti_tuple(typeid(Tm)...)
    {
    }

    template <typename ...A>
    bool operator()(A&&... a) const noexcept
    {
        return _ti_tuple == std::tie(typeid(a)...);
    }

    template <typename FT, typename F>
    struct bind;

    template <typename Rm, typename ...Tm, typename F>
    struct bind<Rm(Tm...), F>
    {
        static_assert(std::is_convertible<Rm, R>::value, "");

        F _raw_func;

        R operator()(T&&... a) const
        {
            return _raw_func(static_cast<Tm&&>(a)...);
        }
    };
};

} // namespace boost::functional

template <typename FT, typename Policy = functional::strict<FT>>
class multimethod;

template <typename R, typename ...T, typename Policy>
class multimethod<R(T...), Policy>
{
    std::vector<std::tuple<Policy, std::function<R(T...)>>> methods;

public:
    R operator()(T&&... a) const
    {
        const auto filter = [&a...](typename decltype(methods)::const_reference m)
        {
            return std::get<0>(m)(a...);
        };
        const auto itr = std::find_if(std::begin(methods), std::end(methods), filter);
        if (itr == std::end(methods))
        {
            throw std::runtime_error("No candidates found");
        }
        return std::get<1>(*itr)(std::forward<T>(a)...);
    }

    template <typename FT, typename Functor>
    void add_rule(Functor&& f)
    {
        using bind = typename Policy::template bind<FT, Functor>;
        methods.emplace_back(
            Policy{mm_detail::arg_tuple<FT>::value},
            bind{std::forward<Functor>(f)}
        );
    }
};

} // namespace boost

#endif // BOOST_FUNCTIONAL_MULTIMETHOD_HPP_

