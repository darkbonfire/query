#ifndef _DB_SRV_TRAITS_H
#define _DB_SRV_TRAITS_H

template <typename T>
struct GetValueType
{
};

template <template <typename, typename> class T, typename DATA, typename ALLOC>
struct GetValueType<T<DATA, ALLOC>>
{
    using type = DATA;
    static const DATA& Getter(typename T<DATA, ALLOC>::const_iterator& iter) { return *iter; }
};

template <template <typename, typename, typename> class T, typename DATA, typename COMP, typename ALLOC>
struct GetValueType<T<DATA, COMP, ALLOC>>
{
    using type = DATA;
    static const DATA& Getter(typename T<DATA, COMP, ALLOC>::const_iterator& iter) { return *iter; }
};

template <template <typename, typename, typename, typename> class T, typename KEY, typename DATA, typename COMP, typename ALLOC>
struct GetValueType<T<KEY, DATA, COMP, ALLOC>>
{
    using type = DATA;
    static const DATA& Getter(typename T<KEY, DATA, COMP, ALLOC>::const_iterator& iter) { return iter->second; }
};

template <typename T>
struct LambdaToFunction
{
    using type = void;
};

template <typename Ret, typename Class, typename... Args>
struct LambdaToFunction<Ret (Class::*)(Args...) const>
{
    using type = std::function<Ret(Args...)>;
};

struct Lambda
{
    template <typename F>
    static typename LambdaToFunction<decltype(&F::operator())>::type LTF(F const& func)
    {
        return func;
    }
};

#endif  // SEARCH_SRV_TRAITS_H
