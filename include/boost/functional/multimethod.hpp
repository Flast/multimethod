// Copyright (C) 2015 Kohei Takahashi
// Distributed under the Boost Software License, Version 1.0
// (see accompanying file LICENSE_1_0.txt or a copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_FUNCTIONAL_MULTIMETHOD_HPP_
#define BOOST_FUNCTIONAL_MULTIMETHOD_HPP_

#include <utility>
#include <type_traits>
#include <typeinfo>
#include <stdexcept>
#include <memory>
#include <tuple>
#include <vector>
#include <iterator>
#include <algorithm>
#include <initializer_list>

namespace multimethod_detail {

template <typename ...T>
using type_tuple = std::tuple<T...>*;

template <typename T>
using remove_reference_t = typename std::remove_reference<T>::type;

template <typename T>
using remove_pointer_t = typename std::remove_pointer<T>::type;

template <typename T>
using decay_t = typename std::decay<T>::type;

template <typename T, typename U = void>
using enable_if_t = typename std::enable_if<T::value, U>::type;


template <typename T, typename... A>
inline std::unique_ptr<T> make_unique(A&&... a)
{
    return std::unique_ptr<T>{new T{std::forward<A>(a)...}};
}


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
    return is_polymorphic_pointer<T>()   ? polymorphic_pointer :
           is_polymorphic_reference<T>() ? polymorphic_reference :
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
    bool is_convertible_from(U* v) const noexcept
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
    bool is_convertible_from(U&& v) const noexcept
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
    constexpr bool is_convertible_from(U&& v) const noexcept
    {
        return std::is_convertible<U, T>::value;
    }
};


template <typename FT> struct arg_tuple;
template <typename R, typename ...T>
struct arg_tuple<R(T...)>
{
    typedef type_tuple<R, T...> type;
};

template <typename FT>
using arg_tuple_t = typename arg_tuple<FT>::type;

template <typename ...T>
using proxy_tuple = std::tuple<proxy<T>...>;

template <typename ...T>
using policy_f = decay_t<bool(proxy_tuple<T...> const&, proxy_tuple<T...> const&)>;


template <typename FT> class function;

template <typename R, typename ...T>
class function<R(T...)>
{
    struct invokable;

    template <typename Fn, typename Rm, typename ...Tm>
    struct invokable_impl;

    std::unique_ptr<invokable> invoker;

public:
    template <typename Fn, typename Rm, typename ...Tm>
    explicit function(Fn&& fn, type_tuple<Rm, Tm...>)
        : invoker(make_unique<invokable_impl<decay_t<Fn>, Rm, Tm...>>(std::forward<Fn>(fn)))
    {
    }

    bool satisfy(policy_f<T...> policy, proxy_tuple<T...> const& args) const noexcept
    {
        return invoker->satisfy(policy, args);
    }

    R operator()(T&&... a) const
    {
        return (*invoker)(std::forward<T>(a)...);
    }
};


template <typename R, typename ...T>
struct function<R(T...)>::invokable
{
    virtual ~invokable() noexcept = default;

    virtual bool satisfy(policy_f<T...> policy, proxy_tuple<T...> const& args) const noexcept = 0;

    virtual R operator()(T&&... a) const = 0;
};


template <typename R, typename ...T>
template <typename Fn, typename Rm, typename ...Tm>
struct function<R(T...)>::invokable_impl : function<R(T...)>::invokable
{
    static_assert(std::is_same<Fn, decay_t<Fn>>::value, "Fn should be decay-ed type.");
    static_assert(std::is_convertible<Rm, R>::value, "Return type should be convertible.");

    Fn fn;
    proxy_tuple<T...> _ti_tuple;

    explicit invokable_impl(Fn&& fn)
        : fn(std::forward<Fn>(fn)), _ti_tuple(proxy<T>{}...)
    {
    }

    bool satisfy(policy_f<T...> policy, proxy_tuple<T...> const& args) const noexcept override
    {
        return policy(_ti_tuple, args);
    }

    R operator()(T&&... a) const noexcept(noexcept(fn(static_cast<Tm&&>(a)...))) override
    {
        return fn(static_cast<Tm&&>(a)...);
    }
};


namespace policy_detail {

template <typename ...T>
bool strict_policy(proxy_tuple<T...> const& lhs, proxy_tuple<T...> const& rhs) noexcept
{
    return lhs == rhs;
}

} // namespace multimethod_detail::policy_detail

} // namespace multimethod_detail

namespace boost {

namespace functional { namespace policy {

using ::multimethod_detail::policy_detail::strict_policy;

} } // namespace boost::functional::policy

template <typename FT>
class multimethod;

template <typename R, typename ...T>
class multimethod<R(T...)>
{
    using function = ::multimethod_detail::function<R(T...)>;
    std::vector<function> methods;

public:
    R exec(::multimethod_detail::policy_f<T...> policy, T&&... a) const
    {
        ::multimethod_detail::proxy_tuple<T...> pt{::multimethod_detail::proxy<T>(a)...};
        const auto filter = [&](typename decltype(methods)::const_reference m) noexcept
        {
            return m.satisfy(policy, pt);
        };
        const auto itr = std::find_if(std::begin(methods), std::end(methods), filter);
        if (itr == std::end(methods))
        {
            throw std::runtime_error("No candidates found");
        }
        return (*itr)(std::forward<T>(a)...);
    }

    R operator()(T&&... a) const
    {
        return exec(functional::policy::strict_policy, std::forward<T>(a)...);
    }

    template <typename FT, typename Functor>
    void add_rule(Functor&& f)
    {
        methods.emplace_back(
            std::forward<Functor>(f),
            ::multimethod_detail::arg_tuple_t<FT>{}
        );
    }
};

} // namespace boost

#endif // BOOST_FUNCTIONAL_MULTIMETHOD_HPP_

