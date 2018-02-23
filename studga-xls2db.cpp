#include <stdio.h>
#include <dirman.h>
#include "schedule_file.h"

int main()
{
    DirMan dir;
    dir.setPath("/home/vitaly/Документы/StudGA");
    std::vector<std::string> filters;
    filters.push_back(".xls");

    if(dir.beginWalking(filters))
    {
        std::string curPath;
        std::vector<std::string> fileList;
        while(dir.fetchListFromWalker(curPath, fileList))
        {
            for(std::string &file : fileList)
            {
                ScheduleFile schedule;
                if(schedule.loadFromExcel(curPath + "/" + file))
                {
                    //std::printf("File %s successfully processed!\n", (curPath + "/" + file).c_str());
                    //std::fflush(stdout);
                }
            }
        }
    }

    std::printf("\nEverything has been completed!\n\n");
    std::fflush(stdout);

    return 0;
}
