<?php
/******************************************************************************
 *                   !!!   ОСТОРОЖНО, ГОВНОКОД!  !!!!                         *
 ******************************************************************************/
/******************************************************************************
 * Это - первая версия загрузчика данных из XLS-файлов в базу данных. Крайне  *
 * неоптимизированное, неповоротливое и наркоманское решение. Читайте на свой *
 * страх и риск!                                                              *
 *                                                                            *
 * Создал я его в 2013м году будучи ещё неопытным студентом. Из-за нехватки   *
 * опыта, код получился очень медленным и кривым: из-за него было очень много *
 * разных ошибок: например, при случайном расписаний с другого семестра, на   *
 * сервере создавался семестр будущего, а все действующие расписания автоматом*
 * становились "устаревшими". Разве это нормально? Так вот, я устал чинить это*
 * безобразие, и создал совершенно новое и обдуманное решение на C++, которое *
 * полностью исправляло все ошибки этого скрипта.                             *
 *                                                                            *
 * Приготовьте тазы, куда тошнить, наслаждаясь сием чтивом ниже.              *
 *                                                                            *
 * Желаю успехов, и здоровья!                                                 *
 ******************************************************************************/


header('Content-Type: text/html; charset=utf-8');
if(defined('STDIN'))
    echo("Starting script...\n");
else
    die("Not for Web\n");

echo date("Y-m-d H:i:s\n");
//exit;

function d_pause()
{
    echo "Type anything and press ENTER to continue: ";
    $handle = fopen ("php://stdin","r");
    $line = fgets($handle);
}

function doDbQuery($query)
{
    return mysqlold_query($query);
}

function getField($query, $field)
{
    $q = doDbQuery($query);
    $arr = mysqlold_fetch_array($q);
    if(!$arr)
    {
        echo "!!!getField: Query results no fields: [$query]\n";
        return NULL;
    }
    return $arr[$field];
}



/////////////////////////////////////////////////////////////////////////
/////////////////////////Отладочный режим!///////////////////////////////
///////////Вывод всевозможных дополнительных данных,/////////////////////
///////////с помощью которых можно отлаживать работу парсера/////////////
$DebugEnable = false;   /////////////////////////////////////////////////
/////////////////////////Отладочный режим!///////////////////////////////
$Debug_AbortOnDayCount = false;
/////////////////////////////////////////////////////////////////////////

/*
Краркая схема работы:
/////////////////////////////////////////////////////////////////////////
DoWriteFileInDataBase - Запустить парсинг выбранного файла
ExcelRead             - Прочитать расписание по группам и подгруппам
CheckData             - Распознать данные ячейки на базе координат и близ-лежащих данных
DoOut                 - Отправить распознанные данные одного занятия в базу данных
/////////////////////////////////////////////////////////////////////////
*/

parse_str(implode('&', array_slice($argv, 1)), $_GET);
echo dirname(__FILE__)."\n";
echo "===========================================================================================\n";
echo "Система расписаний МГТУ ГА\n";
echo "Модуль Обновления содержимого базы данных из имеющихся Excel-расписаний\n";
echo "===========================================================================================\n";

$file = dirname(__FILE__)."/CheckDB_LOG.txt";
$source = "---------------------------------------------------------------\nСкрипт запускался ".date("Y-m-d H:i:s")."\n";

$Saved_File = fopen($file, 'a+');
fwrite($Saved_File, $source);

require dirname(__FILE__)."/../sys/db.php";

set_include_path(get_include_path() .
PATH_SEPARATOR . 'PhpExcel/Classes/');
require_once dirname(__FILE__).'/exceltest/Classes/PHPExcel/IOFactory.php';

///////////////////////////ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ
$FileCounter            = 0;
$FileTotal              = 0;
$weekday                = "";
$number_lection         = 0;
$couples                = NULL;
$lection_ttl            = "";
$l_type                 = "";
$lector                 = "";
$cabinet                = "";
$time_of_lection        = "";
$hour_courator          = 0;

$DateS                  = "";
$DateE                  = "";
$DateEx                 = "";
$OnlyDates              = "";
$DAYCOUNT_Y             = "";
$DAYCOUNT_M             = "";
$DAYCOUNT_D             = "";
$OldDataIsDeleded       = 0;
$IsStartingUpdate       = 0;
$Flow_grname            = "";
$Flow_Number            = 0;
$Flow_GrNmb             = 0;
$Flow_StartYear         = "";
$rowof                  = 0;
$LectionNumber          = 0;
$CountOfRows            = 0;

$TotalRejected          = 0;   //Отклонено файлов
$RejectedFile           = false;//Файл был отклонён
$RejectedFileReason     = "";   //Причина отказа
///////////////////////////ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ

/////////////////////////////////////////ЧТЕНИЕ СПИСКА ФАЙЛОВ
$src_dir    = dirname(__FILE__) . "/excels/new/";
$dir        = dirname(__FILE__) . "/excels/new";

$dirHandle  = opendir($src_dir);
$files      = array();

if(empty($argv[1]))
{
    foreach(scandir($dir) as $file)
        $files[$file] = "$dir/$file";

    asort($files);
    $files = array_keys($files);

    //Чтение списка файлов
    //while (false !== ($file = readdir($dirHandle)))
    for($i=0; $i<count($files);$i++)
    {
        if(($files[$i] != ".") && ($files[$i] != "..") && strstr($files[$i], ".xls"))
        $FileTotal++;
    }
}
else
{
    $FileTotal=1;
    $files[0] = $argv[1];
}

echo "===============================================================================\n";
echo "Будет произведена проверка файлов с общим количеством: $FileTotal\n";
echo "===============================================================================\n";

fwrite($Saved_File, "Всего файлов $FileTotal\n");

//$dirHandle = opendir($src_dir);
//while (false !== ($file = readdir($dirHandle)))

for($i=0; $i < count($files); $i++)
{
    if(($files[$i] != ".") && ($files[$i] != "..") && strstr($files[$i], ".xls"))
    {
        $FileCounter++;
        DoWriteFileInDataBase($files[$i]);
    }
}
/////////////////////////////////////////ЧТЕНИЕ СПИСКА ФАЙЛОВ
//exit;

if($TotalRejected > 0)
{
    echo "===============================================================================\n";
    echo "Есть отклонённые файлы: $TotalRejected\n";
    echo "===============================================================================\n";
}

DoOptimaseIdNumbers("schedule__maindata", "id");

fclose($Saved_File);

//=========================================================================================================
//=========================================================================================================
//=========================================================================================================
//=========================================================================================================
exit;

/////////////////////////////////////////////ФУНКЦИИ////////////////////////////////////////////////////////////

function DoWriteFileInDataBase($FileExc)
{
	$weekdays = array("П О Н Е Д Е Л Ь Н И К", "В Т О Р Н И К", "С Р Е Д А", "Ч Е Т В Е Р Г",
			  "П Я Т Н И Ц А", "С У Б Б О Т А", "В О С К Р Е С Е Н Ь Е");
	global $rowof; global $LectionNumber; global $weekday; global $number_lection; global $couples;
	global $lection_ttl; global $l_type; global $lector; global $cabinet; global $time_of_lection;
	global $DateS; global $DateE; global $DateEx; global $OnlyDates; global $Flow_Number; global $Flow_GrNmb; global $CountOfRows;
	global $Flow_grname; global $FileCounter; global $FileTotal; global $Flow_StartYear; global $IsStartingUpdate; global $OldDataIsDeleded;
	global $Saved_File; global $RejectedFile; global $RejectedFileReason; global $TotalRejected;

	echo "\n\n===============================================================================\n";
	echo "Обработка файла $FileCounter из $FileTotal\n";
	echo "===============================================================================\n";

	////////////////////////////////////ЧТЕНИЕ ИМЕНИ ФАЙЛА
	//$filename = "ПМ 3-1.xls";
	//if(!isset($_GET['excel']))
	//$FileExc = "ПМ 3-1.xls";
	//else
	//$FileExc = $_GET['excel'];
	////////////////////////////////////ЧТЕНИЕ ИМЕНИ ФАЙЛА

	//////////////////////////////////ПРОВЕРКА НАЛИЧИЯ ФАЙЛА
	if(!file_exists(dirname(__FILE__)."/excels/new/".$FileExc))
	{
    	die("Файл не существует!\n");
	}
	//else
	//die("Файл существует!$FileExc\n");
	//////////////////////////////////ПРОВЕРКА НАЛИЧИЯ ФАЙЛА


	///////////////////////////////////ПРОВЕРК НА ОШИБКУ В ИМЕНИ ФАЙЛА И ПОПЫТКА АВТОКОРРЕКЦИИ
	if($FileExc[strlen($FileExc)-8]==" ")
	{
		$FileExc_A = $FileExc;
	}
	else{
		$FileExc_A = "";
		for ($i = 0; $i < strlen($FileExc); $i++)
		{
		    if ($i == strlen($FileExc)-7)
                $FileExc_A .= " ";
		    $FileExc_A .= $FileExc[$i];
		}
	}

	$FileExc_A = str_replace("  ", " ", $FileExc_A);

	echo $FileExc_A."\n";
	///////////////////////////////////ПРОВЕРКА НА ОШИБКУ В ИМЕНИ ФАЙЛА И ПОПЫТКА АВТОКОРРЕКЦИИ


	//////////////////////////////////ПРОВЕРКА АКТУАЛЬНОСТИ БАЗЫ ДАННЫХ
	/*
	НЕ ПЕРЕЗАПИСЫВАТЬ ОДНО И ТО ЖЕ
	ОБНОВЛЯТЬ ЕСЛИ ФАЙЛ ДРУГОЙ
	*/
	if(
		(file_exists(dirname(__FILE__)."/excels/loaded/".$FileExc_A)) && (hash_file("md5", dirname(__FILE__)."/excels/loaded/".$FileExc_A)
		==
		hash_file("md5", dirname(__FILE__)."/excels/new/".$FileExc))
	  )
	{
		echo "База данных актуальна, обновление не требуется\n";
		echo "===============================================================================\n";
		return 1;
	}
	else
	{
		echo "Новый файл, будет произведена запись данных в базу\n";
	}
	//////////////////////////////////ПРОВЕРКА АКТУАЛЬНОСТИ БАЗЫ ДАННЫХ

	$Flow = explode(".", $FileExc_A);
	$Flow = explode("-", str_replace(" ", "-", $Flow[0]));
	echo "===============================================================================\n";
	echo "Начинается процесс записи в базу данных, файл '$FileExc_A'\n";
	echo "Курс $Flow[1] и группа $Flow[2]\n";
	echo "===============================================================================\n";
	fwrite($Saved_File, "-------------\n  $FileExc_A\n-------------\n");
	//die("");


	//////////////////////////ПРОЦЕСС ЗАПИСИ В БАЗУ
	$CountOfRows = 0;
	$OldDataIsDeleded = 0;
    $RejectedFile = false;//Сбросить индикатор отказа

	ExcelRead($FileExc, 0, 0, NULL);//Парсинг первой страницы
    if($RejectedFile)//Если файл был отклонён по каким либо причинам
    {
        echo "\n";
        echo "===============================================================================\n";
        echo "ФАЙЛ ОТКЛОНЁН по причине: $RejectedFileReason. Всего прочитано строк: $CountOfRows\n";
        echo "===============================================================================\n";
        $TotalRejected++;
        return -1;
    }

	ExcelRead($FileExc, 2, 1, 1);
	ExcelRead($FileExc, 3, 1, 2);
	ExcelRead($FileExc, 4, 1, 3);
	ExcelRead($FileExc, 5, 1, 4);
	//////////////////////////ПРОЦЕСС ЗАПИСИ В БАЗУ
	echo "\n";
	echo "===============================================================================\n";
	echo "Всего прочитано строк: $CountOfRows\n";
	echo "===============================================================================\n";
	fwrite($Saved_File, "Всего прочитано строк: $CountOfRows\n==============\n");

	///////////////////КОПИРОВАНИЕ ФАЙЛА НА СКЛАД
	if(file_exists(dirname(__FILE__)."/excels/loaded/".$FileExc_A)) unlink(dirname(__FILE__)."/excels/loaded/".$FileExc_A);
	copy(dirname(__FILE__)."/excels/new/".$FileExc, dirname(__FILE__)."/excels/loaded/".$FileExc_A);
	echo "===============================================================================\n";
	echo "Файл помещён в ".dirname(__FILE__)."/excels/loaded/".$FileExc_A."\n";
	///////////////////КОПИРОВАНИЕ ФАЙЛА НА СКЛАД

	///////////////////ПРИМЕНЕНИЕ ПАРАМЕТРОВ ОБНОВЛЕНИЯ
	if( ($Flow_grname != "") && ($Flow_StartYear != "") )
	   doDbQuery("UPDATE `schedule_flows` SET is_updating=0, `latest_upd`=CURRENT_TIMESTAMP() WHERE gr_name='".$Flow_grname."' AND `gr_year-start` = '".$Flow_StartYear."' LIMIT 1;");
	doDbQuery("UPDATE `schedule_files` SET `flow`=".$Flow_Number.", `group`=".$Flow_GrNmb." WHERE `filename`='".$FileExc."' LIMIT 1;");
	$IsStartingUpdate = 0;
	echo "===============================================================================\n";
	echo "Все изменения успешно внесены\n";
	///////////////////ПРИМЕНЕНИЕ ПАРАМЕТРОВ ОБНОВЛЕНИЯ

	return 0;
}

//=========================================================================================================
//=========================================================================================================
//=========================================================================================================
//=========================================================================================================


function ExcelRead($FileName, $Sheet=0, $is_sbgrp=0, $sbgrp=NULL)
{
	global $CountOfRows;
	global $hour_courator;
    global $RejectedFile;

	$objPHPExcel = PHPExcel_IOFactory::load(dirname(__FILE__)."/excels/new/".$FileName);

	try
    {
        $objPHPExcel->setActiveSheetIndex($Sheet);
    }
	catch(Exception $e)
    {
    	return 0;
    }

	$aSheet = $objPHPExcel->getActiveSheet();

	foreach($aSheet->getRowIterator() as $row)
	{
		$cellIterator = $row->getCellIterator();
		$coll = 0;
		foreach($cellIterator as $cell)
		{
            CheckData(trim($cell->getCalculatedValue()), $coll, $FileName, $is_sbgrp, $sbgrp);
            if($RejectedFile)
                break;//Не продолжать парсинг в случае браковки файла
            $coll++;
		}

        if($RejectedFile)
            break;//Не продолжать парсинг в случае браковки файла
	}

	return 1;
}

function CheckData($cell, $coll, $FileName, $is_sbgrp=0, $sbgrp=NULL)
{
	$weekdays = array("П О Н Е Д Е Л Ь Н И К",
                      "В Т О Р Н И К", "С Р Е Д А", "Ч Е Т В Е Р Г",
        			  "П Я Т Н И Ц А", "С У Б Б О Т А", "В О С К Р Е С Е Н Ь Е");
	global $rowof; global $LectionNumber; global $weekday; global $number_lection; global $couples;

	global $lection_ttl; global $l_type; global $lector; global $cabinet; global $time_of_lection;

	global $DateS; global $DateE; global $DateEx; global $OnlyDates;  global $Flow_Number;

	global $Flow_GrNmb;     global $CountOfRows; global $Flow_grname;
	global $hour_courator;  global $DebugEnable;
    global $RejectedFile;   global $RejectedFileReason;

	if(($cell!="")&&($coll == 0)) //Номер пары
	{
		$number_lection = intval($cell);
		$LectionNumber = 1;
	}

	if((trim($cell) != "") && ($coll == 1)) //чётность
	{
		if($DebugEnable) echo " З".$cell;
    		$couples = ((trim($cell)=="В")?1:2);
		if($DebugEnable) echo " Ч".$couples." ";
    		$LectionNumber = 0;
	}
	else
	if(($LectionNumber == 1) && ($coll == 1)) //номер лекции
	{
		$couples = 0;
		$LectionNumber = 0;
	}


	if(($cell != "") && ($coll == 2))	//данные
	{
		if($rowof == 0)
		{
	        $lection_ttl = $cell;
	        if(strstr($lection_ttl, "Час наставника") && (trim($lection_ttl) != "Час наставника"))
	        {
		        $hour_courator = 1;
		        $l_type = "Час наставника";
		        $time_of_lection = str_replace(",",";",str_replace("Час наставника", "только", $lection_ttl));
		        $lection_ttl = "Час наставника";
	        }
	        else
		        $hour_courator = 0;
		}

		if($rowof==1)
		{
	        if(!$hour_courator)
                $l_type = $cell;
		}

		if(!$hour_courator)
		if($rowof==2)
		{
	        $time_of_lection = trim($cell);
	        DoOut($FileName, $is_sbgrp, $sbgrp);//Если собран полный набор данных - записываем в базу!
	        $CountOfRows++;
            if($RejectedFile)
                return -1;
		}
		if($rowof != 2)
            $rowof++;
        else
            $rowof = 0 ;
	}
    else
    if(($cell == "") && ($coll == 2) && ($rowof == 2))//Если обнаружено пустое поле с датой, пропустить эту запись
    {
        $CountOfRows++;
        $rowof = 0 ;
    }


	if(($cell != "") && ($coll == 4))
	{
		//ЕСЛИ день недели
		if(in_array((string)$cell, $weekdays))
		{
			$weekday = $cell;
		}
		else
		{
			$lector = $cell;
			if($hour_courator)
			{
		        $i=0;
		        while(($lector[$i] != ".") && ($i < strlen($lector)))
                    $i++;
		        $lector = substr($lector, $i+1, strlen($lector)-1).",".substr($lector, 0, $i);
		        DoOut($FileName, $is_sbgrp, $sbgrp);
		        $rowof = 0;
                if($RejectedFile)
                    return -1;
			}
		}
	}

	if(($cell != "") && ($coll == 6))
	{
		$cabinet = $cell;
	}
    return 0;
}

function DoOut($FileName, $is_sbgrp=0, $sbgrp=NULL)
{
    global $Debug_AbortOnDayCount;
	global $weekday;
	global $number_lection;
	global $couples;
	global $lection_ttl;
	global $l_type;
	global $lector;
	global $cabinet;
	global $time_of_lection;

	global $DateS;
	global $DateE;
	global $DateEx;
	global $OnlyDates;

	global $DAYCOUNT_Y;
	global $DAYCOUNT_M;
	global $DAYCOUNT_D;
	global $OldDataIsDeleded;
	global $IsStartingUpdate;
	global $Flow_grname;
	global $Flow_StartYear;
	global $Flow_Number;
	global $Flow_GrNmb;
	global $CountOfRows;
	global $DebugEnable;

    global $RejectedFile;
    global $RejectedFileReason;

	$test = "";

	$DateS = "";
	$DateE = "";
	$DateEx = "";
	$OnlyDates = "";

	$isOnlyDays=false;

	$lector_a = explode(",", $lector);

	$time_of_lection = str_replace(" ", "", $time_of_lection);
	if(stristr($time_of_lection, "с")&&(stristr($time_of_lection, "по")))
	{
		$DateS = "'".TXT_2_Date(substr($time_of_lection, 2, 5))."'";
		$DateE = "'".TXT_2_Date(substr($time_of_lection, 11, 5))."'";
	}
	else
	{
	    $DateS = "NULL";
	    $DateE = "NULL";
	}
	if(stristr($time_of_lection, "кроме"))
	{
		$DateEx = "'".TXT_2_Date(substr($time_of_lection, 26))."'";
	}
	else
	{
    	$DateEx = "NULL";
	}

	if(strpos($time_of_lection, "только") !== false)
	{
		if($DebugEnable)
		{
			echo "Только! [".$time_of_lection."]\n";
		}
		$OnlyDates  = TXT_2_Date(substr($time_of_lection, 12));
		$isOnlyDays = true;
		//$couples = 0; //сбросить чётность для точного указания дня
	}
	else
	{
		$OnlyDates = "";
	}

	switch(trim($lector_a[1]))
	{
        case "преп.":
            $ScinceRank = "Преподаватель"; break;
        case "ст.преп.":
            $ScinceRank = "Старший преподаватель"; break;
        case "доц.":
            $ScinceRank = "Доцент"; break;
        case "проф.":
            $ScinceRank = "Профессор"; break;
        case "ассист.":
            $ScinceRank = "Ассистент"; break;
        case "зав.каф.":
            $ScinceRank = "Заведующий кафедры"; break;
        default:
            $ScinceRank = trim($lector_a[1]);
	}

	$weekdays = array("П О Н Е Д Е Л Ь Н И К",
                      "В Т О Р Н И К",
                      "С Р Е Д А",
                      "Ч Е Т В Е Р Г",
                      "П Я Т Н И Ц А",
                      "С У Б Б О Т А",
                      "В О С К Р Е С Е Н Ь Е");

	switch($weekday)
	{
        case $weekdays[0]:
            	$WeekDid = 1; break;
        case $weekdays[1]:
            	$WeekDid = 2; break;
        case $weekdays[2]:
            	$WeekDid = 3; break;
        case $weekdays[3]:
            	$WeekDid = 4; break;
        case $weekdays[4]:
            	$WeekDid = 5; break;
        case $weekdays[5]:
            	$WeekDid = 6; break;
        case $weekdays[6]:
            	$WeekDid = 0; break;
	}

	if($FileName[strlen($FileName)-8]==" ")
	{
	    $FileName_A = $FileName;
	}
	else
	{
	    $FileName_A = "";

	    for($i = 0; $i < strlen($FileName); $i++)
	    {
		if($i == strlen($FileName) - 7)
		    $FileName_A .= " ";
		    $FileName_A .= $FileName[$i];
	    }
	}
	$FileName_A = str_replace("  ", " ", $FileName_A);

	$Flow = explode(".", $FileName_A);
	$Flow = explode("-", str_replace(" ", "-", $Flow[0]));

	$Flow_SE = ((intval($DAYCOUNT_M)<7)?intval($DAYCOUNT_Y)-$Flow[1]:intval($DAYCOUNT_Y)-$Flow[1]+1);
	$Flow_EE = $Flow_SE + ((strstr($Flow[0], "б"))?4:5);

	if(!getField("SELECT count(*) FROM `schedule_flows` WHERE gr_name='".$Flow[0].
	             "' AND `gr_year-start` = '".$Flow_SE."' LIMIT 1;", 0))
	{
		doDbQuery("INSERT INTO `schedule_flows`
            		(`gr_name`, `gr_year-start`, `gr_year-end`, `id_facult`, `group_q`)
            		values('".$Flow[0]."', '".$Flow_SE."', '".$Flow_EE."',
            		(SELECT id_facult FROM `schedule_facult` WHERE `fac_gr_names` like '%\"".$Flow[0]."\"%' LIMIT 1), 1);");

	        if(getField("SELECT group_q FROM `schedule_flows` WHERE gr_name='" . $Flow[0].
    	                "' AND `gr_year-start` = '".$Flow_SE."' LIMIT 1;", 0) < $Flow[2])
                doDbQuery("UPDATE `schedule_flows` SET group_q=".$Flow[2]." WHERE gr_name='".$Flow[0]."' AND `gr_year-start` = '".$Flow_SE."' LIMIT 1;");
	}
	else
	{
        if(getField("SELECT group_q FROM `schedule_flows` WHERE gr_name='".$Flow[0].
                    "' AND `gr_year-start` = '".$Flow_SE."' LIMIT 1;", 0) < $Flow[2])
            doDbQuery("UPDATE `schedule_flows` SET group_q=".$Flow[2]." WHERE gr_name='".$Flow[0]."' AND `gr_year-start` = '".$Flow_SE."' LIMIT 1;");
	}

	if($IsStartingUpdate==0)
	{
        doDbQuery("UPDATE `schedule_flows` SET is_updating=1 WHERE gr_name='".$Flow[0]."' AND `gr_year-start` = '".$Flow_SE."' LIMIT 1;");
        $Flow_grname = $Flow[0];
        $Flow_StartYear = $Flow_SE;
        $IsStartingUpdate = 1;
	}


    $currentMonth = date('m');
    if($currentMonth > 7)
    {
        if(intval($DAYCOUNT_M) == 1)
            $DAYCOUNT_Y = intval($DAYCOUNT_Y) + 1;
    }

	if(!getField("SELECT count(*) FROM `schedule_daycount` WHERE STR_TO_DATE('".$DAYCOUNT_Y."-".$DAYCOUNT_M."-".$DAYCOUNT_D."', '%Y-%m-%d') BETWEEN daystart AND dayend  LIMIT 1;", 0))
	{
        if(intval($DAYCOUNT_Y) != intval(date('Y')))
        {
            $RejectedFile = true;
            $RejectedFileReason = "Несоответствие сезона расписания: сейчас ". date('Y') . ", распознанное расписание - $DAYCOUNT_Y";
            return -1;
        }

        if(intval($DAYCOUNT_M) > 7)
        {
            if($currentMonth <= 7)
            {
                $RejectedFile = true;
                $RejectedFileReason = "Несоответствие сезона расписания: сейчас ВЕСНА-ЛЕТО, распознанное расписание - ОСЕНЬ-ЗИМА";
                return -1;
            }
            $DayType    = $DAYCOUNT_Y . " - Осень";
            $DayStart   = "$DAYCOUNT_Y-09-01";
            $DayEnd     = "($DAYCOUNT_Y + 1)-01-25";
        }
        else
        {
            if($currentMonth > 7)
            {
                $RejectedFile = true;
                $RejectedFileReason = "Несоответствие сезона расписания: сейчас ОСЕНЬ-ЗИМА, распознанное расписание - ВЕСНА-ЛЕТО";
                return -1;
            }
            $DayType    = $DAYCOUNT_Y . " - Весна";
            $DayStart   = "$DAYCOUNT_Y-01-26";
            $DayEnd     = "$DAYCOUNT_Y-08-01";
        }

        if($Debug_AbortOnDayCount)
        {
            die("ФАЙЛ ".$FileName." ПЫТАЕТСЯ СОЗДАТЬ НОВЫЙ СЕМЕСТР! '".$DayStart."', '".$DayEnd."', '".$DayType."' НЕ НАШЁЛ '".$DAYCOUNT_Y."-".$DAYCOUNT_M."-".$DAYCOUNT_D."\n");
            //DEAD CODE:
        }
    	doDbQuery("INSERT INTO `schedule_daycount` (`daystart`, `dayend`, `desc`) values('".$DayStart."', '".$DayEnd."', '".$DayType."');");
	}



	if(!getField("SELECT count(*) FROM `schedule_lectors` WHERE lcr_shtname='".$lector_a[0]."' LIMIT 1;", 0))
	{
        doDbQuery("INSERT INTO `schedule_lectors` (`lcr_fullname`, `lcr_shtname`, `lcr_rank-l`) values('".$lector_a[0]."', '".$lector_a[0]."', '".$ScinceRank."');");
	}

	if(!getField("SELECT count(*) FROM `schedule_disciplyne` WHERE `dysc_name`='".$lection_ttl."' LIMIT 1;", 0))
	{
    	doDbQuery("INSERT INTO `schedule_disciplyne` (`dysc_name`) values('".$lection_ttl."');");
	}

	if(!getField("SELECT count(*) FROM `schedule_rooms` WHERE `room_number`='".$cabinet."' LIMIT 1;", 0))
	{
        $house = 0;
        if($cabinet[1]=="-")
        {
	        $CabN = explode("-", $cabinet);
	        $house = $CabN[0];
	        $Stage = $CabN[1][0];
        }

        if(!is_numeric($cabinet[4]))
        {
	        $house = 6;
	        if(is_numeric($cabinet[0])) $Stage = $cabinet[0];
	        else $Stage = 0;
        }

		$test .= "Корпус ".$house.", Этаж ".$Stage;

    	doDbQuery("INSERT INTO `schedule_rooms` (`room_number`, `id_house`, `room_stage`) values('".$cabinet."', ".$house.", ".$Stage.");");
	}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	if($DebugEnable)
	{
		echo "weekday='".$weekday."'\n";
		echo "number_lection='".$number_lection."'\n";
		echo "couples='".$couples."'\n";
		echo "lection_ttl='".$lection_ttl."'\n";
		echo "l_type='".$l_type."'\n";
		echo "lector='".$lector."'\n";
		echo "cabinet='".$cabinet."'\n";
		echo "time_of_lection='".$time_of_lection."'\n";
		echo "IsOnlyDays='".$isOnlyDays."'\n\n";
	}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	//echo "SELECT id_lector FROM `schedule_lectors` WHERE lcr_shtname='".$lector_a[0]."' LIMIT 1;\n";
	$Lector_ID = getField("SELECT id_lector FROM `schedule_lectors` WHERE lcr_shtname='".trim($lector_a[0])."' LIMIT 1;", 0);
	//echo "SELECT id_disciplyne FROM `schedule_disciplyne` WHERE dysc_name='".trim($lection_ttl)."' LIMIT 1;\n";
	$Dysc_ID   = getField("SELECT id_disciplyne FROM `schedule_disciplyne` WHERE dysc_name='".trim($lection_ttl)."' LIMIT 1;", 0);
	//echo "SELECT id_ltype FROM `schedule_ltype` WHERE `lt_name_sh`='".trim($l_type)."' LIMIT 1;\n";
	$ltype     = getField("SELECT id_ltype FROM `schedule_ltype` WHERE `lt_name_sh`='".trim($l_type)."' LIMIT 1;", 0);
	$RoomID    = getField("SELECT id_room FROM `schedule_rooms` WHERE `room_number`='".trim($cabinet)."' LIMIT 1;", 0);
	$FlowID    = getField("SELECT id_flow FROM `schedule_flows` WHERE gr_name='".$Flow[0]."' AND `gr_year-start` = '".$Flow_SE."' LIMIT 1;", 0);
	$Day_ID    = getField("SELECT id_day FROM `schedule_daycount` WHERE DATE('".($DAYCOUNT_Y."-".$DAYCOUNT_M."-".$DAYCOUNT_D)."') BETWEEN daystart AND dayend LIMIT 1;", 0);

	//echo $Day_ID."\n";
	$Flow_Number   = intval($FlowID);
	$Flow_GrNmb    = $Flow[2];
	//$cabinet
	//schedule_rooms

    if(mysqlold_error() != "")
        die(mysqlold_error());

    if($DebugEnable)
    {
        $warningCountResult = doDbQuery("SELECT @@warning_count");

        if($warningCountResult)
        {
            $warningCount = mysqlold_fetch_row($warningCountResult);
            if($warningCount[0] > 0)
            {
                //Have warnings
                $warningDetailResult = doDbQuery("SHOW WARNINGS");
                if($warningDetailResult)
                {
                    while($warning = mysqlold_fetch_assoc(warningDetailResult))
                    {
                        //Process it
                    }
                }
                exit("ЧП случилось на этом файле: ".$FileName);
            }//Else no warnings
        }
    }

	if($OldDataIsDeleded == 0)
	{
        doDbQuery("DELETE FROM `schedule__maindata` WHERE `id_flow`=".intval($FlowID)." AND id_group LIKE '%".intval($Flow[2])."%' AND `change` = 0;");
        echo "!!!!!!!!!!!Из базы удалены данные потока ".$FlowID." и группы ".$Flow[2]."!!!!!!!!\n";
        $OldDataIsDeleded = 1;
        //die();
	}

	/*
	echo "$Day_ID $WeekDid $number_lection $couples $Dysc_ID $ltype $FlowID $Flow[2] $is_sbgrp ".(($is_sbgrp)?$sbgrp:"&nbsp;").
	" $Lector_ID $RoomID $DateS $DateE $DateEx $OnlyDates\n";
	*/
	echo "*";

	///ВСТАВКА В БАЗУ ДАННЫХ!!!!

	//if ( ($Dysc_ID != 5) && ($is_sbgrp==0) || ($is_sbgrp == 1))
    doDbQuery("
    INSERT INTO `schedule__maindata`
    (id_day,
    `weekday`,
    lection,
    couples,
    id_disciplyne,
    id_ltype,
    id_flow,
    id_group,
    issubgrp,
    id_subgrp,
    id_lector,
    id_room,
    `period-start`,
    `period-end`,
    exceptions,
    onlydays)

    values($Day_ID, $WeekDid, $number_lection, ".($isOnlyDays ? 0 : $couples).", $Dysc_ID, $ltype, $FlowID, '$Flow[2]', $is_sbgrp, '" .
    (($is_sbgrp) ? (($Dysc_ID==5) ? $sbgrp : $sbgrp + 2) : "")."', $Lector_ID, $RoomID, $DateS, $DateE, $DateEx, '$OnlyDates');");

	/*
	schedule__maindata

	id_day
	weekday
	lection
	couples
	id_disciplyne
	id_ltype
	id_flow
	id_group
	issubgrp
	id_subgrp
	id_lector
	id_room
	period-start
	period-end
	exceptions
	onlydays
	*/
        return 0;
}


function TXT_2_Date($dateOfText)
{
	global $DAYCOUNT_Y;
	global $DAYCOUNT_M;
	global $DAYCOUNT_D;

    if(trim($dateOfText)=="")
    {
        return "";
    }


	$onlydates = $dateOfText;
	//if(strstr($onlydates, ";"))
	//{
	$onlydates_arr = explode(";", $onlydates);
	$onlydates = "";
	for($i=0;$i<count($onlydates_arr);$i++)
	{
		$onlydates_arr2 = explode(".",$onlydates_arr[$i]);
        $edgeSezon_M = 7; //Считать границей между сезонами 15 июля
        $edgeSezon_D = 15;

		//Если сейчас зима, не писать "прошедшее" как будто будущее
        //echo "\$onlydates_arr2[1]=".$onlydates_arr2[1].
        //" date(\"m\") ".date("m")."\n";
        //d_pause();

		if(
            (
            (intval($onlydates_arr2[1]) > intval($edgeSezon_M)) &&
            (intval(date("m")) > intval($edgeSezon_M))
            )
            ||
            (
                (intval($onlydates_arr2[1]) == intval($edgeSezon_M)) &&
                (intval(date("m")) == intval($edgeSezon_M)) &&
                (intval($onlydates_arr2[0]) == intval($edgeSezon_D)) &&
                (intval(date("d")) > intval($edgeSezon_D))
            )
        )
			$year = date("Y"); // Если сейчас осень
		else
		if($onlydates_arr2[1] <= $edgeSezon_M)
			$year = date("Y"); // Если сейчас весна
		else
			$year = (date("Y") - 1);// Если это осень прошлого года

		$onlydates .= $year . "-" . $onlydates_arr2[1] . "-" .
                      str_pad($onlydates_arr2[0], 2, '0', STR_PAD_LEFT) .
                      (($i != (count($onlydates_arr) - 1)) ? " " : "");
	}
	//}
	$_Dates = explode("-",str_replace(" ", "-", $onlydates));
	//echo $_Dates[0]."-".$_Dates[1]."-".$_Dates[2]."\n";
	$DAYCOUNT_Y = $_Dates[0];
	$DAYCOUNT_M = $_Dates[1];
	$DAYCOUNT_D = $_Dates[2];

        if(trim($DAYCOUNT_M)=="")
        {
                echo("ДЕНЬ ПУСТОЙ!!! ".$dateOfText."");
                return "";
        }
        if(trim($DAYCOUNT_D)=="")
        {
                echo("МЕСЯЦ ПУСТОЙ!!! ".$dateOfText."");
                return "";
        }

        //echo "\DAYCOUNT_Y=".$DAYCOUNT_Y." FROM ".$onlydates." ->".$dateOfText."\n";
        //d_pause();

	return $onlydates;
}

function DoOptimaseIdNumbers($tableName, $IDField = 'id')
{
	echo "Оптимизация таблицы $tableName....\n";
	$OptimizeIsEnd = 0;
	$CountOf = getField("SELECT count(*) FROM `".$tableName."`;", 0);
        if($CountOf==0)
        {
            echo "Оптимизация не требуется, таблица пустая";
            return;
        } else if($CountOf==1) {
            echo "Оптимизация не требуется, единственная запись";
            return;
        }
	$query = "SELECT `".$IDField."` FROM `".$tableName."` ORDER BY `".$IDField."`;";
	while(!$OptimizeIsEnd)
	{
    	$MaxN = getField("SELECT MAX(`".$IDField."`) FROM `".$tableName."` LIMIT 1;", 0);
    	$Counter = 1;
    	$Table_q = doDbQuery($query);
    	while(($Table = mysqlold_fetch_array($Table_q))!=NULL)
		{
    		if($Table[$IDField] != $Counter)
			{
    			doDbQuery("UPDATE `".$tableName."` SET `".$IDField."`=".$Counter." WHERE `".$IDField."` = ".$MaxN.";");
    			break;
			}
    		if($Counter == $MaxN)
                $OptimizeIsEnd = 1;
    		$Counter++;
		}
	}
    echo "Сброс счётчика...\n\n";
    doDbQuery("ALTER TABLE `".$tableName."` AUTO_INCREMENT = 1;");
	echo "Оптимизировано\n\n";
}

?>
