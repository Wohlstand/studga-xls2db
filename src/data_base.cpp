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
    return m_res != NULL;
}

bool DataBase::fetchRow(DataBase::Row &row)
{
    if(!m_res)
        return false;

    MYSQL_ROW r;
    MYSQL_FIELD *f;
    uint32_t num_fields = mysql_num_fields(m_res);
    r = mysql_fetch_row(m_res);
    if(r == NULL)
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

std::string DataBase::escapeString(const std::string in)
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
