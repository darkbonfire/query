#ifndef DB_TEST_REPLACE_H
#define DB_TEST_REPLACE_H

#include <string>

struct Replace
{
    template <typename DATA>
    static void SetData(std::string& tmp, const DATA& data)
    {
        SetDataImpl(tmp, tmp, "", data);
    }

    template <typename DATA>
    static void SetData(std::string& tmp, const std::string& id, const DATA& data)
    {
        SetDataImpl(tmp, tmp, id, data);
    }

    template <typename DATA, typename... ARGS>
    static void SetData(std::string& tmp, const std::string& id, const DATA& data, const ARGS&... args)
    {
        SetDataImpl(tmp, tmp, id, data);
        SetData(tmp, args...);
    }

    static void GetData(std::string& str_data, const std::string& data) { str_data = data; }

    static void GetData(std::string& str_data, const char* data) { str_data = data; }

    template <typename DATA>
    static void GetData(std::string& str_data, const DATA& data)
    {
        str_data = std::to_string(data);
    }

    template <typename DATA>
    static void SetDataImpl(std::string& tmp, const std::string& base, const std::string& id, const DATA& data)
    {
        std::string retval;
        size_t last = 0;
        std::string var_id = "{" + id + "}";
        size_t var_id_size = var_id.size();
        size_t next = base.find(var_id, 0);
        std::string str_data;
        GetData(str_data, data);
        while (next != std::string::npos)
        {
            retval.append(base, last, next - last);
            retval.append(str_data);
            last = next + var_id_size;
            next = base.find(var_id, next + var_id_size);
        }
        retval.append(base, last, next);
        tmp = retval;
    }
};

#endif  // DB_TEST_REPLACE_H
