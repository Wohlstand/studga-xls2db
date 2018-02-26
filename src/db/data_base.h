/*
 * A small class to use MySQL connection via libmysqlclient
 *
 * Copyright (c) 2018 Vitaliy Novichkov <admin@wohlnet.ru>
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

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
