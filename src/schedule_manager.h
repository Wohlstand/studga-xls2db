/*
 * MSTUCA Schedule from XLS to DataBase converter
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

#ifndef SCHEDULE_MANAGER_H
#define SCHEDULE_MANAGER_H

#include <cstring>
#include <unordered_map>
#include <utility>
#include "schedule_file.h"
#include "data_base.h"

extern bool g_fullDebugInfo;

class ScheduleManager
{
    DataBase m_db;
    std::string m_errorString;
    bool m_scheduleLockIsActive = false;
public:
    ScheduleManager();
    ~ScheduleManager();

    bool connectDataBase();
    std::string dbError();

    bool passScheduleFile(ScheduleFile &file);

    /**
     * @brief Заблокировать расписание на время обновления
     * @return true - если не было ошибок, false в случае возникновения любых ошибок
     */
    bool lockSchedule();
    /**
     * @brief Разблокировать расписание после обновления
     * @return true - если не было ошибок, false в случае возникновения любых ошибок
     */
    bool unlockSchedule();

    /**
     * @brief Оптимизировать главную таблицу
     * @return true - если не было ошибок, false в случае возникновения любых ошибок
     */
    bool optimizeMainTable();

    std::string errorString() const;

private:
    /**
     * @brief Студенческий поток
     */
    struct FlowInfo
    {
        //! Имя специальности
        std::string name;
        //! Номер курса на текущий момент
        int course = 0;
        //! Номер группы
        int group = 0;
        //! Год поступления
        int year_enter = 0;
        //! Год выпуска
        int year_quit = 0;
        //! ID в базе данных (заполняется после этапа синхронизации с БД)
        int db_id = -1;
    };

    // Only for pairs of std::hash-able types for simplicity.
    // You can of course template this struct to allow other hash functions
    struct pair_hash {
        template <class T1, class T2>
        std::size_t operator () (const std::pair<T1,T2> &p) const {
            auto h1 = std::hash<T1>{}(p.first);
            auto h2 = std::hash<T2>{}(p.second);

            // Mainly for demonstration purposes, i.e. works but is overly simple
            // In the real world, use sth. like boost.hash_combine
            return h1 ^ h2;
        }
    };

    template<class Zkey, class ZHash = std::hash<Zkey> >
    struct DbCache
    {
        bool contains(const Zkey &k)
        {
            return m_cachedRow.find(k) != m_cachedRow.end();
        }
        std::unordered_map<Zkey, DataBase::Row, ZHash> m_cachedRow;
    };

    struct ExInfo
    {
        ScheduleFile::OneDayData_Src src_data;

        struct Date
        {
            int year    = -1;
            int month   = -1;
            int day     = -1;

            bool operator==(const Date &o)
            {
                return std::memcmp(this, &o, sizeof(Date)) == 0;
            }

            bool operator>(const Date &o)
            {
                if(year == o.year)
                {
                    if(month == o.month)
                    {
                        if(day == o.day)
                            return false;
                        else
                            return day > o.day;
                    } else
                        return month > o.month;
                } else
                    return year > o.year;
            }

            bool operator<(const Date &o)
            {
                if(year == o.year)
                {
                    if(month == o.month)
                    {
                        if(day == o.day)
                            return false;
                        else
                            return day < o.day;
                    } else
                        return month < o.month;
                } else
                    return year < o.year;
            }

            bool operator>=(const Date &o)
            {
                if(year == o.year)
                {
                    if(month == o.month)
                    {
                        if(day == o.day)
                            return true;
                        else
                            return day > o.day;
                    } else
                        return month > o.month;
                } else
                    return year > o.year;
            }

            bool operator<=(const Date &o)
            {
                if(year == o.year)
                {
                    if(month == o.month)
                    {
                        if(day == o.day)
                            return true;
                        else
                            return day < o.day;
                    } else
                        return month < o.month;
                } else
                    return year < o.year;
            }
        };
        static std::string datesToStr(const std::vector<Date> &dates, ssize_t index = -1);

        //! Диапазон дат С-По
        std::vector<Date> date_range;
        //! Даты "Только"
        std::vector<Date> date_only;
        //! Даты "Кроме"
        std::vector<Date> date_except;
        //! Номер занятия (от 1 до 7)
        int number = 0;
        //! День недели
        int weekday = 0;
        //! Условие чётности (0 - любой, 1 - чёт, 2 - нечёт)
        int couple_cond = 0;
        //! ID семестра
        int id_daycount = -1;
        //! ID имени дисциплины
        int id_name     = -1;
        //! ID типа занятия
        int id_type     = -1;
        //! ID кабинета, в котором будет проходить занятие
        int id_room     = -1;
        //! ID преподавателя
        int id_lector   = -1;
        //! ID потока
        int id_flow     = -1;
        //! ID группы
        int id_group     = -1;
        //! Длительность пар
        int id_duraction = 1;
        //! ID подгруппы
        int id_subgrp = -1;
        //! Является ли это занятие в погруппах
        int is_subgroups = false;
    };

    /**
     * @brief extractFlowName Извлечь данные о потоке
     * @param in_path Путь к XLS-файлу
     * @param out_flow Структура для заполнения
     * @return true - если не было ошибок, false в случае возникновения любых ошибок
     */
    bool extractFlowName(std::string in_path, FlowInfo &out_flow,
                         const ScheduleFile::BaseDate &dateCouple);

    /**
     * @brief Синхронизировать данные структуры с базой данных
     * @param flow Пред-заполненная структура потока
     * @return true - если не было ошибок, false в случае возникновения любых ошибок
     */
    bool syncFlow(FlowInfo &flow);

    /**
     * @brief Преобразовать строчку в дату
     * @param dateStr Строка с датой
     * @param dateCouple Информация о чётности недели и базовая дата
     * @return Структура с датой и неназначенным годом
     */
    static ScheduleManager::ExInfo::Date strToDate(const std::string &dateStr,
                                                   const ScheduleFile::BaseDate &dateCouple);

    /**
     * @brief Инициализировать список занятий
     * @param file Файл расписания с исходными данными
     * @param out_list Список занятий
     * @return true - если не было ошибок, false в случае возникновения любых ошибок
     */
    bool buildExList(ScheduleFile &file, std::vector<ExInfo> &out_list);

    /**
     * @brief Обнаружить/создать семестр
     * @param out_list Список дней
     * @return true - если не было ошибок, false в случае возникновения любых ошибок
     */
    bool detectSemester(std::vector<ExInfo> &out_list, const ScheduleFile::BaseDate &baseDate);

    /**
     * @brief Синхронизировать связные данные с базой данных
     * @param inout_list Список дней
     * @param out_flow Структура потока
     * @return true - если не было ошибок, false в случае возникновения любых ошибок
     */
    bool syncData(std::vector<ExInfo> &inout_list, FlowInfo &out_flow);

    /**
     * @brief Записать все данные в базу данных
     * @param inout_list Список дней
     * @return true - если не было ошибок, false в случае возникновения любых ошибок
     */
    bool writeToDb(std::vector<ExInfo> &inout_list, FlowInfo &out_flow);


    DbCache<std::pair<int /*Year*/, bool /*isAutumn*/ >, pair_hash> m_daycounts;
    int getSemesterId(int year, bool isAutumn, bool couples);

    DbCache<std::pair<std::string /*Lector*/, std::string/*Rank*/ >, pair_hash> m_lectors;
    int getLectorId(const std::string &name, const std::string &rank);

    DbCache<std::string> m_disciplynes;
    int getDisciplyneId(const std::string &disciplyne);

    DbCache<std::string> m_rooms;
    int getRoomId(const std::string &roomName);

    DbCache<std::string> m_ltypes;
    int getLTypeId(const std::string &ltypeName);

};

#endif // SCHEDULE_MANAGER_H
