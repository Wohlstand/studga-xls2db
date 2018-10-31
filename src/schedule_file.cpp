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

#include "schedule_file.h"
#include <cstdio>
#include <cstring>
#include <ctime>
#include <unordered_map>
#include <regex>
#include "Utils/strings.h"
#include "Utils/files.h"

#include "xl/xl_base.h"
#include "xl/xl_xls.h"
#include "xl/xl_xlsx.h"

ScheduleFile::ScheduleFile() = default;

ScheduleFile::~ScheduleFile() = default;

typedef const std::unordered_map<std::string, std::string> ScheduleWeekDays;
static ScheduleWeekDays g_weekdayNames = {
    {"П О Н Е Д Е Л Ь Н И К",   "1"},
    {"В Т О Р Н И К",           "2"},
    {"С Р Е Д А",               "3"},
    {"Ч Е Т В Е Р Г",           "4"},
    {"П Я Т Н И Ц А",           "5"},
    {"С У Б Б О Т А",           "6"},
    {"В О С К Р Е С Е Н Ь Е",   "0"}
};

#define ENABLE_LEGACY_COURATOR_HOUR

bool ScheduleFile::loadFromExcel(const std::string &path)
{
    int row_l, col_l;
    std::string errorInfo;

    m_orig_file = path;

    m_isInvalid = false;
    m_errorsList.clear();
    m_capturedEntries.clear();
    m_cache.clean();

    XlBase  *xl = nullptr;
    ReadXls  xls_r;
    ReadXlsX xls_x;
    if(ReadXls::isExcel97(path) && xls_r.load(path))
    {
        std::fprintf(stdout, "INFO: File %s loaded as XLS (97-2003)!\n", path.c_str());
        std::fflush(stdout);
        xl = &xls_r;
    }
    if(!xl && ReadXlsX::isExcelX(path) && xls_x.load(path))
    {
        std::fprintf(stdout, "INFO: File %s loaded as XLSX (2007+)!\n", path.c_str());
        std::fflush(stdout);
        xl = &xls_x;
    }

    if(xl)
    {
        int sheets = xl->sheetsCount();
        if(sheets < 2)
        {
            xl->close();
            std::ostringstream errOut;
            errOut << "WARNING: File " << path << " has less than two sheets!\n";
            addError(errOut.str());
            m_isInvalid = true;
            return false;
        }
        int sheetsSubGroups = (sheets - 2);
        std::vector<int> sheetsToFetch;
        sheetsToFetch.push_back(1);//Развёрнутое расписание, для распознания чётности недели
        sheetsToFetch.push_back(0);//Главная страница "Неделя"
        for(int i = 0; i < sheetsSubGroups; i++)
            sheetsToFetch.push_back(2 + i);//Подгруппы по лабораторным и по иностранному языку

        for(int sheet_id : sheetsToFetch)
        {
            if(!xl->chooseSheet(sheet_id))
                continue;

            //! Строка внутри одной записи
            int entryRow = 0;
            bool gotLectionNumber = false;
            int lastRow = xl->lastRow();
            int lastCol = xl->lastCol();
#ifdef ENABLE_LEGACY_COURATOR_HOUR
            bool legacy_hour_courator = false;
#endif

            // Очистим кэш-запись
            m_cache.clean();
            // Рассчитаем номер подгруппы по номеру листа
            m_cache.subgroup = sheet_id <= 1 ? -1 : (sheet_id - 1);

            for(row_l = 0; row_l <= lastRow; row_l++)
            {
                for (col_l = 0; col_l <= lastCol; col_l++)
                {
                    std::string cellstr = xl->getStrCell(row_l, col_l);
                    Strings::doTrim(cellstr);

                    // ==== Лист развёрнутого расписания ====
                    if(sheet_id == 1)
                    {
                        if(row_l == 0)
                            break;//Идти считывать следующую строку...

                        if(row_l == 1 && col_l == 0)
                        {
                            //Контрольная дата для отсчёта чётности
                            XlBase::Date date = xl->getDateCell(row_l, col_l);
                            if(date.day <= 0 || date.month <= 0 || date.year <= 0)
                            {
                                errorInfo = "Поле даты в развёрнутом расписании не правильное!";
                                xl->close();
                                goto invalidFormat;
                            }

                            m_baseDate.date_year = date.year;
                            m_baseDate.date_month = date.month;
                            m_baseDate.date_day = date.day;
                            m_baseDate.datePoint.resize(30);
                            int out = std::snprintf(&m_baseDate.datePoint[0], 30,
                                        "%04d-%02d-%02d",
                                        date.year,
                                        date.month,
                                        date.day);
                            m_baseDate.datePoint.resize((size_t)out);
                            continue;
                        }

                        if(row_l == 1 && col_l == 1)
                            continue;//Тут ничего интересного...

                        if(row_l == 1 && col_l == 2)
                        {
                            //Значение чётности на конкретную дату
                            m_baseDate.couple = cellstr;
                            // Прервать сканирование листа и продолжить чтение других данных
                            // Всё, мы нашли что хотели, валим с этого листа дальше...
                            row_l = lastRow + 1;
                            break;
                        }

                        continue;
                    }// ==== Лист развёрнутого расписания ====


                    //====== Далее "Неделя" и "подгруппы" ======

                    if((col_l == 0) && !cellstr.empty()) //номер пары
                    {
                        double dd = xl->getDoubleCell(row_l, col_l);
                        m_cache.number = dd != 0.0 ? std::to_string((int32_t)dd) : cellstr;
                        gotLectionNumber = true;
                    }

                    if(col_l == 1)  //чётность
                    {
                        if(!cellstr.empty())
                        {
                            m_cache.week_couple = cellstr;
                            gotLectionNumber = false;
                        }
                        else if(gotLectionNumber)
                        {
                            m_cache.week_couple = "";
                            gotLectionNumber = false;
                        }
                    }

                    if((col_l == 2) && !cellstr.empty()) //данные
                    {
                        if(entryRow == 0)
                        {
                            m_cache.disciplyne_name = cellstr;
#ifdef ENABLE_LEGACY_COURATOR_HOUR
                            if((m_cache.disciplyne_name.find("Час наставника") != std::string::npos) &&
                               (m_cache.disciplyne_name != "Час наставника"))
                            {
                                legacy_hour_courator = true;
                                m_cache.type = "Час наставника";
                                m_cache.disciplyne_name = "Час наставника";
                                std::string tmp = cellstr;
                                Strings::replaceAll(tmp, ",", ";");
                                Strings::replaceAll(tmp, " ", "");
                                Strings::replaceAll(tmp, "Часнаставника", "только ");
                                m_cache.date_condition = tmp;
                            }
                            else
                            {
                                legacy_hour_courator = false;
                            }
#endif //ENABLE_LEGACY_COURATOR_HOUR
                        }

                        if(entryRow == 1)
                        {
#ifdef ENABLE_LEGACY_COURATOR_HOUR
                            if(!legacy_hour_courator)
                                m_cache.type = cellstr;
#else //ENABLE_LEGACY_COURATOR_HOUR
                            m_cache.type = cellstr;
#endif //ENABLE_LEGACY_COURATOR_HOUR
                        }

#ifdef ENABLE_LEGACY_COURATOR_HOUR
#   define COURATOR_HOUR_CONDITION (entryRow == 2) && !legacy_hour_courator
#else //ENABLE_LEGACY_COURATOR_HOUR
#   define COURATOR_HOUR_CONDITION entryRow == 2
#endif //ENABLE_LEGACY_COURATOR_HOUR
                        if(COURATOR_HOUR_CONDITION)
                        {
                            m_cache.date_condition = cellstr;
                            //Если собран полный набор данных - записываем в базу!
                            commitCache();
                        }

                        if(entryRow != 2)
                            entryRow++;
                        else
                            entryRow = 0;
                    }
                    //Если обнаружено пустое поле с датой, пропустить эту запись
                    else if(cellstr.empty() && (col_l == 2) && (entryRow == 2))
                    {
                        entryRow = 0;
                    }

                    if((col_l == 4) && !cellstr.empty())
                    {
                        auto wd = g_weekdayNames.find(cellstr);
                        if(wd != g_weekdayNames.end())
                            m_cache.week_day = wd->second;
                        else
                        {
                            m_cache.lector_name = cellstr;
#ifdef ENABLE_LEGACY_COURATOR_HOUR
                            if(legacy_hour_courator)
                            {
                                size_t i = m_cache.lector_name.find('.');
                                if(i == std::string::npos)
                                {
                                    errorInfo = "Синтаксис записи \"Час куратора\" нарушен: поле преподавателя не верно";
                                    xl->close();
                                    goto invalidFormat;
                                }
                                std::string tmp = cellstr.substr(i + 1, (cellstr.size() - i));
                                m_cache.lector_name = tmp + "," + cellstr.substr(0, i);
                                //Если собран полный набор данных - записываем в базу!
                                commitCache();
                                entryRow = 0;
                            }
#endif //ENABLE_LEGACY_COURATOR_HOUR
                        }
                    }

                    if((col_l == 6) && !cellstr.empty())
                    {
                        m_cache.room_name = cellstr;
                    }
                }
            }
            xl->closeSheet();
        }
        xl->close();
        return true;
    }
    else
    {
        std::ostringstream errOut;
        errOut << "WARNING: Невозможно открыть файл " << path << "!\n";
        addError(errOut.str());
        m_isInvalid = true;
        return false;
    }

#ifdef ENABLE_LEGACY_COURATOR_HOUR
invalidFormat:
    {
        std::ostringstream errOut;
        errOut << "WARNING: Ошибка распознания данных в файле " << path << "! (" << errorInfo << ")\n";
        addError(errOut.str());
    }
    m_isInvalid = true;
    return false;
#endif //ENABLE_LEGACY_COURATOR_HOUR
}

bool ScheduleFile::isValid() const
{
    return !m_isInvalid;
}

std::string ScheduleFile::filePath() const
{
    return m_orig_file;
}

std::string ScheduleFile::fileName() const
{
    return Files::basename(m_orig_file);
}

const std::vector<std::string> &ScheduleFile::errorsList() const
{
    return m_errorsList;
}

const ScheduleFile::BaseDate &ScheduleFile::baseDate() const
{
    return m_baseDate;
}

const std::vector<ScheduleFile::OneDayData_Src> &ScheduleFile::entries() const
{
    return m_capturedEntries;
}

void ScheduleFile::addError(const std::string &str)
{
    m_errorsList.push_back(str);
    std::fprintf(stderr, "%s", str.c_str());
    std::fflush(stderr);
}

bool ScheduleFile::commitCache()
{
    OneDayData_Src tmp = m_cache;

    // Разделить имя преподавателя и учёное звание
    std::vector<std::string> l_and_r;
    Strings::split(l_and_r, tmp.lector_name, ',');
    if((tmp.lector_name.find(',') == std::string::npos) || (l_and_r.size() > 2))
    {
        std::ostringstream errOut;
        errOut << "WARNING: Подозрительное значение преподавателя [" << tmp.lector_name << "]!\n";
        addError(errOut.str());
        m_isInvalid = true;
        return false;
    }
    tmp.lector_name = Strings::trim(l_and_r[0]);
    tmp.lector_rank = Strings::trim(l_and_r.size() == 2 ? l_and_r[1] : "");

    if((tmp.subgroup >= 0) && (tmp.disciplyne_name.find("Иностранный язык") == std::string::npos))
    {
        //Добавить 2 к значению лабораторной подгруппы
        tmp.subgroup += 2;
    }

    // Проверить правильность формата условных дат
    if(!tmp.date_condition.empty())
    {
        bool is_valid = false;
        static const std::regex only_day(u8"только (\\d{1,2}\\.\\d{1,2}\\;?)+$");
        static const std::regex except(u8"кроме (\\d\\d\\.\\d\\d\\;?)+$");
        static const std::regex range(u8"с \\d\\d\\.\\d\\d\\ по \\d\\d\\.\\d\\d$");
        static const std::regex range_exc(u8"с \\d\\d\\.\\d\\d\\ по \\d\\d\\.\\d\\d\\ +кроме (\\d\\d\\.\\d\\d\\;?)+");
        is_valid |= std::regex_match(tmp.date_condition, only_day);
        is_valid |= std::regex_match(tmp.date_condition, except);
        is_valid |= std::regex_match(tmp.date_condition, range_exc);
        is_valid |= std::regex_match(tmp.date_condition, range);
        if(!is_valid)
        {
            std::ostringstream errOut;
            errOut << "WARNING: Подозрительное значение условной даты [" << tmp.date_condition << "]!";
            addError(errOut.str());
            m_isInvalid = true;
            return false;
        }
    }

    if(tmp.number.empty())
    {
        addError("WARNING: Отсутствует номер занятия!");
        m_isInvalid = true;
        return false;
    } else {
        static const std::regex numeric("^[\\d]+$");
        if(!std::regex_match(tmp.number, numeric))
        {
            std::ostringstream errOut;
            errOut << "WARNING: Номер занятия имеет несловой вид! [" << tmp.number << "]";
            addError(errOut.str());
            m_isInvalid = true;
            return false;
        }
    }


    if(tmp.week_day.empty())
    {
        addError("WARNING: Отсутствует день недели!\n");
        m_isInvalid = true;
        return false;
    } else {
        static std::regex numeric("^[\\d]+$");
        if(!std::regex_match(tmp.week_day, numeric))
            return false;
    }

    if(!tmp.week_couple.empty() && tmp.week_couple != "В" && tmp.week_couple != "Н")
    {
        std::ostringstream errOut;
        errOut << "WARNING: Неверное значение чётности [" << tmp.week_couple << "]!";
        addError(errOut.str());
        m_isInvalid = true;
        return false;
    }

    m_capturedEntries.push_back(tmp);
    return true;
}
