/* C* Bismillahir Rahmanir Rahiym  C* */

//Muqobil Dasturlar To'plami (c) hijriy 1436 (melodiy 2014-2015)

#include <algorithm>
#include <vector>
#include <iterator>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <ctime>
#include <stdlib.h>
#include <math.h>

#include <gst/gst.h>
#include <gtk/gtk.h>

#include <sqlite3.h>
#include <libusb.h>


#define DEV_CONFIG 1
#define WORD_SIZE 64

#define abs(x) fabs(x)

//consts
const char ver[]      = { "0.78" };
const char prog_name[]= {"GTester"};

using namespace std;

sqlite3 *pDB, *pProdsBoughtDB;
sqlite3_stmt *pStmt;

std::string insert_next_navID = "insert into nav_watts values(:dateidx, $navID, $Watt)";
std::string insert_next_mv = "insert into nav_mv values($navID, :timeID, $mv, $Watt)";

GstElement *pipeline, *source, *demuxer, *decoder, *conv, *sink;
GstElement *playbin2;
GstBus *bus;
GstStateChangeReturn gsStateChRet;

libusb_device **devs; //pointer to pointer of device, used to retrieve a list of devices
libusb_context *ctx = NULL; //a libusb session
libusb_device_handle *hdev;

int r; //for return values
ssize_t cnt; //holding number of devices in list

GtkWidget *view = NULL, *aux_view = NULL;
GtkWidget *but_start_getval;

unsigned char buf[9];
gchar Watt[20];

int i=0, c=0, len=9, actual, base[6];
int timeOut=0, navID, bResume;
short _count_alrm = 0;

std::string tmp, str2num, serr, gTimeStr, sumWatt;
bool corr_order = false, isSettingsNew = false, bTaskPause = false;

GMutex data_mutex;
GCond data_cond;

G_LOCK_DEFINE_STATIC( corr_order );

bool gStarted = false, sound_off = false;
gfloat gUp = 0.0, gLow = 0.0, multNum;
gfloat fres;
double kW=0;

time_t rawtime;
struct tm *pTime;

std::vector<double> amv_per_sec, amv_median, awatt_per_hour;

static void on_pad_added (GstElement *element, GstPad *pad, gpointer data);

int hid_send_feature_report(libusb_device_handle *hdev, const unsigned char *data, size_t length);

void corr_order_foo (gpointer data);
bool isorder_foo(void);

bool copyf(std::istream &ifs, std::ostream &ofs)
{
    ofs << ifs.rdbuf();
    return true;
}

void backup_db()
{
  std::string name_fl = "navbat_id";
  std::ifstream inp(name_fl.c_str(), std::ios_base::in | std::ios_base::binary);
  std::ofstream outp;//("navbat_id.bak");

  name_fl.append(gTimeStr+".bak");
  outp.open(&name_fl[0], std::ios_base::out | std::ios_base::binary );
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

//ostream_iterator< double > ofile(cout, " ");

static gpointer thread_func2(gpointer data)
{
    int ret, _timeout=0;
    double dret = 0;
    //double kW=0;
    char buf_tm[20];
    struct tm *_pTime;

    string del_sql = "delete from nav_mv where navID = $navID";

    while(gStarted)
    {
        //cout << timeOut;
        //isorder_foo();
        sleep(1000);
        //cout << str2num << endl;
        if(isorder_foo() && !bTaskPause)
            {
                timeOut++;
                //c = 0;
                //corr_order = false;
                amv_per_sec[timeOut] = fres;
            }

        if( (timeOut != 0) && (timeOut % 60 == 0) )
        {
            //struct tm *info;
            sort(amv_per_sec.begin(), amv_per_sec.end());
            dret = abs(median(amv_per_sec, amv_per_sec.size()));

            time( &rawtime );
            _pTime = localtime( &rawtime );
            strftime(buf_tm, sizeof(buf_tm),"%H%M", _pTime);
            gTimeStr.assign(buf_tm);

            timeOut = 0;
            //cout << timeOut << endl;
            //if( abs(fres) >= 0.4) //|| fres < (-5.0) )
            if( dret >= 0.4)
            {
                amv_median[++r] = dret;
                //kW += (dret*16.666/0.26);
                kW += (dret*multNum);//64.1);
            }
            else{
                //_timeout++;
                //if((_timeout != 0) && (_timeout % 180 == 0))
                    {
                        //_timeout = 0;
                        kW +=0;
                    }
            }
            g_ascii_dtostr(Watt, 9, kW);
            //copy(amv_per_sec.begin(), amv_per_sec.end(), ofile);

            cout << gTimeStr << " " << r << " " << amv_median[r] << " " << kW << " " << Watt << endl;

                    if (gTimeStr.compare("0800") == 0)
                    {
                        //dret = median(amv_median, amv_median.size());
                        //awatt_per_hour.push_back(dret/0.26);

                        //sort(awatt_per_hour.begin(), awatt_per_hour.end());

                        strftime(buf_tm, sizeof(buf_tm), "%Y%m%d", pTime);
                        gTimeStr.assign(buf_tm);

                        //back up the DB for removing useless dates for next month
                        if( _pTime->tm_mday == 1 )//|| pTime->tm_mday == 29 || pTime->tm_mday == 30 || pTime->tm_mday == 31)
                            {

                                backup_db();

                                ret = sqlite3_prepare_v2(pDB, &del_sql[0], -1, &pStmt, 0);

                                for( unsigned short count_day=1; count_day < 32; count_day++)
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

                        ret = sqlite3_prepare_v2(pDB, &insert_next_navID[0], -1, &pStmt, 0);
                        //kW = median(awatt_per_hour, awatt_per_hour.size());

                        if(ret == SQLITE_OK)
                            {
                                //string msg_info = "insert SQL statement has error...";
                                //MessageBox(msg_info);
                                //return;
                                ret = sqlite3_bind_text(pStmt, 1, &gTimeStr[0], -1,  NULL);
                                ret = sqlite3_bind_int (pStmt, 2, navID);
                                ret = sqlite3_bind_double(pStmt, 3, kW);

                                if( ret != SQLITE_OK) cout << "Error in bind" <<endl;

                                ret = sqlite3_step(pStmt);

                                if(ret != SQLITE_DONE)
                                {
                                    string msg_info = "SQL command evaluation not done...";
                                    cout << msg_info << endl;
                                    //MessageBox(msg_info);
                                    //return;
                                }
                            }
                            //back to the...
                            kW=0;
                            navID = _pTime->tm_mday;
                            *pTime = *_pTime;  //pass the date(yyyy.mm.dd HH.MM) of the new today
                            //awatt_per_hour.clear();
                            strftime(buf_tm, sizeof(buf_tm),"%H%M", _pTime);
                            gTimeStr.assign(buf_tm);
                            sumWatt = " :";
                            sumWatt.append(Watt);

                    }//Watts and next navID

            if(r == amv_median.size()){
                r = 0;
                dret = median(amv_median, amv_median.size());
                //kW += dret/0.26;
                //awatt_per_hour.push_back(dret/0.26);
                //g_ascii_dtostr((gchar*)Watt, 10, kW);
                //cout << "1h mV median: "<< dret <<" "<< dret/0.26 << endl;

                ret = sqlite3_prepare_v2(pDB, &insert_next_mv[0], -1, &pStmt, 0);

                    if(ret == SQLITE_OK){

                        ret = sqlite3_bind_int (pStmt, 1, navID);
                        ret = sqlite3_bind_text(pStmt, 2, &gTimeStr[0], -1,  NULL);
                        ret = sqlite3_bind_double(pStmt, 3, dret);
                        ret = sqlite3_bind_double(pStmt, 4, kW);

                        ret = sqlite3_step(pStmt);

                        if(ret != SQLITE_DONE)
                                {
                                    string msg_info = "SQL command evaluation not done...";
                                    cout << msg_info << endl;
                                }

                        //cout << gTimeStr << endl;
                    }
            }

            gTimeStr.clear();
            //amv_per_sec.clear();
        }
        //c = 0;
    }
}

static gpointer
thread_func( gpointer data )
{
    //gint len = 9, c=0, i=0, actual = 0, base[5];

    stringstream ss;
    string target;

    //char *pEnd;
    //c=0;

    //memset(buf,0x00,sizeof(buf));

    while ( actual  >= 0 )
            {

                if(c  == len)
                {
                    c = 0;
                    if(!corr_order) corr_order_foo(NULL);
                    //corr_order = true;
                    //ss.clear();
                    str2num.clear();
                    //for(i=0; i < len; i++)
                    {
                        //ss << got_nums[i];
                        //ss << tmp;//[i];
                    }
                    ss >> str2num;
                    //ss.clear();
                    //str2num.assign( tmp);
                    //base[2] = strtoul((char*)&tmp[5], reinterpret_cast<char**>(&tmp[6]), 10);
                    //ss << tmp[5]; ss >> target;

                    //str2num.clear();
                    //str2num = "000013231";
                    //if(len > 5)
                        {
                            base[2] = std::stod(string(1,str2num[5]), nullptr); //dot position in
                            base[3] = std::stod(string(1,str2num[6]), nullptr); //DC or AC
                            base[4] = std::stod(string(1,str2num[7]), nullptr); //DC or AC
                            base[5] = std::stod(string(1,str2num[8]), nullptr); //for determainin + or -
                            //cout << str2num;

                            if(base[2] < str2num.length() && base[2] != 0)
                                str2num.insert(base[2], ",");
                            else
                            if(base[3] == 3)
                                str2num.insert(3,",");

                            if (base[5] == 5 || base[5] == 6 || base[5] == 4){
                                    str2num.insert(0 , "-");
                                    str2num.resize(7);
                                }
                            else
                                str2num.resize(6);

                            if(base[3] == 3) serr = "DC mV";
                            else
                            if(base[3] == 1 && base[4] == 0){
                                serr = "DC";
                            }

                            if(base[3] == 2 && base[4] == 3) {
                                serr = "AC";
                            if (base[5] == 1)
                                serr.append(" Auto");
                            else
                            //if( base[5] == 2 )
                                serr.append( " NotAuto");
                            }
                        }

                    fres = atof((char*)&str2num[0]);
                    //fres = strtof((char*)&str2num[0], NULL);
                    //g_snprintf((gchar*)buf, sizeof(buf), "%.4f", fres);
                    //fres = atof((gchar*)buf);

                    //cout << str2num << " " << fres << endl;
                    //for(i=0; i< 179999; i++);

                    //gtk_text_buffer_insert_with_tags_by_name(buffer, &txt_iter, &str2num[0], -1, "big", "blue_foreground" ,"wide_margins", NULL);
                    //tmp.clear();
                    ss.clear();
                }

                //cout << actual << endl;
                //actual = 0; i = 0;
                //while( actual == 0)
                    {

                    //r = libusb_interrupt_transfer(hdev, 0x82, (unsigned char*)buf, sizeof(buf), &actual, 5000);
                    libusb_interrupt_transfer(hdev, 0x82, (unsigned char*)buf, sizeof(buf), &actual, 5000);
                    //actual = 1; i=1;
                    buf[1] &= 0x7F;

                    /*if( isdigit(buf[1] ) != 0)
                    {
                        //base[1] = atoi((char*)&buf[1]);
                        ++i;
                    }*/

                    if( isdigit(buf[1]) !=0  && c <= len && actual != 0)
                    {
                        //cout << actual << " " << sizeof(buf) << endl;
                        c++;
                        //tmp.append((char*)&buf[1]);
                        //tmp.push_back(buf[1]);
                        ss << buf[1];
                    }
                }

            }
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
    if(event->keyval == GDK_KEY_space) { bTaskPause = !bTaskPause; return FALSE;};
    return FALSE;
}

static gboolean
cb_timeout( gpointer data )
{
    GtkTextBuffer *buffer = GTK_TEXT_BUFFER(data);
    GtkTextIter txt_iter;
    gchar *label = NULL;
    //static short _count_alrm = 0;

    //G_LOCK( corr_order );

    //label = g_strdup_printf( "%s", &str2num[0] );
    //G_UNLOCK( corr_order );

    //gtk_button_set_label( GTK_BUTTON( data ), label );

    //buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));

    //buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));

    if(gStarted){
        //cout << str2num.length() << endl;
        gtk_text_buffer_set_text (buffer, "", 0);
        gtk_text_buffer_get_iter_at_offset (buffer, &txt_iter, 0);

        if(corr_order) {

                g_mutex_lock (&data_mutex);
                corr_order = false;
                g_cond_signal (&data_cond);
                g_mutex_unlock (&data_mutex);

                c = 0;
                //corr_order = false;
                //timeOut++;

                if ( ( abs(fres) < gLow ||  abs(fres) > gUp ) && (!bTaskPause) ){
                    ++_count_alrm;
                     if ((!sound_off) && (_count_alrm > 3) && abs(fres) > 0.15)
                        {
                            gsStateChRet = gst_element_set_state (GST_ELEMENT(playbin2), GST_STATE_PLAYING);
                            //if(gsStateChRet == GST_MESSAGE_EOS) _count_alrm = 0;
                        }
                    else gst_element_set_state (GST_ELEMENT(playbin2), GST_STATE_READY);
                }

                //if( timeOut%60 == 0)
                {
                //struct tm *info;
                /*char buf[20];

                time( &rawtime );

                pTime = localtime( &rawtime );

                strftime(buf,WORD_SIZE,"%H%M", pTime);
                gTimeStr.assign(buf);

                if (gTimeStr.compare("0800") == 0)
                    {
                        ++navID;
                        strftime(buf,WORD_SIZE,"%Y%m%d", pTime);
                        gTimeStr.assign(buf);

                        int ret = sqlite3_prepare_v2(pDB, &insert_next_navID[0], -1, &pStmt, 0);
                        int kW = 0;

                        if(ret == SQLITE_OK)
                            {
                                //string msg_info = "insert SQL statement has error...";
                                //MessageBox(msg_info);
                                //return;
                                ret = sqlite3_bind_text(pStmt, 1, &gTimeStr[0], -1,  NULL);
                                ret = sqlite3_bind_int (pStmt, 2, navID);
                                ret = sqlite3_bind_int (pStmt, 3, kW);

                                if( ret != SQLITE_OK) cout << "Error in bind" <<endl;

                                ret = sqlite3_step(pStmt);

                                if(ret != SQLITE_DONE)
                                {
                                    string msg_info = "SQL command evaluation not done...";
                                    cout << msg_info << endl;
                                    //MessageBox(msg_info);
                                    //return;
                                }
                            }
                            //back to the...
                            strftime(buf,WORD_SIZE,"%H%M", pTime);
                            gTimeStr.assign(buf);

                    }//Watts and next navID


                    //int ret = sqlite3_prepare_v2(pDB, &insert_next_mv[0], -1, &pStmt, 0);

                    if(ret == SQLITE_OK){

                        /*ret = sqlite3_bind_int (pStmt, 1, navID);
                        ret = sqlite3_bind_text(pStmt, 2, &gTimeStr[0], -1,  NULL);
                        ret = sqlite3_bind_double(pStmt, 3, fres);

                        ret = sqlite3_step(pStmt);

                        if(ret != SQLITE_DONE)
                                {
                                    string msg_info = "SQL command evaluation not done...";
                                    cout << msg_info << endl;
                                }/

                        cout << gTimeStr << endl;
                    }*/

                    //timeOut = 0;
                }

            }
        /*if(base[3] == 3) serr = "DC mV";
        else
        if(base[3] == 1 && base[4] == 0){
                serr = "DC";
                /*if (base[5] == 5 || base[5] == 6)
                    str2num.insert(0 , "-");/
        }*/
        //g_strlcat((gchar*)&str2num[0], "\n", -1);

        //str2num.append("\n");
        gtk_text_buffer_insert_with_tags_by_name(buffer, &txt_iter, &str2num[0], -1, "big", "blue_foreground", NULL);

        gtk_text_buffer_get_end_iter(buffer, &txt_iter);

        tmp = "\nWatts: ";
        tmp.append(Watt);
        gtk_text_buffer_insert_with_tags_by_name(buffer, &txt_iter, &tmp[0], -1, "right_justify", "little_big", NULL);
        //if(kW == 0)
        {
            //tmp.append(&sumWatt[0]);
            gtk_text_buffer_insert_with_tags_by_name(buffer, &txt_iter, &sumWatt[0], -1, "right_justify", "little_big", "red_foreground", NULL);
        }


        buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (aux_view));
        gtk_text_buffer_set_text (buffer, "", 0);

        gtk_text_buffer_get_iter_at_offset (buffer, &txt_iter, 0);
        //gtk_text_buffer_get_iter_at_line(buffer, &txt_iter, 0);
        bTaskPause ?
        gtk_text_buffer_insert_with_tags_by_name(buffer, &txt_iter, "Process Paused!\n", -1, "right_justify", NULL) :
        gtk_text_buffer_insert_with_tags_by_name(buffer, &txt_iter, "Process Started!\n", -1, "right_justify", NULL);

        gtk_text_buffer_get_end_iter(buffer, &txt_iter);
        gtk_text_buffer_insert_with_tags_by_name(buffer, &txt_iter, &serr[0], -1, "little_big", NULL);

    }

    //g_free( label );
    //c = 0;
    return( TRUE );
}

static void correct_usb_accept(GtkWidget *win, gpointer data)
{
    //G_LOCK( tmp );
    //tmp.clear();
    //G_UNLOCK( tmp );
    c=0;
    gst_element_set_state (playbin2, GST_STATE_READY);
    //gst_element_abort_state(GST_ELEMENT(pipeline));
}

GThread *thread2 = NULL;
static void start_usb_accept(GtkWidget *win, gpointer data)
{
    GtkTextBuffer *buffer = NULL;
    GtkTextIter txt_iter;
    //unsigned char buf[64];
    //int i, c, len, actual, base[6];

    //stringstream ss;
    //string str2num, tmp;

    //std::string str = "Process STARTED!!!\n";

    /*buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));
    gtk_text_buffer_set_text (buffer, "", 0);
    gtk_text_buffer_get_iter_at_offset (buffer, &txt_iter, 0);
    gtk_text_buffer_insert_with_tags_by_name(buffer, &txt_iter, &str[0], -1, "right_justify", "blue_foreground" ,"wide_margins", NULL);*/



    GThread *_thread = (GThread*)(data);
    GError *error, *error2;

    if( _thread == NULL )
        {
            //g_thread_unref( _thread );
            _thread = g_thread_try_new("mine", thread_func, NULL, &error);
            if( ! _thread )
            {
                g_print( "Error: %s\n", error->message );
                return;
            }
            //c=0;
            //cout << "test" << endl;
            gStarted = true;
        }
    if( thread2 == NULL )
        {
            thread2 = g_thread_try_new("mine2", thread_func2, NULL, &error2);
            if( !thread2 )
            {
                g_print( "Error: %s\n", error2->message );
                return;
            }
            r = 0;
            amv_per_sec.resize(60);
            amv_median.resize(10);
        }

    //while(  actual >= 0 )
        /*{
            if(c  >= len)
            {
                c = 0;
                 ss.clear();
                for(i=0; i <= 10; i++)
                    {
                        //ss << got_nums[i];
                        ss << tmp[i];
                    }
                ss >> str2num;
                base[2] = strtoul((char*)&tmp[5], reinterpret_cast<char**>(&tmp[6]), 10);

                //str2num.insert(base[2], ".");
                //fres = strtof((char*)&str2num[0], NULL);
                gtk_text_buffer_insert_with_tags_by_name(buffer, &txt_iter, &str2num[0], -1, "big", "blue_foreground" ,"wide_margins", NULL);
                tmp.clear();
            }

            actual = 0;
            //while ( actual  == 0 )
            {
                r = libusb_interrupt_transfer(hdev, 0x82, (unsigned char*)buf, sizeof(buf), &actual, 5000);
                buf[1] &= 0x7F;

                if( isdigit(buf[1] ) != 0)
                {
                   base[1] = atoi((char*)&buf[1]);
                    ++i;
                }

                if(i>0  && c <= len && actual != 0)
                {
                    ++c;
                    tmp.append((char*)&buf[1]);
                }
            }
        }*/

}
static void levels_accept(GtkWidget *wid, GtkWidget *win)
{
  GtkWidget *content_area;
  GtkWidget *dialog;
  GtkWidget *hbox;
  GtkWidget *stock;
  GtkWidget *table;
  GtkWidget *local_entry1;
  GtkWidget *local_entry2;
  GtkWidget *label;
  gint response;

  dialog = gtk_dialog_new_with_buttons ("Interactive Dialog",
                                        GTK_WINDOW (win),
                                        GTK_DIALOG_MODAL,// | GTK_DIALOG_DESTROY_WITH_PARENT,
                                        GTK_STOCK_OK,
                                        GTK_RESPONSE_OK,
                                        "_Cancle",
                                        GTK_RESPONSE_CANCEL,
                                        NULL);

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
  gtk_grid_attach (GTK_GRID (table), local_entry1, 1, 0, 1, 1);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), local_entry1);

  label = gtk_label_new_with_mnemonic ("Lo_w");
  gtk_grid_attach (GTK_GRID (table), label, 0, 1, 1, 1);

  local_entry2 = gtk_entry_new ();
  //gtk_entry_set_text (GTK_ENTRY (local_entry2), gtk_entry_get_text (GTK_ENTRY (entry2)));
  gtk_grid_attach (GTK_GRID (table), local_entry2, 1, 1, 1, 1);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), local_entry2);

  gtk_widget_show_all (hbox);
  response = gtk_dialog_run (GTK_DIALOG (dialog));

  if (response == GTK_RESPONSE_OK)
    {
       std::basic_string <char>::size_type pos;

       isSettingsNew = true;
      //string _tmp;
      //gtk_entry_set_text (GTK_ENTRY (entry1), gtk_entry_get_text (GTK_ENTRY (local_entry1)));
      //gtk_entry_set_text (GTK_ENTRY (entry2), gtk_entry_get_text (GTK_ENTRY (local_entry2)));
      tmp = static_cast<string>(gtk_entry_get_text (GTK_ENTRY (local_entry1)));

    /*if(gStarted){
        if(base[3] == 3)
            str2num.insert(3,",");
        else
            tmp.insert(base[2], ",");
        }*/
      //pos = tmp.find(".");
      //pos != std::basic_string <char>::npos ? tmp.replace(pos,1, ",") : tmp;
      gUp =  std::stof(tmp, nullptr);
      //gLow = strtof(gtk_entry_get_text (GTK_ENTRY (local_entry2)), nullptr);
      tmp = static_cast<string>(gtk_entry_get_text(GTK_ENTRY(local_entry2)));

    /*if(gStarted){
        if(base[3] == 3)
            str2num.insert(3,",");
        else
            tmp.insert(base[2], ",");
        }*/
      //pos = tmp.find(".");
      //pos != std::basic_string <char>::npos ? tmp.replace(pos,1, ",") : tmp;
      gLow = std::stof(tmp, nullptr);
      cout << gUp << gLow /*<< " " << std::stof("0,0001105", nullptr)*/ <<endl;
    }

  gtk_widget_destroy (dialog);

}

static void helloWorld (GtkWidget *wid, GtkWidget *win)
{
  GtkWidget *dialog = NULL;
  GtkTextBuffer *buffer = NULL;
  GtkTextIter txt_iter;
  bool bset_sensitive  = true;
  std::string str;// = "This just a testing TEXT!!!";

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


  hdev = libusb_open_device_with_vid_pid(ctx, 0x1a86, 0xe008);

  if(hdev==NULL)
            {
                bset_sensitive  = false;
                str = "Failed to connect \n c ya...";
                //libusb_close(hdev);
                //libusb_exit(ctx); //close the session
                //goto fin;
                gtk_text_buffer_insert_with_tags_by_name(buffer, &txt_iter, &str[0], -1, "little_big", "blue_foreground" , NULL);
                return;
            }
         else
            str = "Connected successfully!";

  libusb_free_device_list(devs, 1); //free the list, unref the devices in it

  if(libusb_kernel_driver_active(hdev,0) == 1)
            {
                str="Kernel active";
                gtk_text_buffer_insert_with_tags_by_name(buffer, &txt_iter, &str[0], -1, "big", "blue_foreground" ,"wide_margins", NULL);

                if(libusb_detach_kernel_driver(hdev,0) == 1){
                        str="Detachin from kernel DONE.";
                    gtk_text_buffer_insert_with_tags_by_name(buffer, &txt_iter, &str[0], -1, "big", "blue_foreground" ,"wide_margins", NULL);
                }
            }

          if ((r = libusb_set_configuration(hdev, DEV_CONFIG)) < 0)
          {
              bset_sensitive  = false;
              str = "Configuration settin failure!" ;
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
         //str = "Claimed interface";
         str = "Ready!";

  gtk_text_buffer_insert_with_tags_by_name(buffer, &txt_iter, &str[0], -1, "big", "blue_foreground" ,"wide_margins", NULL);

  memset(buf,0x00,sizeof(buf));
    r = hid_send_feature_report(hdev, buf, 6); // 6 bytes

    if (r < 0) {
            bset_sensitive  = false;
            str = "Unable to send a feature report.\n";
            gtk_text_buffer_insert_with_tags_by_name(buffer, &txt_iter, &str[0], -1, "big", "blue_foreground" ,"wide_margins", NULL);
        }

    gtk_widget_set_sensitive(but_start_getval, bset_sensitive);


  //start_usb_accetp(NULL, NULL);

  /*dialog = gtk_message_dialog_new (GTK_WINDOW (win), GTK_DIALOG_MODAL, GTK_MESSAGE_INFO, GTK_BUTTONS_CLOSE, "Hello World!");
  gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);
  gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_destroy (dialog);*/
}
static void
toggle_snd_off(GtkToggleButton *check_button, gpointer data)
{
if (gtk_toggle_button_get_active (check_button))
    sound_off = true;
  else
    sound_off = false;
}

static void
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

}

static void
on_pad_added (GstElement *element, GstPad *pad, gpointer data)
{
    GstPad *sinkpad;
    GstElement *decoder = (GstElement *) data;
    /* We can now link this pad with the vorbis-decoder sink pad */
    g_print ("Dynamic pad created, linking demuxer/decoder\n");
    sinkpad = gst_element_get_static_pad (decoder, "sink");
    gst_pad_link (pad, sinkpad);
    gst_object_unref (sinkpad);
    //gst_pad_unlink(pad, sinkpad);
}

static gboolean
bus_call (GstBus *bus, GstMessage *msg, gpointer data)
{
    GMainLoop *loop = (GMainLoop *) data;
    switch (GST_MESSAGE_TYPE (msg)) {
    case GST_MESSAGE_EOS:
        {
        //g_print ("End of stream\n");
        //g_main_loop_quit (loop);
        _count_alrm = 0;
        gst_element_set_state (playbin2, GST_STATE_READY);
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
            break;
        }
        default:
        break;
    }
return TRUE;
}

int InitDB()
{
  int ret, TimeStr;
  char buf_tm[20];
  /*string TimeStr;
  TimeStr.resize(10);*/

  ifstream ifpar;

   ifpar.open("params.dat", ios::in);

  time( &rawtime );
  pTime = localtime( &rawtime );
  strftime(buf_tm, sizeof(buf_tm),"%Y%m%d", pTime);

   if(ifpar.is_open())
   {
        ifpar >> multNum >> gUp >> gLow;

        /*ifpar.getline(&TimeStr[0], WORD_SIZE);
        ifpar.getline( &TimeStr[0], WORD_SIZE );*/
        ifpar >> TimeStr >> kW;

        //TimeStr.compare(buf_tm) != 0 ? cout << TimeStr << endl: cout << "Failed" << endl;
        TimeStr == std::stod(buf_tm, nullptr) ? kW : kW=0;
        printf("%.4f\n", kW);
        multNum = 16.666 / multNum;
    }
   /*else
    multNum = 64.1;*/
  ifpar.close();

  ret = sqlite3_open_v2("navbat_id", &pDB, SQLITE_OPEN_READWRITE, NULL);
  ret == SQLITE_OK ? cout << "DB connected OK!" <<endl : cout << "Failure..." <<endl;

  //string SQLst = "select count(*) from nav_watts";

  //ret = sqlite3_prepare_v2(pDB, &SQLst[0], SQLst.length() , &pStmt, NULL);
  //sqlite3_step(pStmt);

  //navID = sqlite3_column_int(pStmt, 0);
  //navID == 0 ? navID = 1 : navID = navID;


  navID = pTime->tm_mday;

  cout << navID <<endl;
  return ret;
}

int FinalizeDB()
{
  ofstream ofpar;

  if(isSettingsNew){
   ofpar.open("params.dat", ios::out);

    if(!ofpar.fail())
        {
            char buf_tm[20];
            strftime(buf_tm, sizeof(buf_tm),"%Y%m%d", pTime);

            ofpar << 16.666/multNum << " " << gUp << " " << gLow << endl;
            ofpar << (int)strtol (buf_tm, (char**)NULL, 10) << " "<< kW << endl;
        }
    ofpar.close();
  }

  int ret = sqlite3_finalize(pStmt);
  ret == SQLITE_OK ? cout << "statement finalized" <<endl : cout << "Failed..." <<endl;

  ret = sqlite3_close_v2(pDB);
  ret == SQLITE_OK ? cout << "DB closed" <<endl : cout << "Closing Failed..." <<endl;

  return ret;
}

void about_us( GtkWidget *widget,
            gpointer   data )
{

   gtk_show_about_dialog (NULL,
                          "program-name", prog_name,
                          "copyright", "Muqobil Dasturlar To'plami (c) 2014-?\nEvaluation edition",
                          "license", "The soft is released under the terms of GPL version 2.",
                          "version", ver,
                          "comments", "O'zbekiston, Toshkent shahri\n  muqobildasturlar@gmail.com",
                          "title", ("About GTester!"),
                          "authors", NULL,
                         NULL);

}

int main (int argc, char *argv[])
{
  GtkWidget *button = NULL, *radio1, *radio2, *radio3, *entry;
  GtkWidget *win = NULL;
  GtkWidget *vbox = NULL, *hbox = NULL, *frame_vert = NULL;
  GtkTextBuffer *buffer = NULL;
  GThread *thread = NULL;

  guint bus_watch_id;

  r = libusb_init(&ctx); //initialize a library session

  /* Initialisation */
  gst_init (&argc, &argv);


  /* Create gstreamer elements */
/*    pipeline = gst_pipeline_new ("audio-player");
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

    playbin2 = gst_element_factory_make ("playbin", "playbin2");
    if( !playbin2){
        g_printerr("Not all elements could be made. \n");
        return -1;
    }
    else{
    /* Set up the pipeline */
    /* we set the input filename to the source element */
    //g_object_set (G_OBJECT (source), "location", "test.ogg", NULL);
    g_object_set (G_OBJECT (playbin2), "uri", "file:///home/mdt/test.ogg", NULL);

    /* we add a message handler */
    //bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
    bus = gst_element_get_bus(playbin2);
    bus_watch_id = gst_bus_add_watch (bus, bus_call, NULL);
    gst_object_unref (bus);

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

    gsStateChRet = gst_element_set_state (playbin2, GST_STATE_PLAYING);
    }

  /* Initialize GTK+ */
  g_log_set_handler ("Gtk", G_LOG_LEVEL_WARNING, (GLogFunc) gtk_false, NULL);
  gtk_init (&argc, &argv);
  g_log_set_handler ("Gtk", G_LOG_LEVEL_WARNING, g_log_default_handler, NULL);

  /* Create the main window */
  win = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_container_set_border_width (GTK_CONTAINER (win), 8);
  gtk_window_set_title (GTK_WINDOW (win), "GTestter");
  gtk_window_set_position (GTK_WINDOW (win), GTK_WIN_POS_CENTER);
  gtk_widget_realize (win);
  g_signal_connect (win, "destroy", gtk_main_quit, NULL);
  g_signal_connect (G_OBJECT (win), "key_press_event", G_CALLBACK (on_key_press), NULL);


  if( InitDB() < 0 ) return 0;

  /* Create a vertical box with buttons */
  vbox = gtk_vbox_new (TRUE, 6);
  gtk_container_add (GTK_CONTAINER (win), vbox);

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_container_set_border_width (GTK_CONTAINER (hbox), 0);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 1);

  /*Values printin out place */
  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 0);
  gtk_box_pack_start (GTK_BOX (hbox), vbox, TRUE, TRUE, 2);

  frame_vert = gtk_frame_new (NULL);
  gtk_widget_set_valign (frame_vert, GTK_ALIGN_START);
  gtk_widget_set_size_request(frame_vert, 1, 55);
  gtk_box_pack_start(GTK_BOX (vbox), frame_vert, FALSE, FALSE, 2);

  aux_view = gtk_text_view_new ();
  gtk_text_view_set_editable(GTK_TEXT_VIEW (aux_view), FALSE);
  gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (aux_view), GTK_WRAP_WORD);

  gtk_container_add (GTK_CONTAINER (frame_vert), aux_view);

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (aux_view));
  gtk_text_buffer_create_tag (buffer, "right_justify", "justification", GTK_JUSTIFY_RIGHT, NULL);
  gtk_text_buffer_create_tag (buffer, "left", "justification", GTK_JUSTIFY_LEFT, NULL);
  gtk_text_buffer_create_tag (buffer, "little_big",
                              /* points times the PANGO_SCALE factor */
                              "size", 30 * PANGO_SCALE, NULL);


  //Main info view
  frame_vert = gtk_frame_new ("MDT:");
  gtk_widget_set_valign (frame_vert, GTK_ALIGN_START);
  gtk_widget_set_size_request(frame_vert, 1, 150);
  gtk_box_pack_start(GTK_BOX (vbox), frame_vert, FALSE, FALSE, 2);

  view = gtk_text_view_new ();
  gtk_text_view_set_editable(GTK_TEXT_VIEW (view), FALSE);
  gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (view), GTK_WRAP_WORD);

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));
  gtk_text_buffer_create_tag (buffer, "left", "justification", GTK_JUSTIFY_LEFT, NULL);
    gtk_text_buffer_create_tag (buffer, "right_justify", "justification", GTK_JUSTIFY_RIGHT, NULL);
    gtk_text_buffer_create_tag (buffer, "big_gap_before_line", "pixels_above_lines", 30, NULL);
    gtk_text_buffer_create_tag (buffer, "big_gap_after_line", "pixels_below_lines", 30, NULL);
    gtk_text_buffer_create_tag (buffer, "wide_margins",  "left_margin", 50, "right_margin", 50, NULL);
    gtk_text_buffer_create_tag (buffer, "blue_foreground", "foreground", "blue", NULL);
    gtk_text_buffer_create_tag (buffer, "red_foreground", "foreground", "red", NULL);
    gtk_text_buffer_create_tag (buffer, "big",
                              /* points times the PANGO_SCALE factor */
                              "size", 70 * PANGO_SCALE, NULL);
    gtk_text_buffer_create_tag (buffer, "little_big",
                              /* points times the PANGO_SCALE factor */
                              "size", 30 * PANGO_SCALE, NULL);

  gtk_container_add (GTK_CONTAINER (frame_vert), view);
  //gtk_box_pack_start(GTK_BOX (hbox), view, TRUE, TRUE, 6);

  gtk_window_set_default_size (GTK_WINDOW (win), 530, 290);
  gdk_threads_add_timeout( 500, cb_timeout, (gpointer)buffer );

/* Create a vertical box with buttons */
  frame_vert = gtk_frame_new ("Controls:");
  gtk_widget_set_valign (frame_vert, GTK_ALIGN_START);
  gtk_box_pack_end(GTK_BOX (hbox), frame_vert, FALSE, FALSE, 10);

  //vbox = gtk_vbox_new (TRUE, 7);
  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);
  gtk_container_add (GTK_CONTAINER (frame_vert), vbox);

  button = gtk_button_new_from_stock (GTK_STOCK_CONNECT);
  g_signal_connect (G_OBJECT (button), "clicked", G_CALLBACK (helloWorld), (gpointer) win);
  gtk_box_pack_start (GTK_BOX (vbox), button, TRUE, TRUE, 0);

  but_start_getval = gtk_button_new_from_stock (GTK_STOCK_GO_FORWARD);
  gtk_widget_set_sensitive(but_start_getval, FALSE);
  g_signal_connect (G_OBJECT (but_start_getval), "clicked", G_CALLBACK (start_usb_accept), (gpointer) thread );
  gtk_box_pack_start (GTK_BOX (vbox), but_start_getval, TRUE, TRUE, 0);

  button = gtk_button_new_from_stock (GTK_STOCK_REFRESH);
  g_signal_connect (G_OBJECT (button), "clicked", G_CALLBACK (correct_usb_accept), NULL );
  gtk_box_pack_start (GTK_BOX (vbox), button, TRUE, TRUE, 0);

  button = gtk_button_new_with_mnemonic("Leve_ls");
  g_signal_connect (G_OBJECT (button), "clicked", G_CALLBACK (levels_accept), NULL );
  gtk_box_pack_start (GTK_BOX (vbox), button, TRUE, TRUE, 0);

  button = gtk_button_new_with_mnemonic("Abou_t");
  g_signal_connect (button, "clicked",	G_CALLBACK (about_us), GTK_WINDOW(win));
  gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 1);

  button = gtk_button_new_from_stock (GTK_STOCK_CLOSE);
  g_signal_connect (button, "clicked", gtk_main_quit, NULL);
  gtk_box_pack_start (GTK_BOX (vbox), button, TRUE, TRUE, 0);

  radio1 = gtk_check_button_new_with_mnemonic("Sound _mute");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio1), FALSE);
  g_signal_connect (radio1, "toggled",  G_CALLBACK (toggle_snd_off), NULL);
  gtk_box_pack_start(GTK_BOX (vbox), radio1, TRUE, TRUE, 0);

   /*string _case1, _case2, _case3 ;
  /* Create a radio button /
   radio1 = gtk_radio_button_new_with_label_from_widget (NULL, "AC");

   _case1 = "1";
   g_signal_connect (radio1, "toggled",  G_CALLBACK (toggle_admin), (gpointer)&_case1[0]);
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


  /* Enter the main loop */
  gtk_widget_show_all (win);
  gtk_main ();

  //gst_element_set_state (pipeline, GST_STATE_NULL);
  //gst_object_unref (GST_OBJECT (pipeline));

  FinalizeDB();

  gst_element_set_state (playbin2, GST_STATE_NULL);
  gst_object_unref (GST_OBJECT (playbin2));
  //g_source_remove (bus_watch_id);

  libusb_close(hdev);
  //delete sel_case;
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
