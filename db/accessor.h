#ifndef _DB_SRV_ACCESSOR_H
#define _DB_SRV_ACCESSOR_H

#include <functional>
#include <map>
#include <memory>
#include <vector>
#include "replace.h"
#include "traits.h"

struct Accessor
{
    virtual ~Accessor() = default;
    virtual bool Render(const std::string& sql, std::string& res) = 0;
    virtual std::vector<std::shared_ptr<Accessor>> MakeSubAccessor(size_t group) = 0;
};

template <typename Source, typename T = typename std::decay<Source>::type, typename PARAM = typename GetValueType<T>::type>
struct BindAccessor : Accessor
{
    using iter = typename T::const_iterator;
    using bind_vect_t = std::vector<std::function<void(std::string&, const PARAM&)>>;

    template <typename... ARGS>
    explicit BindAccessor(std::shared_ptr<Source> data, ARGS&&... args)
        : m_param(data)
    {
        m_param = data;
        Init(data->begin(), data->end());
        SetBind(args...);
    }

    BindAccessor(iter& begin, iter& end, const bind_vect_t& vect)
        : m_begin(begin)
        , m_end(end)
        , m_bind_vect(vect)
    {}

    template <typename P, typename... ARGS>
    void SetBind(const std::string& field, P(PARAM::*ptr), ARGS&&... args)
    {
        SetBind(field, ptr);
        SetBind(args...);
    }

    template <typename P>
    void SetBind(const std::string& field, P(PARAM::*ptr))
    {
        m_bind_vect.template emplace_back([field, ptr](std::string& sql, const PARAM& param) { Replace::SetData(sql, field, param.*ptr); });
    }

    bool Render(const std::string& sql, std::string& res) override
    {
        if (!IsValid())
        {
            return false;
        }
        res = sql;
        for (auto& bind : m_bind_vect)
        {
            bind(res, Value());
        }
        Next();
        return true;
    }

    bool IsValid() { return m_begin != m_end; }

    void Init(iter begin, iter end)
    {
        m_begin = begin;
        m_end = end;
    }

    bool Next()
    {
        if (m_begin == m_end)
        {
            return false;
        }

        m_begin++;
        return m_begin != m_end;
    }

    const PARAM& Value() { return GetValueType<T>::Getter(m_begin); }

    std::vector<std::shared_ptr<Accessor>> MakeSubAccessor(size_t group) override
    {
        if (!IsValid())
        {
            return {std::make_shared<BindAccessor>(m_begin, m_end, m_bind_vect)};
        }

        size_t count = std::distance(m_begin, m_end);
        std::vector<std::shared_ptr<Accessor>> res;
        size_t size = count / group;
        auto begin = m_begin;
        auto end = m_end;

        if (size == 0)
        {
            for (size_t i = 0; i < count; i++)
            {
                auto last_begin = begin;
                std::advance(begin, 1);
                res.template emplace_back(std::make_shared<BindAccessor>(last_begin, begin, m_bind_vect));
            }
        }
        else
        {
            for (size_t i = 0; i < group; i++)
            {
                auto last_begin = begin;
                std::advance(begin, size);
                res.template emplace_back(std::make_shared<BindAccessor>(last_begin, begin, m_bind_vect));
            }
            if (count % group > 0)
            {
                res.template emplace_back(std::make_shared<BindAccessor>(begin, end, m_bind_vect));
            }
        }
        return res;
    }
    std::shared_ptr<Source> m_param;
    std::vector<std::function<void(std::string&, const PARAM&)>> m_bind_vect;
    iter m_begin;
    iter m_end;
};

template <typename Source, typename T = typename std::decay<Source>::type, typename PARAM = typename GetValueType<T>::type>
struct CustomAccessor : Accessor
{
    using iter = typename T::const_iterator;
    using render_t = std::function<void(std::string&, const PARAM&)>;

    explicit CustomAccessor(std::shared_ptr<Source> data, render_t render)
        : m_param(data)
        , m_render(render)
    {
        Init(data->begin(), data->end());
    }

    CustomAccessor(iter& begin, iter& end, render_t render)
        : m_begin(begin)
        , m_end(end)
        , m_render(render)
    {}

    bool Render(const std::string& sql, std::string& res) override
    {
        if (!IsValid() || !m_render)
        {
            return false;
        }
        res = sql;
        m_render(res, Value());
        Next();
        return true;
    }

    bool IsValid() { return m_begin != m_end; }

    void Init(iter begin, iter end)
    {
        m_begin = begin;
        m_end = end;
    }

    bool Next()
    {
        if (m_begin == m_end)
        {
            return false;
        }

        m_begin++;
        return m_begin != m_end;
    }

    const PARAM& Value() { return GetValueType<T>::Getter(m_begin); }

    std::vector<std::shared_ptr<Accessor>> MakeSubAccessor(size_t group) override
    {
        if (!IsValid())
        {
            return {std::make_shared<CustomAccessor>(m_begin, m_end, m_render)};
        }

        size_t count = std::distance(m_begin, m_end);
        std::vector<std::shared_ptr<Accessor>> res;
        size_t size = count / group;
        auto begin = m_begin;
        auto end = m_end;

        if (size == 0)
        {
            for (size_t i = 0; i < count; i++)
            {
                auto last_begin = begin;
                std::advance(begin, 1);
                res.template emplace_back(std::make_shared<CustomAccessor>(last_begin, begin, m_render));
            }
        }
        else
        {
            for (size_t i = 0; i < group; i++)
            {
                auto last_begin = begin;
                std::advance(begin, size);
                res.template emplace_back(std::make_shared<CustomAccessor>(last_begin, begin, m_render));
            }
            if (count % group > 0)
            {
                res.template emplace_back(std::make_shared<CustomAccessor>(begin, end, m_render));
            }
        }
        return res;
    }

    std::shared_ptr<Source> m_param;
    iter m_begin;
    iter m_end;
    render_t m_render;
};

#endif  // _DB_SRV_ACCESSOR_H
