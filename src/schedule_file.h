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
    struct BaseDate
    {
        std::string datePoint;
        int date_year   = -1;
        int date_month  = -1;
        int date_day    = -1;
        std::string couple;
    };
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
        int subgroup = -1;

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
            subgroup = -1;
        }
    };

    bool isValid() const;
    std::string filePath() const;
    std::string fileName() const;
    const std::vector<std::string> &errorsList() const;
    const BaseDate &baseDate() const;
    const std::vector<OneDayData_Src> &entries() const;

private:

    //! Путь к исходному файлу
    std::string                 m_orig_file;
    //! Кэш-запись, копит собранные данные полей перед отправкой снимка в общий список
    OneDayData_Src              m_cache;
    //! Базовый год расписания
    int                         m_baseYear;
    //! Базовая дата
    BaseDate                    m_baseDate;
    //! Список собранных записей
    std::vector<OneDayData_Src> m_capturedEntries;
    //! Была ли обнаружена ошибка при считвании или проверки данных
    bool                        m_isInvalid = false;
    //! Список ошибок
    std::vector<std::string>    m_errorsList;
    //! Проверить кэш и отправить кэш-запись в массив
    bool commitCache();
};

#endif // SCHEDULE_FILE_H
