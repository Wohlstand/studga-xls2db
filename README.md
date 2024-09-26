# stusga-xls2db
Сортировщик XLS-файлов для Системы расписаний МГТУ ГА (репозиторий основного проекта тут: https://github.com/Wohlstand/studga-schedule).

**ВНИМАНИЕ: Начиная с 2024 года, формат данных официального расписания МГТУ ГА изменился (На офицаильном сайте университета появился своя официальная веб-версия расписаний вместо кучи XLS-файлов), и данный модуль больше не актуален. С его помощью лишь можно читать [исторические версии расписаний](https://studga.wohlnet.ru/excels/backup/).**

# Описание
Это вспомогательный компонент Системы Расписаний МГТУ ГА, который выполнял роль
обновления базы данных системы расписаний из предварительно загруженных XLS-файлов
расписаний с официального сайта. Данный компонент был создан в 2018м году в
качестве замены аналогичному модулю, который был написан на PHP в 2013м году.

# Комментарий
Мне как разработчику проще работать именно с C++, а не на PHP, и, устав постоянно
чинить криво написанный будучи студентом скрипт, я решил написать полностью новый
модуль, но на C++, и сделать сам процесс обновления БД более оптимизированным:
старый скрипт во время своей работы делал огромное число запросов к БД по каждой
мелочи, из-за чего процесс полной выгрузки БД занимал десятки минут, а обновлённый
модуль (этот) выполняет свою работу меньше, чем за минуту при той же мощности
оборудования.
