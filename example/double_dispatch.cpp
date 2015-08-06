#include <boost/functional/multimethod.hpp>

#include <iostream>

struct Base
{
    virtual ~Base() noexcept = default;
};

struct Derived1 : Base { };

struct Derived2 : Base { };

void call_12(Derived1 const&, Derived2 const&)
{
    std::cout << __func__ << std::endl;
}

void call_21(Derived2 const&, Derived1 const&)
{
    std::cout << __func__ << std::endl;
}

int main()
{
    boost::multimethod<void(Base&, Base&)> mm;
    mm.add_rule<void(Derived1&, Derived2&)>(call_12);
    mm.add_rule<void(Derived2&, Derived1&)>(call_21);

    Derived1 d1;
    Derived2 d2;

    mm(static_cast<Base&>(d1), static_cast<Base&>(d2));
    mm(static_cast<Base&>(d2), static_cast<Base&>(d1));
}

