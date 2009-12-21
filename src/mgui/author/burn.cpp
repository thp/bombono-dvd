//
// mgui/author/burn.cpp
// This file is part of Bombono DVD project.
//
// Copyright (c) 2009 Ilya Murav'jov
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
// 

#include <mgui/_pc_.h>

#include "burn.h"
#include "execute.h"

#include <mgui/timer.h>
#include <mgui/dialog.h>

#include <boost/regex.hpp>
#include <boost/lexical_cast.hpp>

void devices_clear_devicedata()
{
    Author::GetBD().DVDDevices().clear_items();
}

void devices_add_device(const gchar *device_name, const gchar * /*device_id*/,
                        const gchar *device_node, const gint capabilities)
{
    // требования:
    // - информация доступна об устройстве
    // - резаки DVD
    if( device_name && (capabilities & DC_WRITE_DVDR) )
    {
        //io::cout << "model = " << device_name << ", dev = " << device_node
        //         << ", capabilities = " << capabilities << ", device_id = " << device_id << io::endl;

        using namespace Author;
        BurnData& bd = GetBD();
        bd.brnArr.push_back(Burner(std::string(device_name), std::string(device_node)));
        bd.DVDDevices().append_text(device_name);
    }
}

namespace Author {

static void SetupSpeeds(Gtk::ComboBox& speed_btn);
static void UpdateSpeeds();
static void OnSpeedChange();

BurnData& GetInitedBD()
{
    BurnData& bd = GetBD();
    // чтобы сбросить старые обработчики
    ReRefPtr(bd.dvdDevices);
    ReRefPtr(bd.speedBtn);
    ReRefPtr(bd.label);

    devices_probe_busses();
    Gtk::ComboBoxText& dvd_btn = bd.DVDDevices();

    dvd_btn.signal_changed().connect(&UpdateSpeeds);
    SetupSpeeds(bd.SpeedBtn());
    bd.SpeedBtn().signal_changed().connect(&OnSpeedChange);

    Gtk::TreeModel::Children children = dvd_btn.get_model()->children();
    if( children.size() )
        dvd_btn.set_active(children.begin());

    return bd;
}

static void ConcatToStr(const char* buf, int sz, std::string& str)
{
    str += std::string(buf, sz);
}

boost::regex WriteSpeed_RE("Write Speed #"RG_NUM":"RG_SPS RG_NUM"\\."RG_NUM "x1385"); 

RefPtr<Gtk::ListStore> sp_store;
Gtk::TreeModelColumn<double>      dbl_cln;
Gtk::TreeModelColumn<std::string> str_cln;
const char* UpdateSpeedsEntry = "Update speeds ...";

bool SeparatorFunc(const RefPtr<Gtk::TreeModel>&, const Gtk::TreeIter& itr)
{
    return itr->get_value(str_cln) == "separator";
}

static void SetupSpeeds(Gtk::ComboBox& speed_btn)
{
    if( !sp_store )
    {
        Gtk::TreeModelColumnRecord columns;
        Gtk::TreeModelColumn<double>      dbl_cln_;
        dbl_cln = dbl_cln_;
        Gtk::TreeModelColumn<std::string> str_cln_;
        str_cln = str_cln_;
        columns.add(dbl_cln);
        columns.add(str_cln);

        sp_store = Gtk::ListStore::create(columns);
    }
    speed_btn.set_model(sp_store);
    speed_btn.set_row_separator_func(&SeparatorFunc);

    // * внешний вид
    speed_btn.pack_start(str_cln);
}

bool IsBurnerSetup(std::string& dev_path)
{
    BurnData& bd = GetBD();
    int pos = bd.DVDDevices().get_active_row_number();

    bool res = (pos != -1);
    if( res )
        dev_path = bd.brnArr[pos].devPath;
    return res;
}

double GetBurnerSpeed()
{
    Gtk::TreeIter itr = GetBD().SpeedBtn().get_active();
    ASSERT( itr );
    return itr->get_value(dbl_cln);
}

bool TestDvdDisc(const std::string& dev_path, std::string& str)
{
    ReadReadyFnr fnr = boost::lambda::bind(&ConcatToStr, boost::lambda::_1, boost::lambda::_2, boost::ref(str));
    ExitData ed = ExecuteAsync(0, ("dvd+rw-mediainfo " + dev_path).c_str(), fnr);

    return ed.IsGood();
}

static void UpdateSpeeds()
{
    std::string dev_path;
    if( IsBurnerSetup(dev_path) )
    {
        //io::cout << dev_path << io::endl;

        std::string str;
        typedef std::vector<double> SpeedsArray;
        SpeedsArray speeds;
        if( TestDvdDisc(dev_path, str) )
        {
            std::string::const_iterator start = str.begin(), end = str.end();
            boost::smatch what;
            while( boost::regex_search(start, end, what, WriteSpeed_RE) )
            {
                start = what[0].second;
                double speed = boost::lexical_cast<double>(what.str(2) + "." + what.str(3));
                speeds.push_back(speed);
            }
        }
        std::sort(speeds.begin(), speeds.end());

        //
        // заполнение списка скоростей
        //
        Gtk::ComboBox& speed_btn = GetBD().SpeedBtn();
        ASSERT( sp_store );

        sp_store->clear();
        // * заполняем
        Gtk::TreeRow row = *sp_store->append();
        row[str_cln] = "Auto";
        speed_btn.set_active(row);

        for( SpeedsArray::iterator itr = speeds.begin(), end = speeds.end(); itr != end; ++itr )
        {
            Gtk::TreeRow row = *sp_store->append();
            row[dbl_cln] = *itr;
            row[str_cln] = boost::lexical_cast<std::string>(*itr) + "\303\227";
        }
        row = *sp_store->append();
        row[str_cln] = "separator";
        row = *sp_store->append();
        row[str_cln] = UpdateSpeedsEntry;
    }
}

static bool PopupSpeeds()
{
    bool res = false;
    Gtk::ComboBox& btn = GetBD().SpeedBtn();
    if( !Glib::PropertyProxy_ReadOnly<bool>(&btn, "popup-shown") )
    {
        res = true;
        btn.popup();
    }
    
    return res;
}

static void OnSpeedChange()
{
    BurnData& bd = GetBD();
    Gtk::TreeIter itr = bd.SpeedBtn().get_active();
    if( itr->get_value(str_cln) == UpdateSpeedsEntry )
    {
        UpdateSpeeds();

        // :KLUDGE: сразу speedBtn.popup() не проходит, с нулевым периодом
        // по таймауту - тоже; выбрал 1/100 секунды :)
        Timer().Connect(&PopupSpeeds, 10);
    }
}

DVDInfo ParseDVDInfo(bool is_good, const std::string& out_info)
{
    std::string::const_iterator start = out_info.begin(), end = out_info.end();
    DVDInfo inf(dvdERROR);
    DVDType& res = inf.typ;
    if( !is_good )
    {
        static boost::regex cd_drive_only(":-\\( not a DVD unit");
        static boost::regex cd_disc(":-\\( non-DVD media mounted");
        static boost::regex empty_drive(":-\\( no media mounted");

        if( boost::regex_search(start, end, cd_drive_only) )
            res = dvdCD_DRIVE_ONLY;
        else if( boost::regex_search(start, end, cd_disc) )
            res = dvdCD_DISC;
        else if( boost::regex_search(start, end, empty_drive) )
            res = dvdEMPTY_DRIVE;
    }
    else
    {
        static boost::regex media_type_re("Mounted Media:"RG_SPS"[0-9A-F]+h, ([^ \n]+)");
        boost::smatch what;

        bool is_found = boost::regex_search(start, end, what, media_type_re);
        ASSERT_RTL( is_found );
        res = dvdOTHER;
        std::string name = inf.name = what.str(1);

        int len = name.size();
        if( len >= 5 ) // DVDxR
        {
            if( (strncmp(name.c_str(), "DVD-R", 4) == 0) || 
                (strncmp(name.c_str(), "DVD+R", 4) == 0) )
            {
                if( len == 5 )
                    res = dvdR;
                else if( len == 6 && name[5] == 'W' )
                    res = dvdRW;
            }
        }

        // isBlank
        if( res != dvdOTHER )
        {
            static boost::regex media_status_re("Disc status:"RG_SPS"([a-z]+)\n");
            bool is_found = boost::regex_search(start, end, what, media_status_re);
            ASSERT_RTL( is_found );

            std::string status = what.str(1);
            // б/у RW после форматирования получает статус "appendable"
            inf.isBlank = (status == "blank") || ((status == "appendable") && (res == dvdRW));
        }
    }
    return inf;
}

static bool PulseProgress()
{
    GetES().prgBar.pulse();
    return true;
}

struct WaitProgress
{
    WaitProgress(const std::string& msg) 
    { 
        ExecState& es = GetES();
        es.SetStatus(msg);
        // перевод в режим pulse
        es.prgBar.set_text(" ");
        es.prgBar.set_fraction(0.);

        tm.Connect(&PulseProgress, 500); 
    }
   ~WaitProgress() 
    {
        ExecState& es = GetES();
        es.SetIndicator(0);
        es.SetStatus();
         
        tm.Disconnect(); 
    }

    protected:
    Timer tm;
};

bool CheckDVDBlank()
{
    bool res = false;
    for( bool try_again = true; !res && try_again; )
    {
        std::string dvd_drive;
        bool is_burner_setup = Author::IsBurnerSetup(dvd_drive);
        ASSERT_RTL( is_burner_setup );
        std::string str;
        bool is_good = false;
        {
            WaitProgress wp("Checking Disc ..."); 
            is_good = TestDvdDisc(dvd_drive, str);
        }

        DVDInfo inf = ParseDVDInfo(is_good, str);
        switch( inf.typ )
        {
        case dvdERROR:
            try_again = Gtk::RESPONSE_YES == 
                MessageBox("Unknown error with selected burn drive found. Try again?", 
                           Gtk::MESSAGE_QUESTION, Gtk::BUTTONS_YES_NO, str);
            break;
        case dvdCD_DRIVE_ONLY:
            try_again = Gtk::RESPONSE_OK == 
                MessageBox("Selected burn drive is for CD discs only. Change to another burn drive.", 
                           Gtk::MESSAGE_WARNING, Gtk::BUTTONS_OK_CANCEL, "", true);
            break;
        case dvdCD_DISC:
            try_again = Gtk::RESPONSE_OK == 
                MessageBox("CD disc is found in the drive, not DVD. Change to DVD disc.", 
                           Gtk::MESSAGE_WARNING, Gtk::BUTTONS_OK_CANCEL, "", true);
            break;
        case dvdEMPTY_DRIVE:
            try_again = Gtk::RESPONSE_OK == 
                MessageBox("No DVD disc in the drive. Load a clear one and press OK.", 
                           Gtk::MESSAGE_INFO, Gtk::BUTTONS_OK_CANCEL, "", true);
            break;
        case dvdOTHER:
            try_again = Gtk::RESPONSE_OK == 
                MessageBox("Disc with type \"" + inf.name + "\" is found in the drive but "
                           "for DVD-Video disc type should be one from: DVD-R, DVD+R, DVD-RW, DVD+RW. "
                           "Load a clear one with right type and press OK.", 
                           Gtk::MESSAGE_INFO, Gtk::BUTTONS_OK_CANCEL, "", true);
            break;
        default:
            ASSERT( (inf.typ == dvdR) || (inf.typ == dvdRW) );
            if( inf.typ == dvdR && !inf.isBlank )
                try_again = Gtk::RESPONSE_OK == 
                    MessageBox("Disc with type \"" + inf.name + "\" in the drive is not clear. Only clear recordable "
                               "discs can be used for burning DVD-Video. Load a clear one and press OK.", 
                               Gtk::MESSAGE_INFO, Gtk::BUTTONS_OK_CANCEL, "", true);
            else
            {
                if( inf.typ == dvdRW && !inf.isBlank )
                {
                    std::string title = "Disc with type \"" + inf.name + "\" in the drive is not clear." 
                                        " We need to remove its contents before writing new one. Continue?";
                    Gtk::MessageDialog dlg(MakeMessageBoxTitle(title), true, Gtk::MESSAGE_INFO, Gtk::BUTTONS_NONE);

                    dlg.add_button("_Cancel",    Gtk::RESPONSE_CANCEL);
                    dlg.add_button("_Try again", Gtk::RESPONSE_REJECT);
                    dlg.add_button(Gtk::Stock::OK, Gtk::RESPONSE_OK);
                    dlg.set_default_response(Gtk::RESPONSE_OK);

                    Gtk::ResponseType resp = (Gtk::ResponseType)dlg.run();
                    switch( resp )
                    {
                    case Gtk::RESPONSE_OK:
                        res = true;
                        break;
                    default:
                        try_again = resp == Gtk::RESPONSE_REJECT;
                    }
                }
                else
                    res = true;
            }
        }
    }

    return res;
}

} // namespace Author
