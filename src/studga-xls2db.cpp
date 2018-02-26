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

#include <openssl/md5.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>

#include <stdio.h>
#include <cstring>
#include <dirman.h>
#include "Utils/files.h"
#include "schedule_file.h"
#include "schedule_manager.h"

// Get the size of the file by its file descriptor
static size_t get_size_by_fd(int fd) {
    struct stat statbuf;
    if(fstat(fd, &statbuf) < 0) exit(-1);
    return (size_t)statbuf.st_size;
}

bool file_md5sum(const std::string &path, unsigned char *result)
{
    int file_descript;
    size_t file_size;
    char* file_buffer;
    file_descript = open(path.c_str(), O_RDONLY);
    if(file_descript < 0)
        return false;
    file_size = get_size_by_fd(file_descript);

    file_buffer = (char*)mmap(0, file_size, PROT_READ, MAP_SHARED, file_descript, 0);
    MD5((unsigned char*) file_buffer, file_size, result);
    munmap(file_buffer, file_size);
    return true;
}

bool sameFiles(const std::string &file1, const std::string &file2)
{
    unsigned char result1[MD5_DIGEST_LENGTH];
    unsigned char result2[MD5_DIGEST_LENGTH];
    if(!Files::fileExists(file1))
        return false;
    if(!Files::fileExists(file2))
        return false;
    if(!file_md5sum(file1, result1))
        return false;
    if(!file_md5sum(file2, result2))
        return false;
    return std::memcmp(result1, result2, MD5_DIGEST_LENGTH) == 0;
}

int main()
{
    DirMan dir;
    dir.setPath(DIR_EXCELS_ROOT "/" DIR_EXCELS_NEW_CACHE);
    std::vector<std::string> filters;
    filters.push_back(".xls");

    //if(dir.beginWalking(filters))
    {
        std::string curPath;
        ScheduleManager manager;
        if(manager.connectDataBase())
        {
            // Заблокировать расписание для просмотра
            if(!manager.lockSchedule())
            {
                std::fprintf(stderr, "\n");
                std::fprintf(stderr, "===============================================================================\n");
                std::fprintf(stderr, "ОШИБКА БЛОКИРОВКИ %s\n", manager.errorString().c_str());
                std::fprintf(stderr, "===============================================================================\n");
                std::fflush(stderr);
                return 1;
            }

            std::vector<std::string> fileList;
            DirMan::mkAbsDir(DIR_EXCELS_ROOT "/" DIR_EXCELS_LOADED_CACHE);
            DirMan::mkAbsDir(DIR_EXCELS_ROOT "/" DIR_EXCELS_INVALID_CACHE);
            chmod((DIR_EXCELS_ROOT "/" DIR_EXCELS_LOADED_CACHE), 0755);
            chmod((DIR_EXCELS_ROOT "/" DIR_EXCELS_INVALID_CACHE), 0755);

            curPath = dir.absolutePath();
            size_t files_counter = 1;
            if(dir.getListOfFiles(fileList, filters))
            //while(dir.fetchListFromWalker(curPath, fileList))
            {
                files_counter = 1;
                std::fprintf(stdout, "===============================================================================\n");
                std::fprintf(stdout, "Будет произведена проверка и считывание файлов с общим количеством: %lu\n", fileList.size());
                std::fprintf(stdout, "===============================================================================\n\n");

                for(std::string &file : fileList)
                {
                    std::fprintf(stdout, "\n\n===============================================================================\n");
                    std::fprintf(stdout, "Обработка файла %lu из %lu\n", files_counter, fileList.size());
                    std::fprintf(stdout, "===============================================================================\n");
                    std::fflush(stdout);
                    files_counter++;

                    if(sameFiles(curPath + "/" + file,
                                 DIR_EXCELS_ROOT "/" DIR_EXCELS_LOADED_CACHE "/" + file) ||
                       sameFiles(curPath + "/" + file,
                                 DIR_EXCELS_ROOT "/" DIR_EXCELS_INVALID_CACHE "/" + file) )
                    {
                        std::fprintf(stdout, "База данных актуальна, обновление не требуется\n");
                        std::fprintf(stdout, "===============================================================================\n");
                        std::fflush(stdout);
                        continue;
                    }

                    ScheduleFile schedule;
                    if(schedule.loadFromExcel(curPath + "/" + file))
                    {
                        if(!manager.passScheduleFile(schedule))
                        {
                            std::fprintf(stderr, "\n");
                            std::fprintf(stderr, "===============================================================================\n");
                            std::fprintf(stderr, "ФАЙЛ ОТКЛОНЁН по причине: %s. (Всего строк в файле: %lu)\n",
                                                  manager.errorString().c_str(),
                                                  schedule.entries().size());
                            std::fprintf(stderr, "===============================================================================\n");
                            //std::fprintf(stderr, "Файл %s содержит ошибочные данные!\n", (curPath + "/" + file).c_str());
                            std::fflush(stderr);

                            Files::copyFile(DIR_EXCELS_ROOT "/" DIR_EXCELS_INVALID_CACHE "/" + file,
                                            curPath + "/" + file, true);
                            std::fprintf(stderr, "===============================================================================\n");
                            std::fprintf(stderr, "Файл помещён в %s\n", (DIR_EXCELS_ROOT "/" DIR_EXCELS_INVALID_CACHE "/" + file).c_str());
                            std::fflush(stderr);
                            chmod((DIR_EXCELS_ROOT "/" DIR_EXCELS_INVALID_CACHE "/" + file).c_str(), 0644);
                        }
                        else
                        {
                            std::fprintf(stdout, "\n");
                            std::fprintf(stdout, "===============================================================================\n");
                            std::fprintf(stdout, "Всего прочитано строк: %lu\n", schedule.entries().size());
                            std::fprintf(stdout, "===============================================================================\n");
                            std::fflush(stdout);

                            Files::copyFile(DIR_EXCELS_ROOT "/" DIR_EXCELS_LOADED_CACHE "/" + file,
                                            curPath + "/" + file, true);
                            std::fprintf(stdout, "===============================================================================\n");
                            std::fprintf(stdout, "Файл помещён в %s\n", (DIR_EXCELS_ROOT "/" DIR_EXCELS_LOADED_CACHE "/" + file).c_str());
                            std::fflush(stdout);
                            chmod((DIR_EXCELS_ROOT "/" DIR_EXCELS_LOADED_CACHE "/" + file).c_str(), 0644);
                        }
                        std::fprintf(stdout, "\n");
                        std::fflush(stdout);
                    }
                }
            }

            // Оптимизировать главную таблицу в завершение
            manager.optimizeMainTable();
            // Разблокировать расписание для просмотра
            manager.unlockSchedule();
        } else {
            std::fprintf(stderr, "Can't connect database! [%s]\n", manager.dbError().c_str());
            std::fflush(stderr);
        }
    }

    std::fprintf(stdout, "===============================================================================\n");
    std::fprintf(stdout, "Все изменения успешно внесены\n");
    std::fflush(stdout);

    return 0;
}
