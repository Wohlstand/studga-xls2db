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

#include <sstream>
#include <smtp_client.h>
#include "report_mailer.h"

#include "../smtp_login.h"

ReportEmailer::ReportEmailer() = default;

ReportEmailer::~ReportEmailer()
{
    m_entries.clear();
}

void ReportEmailer::addFileWithErrors(ScheduleFile &file, std::string extraErrorString)
{
    if(file.isValid() && extraErrorString.empty())
        return; // Пропустить правильные файлы

    ReportEntry entry;
    entry.fileName   = file.fileName();
    entry.filePath   = file.filePath();
    entry.errorsList = file.errorsList();
    entry.baseDate   = file.baseDate();
    if(!extraErrorString.empty())
        entry.errorsList.push_back(extraErrorString);

    m_entries.push_back(entry);
}

bool ReportEmailer::sendTestLetter()
{
    SMTP_Client *smtp;
    std::string subject = "Оно живое!";
    std::string body = "<h1>Привет от Лисят! :3</h1>\n";
    if(smtp_init(&smtp) < 0)
    {
        fprintf(stderr, "Ошибка инициализации SMTP_Client-а!\n");
        return false;
    }

    smtp_createLetter(smtp, SMTP_TextHTML,
                      SMTP_NAMEFROM, SMTP_MAILFROM,
                      SMTP_NAMETO, SMTP_MAILTO,
                      subject.c_str(),
                      body.c_str());
    smtp_endLetter(smtp);

    if(smtp_connect(smtp, SMTP_HOST, SMTP_PORT, (SMTP_USE_SSL ? SMTP_SSL : SMTP_NONSECURE)) < 0)
    {
        fprintf(stderr, "Ошибка подключения к SMTP-серверу! [%s]\n", smtp->errorString);
        smtp_free(&smtp);
        return false;
    }
    if(smtp_login(smtp, SMTP_USER, SMTP_PASS) < 0)
    {
        fprintf(stderr, "Ошибка авторизации! [%s]\n", smtp->errorString);
        smtp_free(&smtp);
        return false;
    }

    if(smtp_sendLetter(smtp) < 0)
    {
        fprintf(stderr, "Ошибка отправки письма! [%s]\n", smtp->errorString);
        smtp_free(&smtp);
        return false;
    }

    smtp_free(&smtp);
    return true;
}

void ReportEmailer::sendErrorsReport()
{
    if(m_entries.empty())
        return; // Нечего слать :-P

    fprintf(stdout, "Отправка отчёта об ошибках по email...\n");

    SMTP_Client *smtp;

    std::string subject = "Обнаружены Excel-файлы с ошибками!";
    std::ostringstream body;

    body << "<h1>Обнаружены файлы с ошибками!</h1>\n";
    for(ReportEntry &e : m_entries)
    {
        body << "<p>\n";
        body << "<b>Имя файла:</b> " << e.fileName << "<br>\n";
        body << "<b>Базовая дата:</b> " << e.baseDate.date_year << "-"
                                        << e.baseDate.date_month << "-"
                                        << e.baseDate.date_day << "<br>\n";
        body << "<b>Список ошибок:</b><br>\n";
        body << "<ul>\n";
        for(std::string &err : e.errorsList)
            body << "<li>" << err << "</li>\n";
        body << "</ul>\n";
        body << "</p><br>\n";
    }

    if(smtp_init(&smtp) < 0)
    {
        fprintf(stderr, "Ошибка инициализации SMTP_Client-а!\n");
        return;
    }

    smtp_createLetter(smtp, SMTP_TextHTML,
                      SMTP_NAMEFROM, SMTP_MAILFROM,
                      SMTP_NAMETO, SMTP_MAILTO,
                      subject.c_str(),
                      body.str().c_str());

    // Вложить все файлы с ошибками
    for(ReportEntry &e : m_entries)
        smtp_attachFile(smtp, e.filePath.c_str());

    smtp_endLetter(smtp);

    if(smtp_connect(smtp, SMTP_HOST, SMTP_PORT, (SMTP_USE_SSL ? SMTP_SSL : SMTP_NONSECURE)) < 0)
    {
        fprintf(stderr, "Ошибка подключения к SMTP-серверу! [%s]\n", smtp->errorString);
        smtp_free(&smtp);
        return;
    }
    if(smtp_login(smtp, SMTP_USER, SMTP_PASS) < 0)
    {
        fprintf(stderr, "Ошибка авторизации! [%s]\n", smtp->errorString);
        smtp_free(&smtp);
        return;
    }

    if(smtp_sendLetter(smtp) < 0)
    {
        fprintf(stderr, "Ошибка отправки письма! [%s]\n", smtp->errorString);
        smtp_free(&smtp);
        return;
    }

    smtp_free(&smtp);
}
