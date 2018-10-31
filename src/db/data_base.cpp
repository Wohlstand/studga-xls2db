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

#include "data_base.h"

DataBase::DataBase()
{
    m_conn = mysql_init(NULL);
    if(m_conn)
        mysql_options(m_conn, MYSQL_OPT_COMPRESS, 0);
}

DataBase::~DataBase()
{
    if(m_conn)
        mysql_close(m_conn);
}

bool DataBase::connect(std::string host,
                       unsigned int port,
                       std::string login,
                       std::string password,
                       std::string db)
{
    m_db = mysql_real_connect(m_conn, host.c_str(), login.c_str(), password.c_str(), db.c_str(), port, NULL, 0);
    if(m_db)
        mysql_set_character_set(m_db, "utf8");
    return (bool)m_db;
}

bool DataBase::query(std::string query)
{
    int err = mysql_real_query(m_db, query.c_str(), query.size());
    return err == 0;
}

bool DataBase::prepareFetch()
{
    m_res = mysql_store_result(m_db);
    return m_res != nullptr;
}

bool DataBase::fetchRow(DataBase::Row &row)
{
    if(!m_res)
        return false;

    MYSQL_ROW r;
    MYSQL_FIELD *f;
    uint32_t num_fields = mysql_num_fields(m_res);
    r = mysql_fetch_row(m_res);
    if(r == nullptr)
    {
        mysql_free_result(m_res);
        m_res = nullptr;
        return false;
    }

    row.clear();
    for(size_t i = 0; i < num_fields; i++)
    {
        mysql_field_seek(m_res, MYSQL_FIELD_OFFSET(i));
        f = mysql_fetch_field(m_res);
        row.insert({f->name, r[i] ? r[i] : "NULL"});
    }
    return true;
}

std::string DataBase::escapeString(const std::string &in)
{
    std::string out;
    unsigned long len;
    out.resize(in.size() * 2);
    len = mysql_real_escape_string(m_conn, &out[0], in.data(), in.size());
    out.resize((size_t)len);
    return out;
}

std::string DataBase::error()
{
    return std::string(mysql_error(m_conn));
}
