#include <iostream>
#include "db/query.h"

struct Info
{
    std::string name;
    int32_t value = 0;

    void Clear()
    {
        name.clear();
        value = 0;
    }
};

int main()
{
    mysql_library_init(0, nullptr, nullptr);
    // db池
    DBPool pool;

    // db配置
    DBConfig config;

    Query query;
    auto data_vect = std::make_shared<std::vector<std::string>>();
    data_vect->emplace_back("hello");
    // 同步获取数据
    DataQueue<std::shared_ptr<std::map<int32_t, Info>>> data_queue;
    query.Init("select {} from data a where stock='{stock}'",
               &Info::name, "a.name",
               &Info::value, "a.value")
        .With(data_vect, [](std::string& sql, const std::string& stock) {
            Replace::SetData(sql, "stock", stock);
        })
        .Store([](std::map<int32_t , Info>& data_table, Info* data, Row& row) {
          int32_t id = 0;
          row.Get("id", id);
          data_table[id] = *data;
        })
        .Run(1, pool, config, data_queue);

    std::shared_ptr<std::map<int32_t, Info>> data;
    data_queue.Pop(data);
    if (data)
    {
        for (auto& mgr : *data)
        {
            std::cout << mgr.first << ": " << mgr.second.name << std::endl;
        }
    }
//
//    // 异步获取数据
//    auto ebase = event_base_new();
//    std::function<void(std::map<std::string, FundMgr>&)> callback = std::bind(&Handle, std::placeholders::_1);
//    query.Init("select PersonalCode,ChineseName from MF_PersonalInfo",
//               &FundMgr::code, "PersonalCode",
//               &FundMgr::name, "ChineseName")
//        .Store([](std::map<std::string, FundMgr>& data_table, FundMgr* data, Row& row) {
//            data_table[data->code] = *data;
//        })
//        .Run(1, pool, config, ebase, callback);
//    event_base_dispatch(ebase);
//    event_base_free(ebase);

    mysql_library_end();
    return 0;
}

