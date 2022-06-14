#ifndef _DB_ROW_H
#define _DB_ROW_H

#include <map>
#include <string>
#include "convert.h"

struct Row
{
    template <typename T>
    bool Get(const std::string& key, T& value)
    {
        auto iter = m_field_table.find(key);
        if (iter == m_field_table.end())
        {
            return false;
        }
        Convert::ToData(iter->second, value);
        return true;
    }
    std::map<std::string, const char*> m_field_table;
};

#endif  // SEARCH_SRV_ROW_H
