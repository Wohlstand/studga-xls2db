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
#include <xls.h>            //For Excel 97-2003
#include <xlnt/xlnt.hpp>    //For Excel 2007+
#include <cstdio>
#include <cstring>
#include <ctime>
#include <unordered_map>
#include <regex>
#include "Utils/strings.h"
#include "Utils/files.h"

/**
 * @brief База для XLS и XLSX парсеров
 */
class XlBase
{
public:
    XlBase() {}
    virtual ~XlBase();
    virtual bool load(const std::string path) = 0;
    virtual void close() = 0;

    virtual int  sheetsCount() = 0;
    virtual bool chooseSheet(const int sheet) = 0;
    virtual void closeSheet() = 0;

    struct Date
    {
        int year = -1;
        int month = -1;
        int day = -1;
    };

    virtual int  countRows() = 0;
    virtual int  countCols() = 0;
    virtual int  lastRow() = 0;
    virtual int  lastCol() = 0;
    virtual std::string getStrCell(int row, int col) = 0;
    virtual Date getDateCell(int row, int col) = 0;
    virtual long getLongCell(int row, int col) = 0;
    virtual double getDoubleCell(int row, int col) = 0;
};

XlBase::~XlBase()
{}




/**
 * @brief Обёртка над парсером файлов XLS (97-2003)
 */
class ReadXls final:
        public XlBase
{
    xls::xlsWorkBook*  pWB = nullptr;
    xls::xlsWorkSheet* pWS = nullptr;

    static bool isCellEmpty(xls::st_cell::st_cell_data &cell)
    {
        return (cell.str == NULL) || cell.str[0] == '\0';
    }
public:
    ReadXls() : XlBase() {}
    ~ReadXls();

    static bool isExcel97(const std::string &path)
    {
        static const char *xls_magic = "\xD0\xCF\x11\xE0\xA1\xB1\x1A\xE1";
        char buff[8];
        FILE *f = fopen(path.c_str(), "rb");
        if(!f)
            return false;

        if(fread(buff, 1, 8, f) != 8)
        {
            fclose(f);
            return false;
        }
        fclose(f);

        if(std::memcmp(buff, xls_magic, 8) == 0)
            return true;

        return false;
    }

    bool load(const std::string path) override
    {
        if(pWB)
            close();
        pWB = xls::xls_open(path.c_str(), "UTF-8");
        if (pWB != NULL)
            return true;
        return false;
    }

    void close() override
    {
        closeSheet();
        if(pWB)
            xls::xls_close_WB(pWB);
        pWB = nullptr;
    }

    int  sheetsCount() override
    {
        if(!pWB)
            return 0;
        return (int)pWB->sheets.count;
    }

    bool chooseSheet(const int sheet) override
    {
        if(!pWB)
            return false;
        closeSheet();
        pWS = xls::xls_getWorkSheet(pWB, sheet);
        xls::xls_parseWorkSheet(pWS);
        return true;
    }

    void closeSheet() override
    {
        if(pWS)
            xls::xls_close_WS(pWS);
        pWS = nullptr;
    }

    int  countRows() override
    {
        if(!pWS)
            return 0;
        return pWS->rows.lastrow + 1;
    }
    int  countCols() override
    {
        if(!pWS)
            return 0;
        return pWS->rows.lastcol + 1;
    }

    int  lastRow() override
    {
        if(!pWS)
            return 0;
        return pWS->rows.lastrow;
    }

    int  lastCol() override
    {
        if(!pWS)
            return 0;
        return pWS->rows.lastcol;
    }

    std::string getStrCell(int row, int col) override
    {
        xls::st_row::st_row_data* p_row = &pWS->rows.row[row];
        xls::st_cell::st_cell_data &p_cell = p_row->cells.cell[col];
        return isCellEmpty(p_cell) ? std::string("") : std::string((char*)p_cell.str);
    }

    Date getDateCell(int row, int col) override
    {
        xls::st_row::st_row_data* p_row = &pWS->rows.row[row];
        xls::st_cell::st_cell_data &p_cell = p_row->cells.cell[col];

        if(isCellEmpty(p_cell))
            return Date();

        // initialize
        int y = 1899, m = 12, d = 31;
        std::tm t = {};
        t.tm_year = y - 1900;
        t.tm_mon  = m - 1;
        t.tm_mday = d;
        // modify
        t.tm_mday += ((int)p_cell.d) - 1;
        std::mktime(&t);

        Date out;
        out.year = 1900 + t.tm_year;
        out.month =   1 + t.tm_mon;
        out.day =         t.tm_mday;

        char buffer[30];
        std::strftime(buffer, 30, "%Y-%m-%d", &t);
        return out;
    }

    long getLongCell(int row, int col) override
    {
        xls::st_row::st_row_data* p_row = &pWS->rows.row[row];
        xls::st_cell::st_cell_data &p_cell = p_row->cells.cell[col];
        return (long)p_cell.l;
    }

    double getDoubleCell(int row, int col) override
    {
        xls::st_row::st_row_data* p_row = &pWS->rows.row[row];
        xls::st_cell::st_cell_data &p_cell = p_row->cells.cell[col];
        return p_cell.d;
    }
};

ReadXls::~ReadXls()
{
    close();
}




/**
 * @brief Обёртка над парсером файлов XLSX (2007+)
 */
class ReadXlsX final:
        public XlBase
{
    xlnt::workbook          wb;
    xlnt::worksheet         ws;
public:
    ReadXlsX() : XlBase() {}
    ~ReadXlsX();

    static bool isExcelX(const std::string &path)
    {
        static const char *xlsx_magic[3] =
        {
            "\x50\x4B\x03\x04",
            "\x50\x4B\x05\x06",
            "\x50\x4B\x07\x08"
        };
        char buff[8];
        FILE *f = fopen(path.c_str(), "rb");
        if(!f)
            return false;

        if(fread(buff, 1, 4, f) != 4)
        {
            fclose(f);
            return false;
        }
        fclose(f);

        for(int i = 0; i < 3; i++)
        {
            if(std::memcmp(buff, xlsx_magic[i], 4) == 0)
                return true;
        }

        return false;
    }

    bool load(const std::string path) override
    {
        try {
            wb.load(path);
        } catch (const xlnt::exception &e) {
            std::fprintf(stderr, "Can't load file %s because of exception %s!\n", path.c_str(), e.what());
            std::fflush(stdout);
            return false;
        }
        catch (...) {
            std::fprintf(stderr, "Can't load file %s because of unknown exception!\n", path.c_str());
            std::fflush(stdout);
            return false;
        }
        return true;
    }

    void close() override
    {
        closeSheet();
        wb.clear();
    }

    int  sheetsCount() override
    {
        return (int)wb.sheet_count();
    }

    bool chooseSheet(const int sheet) override
    {
        ws = wb.sheet_by_index((std::size_t)sheet);
        return true;
    }

    void closeSheet() override
    {}

    int  countRows() override
    {
        return (int)ws.rows(false).reference().height();
    }
    int  countCols() override
    {
        return (int)ws.columns(false).reference().width();
    }

    int  lastRow() override
    {
        return (int)ws.rows(false).reference().height() - 1;
    }

    int  lastCol() override
    {
        return (int)ws.columns(false).reference().width() - 1;
    }

    std::string getStrCell(int row, int col) override
    {
        return ws.cell((xlnt::column_t::index_t)col + 1, (xlnt::row_t)row + 1).to_string();
    }

    Date getDateCell(int row, int col) override
    {
        auto cell = ws.cell((xlnt::column_t::index_t)col + 1, (xlnt::row_t)row + 1);
        if(cell.is_date())
        {
            xlnt::date dt = cell.value<xlnt::date>();
            Date d;
            d.year = dt.year;
            d.month = dt.month;
            d.day = dt.day;
            return d;
        }
        return Date();
    }

    long getLongCell(int row, int col) override
    {
        return (long)ws.cell((xlnt::column_t::index_t)col + 1, (xlnt::row_t)row + 1).value<int>();
    }

    double getDoubleCell(int row, int col) override
    {
        return ws.cell((xlnt::column_t::index_t)col + 1, (xlnt::row_t)row + 1).value<double>();
    }
};

ReadXlsX::~ReadXlsX()
{
    close();
}



ScheduleFile::ScheduleFile()
{}

ScheduleFile::~ScheduleFile()
{}

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
    int rowl, coll;
    std::string errorInfo;

    m_orig_file = path;

    m_isInvalid = false;
    m_errorsList.clear();
    m_capturedEntries.clear();
    m_cache.clean();

    XlBase *xl = nullptr;
    ReadXls xlsr;
    ReadXlsX xlsx;
    if(ReadXls::isExcel97(path) && xlsr.load(path))
    {
        std::fprintf(stdout, "INFO: File %s loaded as XLS (97-2003)!\n", path.c_str());
        std::fflush(stdout);
        xl = &xlsr;
    }
    if(!xl && ReadXlsX::isExcelX(path) && xlsx.load(path))
    {
        std::fprintf(stdout, "INFO: File %s loaded as XLSX (2007+)!\n", path.c_str());
        std::fflush(stdout);
        xl = &xlsx;
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
        int sheetsSubGrps = sheets - 2;
        std::vector<int> sheetsToFetch;
        sheetsToFetch.push_back(1);//Развёрнутое расписание, для распознания чётности недели
        sheetsToFetch.push_back(0);//Главная страница "Неделя"
        for(int i = 0; i < sheetsSubGrps; i++)
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

            for(rowl = 0; rowl <= lastRow; rowl++)
            {
                for (coll = 0; coll <= lastCol; coll++)
                {
                    std::string cellstr = xl->getStrCell(rowl, coll);
                    Strings::doTrim(cellstr);

                    // ==== Лист развёрнутого расписания ====
                    if(sheet_id == 1)
                    {
                        if(rowl == 0)
                            break;//Идти считывать следующую строку...
                        if(rowl == 1 && coll == 0)
                        {
                            //Контрольная дата для отсчёта чётности
                            XlBase::Date date = xl->getDateCell(rowl, coll);
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
                        if(rowl == 1 && coll == 1)
                            continue;//Тут ничего интересного...
                        if(rowl == 1 && coll == 2)
                        {
                            //Значение чётности на конкретную дату
                            m_baseDate.couple = cellstr;
                            // Прервать сканирование листа и продолжить чтение других данных
                            // Всё, мы нашли что хотели, валим с этого листа дальше...
                            rowl = lastRow + 1;
                            break;
                        }
                        continue;
                    }// ==== Лист развёрнутого расписания ====


                    //====== Далее "Неделя" и "подгруппы" ======

                    if((coll == 0) && !cellstr.empty()) //номер пары
                    {
                        double dd = xl->getDoubleCell(rowl, coll);
                        m_cache.number = dd != 0.0 ? std::to_string((int32_t)dd) : cellstr;
                        gotLectionNumber = true;
                    }

                    if(coll == 1)  //чётность
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

                    if((coll == 2) && !cellstr.empty())//данные
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
                            } else
                                legacy_hour_courator = false;
                            #endif
                        }

                        if(entryRow == 1)
                        {
                            #ifdef ENABLE_LEGACY_COURATOR_HOUR
                            if(!legacy_hour_courator)
                            #endif
                                m_cache.type = cellstr;
                        }

                        #ifdef ENABLE_LEGACY_COURATOR_HOUR
                        if(entryRow == 2 && !legacy_hour_courator)
                        #else
                        if(entryRow == 2)
                        #endif
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
                    else
                    //Если обнаружено пустое поле с датой, пропустить эту запись
                    if(cellstr.empty() && (coll == 2) && (entryRow == 2))
                    {
                        entryRow = 0;
                    }

                    if((coll == 4) && !cellstr.empty())
                    {
                        ScheduleWeekDays::const_iterator wd = g_weekdayNames.find(cellstr);
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
                            #endif
                        }
                    }

                    if((coll == 6) && !cellstr.empty())
                    {
                        m_cache.room_name = cellstr;
                    }
                }
            }
            xl->closeSheet();
        }
        xl->close();
        return true;
    } else {
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
#endif
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
        static const std::regex onlyday(u8"только (\\d{1,2}\\.\\d{1,2}\\;?)+$");
        static const std::regex except(u8"кроме (\\d\\d\\.\\d\\d\\;?)+$");
        static const std::regex range(u8"с \\d\\d\\.\\d\\d\\ по \\d\\d\\.\\d\\d$");
        static const std::regex range_exc(u8"с \\d\\d\\.\\d\\d\\ по \\d\\d\\.\\d\\d\\ +кроме (\\d\\d\\.\\d\\d\\;?)+");
        is_valid |= std::regex_match(tmp.date_condition, onlyday);
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
