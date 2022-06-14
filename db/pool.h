#ifndef _DB_POOL_H
#define _DB_POOL_H

#include <mysql/mysql.h>
#include <atomic>
#include <condition_variable>
#include <map>
#include <mutex>
#include <queue>
#include <thread>
#include <utility>

struct DBConfig
{
    std::string m_conf_name;
    std::string m_user;
    std::string m_password;
    std::string m_db;
    std::string m_host;
    int32_t m_port = 0;

    bool Equal(const DBConfig& config) const
    {
        return m_conf_name == config.m_conf_name && m_user == config.m_user
               && m_password == config.m_password
               && m_db == config.m_db
               && m_host == config.m_host
               && m_port == config.m_port;
    }
};

struct SQLConnect
{
    void SetConnect(MYSQL* con, std::shared_ptr<DBConfig>& config)
    {
        if (!m_con)
        {
            mysql_close(m_con);
        }

        m_con = con;
        m_is_connect = true;
        m_reconnect_count = 0;
        m_config = config;
    }

    void TestConnect()
    {
        m_is_connect = mysql_ping(m_con) == 0;
        if (!m_is_connect && !m_reconnect(m_con))
        {
            m_reconnect_count++;
            return;
        }
        m_reconnect_count = 0;
    }

    bool IsVaild() const
    {
        return m_config.lock() != nullptr;
    }

    ~SQLConnect()
    {
        mysql_close(m_con);
    }

    bool m_is_connect = false;
    int32_t m_reconnect_count = 0;
    MYSQL* m_con = nullptr;
    std::function<bool(MYSQL*)> m_reconnect;
    std::weak_ptr<DBConfig> m_config;
};


struct DBRequest
{
    DBRequest() = default;

    DBRequest(std::shared_ptr<DBConfig> db, std::function<void(MYSQL*)>  func, bool reconnect = false)
        : m_db(std::move(db))
        , m_func(std::move(func))
        , m_reconnect(reconnect)
    {
    }

    std::shared_ptr<DBConfig> m_db;
    std::function<void(MYSQL*)> m_func;
    bool m_reconnect = false;
};

class DBPool
{
public:
    explicit DBPool(int32_t parallel = 1)
        : m_exit(false)
        , m_queue_size(0)
    {
        for (int32_t i = 0; i < parallel; i++)
        {
            m_db_thread.emplace_back(std::bind(&DBPool::Thread, this));
        }
    }

    bool Add(const std::function<void(MYSQL*)>& exec, const DBConfig& config)
    {
        static thread_local std::map<std::string, std::shared_ptr<DBConfig>> config_table;
        auto iter = config_table.find(config.m_conf_name);
        if (iter == config_table.end())
        {
            // new config
            iter = config_table.emplace(config.m_conf_name, std::make_shared<DBConfig>(config)).first;
        }
        else if (!iter->second->Equal(config))
        {
            // change the config
            iter->second = std::make_shared<DBConfig>(config);
        }

        std::lock_guard<std::mutex> lk(m_queue_mut);
        m_request_queue.emplace(iter->second, exec);
        m_queue_size++;
        m_cond.notify_all();
        return true;
    }

    ~DBPool()
    {
        while (m_queue_size > 0)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(64));
        }

        m_exit = true;
        m_cond.notify_all();
        for (auto& t : m_db_thread)
        {
            if (t.joinable())
            {
                t.join();
            }
        }
        m_db_thread.clear();
    }

private:
    void SetOptions(MYSQL* con)
    {
        mysql_options(con, MYSQL_SET_CHARSET_NAME, "utf8");
        mysql_options(con, MYSQL_INIT_COMMAND, "SET NAMES utf8");
        mysql_options(con, MYSQL_OPT_CONNECT_TIMEOUT, &m_connect_timeout);
        mysql_options(con, MYSQL_OPT_RECONNECT, &m_reconnect);
        mysql_options(con, MYSQL_OPT_COMPRESS, nullptr);
    }

    void Thread()
    {

        std::map<std::string, SQLConnect> db_table;
        MYSQL* con = nullptr;
        DBRequest req;
        while (!m_exit)
        {
            {
                std::unique_lock<std::mutex> lk(m_queue_mut);
                m_cond.wait_for(lk, std::chrono::seconds(2), [&] { return m_exit || !m_request_queue.empty(); });
                if (m_exit)
                {
                    break;
                }

                if (m_request_queue.empty())
                {
                    lk.unlock();
                    for (auto iter = db_table.begin(); iter != db_table.end(); iter++)
                    {
                        if (!iter->second.IsVaild())
                        {
                            iter = db_table.erase(iter);
                            continue;
                        }
                        iter->second.TestConnect();
                    }
                    for (auto& db : db_table)
                    {
                        db.second.TestConnect();
                    }
                    continue;
                }

                req = m_request_queue.front();
                m_request_queue.pop();
                m_queue_size--;
            }

            auto& db = req.m_db;
            auto db_iter = db_table.find(db->m_conf_name);
            if (db_iter == db_table.end())
            {
                if (!(con = Connect(db.get())))
                {
                    continue;
                }
                db_table[db->m_conf_name].SetConnect(con, db);
            }
            else
            {
                con = db_iter->second.m_con;
            }
            req.m_func(con);
        }
    }

    MYSQL* Connect(const DBConfig* config)
    {
        MYSQL* con = mysql_init(nullptr);
        if (!con)
        {
            return nullptr;
        }

        SetOptions(con);
        if (!mysql_real_connect(con, config->m_host.c_str(), config->m_user.c_str(), config->m_password.c_str(), config->m_db.c_str(), config->m_port, nullptr, 0))
        {
            mysql_close(con);
            return nullptr;
        }
        return con;
    }

    std::atomic<bool> m_exit;
    std::condition_variable m_cond;
    std::queue<DBRequest> m_request_queue;

    int32_t m_connect_timeout = 8;
    int32_t m_reconnect = 1;

    std::mutex m_queue_mut;
    std::vector<std::thread> m_db_thread;
    std::atomic_size_t m_queue_size;
};

#endif  // DB_TEST_POOL_H
