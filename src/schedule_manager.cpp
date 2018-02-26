#include "schedule_manager.h"
#include "../db_login.h"
#include "Utils/strings.h"
#include <libgen.h>
#include <chrono>
#include <regex>
#include <sstream>

bool g_fullDebugInfo = false;

ScheduleManager::ScheduleManager()
{}

ScheduleManager::~ScheduleManager()
{
    unlockSchedule();
}

bool ScheduleManager::connectDataBase()
{
    return m_db.connect(BM_DATABASE_HOST, BM_DATABASE_PORT,
                        BM_DATABASE_USER, BM_DATABASE_PASSWORD,
                        BM_DATABASE_DATABASE);
}

std::string ScheduleManager::dbError()
{
    return m_db.error();
}

static void getDateTime(int *year, int *month = NULL, int *day = NULL, int *weekday = NULL, int *week = NULL)
{
    typedef std::chrono::system_clock Clock;
    auto now = Clock::now();
    std::time_t now_c = Clock::to_time_t(now);
    struct tm *parts = std::localtime(&now_c);
    if(year)
        *year = 1900 + parts->tm_year;
    if(month)
        *month = 1 + parts->tm_mon;
    if(day)
        *day = parts->tm_mday;
    if(weekday)
        *weekday = parts->tm_wday;
    if(week)
    {
        char weekbuffer[4];
        std::strftime(weekbuffer,4,"%W", parts);
        *week = std::atoi(weekbuffer);
    }
}


std::string ScheduleManager::ExInfo::datesToStr(const std::vector<ScheduleManager::ExInfo::Date> &dates, ssize_t index)
{
    std::string out;
    char buffer[50];
    if((index >= 0) && ((size_t)index < dates.size()))
    {
        const Date &d = dates[(size_t)index];
        std::snprintf(buffer, 50, "%04d-%02d-%02d", d.year, d.month, d.day);
        out += std::string(buffer);
    }
    else
    for(size_t i = 0; i < dates.size(); i++)
    {
        const Date &d = dates[i];
        std::snprintf(buffer, 50, "%04d-%02d-%02d", d.year, d.month, d.day);
        out += std::string(buffer);
        if(i < (dates.size() - 1))
            out.push_back(' ');
    }
    return out;
}


bool ScheduleManager::passScheduleFile(ScheduleFile &file)
{
    FlowInfo flow;
    std::vector<ExInfo> entries;
    m_errorString.clear();

    // Вычислить данные потока
    if(!extractFlowName(file.filePath(), flow, file.baseDate()))
        return false;
    // Синхронизировать их с базой данных
    if(!syncFlow(flow))
        return false;

    std::fprintf(stdout, "===============================================================================\n");
    std::fprintf(stdout, "Начинается процесс записи в базу данных, файл '%s'\n", file.fileName().c_str());
    std::fprintf(stdout, "Курс %d и группа %d\n", flow.course, flow.group);
    std::fprintf(stdout, "===============================================================================\n");
    std::fflush(stdout);

    if(!buildExList(file, entries))
        return false;

    if(!detectSemester(entries, file.baseDate()))
        return false;

    if(!syncData(entries, flow))
        return false;

    if(!writeToDb(entries, flow))
        return false;

    return true;
}

bool ScheduleManager::lockSchedule()
{
    if(m_scheduleLockIsActive)
        return true;
    std::ostringstream q;
    q << "UPDATE `schedule_flows` ";
    q << "SET ";
    q << "is_updating=1;";

    if(!m_db.query(q.str()))
    {
        std::fprintf(stderr, "DBERROR: (Включение блокировки) %s\n",
                     m_db.error().c_str());
        std::fflush(stderr);
        return false;
    }

    std::fprintf(stdout, "==DBWRITE: Установлена общая блокировка\n");
    std::fflush(stdout);
    m_scheduleLockIsActive = true;

    return true;
}

bool ScheduleManager::unlockSchedule()
{
    if(!m_scheduleLockIsActive)
        return true;
    std::ostringstream q;
    q << "UPDATE `schedule_flows` ";
    q << "SET ";
    q << "is_updating=0;";

    if(!m_db.query(q.str()))
    {
        std::fprintf(stderr, "DBERROR: (Отключение блокировки) %s\n",
                     m_db.error().c_str());
        std::fflush(stderr);
        return false;
    }

    std::fprintf(stdout, "==DBWRITE: Снята общая блокировка\n");
    std::fflush(stdout);
    m_scheduleLockIsActive = false;

    return true;
}

bool ScheduleManager::extractFlowName(std::string in_path, FlowInfo &out_flow, const ScheduleFile::BaseDate &dateCouple)
{
    static std::regex e_flowName(u8"([АБВГДЕЁЖЗИЙКЛМНОПРСТУФХЦЧШЩЪЫЬЭЮЯабвгдеёжзийклмнопрастуфхцчшщъыьэюя]+)\\ *(\\d+)\\-(\\d+)");
    static std::regex e_flowTitle(u8"^[АБВГДЕЁЖЗИЙКЛМНОПРСТУФХЦЧШЩЪЫЬЭЮЯабвгдеёжзийклмнопрастуфхцчшщъыьэюя]+$");
    static std::regex e_flowNumber(u8"^\\d+$");
    char *baseName = basename(&in_path[0]);
    std::string name(baseName);

    std::sregex_iterator e_iter(name.begin(), name.end(), e_flowName);
    std::sregex_iterator e_end;

    if(e_iter != e_end)
    {
        std::smatch rm = (*e_iter);
        if(rm.size() != 4)
        {
            m_errorString.resize(10000);
            int len = std::snprintf(&m_errorString[0], 10000, "ИМЯ ПОТОКА [%s] НЕ ВЕРНОЕ (кол-во REGEX-групп не совпало)!", name.c_str());
            m_errorString.resize((size_t)len);
            return false;
        }

        if(!std::regex_match(rm[1].str(), e_flowTitle))
        {
            m_errorString.resize(10000);
            int len = std::snprintf(&m_errorString[0], 10000, "ИМЯ ПОТОКА [%s] НЕ ВЕРНОЕ (Неверный формат названия)!", name.c_str());
            m_errorString.resize((size_t)len);
            return false;
        }

        if(!std::regex_match(rm[2].str(), e_flowNumber))
        {
            m_errorString.resize(10000);
            int len = std::snprintf(&m_errorString[0], 10000, "ИМЯ ПОТОКА [%s] НЕ ВЕРНОЕ (Неверный формат номера курса)!", name.c_str());
            m_errorString.resize((size_t)len);
            return false;
        }

        if(!std::regex_match(rm[3].str(), e_flowNumber))
        {
            m_errorString.resize(10000);
            int len = std::snprintf(&m_errorString[0], 10000, "ИМЯ ПОТОКА [%s] НЕ ВЕРНОЕ (Неверный формат номера группы)!", name.c_str());
            m_errorString.resize((size_t)len);
            return false;
        }

        out_flow.name   = rm[1].str();
        out_flow.course = std::atoi(rm[2].str().c_str());
        out_flow.group  = std::atoi(rm[3].str().c_str());

        //++e_iter;
    } else {
        m_errorString.resize(10000);
        int len = std::snprintf(&m_errorString[0], 10000, "ERROR: ИМЯ ПОТОКА [%s] НЕ ВЕРНОЕ (Выражение не сработало)!", name.c_str());
        m_errorString.resize((size_t)len);
        return false;
    }

    int baseYear = dateCouple.date_year, baseMonth = dateCouple.date_month;
    out_flow.year_enter = (baseMonth < 8) ? baseYear - out_flow.course : baseYear - out_flow.course + 1;
    out_flow.year_quit = out_flow.year_enter + (name.find("б") != std::string::npos ? 4 : 5);

    std::fprintf(stdout, "INFO: Файл %s, Поток: %s (%d-%d); Курс %d, группа %d\n",
                 name.c_str(),
                 out_flow.name.c_str(),
                 out_flow.year_enter,
                 out_flow.year_quit,
                 out_flow.course,
                 out_flow.group);
    std::fflush(stdout);

    return true;
}

bool ScheduleManager::syncFlow(ScheduleManager::FlowInfo &flow)
{
retryQuery:
    std::ostringstream q;
    q << "SELECT * FROM `schedule_flows` ";
    q << "WHERE ";
    q << "gr_name='" << flow.name << "' ";
    q << "AND `gr_year-start`=" << flow.year_enter << " ";
    q << "LIMIT 1;";

    if(m_db.query(q.str()) && m_db.prepareFetch())
    {
        DataBase::Row row;
        while(m_db.fetchRow(row))
        {
            std::fprintf(stdout, "INFO: БД: ПОТОК: id=%s fac=%s, groups_q=%s\n",
                        row["id_flow"].c_str(),
                        row["id_facult"].c_str(),
                        row["group_q"].c_str()
                    );
            std::fflush(stdout);
            // Возьмём внутреннее ID для последующей записи в БД
            flow.db_id = std::atoi(row["id_flow"].c_str());

            // Увеличим счётчик групп в соответствии с полученным номером группы
            if(std::atoi(row["group_q"].c_str()) < flow.group)
            {
                std::ostringstream qu;
                qu << "UPDATE `schedule_flows` SET ";
                qu << "group_q=" << flow.group << " ";
                qu << "WHERE `id_flow`=" << flow.db_id << " ";
                qu << "LIMIT 1;";
                if(!m_db.query(qu.str()))
                    goto dberror_is_here;
                std::fprintf(stdout, "INFO: БД: Обновлено groups_q=%d\n", flow.db_id);
                std::fflush(stdout);
            }

            return true;
        }

        // Если данных нету, создадим!
        {
            std::ostringstream qi;
            qi << "INSERT INTO `schedule_flows` ";
            qi << "(`gr_name`, `gr_year-start`, `gr_year-end`, `id_facult`, `group_q`)";
            qi << "values(";
            qi << "'" << flow.name << "', ";
            qi << flow.year_enter << ", ";
            qi << flow.year_quit << ", ";
            qi << "(SELECT id_facult FROM `schedule_facult` WHERE `fac_gr_names` like '%\"" << flow.name << "\"%' LIMIT 1), ";
            qi << 1;
            qi << ");";
            if(!m_db.query(qi.str()))
                goto dberror_is_here;
            std::fprintf(stdout, "INFO: БД: Создана новая запись id_flow=%d\n", flow.db_id);
            std::fflush(stdout);
            goto retryQuery;
        }
    }

dberror_is_here:
    std::fprintf(stderr, "DBERROR: (Потоки) %s\n",
                 m_db.error().c_str());
    std::fflush(stderr);
    return false;
}

#if 0
#define printDA(arg, ...) std::printf(arg, __VA_ARGS__)
#define printD(arg) std::printf(arg)
#else
#define printDA(arg, ...)
#define printD(arg)
#endif

ScheduleManager::ExInfo::Date ScheduleManager::strToDate(const std::string &dateStr,
                                                         const ScheduleFile::BaseDate &dateCouple)
{
    std::vector<std::string> got_date;
    Strings::split(got_date, dateStr, '.');

    ExInfo::Date date;
    date.day    = std::atoi(got_date[0].c_str());
    date.month  = std::atoi(got_date[1].c_str());
    date.year   = dateCouple.date_month >= 8 ?
                    ((date.month < 8) ? dateCouple.date_year + 1 : dateCouple.date_year)
                    : dateCouple.date_year;
    return date;
}

bool ScheduleManager::buildExList(ScheduleFile &file, std::vector<ExInfo> &out_list)
{
    out_list.clear();

    for(const ScheduleFile::OneDayData_Src &entry : file.entries())
    {
        static const std::regex onlyday(u8"^только (\\d{1,2}\\.\\d{1,2}\\;?)+$");
        static const std::regex except(u8"^кроме (\\d\\d\\.\\d\\d\\;?)+$");
        static const std::regex range(u8"^с (\\d\\d\\.\\d\\d)\\ по (\\d\\d\\.\\d\\d)$");
        static const std::regex range_exc(u8"^с \\d\\d\\.\\d\\d\\ по \\d\\d\\.\\d\\d\\ +кроме (\\d\\d\\.\\d\\d\\;?)+");

        static const std::regex one_date(u8"(\\d{1,2}\\.\\d{1,2})");
        ExInfo info;
        info.src_data = entry;

        if(std::regex_match(info.src_data.date_condition, onlyday))
        {
            std::sregex_iterator e_iter(info.src_data.date_condition.begin(),
                                        info.src_data.date_condition.end(),
                                        one_date, std::regex_constants::match_any);
            std::sregex_iterator e_end;
            printDA("DATES INPUT: %s\n", info.src_data.date_condition.c_str());
            while(e_iter != e_end)
            {
                std::smatch rm = (*e_iter);
                printDA("DATES: %lu\n", rm.size());
                for(size_t i = 1; i < rm.size(); i++)
                {
                    printDA("DATE ONLY: %s\n", rm[i].str().c_str());
                    info.date_only.push_back(strToDate(rm[i].str(), file.baseDate()));
                }
                ++e_iter;
            }
            printD("------------\n");
        }

        else
        if(std::regex_match(info.src_data.date_condition, range))
        {
            std::sregex_iterator e_iter(info.src_data.date_condition.begin(),
                                        info.src_data.date_condition.end(),
                                        one_date, std::regex_constants::match_any);
            std::sregex_iterator e_end;
            size_t ir = 0;

            printDA("DATES INPUT: %s\n", info.src_data.date_condition.c_str());
            info.date_range.resize(2);
            while((e_iter != e_end) && (ir < 2))
            {
                std::smatch rm = (*e_iter);
                printDA("DATES: %lu\n", rm.size());
                for(size_t i = 1; i < rm.size(); i++)
                {
                    printDA("DATE_RANGE: %s\n", rm[i].str().c_str());
                    info.date_range[ir] = strToDate(rm[i].str(), file.baseDate());
                    ++ir;
                }
                ++e_iter;
            }
            printD("------------\n");
        }

        else
        if(std::regex_match(info.src_data.date_condition, except))
        {
            std::sregex_iterator e_iter(info.src_data.date_condition.begin(),
                                        info.src_data.date_condition.end(),
                                        one_date, std::regex_constants::match_any);
            std::sregex_iterator e_end;
            printDA("DATES INPUT: %s\n", info.src_data.date_condition.c_str());
            while(e_iter != e_end)
            {
                std::smatch rm = (*e_iter);
                printDA("DATES: %lu\n", rm.size());
                for(size_t i = 1; i < rm.size(); i++)
                {
                    printDA("DATE_EXCEPT: %s\n", rm[i].str().c_str());
                    info.date_except.push_back(strToDate(rm[i].str(), file.baseDate()));
                }
                ++e_iter;
            }
            printD("------------\n");
        }

        else
        if(std::regex_match(info.src_data.date_condition, range_exc))
        {
            std::sregex_iterator e_iter(info.src_data.date_condition.begin(),
                                        info.src_data.date_condition.end(),
                                        one_date, std::regex_constants::match_any);
            std::sregex_iterator e_end;
            size_t ir = 0;

            info.date_range.resize(2);
            printDA("DATES INPUT: %s\n", info.src_data.date_condition.c_str());
            while(e_iter != e_end)
            {
                std::smatch rm = (*e_iter);
                printDA("DATES: %lu\n", rm.size());
                for(size_t i = 1; i < rm.size(); i++)
                {
                    if(ir < 2)
                    {
                        printDA("DATE_RANGE: %s\n", rm[i].str().c_str());
                        info.date_range[ir] = strToDate(rm[i].str(), file.baseDate());
                        ++ir;
                    }
                    else
                    {
                        printDA("DATE_EXCEPT: %s\n", rm[i].str().c_str());
                        info.date_except.push_back(strToDate(rm[i].str(), file.baseDate()));
                    }
                }
                ++e_iter;
            }
            printD("------------\n");
        }
        else
        {
            std::fprintf(stderr, "ERROR: Не удалось распознать тип даты! [%s]\n", info.src_data.date_condition.c_str());
            std::fflush(stderr);
            return false;
        }

        out_list.push_back(info);
    }

    return true;
}

bool ScheduleManager::detectSemester(std::vector<ScheduleManager::ExInfo> &out_list,
                                     const ScheduleFile::BaseDate &baseDate)
{
    //      Включить статистическое распознание семестра (весна / осень)
    //Статистическое распознание происходит за счёт подсчёта упоминаемых номеров месяцев
    //по временам года. Перевес определяет, весенний ли это семестр или осенний.
    //      Полезно для выявления подозрительных файлов расписаний, которые по "базовой дате"
    //(самой первой дате, указанной на листе "развёрнуто") дают одно время года
    //но содержимое далеко уходит в другое...
#define DATES_STAT_WINTER

    int current_year,
        current_month,
    #ifdef DATES_STAT_WINTER
        dates_spring  = 0,
        dates_autumnt = 0,
    #endif
        base_year = baseDate.date_year;

#ifdef DATES_STAT_WINTER
    //! Файл принадлежит осенне-зимнему семестру (статистически)
    bool isAutumnWinterStat = false;
#endif
    //! Файл принадлежит осенне-зимнему семестру (по базовой дате)
    bool isAutumnWinterBase = false;
    //! Осенне-зимний семестр здесь и сейчас
    bool isAutumnWinterNow = false;

    getDateTime(&current_year, &current_month);
    isAutumnWinterNow = (current_month >= 8);
    isAutumnWinterBase = (baseDate.date_month >= 9);

#if 1
    if(current_year != base_year)
    {
        m_errorString = "ERROR: Семестр файла не соответсвует текущему году!";
        return false;
    }
#endif

#ifdef DATES_STAT_WINTER
    for(ExInfo &info : out_list)
    {
        for(ExInfo::Date &d : info.date_range)
        {
            switch(d.month)
            {
            case 9: case 10: case 11:
            case 12: case 1:
                dates_autumnt++;
                break;
            case 2://Февраль как "весна", т.е. начало нового семестра
            case 3: case 4: case 5:
            case 6: case 7: case 8:
                dates_spring++;
                break;
            }
        }

        for(ExInfo::Date &d : info.date_only)
        {
            switch(d.month)
            {
            case 9: case 10: case 11:
            case 12: case 1:
                dates_autumnt++;
                break;
            case 2://Февраль как "весна", т.е. начало нового семестра
            case 3: case 4: case 5:
            case 6: case 7: case 8:
                dates_spring++;
                break;
            }
        }

        for(ExInfo::Date &d : info.date_except)
        {
            switch(d.month)
            {
            case 9: case 10: case 11:
            case 12: case 1:
                dates_autumnt++;
                break;
            case 2://Февраль как "весна", т.е. начало нового семестра
            case 3: case 4: case 5:
            case 6: case 7: case 8:
                dates_spring++;
                break;
            }
        }
    }

    isAutumnWinterStat = dates_autumnt > dates_spring;
    if(isAutumnWinterStat)
        std::printf("Осень-Зима %d (статистика)\n", base_year);
    else
        std::printf("Весна-Лето %d (статистика)\n", base_year);
#endif

    if(isAutumnWinterBase)
        std::printf("Осень-Зима %d (базовая дата)\n", base_year);
    else
        std::printf("Весна-Лето %d (базовая дата)\n", base_year);
    std::fflush(stdout);

#ifdef DATES_STAT_WINTER
    if(isAutumnWinterBase != isAutumnWinterStat)
    {
        std::fprintf(stderr, "WARNING: Базовый и Статистические сезоны не совпадают!\n");
        std::fflush(stderr);
        return false;
    }
#endif

    bool couples = false;
    {
        //Рассчитать бит чётности
        std::tm time;
        char wbuf[3];
        memset(&time, 0, sizeof(std::tm));
        time.tm_year = baseDate.date_year - 1900;
        time.tm_mon  = baseDate.date_month - 1;
        time.tm_mday = baseDate.date_day;
        std::mktime(&time);
        std::strftime(wbuf, 3, "%W", &time);
        int weekOfBase = std::atoi(wbuf);
        int couplesBase = (int)(baseDate.couple == "Н");
        int couplesCalculated = (weekOfBase % 2) ^ couples;
        if(couplesBase != couplesCalculated)
            couples = true;
        std::printf("Базовая дата %04d-%02d-%02d\n",
                    time.tm_year + 1900,
                    time.tm_mon + 1,
                    time.tm_mday);
        std::printf("Чётности: базовая %s, рассчитанная %s (до корректировки)\n",
                    couplesBase ? "Н" : "B",
                    couplesCalculated ? "Н" : "B");
        couplesCalculated = (weekOfBase % 2) ^ couples;
        std::printf("Чётности: базовая %s, рассчитанная %s (после корректировки)\n",
                    couplesBase ? "Н" : "B",
                    couplesCalculated ? "Н" : "B");
        if(isAutumnWinterBase)
            std::printf("Чётность: %d (осень)\n", (int)couples);
        else
            std::printf("Чётность: %d (весна)\n", (int)couples);
    }

    int semesterID = getSemesterId(base_year, isAutumnWinterBase, couples);
    for(ExInfo &info : out_list)
        info.id_daycount = semesterID;

// #define DATES_DEBUG

#ifdef DATES_DEBUG
    for(ExInfo &info : out_list)
    {
        for(ExInfo::Date &d : info.date_range)
        {
            //if(d.month >= 8)
            //    d.year = isAutumnWinter ? base_year : base_year - 1;
            //else
            //    d.year = isAutumnWinter ? base_year + 1 : base_year;
        #ifdef DATES_DEBUG
            std::printf("DATE: %d-%d-%d\n", d.year, d.month, d.day);
        #endif
        }

        for(ExInfo::Date &d : info.date_only)
        {
            //if(d.month >= 8)
            //    d.year = isAutumnWinter ? base_year : base_year - 1;
            //else
            //    d.year = isAutumnWinter ? base_year + 1 : base_year;
            #ifdef DATES_DEBUG
                std::printf("DATE: %d-%d-%d\n", d.year, d.month, d.day);
            #endif
        }

        for(ExInfo::Date &d : info.date_except)
        {
            //if(d.month >= 8)
            //    d.year = isAutumnWinter ? base_year : base_year - 1;
            //else
            //    d.year = isAutumnWinter ? base_year + 1 : base_year;
            #ifdef DATES_DEBUG
                std::printf("DATE: %d-%d-%d\n", d.year, d.month, d.day);
            #endif
        }
    }
#endif

    std::fflush(stdout);

    return true;
}

bool ScheduleManager::syncData(std::vector<ScheduleManager::ExInfo> &inout_list, FlowInfo &out_flow)
{
    for(ExInfo &info : inout_list)
    {
        info.id_flow    = out_flow.db_id;
        info.id_group   = out_flow.group;
        info.is_subgroups = info.src_data.subgroup > 0;
        info.id_subgrp  = info.src_data.subgroup;

        info.number     = std::atoi(info.src_data.number.c_str());
        info.weekday    = std::atoi(info.src_data.week_day.c_str());
        if(!info.src_data.week_couple.empty())
        {
            if(info.src_data.week_couple == "В")
                info.couple_cond = 1;
            else
                info.couple_cond = 2;
        }

        info.id_lector  = getLectorId(info.src_data.lector_name, info.src_data.lector_rank);
        if(info.id_lector < 0)
            return false;
        info.id_type    = getLTypeId(info.src_data.type);
        if(info.id_type < 0)
            return false;
        info.id_name    = getDisciplyneId(info.src_data.disciplyne_name);
        if(info.id_name < 0)
            return false;
        info.id_room    = getRoomId(info.src_data.room_name);
        if(info.id_room < 0)
            return false;
    }

    return true;
}

bool ScheduleManager::writeToDb(std::vector<ScheduleManager::ExInfo> &inout_list, FlowInfo &out_flow)
{
    std::ostringstream dq;
    dq << "DELETE FROM `schedule__maindata` ";
    dq << "WHERE ";
    dq << "`id_flow`=" << out_flow.db_id << " ";
    dq << "AND `id_group` LIKE '%" << out_flow.group << "%' ";
    dq << "AND `change` = 0;";
    if(!m_db.query(dq.str()))
    {
        std::fprintf(stderr, "DBERROR: (удаление старых данных перед добавлением) %s\n",
                     m_db.error().c_str());
        std::fflush(stderr);
        return false;
    }

    std::fprintf(stdout, "==DBWRITE: Из базы удалены данные потока %d и группы %d !!!\n", out_flow.db_id, out_flow.group);
    std::fflush(stdout);

    std::ostringstream q;
    q << "INSERT INTO `schedule__maindata` ";
    q << "(id_day,"
         "`weekday`, "
         "lection, "
         "couples, "
         "id_disciplyne, "
         "id_ltype, "
         "id_flow, "
         "id_group, "
         "issubgrp, "
         "id_subgrp, "
         "id_lector, "
         "id_room, "
         "`period-start`, "
         "`period-end`, "
         "exceptions, "
         "onlydays)\n";

    bool first = true;
    for(ExInfo &info : inout_list)
    {
        if(first)
        {
            q << "values\n";
            first = false;
        }
        else
            q << ",\n";

        q << "(";
        q << info.id_daycount << ", ";
        q << info.weekday << ", ";
        q << info.number << ", ";
        q << (!info.date_only.empty() ? 0 : info.couple_cond) << ", ";
        q << info.id_name << ", ";
        q << info.id_type << ", ";
        q << info.id_flow << ", ";
        q << "'" << info.id_group << "', ";
        q << info.is_subgroups << ", ";
        q << "'" << (info.is_subgroups ? std::to_string(info.id_subgrp) : "") << "', ";
        q << info.id_lector << ", ";
        q << info.id_room << ", ";
        if(info.date_range.size() != 2)
            q << "NULL, NULL, ";
        else
        {
            q << "'" << ExInfo::datesToStr(info.date_range, 0) << "', ";
            q << "'" << ExInfo::datesToStr(info.date_range, 1) << "', ";
        }
        q << "'" << ExInfo::datesToStr(info.date_except) << "', ";
        q << "'" << ExInfo::datesToStr(info.date_only) << "'";
        q << ")";
    }
    q << ";";

    if(g_fullDebugInfo)
    {
        std::printf("\n-------------------------\n%s\n-----------------------", q.str().c_str());
        std::fflush(stdout);
    }

    if(!m_db.query(q.str()))
    {
        std::fprintf(stderr, "DBERROR: (добавление новых данных) %s\n", m_db.error().c_str());
        std::fflush(stderr);
        return false;
    }

    std::fprintf(stdout, "==DBWRITE: Новая таблица для потока %d и группы %d успешно записана!\n", out_flow.db_id, out_flow.group);
    std::fflush(stdout);

    {
        std::ostringstream dm;
        dm << "UPDATE `schedule_flows` ";
        dm << "SET ";
        dm << "`latest_upd`=CURRENT_TIMESTAMP() ";
        dm << "WHERE `id_flow`=" << out_flow.db_id << ";";

        if(!m_db.query(dm.str()))
        {
            std::fprintf(stderr, "DBERROR: (Установка времени обновления расписания потоку) %s\n",
                         m_db.error().c_str());
            std::fflush(stderr);
            return false;
        }
    }

    return true;
}

static bool optimizeMainTable_Commit(DataBase &m_db,
                                     std::vector<unsigned long long> qu_ids_in,
                                     unsigned long long &counter)
{
    std::ostringstream qu;
    unsigned long long counter_commit = counter;
    unsigned long long inserted_ids = 0;

    qu << "UPDATE `schedule__maindata` SET id=";
    qu << "(CASE \n";
    for(size_t i = 0; i < qu_ids_in.size(); i++)
    {
        unsigned long long subid = qu_ids_in[i];
        if(subid == counter)
        {
            counter++;
            continue;
        }
        inserted_ids++;
        qu << "WHEN id=" << subid << " THEN " << (counter++) << " \n";
    }
    qu << "END) \n";

    if(inserted_ids == 0)
        return true;//Нечего делать

    qu << "WHERE id in (";
    counter = counter_commit;
    for(size_t i = 0; i < qu_ids_in.size(); i++)
    {
        unsigned long long subid = qu_ids_in[i];
        if(subid == counter)
        {
            counter++;
            continue;
        }
        counter++;
        qu << subid;
        if(i < qu_ids_in.size() - 1)
            qu << ", ";
    }
    counter_commit = counter;
    qu << ");\n";

    if(g_fullDebugInfo)
    {
        std::printf("\n-------------------------\n%s\n-----------------------", qu.str().c_str());
        std::fflush(stdout);
    }

    if(!m_db.query(qu.str()))
        return false;
    return true;
}

bool ScheduleManager::optimizeMainTable()
{
    std::fprintf(stdout, "==DBWRITE: Оптимизация таблицы `schedule__maindata`....\n");
    std::fflush(stdout);
    unsigned long long total = 0;
    std::ostringstream qt;
    std::ostringstream q;
    DataBase::Row row;

    qt << "SELECT count(*) AS total FROM `schedule__maindata` ORDER BY id ASC LIMIT 1;";
    if(!m_db.query(qt.str()))
        goto dberror_is_here;
    if(m_db.prepareFetch() && m_db.fetchRow(row))
    {
        total = std::stoull(row["total"].c_str(), NULL, 10);
    }

    q << "SELECT id FROM `schedule__maindata` ORDER BY id ASC;";

    if(m_db.query(q.str()))
    {
        if(m_db.prepareFetch())
        {
            unsigned long long counter = 1;
            unsigned long long id;
            std::vector<unsigned long long> qu_ids_in;
            int limitter = 500;

            while(m_db.fetchRow(row))
            {
                id = std::stoull(row["id"].c_str(), NULL, 10);
                qu_ids_in.push_back(id);
                if(limitter <= 0)
                {
                    if(!optimizeMainTable_Commit(m_db, qu_ids_in, counter))
                        goto dberror_is_here;
                    limitter = 500;
                    qu_ids_in.clear();
                } else
                    limitter--;

                static const char* strs[4] =
                {
                    ".   ", "..  ", "... ", "...."
                };

                std::fprintf(stdout, "(totally %llu) %llu%s\r", total, counter, strs[counter % 4]);
                std::fflush(stdout);
            }

            if(limitter > 0)
            {
                if(!optimizeMainTable_Commit(m_db, qu_ids_in, counter))
                    goto dberror_is_here;
            }

            {
                std::fprintf(stdout, "==DBWRITE: Сброс счётчика...\n");
                std::fflush(stdout);

                std::ostringstream qa;
                qa << "ALTER TABLE `schedule__maindata` AUTO_INCREMENT=" << (total + 1) << ";";
                if(!m_db.query(qa.str()))
                    goto dberror_is_here;
            }
        } else
            goto dberror_is_here;
    } else {
dberror_is_here:
        std::fprintf(stderr, "DBERROR: (Оптимизация главной таблицы) %s\n",
                     m_db.error().c_str());
        std::fflush(stderr);
        return false;
    }

    std::fprintf(stdout, "==DBWRITE: Оптимизировано!\n");
    std::fflush(stdout);

    return true;
}

std::string ScheduleManager::errorString() const
{
    return m_errorString;
}



int ScheduleManager::getSemesterId(int year, bool isAutumn, bool couples)
{
    // Попытаться воспользоваться кэшированной-записью (чтобы не обращаться каждый раз к серверу баз данных)
    if(m_daycounts.contains({year, isAutumn}))
    {
        DataBase::Row &row = m_daycounts.m_cachedRow[{year, isAutumn}];
        int id = std::atoi(row["id_day"].c_str());
        if(g_fullDebugInfo)
        {
            std::fprintf(stdout, "INFO: Сееместр %d (%s) [CACHE]\n", id, row["desc"].c_str());
            std::fflush(stdout);
        }
        return id;
    }

retryQuery:
    std::ostringstream q;
    q << "SELECT * FROM `schedule_daycount` ";
    q << "WHERE ";
    q << "STR_TO_DATE('" << year << "-" << (isAutumn ? 11 : 4) << "-25', '%Y-%m-%d') ";
    q << "BETWEEN daystart AND dayend ";
    q << "LIMIT 1;";

    if(m_db.query(q.str()) && m_db.prepareFetch())
    {
        DataBase::Row row;
        int id = -1;
        while(m_db.fetchRow(row))
        {
            id = std::atoi(row["id_day"].c_str());
            if(g_fullDebugInfo)
            {
                std::fprintf(stdout, "INFO: Сееместр %d (%s)\n", id, row["desc"].c_str());
                std::fflush(stdout);
            }
            m_daycounts.m_cachedRow.insert({{year, isAutumn}, row});
        }

        if(id < 0)
        {
            std::ostringstream qi;
            qi << "INSERT INTO `schedule_daycount` ";
            qi << "(`daystart`, `dayend`, `desc`, `couples`) ";
            qi << "values(";
            if(isAutumn)
                qi << "'" << year << "-09-01', '" << (year + 1) << "-01-25', '" << year << " - Осень', ";
            else
                qi << "'" << year << "-01-26', '" << (year + 1) << "-08-01', '" << year << " - Весна', ";
            qi << (couples ? "1" : "0") << ");";
            if(!m_db.query(qi.str()))
                goto dberror_is_here;
            std::fprintf(stdout, "INFO: БД: Создан новый семестр! [%04d] - %s\n", year, (isAutumn ? "Осень" : "Весна"));
            std::fflush(stdout);
            goto retryQuery;
        }
        return id;
    }

dberror_is_here:
    std::fprintf(stderr, "DBERROR: (Семестры) %s\n",
                 m_db.error().c_str());
    std::fflush(stderr);
    return -1;
}

static const std::unordered_map<std::string, std::string> g_lectorRank =
{
    {"преп.",       "Преподаватель"},
    {"ст.преп.",    "Старший преподаватель"},
    {"доц.",        "Доцент"},
    {"проф.",       "Профессор"},
    {"ассист.",     "Ассистент"},
    {"зав.каф.",    "Заведующий кафедры"}
};

int ScheduleManager::getLectorId(const std::string &name, const std::string &rank)
{
    // Попытаться воспользоваться кэшированной-записью (чтобы не обращаться каждый раз к серверу баз данных)
    if(m_lectors.contains({name, rank}))
    {
        DataBase::Row &row = m_lectors.m_cachedRow[{name, rank}];
        int id = std::atoi(row["id_lector"].c_str());
        if(g_fullDebugInfo)
        {
            std::fprintf(stdout, "INFO: Преподаватель %d (%s) [CACHE]\n", id, row["lcr_fullname"].c_str());
            std::fflush(stdout);
        }
        return id;
    }

retryQuery:
    std::ostringstream q;
    q << "SELECT * FROM `schedule_lectors` ";
    q << "WHERE ";
    q << "lcr_shtname='" << name << "' ";
    q << "LIMIT 1;";

    if(m_db.query(q.str()) && m_db.prepareFetch())
    {
        DataBase::Row row;
        int id = -1;
        while(m_db.fetchRow(row))
        {
            id = std::atoi(row["id_lector"].c_str());
            if(g_fullDebugInfo)
            {
                std::fprintf(stdout, "INFO: Преподаватель %d (%s)\n", id, row["lcr_fullname"].c_str());
                std::fflush(stdout);
            }
            m_lectors.m_cachedRow.insert({{name, rank}, row});
        }

        if(id < 0)
        {
            std::string rankLong = rank;
            std::unordered_map<std::string, std::string>::const_iterator rank_it = g_lectorRank.find(rank);
            if(rank_it != g_lectorRank.end())
                rankLong = rank_it->second;

            std::ostringstream qi;
            qi << "INSERT INTO `schedule_lectors` ";
            qi << "(`lcr_fullname`, `lcr_shtname`, `lcr_rank-l`) ";
            qi << "values(";
            qi << "'" << name << "', '" << name << "', '" << rankLong << "'";
            qi << ");";
            if(!m_db.query(qi.str()))
                goto dberror_is_here;
            std::fprintf(stdout, "INFO: БД: Создан новый преподаватель!\n");
            std::fflush(stdout);
            goto retryQuery;
        }
        return id;
    }

dberror_is_here:
    std::fprintf(stderr, "DBERROR: (Преподаватели) %s\n",
                 m_db.error().c_str());
    std::fflush(stderr);
    return -1;
}

int ScheduleManager::getDisciplyneId(const std::string &disciplyne)
{
    // Попытаться воспользоваться кэшированной-записью (чтобы не обращаться каждый раз к серверу баз данных)
    if(m_disciplynes.contains(disciplyne))
    {
        DataBase::Row &row = m_disciplynes.m_cachedRow[disciplyne];
        int id = std::atoi(row["id_disciplyne"].c_str());
        if(g_fullDebugInfo)
        {
            std::fprintf(stdout, "INFO: Дисциплина %d (%s) [CACHE]\n", id, row["dysc_name"].c_str());
            std::fflush(stdout);
        }
        return id;
    }

retryQuery:
    std::ostringstream q;
    q << "SELECT * FROM `schedule_disciplyne` ";
    q << "WHERE ";
    q << "dysc_name='" << disciplyne << "' ";
    q << "LIMIT 1;";

    if(m_db.query(q.str()) && m_db.prepareFetch())
    {
        DataBase::Row row;
        int id = -1;
        while(m_db.fetchRow(row))
        {
            id = std::atoi(row["id_disciplyne"].c_str());
            if(g_fullDebugInfo)
            {
                std::fprintf(stdout, "INFO: Дисциплина %d (%s)\n", id, row["dysc_name"].c_str());
                std::fflush(stdout);
            }
            m_disciplynes.m_cachedRow.insert({disciplyne, row});
        }

        if(id < 0)
        {
            std::ostringstream qi;
            qi << "INSERT INTO `schedule_disciplyne` ";
            qi << "(`dysc_name`) ";
            qi << "values(";
            qi << "'" << disciplyne << "'";
            qi << ");";
            if(!m_db.query(qi.str()))
                goto dberror_is_here;
            std::fprintf(stdout, "INFO: БД: Создана новая дисциплина!\n");
            std::fflush(stdout);
            goto retryQuery;
        }
        return id;
    }

dberror_is_here:
    std::fprintf(stderr, "DBERROR: (Имена дисциплин) %s\n",
                 m_db.error().c_str());
    std::fflush(stderr);
    return -1;
}

int ScheduleManager::getRoomId(const std::string &roomName)
{
    // Попытаться воспользоваться кэшированной-записью (чтобы не обращаться каждый раз к серверу баз данных)
    if(m_rooms.contains(roomName))
    {
        DataBase::Row &row = m_rooms.m_cachedRow[roomName];
        int id = std::atoi(row["id_room"].c_str());
        if(g_fullDebugInfo)
        {
            std::fprintf(stdout, "INFO: Аудитория %d (%s) [CACHE]\n", id, row["room_number"].c_str());
            std::fflush(stdout);
        }
        return id;
    }

retryQuery:
    std::ostringstream q;
    q << "SELECT * FROM `schedule_rooms` ";
    q << "WHERE ";
    q << "`room_number`='" << roomName << "' ";
    q << "LIMIT 1;";

    if(m_db.query(q.str()) && m_db.prepareFetch())
    {
        DataBase::Row row;
        int id = -1;
        while(m_db.fetchRow(row))
        {
            id = std::atoi(row["id_room"].c_str());
            if(g_fullDebugInfo)
            {
                std::fprintf(stdout, "INFO: Аудитория %d (%s)\n", id, row["room_number"].c_str());
                std::fflush(stdout);
            }
            m_rooms.m_cachedRow.insert({roomName, row});
        }

        if(id < 0)
        {
            int houseId = 0;
            int stageNum = 0;
            static std::regex room_oldBuild(u8"[1-5]\\-\\d\\d\\d");
            static std::regex room_newBuildWings(u8"^\\d\\d\\d(?:а|б|в|г|д|е)$");
            static std::regex room_newBuildR(u8"^\\d+р$");

            if(std::regex_match(roomName, room_oldBuild))
            {
                std::vector<std::string> cabN;
                Strings::split(cabN, roomName, '-');
                if(cabN.size() == 2)
                {
                    houseId = std::atoi(cabN[0].c_str());
                    stageNum = (cabN[1][0] - '0');
                }
            }
            else
            if(std::regex_match(roomName, room_newBuildWings))
            {
                houseId = 6;
                if(std::isdigit(roomName[0]))
                    stageNum = (roomName[0] - '0');
                else
                    stageNum = 0;
            }
            else
            if(std::regex_match(roomName, room_newBuildR))
            {
                houseId = 6;
                stageNum = 1;
            }

            std::ostringstream qi;
            qi << "INSERT INTO `schedule_rooms` ";
            qi << "(`room_number`, `id_house`, `room_stage`) ";
            qi << "values(";
            qi << "'" << roomName << "', " << houseId << ", " << stageNum << "";
            qi << ");";
            if(!m_db.query(qi.str()))
                goto dberror_is_here;
            std::fprintf(stdout, "INFO: БД: Создана новая комната [%s]!\n", roomName.c_str());
            std::fflush(stdout);
            goto retryQuery;
        }
        return id;
    }

dberror_is_here:
    std::fprintf(stderr, "DBERROR: (Комнаты/аудитории) %s\n",
                 m_db.error().c_str());
    std::fflush(stderr);
    return -1;
}



int ScheduleManager::getLTypeId(const std::string &ltypeName)
{
    // Попытаться воспользоваться кэшированной-записью (чтобы не обращаться каждый раз к серверу баз данных)
    if(m_ltypes.contains(ltypeName))
    {
        DataBase::Row &row = m_ltypes.m_cachedRow[ltypeName];
        int id = std::atoi(row["id_ltype"].c_str());
        if(g_fullDebugInfo)
        {
            std::fprintf(stdout, "INFO: Тип занятий %d (%s) [CACHE]\n", id, row["lt_name"].c_str());
            std::fflush(stdout);
        }
        return id;
    }

retryQuery:
    std::ostringstream q;
    q << "SELECT * FROM `schedule_ltype` ";
    q << "WHERE ";
    q << "lt_name_sh='" << ltypeName << "' ";
    q << "LIMIT 1;";

    if(m_db.query(q.str()) && m_db.prepareFetch())
    {
        DataBase::Row row;
        int id = -1;
        while(m_db.fetchRow(row))
        {
            id = std::atoi(row["id_ltype"].c_str());
            if(g_fullDebugInfo)
            {
                std::fprintf(stdout, "INFO: Тип занятий %d (%s)\n", id, row["lt_name"].c_str());
                std::fflush(stdout);
            }
            m_ltypes.m_cachedRow.insert({ltypeName, row});
        }

        if(id < 0)
        {
            std::string ltypeNameLong = ltypeName;

            std::ostringstream qi;
            qi << "INSERT INTO `schedule_ltype` ";
            qi << "(`lt_name`, `lt_name_sh`) ";
            qi << "values(";
            qi << "'" << ltypeNameLong << "', '" << ltypeName << "'";
            qi << ");";
            if(!m_db.query(qi.str()))
                goto dberror_is_here;
            std::fprintf(stdout, "INFO: БД: Создан новый тип занятий! [%s]\n", ltypeNameLong.c_str());
            std::fflush(stdout);
            goto retryQuery;
        }
        return id;
    }

dberror_is_here:
    std::fprintf(stderr, "DBERROR: (Типы занятий) %s\n",
                 m_db.error().c_str());
    std::fflush(stderr);
    return -1;
}
