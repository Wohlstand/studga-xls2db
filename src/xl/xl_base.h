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

#ifndef STUDGA_XLS2DB_XL_BASE_H
#define STUDGA_XLS2DB_XL_BASE_H

#include <string>

/**
 * @brief База для XLS и XLSX парсеров
 */
class XlBase
{
public:
    XlBase() = default;
    virtual ~XlBase();
    virtual bool load(const std::string &path) = 0;
    virtual void close() = 0;

    virtual int  sheetsCount() = 0;
    virtual bool chooseSheet(int sheet) = 0;
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

XlBase::~XlBase() = default;

#endif //STUDGA_XLS2DB_XL_BASE_H
