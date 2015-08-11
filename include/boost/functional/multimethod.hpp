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

namespace multimethod_detail {

template <typename ...T>
using type_tuple = std::tuple<T...>*;

template <typename FT> struct arg_tuple;
template <typename R, typename ...T>
struct arg_tuple<R(T...)>
{
    static constexpr type_tuple<T...> value = nullptr;
};


template <typename T>
using remove_reference_t = typename std::remove_reference<T>::type;

template <typename T>
using remove_pointer_t = typename std::remove_pointer<T>::type;

template <typename T, typename U = void>
using enable_if_t = typename std::enable_if<T::value, U>::type;


template <typename T>
inline constexpr bool is_polymorphic_pointer() noexcept
{
    return std::is_pointer<T>::value && std::is_polymorphic<remove_pointer_t<T>>::value;
}

template <typename T>
inline constexpr bool is_polymorphic_reference() noexcept
{
    return std::is_reference<T>::value && std::is_polymorphic<remove_reference_t<T>>::value;
}

enum proxy_type
{
    polymorphic_pointer,
    polymorphic_reference,
    non_polymorphic,
};

template <typename T>
inline constexpr proxy_type type() noexcept
{
    return is_polymorphic_pointer<T>()    ? polymorphic_pointer :
           is_polymorphic_reference<T>()  ? polymorphic_reference :
                                            non_polymorphic;
}

template <typename T, proxy_type = type<T>()>
struct proxy;

template <typename T>
struct proxy<T, polymorphic_pointer>
{
    std::type_info const& ti;

    template <typename U> using rebind = proxy<U, polymorphic_pointer>;

    proxy() noexcept : ti(typeid(T)) {}
    explicit proxy(T v) noexcept : ti(typeid(v)) {}
    template <typename U>
    proxy(rebind<U> const& other) noexcept : ti(other.ti) {}

    template <typename U>
    friend bool operator==(proxy const& lhs, rebind<U> const& rhs) noexcept
    {
        return lhs.ti == rhs.ti;
    }

    template <typename U>
    bool is_convertible(U* v) const noexcept
    {
        return dynamic_cast<T>(v) != nullptr;
    }
};

template <typename T>
struct proxy<T, polymorphic_reference>
{
    std::type_info const& ti;

    template <typename U> using rebind = proxy<U, polymorphic_reference>;

    proxy() noexcept : ti(typeid(T)) {}
    explicit proxy(T v) noexcept : ti(typeid(v)) {}
    template <typename U>
    proxy(rebind<U> const& other) noexcept : ti(other.ti) {}

    template <typename U>
    friend bool operator==(proxy const& lhs, rebind<U> const& rhs) noexcept
    {
        return lhs.ti == rhs.ti;
    }

    template <typename U>
    bool is_convertible(U&& v) const noexcept
    {
        return dynamic_cast<remove_reference_t<T>*>(&v) != nullptr;
    }
};

template <typename T>
struct proxy<T, non_polymorphic>
{
    template <typename U> using rebind = proxy<U, non_polymorphic>;

    constexpr proxy() noexcept = default;
    explicit constexpr proxy(T const& v) noexcept {}

    template <typename U>
    friend constexpr bool operator==(proxy const&, rebind<U> const&) noexcept
    {
        return std::is_convertible<U, T>::value;
    }

    template <typename U>
    constexpr bool is_convertible(U&& v) const noexcept
    {
        return std::is_convertible<U, T>::value;
    }
};

} // namespace multimethod_detail

namespace boost {

namespace functional {

template <typename FT> struct strict;

template <typename R, typename ...T>
struct strict<R(T...)>
{
    std::tuple<::multimethod_detail::proxy<T>...> _ti_tuple;

    template <typename ...Tm>
    explicit
    strict(::multimethod_detail::type_tuple<Tm...>) noexcept
        : _ti_tuple(::multimethod_detail::proxy<Tm>{}...)
    {
    }

    template <typename ...A>
    bool operator()(A&&... a) const noexcept
    {
        return _ti_tuple == std::forward_as_tuple(::multimethod_detail::proxy<A>{a}...);
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
            Policy{::multimethod_detail::arg_tuple<FT>::value},
            bind{std::forward<Functor>(f)}
        );
    }
};

} // namespace boost

#endif // BOOST_FUNCTIONAL_MULTIMETHOD_HPP_

