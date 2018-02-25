#include <openssl/md5.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>

#include <stdio.h>
#include <dirman.h>
#include "files.h"
#include "schedule_file.h"
#include "schedule_manager.h"

// Get the size of the file by its file descriptor
static size_t get_size_by_fd(int fd) {
    struct stat statbuf;
    if(fstat(fd, &statbuf) < 0) exit(-1);
    return (size_t)statbuf.st_size;
}

std::string file_md5sum(const std::string &path)
{
    int file_descript;
    size_t file_size;
    char* file_buffer;
    unsigned char result[MD5_DIGEST_LENGTH];
    char result_str[MD5_DIGEST_LENGTH * 2 + 1];
    file_descript = open(path.c_str(), O_RDONLY);
    if(file_descript < 0)
        return "<none>";
    file_size = get_size_by_fd(file_descript);

    file_buffer = (char*)mmap(0, file_size, PROT_READ, MAP_SHARED, file_descript, 0);
    MD5((unsigned char*) file_buffer, file_size, result);
    munmap(file_buffer, file_size);
    for(int i=0; i < MD5_DIGEST_LENGTH; i++) {
        std::sprintf(&result_str[i*2], "%02x", result[i]);
    }
    return std::string(result_str);
}

int main()
{
    DirMan dir;
    dir.setPath("/home/vitaly/Документы/StudGA");
    std::vector<std::string> filters;
    filters.push_back(".xls");

    //if(dir.beginWalking(filters))
    {
        std::string curPath;
        ScheduleManager manager;
        if(manager.connectDataBase())
        {
            // Заблокировать расписание для просмотра
            manager.lockSchedule();

            std::vector<std::string> fileList;
            dir.mkdir("loaded");
            dir.mkdir("invalid");
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

                    if(  (Files::fileExists(curPath + "/loaded/" + file) &&
                        (file_md5sum(curPath + "/" + file) == file_md5sum(curPath + "/loaded/" + file)) ) ||
                         (Files::fileExists(curPath + "/invalid/" + file) &&
                        (file_md5sum(curPath + "/" + file) == file_md5sum(curPath + "/invalid/" + file)) )
                    )
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

                            Files::copyFile(curPath + "/invalid/" + file, curPath + "/" + file, true);
                            std::fprintf(stderr, "===============================================================================\n");
                            std::fprintf(stderr, "Файл помещён в %s\n", (curPath + "/invalid/" + file).c_str());
                            std::fflush(stderr);
                        }
                        else
                        {
                            std::fprintf(stdout, "\n");
                            std::fprintf(stdout, "===============================================================================\n");
                            std::fprintf(stdout, "Всего прочитано строк: %lu\n", schedule.entries().size());
                            std::fprintf(stdout, "===============================================================================\n");
                            std::fflush(stdout);

                            Files::copyFile(curPath + "/loaded/" + file, curPath + "/" + file, true);
                            std::fprintf(stdout, "===============================================================================\n");
                            std::fprintf(stdout, "Файл помещён в %s\n", (curPath + "/loaded/" + file).c_str());
                            std::fflush(stdout);
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
