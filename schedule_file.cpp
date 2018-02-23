#include "schedule_file.h"
#include <xls.h>            //For Excel 97-2003
#include <xlnt/xlnt.hpp>    //For Excel 2007+
#include <cstdio>
#include <unordered_map>
#include <regex>
#include "strings.h"

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

    virtual int  countRows() = 0;
    virtual int  countCols() = 0;
    virtual int  lastRow() = 0;
    virtual int  lastCol() = 0;
    virtual std::string getStrCell(int row, int col) = 0;
    virtual long getLongCell(int row, int col) = 0;
    virtual double getDoubleCell(int row, int col) = 0;
};

XlBase::~XlBase()
{}

static bool isCellEmpty(xls::st_cell::st_cell_data &cell)
{
    return (cell.str == NULL) || cell.str[0] == '\0';
}

class ReadXls final:
        public XlBase
{
    xls::xlsWorkBook*  pWB = nullptr;
    xls::xlsWorkSheet* pWS = nullptr;
public:
    ReadXls() : XlBase() {}
    ~ReadXls();

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






class ReadXlsX final:
        public XlBase
{
    xlnt::workbook  wb;
    xlnt::worksheet ws;
public:
    ReadXlsX() : XlBase() {}
    ~ReadXlsX();

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
        ws = wb.sheet_by_index((size_t)sheet);
        return true;
    }

    void closeSheet() override
    {}

    int  countRows() override
    {
        return (int)ws.rows(false).reference().width();
    }
    int  countCols() override
    {
        return (int)ws.columns(false).reference().height();
    }

    int  lastRow() override
    {
        return (int)ws.rows(false).reference().width() - 1;
    }

    int  lastCol() override
    {
        return (int)ws.columns(false).reference().height() - 1;
    }

    std::string getStrCell(int row, int col) override
    {
        auto current_cell = xlnt::cell_reference("A1");
        return ws.cell(current_cell.column() + (unsigned int)col, current_cell.row() + (unsigned int)row).to_string();
    }

    long getLongCell(int row, int col) override
    {
        auto current_cell = xlnt::cell_reference("A1");
        return (long)ws.cell(current_cell.column() + (unsigned int)col, current_cell.row() + (unsigned int)row).value<int>();
    }

    double getDoubleCell(int row, int col) override
    {
        auto current_cell = xlnt::cell_reference("A1");
        return ws.cell(current_cell.column() + (unsigned int)col, current_cell.row() + (unsigned int)row).value<double>();
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

    is_invalid = false;
    m_errorsList.clear();
    m_capturedEntries.clear();
    m_cache.clean();

    XlBase *xl = nullptr;
    ReadXls xlsr;
    ReadXlsX xlsx;
    if(xlsr.load(path))
        xl = &xlsr;
    if(!xl && xlsx.load(path))
    {
        std::fprintf(stderr, "INFO: File %s loaded as XLSX!\n", path.c_str());
        std::fflush(stdout);
        xl = &xlsx;
    }

    if(xl)
    {
        int sheets = xl->sheetsCount();
        if(sheets < 2)
        {
            xl->close();
            std::fprintf(stderr, "WARNING: File %s has less than two sheets!\n", path.c_str());
            std::fflush(stdout);
            return true;
        }
        int sheetsSubGrps = sheets - 2;
        std::vector<int> sheetsToFetch;
        sheetsToFetch.push_back(0);
        for(int i = 0; i < sheetsSubGrps; i++)
            sheetsToFetch.push_back(2 + i);

        for(int sheet_id : sheetsToFetch)
        {
            if(!xl->chooseSheet(sheet_id))
                continue;
            int rowof = 0;
            bool LectionNumber = false;
            int lastRow = xl->lastRow();
            int lastCol = xl->lastCol();
            #ifdef ENABLE_LEGACY_COURATOR_HOUR
            bool legacy_hour_courator = false;
            #endif

            m_cache.clean();
            m_cache.subgroup = sheet_id == 0 ? -1 : (int)(sheet_id - 2);

            for(rowl = 0; rowl <= lastRow; rowl++)
            {
                for (coll = 0; coll <= lastCol; coll++)
                {
                    std::string cellstr = xl->getStrCell(rowl, coll);
                    Strings::doTrim(cellstr);

                    if((coll == 0) && !cellstr.empty()) //номер пары
                    {
                        double dd = xl->getDoubleCell(rowl, coll);
                        m_cache.number = dd != 0.0 ? std::to_string((int32_t)dd) : cellstr;
                        LectionNumber = true;
                    }

                    if(coll == 1)  //чётность
                    {
                        if(!cellstr.empty())
                        {
                            m_cache.week_couple = cellstr;
                            LectionNumber = false;
                        }
                        else if(LectionNumber)
                        {
                            m_cache.week_couple = "";
                            LectionNumber = false;
                        }
                    }

                    if((coll == 2) && !cellstr.empty())//данные
                    {
                        if(rowof == 0)
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

                        if(rowof == 1)
                        {
                            #ifdef ENABLE_LEGACY_COURATOR_HOUR
                            if(!legacy_hour_courator)
                            #endif
                                m_cache.type = cellstr;
                        }

                        #ifdef ENABLE_LEGACY_COURATOR_HOUR
                        if(rowof == 2 && !legacy_hour_courator)
                        #else
                        if(rowof == 2)
                        #endif
                        {
                            m_cache.date_condition = cellstr;
                            //Если собран полный набор данных - записываем в базу!
                            commitCache();
                        }

                        if(rowof != 2)
                            rowof++;
                        else
                            rowof = 0;
                    }
                    else
                    //Если обнаружено пустое поле с датой, пропустить эту запись
                    if(cellstr.empty() && (coll == 2) && (rowof == 2))
                    {
                        rowof = 0;
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
                                rowof = 0;
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
        std::fprintf(stderr, "WARNING: Can't open %s file!\n", path.c_str());
        std::fflush(stderr);
        return false;
    }

#ifdef ENABLE_LEGACY_COURATOR_HOUR
invalidFormat:
    m_errorsList.push_back("WARNING: Error while parsing file!");
    std::fprintf(stderr, "WARNING: Error while parsing %s file! (%s)\n", path.c_str(), errorInfo.c_str());
    std::fflush(stdout);
    return false;
#endif
}

bool ScheduleFile::commitCache()
{
    OneDayData_Src tmp = m_cache;

    // Разделить имя преподавателя и учёное звание
    std::vector<std::string> l_and_r;
    Strings::split(l_and_r, tmp.lector_name, ',');
    if((tmp.lector_name.find(',') == std::string::npos) || (l_and_r.size() > 2))
    {
        std::fprintf(stderr, "WARNING: Подозрительное значение преподавателя [%s]!\n", tmp.lector_name.c_str());
        std::fflush(stdout);
        return false;
    }
    tmp.lector_name = Strings::trim(l_and_r[0]);
    tmp.lector_rank = Strings::trim(l_and_r.size() == 2 ? l_and_r[1] : "");

    if(tmp.disciplyne_name.find("Иностранный язык") == std::string::npos)
    {
        //Добавить 2 к значению лабораторной подгруппы
        tmp.subgroup += 2;
    }

    // Проверить правильность формата дат
    if(!tmp.date_condition.empty())
    {
        bool is_valid = false;
        static std::regex onlyday("только (\\d{1,2}\\.\\d{1,2}\\;?)+$");
        static std::regex except("кроме (\\d\\d\\.\\d\\d\\;?)+$");
        static std::regex range("с \\d\\d\\.\\d\\d\\ по \\d\\d\\.\\d\\d$");
        static std::regex range_exc("с \\d\\d\\.\\d\\d\\ по \\d\\d\\.\\d\\d\\ +кроме (\\d\\d\\.\\d\\d\\;?)+");
        is_valid |= std::regex_match(tmp.date_condition, onlyday);
        is_valid |= std::regex_match(tmp.date_condition, except);
        is_valid |= std::regex_match(tmp.date_condition, range_exc);
        is_valid |= std::regex_match(tmp.date_condition, range);
        if(!is_valid)
        {
            m_errorsList.push_back("WARNING: Подозрительное значение условной даты [" + tmp.date_condition + "]!");
            std::fprintf(stderr, "WARNING: Подозрительное значение условной даты [%s]!\n", tmp.date_condition.c_str());
            std::fflush(stdout);
            return false;
        }
    }

    if(tmp.number.empty())
    {
        m_errorsList.push_back("WARNING: Отсутствует номер занятия!");
        std::fprintf(stderr, "WARNING: Отсутствует номер занятия!\n");
        std::fflush(stdout);
        is_invalid = true;
        return false;
    } else {
        static std::regex numeric("^[\\d]+$");
        if(!std::regex_match(tmp.number, numeric))
            return false;
    }


    if(tmp.week_day.empty())
    {
        m_errorsList.push_back("WARNING: Отсутствует день недели!");
        std::fprintf(stderr, "WARNING: Отсутствует день недели!\n");
        std::fflush(stdout);
        is_invalid = true;
        return false;
    } else {
        static std::regex numeric("^[\\d]+$");
        if(!std::regex_match(tmp.week_day, numeric))
            return false;
    }

    if(!tmp.week_couple.empty() && tmp.week_couple != "В" && tmp.week_couple != "Н")
    {
        m_errorsList.push_back("WARNING: Неверное значение чётности [" + tmp.week_couple + "]!");
        std::fprintf(stderr, "WARNING: Неверное значение чётности [%s]!\n", tmp.week_couple.c_str());
        std::fflush(stdout);
        is_invalid = true;
        return false;
    }

    m_capturedEntries.push_back(tmp);
    return true;
}
