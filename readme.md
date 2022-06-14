# MYSQL Query

定义 : 执行一次查询需要以下动作

- 生成sql并查询
- 处理每一行结果

## 生成sql = sql [ + 参数]

```cpp
Query query;

// 不带参数
query.Init("select * from data");

// 带参数, 在sql模板中, 参数用 {参数名} 表示

// 自定义设置
std::shared_ptr<std::vector<std::string>> code_vect;
query.Init("select * from data where stock='{stock}' ")
     .With(code_vect, [](std::string& sql, const std::string& stock) {
         Replace::SetData(sql, "stock", stock);
})

struct Param
{
    std::string code;
};

// 绑定参数
std::shared_ptr<std::vector<Param>> code_vect;
query.Init("select * from data where stock='{stock}' ")
     .WithParam(code_vect, "stock", &Param::code)
```
## 处理数据

对于每一行的结果,我们需要将其处理/存储起来

处理函数有两种形式

### 1.没有绑定数据

```cpp
template <typename STORE>
Query& Store(std::function<void(STORE& store, Row& row)> handler);
```

如果客户使用了这个函数,query会在内部创建 store 对象, 然后传递给用户的handler

示例
```cpp
Query query;

// 不初始化绑定, 如果没有绑定，只能使用Row对象获取数据
query.Init("select name, id from data")
     .Store([](std::map<int32_t, Info>& data_table, Row& row) {
         int32_t id = 0;
         std::string name;
         row.GetData("id", id);
         row.GetData("name", name);
         data_table[id] = name;
     })
```

### 2.有绑定数据

```cpp
template <typename STORE, typename OBJ>
Query& Store(std::function<void(STORE& store, OBJ*, Row& row)> handler)
```

如果客户使用了这个函数,query会在内部创建 store, obj 对象, 然后传递给用户的handler

示例
```cpp
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
```

在Init中, 参数列表会自动替换 `{}`, Init后, sql模板为 

`select a.name, a.value from data a where stock='{stock}'`

在Init中进行绑定的对象成员变量,每一次`mysql_fetch_row`后,会自动设置绑定的值

没有进行绑定的值,会放到Row中,用户可以使用字符串进行匹配获取
