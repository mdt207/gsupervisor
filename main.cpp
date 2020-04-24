/* C* Bismillahir Rahmanir Raheem  C* */

//Muqobil Dasturlar To'plami (c) hijriy 1436,1437,1438 (melodiy 2014,2015,2016,2017)

//#define _WIN32_WINNT 0x0501
//#define _WIN32_WINNT _WIN32_WINNT_WINXP
//#define WINVER 0x0501
//#define NTDDI_VERSION NTDDI_WINXPSP2//NTDDI_WINXP

//#define _CRT_SECURE_NO_WARNINGS_GLOBALS
//#define _CRT_INSECURE_DEPRECATE
//#define _CRT_SECURE_NO_WARNINGS
//#include <windows.h>

#include <algorithm>
#include <vector>
#include <iterator>
#include <iostream>
#include <fstream>
#include <ostream>
#include <sstream>
#include <string>
#include <ctime>
#include <stdlib.h>
#include <math.h>

#include <gst/gst.h>
#include <gtk/gtk.h>

#include <sqlite3.h>
#include <libusb.h>

#include <boost/lexical_cast.hpp>
#include <boost/interprocess/shared_memory_object.hpp>
#include <boost/interprocess/mapped_region.hpp>

#include "utils.h"
#include "gtester_ico.h"

#define APP_IMITATE
//#undef  APP_IMITATE

#define DEV_CONFIG 1
#define WORD_SIZE 64

//#define abs(x) fabs(x)
#define usleep(x) Sleep(x)

//consts
const char ver[]   = { "1.15" };
const char prog_name[]= {"GSupervisor"};

const char *dseg_fonts[4] = {
			"DSEG7Classic-Regular.ttf",
			"DSEG14Classic-Regular.ttf",
			"DSEG7Modern-Regular.ttf",
			"DSEG14Modern-Regular.ttf"
			};

using namespace std;
using namespace boost::interprocess;

STARTUPINFO si;
PROCESS_INFORMATION pi;

sqlite3 *pDB, *pProdsBoughtDB;
sqlite3_stmt *pStmt;

std::string insert_next_mv = "insert into nav_mv values($nID, $navID, :timeID, $mv, $Watt)";

GMainLoop *gLoop = NULL;

GstElement *pipeline, *source, *demuxer, *decoder, *conv, *sink;
GstElement *playbin2;
GstBus *bus;
GstStateChangeReturn gsStateChRet;

libusb_device **devs; //pointer to pointer of device, used to retrieve a list of devices
libusb_context *ctx = NULL; //a libusb session
libusb_device_handle *hdev;

shared_memory_object *pShared_mem = NULL;
mapped_region *pMmap = NULL;

int r; //for return values
ssize_t cnt; //holding number of devices in list

GtkApplication *gApp; //main application

GtkWidget *window = NULL; //main window
GtkWidget *view = NULL, *aux_view = NULL, *radio1 = NULL, *radio2;
GtkWidget *btn_start_getval, *but_corr_usb_accept;
static GdkRGBA /*text_color,*/ bg_color;
PangoFontDescription *font_desc = NULL;
GdkPixbuf *myPixbuf = NULL;

char gl_buf_tm[20], ipc_mes[16] = "HelloWorld";
gchar Watt[20], *gCmdLine = NULL, *ipc_notifier_path=NULL;
char *pStr2num = NULL, *pWatts = NULL;
char param[8], param2[8], param3[8];
char  params[32];
gchar *pCurrentDir = NULL;
char *ipc_notify_audio_file_path, *alarm_audio_file_path;

int i=0, thread_counter=0, /*len=9, actual=0*/ base[6], case_of_tasks=-1;
int speakerOff_timeOut=3600, nID = 1, navID, bResume;
int _count_alrm = 0, period_notify_count = 0, period_ipc_notify=2, period_mod = 600;
int gCountDownTime = 0, _CountDownTime; //120 min -> 2 hours count down
int gWndWidth = 690, gWndHeight = 290;
int indx_vSerr = 0, last_error;
int font_no = 0, amv_median_size = 0;

std::string tmp, /*str2num, serr,*/ gTimeStr, sumWatt("|");
std::string min_per_tm("02:30"), max_per_tm("05:30");
bool corr_order = false, isSettingsNew = false/*, bTaskPause = false*/;
bool bDo_ipc_notify=false, isRecOn = true; //bclose_splash=false;
bool bDoUpdate = true, isCovertV2mV = true;
int isMakeZero_Watt_counter = 0;

GMutex data_mutex;
GCond data_cond;

G_LOCK_DEFINE_STATIC( corr_order );

bool bIsRunIPCnotifier = false, bdo_some_thing = false, bDone_task = false;
bool gStarted = false, sound_off = false, speaker_off = false, bperiod_notify = true, bStr2NumDone=false;

int gUp = 0, gLow = 0, multNum = 0, rmultNum = 0;
int kW=0, dret=0, fres = 0;
int gIgnoranceVal = 0; //limit for starting count down

time_t rawtime;
struct tm *pTime, *_pTime;

std::vector<int> amv_per_sec, amv_median;//, awatt_per_hour;
vector<string> vSerr;
vector<string> vSettings;

//string sWatts;

//funcs forward definitions
static void levels_accept(GtkWidget *wid, GtkWidget *win);
static void parameters_accept(GtkWidget *wid, GtkWidget *win);
static void helloWorld (GtkWidget *wid, GtkWidget *win);
static void start_usb_accept(GtkWidget *win, gpointer data);

static void toggle_snd_off(GtkToggleButton *check_button, gpointer data);

int ipc_notify(gpointer _pSharedMem_data1, gpointer _pMmap_data2);
int ipc_init(void);
int ipc_done(void);

//static bool save_app_params(gpointer);
int FinalizeDB(void);
//static void outtext(const char* mess);

//static void on_pad_added (GstElement *element, GstPad *pad, gpointer data);

int hid_send_feature_report(libusb_device_handle *hdev, const unsigned char *data, size_t length);

void corr_order_foo (gpointer data);
bool isorder_foo(void);

void
app_shutdown(GApplication *app, gpointer user_data);

static void
activate_quit (GSimpleAction *action, GVariant *parameter, gpointer user_data);

static void
toggle_spkr_off(GtkToggleButton *check_button, gpointer data);

static void
seek_to_time (GstElement *pipeline, gint64  time_nanoseconds);

static gboolean
flush_pipe (GstElement *pipeline)
{
    GstEvent* flush_start = gst_event_new_flush_start();
    gboolean ret = FALSE;
    ret = gst_element_send_event(GST_ELEMENT(pipeline), flush_start);

   /*if(ret)
    {
        ofstream outf;
        outf.open("temp.txt", ios::out);
        outf << "Ok" << endl;
        outf.close();
    }*/

   if(!ret) return ret;

   /*GstEvent* flush_stop = gst_event_new_flush_stop(TRUE);
   ret = gst_element_send_event(GST_ELEMENT(pipeline), flush_stop);*/


   return ret;
}

static int get_last_record_nav_mv()
{
  char SQLst[] = "select * from nav_mv where navID = $navID order by nID desc limit 1";
  char buf_tm[20];
  int hh = 0, mm = 0, min_period = 150, max_period = 300;
  int res = 0;
  CHours2Min h2m;

  navID = pTime->tm_mday;

  //ofstream outf;
  //outf.open("tmp.out", ios::out);

  sqlite3_finalize(pStmt);

  int ret = sqlite3_prepare_v2(pDB, &SQLst[0], strlen(SQLst) , &pStmt, NULL);
  ret = sqlite3_bind_int(pStmt, 1, navID);
  if(pTime->tm_hour >=0  && pTime->tm_hour < 8)
  {
     pTime->tm_wday == 0 ? pTime->tm_wday = 6 : --pTime->tm_wday;
     ret = sqlite3_bind_int(pStmt, 1, navID > 1 ? --navID : navID);
  }
  sqlite3_step(pStmt);
  nID = sqlite3_column_int(pStmt, 0);
  //nID != 0 ? ++nID : nID;
  //outf << "navID: " << navID << "nID:" << nID << endl;
  if(nID != 0)
  {
    int grainMM = 0;
    ++nID;
    strcpy((char*)buf_tm, (const char*)sqlite3_column_text(pStmt, 2));

    hh = pTime->tm_hour -  boost::lexical_cast<int>(&buf_tm[0], 2);
    mm = pTime->tm_min -  boost::lexical_cast<int>(&buf_tm[2], 2);
    mm += (hh * 60);

    h2m.setValue(min_per_tm);
    min_period = h2m.getMinutes();
    h2m.setValue(max_per_tm);
    max_period = h2m.getMinutes();

    //( mm >= min_period && mm <= max_period) ? kW = sqlite3_column_double(pStmt, 4) : kW = 0;
    //(mm >= min_period && mm <= max_period) || mm < -30 ? kW = 0 : kW = sqlite3_column_double(pStmt, 4);

    kW = sqlite3_column_int(pStmt, 4);

    if( (mm >= min_period && mm <= max_period) || isMakeZero_Watt_counter)
    {
        kW = 0;
    }
    /*grainMM = abs(mm - min_period);
    if(grainMM >= 0 && grainMM <= 10 )
    {
        kW = 0;
    }

    grainMM = abs(mm - max_period);
    if(grainMM >= 0 && grainMM <= 10 )
    {
        kW = 0;
    }*/

    /*if((mm >= min_period && mm <= max_period) || mm < 0 )
        kW = 0;
    else
        kW = sqlite3_column_double(pStmt, 4);*/

    //cout << "gotcha1 " << navID << " " << nID << " " << kW  << " " << buf_tm << " " << mm << " " << max_period << endl;
  }
  else if(nID == 0)
  {
     strncpy(SQLst, "select * from nav_mv order by nID desc limit 1", 46);
     //sqlite3_reset(pStmt);
     sqlite3_finalize(pStmt);
     ret = sqlite3_prepare_v2(pDB, SQLst, 46 , &pStmt, NULL);
     sqlite3_step(pStmt);
     nID = sqlite3_column_int(pStmt, 0);
     nID != 0 ? ++nID : nID;

     //cout << "gotcha2 " << navID << " " << nID << " " << kW << endl;
     //outf << "gotcha " << navID << " " << nID << " " << kW << " " << pTime->tm_wday << endl;
     res = -1;
   }
   //outf << "gotcha " << navID << " " << nID << " " << kW << " " << pTime->tm_wday << endl;
   //outf.close();
   //cout << "gotcha " << navID << " " << nID << " " << kW << endl;
  return res;
}

static void get_record_nav_watts(const char* dateidx)
{
  string SQLst = "select * from nav_watts where dateidx = :dateidx";

        //navID = pTime->tm_mday;
        int ret = sqlite3_prepare_v2(pDB, &SQLst[0], SQLst.length() , &pStmt, NULL);
        ret = sqlite3_bind_text(pStmt, 0, dateidx, sizeof(dateidx), NULL);
        sqlite3_step(pStmt);
}

bool copyf(std::istream &ifs, std::ostream &ofs)
{
 //std::streambuf *sb = new std::streambuf;

 //ifs.rdbuf(sb);
 //char buff[bufsize];
 /*while(!ifs.eof())
  {
   //std::cout << "test";
   //ofs << ifs.rdbuf();
   ifs.read(buff, sizeof(buff));
   ofs.write(buff, sizeof(buff));
  }*/
 ofs << ifs.rdbuf();

 return true;
}

void drop_db_tables(void)
{
 int count_day=1, ret;

 //ret = sqlite3_finalize(pStmt);

 const char drop_mv_sql[]   = "drop table nav_mv";
 const char drop_watts_sql[] = "drop table nav_watts";

 ret = sqlite3_prepare_v2(pDB, drop_mv_sql, -1, &pStmt, 0);
 ret = sqlite3_step(pStmt);
 //cout << "dropping... " << " " << ret << endl;

 ret = sqlite3_reset(pStmt);

 ret = sqlite3_prepare_v2(pDB, drop_watts_sql, -1, &pStmt, 0);
 ret = sqlite3_step(pStmt);

}

void restore_db_tables(void)
{
 sqlite3_finalize(pStmt);
 int count_day=1, ret;
 string restore_mv   = "CREATE TABLE nav_mv(nID int, navID int, timeID varchar(5), mV int, Watt int, primary key(navID, timeID));";
 string restore_watts = "CREATE TABLE nav_watts(dateidx varchar(10) primary key, navID int, Watt int);";

 ret = sqlite3_prepare_v2(pDB, &restore_mv[0], -1, &pStmt, 0);
 ret = sqlite3_step(pStmt);

 ret = sqlite3_reset(pStmt);

 ret = sqlite3_prepare_v2(pDB, &restore_watts[0], -1, &pStmt, 0);
 ret = sqlite3_step(pStmt);

}

void clear_db_tables(void)
{
 int count_day=1, ret;
 string del_sql   = "delete from nav_mv where navID = $navID";
 string del_watts = "delete from nav_watts where navID = $navID";

 ret = sqlite3_prepare_v2(pDB, &del_sql[0], -1, &pStmt, 0);

 for( ; count_day < 32; count_day++)
 {
   if(ret == SQLITE_OK)
   {
     ret = sqlite3_bind_int (pStmt, 1, count_day);
     if(ret == SQLITE_OK)
     {
        ret = sqlite3_step(pStmt);
        if(ret == SQLITE_DONE)
              ret = sqlite3_reset(pStmt);
      }
    }
 }

 //clear the nav_watts table
 ret = sqlite3_prepare_v2(pDB, &del_watts[0], -1, &pStmt, 0);

 for( count_day=1; count_day < 32; count_day++)
  {
    if(ret == SQLITE_OK)
    {
      ret = sqlite3_bind_int (pStmt, 1, count_day);

      if(ret == SQLITE_OK)
      {
        ret = sqlite3_step(pStmt);
        if(ret == SQLITE_DONE)
           ret = sqlite3_reset(pStmt);
      }
    }
  }
}

void backup_db_file(const char* time_appendx)
{
  char timeBuf[20];
  //std::string name_fl = "navbat_id";
  char name_fl[] = "navbat_id_nxt";
  std::ifstream inp(name_fl, std::ios_base::in | std::ios_base::binary);
  std::ofstream outp;//("navbat_id.bak");

  //gTimeStr.resize(20);
  if(time_appendx == NULL)
  {
    //strftime(&gTimeStr[0], 36, "%Y%m%d", pTime);
    strftime(timeBuf, 36, "%Y%m%d", pTime);
  }
  else
    //g_strlcpy(&gTimeStr[0], time_appendx, gTimeStr.length());
    g_strlcpy(timeBuf, time_appendx, sizeof(timeBuf));
  //gTimeStr.append(".bak");
  //name_fl.append(timeBuf);//gTimeStr);
  strcat(name_fl, timeBuf);
  outp.open(name_fl, std::ios_base::out | std::ios_base::binary );
  copyf(inp, outp);

  inp.close();
  outp.close();
}

double median(vector<double> &amv, int n)
{
 double retval = 0;
 int z;
 for( z = 0; (z < n/2); z++)
   retval = amv[z];
 if( n % 2 )
  retval = amv[z];
 else
  retval = (retval + amv[z]) / 2;
 return retval;
}

int mode(vector<int> &amv, int n)
{
	int instances = 0, tempInstances = 1, i = 1;
	int tempMode, retMode = 1;

	tempMode = amv[0];
	while(i < n)
	{
		while(amv[i] == tempMode)
		{
			++i;
			++tempInstances;
		}
		if(tempInstances > instances)
		{
			retMode = tempMode;
			instances = tempInstances;
		}
		tempInstances = 1;
		tempMode = amv[i];
		++i;
	}
	return retMode;
}

int insert_mv_nav(char* buf_tm)
{
    sqlite3_finalize(pStmt);
     //char buf_tm[20];
     dret == 0 ? dret = fres : dret;
     int ret = sqlite3_prepare_v2(pDB, &insert_next_mv[0], -1, &pStmt, 0);
      if(ret == SQLITE_OK)
      {
           ret = sqlite3_bind_int (pStmt, 1, nID);
           ret = sqlite3_bind_int (pStmt, 2, navID);
           ret = sqlite3_bind_text(pStmt, 3, static_cast<const char*>(buf_tm) /*&gTimeStr[0]*/, -1,  NULL);
           ret = sqlite3_bind_int(pStmt, 4, dret); //mV
           ret = sqlite3_bind_int(pStmt, 5, kW);

           ret = sqlite3_step(pStmt);

           if(ret != SQLITE_DONE)
            {
                //string msg_info = "SQL command evaluation not done...";
                //cout << nID << " " << navID << " " << buf_tm << " " << dret << " " << kW << " "<< ret <<endl;
                return -1;
            }
        //cout << nID << " " << navID << " " << buf_tm << " " << dret << " " << kW << " "<< ret <<endl;
       //cout << gTimeStr << endl;
      }
  return 0;
}
//ostream_iterator< double > ofile(cout, " ");
static int alarm_beep()
{
    DWORD i=2000, b=0;

    for(; i>=1700; i-=100)
    {
        //b = (i+100)*2;
        !speaker_off ? Beep(1400, 250) : speaker_off;
            //cout << i << " ";
    }
    //cout << endl;
    /*for(; i<=2000; i+=20)
    {
        //b = (i+350);
        !speaker_off ? Beep(i, 30) : speaker_off;
        //cout << i << " ";
    }*/
  return 0;
}

int ShutDown()
{
    //if(_CountDownTime < 1)
    {
        gStarted = false;
        if(bIsRunIPCnotifier)
        {
            strcpy(ipc_mes, "GuleGule");
            ipc_notify(pShared_mem, pMmap);
        }
        //system(gCmdLine);
        if( !CreateProcess( NULL,   // No module name (use command line)
            gCmdLine,        // Command line
            NULL,           // Process handle not inheritable
            NULL,           // Thread handle not inheritable
            FALSE,          // Set handle inheritance to FALSE
            0,              // No creation flags
            NULL,           // Use parent's environment block
            NULL,           // Use parent's starting directory
            &si,            // Pointer to STARTUPINFO structure
            &pi )           // Pointer to PROCESS_INFORMATION structure
        )
        {
            last_error = GetLastError();
            system(gCmdLine);

            /*ofstream outf;
            outf.open("tmp.out", ios::out);
            outf << "error" << " " << last_error << "\n";
            outf << gCmdLine << "\n";
            outf.close();*/

            //return;
        }

        //if(_spawnl(_P_NOWAITO, gCmdLine, param, param2, param3, NULL) == -1)
        /*{
            /*ofstream outf;
            outf.open("tmp.out", ios::out);
            outf << "error" << " " << errno << "\n";
            outf << gCmdLine << "\n";
            outf.close();/
            //strcat(gCmdLine, " -s -t 3");
            //system(gCmdLine);
        }*/
        if(gApp != NULL)
        {
          GVariant *bool_variant = g_variant_new_boolean(TRUE);

          activate_quit(NULL, bool_variant, G_APPLICATION(gApp));
        }
        return 1;
    }
  return 0;
}

static gpointer thread_func3(gpointer data)
{
    char buf_tm[20];
	bool /*bdlg_hidden,*/ bOnce = false;
//    GtkWidget *win = NULL;//GTK_WIDGET(data);
//    GtkWidget *dialog = NULL, *image = NULL, *content_area;

while(1)
{
	//usleep(500);
    //win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    //gtk_window_set_type_hint(GTK_WINDOW(win),GDK_WINDOW_TYPE_HINT_SPLASHSCREEN);

    //(win == NULL) && (window != NULL) ? win = gtk_dialog_new() : win;
    /*gtk_dialog_new_with_buttons ("Some Message",
                    GTK_WINDOW (win),
                    GTK_DIALOG_MODAL,
                    "_Ok",
                    GTK_RESPONSE_OK,
                    "_Cancle",
                    GTK_RESPONSE_CANCEL,
                    NULL);*/

    //gtk_widget_set_parent_window(GTK_WIDGET(win), GDK_WINDOW(window));
    /*if( win != NULL && window != NULL && !bOnce)
    {
    	gtk_window_set_transient_for(GTK_WINDOW(win), GTK_WINDOW(window));
    	gtk_window_set_destroy_with_parent(GTK_WINDOW (win), TRUE);
    	gtk_window_set_title(GTK_WINDOW(win), "Thread win popup...");

    	content_area = gtk_dialog_get_content_area (GTK_DIALOG (win));
    	//image=gtk_image_new_from_file("c:/test.png");//image_name);

    	image=gtk_image_new_from_file("test.png");//image_name);

    	gtk_container_add(GTK_CONTAINER(content_area), image);
    	//bdlg_hidden = FALSE;
    	//gtk_widget_hide(win);
    	//gtk_dialog_run(GTK_DIALOG(win));
    	//gtk_window_set_destroy_with_parent(GTK_WINDOW(data), true);
    	//g_signal_connect (win, "destroy", G_CALLBACK (gtk_widget_destroy), win);
    	bOnce = true;
    }*/


       /* if(bdo_some_thing)
            {
                //gtk_dialog_run(GTK_DIALOG(win));
                if(win != NULL)
	               //if(gtk_widget_is_visible (win) == FALSE)
	                   gtk_widget_show_all(win);
	             //sleep(1);
                //gtk_main();
                //gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(win));
                //cout << "test thread3" << endl;
                //bdo_some_thing = false;
                //gtk_widget_destroy (dialog);
            }
        if(!bdo_some_thing)
            {
                //sleep(1);
                if(win != NULL)
                  //if(gtk_widget_get_visible (win) == TRUE)
                        gtk_widget_hide(win);
                //gtk_main_quit();
            } //cout << "done thread3" << endl;*/
    //gtk_widget_destroy(win);
    if(bdo_some_thing)
    {
         switch(case_of_tasks)
         {
           case 0:
               {
                   //cout << "thread3 test0" << endl;
                   case_of_tasks=-1;
                   bdo_some_thing = false;
                   alarm_beep(); //usleep(250);

                   /*if(!bIsRunIPCnotifier)
                   {
                        gchar *pCurrentDir = g_get_current_dir();


                        strncpy(gCmdLine, pCurrentDir, strlen((const char*)pCurrentDir));
                        strncat(gCmdLine, "/", 1);
                        strncat(gCmdLine, "shutdown.exe", 12);
                        g_free((gpointer)pCurrentDir);
                        pCurrentDir = NULL;

                        g_strdelimit(gCmdLine, "\\", '/');
                    }*/
                }
           break;

           case 1:
           {
              case_of_tasks = -1;
              bdo_some_thing = false;

              //cout << "thread3 test1" << endl;
              /*strftime(buf_tm, sizeof(buf_tm),"%H%M", _pTime);
              insert_mv_nav(buf_tm) == 0 ? ++nID : 0;*/
              //memset(pStr2num, 0x0, sizeof(pStr2num));

              /*backup_db_file(NULL);
              //clear_db_tables();
              drop_db_tables();
              restore_db_tables();
              get_last_record_nav_mv();*/

           }break;
           case 2:
           {
               /*if(!bIsRunIPCnotifier)
               {
                    _spawnl(_P_NOWAITO, "c:/gstreamer/1.0/x86/bin/ipc_notifier.exe", NULL);//_execvpe("C:/gstreamer/1.0/x86/bin/ipc_notifier_2.exe", NULL);
                    bDo_ipc_notify = bIsRunIPCnotifier = true;
               }*/

               //memset(pStr2num, 0x0, sizeof(pStr2num));
               //if(!corr_order) corr_order_foo(NULL);
               bdo_some_thing = false;
               case_of_tasks = -1;

           }break;
           default:{ bdo_some_thing=false; case_of_tasks=-1; }
       }
       bDone_task = true;
       //cout << case_of_tasks << endl;
       //outtext("");
    }
    //else
    {
      !bDoUpdate  ? bDoUpdate = true : bDoUpdate;
      __asm__("nop"); usleep(1000);
		/*__asm{
			nop
		}*/

      //memset(pStr2num, 0x0, sizeof(pStr2num));

      //!bDoUpdate  ? bDoUpdate = true : bDoUpdate;

	  //if(case_of_tasks == -1) usleep(3000);

	  //should be commented out for release version. it's just for testing purpose
//#ifdef APP_IMITATE
	  strcpy(pWatts, "\nWatts:");//sWatts = "\nWatts:";
	  strcat(pWatts, Watt); //sWatts.append(Watt);
//#endif

      if(speakerOff_timeOut > 0)
        {
            //usleep(1000);
            --speakerOff_timeOut;//-=1;
        }
        /*else
        {
            speaker_off = false;
            cout << speakerOff_timeOut << endl;
        }*/
	  //if (speakerOff_timeOut <= 0 && !sound_off && speaker_off)
	  /*else if(!sound_off && speaker_off )
	  {
		  speaker_off = false;
		  //if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(radio2)))
		  {
			  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio2), FALSE);
		  }
		  //bdo_some_thing = true; case_of_tasks = 0;
	  }*/

	  if( !gStarted /*&& bDo_ipc_notify*/ || std::abs(fres) < gIgnoranceVal || !bDo_ipc_notify ) //printf("%d\n",_CountDownTime-=3);
        _CountDownTime-=1;
      else
        _CountDownTime != gCountDownTime ? _CountDownTime = gCountDownTime : _CountDownTime;
	 _CountDownTime < 1 ? ShutDown() : 0;

    }
   }//while(1)
}

static gpointer thread_func2(gpointer data)
{
 std::string insert_next_navID = "insert into nav_watts values(:dateidx, $navID, $Watt)";
 int ret, _timeOut=0, _counter=0;
 //int amv_median_size = amv_median.size();

 //double dret = 0;
 //double kW=0;
 char buf_tm[20];
 //struct tm *_pTime;

 //string del_sql   = "delete from nav_mv where navID = $navID";
 //string del_watts = "delete from nav_watts where navID = $navID";
 /*ofstream outf;
 outf.open("tmp.out", ios::out);*/
 while(1)
 {
     //cout << _timeout++ << endl;
    //#ifdef APP_IMITATE
    //if(!gStarted)
    //#else
    if(gStarted)
    //#endif // APP_IMITATE
        {

      //cout << timeOut;
      //isorder_foo();
      usleep(1000);
      //cout << str2num << endl;
      if(isorder_foo() /*&& !bTaskPause*/)
       {
         amv_per_sec[_timeOut] = fres;
         ++_timeOut;
         //c = 0;
         //corr_order = false;
       }

      if( (_timeOut != 0) && (_timeOut % 60 == 0) )
      {

        _timeOut ^= _timeOut;

        time( &rawtime );
       _pTime = localtime( &rawtime );
        //bdo_some_thing = true; case_of_tasks = 1;

       //struct tm *info;
       sort(amv_per_sec.begin(), amv_per_sec.end());
       dret = std::abs(mode(amv_per_sec, amv_per_sec.size()));

       //cout << dret << " " << amv_per_sec.size() << endl;

       //gTimeStr.assign(buf_tm);


       //cout << _timeOut  << " " << dret << endl;
       //if( abs(fres) >= 0.4) //|| fres < (-5.0) )
       if( dret >= gIgnoranceVal)//0.4)
       {
         //amv_median[_counter] = dret;
         ++_counter;
         //kW += (dret*16.666/0.26);
         kW += (dret*multNum);//64.1);
         isSettingsNew = true;
         //cout << "watts:" << kW << endl;
       }
       else
        {
            //cout << fres << endl;
         //_timeout++;
         //if((_timeout != 0) && (_timeout % 180 == 0))
			{
				//_timeout = 0;
				//kW +=0;
				__asm__(" nop ");
				//__asm { nop }
          }
        }
       //cout << "watts:" << kW << " " << dret << " " << fres << endl;


       //outf << _counter << " " << amv_median_size << endl;

       //g_ascii_dtostr(Watt, 9, kW);
	   g_snprintf(Watt, 9, "%d", kW);


	   //strcpy(pWatts, "\nWatts:");//sWatts = "\nWatts:";
	   //strcat(pWatts, Watt); //sWatts.append(Watt);

       //copy(amv_per_sec.begin(), amv_per_sec.end(), ofile);

       //cout << gTimeStr << " " << r << " " << amv_median[r] << " " << kW << " " << Watt << endl;

      //if (gTimeStr.compare("0800") == 0)
      /*if ((_pTime->tm_hour == 0) && (_pTime->tm_min >= 0) && (pTime->tm_wday != _pTime->tm_wday))
        save_app_params(_pTime);*/
      if ((_pTime->tm_hour == 8) && (_pTime->tm_min >= 0) && (pTime->tm_wday != _pTime->tm_wday))
      {
       //dret = median(amv_median, amv_median.size());
       //awatt_per_hour.push_back(dret/0.26);

       //sort(awatt_per_hour.begin(), awatt_per_hour.end());

       strftime(buf_tm, sizeof(buf_tm), "%Y%m%d", pTime);
       //gTimeStr.assign(buf_tm);

       sqlite3_finalize(pStmt);
       ret = sqlite3_prepare_v2(pDB, &insert_next_navID[0], -1, &pStmt, 0);

       //kW = median(awatt_per_hour, awatt_per_hour.size());

       if(ret == SQLITE_OK)
        {
          //string msg_info = "insert SQL statement has error...";
          //MessageBox(msg_info);
          //return;

          ret = sqlite3_bind_text(pStmt, 1, (const char*)buf_tm /*&gTimeStr[0]*/, -1,  NULL);
          ret = sqlite3_bind_int (pStmt, 2, navID);
          ret = sqlite3_bind_int(pStmt, 3, kW);

          //if( ret != SQLITE_OK) cout << "Error in bind" <<endl;

          ret = sqlite3_step(pStmt);

          /*if(ret != SQLITE_DONE)
          {
           //string msg_info = "SQL command evaluation not done...";
           //cout << msg_info << endl;
           //MessageBox(msg_info);
           //return;
          }*/
        }

        //back up the DB for removing useless dates for next month
       if( _pTime->tm_mday == 1 )//&& _pTime->tm_hour == 8 && _pTime->tm_min >= 0 )//|| pTime->tm_mday == 29 || pTime->tm_mday == 30 || pTime->tm_mday == 31)
        {
           nID = 1;
           ret = sqlite3_finalize(pStmt);
           //cout << "from thread..." << ret << endl;

           strftime(buf_tm, sizeof(buf_tm), "%Y%m%d", pTime);

           backup_db_file((const char*)buf_tm);
           //backup_db_file(NULL);

           drop_db_tables();
           restore_db_tables();

           //clear_db_tables();
        }//cleaning sqlite3 DB nav_watts and nav_mv tables

        //back to the...
         /*__asm__
         (
             //"xor %0, %0\n": "=r"(kW)
            "mov 0, %0\n": "=r"(kW)
         );*/kW = 0;
        navID = _pTime->tm_mday;
        *pTime = *_pTime;  //pass the date(yyyy.mm.dd HH.MM) of the new today
        //awatt_per_hour.clear();
        strftime(buf_tm, sizeof(buf_tm),"%H%M", _pTime);
        //gTimeStr.assign(buf_tm);
        sumWatt = "|";
        sumWatt.append(Watt);

      }//Watts and next navID

    if(_counter >= amv_median.size())
    {
     /*__asm__
     (
        "xor %0, %0\n": "=r"(_counter) //r = 0;
     );*/
	 _counter ^= _counter;
     dret = std::abs(mode(amv_median, amv_median.size()));
     //kW += dret/0.26;
     //awatt_per_hour.push_back(dret/0.26);
     //g_ascii_dtostr((gchar*)Watt, 10, kW);
     //cout << "1h mV median: "<< dret <<" "<< dret/0.26 << endl;

      //bdo_some_thing = true; case_of_tasks = 1;
      strftime(buf_tm, sizeof(buf_tm),"%H%M", _pTime);
      ++nID;
      insert_mv_nav(buf_tm) == 0 ? nID : 0;
    }

       //gTimeStr.clear();
       //amv_per_sec.clear();
      }
  } //c = 0;
  else
    {
        usleep(1000);
        //g_thread_yield();
    }
 }
 //outf.close();
}

static gpointer
thread_func( gpointer data )
{
 //gint len = 9, c=0, i=0, actual = 0, base[5];
 const int len = 9;

 int actual=0, ipc_notifier_counter=0, /*len = 8,*/ i = 0, z = 0;
 int u = 0, c = 0;
 unsigned char buf[9], strBuf[9];
 //int _counter = 0;
 stringstream ss;
 //string::const_iterator it, end;
 #ifdef APP_IMITATE
 string target;
 char  tmp_str [10][10] = {
                          "003510301",
                          "003410301",
                          "003310301",
                          "003110301",
                          "003210301",
                          "003710301",
                          "000361101",
                          "000321101",
                          "000321101",
                          "000311101"  }; //imitate
 #endif // APP_IMITATE

 //char *pEnd;
 //c=0;

 //memset(buf,0x00,sizeof(buf));
 //str2num.resize(8);//reserve(8);

 while (1)//( actual  >= 0 )
   {
       #ifdef APP_IMITATE
       usleep(30); //imitate
       #endif
       //cout << _timeout++ << endl;
       if(gStarted)
        {
        if(thread_counter  == len /*&& !corr_order*/ )
        {
            //cout << thread_counter << " " << u << endl;
            /*__asm__
            (
                "xor %0, %0\n": "=r"(thread_counter) //c ^= c;
            );*/
			thread_counter ^= thread_counter;
			//memset(strBuf, 0x0, 9);

			#ifdef APP_IMITATE
			(u < len) ? ++u  : u ^=u;
			#endif // APP_IMITATE

			ss >> strBuf;//str2num;
			//ss.clear();
			std::stringstream().swap(ss);

            if(!corr_order) corr_order_foo(NULL);
            //bdo_some_thing = true; case_of_tasks = 2;

         //corr_order = true;
         //ss.clear();
         //str2num.clear();
         //for(i=0; i < len; i++)
         {
          //ss << got_nums[i];
          //ss << tmp;//[i];
         }

         //ss >> str2num;

         //ss.clear();
         //str2num.assign( tmp);
         //base[2] = strtoul((char*)&tmp[5], reinterpret_cast<char**>(&tmp[6]), 10);
         //ss << tmp[5]; ss >> target;

         //str2num.clear();
         //str2num = "000114331";
         //if(len > 5)
          {
            /*base[2] = std::stod(string(1,str2num[5]), nullptr); //dot position in
           base[3] = std::stod(string(1,str2num[6]), nullptr); //DC or AC
           base[4] = std::stod(string(1,str2num[7]), nullptr); //DC or AC
           base[5] = std::stod(string(1,str2num[8]), nullptr); //for determainin + or -
           //cout << str2num;*/
           //try
           {
               /*base[0] = boost::lexical_cast<int>(&str2num[5], 1); //dot position in
               base[1] = boost::lexical_cast<int>(&str2num[6], 1); //DC or AC
               base[2] = boost::lexical_cast<int>(&str2num[7], 1); //DC or AC
               base[3] = boost::lexical_cast<int>(&str2num[8], 1); //for determining + or -*/

               /*for(int c=5; c<9; ++c)
                cout << strBuf[c];
               cout << endl;*/

			   base[0] = boost::lexical_cast<int>(strBuf[5]); //dot position in
			   base[1] = boost::lexical_cast<int>(strBuf[6]); //DC or AC
			   base[2] = boost::lexical_cast<int>(strBuf[7]); //DC or AC
			   base[3] = boost::lexical_cast<int>(strBuf[8]); //for determining + or -
           }
           //catch(...)
           //catch(boost::bad_lexical_cast)
           {
               //FinalizeDB();
           }

		   memset(pStr2num, 0x0, 5);//sizeof(pStr2num));

		   /*if (base[0] < len/*str2num.length()/ && base[0] != 0)
			   isCovertV2mV ? pStr2num[4] = '.' : pStr2num[base[0]] = '.';
		   else
		   if (base[1] == 3)
			   pStr2num[3] = '.';*/

		   if (base[3] == 5 || base[3] == 6 || base[3] == 4)
			   pStr2num[0] = '-';



		   for (z=0,i=0/*, it = str2num.begin(), end = str2num.end()*/; i < 5/*str2num.length()-2/*it != end*/; ++i)
		   {
			   /*if (pStr2num[i] == '.' || pStr2num[i] == '-')
			   {
				   continue;
			   }*/
			   //pStr2num[i] = *it;
			   pStr2num[i] = strBuf[i];
			   ++z;
			   //++it;
			   //cout << *it;
		   }

		   //cout << pStr2num << endl;

           /*if(base[0] < str2num.length() && base[0] != 0)
            str2num.insert(base[0], ".");//",");
           else
           if(base[1] == 3)
            str2num.insert(3,".");//",");

           if (base[3] == 5 || base[3] == 6 || base[3] == 4){
             str2num.insert(0 , "-");
             str2num.resize(7);
            }
           else
            str2num.resize(6);*/

            //str2num.erase(6, 4);
            //cout << str2num << endl;


           //if(strncmp((const char *)&(strBuf[5]) , "0301", 4) == 0)
           //if(strcmp((const char *)&(strBuf[5]) , "0301") == 0)
           //if(strCmp((const char*)&strBuf[5]))

            {
              fres = boost::lexical_cast<int>(&pStr2num[0],5);
              //cout << pStr2num << " " << fres << endl;

              /*if(!sound_off)
              {
                if ( ( std::abs(fres) < gLow ||  std::abs(fres) > gUp ) /&& (!bTaskPause) /)
                {
                    if(bperiod_notify)
                            bperiod_notify = false;
                    //cout << "test alarm" << endl;
                    if(_count_alrm != 3)
                        ++_count_alrm;
                    //++_count_alrm;

                    if (/(!sound_off) &&/ (_count_alrm >= 3) /&& abs(fres) > 0.15/)
                    {
                        bdo_some_thing = true; case_of_tasks = 0;

                        //g_object_set (G_OBJECT (playbin2), "uri", "file:///C:/test.ogg", NULL);
                        g_object_set (G_OBJECT (playbin2), "uri", alarm_audio_file_path, NULL);
                        //g_object_set (G_OBJECT (playbin2), "uri", "file:////home/mdt/test.ogg", NULL);
                        gsStateChRet = gst_element_set_state (GST_ELEMENT(playbin2), GST_STATE_PLAYING);

                        //if(!g_main_loop_is_running(gLoop) ) g_main_loop_run(gLoop);
                        //if(gsStateChRet == GST_MESSAGE_EOS) _count_alrm = 0;

                    }
                    /else
                    {
                        //bdo_some_thing = false; case_of_tasks = -1;
                        //gst_element_set_state (GST_ELEMENT(playbin2), GST_STATE_PAUSED); //GST_STATE_READY);

                        //flush_pipe(GST_ELEMENT(playbin2));

                        //gst_element_set_state (GST_ELEMENT(playbin2), GST_STATE_READY);
                        //gst_element_set_state (GST_ELEMENT(playbin2), GST_STATE_PAUSED);
                        //gst_element_set_state (GST_ELEMENT(playbin2), GST_STATE_READY);
                        //seek_to_time(playbin2, 0);

                    }/
                }
                else
                {
                    //!bperiod_notify ? gst_element_set_state(GST_ELEMENT(playbin2), GST_STATE_READY) : 0;
                    //_count_alrm ^= _count_alrm;
                    if(!bperiod_notify)
                    {
                        _count_alrm ^= _count_alrm;
                        bperiod_notify = true;
                        //g_object_set (G_OBJECT (playbin2), "uri", "file:///C:/notify.ogg", NULL);
                        g_object_set (G_OBJECT (playbin2), "uri", ipc_notify_audio_file_path, NULL);
                        gst_element_set_state (GST_ELEMENT(playbin2), GST_STATE_PAUSED);// GST_STATE_READY);
                        //flush_pipe(GST_ELEMENT(playbin2));

                        //gst_element_set_state (GST_ELEMENT(playbin2), GST_STATE_READY);
                        //gst_element_set_state (GST_ELEMENT(playbin2), GST_STATE_PAUSED);
                        seek_to_time(playbin2, 0);
                    }
                }
            }*/

            }
            //memset(strBuf, 0x0, 9);
            //cout << &pStr2num[0] << " " << &strBuf[9] << endl;
            //printf("%d%d%d%d\n", base[0],base[1],base[2],base[3]);

           //fres = std::stod(str2num, nullptr); //not fully converting

		   /*if (base[1] == 3)  strncpy(&serr[0], "DC mv", 5);//serr = "DC mV";
           else
           if(base[1] == 1 && base[4] == 0){
            serr = "DC";
           }*/
		   if (!bDoUpdate)
		   {
			   if (base[1] == 1 && base[4] == 0)
			   {
				   //serr = "DC";
				   indx_vSerr = 0;
			   }
			   if (base[1] == 3)
			   {
				   //strncpy(&serr[0], "DC mv", 5);
				   indx_vSerr = 1;
			   }

			   if (base[1] == 2 && base[2] == 3)
			   {
				   //serr = "AC";
				   if (base[3] == 1)
					   //serr.append(" Auto");
					   indx_vSerr = 2;
				   else
					   //if( base[5] == 2 )
					   //serr.append( " NotAuto");
					   indx_vSerr = 3;
			   }
			   //bDoUpdate = true;
		   }

          }

         //fres = atof((const char*)&str2num[0]);
         //cout << fres << endl;
         //fres = strtof((char*)&str2num[0], NULL);
         //g_snprintf((gchar*)buf, sizeof(buf), "%.4f", fres);
         //fres = atof((gchar*)buf);

         //cout << str2num << " " << fres << endl;
         //for(i=0; i< 179999; i++);

         //gtk_text_buffer_insert_with_tags_by_name(buffer, &txt_iter, &str2num[0], -1, "big", "blue_foreground" ,"wide_margins", NULL);
         //tmp.clear();

		  //ss.str(std::string());
		  //ss.clear();

		  //std::stringstream().swap(ss);
        }

         //r = libusb_interrupt_transfer(hdev, 0x82, (unsigned char*)buf, sizeof(buf), &actual, 5000);
         //libusb_interrupt_transfer(hdev, 0x82, (unsigned char*)buf, sizeof(buf), &actual, 5000);
         //actual = 1; i=1;
         #ifdef APP_IMITATE
         buf[1] = tmp_str[u][thread_counter]; //imitate
         #else
         libusb_interrupt_transfer(hdev, 0x82, (unsigned char*)buf, sizeof(buf), &actual, 5000);
         #endif // APP_IMITATE
         buf[1] &= 0x7F;

         #ifdef APP_IMITATE
         if( isdigit(buf[1]) !=0  && thread_counter <= len /*&& actual != 0*/) //imitate
         #else
         if( isdigit(buf[1]) !=0  && thread_counter <= len && actual > 0)
         #endif // APP_IMITATE
         {
          //cout << actual << " " << sizeof(buf) << endl;
          ss << buf[1];
          ipc_notifier_counter ^= ipc_notifier_counter;
          ++thread_counter;
         }
         else
         //if(boost::lexical_cast<BYTE>(buf[1]) == 0)
            {
                ipc_notifier_counter != 70 ? ++ipc_notifier_counter : 0;
            }
         /*else
            {
                ipc_notifier_counter ^= ipc_notifier_counter;
            }*/
            ipc_notifier_counter >= 70 ? bDo_ipc_notify = false : bDo_ipc_notify = true;
         //printf("%x\n",buf[1]);
        //actual ^= actual;
        //memset(buf,0x0,sizeof(buf));
       }
       else
       {
           /*__asm__
            (
                "xor %0, %0\n": "=r"(thread_counter) //c ^= c;
            );*/
		   thread_counter ^= thread_counter;
           //g_thread_yield();
           usleep(1000);
       }
    }
	//return 0;
}

void
corr_order_foo (gpointer data)
{
  g_mutex_lock (&data_mutex);
  corr_order = true;
  g_cond_signal (&data_cond);
  g_mutex_unlock (&data_mutex);
}

bool
isorder_foo (void)
{
  bool data;

  g_mutex_lock (&data_mutex);
  while (!corr_order)
    g_cond_wait (&data_cond, &data_mutex);
  data = corr_order;
  //corr_order = false;
  g_mutex_unlock (&data_mutex);

  return data;
}

gboolean on_key_press(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
  GdkModifierType mods = gtk_accelerator_get_default_mod_mask();

 /*if( ((event->state & GDK_CONTROL_MASK) == GDK_CONTROL_MASK ) ||
     ((event->state & GDK_MOD1_MASK) == GDK_MOD1_MASK )||
     ((event->state & GDK_META_MASK) ==  GDK_META_MASK) )
     {
       if( event->keyval == GDK_KEY_R  || event->keyval == GDK_KEY_r )
         isRecOn = !isRecOn;
     }*/
 //if(event->keyval == GDK_KEY_space) { bTaskPause = !bTaskPause; return FALSE;};

 if((event->state & mods) == GDK_CONTROL_MASK )
 {
     if( event->keyval == GDK_KEY_T  || event->keyval == GDK_KEY_t )
        isCovertV2mV = !isCovertV2mV;
 }

 if( event->keyval == GDK_KEY_F3 )
 {
     levels_accept(NULL, window);
 }

 if( event->keyval == GDK_KEY_F7 )
 {
     parameters_accept(NULL, window);
 }

 if( event->keyval == GDK_KEY_F9 )
 {
     helloWorld(NULL, NULL);
     if(gtk_widget_get_sensitive(btn_start_getval))
        start_usb_accept(NULL, NULL);
 }

 if( event->keyval == GDK_KEY_F5 )
 {
   //toggle_snd_off(NULL, NULL);
   //g_signal_emit_by_name(G_OBJECT(radio2), "toggled");
   //gtk_widget_set_state(radio1, GTK_STATE_ACTIVE);
   if (sound_off)
   {
        sound_off = false;
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio1), FALSE);
        //gtk_widget_set_sensitive(radio2, FALSE);
   }
   else
   {
       sound_off = true;
       gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio1), TRUE);
       //gtk_widget_set_sensitive(radio2, TRUE);
   }
 }
 if( event->keyval == GDK_KEY_F12 )
 {
     activate_quit(NULL, NULL, G_APPLICATION(gApp));
 }
 return FALSE;
}

static gboolean
cb_timeout( gpointer data )
{
 GtkTextBuffer *buffer = GTK_TEXT_BUFFER(data);
 GtkTextBuffer *aux_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(aux_view));
 GtkTextIter txt_iter, aux_txt_iter;
 gchar *label = NULL;


 if (bDoUpdate)
 {
   gtk_text_buffer_set_text(aux_buffer, "", 0);
   gtk_text_buffer_get_iter_at_offset(aux_buffer, &aux_txt_iter, 0);
 }

 //gchar d2str[10];
 //string sWatts;

 /*if(bDone_task)
 {
   outtext(""); bDone_task = false;
 }*/

 //static short _count_alrm = 0;

 //G_LOCK( corr_order );

 //label = g_strdup_printf( "%s", &str2num[0] );
 //G_UNLOCK( corr_order );

 //gtk_button_set_label( GTK_BUTTON( data ), label );

 //buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));

 //buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));

 /*++period_notify_count;
 	if( period_notify_count >= period_ipc_notify )
	{
	   //bIsRunIPCnotifier ? bIsRunIPCnotifier : _spawnl(_P_NOWAITO, "C:/gstreamer/1.0/x86/bin/ipc_notifier_2.exe", NULL);//_execvpe("C:/gstreamer/1.0/x86/bin/ipc_notifier_2.exe", NULL);
       //bIsRunIPCnotifier = true;
       if(!bIsRunIPCnotifier)
       {
        _spawnl(_P_NOWAITO, "C:/gstreamer/1.0/x86/bin/ipc_notifier_2.exe", NULL);//_execvpe("C:/gstreamer/1.0/x86/bin/ipc_notifier_2.exe", NULL);
         bIsRunIPCnotifier = true;
       }
	  //cout << "notify" << endl;
	  period_notify_count ^= period_notify_count;

	  if(playbin2)
		{
			//period_notify_count ^= period_notify_count;
			//gst_element_set_state (GST_ELEMENT(playbin2), GST_STATE_READY);
			//g_object_set (G_OBJECT (playbin2), "uri", "file:////home/mdt/notify.ogg", NULL);
			g_object_set (G_OBJECT (playbin2), "uri", "file:///c:/notify.ogg", NULL);
			gsStateChRet = gst_element_set_state (GST_ELEMENT(playbin2), GST_STATE_PLAYING);
		}

	  ipc_notify(pShared_mem, pMmap);
	}*/
if(gStarted)
{

   ++period_notify_count;
   //#ifndef APP_IMITATE
   if (period_notify_count >= period_ipc_notify)
    {
       /*if(!bIsRunIPCnotifier)
        {
          _spawnl(_P_NOWAITO, "c:/gstreamer/1.0/x86/bin/ipc_notifier.exe", NULL);//_execvpe("C:/gstreamer/1.0/x86/bin/ipc_notifier_2.exe", NULL);
          bIsRunIPCnotifier = true;
        }*/
      bDo_ipc_notify ? ipc_notify(pShared_mem, pMmap) : bDo_ipc_notify;
    }
   //#endif // APP_IMITATE

	if(bperiod_notify && period_notify_count !=0 && period_notify_count%period_mod == 0)
	{
		//bDoUpdate = true;
		if(playbin2)
		{
			period_notify_count ^= period_notify_count;
			//gst_element_set_state (GST_ELEMENT(playbin2), GST_STATE_READY);
			//g_object_set (G_OBJECT (playbin2), "uri", "file:////home/mdt/notify.ogg", NULL);
			//g_object_set (G_OBJECT (playbin2), "uri", "file:///c:/notify_3.ogg", NULL);
			if(_count_alrm == 0)
                gsStateChRet = gst_element_set_state (GST_ELEMENT(playbin2), GST_STATE_PLAYING);

			//if(!g_main_loop_is_running(gLoop) ) g_main_loop_run(gLoop);
		}
	}

    if( speaker_off && !sound_off )
    {
        if(speakerOff_timeOut <= 0)
        {
            speaker_off = false;
            //if(gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(radio2)))
            {
                gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(radio2), FALSE);
            }
            //bdo_some_thing = true; case_of_tasks = 0;
        }
    }
 //else
   //  gst_element_set_state (GST_ELEMENT(playbin2), GST_STATE_READY);

  //cout << str2num.length() << endl;
  gtk_text_buffer_set_text (buffer, "", 0);
  gtk_text_buffer_get_iter_at_offset (buffer, &txt_iter, 0);
  gtk_text_buffer_insert_with_tags_by_name(buffer, &txt_iter, "\n\n", -1, "double_spaced_line",NULL);

  if( corr_order )
    {
        //c ^= c;
        /*__asm__
        (
            "xor %0, %0\n": "=r"(thread_counter)
        );*/
	    thread_counter ^= thread_counter;
        g_mutex_lock (&data_mutex);
        corr_order = false;
        g_cond_signal (&data_cond);
        g_mutex_unlock (&data_mutex);

        if(!sound_off)
        {
          if ( ( std::abs(fres) < gLow ||  std::abs(fres) > gUp ) /*&& (!bTaskPause) */)
          {
           if(bperiod_notify)
                    bperiod_notify = false;
                    //cout << "test alarm" << endl;
           if(_count_alrm != 3)
                   ++_count_alrm;
                    //++_count_alrm;

           if (/*(!sound_off) &&*/ (_count_alrm >= 3) /*&& abs(fres) > 0.15*/)
           {
             bdo_some_thing = true; case_of_tasks = 0;

                        //g_object_set (G_OBJECT (playbin2), "uri", "file:///C:/test.ogg", NULL);
             g_object_set (G_OBJECT (playbin2), "uri", alarm_audio_file_path, NULL);
                        //g_object_set (G_OBJECT (playbin2), "uri", "file:////home/mdt/test.ogg", NULL);
             gsStateChRet = gst_element_set_state (GST_ELEMENT(playbin2), GST_STATE_PLAYING);

                        //if(!g_main_loop_is_running(gLoop) ) g_main_loop_run(gLoop);
                        //if(gsStateChRet == GST_MESSAGE_EOS) _count_alrm = 0;

            }
                    /*else
                    {
                        //bdo_some_thing = false; case_of_tasks = -1;
                        //gst_element_set_state (GST_ELEMENT(playbin2), GST_STATE_PAUSED); //GST_STATE_READY);

                        //flush_pipe(GST_ELEMENT(playbin2));

                        //gst_element_set_state (GST_ELEMENT(playbin2), GST_STATE_READY);
                        //gst_element_set_state (GST_ELEMENT(playbin2), GST_STATE_PAUSED);
                        //gst_element_set_state (GST_ELEMENT(playbin2), GST_STATE_READY);
                        //seek_to_time(playbin2, 0);

                    }*/
        }
        else
        {
                    //!bperiod_notify ? gst_element_set_state(GST_ELEMENT(playbin2), GST_STATE_READY) : 0;
                    //_count_alrm ^= _count_alrm;
           if(!bperiod_notify)
           {
             _count_alrm ^= _count_alrm;
             bperiod_notify = true;
                        //g_object_set (G_OBJECT (playbin2), "uri", "file:///C:/notify.ogg", NULL);
             g_object_set (G_OBJECT (playbin2), "uri", ipc_notify_audio_file_path, NULL);
             gst_element_set_state (GST_ELEMENT(playbin2), GST_STATE_PAUSED);// GST_STATE_READY);
                        //flush_pipe(GST_ELEMENT(playbin2));

                        //gst_element_set_state (GST_ELEMENT(playbin2), GST_STATE_READY);
                        //gst_element_set_state (GST_ELEMENT(playbin2), GST_STATE_PAUSED);
              seek_to_time(playbin2, 0);
            }
       }
   }

    /*if(_CountDownTime < 1)
    {
        strcpy(ipc_mes, "GuleGule");
        ipc_notify(pShared_mem, pMmap);
        system(gCmdLine);
        activate_quit(NULL, NULL, G_APPLICATION(gApp));
        //break;
                //g_signal_emit_by_name(G_OBJECT(window), "destroy");
    }*/
    //if(_CountDownTime < 5) fres = 0.5;

    /*if(_CountDownTime < 1) ShutDown();
    if(std::fabs(fres) < gIgnoranceVal || !bDo_ipc_notify) --_CountDownTime;
    else _CountDownTime != gCountDownTime ? _CountDownTime = gCountDownTime : 0;*/

   }

  //str2num.append("\n");
  //gtk_text_buffer_insert_with_tags_by_name(buffer, &txt_iter, &str2num[0], -1, "right_justify","big", "blue_foreground", NULL);
  gtk_text_buffer_insert_with_tags_by_name(buffer, &txt_iter, &pStr2num[0], 5, "right_justify","big", "lightseagreen_foreground", NULL);
  gtk_text_buffer_get_end_iter(buffer, &txt_iter);

  /*sWatts = "\nWatts:";
  sWatts.append(Watt);*/
  //gtk_text_buffer_insert_with_tags_by_name(buffer, &txt_iter, &tmp[0], -1, "right_justify", "little_big", NULL);
  gtk_text_buffer_insert_with_tags_by_name(buffer, &txt_iter, pWatts/*&sWatts[0]*/, -1, "right_justify", "little_big", "lightgray_foreground", NULL);

  //if(kW == 0)
  {
   //tmp.append(&sumWatt[0]);

   //gtk_text_buffer_insert_with_tags_by_name(buffer, &txt_iter, &sumWatt[0], -1, "right_justify", "little_big", "red_foreground", NULL);
   gtk_text_buffer_insert_with_tags_by_name(buffer, &txt_iter, &sumWatt[0], -1, "right_justify", "little_big", "orange_foreground", NULL);
  }

  if (bDoUpdate)
  {
	  /*tmp = vSettings[0];
	  tmp += vSettings[1];
	  tmp += vSettings[2];
	  tmp += vSettings[3];
	  tmp += vSettings[4];
	  tmp += vSettings[5];
	  cout << tmp << endl;*/

	  /*aux_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(aux_view));
	  gtk_text_buffer_set_text(aux_buffer, "", 0);

	  gtk_text_buffer_get_iter_at_offset(aux_buffer, &txt_iter, 0);*/
	  //gtk_text_buffer_get_iter_at_line(buffer, &txt_iter, 0);
	  //gStarted ?
	  //isSettingsNew ?
		  //gtk_text_buffer_insert_with_tags_by_name(buffer, &txt_iter, "Process Paused!\n", -1, "right_justify", NULL);
		  gtk_text_buffer_insert_with_tags_by_name(aux_buffer, &aux_txt_iter, "Process Started!\n", -1,
		  "right_justify", "gray_foreground", NULL); /*:
		  gtk_text_buffer_insert_with_tags_by_name(buffer, &txt_iter, "Process Stopped!\n", -1, "right_justify",
		  "gray_foreground", NULL);*/
	  //gtk_text_buffer_insert_with_tags_by_name(buffer, &txt_iter, "Process Started!\n", -1, "right_justify", NULL);

	  gtk_text_buffer_get_end_iter(aux_buffer, &aux_txt_iter);
	  //gtk_text_buffer_insert_with_tags_by_name(buffer, &txt_iter, &serr[0], -1, "little_big", NULL);
	  gtk_text_buffer_insert_with_tags_by_name(aux_buffer, &aux_txt_iter, &(vSerr[indx_vSerr])[0], -1, "little_big", "gray_foreground", NULL);

	  //Params output:up, low, CFWC

	  /*tmp = "| Up:";
	  sprintf(d2str, "%.2f", gUp);
	  tmp.append(d2str);

	  tmp.append(" | Low:");
	  sprintf(d2str, "%.2f", gLow);
	  tmp.append(d2str);

	  tmp.append(" | CFWC:");
	  sprintf(d2str, "%.3f", rmultNum);
	  tmp.append(d2str);*/

	  gtk_text_buffer_insert_with_tags_by_name(aux_buffer, &aux_txt_iter, &tmp[0], -1, "right_justify", "blue2_foreground", NULL);

	  bDoUpdate = false;//!bDoUpdate;
  }
 }
 else
 {
    if(bDoUpdate)
    {
        gtk_text_buffer_insert_with_tags_by_name(aux_buffer, &aux_txt_iter, "Process Stopped!\n", -1, "right_justify",
            "orange_foreground", NULL);
        bDoUpdate = false;//!bDoUpdate;
	}
 }

 /*if(speakerOff_timeOut <= 0 && !sound_off)
 {
   speaker_off = false;
   if(gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(radio2)))
   {
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(radio2), FALSE);
   }
   bdo_some_thing = true; case_of_tasks = 0;
 }*/
 //g_free( label );
 //c = 0;
 return( TRUE );
}

static void quit_app(GtkWidget *win, gpointer data)
{
 //system(gCmdLine);
 activate_quit(NULL, NULL, data);
}

/*static void correct_usb_accept(GtkWidget *win, gpointer data)
{
 //G_LOCK( tmp );
 //tmp.clear();
 //G_UNLOCK( tmp );
 thread_counter=0;
 gst_element_set_state (playbin2, GST_STATE_READY);
 //gst_element_abort_state(GST_ELEMENT(pipeline));
}*/

GThread *thread = NULL;
GThread *thread2 = NULL;
GThread *thread3 = NULL;
static void start_usb_accept(GtkWidget *win, gpointer data)
{
 GtkTextBuffer *buffer = NULL;
 //GtkTextIter txt_iter;
 //unsigned char buf[64];
 //int i, c, len, actual, base[6];

 //stringstream ss;
 //string str2num, tmp;

 //std::string str = "Process STARTED!!!\n";

 /*buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));
 gtk_text_buffer_set_text (buffer, "", 0);
 gtk_text_buffer_get_iter_at_offset (buffer, &txt_iter, 0);
 gtk_text_buffer_insert_with_tags_by_name(buffer, &txt_iter, &str[0], -1, "right_justify", "blue_foreground" ,"wide_margins", NULL);*/

 //GThread *_thread = (GThread*)(data);
 GError *error, *error2;

 gtk_widget_grab_focus(btn_start_getval);
 if(!gStarted)
 {
  if( thread == NULL )
  {
   //g_thread_unref( _thread );
   thread = g_thread_try_new("mine", thread_func, NULL, &error);
   if( ! thread )
   {
    //g_print( "Error: %s\n", error->message );
    return;
   }
   //c=0;
   //cout << "test" << endl;
   //gStarted = true;
  }
 if( thread2 == NULL )
  {
   thread2 = g_thread_try_new("mine2", thread_func2, NULL, &error2);
   if( !thread2 )
   {
    //g_print( "Error: %s\n", error2->message );
    return;
   }
   r = 0;
   amv_per_sec.resize(60); //reserve(60);
   amv_median.resize(10); //reserve(10);
   amv_median_size = amv_median.size();
  }
   if( thread && thread2 )
    {
        gStarted = true;
        //bdo_some_thing = true; case_of_tasks = 2; //launch ipc_notifier.exe
        if(!bIsRunIPCnotifier)
        {
            //_spawnl(_P_NOWAITO, (const char*)ipc_notifier_path /*"c:/gstreamer/1.0/x86/bin/ipc_notifier.exe"*/, NULL);//_execvpe("C:/gstreamer/1.0/x86/bin/ipc_notifier_2.exe", NULL);
            if( !CreateProcess( ipc_notifier_path,   // No module name (use command line)
            NULL,        // Command line
            NULL,           // Process handle not inheritable
            NULL,           // Thread handle not inheritable
            FALSE,          // Set handle inheritance to FALSE
            0,              // No creation flags
            NULL,           // Use parent's environment block
            NULL,           // Use parent's starting directory
            &si,            // Pointer to STARTUPINFO structure
            &pi )           // Pointer to PROCESS_INFORMATION structure
        )
        {
            last_error = GetLastError();
            //return;
        }
            bDo_ipc_notify = bIsRunIPCnotifier = true;
        }
    }
 }
 else
    {
        gStarted = false;
		//memset(pStr2num, 0x0, sizeof(pStr2num));
    }

 gStarted ? gtk_button_set_label(GTK_BUTTON(btn_start_getval), "_Stop") :
     gtk_button_set_label(GTK_BUTTON(btn_start_getval), "_Start");
}

static void update_parameters(void)
{
   tmp = vSettings[0];
   tmp += vSettings[1];
   tmp += vSettings[2];
   tmp += vSettings[3];
   tmp += vSettings[4];
   tmp += vSettings[5];
}

static void parameters_accept(GtkWidget *wid, GtkWidget *win)
{
  GtkWidget *content_area;
  GtkWidget *dialog;
  GtkWidget *hbox, *toggle_btn;
  GtkWidget *stock;
  GtkWidget *table;
  GtkWidget *local_entry1;
  GtkWidget *local_entry2, *local_entry3;
  GtkWidget *local_entry4, *local_entry5;
  GtkWidget *label1, *label2;
  gint response;
  string _tmp;

  dialog = gtk_dialog_new_with_buttons ("Settings",
          GTK_WINDOW (win),
          GTK_DIALOG_MODAL,
          "_Ok",
          GTK_RESPONSE_OK,
          "_Cancle",
          GTK_RESPONSE_CANCEL,
          NULL);

 gtk_window_set_destroy_with_parent(GTK_WINDOW (dialog), TRUE);
 gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(win));
 //gtk_window_resize(GTK_WINDOW(dialog), 10, 5);
 gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
 gtk_widget_grab_default (GTK_WIDGET(dialog));
 gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);

 content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));

 hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
 gtk_container_set_border_width (GTK_CONTAINER (hbox), 8);
 gtk_box_pack_start (GTK_BOX (content_area), hbox, TRUE, FALSE, 0);
 gtk_box_set_homogeneous(GTK_BOX(hbox), FALSE);

 stock = gtk_image_new_from_stock (GTK_STOCK_DIALOG_QUESTION, GTK_ICON_SIZE_DIALOG);
 gtk_box_pack_start (GTK_BOX (hbox), stock, FALSE, FALSE, 0);

 table = gtk_grid_new ();
 gtk_grid_set_row_spacing (GTK_GRID (table), 4);
 gtk_grid_set_column_spacing (GTK_GRID (table), 4);
 gtk_box_pack_start (GTK_BOX (hbox), table, FALSE, FALSE, 0);
 gtk_container_child_set(GTK_CONTAINER(hbox), GTK_WIDGET(table), "homogeneous",FALSE, "expand", TRUE,
                         "fill", FALSE, NULL);
 //gtk_widget_set_size_request(GTK_WIDGET(table), 15, 5);

 label1 = gtk_label_new_with_mnemonic ("_Notify period:");
 gtk_widget_set_halign(GTK_WIDGET(label1), GTK_ALIGN_END);
 gtk_grid_attach (GTK_GRID (table), label1, 0, 0, 1, 1);

 label2 = gtk_label_new_with_mnemonic ("_IPC notify period:");
 gtk_widget_set_halign(GTK_WIDGET(label2), GTK_ALIGN_END);
 gtk_grid_attach (GTK_GRID (table), label2, 0, 1, 1, 1);

 local_entry1 = gtk_entry_new ();
 gtk_entry_set_width_chars(GTK_ENTRY (local_entry1), 9);
 gtk_entry_set_activates_default (GTK_ENTRY(local_entry1), TRUE);
 //gtk_entry_set_max_length(GTK_ENTRY(local_entry1), 1);
 //gtk_entry_set_text (GTK_ENTRY (local_entry1), gtk_entry_get_text (GTK_ENTRY (entry1)));
 gtk_grid_attach (GTK_GRID (table), local_entry1, 1, 0, 1, 1);
 gtk_label_set_mnemonic_widget (GTK_LABEL (label1), local_entry1);

 local_entry2 = gtk_entry_new ();
 gtk_entry_set_width_chars(GTK_ENTRY (local_entry2), 9);
 //gtk_entry_set_text (GTK_ENTRY (local_entry1), gtk_entry_get_text (GTK_ENTRY (entry1)));
 gtk_grid_attach (GTK_GRID (table), local_entry2, 1, 1, 1, 1);
 gtk_label_set_mnemonic_widget (GTK_LABEL (label2), local_entry2);

 label1 = gtk_label_new_with_mnemonic ("CF_WC:");
 gtk_widget_set_halign(GTK_WIDGET(label1), GTK_ALIGN_END);
 gtk_grid_attach (GTK_GRID (table), label1, 0, 2, 1, 1);

 local_entry3 = gtk_entry_new ();
 gtk_entry_set_width_chars(GTK_ENTRY (local_entry3), 9);
 gtk_entry_set_activates_default (GTK_ENTRY(local_entry3), TRUE);
 //gtk_entry_set_text (GTK_ENTRY (local_entry2), gtk_entry_get_text (GTK_ENTRY (entry2)));
 gtk_grid_attach (GTK_GRID (table), local_entry3, 1, 2, 1, 1);
 gtk_label_set_mnemonic_widget (GTK_LABEL (label1), local_entry3);


 label1 = gtk_label_new_with_mnemonic ("_Halt duration:");
 gtk_widget_set_halign(GTK_WIDGET(label1), GTK_ALIGN_END);
 gtk_grid_attach (GTK_GRID (table), label1, 0, 3, 1, 1);

 local_entry4 = gtk_entry_new ();
 gtk_entry_set_width_chars(GTK_ENTRY (local_entry4), 9);
 gtk_entry_set_placeholder_text(GTK_ENTRY (local_entry4), "Min:00:00");
 gtk_grid_attach (GTK_GRID (table), local_entry4, 1, 3, 1, 1);
 /*gtk_container_child_set(GTK_CONTAINER(hbox), GTK_WIDGET(local_entry4), "homogeneous",FALSE, "expand", TRUE,
                         "fill", FALSE, NULL);*/
 gtk_label_set_mnemonic_widget (GTK_LABEL (label1), local_entry4);

 //gtk_box_pack_start (GTK_BOX (hbox), local_entry4, TRUE, TRUE, 0);

 //gtk_widget_set_margin_right(GTK_WIDGET(local_entry4), 1);
 //gtk_widget_set_size_request(GTK_WIDGET(local_entry4), 15, 5);



 local_entry5 = gtk_entry_new ();
 gtk_entry_set_width_chars(GTK_ENTRY (local_entry5), 9);
 gtk_entry_set_placeholder_text(GTK_ENTRY (local_entry5), "Max:00:00");
 gtk_grid_attach (GTK_GRID (table), local_entry5, 2, 3, 1, 1);
 //gtk_label_set_mnemonic_widget (GTK_LABEL (label1), local_entry5);

 toggle_btn = gtk_check_button_new_with_mnemonic("_Reset Watts on quit\napp");
 gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(toggle_btn), isMakeZero_Watt_counter);
 gtk_grid_attach (GTK_GRID (table), toggle_btn, 2, 0, 1, 1);

 gtk_widget_show_all (hbox);
 response = gtk_dialog_run (GTK_DIALOG (dialog));

 if (response == GTK_RESPONSE_OK)
 {
  CTimeParse _parser;
  std::basic_string <char>::size_type pos;

  bDoUpdate = isSettingsNew = true;
  _tmp = static_cast<string>(gtk_entry_get_text (GTK_ENTRY (local_entry1)));

  if(!_tmp.empty())
  period_mod = boost::lexical_cast<int>(&_tmp[0], _tmp.length());//(tmp, nullptr);

  if(period_mod > 0 && period_mod <= 60)
  {
   period_mod = 60 * period_mod;
   //cout << "Ogohlantiruv har: ~"<< period_mod << " soniyada" << endl;
   period_mod *=2;
  }
  else period_mod = 600; //2*300 sec. ~5 min.


  _tmp = static_cast<string>(gtk_entry_get_text (GTK_ENTRY (local_entry2)));

  if(!_tmp.empty())
  period_ipc_notify = boost::lexical_cast<int>(&_tmp[0], _tmp.length());
  if(period_ipc_notify > 0 && period_ipc_notify <= 5)
  {
	period_ipc_notify *=2;
  }
  else period_ipc_notify = 5;

   _tmp = static_cast<string>(gtk_entry_get_text(GTK_ENTRY(local_entry3)));
   if (!_tmp.empty())
   {
	   vSettings[5] = _tmp;
	   //pos = vSettings[5].find(",");
	   //pos != std::basic_string <char>::npos ? vSettings[5].replace(pos, 1, ".") : vSettings[5];

		rmultNum = boost::lexical_cast<int>(&(vSettings[5])[0], vSettings[5].length());//std::stof(tmp, nullptr);
		multNum = 16666 / rmultNum;
	}
	//tmp += vSettings[5];
	update_parameters();

	_tmp = static_cast<string>(gtk_entry_get_text (GTK_ENTRY (local_entry4)));

    if(!_tmp.empty())
    {
        _parser.setTime(_tmp);
        _parser.run() ?  min_per_tm = boost::lexical_cast<string>(&_tmp[0], _tmp.length()) : min_per_tm;
    }

    _tmp = static_cast<string>(gtk_entry_get_text (GTK_ENTRY (local_entry5)));

    if(!_tmp.empty())
    {
        _parser.setTime(_tmp);
        _parser.run() ?  max_per_tm = boost::lexical_cast<string>(&_tmp[0], _tmp.length()) : max_per_tm;
    }

    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(toggle_btn)))
    {
        isMakeZero_Watt_counter = 1;//true;

    }
    else
    {
        isMakeZero_Watt_counter = 0;//false;
    }

 }
 gtk_widget_destroy (dialog);
}

static void levels_accept(GtkWidget *wid, GtkWidget *win)
{
 GtkWidget *dialog, *content_area;
 GtkWidget *hbox;
 GtkWidget *stock, *table;
 GtkWidget *local_entry1, *local_entry2, *local_entry3;
 GtkWidget *label;
 GtkDialogFlags flags = GTK_DIALOG_MODAL;
 gint response;
 gchar d2str[5];

  dialog = gtk_dialog_new_with_buttons ("Limits",
          GTK_WINDOW (win),
          flags,
          "_OK",
          GTK_RESPONSE_OK,
          "_Cancle",
          GTK_RESPONSE_CANCEL,
          NULL);

  gtk_window_set_destroy_with_parent (GTK_WINDOW(dialog), TRUE);
  gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(win));
  gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
  //gtk_widget_set_can_default (GTK_WIDGET(dialog), TRUE);
  gtk_widget_grab_default (GTK_WIDGET(dialog));
  gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);


  content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
  gtk_container_set_border_width (GTK_CONTAINER (hbox), 8);
  gtk_box_pack_start (GTK_BOX (content_area), hbox, FALSE, FALSE, 0);

  stock = gtk_image_new_from_stock (GTK_STOCK_DIALOG_QUESTION, GTK_ICON_SIZE_DIALOG);
  gtk_box_pack_start (GTK_BOX (hbox), stock, FALSE, FALSE, 0);

  table = gtk_grid_new ();
  gtk_grid_set_row_spacing (GTK_GRID (table), 4);
  gtk_grid_set_column_spacing (GTK_GRID (table), 4);
  gtk_box_pack_start (GTK_BOX (hbox), table, TRUE, TRUE, 0);

  label = gtk_label_new_with_mnemonic ("U_p");
  gtk_grid_attach (GTK_GRID (table), label, 0, 0, 1, 1);
  local_entry1 = gtk_entry_new ();
  //gtk_entry_set_text (GTK_ENTRY (local_entry1), gtk_entry_get_text (GTK_ENTRY (entry1)));
  sprintf(d2str, "%.2f", gUp);
  gtk_entry_set_text(GTK_ENTRY(local_entry1), (const gchar*)(d2str));
  gtk_entry_set_activates_default (GTK_ENTRY(local_entry1), TRUE);
  gtk_grid_attach (GTK_GRID (table), local_entry1, 1, 0, 1, 1);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), local_entry1);

  label = gtk_label_new_with_mnemonic ("_Low");
  gtk_grid_attach (GTK_GRID (table), label, 0, 1, 1, 1);

  local_entry2 = gtk_entry_new ();
  sprintf(d2str, "%.2f", gLow);
  gtk_entry_set_text(GTK_ENTRY(local_entry2), (const gchar*)(d2str));
  gtk_entry_set_activates_default (GTK_ENTRY(local_entry2), TRUE);
  //gtk_entry_set_text (GTK_ENTRY (local_entry2), gtk_entry_get_text (GTK_ENTRY (entry2)));
  gtk_grid_attach (GTK_GRID (table), local_entry2, 1, 1, 1, 1);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), local_entry2);


  /*label = gtk_label_new_with_mnemonic ("CF_WC");
  gtk_grid_attach (GTK_GRID (table), label, 0, 2, 1, 1);

  local_entry3 = gtk_entry_new ();
  //gtk_entry_set_text (GTK_ENTRY (local_entry2), gtk_entry_get_text (GTK_ENTRY (entry2)));
  gtk_grid_attach (GTK_GRID (table), local_entry3, 1, 2, 1, 1);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), local_entry3);*/



  gtk_widget_show_all (hbox);
  response = gtk_dialog_run (GTK_DIALOG (dialog));

  if (response == GTK_RESPONSE_OK)
 {
    std::basic_string <char>::size_type pos;

	bDoUpdate = isSettingsNew = true;
   //string _tmp;
   //gtk_entry_set_text (GTK_ENTRY (entry1), gtk_entry_get_text (GTK_ENTRY (local_entry1)));
   //gtk_entry_set_text (GTK_ENTRY (entry2), gtk_entry_get_text (GTK_ENTRY (local_entry2)));

 /*if(gStarted){
  if(base[3] == 3)
   str2num.insert(3,",");
  else
   tmp.insert(base[2], ",");
  }*/
	tmp = static_cast<string>(gtk_entry_get_text(GTK_ENTRY(local_entry1)));
	if (!tmp.empty())
	{
	   vSettings[1] = tmp;
	   pos = vSettings[1].find(",");
	   pos != std::basic_string <char>::npos ? vSettings[1].replace(pos, 1, ".") : vSettings[1];

	   gUp = boost::lexical_cast<double>(&(vSettings[1])[0], vSettings[1].length());//std::stof(tmp, nullptr);
	}

   //gLow = strtof(gtk_entry_get_text (GTK_ENTRY (local_entry2)), nullptr);

 /*if(gStarted){
  if(base[3] == 3)
   str2num.insert(3,",");
  else
   tmp.insert(base[2], ",");
  }*/
   tmp = static_cast<string>(gtk_entry_get_text(GTK_ENTRY(local_entry2)));
   if (!tmp.empty())
   {
	   vSettings[3] = tmp;
	   pos = vSettings[3].find(",");
	   pos != std::basic_string <char>::npos ? vSettings[3].replace(pos, 1, ".") : vSettings[3];
   //if (!vSettings[3].empty())
	   gLow = boost::lexical_cast<double>(&(vSettings[3])[0], vSettings[3].length());//std::stof(tmp, nullptr);
   }

   /*tmp = static_cast<string>(gtk_entry_get_text(GTK_ENTRY(local_entry3)));
   if (!tmp.empty())
   {
	   vSettings[5] = tmp;
	   pos = vSettings[5].find(",");
	   pos != std::basic_string <char>::npos ? vSettings[5].replace(pos, 1, ".") : vSettings[5];

		rmultNum = boost::lexical_cast<double>(&(vSettings[5])[0], vSettings[5].length());//std::stof(tmp, nullptr);
		multNum = 16.666 / rmultNum;
	}*/

   //cout << gUp << " " << gLow << " " << rmultNum /*std::stof("0,0001105", nullptr)*/ <<endl;
   //tmp = vSettings[0] + vSettings[1] + vSettings[2] + vSettings[3] + vSettings[4] + vSettings[5];

   /*tmp = vSettings[0];
   tmp += vSettings[1];
   tmp += vSettings[2];
   tmp += vSettings[3];
   tmp += vSettings[4];
   tmp += vSettings[5];*/

   update_parameters();

   //cout << tmp << endl;
 }

  gtk_widget_destroy (dialog);

}

static void helloWorld (GtkWidget *wid, GtkWidget *win)
{
  GtkWidget *connect_btn = GTK_WIDGET(win);
  GtkWidget *dialog = NULL;
  GtkTextBuffer *buffer = NULL;
  GtkTextIter txt_iter;
  bool bset_sensitive  = true;
  std::string str("     Ready!  ");// = "This just a testing TEXT!!!";
  unsigned char buf[9];

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));
  gtk_text_buffer_set_text (buffer, "", 0);
  gtk_text_buffer_get_iter_at_offset (buffer, &txt_iter, 0);

  //g_signal_connect (demuxer, "pad-added", G_CALLBACK (on_pad_added), decoder);
  //gst_element_set_state (pipeline, GST_STATE_PAUSED);
  //gst_element_change_state(pipeline, GST_STATE_CHANGE_PLAYING_TO_PAUSED);
  //gst_element_change_state(pipeline, GST_STATE_CHANGE_PAUSED_TO_PLAYING);
  //if(!sound_off) gst_element_set_state (GST_ELEMENT(playbin2), GST_STATE_PLAYING);

  /*GstPad *sinkpad, *pad;
  pad = gst_element_get_static_pad (demuxer, "src");
  sinkpad = gst_element_get_static_pad (decoder, "sink");
  gst_pad_unlink (pad, sinkpad);
  gst_object_unref (sinkpad);
  gst_object_unref (pad);*/

  //gtk_widget_set_sensitive(btn_start_getval, true/*bset_sensitive*/);

  #ifndef APP_IMITATE
  hdev = libusb_open_device_with_vid_pid(ctx, 0x1a86, 0xe008);

  if(hdev==NULL)
   {
    bset_sensitive  = false;
    //str = "Failed to connect \n c ya...";
    str = "Failed to connect.\nPlease check the connection and ADC.";
    //libusb_close(hdev);
    //libusb_exit(ctx); //close the session
    //goto fin;
    gtk_text_buffer_insert_with_tags_by_name(buffer, &txt_iter, &str[0], -1, "little_big", "blue_foreground" , NULL);
    //bdo_some_thing = true; case_of_tasks = 0;
    return;
   }
   /*else
   str = "Connected successfully!"; */

  libusb_free_device_list(devs, 1); //free the list, unref the devices in it

  if(libusb_kernel_driver_active(hdev,0) == 1)
   {
    //str="Kernel active";
    //gtk_text_buffer_insert_with_tags_by_name(buffer, &txt_iter, &str[0], -1, "big", "blue_foreground" ,"wide_margins", NULL);

    if(libusb_detach_kernel_driver(hdev,0) == 1){
      //str="Detachin from kernel DONE.";
     //gtk_text_buffer_insert_with_tags_by_name(buffer, &txt_iter, &str[0], -1, "big", "blue_foreground" ,"wide_margins", NULL);
    }
   }

    if ((r = libusb_set_configuration(hdev, DEV_CONFIG)) < 0)
    {
     bset_sensitive  = false;
     str = "Configuration setting failure!" ;
     gtk_text_buffer_insert_with_tags_by_name(buffer, &txt_iter, &str[0], -1, "big", "blue_foreground" ,"wide_margins", NULL);
     return;
    }

   r = libusb_claim_interface(hdev, 0);
   if(r < 0)
   {
   bset_sensitive  = false;
   //str = "Failed to claim interface";
   str = "Failure";
   gtk_text_buffer_insert_with_tags_by_name(buffer, &txt_iter, &str[0], -1, "big", "blue_foreground" ,"wide_margins", NULL);
   return;
   }
   #endif // APP_IMITATE
   //str = "Claimed interface";
   //str = "Ready!";
  //bdo_some_thing = true; case_of_tasks = 2;

  gtk_text_buffer_insert_with_tags_by_name(buffer, &txt_iter, "\n", -1, "double_spaced_line", NULL);
  gtk_text_buffer_insert_with_tags_by_name(buffer, &txt_iter, &str[0], -1, "another_big", "blue_foreground" ,"double_spaced_line", NULL);
  gtk_text_buffer_insert_with_tags_by_name(buffer, &txt_iter, "\n\n", -1, "double_spaced_line", NULL);

  memset(buf,0x00,sizeof(buf));
  #ifndef APP_IMITATE
  r = hid_send_feature_report(hdev, buf, 6); // 6 bytes

 if (r < 0) {
   bset_sensitive  = false;
   str = "Unable to send a feature report.\n";
   gtk_text_buffer_insert_with_tags_by_name(buffer, &txt_iter, &str[0], -1, "big", "blue_foreground" ,"wide_margins", NULL);
  }
 #endif // APP_IMITATE
 gtk_widget_set_sensitive(btn_start_getval, bset_sensitive);
 //gtk_widget_set_sensitive(but_corr_usb_accept, bset_sensitive);
 gtk_widget_grab_focus(connect_btn);

}

static void
toggle_snd_off(GtkToggleButton *check_button, gpointer data)
{
 if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(radio1)))
 {
  sound_off = true;
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio2), TRUE);
  gtk_widget_set_sensitive(radio2, FALSE);
 }
 else
 {
    sound_off = false;
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio2), FALSE);
    gtk_widget_set_sensitive(radio2, TRUE);
 }
 toggle_spkr_off(GTK_TOGGLE_BUTTON(radio2), NULL);
}

static void
toggle_spkr_off(GtkToggleButton *check_button, gpointer data)
{
 if (gtk_toggle_button_get_active (check_button))
 {
   speaker_off = true;
   speakerOff_timeOut = 3600;
 }
 else
  {
    speaker_off = false;
    speakerOff_timeOut ^= speakerOff_timeOut;
  }
}

/*static void outtext(const char* mess)
{
  GtkTextBuffer *buffer = NULL;
  GtkTextIter txt_iter;

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));
  gtk_text_buffer_set_text (buffer, "", 0);
  gtk_text_buffer_get_iter_at_offset (buffer, &txt_iter, 0);

  //str = "Failure";
  gtk_text_buffer_insert_with_tags_by_name(buffer, &txt_iter,  mess, -1, "little_big", "red_foreground" ,"wide_margins", NULL);
}*/

/*static void
toggle_period_notify(GtkToggleButton *check_button, gpointer data)
{
 gtk_toggle_button_get_active (check_button) ? bperiod_notify = true : bperiod_notify = false;
}*/

/*static void
toggle_admin (GtkToggleButton *check_button, gpointer data)
{
 gchar *which = reinterpret_cast<gchar*>(data);
 if(gtk_toggle_button_get_active(check_button)){
  switch(atoi((char*)which))
  {
   case 1: len = 9; cout << len  <<endl; break;
   case 2: len = 9; cout << len  <<endl; break;
   case 3: len = 5; cout << len  <<endl; break;
  }
 }

}*/

/*static void
on_pad_added (GstElement *element, GstPad *pad, gpointer data)
{
 GstPad *sinkpad;
 GstElement *decoder = (GstElement *) data;
 /* We can now link this pad with the vorbis-decoder sink pad /
 //g_print ("Dynamic pad created, linking demuxer/decoder\n");
 sinkpad = gst_element_get_static_pad (decoder, "sink");
 gst_pad_link (pad, sinkpad);
 gst_object_unref (sinkpad);
 //gst_pad_unlink(pad, sinkpad);
}*/

static void
seek_to_time (GstElement *pipeline,
          gint64      time_nanoseconds)
{
  if (!gst_element_seek (pipeline, 1.0, GST_FORMAT_TIME, (GstSeekFlags)(GST_SEEK_FLAG_FLUSH),
                         GST_SEEK_TYPE_SET, time_nanoseconds,
                         GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE)) {
    g_print ("Seek failed!\n");
  }
}


static gboolean
bus_call (GstBus *bus, GstMessage *msg, gpointer data)
{
 GMainLoop *loop = (GMainLoop *) data;
 switch (GST_MESSAGE_TYPE (msg)) {

 case GST_MESSAGE_ANY:
    {
        /*ofstream outf;
        outf.open("temp.txt", ios::out);
        outf << "clock lost" << endl;
        outf.close();*/
       //flush_pipe(playbin2);
       //gst_element_set_state (playbin2, GST_STATE_READY );
    }break;
 /*case GST_MESSAGE_STREAM_STATUS:
    {
       GstStreamStatusType gstStreamStatusType;
       //seek_to_time(playbin2, 0);
       gst_message_parse_stream_status (msg, &gstStreamStatusType, &playbin2);

       ofstream outf;
       outf.open("temp.txt", ios::out);
       switch(gstStreamStatusType)
       {
           case GST_STREAM_STATUS_TYPE_CREATE : outf << "GST_STREAM_STATUS_TYPE_CREATE" << endl; break;
           case GST_STREAM_STATUS_TYPE_ENTER:   outf << "GST_STREAM_STATUS_TYPE_ENTER" << endl; break;
           case GST_STREAM_STATUS_TYPE_LEAVE:   outf << "GST_STREAM_STATUS_TYPE_LEAVE" << endl; break;
           case GST_STREAM_STATUS_TYPE_DESTROY: outf << "GST_STREAM_STATUS_TYPE_DESTROY" << endl; break;
           case GST_STREAM_STATUS_TYPE_START:   outf << "GST_STREAM_STATUS_TYPE_START" << endl; break;
           case GST_STREAM_STATUS_TYPE_PAUSE:   outf << "GST_STREAM_STATUS_TYPE_PAUSE" << endl; break;
           case GST_STREAM_STATUS_TYPE_STOP:    outf << "GST_STREAM_STATUS_TYPE_STOP" << endl; break;
           default :break;
       }

       outf.close();

    }break;*/
 case GST_MESSAGE_EOS:
  {
   //g_print ("End of stream\n");
   //g_main_loop_quit (loop);
   //period_notify_count ^= period_notify_count;
   //_count_alrm = 0;

   /*ofstream outf;
   outf.open("temp.txt", ios::out);
   outf << _count_alrm << endl;*/


   /*FILE *pFl;
   pFl = fopen("temp.txt", "a");
   fwrite(&_count_alrm , sizeof(int), 1, pFl);
   //fwrite("\n", 1, 1, pFl);
   fclose(pFl);*/

   _count_alrm ^= _count_alrm;

   gst_element_set_state(playbin2, GST_STATE_PAUSED);
   //flush_pipe(playbin2);
   seek_to_time(playbin2, 0);

   //gst_element_set_state (playbin2, GST_STATE_READY );

   //gst_element_set_start_time(playbin2, 0);
   //gst_element_set_state(playbin2, GST_STATE_PAUSED);

  }
  break;
  case GST_MESSAGE_ERROR: {
   gchar *debug;
   GError *error;
   gst_message_parse_error (msg, &error, &debug);
   g_free (debug);
   g_printerr ("Error: %s\n", error->message);
   g_error_free (error);
   //g_main_loop_quit (loop);

   /*ofstream outf;
   outf.open("temp.txt", ios::out);
   outf << _count_alrm << endl;
   outf.close();*/

   break;
  }
  default:{
    //gst_element_set_state(playbin2, GST_STATE_PAUSED);
    //seek_to_time(playbin2, 0);
  }
  break;
 }
return TRUE;
}

int ipc_done()
{
	delete pShared_mem;
	delete pMmap;
	pShared_mem = NULL;
	pMmap = NULL;
	return 0;
}

int ipc_init()
{
	//std::strcpy(static_cast<char*>(mmap.get_address()), "Hello World!\n");
	try
	{
		pShared_mem = new shared_memory_object(open_or_create, "Hello", read_write);
		pShared_mem->truncate(256);

		pMmap = new mapped_region(*pShared_mem, read_write);
	}
	catch(interprocess_exception &e )
	{
	//clean up
	}
	return 0;
}

int ipc_notify(gpointer _pSharedMem_data1, gpointer _pMmap_data2)
{
  //using namespace boost::interprocess;
  //try
  {
	//shared_memory_object *_pSharedMem = (shared_memory_object *)_pSharedMem_data1;//(open_or_create, "Hello", read_write);

	//setting the size of shared mem
	//_pSharedMem->truncate(256);
	//mapped_region *_pMmap = (mapped_region*)_pMmap_data2;//(*pShared_mem,  read_write);
    std::strcpy(static_cast<char*>(pMmap->get_address()), static_cast<const char*>(ipc_mes));//"HelloWorld");

  }
  /*catch(interprocess_exception &e )
  {
	//clean up
  }*/
  return 0;
}

int initFont()
{
  char buf[64];

  //gchar *gFontPath = NULL;
  //int str_pos = 0;
  size_t str_len=0;
  string str;
  basic_string <char>::size_type pos;

  //gFontPath = new gchar[WORD_SIZE];

  str_len = strlen(dseg_fonts[font_no]);
  memset(buf, 0, 64);
  strncpy(buf, dseg_fonts[font_no], str_len);
  //printf("%s %d\n", buf, str_len);
  g_strdelimit(buf, "-", ' ');
  //printf("%s\n", buf);
  g_strdelimit(buf, ".ttf", ' ');
  g_strstrip(buf);
  //printf("%s\n", buf);
  str.assign(buf);
  pos = str.find("4");
  if(pos != std::basic_string <char>::npos)
  {
      str.insert(pos+1, " ");
  }
  else
  {
      pos = str.find("7");
      str.insert(pos+1, " ");
  }
  //printf("%s", &str[0]);
  font_desc = pango_font_description_from_string(&str[0]);
  myPixbuf = gdk_pixbuf_from_pixdata(&GTesterLogo, FALSE, NULL);

}


int InitDB()
{
  gCmdLine = new gchar[WORD_SIZE];
  //string SQLst = "select * from nav_mv where navID = $navID order by nID desc limit 1";
  string nav_watts_SQLst = "select * from nav_watts order by navID desc limit 1";

  int ret, TimeInt, b; //year_db_nav_watts, month_db_nav_watts;
  ifstream ifpar;

  //char buf_tm[20];
  /*string TimeStr;
  TimeStr.resize(10);*/

  /*GError *error;
  if( thread3 == NULL && window != NULL )
    {
       //g_thread_unref( _thread );
        thread3 = g_thread_try_new("mine3", thread_func3, (gpointer)window, &error);
        if( ! thread3 )
        {
            g_print( "Error: %s\n", error->message );
            return -1;
        }
    }*/


  ret = sqlite3_open_v2("navbat_id_nxt", &pDB, SQLITE_OPEN_READWRITE, NULL);

  time( &rawtime );
  pTime = localtime( &rawtime );
  strftime(gl_buf_tm, sizeof(gl_buf_tm),"%Y%m%d", pTime);
  //cout << gl_buf_tm << endl;

  ifpar.open("params.dat", ios::in);

  if(ifpar.is_open())
  {
    CTimeParse timeParser;
    //gchar *pCurrentDir = NULL;
	gchar d2str[10], buf[WORD_SIZE];
	//memset(buf, 0, WORD_SIZE);

    //gCmdLine = new gchar[WORD_SIZE];

    ifpar >> rmultNum >> gUp >> gLow;

	//tmp = "| Up:";
	sprintf(d2str, "%d", gUp);
	vSettings[1].assign(d2str);
	//tmp.append(d2str);

	//tmp.append(" | Low:");
	sprintf(d2str, "%d", gLow);
	vSettings[3].assign(d2str);
	//tmp.append(d2str);

	//tmp.append(" | CFWC:");
	sprintf(d2str, "%d", rmultNum);
	vSettings[5].assign(d2str);
	//tmp.append(d2str);
	tmp = vSettings[0];
	tmp += vSettings[1];
	tmp += vSettings[2];
	tmp += vSettings[3];
	tmp += vSettings[4];
	tmp += vSettings[5];

	bDoUpdate = true;

  //rmultNum = multNum;

  /*ifpar.getline(&TimeStr[0], WORD_SIZE);
  ifpar.getline( &TimeStr[0], WORD_SIZE );*/
   ifpar >> TimeInt >> kW;
   ifpar >> period_mod;
   ifpar >> min_per_tm >> max_per_tm;
   ifpar >> gIgnoranceVal >> gCountDownTime; //>> buf;// >> param >> param2 >> param3;
   ifpar.getline(buf, WORD_SIZE);
   g_strstrip(buf);
   //ifpar.getline(params, 32);

   ifpar >> font_no >> isMakeZero_Watt_counter;

   ifpar.close();

   font_no > 3 ? font_no = 0 : font_no;
   isMakeZero_Watt_counter > 1 || isMakeZero_Watt_counter < 0 ? isMakeZero_Watt_counter = 0 : isMakeZero_Watt_counter;

   timeParser.setTime(min_per_tm);
   if(!timeParser.run())
    {
      min_per_tm = "02:00";
      isSettingsNew = true;
    }

   timeParser.setTime(max_per_tm);
   if(!timeParser.run())
   {
       max_per_tm = "24:00";
       isSettingsNew = true;
   }

   //cout << period_mod << " " << gIgnoranceVal << " " << gCountDownTime << " " << gCmdLine << endl;
   int _length = strlen(buf);
   //_length += strlen(params);

   if( _length < 20 || _length > 21 )
    {
        strncpy(buf, "shutdown.exe -s -t 3", 20);
        /*strcpy(param, " -s");
        strcpy(param2, " -t");
        strcpy(param3, " 15");*/
   }
   /*else
   {
        strncpy(param, &params[0], 3);
        strncpy(param2, &params[3], 3);
        strncpy(param3, &params[6], 3);
   }*/
        //g_strstrip(params);
        //g_strlcat(buf, params, WORD_SIZE);

   /*ofstream outf;
   outf.open("tmp.out", ios::out);*/

    strncpy(gCmdLine, pCurrentDir, strlen((const char*)pCurrentDir));
    strncat(gCmdLine, "\\", 2);
    strncat(gCmdLine, buf, _length);

    //g_strdelimit(gCmdLine, "\\", '/');

   /*outf << gCmdLine << " " << buf << " " << _length <<"\n";//param << param2 << param3 << " len: " << _length << "\n";
   outf.close();*/

  //if(isdigit(period_mod))
    period_mod <= 100 ? period_mod = 360 : NULL; //600 -> 5 minutes
    if(gIgnoranceVal <= 0 || gCountDownTime <= 0)
    {
        gIgnoranceVal = 8;
        gCountDownTime = 5400;
    }

   _CountDownTime = gCountDownTime;
   //cout << period_mod << " " << gIgnoranceVal << " " << gCountDownTime << " " << gCmdLine << endl;

  //TimeStr.compare(buf_tm) != 0 ? cout << TimeStr << endl: cout << "Failed" << endl;
  //TimeInt == std::stod(gl_buf_tm, nullptr) ? kW : kW=0;
  //cout << gIgnoranceVal << " " << gCountDownTime << " " << gCmdLine << endl;
   TimeInt == boost::lexical_cast<int>(&gl_buf_tm[0]) ? kW : kW=0; //check for to continue or to restart
   isMakeZero_Watt_counter == 1 ? kW = 0: kW;
   /*if(TimeStr != boost::lexical_cast<int>(&gl_buf_tm[0]))
    {
        double dTmp = kW;
        kW = 0;
    }*/
    //if( (TimeStr == boost::lexical_cast<int>(&gl_buf_tm[0]))
    /*if((boost::lexical_cast<int>(&gl_buf_tm[0]) - TimeInt) == 1 && pTime->tm_hour < 8)
    {
        //kW = dTmp;
        ret = sqlite3_prepare_v2(pDB, &SQLst[0], SQLst.length() , &pStmt, NULL);
        sqlite3_step(pStmt);
        kW = sqlite3_column_double(pStmt, 3);
    }*/
  //printf("TimeStr: %d glBugTm: %s  %.4f\n", TimeStr, gl_buf_tm, kW);
   multNum = 16666 / rmultNum;
  //printf("%d\n", period_mod);
   g_free((gpointer)pCurrentDir);
   pCurrentDir = NULL;
  }
  else
  {
    /*multNum = 64.1;*/
    strncpy(gCmdLine, "shutdown.exe -s -t 3", 20);
  }


  //ret = sqlite3_open_v2("navbat_id", &pDB, SQLITE_OPEN_READWRITE, NULL);
  //ret == SQLITE_OK ? cout << "DB connected OK!" <<endl : cout << "Failure..." <<endl;

  //string SQLst = "select count(*) from nav_watts";

  //navID = sqlite3_column_int(pStmt, 0);
  //navID == 0 ? navID = 1 : navID = navID;


  //get the all info from nav_watts table
  //ret = sqlite3_prepare_v2(pDB, &nav_watts_SQLst[0], nav_watts_SQLst.length() , &pStmt, NULL);

  //ret = sqlite3_step(pStmt);
  //printf("%d\n", ret);
  /*if(ret != SQLITE_ERROR || ret != SQLITE_FAIL)
    {
        TimeInt = boost::lexical_cast<int>(sqlite3_column_text(pStmt, 0));
        //navID   = sqlite3_column_int(pStmt, 1);
    }

  sqlite3_reset(pStmt);*/

  string is_db_table_empty = "select count(*) from nav_watts";
  ret = sqlite3_prepare_v2(pDB, &is_db_table_empty[0], is_db_table_empty.length() , &pStmt, NULL);
  ret = sqlite3_step(pStmt);
  if(sqlite3_column_int(pStmt, 0) != 0 ) //nav_watts DB table is not empty
    {
        sqlite3_finalize(pStmt);
        //sqlite3_reset(pStmt);
        ret = sqlite3_prepare_v2(pDB, &nav_watts_SQLst[0], nav_watts_SQLst.length() , &pStmt, NULL);
        ret = sqlite3_step(pStmt);

    //cout << (const char*)sqlite3_column_text(pStmt, 0) << endl;
    //if( strlen((const char*)sqlite3_column_text(pStmt, 0)) != 0)
    if(ret == SQLITE_DONE || ret == SQLITE_ROW)
        {
            strcpy((char*)gl_buf_tm, (const char*)sqlite3_column_text(pStmt, 0));
            //sqlite3_reset(pStmt);
            if( (pTime->tm_mon+1 > boost::lexical_cast<int>(&gl_buf_tm[4],2)) || (pTime->tm_year+1900 > boost::lexical_cast<int>(&gl_buf_tm[0],4)))
            {
                //cout << "count row" << " " << ret << endl;
                sqlite3_finalize(pStmt);

                /*backup_db_file(gl_buf_tm);
                drop_db_tables();
                restore_db_tables();*/

                //outtext("Please Wait a while");
                //clear_db_tables();
            }
            else
            {
                //if current date day != date day of Watt in nav_watts
                if(pTime->tm_mday != boost::lexical_cast<int>(&gl_buf_tm[6],2))
                {
                    //g_ascii_dtostr(Watt, 9, sqlite3_column_double(pStmt, 2) );
                    //sumWatt = "|";

                    if( get_last_record_nav_mv() == -1 )
                    {
                        char SQLst[] = "select * from nav_mv order by nID desc limit 1";
                        char insert_next_navID[] = "insert into nav_watts values(:dateidx, $navID, $Watt)";

                        char buf_tm[20], d2str[6];
                        strftime(buf_tm, sizeof(buf_tm),"%Y%m%d", pTime);

                        int day_of_Watts = 0, day_of_mV = 0;
                        int kW_of_mV;
                        day_of_Watts = boost::lexical_cast<int>(&gl_buf_tm[6],2);

                        //ofstream outf;
                        //outf.open("tmp.out", ios::out);

                        sqlite3_finalize(pStmt);
                        ret = sqlite3_prepare_v2(pDB, SQLst, 46 , &pStmt, NULL);
                        sqlite3_step(pStmt);

                        day_of_mV = sqlite3_column_int(pStmt, 1);
                        kW_of_mV  = sqlite3_column_int(pStmt, 4);

                        //g_ascii_dtostr(&buf_tm[6], 20, (gdouble)day_of_mV);
                        g_snprintf(&buf_tm[6], 20, "%d", day_of_mV);

                        if(day_of_mV > day_of_Watts)
                        {
                            //outf << buf_tm << " " << day_of_Watts << " " << day_of_mV << " " << kW_of_mV <<"\n";

                            sqlite3_finalize(pStmt);
                            ret = sqlite3_prepare_v2(pDB, &insert_next_navID[0], -1, &pStmt, 0);


                            if(ret == SQLITE_OK)
                            {
                                ret = sqlite3_bind_text(pStmt, 1, (const char*)buf_tm /*&gTimeStr[0]*/, -1,  NULL);
                                ret = sqlite3_bind_int (pStmt, 2, day_of_mV);
                                ret = sqlite3_bind_int(pStmt, 3, kW_of_mV);

                                ret = sqlite3_step(pStmt);
                            }

                        }

                        //outf.close();
                        //g_ascii_dtostr(Watt, 9, kW_of_mV);

                        //sumWatt = "|";
                    }
                    sumWatt.append(Watt, 9);

                }
                else
                {
                    /*if( get_last_record_nav_mv() == 0 )
                        g_ascii_dtostr(Watt, 9, kW);
                    else
                        g_ascii_dtostr(Watt, 9, kW);*/
                }
            }
        }
      //sqlite3_reset(pStmt);
      //sqlite3_finalize(pStmt);

      //obtain the last record from nav_mv table
      //if(!bdo_some_thing)
      {
        //outtext("test!!!");
       // bdo_some_thing=true; case_of_tasks = 0;

        //memset(Watt, 0, sizeof(Watt));
        //sprintf(Watt, "%s", "0");

        /*if( get_last_record_nav_mv() == 0 )
            g_ascii_dtostr(Watt, 9, kW);
        else
            g_ascii_dtostr(Watt, 9, kW);*/
        //cout << Watt << " " << sumWatt << endl;
        /*ofstream outf;
        outf.open("tmp.out", ios::out);
        outf << Watt << " " << sumWatt << endl;
        outf.close();*/
      }
    }
    /*else{
        sumWatt.append("0");
    }*/

    {
        int res = get_last_record_nav_mv();
        if( res == 0 )
            //g_ascii_dtostr(Watt, 9, kW);
			g_snprintf(Watt, 9, "%d", kW);
        if(res == -1)
            //g_ascii_dtostr(Watt, 9, 0);
			g_snprintf(Watt, 9, "%d", 0);
    }

  /*int len = strlen(&sumWatt[0]), len2 = strlen(Watt);
  if (len != 0)
  {
	  //cout << len << endl;
	  (len  > 8 || len  < 8) ? len  = 8 : len;
	  (len2 > 8 || len2 < 8) ? len2 = 8 : len2;
	  gWndWidth += (len * len2) + 8;
	  //cout << gWndWidth << endl;
  }*/
  //strcpy(pWatts, "\nWatts:");//sWatts = "\nWatts:";
  //strcat(pWatts, Watt); //sWatts.append(Watt);


  //if permitted then get the last Watt from nav_mv table
  //if((TimeInt - boost::lexical_cast<int>(&gl_buf_tm[0])) == -1 /*&& pTime->tm_hour < 8*/)
    {
        //cout << "gotcha" << endl;
        //kW = sqlite3_column_double(pStmt, 4);
    }
    //else kW = 0;

  //cout << nID <<" " << navID << " " << TimeInt << " " << kW << endl;
  //cout << gl_buf_tm <<" " << boost::lexical_cast<int>(&gl_buf_tm[0],4) << " " << pTime->tm_year+1900 << endl;

  #ifndef APP_IMITATE
  bdo_some_thing=true; case_of_tasks = 0;
  #endif // APP_IMITATE

  /*std::string str = "00335";

    str.insert(3,".");
    double y = std::stod(str, nullptr);//boost::lexical_cast<double>(&str[0], str.length());//std::fabs(atof((const char*)&str[2]));
    std::vector<double> av;

    av.resize(5);
    for(int i = 0; i < 5; ++i)
    {
        av[i] = y;
    }
    dret = std::fabs(median(av, 5));
    ++nID;
    strftime(gl_buf_tm, sizeof(gl_buf_tm),"%H%M", pTime);
    //cout << dret << endl;
  insert_mv_nav(gl_buf_tm);*/

  ifpar.close();
  //#ifndef APP_IMITATE
  ipc_init();
  //#endif // APP_IMITATE
  return ret;
}

int FinalizeDB(void)
{

  /*if(isSettingsNew)
  {
    //save_app_params();
    ofstream ofpar;
  }*/

  pango_font_description_free(font_desc);
  //cout << "FinalizeDB->" << endl;

  if(kW != 0 && bIsRunIPCnotifier)
    {
      char buf_tm[20];
      time( &rawtime );
      pTime = localtime( &rawtime );
      strftime(buf_tm, sizeof(buf_tm),"%H%M", pTime);

      nID == 0 ? ++nID : nID;
      insert_mv_nav(buf_tm);
    }

  int ret = 0;
  //g_free((gpointer)pCurrentDir);
  delete [] pStr2num;
  delete [] pWatts;
  pStr2num = pWatts = NULL;

  delete [] gCmdLine;
  //g_free((gpointer)gCmdLine);
  gCmdLine = NULL;

 //if( gStarted || isSettingsNew)

 //#ifndef APP_IMITATE
   ipc_done();
 //#endif // APP_IMITATE


  //ret == SQLITE_OK ? cout << "statement finalized" <<endl : cout << "Failed..." <<endl;
  ret = sqlite3_finalize(pStmt);
  ret = sqlite3_close(pDB);
  //ret == SQLITE_OK ? cout << "DB closed" <<endl : cout << "Closing Failed..." <<endl;
   //shared_memory_object::remove("Hello");

  return ret;
}

/*static bool save_app_params(gpointer data)
{
   bool bRes = false;
   ofstream ofpar("params.dat");
   if(ofpar.is_open())
    ofpar.close();

   ofpar.open("params.dat", ios::out);

   //if(ofpar.is_open())
    {
       //char buf_tm[20];
       data != NULL ? strftime(gl_buf_tm, sizeof(gl_buf_tm),"%Y%m%d", reinterpret_cast<tm*>(data)) : NULL;//pTime);

       ofpar << rmultNum << " " << gUp << " " << gLow << endl;
       //ofpar << (int)strtol (gl_buf_tm, (char**)NULL, 10) << " "<< kW << endl;
       ofpar << gl_buf_tm << " " << kW << endl;
       ofpar << period_mod << endl;
       ofpar << min_per_tm << " " << max_per_tm << endl;
       ofpar << gIgnoranceVal << " " << gCountDownTime << " " << g_path_get_basename(gCmdLine) << params << endl;
       ofpar << font_no << " " << isMakeZero_Watt_counter <<endl;
    }
   /*else
   {
	ofstream ofpar("params.dat");
   }/
    bRes = true;
    ofpar.close();
 return bRes;
}*/

void about_us( GtkWidget *widget,
   gpointer   data )
{
 //GdkPixbuf *myPixbuf = gdk_pixbuf_from_pixdata(&GTesterLogo, FALSE, NULL);
 const gchar *authors[] = {
 "1. TKL K96 207",
 "2. DSEG fonts copyright (c) 2017, keshikan", "\twww.keshikan.net",
 NULL
  };

// gtk_about_dialog_set_logo_icon_name()
 gtk_show_about_dialog (GTK_WINDOW(data),
        "program-name", prog_name,
        "copyright", "Muqobil Dasturlar To'plami (c) Build: June 3 2018 - GTK+ 3.6.4",
        "license-type", GTK_LICENSE_GPL_3_0,
        "version", ver,
        "comments", "O'zbekiston, Toshkent shahri\n  muqobildasturlar@gmail.com",
        "title", ("About GSupervisor"),
        "authors", authors,
        //"logo-icon-name", "gtester.ico",
        "logo", (const GdkPixbuf*)myPixbuf,
       NULL);

 //g_free(myPixbuf);
}

GtkWidget * create_my_window(gpointer data)
{
 char buf_name[32];
 guint bus_watch_id;
 //GtkWidget *window = NULL;
 //GtkApplicationWindow *window = NULL;
 GtkWidget *button = NULL;//, *radio1, *radio2, *radio3, *entry;
 //GtkWidget *win = NULL;
 GtkWidget *vbox = NULL, *hbox = NULL, *frame_vert = NULL;
 GtkWidget *label = NULL, *overlay = NULL;
 GtkTextBuffer *buffer = NULL;
 //GThread *thread = NULL;

 //if (InitDB() < 0) return NULL;

 playbin2 = gst_element_factory_make ("playbin", "playbin2");
 if( !playbin2){
  g_printerr("Not all elements could be made. \n");
  return NULL;
 }
 else
 {
    //gLoop = g_main_loop_new(NULL, FALSE);

    //gchar * app_title = g_strjoin(" ", prog_name, ver);
    strncpy(buf_name, prog_name, strlen((const char*)prog_name));
    //strncat(buf_name, "-", 1);
    g_strlcat(buf_name, " ", sizeof(buf_name));
    g_strlcat(buf_name, ver, sizeof(buf_name));
    //g_strdelimit(buf_name, "|",'');


    /* Set up the pipeline */
    /* we set the input filename to the source element */
    g_object_set (G_OBJECT (playbin2), "uri", alarm_audio_file_path, NULL);
    //g_object_set (G_OBJECT (playbin2), "uri", "file:///C:/test.ogg", NULL);
    //g_object_set (G_OBJECT (playbin2), "uri", "file:////home/mdt/test.ogg", NULL);

    //bus = gst_element_get_bus(playbin2);
    bus = gst_pipeline_get_bus(GST_PIPELINE(playbin2));
    bus_watch_id = gst_bus_add_watch (bus, bus_call, NULL);
    gst_object_unref (bus);

    /* we add a message handler */
    gsStateChRet = gst_element_set_state (playbin2, GST_STATE_PLAYING);

    //g_main_loop_run(gLoop);

 }

 //if( InitDB() < 0 ) return NULL;

 //window = GTK_APPLICATION_WINDOW(gtk_application_window_new (GTK_APPLICATION (app)));

 /* Create the main window */
  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  //sleep(1);
  //if( InitDB() < 0 ) return NULL;

  gtk_container_set_border_width (GTK_CONTAINER (window), 8);

  //gtk_window_set_title(GTK_WINDOW(window), buf_name); // "GTester 0.96.1");
  //g_free(app_title);
  //gtk_window_set_position (GTK_WINDOW (window), GTK_WIN_POS_CENTER);
  //gtk_widget_realize (window);
  //g_signal_connect (window, "destroy", gtk_main_quit, NULL);
  gtk_widget_add_events(window, GDK_KEY_PRESS_MASK);
  g_signal_connect (G_OBJECT (window), "key_press_event", G_CALLBACK (on_key_press), NULL);

  //if(font_no <=3)
    initFont();
  /*else
  {
   font_no = 0;
   initFont();
  }*/

 /* Create a vertical box with buttons */
  /*vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  gtk_container_add (GTK_CONTAINER (window), vbox);*/

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_container_set_border_width (GTK_CONTAINER (hbox), 0);
  //gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 1);
  gtk_container_add (GTK_CONTAINER (window), hbox);

  /*Values printin out place */
  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 0);
  gtk_box_pack_start (GTK_BOX (hbox), vbox, TRUE, TRUE, 2);

  frame_vert = gtk_frame_new (NULL);
  gtk_widget_set_valign (frame_vert, GTK_ALIGN_START);
  gtk_widget_set_size_request(frame_vert, 1, 71);
  gtk_box_pack_start(GTK_BOX (vbox), frame_vert, FALSE, FALSE, 2);

  aux_view = gtk_text_view_new ();
  gtk_text_view_set_editable(GTK_TEXT_VIEW (aux_view), FALSE);
  gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (aux_view), GTK_WRAP_WORD);

  gtk_container_add (GTK_CONTAINER (frame_vert), aux_view);

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (aux_view));
  gtk_text_buffer_create_tag (buffer, "blue_foreground", "foreground", "blue", NULL);
  gtk_text_buffer_create_tag (buffer, "right_justify", "justification", GTK_JUSTIFY_RIGHT, NULL);
  gtk_text_buffer_create_tag (buffer, "left", "justification", GTK_JUSTIFY_LEFT, NULL);

  gtk_text_buffer_create_tag (buffer, "blue2_foreground", "foreground", "AliceBlue", NULL);
  gtk_text_buffer_create_tag (buffer, "orange_foreground", "foreground", "tan1", NULL);
  gtk_text_buffer_create_tag (buffer, "green_foreground", "foreground", "SpringGreen3", NULL);
  gtk_text_buffer_create_tag (buffer, "gray_foreground", "foreground", "gray86", NULL);

  gtk_text_buffer_create_tag (buffer, "little_big",
         /* points times the PANGO_SCALE factor */
         "size", 30 * PANGO_SCALE, NULL);


  //Main info view
  frame_vert = gtk_frame_new (NULL);//"MDT:");
  gtk_widget_set_valign (frame_vert, GTK_ALIGN_START);
  gtk_widget_set_size_request(frame_vert, 1, 187);
  gtk_box_pack_start(GTK_BOX (vbox), frame_vert, FALSE, FALSE, 15);

  view = gtk_text_view_new ();
  gtk_text_view_set_editable(GTK_TEXT_VIEW (view), FALSE);
  gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (view), GTK_WRAP_WORD);

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));
  gtk_text_buffer_create_tag (buffer, "left", "justification", GTK_JUSTIFY_LEFT, NULL);
  gtk_text_buffer_create_tag (buffer, "right_justify", "justification", GTK_JUSTIFY_RIGHT, NULL);
  gtk_text_buffer_create_tag (buffer, "big_gap_before_line", "pixels_above_lines", 30, NULL);
  gtk_text_buffer_create_tag (buffer, "big_gap_after_line", "pixels_below_lines", 30, NULL);
  gtk_text_buffer_create_tag (buffer, "wide_margins",  "left_margin", 50, "right_margin", 50, NULL);
  gtk_text_buffer_create_tag (buffer, "double_spaced_line", "pixels_inside_wrap", 10, NULL);

  gtk_text_buffer_create_tag (buffer, "blue_foreground", "foreground", "LightSkyBlue", NULL);
  gtk_text_buffer_create_tag (buffer, "red_foreground", "foreground", "red", NULL);

  gtk_text_buffer_create_tag (buffer, "orange_foreground", "foreground", "goldenrod1"/*"OrangeRed1"*/, NULL);
  gtk_text_buffer_create_tag (buffer, "lightseagreen_foreground", "foreground", "aquamarine2", NULL);
  gtk_text_buffer_create_tag (buffer, "lightgray_foreground", "foreground", "gray86", NULL);

  gtk_text_buffer_create_tag (buffer, "big", "font-desc", font_desc,
         /* points times the PANGO_SCALE factor */
         "size", 70 * PANGO_SCALE, NULL);

  gtk_text_buffer_create_tag (buffer, "another_big",
         /* points times the PANGO_SCALE factor */
         "size", 70 * PANGO_SCALE, NULL);

  gtk_text_buffer_create_tag (buffer, "little_big",
         /* points times the PANGO_SCALE factor */
         "size", 30 * PANGO_SCALE, NULL);

 gtk_container_add (GTK_CONTAINER (frame_vert), view);
  //gtk_box_pack_start(GTK_BOX (hbox), view, TRUE, TRUE, 6);


 GdkGeometry geomtry_hints;
 //geomtry_hints.min_width   = 100;
 //geomtry_hints.min_height  = 290;
 geomtry_hints.max_width   = 690;
 geomtry_hints.max_height  = 290;
 //geomtry_hints.base_height = -1;
 //geomtry_hints.base_width  = -1;

 gtk_window_set_default_size(GTK_WINDOW(window), gWndWidth, gWndHeight);//623, 290);
 //gtk_widget_set_size_request(GTK_WIDGET(window), gWndWidth, gWndHeight);
 gtk_window_set_geometry_hints(GTK_WINDOW(window), NULL, &geomtry_hints, (GdkWindowHints)(GDK_HINT_MAX_SIZE));
 gtk_window_set_title(GTK_WINDOW(window), buf_name); // "GTester 0.96.1");
 gtk_window_set_position (GTK_WINDOW (window), GTK_WIN_POS_CENTER);

 gdk_threads_add_timeout( 500, cb_timeout, (gpointer)buffer );

 /* Create a vertical box with buttons */
 frame_vert = gtk_frame_new (NULL);//"Controls:");
 gtk_widget_set_halign (frame_vert, GTK_ALIGN_START);
 gtk_box_pack_end(GTK_BOX (hbox), frame_vert, FALSE, TRUE, 10);

 overlay = gtk_overlay_new ();
 gtk_container_add (GTK_CONTAINER (frame_vert), overlay);

 //vbox = gtk_vbox_new (TRUE, 7);
 vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);
 gtk_container_add (GTK_CONTAINER (overlay), vbox);
 //gtk_container_add (GTK_CONTAINER (frame_vert), vbox);

 //button = gtk_button_new_from_icon_name (GTK_STOCK_CONNECT, GTK_ICON_SIZE_BUTTON);
 button = gtk_button_new_with_mnemonic ("_Connect");
 g_signal_connect (G_OBJECT (button), "clicked", G_CALLBACK (helloWorld), (gpointer) button);
 gtk_box_pack_start (GTK_BOX (vbox), button, TRUE, TRUE, 0);

 btn_start_getval = gtk_button_new_with_mnemonic ("_Start");
 gtk_widget_set_sensitive(btn_start_getval, FALSE);
 g_signal_connect (G_OBJECT (btn_start_getval), "clicked", G_CALLBACK (start_usb_accept), NULL/*(gpointer) thread*/ );
 gtk_box_pack_start (GTK_BOX (vbox), btn_start_getval, TRUE, TRUE, 0);

 /*but_corr_usb_accept = gtk_button_new_with_mnemonic ("_Refresh");
 gtk_widget_set_sensitive(but_corr_usb_accept, FALSE);
 g_signal_connect (G_OBJECT (but_corr_usb_accept), "clicked", G_CALLBACK (correct_usb_accept), NULL );
 gtk_box_pack_start (GTK_BOX (vbox), but_corr_usb_accept, TRUE, TRUE, 0);*/

 button = gtk_button_new_with_mnemonic("_Limits F3");
 g_signal_connect (G_OBJECT (button), "clicked", G_CALLBACK (levels_accept), GTK_WINDOW(window));
 gtk_box_pack_start (GTK_BOX (vbox), button, TRUE, TRUE, 0);

 button = gtk_button_new_with_mnemonic("_Parameters F7");//"_Notify");
 g_signal_connect (G_OBJECT (button), "clicked", G_CALLBACK (parameters_accept), GTK_WINDOW(window) );
 gtk_box_pack_start (GTK_BOX (vbox), button, TRUE, TRUE, 0);

 button = gtk_button_new_with_mnemonic("_About");
 g_signal_connect (button, "clicked", G_CALLBACK (about_us), GTK_WINDOW(window));
 gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 1);

 button = gtk_button_new_with_mnemonic ("_Quit F12");
 g_signal_connect (button, "clicked", G_CALLBACK(quit_app)/*gtk_main_quit*/, G_APPLICATION(data));
 gtk_box_pack_start (GTK_BOX (vbox), button, TRUE, TRUE, 0);

 //gtk_box_pack_start (GTK_BOX (vbox), overlay, TRUE, TRUE, 0);

 radio1 = gtk_check_button_new_with_mnemonic("Sound _mute");
 gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio1), sound_off);
 g_signal_connect (G_OBJECT(radio1), "toggled", G_CALLBACK (toggle_snd_off), NULL);
 gtk_box_pack_start(GTK_BOX (vbox), radio1, FALSE, FALSE, 0);


 label = gtk_label_new ("<span color=\"#F90101\">F5 mute on/off</span>");
 gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
 //gtk_label_set_text( GTK_LABEL (label), "F5 on/off");
 gtk_label_set_justify( GTK_LABEL(label), GTK_JUSTIFY_CENTER);
 //gtk_box_pack_start(GTK_BOX (vbox), label, FALSE, FALSE, 0)
 gtk_widget_set_halign (label, GTK_ALIGN_CENTER);
 gtk_widget_set_valign (label, GTK_ALIGN_END);

 gtk_overlay_add_overlay (GTK_OVERLAY (overlay), label);
 gtk_widget_set_margin_left (label, 5);
 gtk_widget_set_margin_right (label, 5);
 gtk_widget_set_margin_top (label, 5);
 gtk_widget_set_margin_bottom (label, 17);

 radio2 = gtk_check_button_new_with_mnemonic("Speaker o_ff");
 //gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio2), speaker_off);
 g_signal_connect (G_OBJECT(radio2), "toggled", G_CALLBACK (toggle_spkr_off), NULL);
 gtk_box_pack_start(GTK_BOX (vbox), radio2, FALSE, FALSE, 0);

 /*radio1 = gtk_check_button_new_with_mnemonic("Periodic _notify");
 gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio1), bperiod_notify);
 g_signal_connect (radio1, "toggled", G_CALLBACK (toggle_period_notify), NULL);
 gtk_box_pack_start(GTK_BOX (vbox), radio1, TRUE, TRUE, 0);*/


  /*string _case1, _case2, _case3 ;
 /* Create a radio button /
  radio1 = gtk_radio_button_new_with_label_from_widget (NULL, "AC");

  _case1 = "1";
  g_signal_connect (radio1, "toggled", G_CALLBACK (toggle_admin), (gpointer)&_case1[0]);
  /*entry = gtk_entry_new ();
  gtk_container_add (GTK_CONTAINER (radio1), entry);/


  /* Create a radio button with a label /
  radio2 = gtk_radio_button_new_with_label_from_widget (GTK_RADIO_BUTTON (radio1),
               "DC");
   _case2 = "2";
   g_signal_connect (radio2, "toggled",  G_CALLBACK (toggle_admin), (gpointer)&_case2[0]);

   radio3 = gtk_radio_button_new_with_label_from_widget (GTK_RADIO_BUTTON (radio2),
               "mV");
 _case3 = "3";
   g_signal_connect (radio3, "toggled",  G_CALLBACK (toggle_admin), (gpointer)&_case3[0]);

 gtk_box_pack_start (GTK_BOX (vbox), radio1, TRUE, TRUE, 2);
 gtk_box_pack_start (GTK_BOX (vbox), radio2, TRUE, TRUE, 2);
 gtk_box_pack_start (GTK_BOX (vbox), radio3, TRUE, TRUE, 2);*/

 //if( InitDB() < 0 ) return NULL;

 /* Enter the main loop */
 //gtk_window_present( GTK_WINDOW(window) );
 string _bg_color;
 _bg_color = "#000000000000";
 if (gdk_rgba_parse(&bg_color, (const gchar*)&_bg_color[0]))
 {
    gtk_widget_override_background_color(GTK_WIDGET(view), GTK_STATE_FLAG_NORMAL, &bg_color);
    gtk_widget_override_background_color(GTK_WIDGET(aux_view), GTK_STATE_FLAG_NORMAL, &bg_color);
 }

  //gtk_widget_modify_base (GTK_WIDGET(view), GTK_STATE_NORMAL, &bg_color);

 gtk_widget_show_all (GTK_WIDGET(window));

 /*FILE *p;
 p = fopen("hello.txt", "w");
 fclose(p);*/
 //DeleteFile("hello.txt");
 const char file_name[] = "hello.txt";
 g_file_test(file_name, G_FILE_TEST_EXISTS) ? remove(file_name) : 0;
 //gtk_main ();
 //FreeConsole();
 return window;
}

/* Close the splash screen */
/*gboolean close_screen(gpointer data)
{
 gtk_widget_destroy(GTK_WIDGET(data));
 //gtk_main_quit ();
 return(FALSE);
}*/

/*int Show_Splash_Screen(char* image_name,int time,int width,int height, gpointer data)
{
 GtkWidget  *image, *window = NULL;
 //window = GTK_WIDGET(data);
 window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
 gtk_widget_set_size_request (window, width, height);
 gtk_window_set_decorated(GTK_WINDOW (window), FALSE);
 gtk_window_set_position(GTK_WINDOW(window),GTK_WIN_POS_CENTER_ALWAYS);
 gtk_window_set_type_hint(GTK_WINDOW(window),GDK_WINDOW_TYPE_HINT_SPLASHSCREEN);
 gtk_window_set_resizable(GTK_WINDOW(window), FALSE);
 image=gtk_image_new_from_file(image_name);
 gtk_container_add(GTK_CONTAINER(window), image);
 gtk_widget_show_all (window);
 //bclose_splash ? close_screen(GTK_WIDGET(window)) : NULL;
 g_timeout_add (time, close_screen, window);
 //gtk_main ();
 return 0;
}*/

static GActionEntry app_entries[] = {
  { "quit", activate_quit, NULL, NULL, NULL }
  //{ "dark", activate_toggle, NULL, "false", change_theme_state }
};

static void
activate_quit (GSimpleAction *action,
      GVariant   *parameter,
      gpointer    user_data)
{
 GApplication *app = G_APPLICATION(user_data);
 GtkWidget *dialog, *label, *content_area;
 //GtkApplication *app = GTK_APPLICATION(user_data);
 //GtkWidget *win;//GTK_WIDGET(user_data);
 //GList *list, *next;


 //list = gtk_application_get_windows ( GTK_APPLICATION(app) );

 /* while (list)
  {
    win = GTK_WIDGET(list->data);
    next = list->next;

    gtk_application_remove_window ( GTK_APPLICATION(app), GTK_WINDOW(win));
    //gtk_widget_destroy ( win );

    list = next;
  }*/
 gint response;
 gboolean pass_by = FALSE;
 if( parameter != NULL )
 {
     if(pass_by = g_variant_get_boolean(parameter))
        response = GTK_RESPONSE_YES;
 }
 dialog = gtk_dialog_new_with_buttons("Confirmation",
                                   GTK_WINDOW (window),
                                   GTK_DIALOG_MODAL ,
                                   GTK_STOCK_YES,
                                   GTK_RESPONSE_YES,
                                   GTK_STOCK_NO,
                                   GTK_RESPONSE_NO,
                                   NULL);
 gtk_window_set_destroy_with_parent (GTK_WINDOW(dialog), TRUE);

 content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
 label = gtk_label_new ("<span color=\"#F90101\">Do you really want to exit?!</span>");
 gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
 gtk_label_set_justify( GTK_LABEL(label), GTK_JUSTIFY_CENTER);
 gtk_container_add (GTK_CONTAINER (content_area), label);

 //g_signal_connect_swapped (dialog, "response", G_CALLBACK (gtk_widget_destroy), dialog);
 gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_NO);

 gtk_widget_show_all(label);
 pass_by == FALSE ? response = gtk_dialog_run (GTK_DIALOG (dialog)) : pass_by;
 if(response == GTK_RESPONSE_YES)
 {
    delete [] ipc_notifier_path;
    delete [] ipc_notify_audio_file_path;
    delete [] alarm_audio_file_path;

    gst_element_set_state (playbin2, GST_STATE_NULL);
    gst_object_unref (GST_OBJECT (playbin2));

    if(isSettingsNew)
    {
        //save_app_params(NULL);
        ofstream ofpar("params.dat");
        if(ofpar.is_open())
            ofpar.close();

        ofpar.open("params.dat", ios::out);

        if(ofpar.is_open())
        {
            char buf_tm[20];
            time( &rawtime );
            pTime = localtime( &rawtime );
            strftime(gl_buf_tm, sizeof(gl_buf_tm),"%Y%m%d", reinterpret_cast<tm*>(pTime));

            ofpar << rmultNum << " " << gUp << " " << gLow << endl;
            //ofpar << (int)strtol (gl_buf_tm, (char**)NULL, 10) << " "<< kW << endl;
            ofpar << gl_buf_tm << " " << kW << endl;
            ofpar << period_mod << endl;
            ofpar << min_per_tm << " " << max_per_tm << endl;
            ofpar << gIgnoranceVal << " " << gCountDownTime << " " << g_path_get_basename(gCmdLine) << params << endl;
            ofpar << font_no << " " << isMakeZero_Watt_counter <<endl;
        }

        ofpar.close();
    }

    gStarted = false;

    //usleep(100);
    if(hdev != NULL)
    {
        libusb_close(hdev);
        libusb_exit(ctx);
    }
    //Sleep(300);

    FinalizeDB();

    //app_shutdown(G_APPLICATION(app), NULL);

    //gst_object_unref (bus);

    //gst_element_set_state (playbin2, GST_STATE_NULL);
    //gst_object_unref (GST_OBJECT (playbin2));

    gtk_widget_destroy(GTK_WIDGET(window));
    g_application_quit( G_APPLICATION(app) );
 }
 gtk_widget_destroy (dialog);
}

void
open(GApplication *app, gpointer  user_data)
{
 GList *list;
 GtkWidget * gTesterWindow;

 list = gtk_application_get_windows ( GTK_APPLICATION(app) );
 if (list)
  gtk_window_present( GTK_WINDOW (list->data) );
 else
 {
  //Show_Splash_Screen("c://gt_splash.png" ,50, 25, 50, NULL);
  gTesterWindow = create_my_window(G_APPLICATION(app));
  gtk_window_set_application (GTK_WINDOW (gTesterWindow), GTK_APPLICATION(app));
  gtk_widget_show (gTesterWindow);
  //bclose_splash = true;
 }
}

void
app_shutdown(GApplication *app, gpointer user_data)
{
 //libusb_close(hdev);

/* gst_object_unref (bus);
 gst_element_set_state (playbin2, GST_STATE_NULL);
 gst_object_unref (GST_OBJECT (playbin2));*/

 //FinalizeDB();
}

void
startup(GApplication *app, gpointer user_data)
{
 /* Initialisation */
 r = libusb_init(&ctx); //initialize a library session
}

static void
activate (GApplication *app, gpointer user_data)
{
 open(app, NULL);
}

int main (int argc, char *argv[])
{
    //char path_buf[128];

    pCurrentDir = g_get_current_dir();

	pStr2num = new char[10];
	pWatts   = new char[32];

	ipc_notifier_path = new gchar[WORD_SIZE];
	ipc_notify_audio_file_path = new char[WORD_SIZE];
	alarm_audio_file_path = new char[WORD_SIZE];

	if( pStr2num == NULL || pWatts == NULL ) return -1;

    if(ipc_notifier_path)
    {
        /*STARTUPINFO si;
        PROCESS_INFORMATION pi;*/
        unsigned int len = strlen((const char*)pCurrentDir);

        ZeroMemory( &si, sizeof(si) );
        si.cb = sizeof(si);
        ZeroMemory( &pi, sizeof(pi) );

        memset(ipc_notifier_path, 0, WORD_SIZE);
        strncpy(ipc_notifier_path/*path_buf*/, pCurrentDir, len);
        strncat(ipc_notifier_path/*path_buf*/,"\\", 2);

        strncpy(ipc_notify_audio_file_path, "file:///", 8);
        strncpy(alarm_audio_file_path, "file:///", 8);

        strncat(ipc_notify_audio_file_path, ipc_notifier_path, len+2);
        strncat(ipc_notify_audio_file_path, "ipc_notify.ogg", 14);
        g_strdelimit(ipc_notify_audio_file_path, "\\", '/');

        strncat(alarm_audio_file_path, ipc_notifier_path, len+2);
        strncat(alarm_audio_file_path, "alarm.ogg", 9);
        g_strdelimit(alarm_audio_file_path, "\\", '/');

        strncat(ipc_notifier_path/*path_buf*/, "splashWindow.exe",16);
        //g_strdelimit(ipc_notifier_path/*path_buf*/, "\\", '/');
        //_spawnl(_P_NOWAITO, ipc_notifier_path/*path_buf*/, NULL);

        if( !CreateProcess( ipc_notifier_path,   // No module name (use command line)
            NULL,        // Command line
            NULL,           // Process handle not inheritable
            NULL,           // Thread handle not inheritable
            FALSE,          // Set handle inheritance to FALSE
            0,              // No creation flags
            NULL,           // Use parent's environment block
            NULL,           // Use parent's starting directory
            &si,            // Pointer to STARTUPINFO structure
            &pi )           // Pointer to PROCESS_INFORMATION structure
        )
        {
            last_error = GetLastError();
            //return;
        }

        strncpy(&ipc_notifier_path[len+1], "ipc_notifier.exe", 16);
        //ofstream outf;
        //len = 0;
        //_get_errno((int*)&len);
        //outf.open("tmp.out", ios::out);
        //outf <<  "CreateProcess failed:" << len << endl;
        //outf.close();
    }




	vSerr.resize(4); //reserve(4);
	vSettings.resize(6);//reserve(6);

	vSerr[0] = "DC";
	vSerr[1] = "DC mV";
	vSerr[2] = "AC Auto";
	vSerr[3] = "AC NotAuto";

	vSettings[0] = " | Up:";
	vSettings[2] = " | Low:";
	vSettings[4] = " | CFWC:";

  if (InitDB() < 0) return NULL;

  GError *error;
  if( thread3 == NULL)
    {
       //g_thread_unref( _thread );
        thread3 = g_thread_try_new("mine3", thread_func3, (gpointer)window, &error);
        if( ! thread3 )
        {
            //g_print( "Error: %s\n", error->message );
            return -1;
        }
        //bdo_some_thing = true;
    }

 //GtkApplication *app;

   //guint bus_watch_id;

   //r = libusb_init(&ctx); //initialize a library session

   /* Initialisation */
  //if( InitDB() < 0 ) return NULL;
  gst_init (&argc, &argv);

   /* Initialize GTK+ */
   /*g_log_set_handler ("Gtk", G_LOG_LEVEL_WARNING, (GLogFunc) gtk_false, NULL);
   gtk_init (&argc, &argv);
   g_log_set_handler ("Gtk", G_LOG_LEVEL_WARNING, g_log_default_handler, NULL);*/

   //Show_Splash_Screen("/org/gnome/gnome-screenshot/test.png" ,10, 25, 50, GTK_WINDOW(win));

   /* Create gstreamer elements */
/* pipeline = gst_pipeline_new ("audio-player");
     source = gst_element_factory_make ("filesrc", "file-source");
       demuxer = gst_element_factory_make ("oggdemux", "ogg-demuxer");
       //demuxer = gst_element_factory_make ("oggdemux", "demuxer");
 decoder = gst_element_factory_make ("vorbisdec", "vorbis-decoder");
 conv = gst_element_factory_make ("audioconvert", "converter");
 sink = gst_element_factory_make ("autoaudiosink", "audio-output");

 if (!pipeline || !source || !demuxer || !decoder || !conv || !sink) {
  g_printerr ("One element could not be created. Exiting.\n");
  return -1;
 }*/

 //pipeline = gst_parse_launch ("playbin uri=file://C:/gstreamer/1.0/x86/bin/test.ogg", NULL);

 /*playbin2 = gst_element_factory_make ("playbin", "playbin2");
 if( !playbin2){
  g_printerr("Not all elements could be made. \n");
  return -1;
 }*/
 //else{
 /* Set up the pipeline */
 /* we set the input filename to the source element */
 //g_object_set (G_OBJECT (source), "location", "test.ogg", NULL);
 //g_object_set (G_OBJECT (playbin2), "uri", "file:///C:/test.ogg", NULL);
   // g_object_set (G_OBJECT (playbin2), "uri", "file:////home/mdt/test.ogg", NULL);

 /* we add a message handler */
 //bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
  /*gsStateChRet = gst_element_set_state (playbin2, GST_STATE_PLAYING);

     bus = gst_element_get_bus(playbin2);
   bus_watch_id = gst_bus_add_watch (bus, bus_call, NULL);*/
 //gst_object_unref (bus);

 /* we add all elements into the pipeline */
 /* file-source | ogg-demuxer | vorbis-decoder | converter | alsa-output */
 //gst_bin_add_many (GST_BIN (pipeline),source, demuxer, decoder, conv, sink, NULL);
 //gst_bin_add_many (GST_BIN (pipeline),source, demuxer, NULL);
 //gst_element_link_pads(source, "src", demuxer, "sink");

 /* we link the elements together */
 /* file-source -> ogg-demuxer ~> vorbis-decoder -> converter -> alsa-output */
 /*gst_element_link (source, demuxer);
 gst_element_link_many (decoder, conv, sink, NULL);*/

 //g_signal_connect (demuxer, "pad-added", G_CALLBACK (on_pad_added), decoder);
 //}


   //if( InitDB() < 0 ) return 0;

   if ( g_application_id_is_valid ("tkl.mdt.GTester") )
    gApp = gtk_application_new ("tkl.mdt.GTester", G_APPLICATION_FLAGS_NONE);
   else
   {
     g_object_unref( gApp );
     return -1;
   }

 g_action_map_add_action_entries (G_ACTION_MAP (gApp),
           app_entries, G_N_ELEMENTS (app_entries),
           gApp);

 g_signal_connect (gApp, "startup", G_CALLBACK (startup), NULL);
 g_signal_connect (gApp, "shutdown", G_CALLBACK (app_shutdown), NULL);
 g_signal_connect (gApp, "activate", G_CALLBACK (activate), NULL);
 //g_signal_connect (app, "open", G_CALLBACK (open), NULL);

 //gtk_main ();

 //gst_element_set_state (pipeline, GST_STATE_NULL);
 //gst_object_unref (GST_OBJECT (pipeline));

 //FinalizeDB();

 int status = g_application_run (G_APPLICATION (gApp), 0, NULL);

 g_object_unref( gApp );

 //delete sel_case;

 /*gst_object_unref (bus);
 gst_element_set_state (playbin2, GST_STATE_NULL);
 gst_object_unref (GST_OBJECT (playbin2));*/

 //g_source_remove (bus_watch_id);

 //libusb_close(hdev);
 return 0;
}

int hid_send_feature_report(libusb_device_handle *hdev, const unsigned char *data, size_t length)
{
  int res = -1;
  int skipped_report_id = 0;
  int report_number = data[0];

  if (report_number == 0x0) {
    data++;
    length--;
    skipped_report_id = 1;
  }

  res = libusb_control_transfer(hdev,
    LIBUSB_REQUEST_TYPE_CLASS|LIBUSB_RECIPIENT_INTERFACE|LIBUSB_ENDPOINT_OUT,
    0x09/*HID set_report*/,
    (3/*HID feature*/ << 8) | report_number,
    0,
    (unsigned char *)data, length,
    1000/*timeout millis*/);

  if (res < 0)
    return -1;

  /* Account for the report ID */
  if (skipped_report_id)
    length++;
  return length;
}
