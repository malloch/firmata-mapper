 /*  Firmata GUI-friendly libmapper interface adapted especially for the T-Stick
 *  Joseph Malloch (joseph.malloch@gmail.com)
 *  Julie René
 *  Adapted from firmata_test
 *  Copyright 2010, Paul Stoffregen (paul@pjrc.com)
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#if defined(__GNUG__) && !defined(NO_GCC_PRAGMA)
#pragma implementation "firmata_mapper.h"
#endif

#include "wx/wxprec.h"
#include "Firmapper_TStick.h"
#include "serial.h"
#include "mapper/mapper.h"
#include <iostream>
#include <fstream>
#include <string>


using namespace std;


//------------------------------------------------------------------------------
// MyFrame
//------------------------------------------------------------------------------

Serial port;
bool isProgramLoaded;
mapper_device dev = 0;
mapper_timetag_t tt;
int needs_update = 0;
typedef struct {
  uint8_t mode;
  uint8_t analog_channel;
  uint64_t supported_modes;
  uint32_t value;
  mapper_signal sig;
  string name; 
  string unit;
  int grid_row;//place of the signal in the interface grid
} pin_t;
string names[128];
pin_t pin_info[128];
wxString firmata_name;
unsigned int rx_count, tx_count;
wxMenu *port_menu;
wxMenu *file_menu;
wxMenu *signal_menu;
wxMenu *EEPROM_menu;
bool isPinChosed = false;
bool isConfigurationSaved = false;

#define MODE_INPUT    0x00
#define MODE_OUTPUT   0x01
#define MODE_ANALOG   0x02
#define MODE_PWM      0x03
#define MODE_SERVO    0x04
#define MODE_SHIFT    0x05
//#define MODE_I2C      0x06

#define START_SYSEX             0xF0 // start a MIDI Sysex message
#define END_SYSEX               0xF7 // end a MIDI Sysex message
#define PIN_MODE_QUERY          0x72 // ask for current and supported pin modes
#define PIN_MODE_RESPONSE       0x73 // reply with current and supported pin modes
#define PIN_STATE_QUERY         0x6D
#define PIN_STATE_RESPONSE      0x6E
#define CAPABILITY_QUERY        0x6B
#define CAPABILITY_RESPONSE     0x6C
#define ANALOG_MAPPING_QUERY    0x69
#define ANALOG_MAPPING_RESPONSE 0x6A
#define REPORT_FIRMWARE         0x79 // report name and version of the firmware
#define EEPROM_LOADING          0x0B
#define RECEIVE_NAME            0x0C
#define TSTICK_DATA             0x0D


#define SIZE_MAX_NAME           2
#define SIZE_MAX_UNIT           2

#define SAVE_FILE_ID            6323
#define LOAD_FILE_ID            6324
#define ADD_PIN_ID              6325
#define WRITE_EEPROM_ID         6326
#define CLEAR_EEPROM_ID         6327
#define LOAD_EEPROM_ID          6328
#define MODE_CHANGE             6329
#define MODE_TEMP_CHANGE        6330

#define PIN_FRAME_ID            6331
#define NAME_ID                 6332
#define UNIT_ID                 6333
#define MODE_ID                 6334
#define PIN_ID                  6335
#define WARNING_ID              6336

BEGIN_EVENT_TABLE(MyFrame,wxFrame)
        EVT_MENU(SAVE_FILE_ID, MyFrame::OnSaveFile)
        EVT_MENU(LOAD_FILE_ID, MyFrame::OnLoadFile)
        EVT_MENU(WRITE_EEPROM_ID, MyFrame::OnEEPROM) 
//EVT_MENU(CLEAR_EEPROM_ID, MyFrame::OnEEPROM) 
        EVT_MENU(LOAD_EEPROM_ID, MyFrame::OnEEPROM)
        EVT_MENU(ADD_PIN_ID, MyFrame::OnAddPin)
	EVT_MENU(wxID_ABOUT, MyFrame::OnAbout)
	EVT_MENU(wxID_EXIT, MyFrame::OnQuit)
	EVT_MENU_RANGE(9000, 9999, MyFrame::OnPort)
        EVT_CHOICE(-1, MyFrame::OnModeChange)
        EVT_CHOICE(MODE_TEMP_CHANGE, MyFrame::OnModeChange)
        EVT_IDLE(MyFrame::OnIdle)
        EVT_TOGGLEBUTTON(-1, MyFrame::OnToggleButton)
        EVT_BUTTON(-1, MyFrame::OnButton)
	EVT_SCROLL_THUMBTRACK(MyFrame::OnSliderDrag)
	EVT_MENU_OPEN(MyMenu::OnShowPortList)
	EVT_MENU_HIGHLIGHT(-1, MyMenu::OnHighlight)
	EVT_CLOSE(MyFrame::OnCloseWindow)
	//EVT_SIZE(MyFrame::OnSize)
END_EVENT_TABLE()


MyFrame::MyFrame( wxWindow *parent, wxWindowID id, const wxString &title,
    const wxPoint &position, const wxSize& size, long style ) :
    wxFrame( parent, id, title, position, size, style )
{
	#ifdef LOG_MSG_TO_WINDOW
	wxLog::SetActiveTarget(new wxLogWindow(this, _("Debug Messages")));
	#endif
	port.Set_baud(57600);
	wxMenuBar *menubar = new wxMenuBar;
	wxMenu *menu = new wxMenu;
	menu->Append( SAVE_FILE_ID, _("Save configuration"), _(""));
	menu->Enable( SAVE_FILE_ID, false);
	menu->Append( LOAD_FILE_ID, _("Load configuration"), _(""));
	menu->Enable( LOAD_FILE_ID, false);
	menu->Append( wxID_ABOUT, _("About"), _(""));
	menu->Append( wxID_EXIT, _("Quit"), _(""));
	menubar->Append(menu, _("File"));
	file_menu = menu;

	menu = new wxMenu;
	menubar->Append(menu, _("Port"));
	port_menu = menu;

	menu = new wxMenu;
	menu->Append(ADD_PIN_ID, _("Add a signal"));
	menu->Enable( ADD_PIN_ID, false);
	menubar->Append( menu, _("Signal manager"));
	signal_menu = menu;

	menu = new wxMenu;
	menu->Append( WRITE_EEPROM_ID, _("Write on EEPROM"));
	menu->Enable( WRITE_EEPROM_ID, false);
	menu->Append( LOAD_EEPROM_ID, _("Load EEPROM"));
	menu->Enable( LOAD_EEPROM_ID, false);
	//menu->Append( CLEAR_EEPROM_ID, _("Clear EEPROM"));
	//menu->Enable( CLEAR_EEPROM_ID, false);
	menubar->Append(menu, _("EEPROM"));
	EEPROM_menu = menu;
	
	SetMenuBar(menubar);
	CreateStatusBar(1);

	scroll = new wxScrolledWindow(this);
	scroll->SetScrollRate(20, 20);
	grid = new wxFlexGridSizer(0, 4, 4, 20); 
	scroll->SetSizer(grid);
	grid_count = 2;
	

	init_data();

#if 0
	// For testing only, add many controls so something
	// appears in the window without requiring any
	// actual communication...
	dev = mdev_new("Firmata", 9000, 0);

	pin_info[0].mode = MODE_INPUT;
	pin_info[0].analog_channel = 2;
	pin_info[0].supported_modes = 0;
	pin_info[0].value = 0;
	pin_info[0].name = "Test1";
	pin_info[0].grid_row = 2;
	pin_info[0].unit = "blah";
	pin_info[0].sig = 0;
	
	pin_info[1].mode = MODE_INPUT;
	pin_info[1].analog_channel = 3;
	pin_info[1].supported_modes = 0;
	pin_info[1].value = 0;
	pin_info[1].name = "Test2";
	pin_info[1].grid_row = 3;
	pin_info[1].unit = "blah";
	pin_info[1].sig = 0;
	
	pin_info[2].mode = MODE_INPUT;
	pin_info[2].analog_channel = 4;
	pin_info[2].supported_modes = 0;
	pin_info[2].value = 0;
	pin_info[2].name = "Test2";
	pin_info[2].grid_row = 4;
	pin_info[2].unit = "blah";
	pin_info[2].sig = 0;
	
	rebuild_grid(); 
#endif
}

void MyFrame::init_data(void)
{
  //grid->Clear(true);
  //	grid->SetRows(0);
  //	grid->SetCols(5);
	for (int i=0; i < 128; i++) {
 		pin_info[i].mode = 255;
		pin_info[i].analog_channel = 127;
		pin_info[i].supported_modes = 0;
		pin_info[i].value = 0;
		pin_info[i].sig = 0;
		pin_info[i].name = "";
		pin_info[i].grid_row = 0;
	}
	tx_count = rx_count = 0;
	firmata_name = _("");
	
	init_add_pin_frame();

	UpdateStatus();
	//new_size();
	rebuild_grid(); 
}

void MyFrame::init_add_pin_frame(void)
{
  	wxFrame *addPinFrame = new wxFrame(scroll, PIN_FRAME_ID, _("add pin"), wxPoint(500,100), wxDefaultSize, wxSTAY_ON_TOP);
	wxStaticText *warning = new wxStaticText(addPinFrame, WARNING_ID,  _(""), wxPoint(305, 50), wxSize(-1,-1), wxALIGN_CENTRE, _(""));
	wxTextCtrl *nameTextCtrl = new wxTextCtrl(addPinFrame, NAME_ID, _(""), wxPoint(200,50), wxSize(100, 25));
	wxTextCtrl *unitTextCtrl = new wxTextCtrl(addPinFrame, UNIT_ID, _(""), wxPoint(200,80), wxSize(100, 25));
	wxChoice *modesChoice = new wxChoice(addPinFrame, MODE_ID, wxPoint(200, 110), wxSize(-1, -1), NULL);
	wxChoice *pinsChoice = new wxChoice(addPinFrame, PIN_ID, wxPoint(200, 110), wxSize(-1, -1), NULL);
	wxStaticText *staticName = new wxStaticText(addPinFrame, -1, _("name : "), wxPoint(70, 50), \
						    wxSize(-1,-1), wxALIGN_CENTRE, _("staticText"));
	
	wxStaticText *staticUnit = new wxStaticText(addPinFrame, -1, _("unit : "), wxPoint(70, 80), \
						    wxSize(-1,-1), wxALIGN_CENTRE, _("staticText"));
	
	wxStaticText *staticMode = new wxStaticText(addPinFrame, -1, _("available Mode : "), \
						    wxPoint(70, 110), wxSize(-1,-1), wxALIGN_CENTRE, \
						    _("staticText"));
	
	wxStaticText *staticPin = new wxStaticText(addPinFrame, -1, _("available pin : "), \
						   wxPoint(70, 140), wxSize(-1,-1), wxALIGN_CENTRE, \
						   _("staticText"));    
	//management buttons
	wxButton *OKButton = new wxButton(addPinFrame, 7250, _(" OK "), wxPoint(100, 200), wxDefaultSize);
	wxButton *CancelButton = new wxButton(addPinFrame, 7251, _(" Cancel "), wxPoint(200, 200), \
					  wxDefaultSize);
	
	nameTextCtrl->SetMaxLength(SIZE_MAX_NAME-1);
	unitTextCtrl->SetMaxLength(SIZE_MAX_UNIT-1);
	addPinFrame->Show(false);

}

void MyFrame::new_size(void)
{
	grid->Layout();
	scroll->FitInside();
	Refresh();
}

void MyFrame::add_item_to_grid(int row, int col, wxWindow *item)
{
  
	int num_col = grid->GetCols();
	int num_row = grid->GetRows();
	if (num_row <= row) {
	  //printf("adding rows, row=%d, num_row=%d\n", row, num_row);
		grid->SetRows(row + 1);
		while (num_row < row + 1) {
		  //printf("  add %d static text\n", num_col);
			for (int i=0; i<num_col; i++) {
				grid->Add(new wxStaticText(scroll, -1, _("        ")));
			}
			num_row++;
		}
	}
	int index = row * num_col + col + 1;
	//printf("index = %d: ", index);
	wxSizerItem *existing = grid->GetItem(index);
	if (existing != NULL) {
		wxWindow *old = existing->GetWindow();
		if (old) {
			grid->Replace(old, item);
			old->Destroy();
			wxSizerItem *newitem = grid->GetItem(item);
			if (newitem) 
				newitem->SetFlag(wxALIGN_CENTER_VERTICAL);
		}
	} else {
	  printf("WARNING, using insert\n");
	  grid->Insert(index, item);
	}
}

void MyFrame::add_pin(int pin)
{	   
	if (pin_info[pin].grid_row == 0){
	  pin_info[pin].grid_row = grid_count;
	  grid_count++;
	}

	//name
	wxString *str = new wxString();
	*str = wxString::FromAscii(names[pin].c_str());
	wxStaticText *wxName = new wxStaticText				\
	  (scroll, 15000+pin, *str , wxDefaultPosition, wxSize(-1,-1), wxALIGN_CENTER, _("staticText"));
	add_item_to_grid(pin_info[pin].grid_row, 0, wxName);			
	pin_info[pin].name = names[pin];

	//mode 
	wxStaticText *modes = new wxStaticText(scroll, -1, _("") , wxDefaultPosition, wxSize(-1,-1));
	if (pin_info[pin].mode == MODE_INPUT) 	  
	  modes->SetLabel(_("Input"));
	if (pin_info[pin].mode == MODE_OUTPUT)
	  modes->SetLabel(_("Output"));
	if (pin_info[pin].mode == MODE_ANALOG)
	  modes->SetLabel(_("Analog"));
	if (pin_info[pin].mode == MODE_PWM) 
	  modes->SetLabel(_("PWM"));
	if (pin_info[pin].mode == MODE_SERVO) 
	  modes->SetLabel(_("Servo"));

	add_item_to_grid(pin_info[pin].grid_row, 1, modes); 
	
	
	// create the 3rd column control for this mode
	if (pin_info[pin].mode == MODE_OUTPUT) {
	  wxToggleButton *button = new  wxToggleButton(scroll, 7000+pin, 
						       pin_info[pin].value ? _("High") : _("Low"));
	  button->SetValue(pin_info[pin].value);
	  add_item_to_grid(pin_info[pin].grid_row, 2, button);
	} else if (pin_info[pin].mode == MODE_INPUT) {
	  wxStaticText *text = new wxStaticText(scroll, 5000+pin,
						pin_info[pin].value ? _("High") : _("Low"));
	  wxSize size = wxSize(128, -1);
	  text->SetMinSize(size);
	  text->SetWindowStyle(wxALIGN_CENTRE);
	  add_item_to_grid(pin_info[pin].grid_row, 2, text);
	} else if (pin_info[pin].mode == MODE_ANALOG) {
	  wxString val;
	  val.Printf(_("%d"), pin_info[pin].value);
	  wxStaticText *text = new wxStaticText(scroll, 5000+pin, val);
	  wxSize size = wxSize(128, -1);
	  text->SetMinSize(size);
	  text->SetWindowStyle(wxALIGN_CENTRE);
	  add_item_to_grid(pin_info[pin].grid_row, 2, text);
	} else if (pin_info[pin].mode == MODE_PWM || pin_info[pin].mode == MODE_SERVO) {
	  int maxval = (pin_info[pin].mode == MODE_PWM) ? 255 : 180;
	  wxSlider *slider = new wxSlider(scroll, 6000+pin,
					  pin_info[pin].value, 0, maxval);
	  wxSize size = wxSize(128, -1);
	  slider->SetMinSize(size);
	  add_item_to_grid(pin_info[pin].grid_row, 2, slider);
	}
	
	//delete button
	wxButton *deleteButton = new wxButton(scroll, 7500+pin, _("delete"));
	add_item_to_grid(pin_info[pin].grid_row, 3, deleteButton);
	
	new_size();

	//create libmapper signal
	create_signal(pin);

}

void MyFrame::create_signal(int pin){
  //send the mode to the firmware
    uint8_t buf[SIZE_MAX_NAME+3];
    buf[0] = 0xF4;
    buf[1] = pin;
    buf[2] = pin_info[pin].mode;    
    port.Write(buf, 3);
    tx_count += 3;  
    pin_info[pin].value = 0;

    //to delete the corresponding signal if it already exist
    if (pin_info[pin].sig) {
      mapper_db_signal props = msig_properties(pin_info[pin].sig);
      if (props->is_output)
	mdev_remove_output(dev, pin_info[pin].sig);
      else 
	mdev_remove_input(dev, pin_info[pin].sig);
    }

    //name mangement
    char signame[SIZE_MAX_NAME];
    for (int i=0; i<SIZE_MAX_NAME; i++){
      signame[i]=(pin_info[pin].name)[i];
    }
    char sigunit[SIZE_MAX_UNIT];
    for (int i=0; i<SIZE_MAX_UNIT; i++)
      sigunit[i]=(pin_info[pin].unit)[i];
    
    //to create the new signal with the right mode
    int min = 0, max;
    switch (pin_info[pin].mode) {
    case MODE_INPUT:
      max = 1;
      pin_info[pin].sig = mdev_add_output(dev, signame, 1, 'i', sigunit, &min, &max);
      break;
    case MODE_OUTPUT:
      max = 1;
      pin_info[pin].sig = mdev_add_input(dev, signame, 1, 'i', sigunit, &min, &max,
			 		 MapperSignalHandler, (void *)pin);
      break;
    case MODE_ANALOG:
      max = 1023;
      pin_info[pin].sig = mdev_add_output(dev, signame, 1, 'i',sigunit, &min, &max);
      break;
    case MODE_PWM:
      max = 255;
      pin_info[pin].sig = mdev_add_input(dev, signame, 1, 'i', sigunit, &min, &max,
					 MapperSignalHandler, (void *)pin);
      break;
    case MODE_SERVO:
      max = 180;
      pin_info[pin].sig = mdev_add_input(dev, signame, 1, 'i', sigunit, &min, &max,
					 MapperSignalHandler, (void *)pin);
    default:
      break;
    }
}

//delete the pin from the interface and destroy its signal
void MyFrame::delete_pin(int pin)
{
     mapper_db_signal props = msig_properties(pin_info[pin].sig);
     if (props->is_output)
       mdev_remove_output(dev, pin_info[pin].sig);
     else 
       mdev_remove_input(dev, pin_info[pin].sig);

     //reinit the pin
     pin_info[pin].mode = 255;
     pin_info[pin].value = 0;
     pin_info[pin].sig = 0;
     pin_info[pin].name = "";
     
     // decrement grid row for all pins below it
     for (int i=0; i<128; i++) {
       if (pin_info[i].grid_row > pin_info[pin].grid_row)
	 pin_info[i].grid_row --;
     } 
     pin_info[pin].grid_row = 0;
     grid_count--;
     rebuild_grid(); 
}

void MyFrame::rebuild_grid()
{
  grid->Clear(true);
  grid->SetRows(0);
  grid->SetCols(5);
  
  for (int i = 0; i<128; i++) {
  if (pin_info[i].grid_row > 0) {
    names[i] = pin_info[i].name;
    add_pin(i);
  }
 }
 
  new_size();
} 

//convert a string from std to wx
wxString std2wx(std::string s){
  wxString wx;
  const char* my_string=s.c_str();
  wxMBConvUTF8 *wxconv= new wxMBConvUTF8();
  wx=wxString(wxconv->cMB2WC(my_string),wxConvUTF8);
  delete wxconv;
  // test if conversion works of not. In case it fails convert from Ascii
  if(wx.length()==0)
    wx=wxString(wxString::FromAscii(s.c_str()));
 return wx;
} 

void MyFrame::UpdateStatus(void)
{
  wxString status;
  if (port.Is_open()/* && isProgramLoaded*/)
    status.Printf(port.get_name() + _("    Tx:%u Rx:%u     ") + firmata_name, tx_count, rx_count );
  //else if (port.Is_open() && !isProgramLoaded)
    //TODO: find a way to define if an operational firmware is loaded on the Arduino
    //status = _("Firmata program not loaded on the arduino");
  else 
    status = _("Please choose serial port");
  SetStatusText(status);
}	

void MyFrame::OnModeChange(wxCommandEvent &event)
{
  if(event.GetId()==MODE_ID){ //when a mode is temporarly changed on the "add pin" frame 
    
    wxFrame *addPinFrame = (wxFrame *) FindWindowById(PIN_FRAME_ID, NULL);
    wxChoice *modesChoice = (wxChoice *) FindWindowById(MODE_ID, NULL);    
    wxChoice *pinsChoice = (wxChoice *) FindWindowById(PIN_ID, NULL);

    string modeSelected =  wx2std(modesChoice->GetStringSelection());
    
    //pin
    wxArrayString pinList;
    if (modeSelected == "Input"){
      for (int i = 0; i< 128; i++)
	if (pin_info[i].supported_modes & (1<<MODE_INPUT) && pin_info[i].sig==0) 
	  pinList.Add(wxString(wxString::Format(wxT("%d"), i))); 
    }else if (modeSelected == "Output"){
      for (int i = 0; i< 128; i++) 
	if (pin_info[i].supported_modes & (1<<MODE_OUTPUT) && pin_info[i].sig==0)
	  pinList.Add(wxString(wxString::Format(wxT("%d"), i)));
    }else if (modeSelected == "Analog") {
      
      for (int i = 0; i< 128; i++)
	if (pin_info[i].supported_modes & (1<<MODE_ANALOG) && pin_info[i].sig==0 )
	  pinList.Add(wxString(wxString::Format(wxT("%d"), i)));
    }else if (modeSelected == "Servo") {
      for (int i = 0; i< 128; i++)
	if (pin_info[i].supported_modes & (1<<MODE_SERVO) && pin_info[i].sig==0)
	  pinList.Add(wxString(wxString::Format(wxT("%d"), i)));
    } else if (modeSelected == "PWM"){
      for (int i = 0; i< 128; i++)
	if (pin_info[i].supported_modes & (1<<MODE_PWM) && pin_info[i].sig==0)
	  pinList.Add(wxString(wxString::Format(wxT("%d"), i)));
    }

    if (pinsChoice != NULL)
      pinsChoice->Destroy();
    pinsChoice = new wxChoice(addPinFrame, PIN_ID, wxPoint(200, 140),  wxSize(-1, -1), pinList);
  }
}

void MyFrame::MapperSignalHandler(mapper_signal msig, mapper_db_signal props,
                                  int instance_id, void *value, int count,
                                  mapper_timetag_t *time)
{
    int pin = (size_t)props->user_data;
    if (pin < 0 || pin > 127) return;
    if (!pin_info[pin].sig) return;
    if (!value) return;
    int *i = (int *)value;
    int val = i[0];
    //int id;
    uint8_t buf[3];
    switch (pin_info[pin].mode) {
        case MODE_OUTPUT:
        {
	    val = val ? 1 : 0;
            //id = pin + 7000;
            //wxToggleButton *button = (wxToggleButton *)FindWindowById(id, scroll);
            //button->SetValue(val);
            pin_info[pin].value = val;
            int port_num = pin / 8;
            int port_val = 0;
            for (int i=0; i<8; i++) {
                int p = port_num * 8 + i;
                if (pin_info[p].mode == MODE_OUTPUT || pin_info[p].mode == MODE_INPUT) {
                    if (pin_info[p].value) {
                        port_val |= (1<<i);
                    }
                }
            }
            buf[0] = 0x90 | port_num;
            buf[1] = port_val & 0x7F;
            buf[2] = (port_val >> 7) & 0x7F;
            port.Write(buf, 3);
            tx_count += 3;
            //UpdateStatus();
            needs_update = 1;
            break;
        }
        case MODE_PWM:
	  if (val > 255)
                val = 255;
            //id = pin + 6000;
            //wxSlider *slider = (wxSlider *)FindWindowById(id, scroll);
            //slider->SetValue(val);
            pin_info[pin].value = val;
            buf[0] = 0xE0 | pin;
            buf[1] = val & 0x7F;
            buf[2] = (val >> 7) & 0x7F;
            port.Write(buf, 3);
            tx_count += 3;
            //UpdateStatus();
            needs_update = 1;
            break;
        case MODE_SERVO:
            if (val > 180)
                val = 180;
            //id = pin + 6000;
            //wxSlider *slider = (wxSlider *)FindWindowById(id, scroll);
            //slider->SetValue(val);
            pin_info[pin].value = val;
            buf[0] = 0xE0 | pin;
            buf[1] = val & 0x7F;
            buf[2] = (val >> 7) & 0x7F;
            port.Write(buf, 3);
            tx_count += 3;
            //UpdateStatus();
            needs_update = 1;
            break;
        default:
            break;
    }
}

void MyFrame::OnToggleButton(wxCommandEvent &event)
{
	int id = event.GetId();
	int pin = id - 7000;
	if (pin < 0 || pin > 127) return;
	wxToggleButton *button = (wxToggleButton *)FindWindowById(id, scroll);
	int val = button->GetValue() ? 1 : 0;
	//printf("Toggle Button, id = %d, pin=%d, val=%d\n", id, pin, val);
	button->SetLabel(val ? _("High") : _("Low"));
	pin_info[pin].value = val;
	int port_num = pin / 8;
	int port_val = 0;
	for (int i=0; i<8; i++) {
		int p = port_num * 8 + i;
		if (pin_info[p].mode == MODE_OUTPUT || pin_info[p].mode == MODE_INPUT) {
			if (pin_info[p].value) {
				port_val |= (1<<i);
			}
		}
	}
	uint8_t buf[3];
	buf[0] = 0x90 | port_num;
	buf[1] = port_val & 0x7F;
	buf[2] = (port_val >> 7) & 0x7F;
	port.Write(buf, 3);
	tx_count += 3;
    if (pin_info[pin].sig)
        msig_update_int(pin_info[pin].sig, val);
	UpdateStatus();
}

//send the EEPROM orders to the firmware
void MyFrame::OnEEPROM(wxCommandEvent &event)
{ 
  //A very bad solution to the problem of not loading the first time
  //TODO: fix the protocol problem in the right way
  for (int i = 0; i<=1; i++){
    uint8_t buf[2];
    if (event.GetId() == WRITE_EEPROM_ID)
      for (int i = 0; i <128; i++){
	int pinTemp = searchPinByCreatedOrder(i);
	if ( pinTemp != -1)
	  sendName(pinTemp);
	else
	  continue;
      }
    
    buf[0]=0x09; 
    if (event.GetId() == WRITE_EEPROM_ID)
      buf[1]=0;
    /*else if (event.GetId() == CLEAR_EEPROM_ID)
      buf[1]=3;//conflict with 1 (don't find why)*/
    else if (event.GetId() == LOAD_EEPROM_ID){
      buf[1]=2;
    }
    port.Write(buf, 2);
    tx_count += 2;
  }
}

//Create a window to add a pin
void MyFrame::OnAddPin(wxCommandEvent &event)
{
  //check if at least one pin is available
  bool isAPinFree = false;
  for (int i=0; i<128; i++)
    if (pin_info[i].sig == 0 && (pin_info[i].supported_modes & (1<<MODE_INPUT) ||\
				 pin_info[i].supported_modes & (1<<MODE_OUTPUT) ||\
				 pin_info[i].supported_modes & (1<<MODE_ANALOG) ||\
				 pin_info[i].supported_modes & (1<<MODE_PWM) ||\
				 pin_info[i].supported_modes & (1<<MODE_SERVO)))
      //TODO: find a better way to do this ?
      isAPinFree = true;

  
  if (isAPinFree){
    this->Disable();
    
    wxFrame *addPinFrame = (wxFrame*)FindWindowById(PIN_FRAME_ID, scroll);
    wxTextCtrl *nameTextCtrl = (wxTextCtrl*) FindWindowById(NAME_ID, NULL);
    nameTextCtrl->SetValue(_(""));
    wxTextCtrl *unitTextCtrl = (wxTextCtrl*) FindWindowById(UNIT_ID, NULL);
    unitTextCtrl->SetValue(_(""));
    
    //list of available mode
    wxArrayString modeList;
    bool isInputDispo = false;
    bool isOutputDispo = false;
    bool isAnalogDispo = false;
    bool isServoDispo = false;
    bool isPWMDispo = false;
    
    //check if each mode is available on at least one pin
    for (int i=0; i< 128; i++){  
      if (pin_info[i].supported_modes & (1<<MODE_INPUT) && pin_info[i].sig==0)
	isInputDispo = true;
      if (pin_info[i].supported_modes & (1<<MODE_OUTPUT) && pin_info[i].sig==0)
 	isOutputDispo = true;
      if (pin_info[i].supported_modes & (1<<MODE_ANALOG) && pin_info[i].sig==0) 
	isAnalogDispo = true;
      if (pin_info[i].supported_modes & (1<<MODE_SERVO) && pin_info[i].sig==0) 
	isServoDispo = true;
      if (pin_info[i].supported_modes & (1<<MODE_PWM) && pin_info[i].sig==0)
	isPWMDispo = true;
    }
    if (isInputDispo)
      modeList.Add(_("Input"));
    if (isOutputDispo)
      modeList.Add(_("Output"));
    if (isAnalogDispo)
      modeList.Add(_("Analog"));
    if (isServoDispo)
      modeList.Add(_("Servo"));
    if (isPWMDispo)
      modeList.Add(_("PWM"));
    
    wxChoice *modesChoice = (wxChoice*) FindWindowById(MODE_ID, NULL);
    if (modesChoice !=NULL)
      modesChoice->Destroy();
    modesChoice = new wxChoice(addPinFrame, MODE_ID, wxPoint(200, 110), wxSize(-1, -1), modeList);
    modesChoice->SetStringSelection(_("Analog"));
    
    modesChoice->Validate();
    wxCommandEvent cmd = wxCommandEvent(wxEVT_COMMAND_CHOICE_SELECTED, MODE_TEMP_CHANGE);
    modesChoice->Command(cmd);
    OnModeChange(cmd);  
    string modeSelected =  wx2std(modesChoice->GetStringSelection());
    
    //list of available pins 
    wxArrayString pinList;
    if (modeSelected == "Input")
      for (int i = 0; i< 128; i++){
	if (pin_info[i].supported_modes & (1<<MODE_INPUT) && !pin_info[i].sig) 
	  pinList.Add(wxString(wxString::Format(wxT("%d"), i)));
      } 
    else if (modeSelected == "Output")
      for (int i = 0; i< 128; i++){ 
	if (pin_info[i].supported_modes & (1<<MODE_OUTPUT) && !pin_info[i].sig)
	pinList.Add(wxString(wxString::Format(wxT("%d"), i)));
      }
    else if (modeSelected == "Analog")
      for (int i = 0; i< 128; i++){
	if (pin_info[i].supported_modes & (1<<MODE_ANALOG) && !pin_info[i].sig )
	  pinList.Add(wxString(wxString::Format(wxT("%d"), i)));
      }
    else if (modeSelected == "Servo")
      for (int i = 0; i< 128; i++){
	if (pin_info[i].supported_modes & (1<<MODE_SERVO) && !pin_info[i].sig )
	  pinList.Add(wxString(wxString::Format(wxT("%d"), i)));
      }
    else if (modeSelected == "PWM")
      for (int i = 0; i< 128; i++){
	if (pin_info[i].supported_modes & (1<<MODE_PWM) && !pin_info[i].sig)
	  pinList.Add(wxString(wxString::Format(wxT("%d"), i)));
      }

    wxChoice *pinsChoice = (wxChoice*) FindWindowById(PIN_ID, NULL);
    if (pinsChoice !=NULL)
      pinsChoice->Destroy();
    pinsChoice = new wxChoice(addPinFrame, PIN_ID, wxPoint(200, 140),  wxSize(-1, -1), pinList);
    
    wxStaticText *warning = (wxStaticText*)FindWindowById(WARNING_ID, addPinFrame);
    warning->SetLabel( _(""));
     
        
    addPinFrame->Show(true);
  } else {
    
  wxMessageDialog dialog( this, _("No pin available"), wxT("Warning"), wxOK|wxICON_ERROR );
  dialog.ShowModal();
  }
}

void MyFrame::OnButton(wxCommandEvent &event)
{
  wxFrame *addPinFrame = (wxFrame *)FindWindowById(PIN_FRAME_ID, NULL);

  if (event.GetId()==7251){ // cancel button
    addPinFrame->Show(false);
    this->Enable();
  }
  if (event.GetId()==7250){ // ok button
    bool isAFreeName = true;
    wxTextCtrl *nameTextCtrl = (wxTextCtrl *)FindWindowById(NAME_ID, NULL);
    wxTextCtrl *unitTextCtrl = (wxTextCtrl *)FindWindowById(UNIT_ID, NULL);
    wxChoice *modesChoice = (wxChoice *) FindWindowById(MODE_ID, NULL);
    wxChoice *pinsChoice = (wxChoice *) FindWindowById(PIN_ID, NULL);
    for (int i = 0; i < 128; i++)
      if (pin_info[i].grid_row!=0 && pin_info[i].name == wx2std(nameTextCtrl->GetValue()))
	
	//check if the two signals are both input or output 
	//( an input and an output can have the same name, but not two inputs or two outputs)
	if (((pin_info[i].mode == MODE_INPUT				\
	      || pin_info[i].mode == MODE_ANALOG)			\
	     && (wx2std(modesChoice->GetStringSelection()) == "Input"	\
		 || wx2std(modesChoice->GetStringSelection())=="Analog")) \
	    || ((pin_info[i].mode == MODE_SERVO				\
		 || pin_info[i].mode == MODE_OUTPUT			\
		 || pin_info[i].mode ==MODE_PWM)			\
		&& ( wx2std(modesChoice->GetStringSelection())=="Servo"	\
		     || wx2std(modesChoice->GetStringSelection())=="PWM" \
		     || wx2std(modesChoice->GetStringSelection())=="Output"))){
	  isAFreeName = false;
	}
    
    if (wx2std(nameTextCtrl->GetValue())!="" && isAFreeName){
      isPinChosed = true;
      addPinFrame->Show(false);
      this->Enable();

      int pin = atoi((wx2std(pinsChoice->GetStringSelection())).c_str());
 
      string mode = wx2std(modesChoice->GetStringSelection());
      if (mode =="Input")
      pin_info[pin].mode = MODE_INPUT;
      else if (mode == "Output")
	pin_info[pin].mode = MODE_OUTPUT;
      else if (mode == "Analog")
	pin_info[pin].mode = MODE_ANALOG;
      else if (mode == "Servo")
	pin_info[pin].mode = MODE_SERVO;
      else if (mode == "PWM")
	pin_info[pin].mode = MODE_PWM;
      
      pin_info[pin].name = wx2std(nameTextCtrl->GetValue());
      names[pin] =  pin_info[pin].name;

      pin_info[pin].unit = wx2std(unitTextCtrl->GetValue());
  
      add_pin(pin);
      
      isConfigurationSaved = false;
    }else { 
      
      wxStaticText *warning = (wxStaticText*)FindWindowById(WARNING_ID, addPinFrame);

      if (!isAFreeName)
	warning->SetLabel( _("already exist"));
      if (wx2std(nameTextCtrl->GetValue())=="")
	warning->SetLabel(_("enter a name"));
    }
  } else { //delete button
    int id = event.GetId();
    int pin = id - 7500;
    if (pin < 0 || pin > 127) return;
    delete_pin(pin);
  }
}
//To send names to the firmware
void MyFrame::sendName(int pin)
{
  if (pin < 0 || pin > 127) return;
  
  uint8_t buf[SIZE_MAX_NAME+SIZE_MAX_UNIT+2];
  for (int i = 0; i<SIZE_MAX_NAME+2;i++){
    buf[i] = 0;
  }
  buf[0] = 0x08;
  buf[1] = pin;
  char signame[SIZE_MAX_NAME];
  for (int i=0; i<SIZE_MAX_NAME; i++){
    signame[i]='\0';
  }
  char sigunit[SIZE_MAX_NAME];
  for (int i=0; i<SIZE_MAX_UNIT; i++){
    sigunit[i]='\0';
  }
  
  //TODO: make it in one step 
  for (int i=0; i<(int)(pin_info[pin].name).length(); i++)
    signame[i]=(pin_info[pin].name)[i]; 
  for (int i=0; i<(int)(pin_info[pin].unit).length(); i++)
    sigunit[i]=(pin_info[pin].unit)[i];
  
  for (int i = 0; i < SIZE_MAX_NAME ; i++)
    buf[i+2] = (uint8_t)signame[i];
  for (int i = 0; i < SIZE_MAX_UNIT ; i++)
    buf[i+2+SIZE_MAX_NAME] = (uint8_t)sigunit[i];
  
  port.Write(buf, SIZE_MAX_NAME+SIZE_MAX_UNIT+2);
  tx_count += SIZE_MAX_NAME+SIZE_MAX_UNIT+2; 
}

//convert a string from wx to std
//TODO: gate together the two conversion functions
std::string MyFrame::wx2std(wxString s){
  std::string s2;
  if(s.wxString::IsAscii()) {
    s2=s.wxString::ToAscii();
  } else {
    const wxWX2MBbuf tmp_buf = wxConvCurrent->cWX2MB(s);
    const char *tmp_str = (const char*) tmp_buf;
    s2=std::string(tmp_str, strlen(tmp_str));
  }
  return s2;
} 

void MyFrame::OnSliderDrag(wxScrollEvent &event)
{
	int id = event.GetId();
	int pin = id - 6000;
	if (pin < 0 || pin > 127) return;
	wxSlider *slider = (wxSlider *)FindWindowById(id, scroll);
	int val = slider->GetValue();
	printf("Slider Drag, id = %d, pin=%d, val=%d\n", id, pin, val);
	if (pin <= 15 && val <= 16383) {
		uint8_t buf[3];
		buf[0] = 0xE0 | pin;
		buf[1] = val & 0x7F;
		buf[2] = (val >> 7) & 0x7F;
		port.Write(buf, 3);
		tx_count += 3;
	} else {
		uint8_t buf[12];
		int len=4;
		buf[0] = 0xF0;
		buf[1] = 0x6F;
		buf[2] = pin;
		buf[3] = val & 0x7F;
		if (val > 0x00000080) buf[len++] = (val >> 7) & 0x7F;
		if (val > 0x00004000) buf[len++] = (val >> 14) & 0x7F;
		if (val > 0x00200000) buf[len++] = (val >> 21) & 0x7F;
		if (val > 0x10000000) buf[len++] = (val >> 28) & 0x7F;
		buf[len++] = 0xF7;
		port.Write(buf, len);
		tx_count += len;
	}
	if (pin_info[pin].sig)
	  msig_update_int(pin_info[pin].sig, val);
	UpdateStatus();
}

void MyFrame::OnPort(wxCommandEvent &event)
{
  int id = event.GetId();
  wxString name = port_menu->FindItem(id)->GetLabel();
  
  if (dev)
    mdev_free(dev);
  grid_count = 2;
  dev = 0;

  //disable menu options which can't work without a microcontroller plugged
  file_menu->Enable( LOAD_FILE_ID, false);
  file_menu->Enable( SAVE_FILE_ID, false);
  EEPROM_menu->Enable( WRITE_EEPROM_ID, false);
  EEPROM_menu->Enable( LOAD_EEPROM_ID, false);
  //EEPROM_menu->Enable( CLEAR_EEPROM_ID, false);
  signal_menu->Enable( ADD_PIN_ID, false);
  
  port.Close();
  init_data();
  //printf("OnPort, id = %d, name = %s\n", id, (const char *)name);
  if (id == 9000) return;
  
  port.Open(name);
  port.Set_baud(57600);
  if (port.Is_open()) {
    //printf("port is open\n");
    firmata_name = _("");
    rx_count = tx_count = 0;
    parse_count = 0;
    parse_command_len = 0;
    UpdateStatus();
    
    //enable menu options
    file_menu->Enable( LOAD_FILE_ID, true);
    file_menu->Enable( SAVE_FILE_ID, true);
    EEPROM_menu->Enable( WRITE_EEPROM_ID, true);
    EEPROM_menu->Enable( LOAD_EEPROM_ID, true);
    //EEPROM_menu->Enable( CLEAR_EEPROM_ID, true);
    signal_menu->Enable( ADD_PIN_ID, true);
 
    /* 
       The startup strategy is to open the port and immediately
       send the REPORT_FIRMWARE message.  When we receive the
       firmware name reply, then we know the board is ready to
       communicate.
       
       For boards like Arduino which use DTR to reset, they may
       reboot the moment the port opens.  They will not hear this
       REPORT_FIRMWARE message, but when they finish booting up
       they will send the firmware message.
       
       For boards that do not reboot when the port opens, they
       will hear this REPORT_FIRMWARE request and send the
       response.  If this REPORT_FIRMWARE request isn't sent,
       these boards will not automatically send this info.
       
       Arduino boards that reboot on DTR will act like a board
       that does not reboot, if DTR is not raised when the
       port opens.  This program attempts to avoid raising
       DTR on windows.  (is this possible on Linux and Mac OS-X?)
       
       Either way, when we hear the REPORT_FIRMWARE reply, we
       know the board is alive and ready to communicate.
    */
    uint8_t buf[3];
    buf[0] = START_SYSEX;
    buf[1] = REPORT_FIRMWARE; // read firmata name & version
    buf[2] = END_SYSEX;
    port.Write(buf, 3);
    tx_count += 3; 
    wxWakeUpIdle();
    dev = mdev_new("Firmata", 9000, 0);
  } else {
    printf("error opening port\n");
  }
  UpdateStatus();
}

void MyFrame::OnIdle(wxIdleEvent &event)
{
    uint8_t buf[1024];
    int r;

    if (dev)
      mdev_poll(dev, 0);
    if (needs_update)
      UpdateStatus();
    r = port.Input_wait(40);
    if (r > 0) {
      r = port.Read(buf, sizeof(buf));
      if (r < 0) {
	// error
	return;
      }
      if (r > 0) {
	// parse
	rx_count += r;
	for (int i=0; i < r; i++) {
	  //printf("%02X ", buf[i]);
	}
	//printf("\n");
	Parse(buf, r);
	UpdateStatus();
      }
    } else if (r < 0) {
      return;
    }
    event.RequestMore(true);
}

void MyFrame::Parse(const uint8_t *buf, int len)
{
  const uint8_t *p, *end;
  
  p = buf;
  end = p + len;

  mdev_timetag_now(dev, &tt);
  mdev_start_queue(dev, tt);
  for (p = buf; p < end; p++) {
    uint8_t msn = *p & 0xF0;
    if (msn == 0xE0 || msn == 0x90 || *p == 0xF9) {
      isProgramLoaded = true;
      UpdateStatus();
      parse_command_len = 3;
      parse_count = 0;
    } else if (msn == 0xC0 || msn == 0xD0) {
      parse_command_len = 2;
      parse_count = 0;
    } else if (*p == START_SYSEX) {
      parse_count = 0;
      parse_command_len = sizeof(parse_buf);
    } else if (*p == END_SYSEX) {
      parse_command_len = parse_count + 1;
    } else if ((*p & 0x80) && (parse_count==0)) { // conflict with the 128 of the capacitive touch sensor
      parse_command_len = 1;
      parse_count = 0;
    } else if (*p == EEPROM_LOADING) {
      parse_command_len = SIZE_MAX_NAME+SIZE_MAX_UNIT+3; //command + pin + name + mode 
      parse_count = 0;
    } else if (*p == RECEIVE_NAME){
      parse_command_len = SIZE_MAX_NAME-1+2;
      parse_count = 0;
    } else if (*p == TSTICK_DATA){
      parse_command_len = 2+6;
      parse_count = 0;
    }
    if (parse_count < (int)sizeof(parse_buf))
      parse_buf[parse_count++] = *p;
    if (parse_count == parse_command_len) {
      DoMessage();
      parse_count = parse_command_len = 0;
    }
  }
  mdev_send_queue(dev, tt);
}

void MyFrame::DoMessage(void)
{
  uint8_t cmd = (parse_buf[0] & 0xF0);
  //printf("message, %d bytes, %02X\n", parse_count, parse_buf[0]);
  if (cmd == 0xE0 && parse_count == 3) {
    int analog_ch = (parse_buf[0] & 0x0F);
    int analog_val = parse_buf[1] | (parse_buf[2] << 7);
    for (int pin=0; pin<128; pin++) {
      if (pin_info[pin].analog_channel == analog_ch) {
	pin_info[pin].value = analog_val;
	if (pin_info[pin].sig)
	  msig_update(pin_info[pin].sig, &analog_val, 1, tt);
	//printf("pin %d is A%d = %d\n", pin, analog_ch, analog_val);
	wxStaticText *text = (wxStaticText *)FindWindowById(5000 + pin, scroll);
	if (text) {
	  wxString val;
	  val.Printf(_("A%d: %d"), analog_ch, analog_val);
	  text->SetLabel(val);
	}
	return;
      }
    }
    return;
  }
  if (cmd == 0x90 && parse_count == 3) {
    int port_num = (parse_buf[0] & 0x0F);
    int port_val = parse_buf[1] | (parse_buf[2] << 7);
    int pin = port_num * 8;
    //printf("port_num = %d, port_val = %d\n", port_num, port_val);
    for (int mask=1; mask & 0xFF; mask <<= 1, pin++) {
      if (pin_info[pin].mode == MODE_INPUT) {
	int val = (port_val & mask) ? 1 : 0;
	if (pin_info[pin].sig)
	  msig_update(pin_info[pin].sig, &val, 1, tt);
	if (pin_info[pin].value != (uint32_t)val) {
	  //printf("pin %d is %d\n", pin, val);
	  wxStaticText *text = (wxStaticText *)
	    FindWindowById(5000 + pin, scroll);
	  if (text) text->SetLabel(val ? _("High") : _("Low"));
	  pin_info[pin].value = val;
	}
      }
    }
    return;
  }
  
  //names coming from EEPROM processing
  if (parse_buf[0] == EEPROM_LOADING){

    bool isEmpty = true;
    bool isAvailable = true;

    //pin
    int pin = (int)parse_buf[1];
    if (pin <0 || pin>70)
      isAvailable = false;

    //init
    (pin_info[pin].name).resize(SIZE_MAX_NAME);
    (pin_info[pin].unit).resize(SIZE_MAX_UNIT);
    names[pin].resize(SIZE_MAX_NAME);

    //name
    for (int i=0; i<SIZE_MAX_NAME; i++){
      (pin_info[pin].name)[i] =parse_buf[i+2];
       if (parse_buf[i+2]!=0)
	 isEmpty= false;
    }
    names[pin] = pin_info[pin].name;

    //unit
    for (int i = 0; i<SIZE_MAX_UNIT; i++) 
      (pin_info[pin].unit)[i] =parse_buf[i+2+SIZE_MAX_NAME];
      
    //mode
    pin_info[pin].mode = parse_buf[SIZE_MAX_NAME+SIZE_MAX_UNIT+2];
    if ( pin_info[pin].mode<0 || pin_info[pin].mode>=5)
      isAvailable = false;

    if (isEmpty || !isAvailable){
      (pin_info[pin].name).clear();
      names[pin].clear();
      (pin_info[pin].unit).clear();
      pin_info[pin].mode = 255;
    } else {
      add_pin(pin);
    }
  }
  
  if (parse_buf[0] == RECEIVE_NAME && parse_buf[1]>0 && parse_buf[1]<=70){
    int pin = parse_buf[1];
    (pin_info[pin].name).resize(SIZE_MAX_NAME);
    for (int i=0; i<SIZE_MAX_NAME-1; i++)
      (pin_info[pin].name)[i]=parse_buf[i+2];
    names[pin] = pin_info[pin].name;
  }

  if (parse_buf[0] == TSTICK_DATA){
    int pin =  parse_buf[1];
    if (!pin_info[pin].sig && pin>0 && pin<127){ //initialize the signal on the interface and libmapper
      pin_info[pin].mode = MODE_ANALOG;
      add_pin(pin);
    }
    wxStaticText *text = (wxStaticText *)FindWindowById(5000 + pin, scroll);
    int values[6];
    int capaActiv[48];
    int capaCounter = 0;
    for (int i = 0; i<6; i++){
      values[i]=int(parse_buf[i+2]);
      for ( int j = 0; j< 8; j++)
	if ( (parse_buf[i+2] & (1<<j) )){
	  capaActiv[capaCounter] = i*8+j+1;
	  if (pin_info[pin].sig)
	    msig_update(pin_info[pin].sig,  &capaActiv[capaCounter] , 1, tt);
	  capaCounter++;
	  if (text) {
	    wxString val; 
	    val.Printf(_("%d"), i*8+j+1 );
	    text->SetLabel(val);
	  }
	}
    }
  }
  
  if (parse_buf[0] == START_SYSEX && parse_buf[parse_count-1] == END_SYSEX) {
    // Sysex message
    if (parse_buf[1] == REPORT_FIRMWARE) {
      char name[140];
      int len=0;
      for (int i=4; i < parse_count-2; i+=2) 
	name[len++] = (parse_buf[i] & 0x7F) | ((parse_buf[i+1] & 0x7F) << 7);
      name[len++] = '-';
      name[len++] = parse_buf[2] + '0';
      name[len++] = '.';
      name[len++] = parse_buf[3] + '0';
      name[len++] = 0;
      firmata_name = wxString(name,wxConvUTF8);
      // query the board's capabilities only after hearing the
      // REPORT_FIRMWARE message.  For boards that reset when
      // the port open (eg, Arduino with reset=DTR), they are
      // not ready to communicate for some time, so the only
      // way to reliably query their capabilities is to wait
      // until the REPORT_FIRMWARE message is heard.
      uint8_t buf[80];
      len=0;
      buf[len++] = START_SYSEX;
      buf[len++] = ANALOG_MAPPING_QUERY; // read analog to pin # info
      buf[len++] = END_SYSEX;
      buf[len++] = START_SYSEX;
      buf[len++] = CAPABILITY_QUERY; // read capabilities
      buf[len++] = END_SYSEX;
      for (int i=0; i<16; i++) {
	buf[len++] = 0xC0 | i;  // report analog
	buf[len++] = 1;
	buf[len++] = 0xD0 | i;  // report digital
	buf[len++] = 1;
      }
      port.Write(buf, len);
      tx_count += len;
    } else if (parse_buf[1] == CAPABILITY_RESPONSE) {
      int pin, i, n;
      for (pin=0; pin < 128; pin++) 
	pin_info[pin].supported_modes = 0;
      for (i=2, n=0, pin=0; i<parse_count; i++) {
	if (parse_buf[i] == 127) {
	  pin++;
	  n = 0;
	  continue;
	}
	if (n == 0) {
	  // first byte is supported mode
	  pin_info[pin].supported_modes |= (1<<parse_buf[i]);
	}
	n = n ^ 1;
      }
      // send a state query for for every pin with any modes
      for (pin=0; pin < 128; pin++) {
	uint8_t buf[512];
	int len=0;
	if (pin_info[pin].supported_modes) {
	  buf[len++] = START_SYSEX;
	  buf[len++] = PIN_STATE_QUERY;
	  buf[len++] = pin;
	  buf[len++] = END_SYSEX;
	}
	port.Write(buf, len);
	tx_count += len;
      }
    } else if (parse_buf[1] == ANALOG_MAPPING_RESPONSE) {
      int pin=0;
      for (int i=2; i<parse_count-1; i++) {
	pin_info[pin].analog_channel = parse_buf[i];
	pin++;
      }
      return;
    } else if (parse_buf[1] == PIN_STATE_RESPONSE && parse_count >= 6) {
      int pin = parse_buf[2];
      pin_info[pin].mode = parse_buf[3];
      pin_info[pin].value = parse_buf[4];
      if (parse_count > 6) pin_info[pin].value |= (parse_buf[5] << 7);
      if (parse_count > 7) pin_info[pin].value |= (parse_buf[6] << 14);
    }
    return;
  }
}

//save the configuration in an external file
void MyFrame::OnSaveFile( wxCommandEvent&event)
{
  string my_file;
  wxFileDialog * saveFileDialog = new wxFileDialog(this, _("Save File As _?"),\
						   _("saves"), wxEmptyString, \
						   _("*.mapconf"), wxFD_SAVE | wxFD_OVERWRITE_PROMPT, \
						   wxDefaultPosition, wxDefaultSize, _("save window"));
  if (saveFileDialog->ShowModal() == wxID_OK){
    wxString wxFileName = saveFileDialog->GetPath();
    my_file = wx2std(wxFileName);
  }
  strcat(&my_file[0], ".mapconf");
  ofstream file(my_file.c_str(), ios::out);
  if (file){ 
    for (int i = 2; i <128; i++){
      int pinTemp = searchPinByCreatedOrder(i);
      if ( pinTemp != -1){
	for (int k=0; k< SIZE_MAX_NAME; k++)
	  if((pin_info[pinTemp].name)[k]== ' ')
	    (pin_info[pinTemp].name)[k] = '&';
	for (int k=0; k< SIZE_MAX_UNIT; k++)
	  if((pin_info[pinTemp].unit)[k]== ' ')
	    (pin_info[pinTemp].unit)[k] = '&';
	if (pin_info[pinTemp].unit!="")
	  file << pinTemp << " " << pin_info[pinTemp].name << " " << (int)pin_info[pinTemp].mode\
	       << " " << pin_info[pinTemp].unit << endl;
	else
	  file << pinTemp << " " << pin_info[pinTemp].name << " " << (int)pin_info[pinTemp].mode\
	       << " NULL" <<  endl;
	
      }
      else
	continue;
    }
    file << "-1" << endl;
    file.close();
    isConfigurationSaved = true;
  }
}
//Return the pin number associated with the place on the grid
int MyFrame::searchPinByCreatedOrder(int grid_row){
  for (int i = 0; i< 128; i++){
    if (pin_info[i].grid_row == grid_row)
      return i;
  }
  return -1;
}
//load a configuration from an external file
void MyFrame::OnLoadFile( wxCommandEvent &event)
{  
  string my_file;
  int pinTemp[128];
  for (int i=0; i<128; i++)
    pinTemp[i] = -1;
  string nameTemp[128];
  int modeTemp[128];
  string unitTemp[128];
  int j=0;
  bool isFileOk = true;

  wxTextCtrl* tc = new wxTextCtrl(this, -1, wxT(""), wxPoint(-1, -1), 
		      wxSize(0, 0), wxTE_MULTILINE);

  wxFileDialog * openFileDialog = new wxFileDialog(this, _("Load file"), _("saves"), \
						   wxEmptyString, _("*.mapconf"), \
						   wxFD_DEFAULT_STYLE, wxDefaultPosition,\
						   wxDefaultSize, _("load window"));

  if (openFileDialog->ShowModal() == wxID_OK){
    wxString wxFileName = openFileDialog->GetPath();
    tc->LoadFile(wxFileName);
    my_file = wx2std(wxFileName);    
  }
  ifstream file(my_file.c_str(), ios::in);
  if (file){
    
    //delete the existing configuration
    for (int i = 0 ; i< 128; i++)
      if (pin_info[i].sig!=0)
	delete_pin(i);

    while (isFileOk){
      file >> pinTemp[j] >> nameTemp[j] >> modeTemp[j] >> unitTemp[j];
      if (pinTemp[j] == -1)
	isFileOk = false;
      else {
	pin_info[pinTemp[j]].name = nameTemp[j];

	//replace & by space
	for (int k=0; k< SIZE_MAX_NAME; k++)
	  if((pin_info[pinTemp[j]].name)[k]== '&')
	    (pin_info[pinTemp[j]].name)[k] = 0x20;
	
	//Fill up each pin characteristics
	names[pinTemp[j]] = pin_info[pinTemp[j]].name;
	pin_info[pinTemp[j]].mode = modeTemp[j];
	if (unitTemp[j]!="NULL")
	  pin_info[pinTemp[j]].unit = unitTemp[j];
	
	// replace  & by space in the unit
	for (int k=0; k< SIZE_MAX_UNIT; k++)
	  if((pin_info[pinTemp[j]].unit)[k]== '&')
	    (pin_info[pinTemp[j]].unit)[k] = 0x20;

	add_pin(pinTemp[j]);
      }
      j++;
    }
    file.close();
  } else {
    cout << "WARNING : This file doesn't exist, try again" << endl;
  }
}

void MyFrame::OnAbout( wxCommandEvent &event )
{
  wxMessageDialog dialog( this, _("Firmata Mapper 1.0\nCopyright Joseph Malloch and Julie René, based on Firmata Test by Paul Stoffregen"), wxT("About Firmata Mapper"), wxOK|wxICON_INFORMATION );
  dialog.ShowModal();
}

void MyFrame::OnQuit( wxCommandEvent &event )
{

  if (!isConfigurationSaved && grid_count>2){
    wxMessageDialog quitDialog( this, _("Do you want to save the configuration ?"), wxT(""), wxYES_NO );
    int result = quitDialog.ShowModal();
    if (result == wxID_YES){
      wxCommandEvent cmd = wxCommandEvent(-1, -1);
      OnSaveFile(cmd);
    }
  }

  if (dev)
        mdev_free(dev);
    dev = 0;
    Close( true );
}

void MyFrame::OnCloseWindow( wxCloseEvent &event )
{
  if (!isConfigurationSaved && grid_count>2){
    wxMessageDialog quitDialog( this, _("Do you want to save the configuration ?"), wxT(""), wxYES_NO );
    int result = quitDialog.ShowModal();
    if (result == wxID_YES){
      wxCommandEvent cmd = wxCommandEvent(-1, -1);
      OnSaveFile(cmd);
    }
  }
    if (dev)
        mdev_free(dev);
    dev = 0;
    Destroy();
}

void MyFrame::OnSize( wxSizeEvent &event )
{
    event.Skip( true );
}

//------------------------------------------------------------------------------
// Port Menu
//------------------------------------------------------------------------------

MyMenu::MyMenu(const wxString& title, long style) : wxMenu(title, style)
{
}
	
void MyMenu::OnShowPortList(wxMenuEvent &event)
{
	wxMenu *menu;
	wxMenuItem *item;
	int num, any=0;

	menu = event.GetMenu();
	//printf("OnShowPortList, %s\n", (const char *)menu->GetTitle());
	if (menu != port_menu) return;

	wxMenuItemList old_items = menu->GetMenuItems();

	// Disable for dynamic ports
	// Work-around menu "append" bug in wx2.8 for Mac, can't support dynamic menus.
#ifdef __WXMAC__
	if (old_items.GetCount() == 0)
#endif
	  {
	    menu->AppendRadioItem(9000, _(" (none)"));
	    wxArrayString list = port.port_list();
	    num = list.GetCount();
	    for (int i=0; i < num; i++) {
	      //printf("%d: port %s\n", i, (const char *)list[i]);
	      item = menu->AppendRadioItem(9001 + i, list[i]);
	      if (port.Is_open() && port.get_name().IsSameAs(list[i])) {
		menu->Check(9001 + i, true);
		any = 1;
	      }
	    }
	    
	    num = old_items.GetCount();
	    for (int i = old_items.GetCount() - 1; i >= 0; i--) {
	      menu->Delete(old_items[i]);
	    }
	  }
	
	if (!any) menu->Check(9000, true);
} 

void MyMenu::OnHighlight(wxMenuEvent &event)
{
}


//------------------------------------------------------------------------------
// MyApp
//------------------------------------------------------------------------------

IMPLEMENT_APP(MyApp)

MyApp::MyApp()
{
}

bool MyApp::OnInit()
{
    MyFrame *frame = new MyFrame( NULL, -1, _("Firmata Mapper"), wxPoint(500, 50), wxSize(550,400) );
    frame->Show( true );
    
    /*addPinFrame = new wxFrame(frame, NULL, _("add a pin"), wxPoint(500, 50), wxDefaultSize);
      addPinFrame->Show(false);*/
    
    for (int i=0;i<128;i++)
      names[i]="";
    return true;
}

int MyApp::OnExit()
{
    if (dev)
        mdev_free(dev);
    dev = 0;
    return 0;
}

