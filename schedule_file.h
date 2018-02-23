#ifndef SCHEDULE_FILE_H
#define SCHEDULE_FILE_H

#include <string>
#include <vector>

class ScheduleFile
{
public:
    ScheduleFile();
    virtual ~ScheduleFile();

    bool loadFromExcel(const std::string &path);
    struct OneDayData_Src
    {
        //! День недели
        std::string week_day;
        //! Чётность недели
        std::string week_couple;
        //! Номер пары
        std::string number;

        //! Название предмета
        std::string disciplyne_name;
        //! Тип занятия (Лекция, практика, лабораторная, и т.п.)
        std::string type;

        //! Преподаватель
        std::string lector_name;
        //! Учёное звание (доцент, профессор, преподаватель, и т.п.)
        std::string lector_rank;

        //! Имя комнаты проведения занятия
        std::string room_name;

        //! Даты-условие проведения занятия (период, конкретные дни, дни-исключения)
        std::string date_condition;

        //! Подгруппа
        int subgroup = 0;

        void clean()
        {
            week_day.clear();
            week_couple.clear();
            number.clear();
            disciplyne_name.clear();
            type.clear();
            lector_name.clear();
            lector_rank.clear();
            room_name.clear();
            date_condition.clear();
            subgroup = 0;
        }
    };

    std::string         m_orig_file;
    OneDayData_Src      m_cache;

    bool commitCache();
    std::vector<OneDayData_Src> m_capturedEntries;
    bool is_invalid = false;
    std::vector<std::string> m_errorsList;
};

#endif // SCHEDULE_FILE_H
