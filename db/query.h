#ifndef DB_QUERY_H
#define DB_QUERY_H

#include <functional>
#include <sstream>

#include <unistd.h>
#include <cassert>
#include "accessor.h"
#include "adapter.h"
#include "pool.h"
#include "replace.h"
#include "row.h"
#include "data_queue.h"
#include "event.h"
#include <sys/eventfd.h>

struct QueryContext
{
    size_t m_bind_type_code = 0;
    size_t m_obj_type_code = 0;
    void* m_obj = nullptr;
    void* m_store = nullptr;
    Row m_row;
    std::function<void(void*)> m_clear;
    std::vector<std::function<void(const char*, void*)>> m_bind_vect;
};

template<typename T>
struct QueryAsyncContext
{
    explicit QueryAsyncContext()
    {
        m_fd = eventfd(0, EFD_CLOEXEC | EFD_SEMAPHORE);
        assert(m_fd != -1);
    }

    ~QueryAsyncContext()
    {
        if (m_fd != -1)
        {
            close(m_fd);
        }
    }

    static void Callback(evutil_socket_t fd, short ev, void *ptr)
    {
        if (fd != -1)
        {
            int64_t data;
            read(fd, &data, sizeof(data));
        }

        auto ctx = static_cast<QueryAsyncContext*>(ptr);
        std::shared_ptr<T> data;
        while(ctx->m_queue.Pop(data, false))
        {
            if (data && ctx->m_handler)
            {
                ctx->m_handler(*data);
            }
        }

        if (ctx->m_queue.IsEmpty())
        {
            delete ctx;
        }
    }

    int32_t m_fd = -1;
    std::function<void(T&)> m_handler;
    DataQueue<std::shared_ptr<T>> m_queue;
};

struct Query
{
    Query() = default;

    Query& Init(const std::string& sql)
    {
        m_sql = sql;
        return *this;
    }

    template <typename OBJ, typename DATA, typename... ARGS>
    Query& Init(const std::string& sql, DATA(OBJ::*ptr), const std::string& field, ARGS&&... args)
    {
        m_ctx.m_bind_type_code = typeid(OBJ).hash_code();
        m_sql = sql;
        std::string query_list;
        Add(query_list, ptr, field, args...);
        Replace::SetData(m_sql, query_list);
        return *this;
    }

    template <typename PARAM>
    Query& With(std::shared_ptr<PARAM> param, typename CustomAccessor<PARAM>::render_t render)
    {
        m_accessor = std::make_shared<CustomAccessor<PARAM>>(param, render);
        return *this;
    }

    template <typename PARAM, typename OBJ, typename DATA, typename... ARGS>
    Query& WithParam(std::shared_ptr<PARAM> param, const std::string& filed, DATA(OBJ::*ptr), ARGS&&... args)
    {
        m_accessor = std::make_shared<BindAccessor<PARAM>>(param, filed, ptr, args...);
        return *this;
    }

    template <typename T>
    Query& Store(T func)
    {
        Store(Lambda::LTF(func));
        return *this;
    }

    template <typename STORE, typename OBJ>
    Query& Store(std::function<void(STORE& store, OBJ*, Row& row)> func)
    {
        m_ctx.m_obj_type_code = typeid(OBJ).hash_code();
        assert(m_ctx.m_bind_type_code == m_ctx.m_obj_type_code && "bind type != store type");
        m_create = [](QueryContext& ctx) {
            ctx.m_store = new STORE();
            ctx.m_obj = new OBJ();
        };

        m_delete = [](QueryContext& ctx) {
            delete static_cast<OBJ*>(ctx.m_obj);
            delete static_cast<STORE*>(ctx.m_store);
        };

        m_ctx.m_clear = [](void* obj) { static_cast<OBJ*>(obj)->Clear(); };

        m_fetch = [func](QueryContext& ctx, std::vector<std::string>& field_vect, MYSQL_ROW row) {
            if (!ctx.m_store && !ctx.m_obj)
            {
                return;
            }
            ctx.m_row.m_field_table.clear();
            ctx.m_clear(ctx.m_obj);
            for (size_t i = 0; i < field_vect.size(); i++)
            {
                if (i < ctx.m_bind_vect.size() && ctx.m_bind_vect[i])
                {
                    ctx.m_bind_vect[i](row[i], ctx.m_obj);
                }
                else
                {
                    ctx.m_row.m_field_table[field_vect[i]] = row[i];
                }
            }
            func(*static_cast<STORE*>(ctx.m_store), static_cast<OBJ*>(ctx.m_obj), ctx.m_row);
        };
        return *this;
    }

    template <typename STORE>
    Query& Store(std::function<void(STORE& store, Row& row)> func)
    {
        m_create = [](QueryContext& ctx) { ctx.m_store = new STORE(); };

        m_delete = [](QueryContext& ctx) { delete static_cast<STORE*>(ctx.m_store); };

        m_fetch = [func](QueryContext& ctx, std::vector<std::string>& field_vect, MYSQL_ROW row) {
            if (ctx.m_store)
            {
                return;
            }
            for (size_t i = 0; i < field_vect.size(); i++)
            {
                ctx.m_row.m_field_table[field_vect[i]] = row[i];
            }
            func(*static_cast<STORE*>(ctx.m_store), ctx.m_row);
        };
        return *this;
    }

    template <typename T, typename DATA>
    void Add(std::string& query_list, DATA(T::*ptr), const std::string& field)
    {
        if (!query_list.empty())
            query_list.append(",");
        query_list.append(field);

        m_ctx.m_bind_vect.emplace_back([ptr](const char* data_ptr, void* obj) {
            if (!obj)
            {
                return;
            }
            auto* data = static_cast<T*>(obj);
            Convert::ToData(data_ptr, data->*ptr);
        });
    }

    template <typename T>
    void Add(std::string& query_list, std::string(T::*ptr), const std::string& field)
    {
        if (!query_list.empty())
            query_list.append(",");
        query_list.append(field);

        m_ctx.m_bind_vect.emplace_back([ptr](const char* data_ptr, void* obj) {
            if (!obj)
            {
                return;
            }
            auto* data = static_cast<T*>(obj);
            data->*ptr = data_ptr;
        });
    }

    template <typename T, typename DATA, typename... ARGS>
    void Add(std::string& query_list, DATA(T::*ptr), const std::string& field, ARGS&&... args)
    {
        Add(query_list, ptr, field);
        Add(query_list, args...);
    }

    void DoQuery(MYSQL* con)
    {
        std::string sql;
        if (m_accessor)
        {
            sql = m_sql;
            std::string error;
            MYSQL_ROW row;
            MYSQL_RES* mysql_res = nullptr;

            m_delete(m_ctx);
            m_create(m_ctx);
            std::vector<std::string> field_vect;
            while (m_accessor->Render(m_sql, sql))
            {
                if (mysql_res)
                {
                    mysql_free_result(mysql_res);
                    mysql_res = nullptr;
                }

                if (mysql_query(con, sql.c_str()) != 0)
                {
                    error = mysql_error(con);
                    Log::Warn("%s", error.c_str());
                    continue;
                }

                mysql_res = mysql_store_result(con);
                if (!mysql_res)
                {
                    error = mysql_error(con);
                    Log::Warn("%s", error.c_str());
                    continue;
                }

                if (field_vect.empty())
                {
                    MYSQL_FIELD* field;
                    while ((field = mysql_fetch_field(mysql_res)))
                    {
                        field_vect.emplace_back(field->name);
                    }
                }

                while ((row = mysql_fetch_row(mysql_res)))
                {
                    m_fetch(m_ctx, field_vect, row);
                }
            }
        }
        else
        {
            std::string error;
            MYSQL_ROW row;
            MYSQL_RES* mysql_res = nullptr;

            m_delete(m_ctx);
            m_create(m_ctx);
            std::vector<std::string> field_vect;
            if (mysql_query(con, m_sql.c_str()) != 0)
            {
                error = mysql_error(con);
                Log::Warn("%s", error.c_str());
                return;
            }

            mysql_res = mysql_store_result(con);
            if (!mysql_res)
            {
                error = mysql_error(con);
                Log::Warn("%s", error.c_str());
                return;
            }

            if (field_vect.empty())
            {
                MYSQL_FIELD* field;
                while ((field = mysql_fetch_field(mysql_res)))
                {
                    field_vect.emplace_back(field->name);
                }
            }

            while ((row = mysql_fetch_row(mysql_res)))
            {
                m_fetch(m_ctx, field_vect, row);
            }
            mysql_free_result(mysql_res);
        }
    }

    template <typename Ret>
    void Run(int32_t parallel, DBPool& pool, const DBConfig& config, DataQueue<std::shared_ptr<Ret>>& data_queue)
    {
        if (m_accessor)
        {
            auto accessor_vect = m_accessor->MakeSubAccessor(parallel);
            std::vector<std::shared_ptr<Query>> query_vect;
            std::vector<std::function<void(MYSQL*)>> func_vect;
            for (auto& accessor : accessor_vect)
            {
                auto sub_query = std::make_shared<Query>(*this);
                sub_query->m_accessor = accessor;
                query_vect.emplace_back(sub_query);
                func_vect.emplace_back(std::bind(&Query::DoQuery, sub_query.get(), std::placeholders::_1));
            }
            
            for (auto& func : func_vect)
            {
                pool.Add(func, config);
            }
        }
        else
        {
            auto query = std::make_shared<Query>(*this);
            data_queue.SetMax(1);
            auto func = [query, &data_queue](MYSQL* con) {
                query->DoQuery(con);
                data_queue.Push(std::shared_ptr<Ret>(static_cast<Ret*>(query->m_ctx.m_store)));
                query->m_ctx.m_store = nullptr;
                query->m_delete(query->m_ctx);
            };
            pool.Add(func, config);
        }
    }

    template <typename Ret>
    void Run(int32_t parallel, DBPool& pool, const DBConfig& config, event_base* ebase, std::function<void(Ret&)> handler)
    {
        if (m_accessor)
        {
            auto accessor_vect = m_accessor->MakeSubAccessor(parallel);
            std::vector<std::shared_ptr<Query>> query_vect;
            std::vector<std::function<void(MYSQL*)>> func_vect;
            for (auto& accessor : accessor_vect)
            {
                auto sub_query = std::make_shared<Query>(*this);
                sub_query->m_accessor = accessor;
                query_vect.emplace_back(sub_query);
                func_vect.emplace_back(std::bind(&Query::DoQuery, sub_query.get(), std::placeholders::_1));
            }

            for (auto& func : func_vect)
            {
                pool.Add(func, config);
            }
        }
        else
        {
            auto* async_ctx = new QueryAsyncContext<Ret>();
            async_ctx->m_handler = handler;

            auto query = std::make_shared<Query>(*this);
            auto func = [query, async_ctx](MYSQL* con) {
                query->DoQuery(con);
                async_ctx->m_queue.Push(std::shared_ptr<Ret>(static_cast<Ret*>(query->m_ctx.m_store)));
                query->m_ctx.m_store = nullptr;
                query->m_delete(query->m_ctx);
                int64_t data = 1;
                write(async_ctx->m_fd, &data, sizeof(data));
            };

            event_base_once(ebase, async_ctx->m_fd, EV_READ, &QueryAsyncContext<Ret>::Callback, async_ctx, nullptr);
            pool.Add(func, config);
        }
    }

    std::string m_sql;
    QueryContext m_ctx;
    std::shared_ptr<Accessor> m_accessor;
    std::function<void(QueryContext&, std::vector<std::string>&, MYSQL_ROW)> m_fetch;
    std::function<void(QueryContext&)> m_create;
    std::function<void(QueryContext&)> m_delete;
};

#endif  // DB_QUERY_H
