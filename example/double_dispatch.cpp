#include <boost/functional/multimethod.hpp>

#include <iostream>
#include <cstdlib>

struct Base
{
    virtual ~Base() noexcept = default;
};

struct Derived1 : Base { };

struct Derived2 : Base { };

struct SubDerived1 : Derived1 { };

void call_12(Derived1 const&, Derived2 const&, int)
{
    std::cout << __func__ << std::endl;
}

void call_21(Derived2 const&, Derived1 const&, long)
{
    std::cout << __func__ << std::endl;
}

int main()
{
    boost::multimethod<void(Base&, Base&, int)> mm;
    mm.add_rule<void(Derived1&, Derived2&, int)>(call_12);
    mm.add_rule<void(Derived2&, Derived1&, int)>(call_21);

    Derived1 d1;
    Derived2 d2;

    mm(static_cast<Base&>(d1), static_cast<Base&>(d2), 0L);
    mm(static_cast<Base&>(d2), static_cast<Base&>(d1), 0L);

    SubDerived1 sd1;
    try { mm(static_cast<Base&>(sd1), d2, 0); std::abort(); } catch (std::runtime_error&) {}
    try { mm(d2, static_cast<Base&>(sd1), 0); std::abort(); } catch (std::runtime_error&) {}
}

