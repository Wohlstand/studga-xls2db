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

#ifndef STUDGA_XLS2DB_XL_XLS_H
#define STUDGA_XLS2DB_XL_XLS_H

#include "xl_base.h"

#include <xls.h>            //For Excel 97-2003
#include <cstring>
#include <ctime>

/**
 * @brief Обёртка над парсером файлов XLS (97-2003)
 */
class ReadXls final : public XlBase
{
    xls::xlsWorkBook*  pWB = nullptr;
    xls::xlsWorkSheet* pWS = nullptr;

    static bool isCellEmpty(xls::st_cell::st_cell_data &cell)
    {
        return (cell.str == nullptr) || cell.str[0] == '\0';
    }
public:
    ReadXls() : XlBase() {}
    ~ReadXls() override;

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

        return (std::memcmp(buff, xls_magic, 8) == 0);
    }

    bool load(const std::string &path) override
    {
        if(pWB)
            close();
        pWB = xls::xls_open(path.c_str(), "UTF-8");
        return (pWB != nullptr);
    }

    void close() override
    {
        closeSheet();
        if(pWB)
            xls::xls_close_WB(pWB);
        pWB = nullptr;
    }

    int sheetsCount() override
    {
        if(!pWB)
            return 0;
        return (int)pWB->sheets.count;
    }

    bool chooseSheet(int sheet) override
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
            return {};

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

#endif //STUDGA_XLS2DB_XL_XLS_H
