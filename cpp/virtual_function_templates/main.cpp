#include <iostream>
#include <type_traits>
#include <memory>
#include <span>
#include <array>

template <typename... Ts>
struct type_list
{
    template <typename... Us>
    constexpr auto operator+(type_list<Us...>) const noexcept
    {
        return type_list<Ts..., Us...>{};
    }

    template <typename... Us>
    constexpr bool operator==(type_list<Us...>) const noexcept
    {
        return std::is_same_v<type_list<Ts...>, type_list<Us...>>;
    }
};

struct stateful_type_list
{
private:
    template <size_t N>
    struct getter
    {
        friend consteval auto flag(getter);
    };

    template <typename T, size_t N>
    struct setter
    {
        friend consteval auto flag(getter<N>)
        {
            return type_list<T>{};
        }

        static constexpr size_t value = N;
    };

public:
    template <typename T, size_t N = 0>
    consteval static size_t try_push()
    {
        if constexpr (requires { flag(getter<N>{}); })
        {
            return try_push<T, N + 1>();
        }
        else
        {
            return setter<T, N>::value;
        }
    }

    template <typename Unique, size_t N = 0, auto = []{}>
    consteval static auto get()
    {
        if constexpr (requires { flag(getter<N>{}); })
        {
            return flag(getter<N>{}) + get<Unique, N + 1>();
        }
        else
        {
            return type_list{};
        }
    }
};

class Printer
{
    template <typename... Args>
    struct vtable_func
    {
        template <typename Derived>
        static void run(Printer* printer, void* argsTuplePtr)
        {
            const auto bound = [&](Args&&... args)
            {
                static_cast<Derived*>(printer)->print(std::forward<Args>(args)...);
            };

            auto& argsTuple = *static_cast<std::tuple<Args&&...>*>(argsTuplePtr);

            std::apply(bound, std::move(argsTuple));
        }
    };

    template <typename Derived, typename... Funcs> 
    static auto create_vtable(type_list<Funcs...>)
    {
        static constinit std::array vtable { Funcs::template run<Derived>... };

        return std::span{ vtable };
    }

    const std::span<void(*)(Printer*, void*)> m_vtable;

protected:
    template <typename Derived>
    Printer(Derived*) :
        m_vtable{ create_vtable<Derived>(stateful_type_list::get<Derived>()) }
    {}

public:
    template <typename... Args, 
              size_t Index = stateful_type_list::try_push<vtable_func<Args...>>()>
    void print(Args&&... args)
    {
        auto argsTuple = std::forward_as_tuple(std::forward<Args>(args)...);

        m_vtable[Index](this, &argsTuple);
    }

    virtual ~Printer() = default;
};

struct PrinterImpl : Printer
{
public:
    PrinterImpl() :
        Printer{ this }
    {}

    template <typename... Args>
    void print(Args&&... args)
    {
        ((std::cout << args << '\n'), ...);
    }
};

std::unique_ptr<Printer> make_printer();

int main()
{
    auto p = make_printer();

    double d = 2.5;
    const std::string s = "Hello, world!";

    p->print(5, d, s); // calls PrinterImpl::print !!!

    return 0;
}

std::unique_ptr<Printer> make_printer()
{
    return std::make_unique<PrinterImpl>();
}