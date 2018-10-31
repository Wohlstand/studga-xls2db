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

#ifndef STUDGA_XLS2DB_XL_XLSX_H
#define STUDGA_XLS2DB_XL_XLSX_H

#include "xl_base.h"

#include <xlnt/xlnt.hpp>    //For Excel 2007+
#include <cstring>
#include <ctime>

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
    ~ReadXlsX() override;

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

        for(auto &i : xlsx_magic)
        {
            if(std::memcmp(buff, i, 4) == 0)
                return true;
        }

        return false;
    }

    bool load(const std::string &path) override
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

    bool chooseSheet(int sheet) override
    {
        ws = wb.sheet_by_index(static_cast<std::size_t>(sheet));
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
        return {};
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

#endif //STUDGA_XLS2DB_XL_XLSX_H
