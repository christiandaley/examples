#include <iostream>
#include <memory>
#include <span>
#include <array>
#include <sstream>
#include <functional>
#include <optional>

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
            return type_list<>{};
        }
    }
};

class Printer
{
    enum class Function
    {
        Print,
        PrintToStream,
        PrintToString,
    };

    template <Function F, typename... Ts>
    struct vtable_func
    {
    private:
        template <typename Derived, typename R, typename... Args>
        static void run_impl(R(Derived::* func)(Args...), Printer* printer, void* argsTuplePtr, void* ret)
        {
            const auto bound = std::bind_front(func, static_cast<Derived*>(printer));

            auto& argsTuple = *static_cast<std::tuple<Args&&...>*>(argsTuplePtr);

            if constexpr (std::is_same_v<R, void>)
            {
                std::apply(bound, std::move(argsTuple));
            }
            else
            {
                *static_cast<std::optional<R>*>(ret) = std::apply(bound, std::move(argsTuple));
            }
        }

    public:
        template <typename Derived>
        static void run(Printer* printer, void* argsTuplePtr, void* ret)
        {
            if constexpr (F == Function::Print)
            {
                run_impl(&Derived::template print<Ts...>, printer, argsTuplePtr, ret);
            }
            else if constexpr (F == Function::PrintToStream)
            {
                run_impl(&Derived::template print_to_stream<Ts...>, printer, argsTuplePtr, ret);
            }
            else if constexpr (F == Function::PrintToString)
            {
                run_impl(&Derived::template print_to_string<Ts...>, printer, argsTuplePtr, ret);
            }
        }
    };

    template <typename Derived, typename... Funcs> 
    static auto create_vtable(type_list<Funcs...>)
    {
        static constinit std::array vtable { Funcs::template run<Derived>... };

        return std::span{ vtable };
    }

    const std::span<void(*)(Printer*, void*, void*)> m_vtable;

protected:
    template <typename Derived>
    Printer(Derived*) :
        m_vtable{ create_vtable<Derived>(stateful_type_list::get<Derived>()) }
    {}

public:
    template <typename... Args, 
              size_t Index = stateful_type_list::try_push<vtable_func<Function::Print, Args...>>()>
    void print(Args&&... args)
    {
        auto argsTuple = std::forward_as_tuple(std::forward<Args>(args)...);

        m_vtable[Index](this, &argsTuple, nullptr);
    }

    template <typename... Args, 
              size_t Index = stateful_type_list::try_push<vtable_func<Function::PrintToStream, Args...>>()>
    void print_to_stream(std::ostream& stream, Args&&... args)
    {
        auto argsTuple = std::forward_as_tuple(stream, std::forward<Args>(args)...);

        m_vtable[Index](this, &argsTuple, nullptr);
    }

    template <typename... Args, 
              size_t Index = stateful_type_list::try_push<vtable_func<Function::PrintToString, Args...>>()>
    std::string print_to_string(Args&&... args)
    {
        auto argsTuple = std::forward_as_tuple(std::forward<Args>(args)...);

        std::optional<std::string> ret;

        m_vtable[Index](this, &argsTuple, &ret);

        return std::move(*ret);
    }
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

    template <typename... Args>
    void print_to_stream(std::ostream& stream, Args&&... args)
    {
        ((stream << args << '\n'), ...);
    }

    template <typename... Args>
    std::string print_to_string(Args&&... args)
    {
        std::stringstream stream;

        ((stream << args << '\n'), ...);

        return stream.str();
    }
};

std::unique_ptr<Printer> make_printer();

int main()
{
    auto p = make_printer();

    double d = 2.5;
    const std::string s = "Hello, world!";

    const auto str = p->print_to_string(5, d, s);

    std::cout << str;

    return 0;
}

std::unique_ptr<Printer> make_printer()
{
    return std::make_unique<PrinterImpl>();
}