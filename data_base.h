#ifndef DATA_BASE_H
#define DATA_BASE_H

#include <string>
#include <map>
#include <mysql/mysql.h>

class DataBase
{
    MYSQL *m_conn = nullptr;
    MYSQL *m_db = nullptr;
    MYSQL_RES* m_res = nullptr;
public:
    typedef std::map<std::string, std::string> Row;

    DataBase();
    ~DataBase();

    bool connect(std::string host,
                 unsigned int port,
                 std::string login,
                 std::string password,
                 std::string db);

    bool query(std::string query);

    bool prepareFetch();
    bool fetchRow(Row &row);

    std::string escapeString(const std::string in);
    std::string error();

};

#endif // DATA_BASE_H
