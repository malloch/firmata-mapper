#ifndef __firmata_mapper_H__
#define __firmata_mapper_H__

#if defined(__GNUG__) && !defined(NO_GCC_PRAGMA)
#pragma interface "firmata_mapper.h"
#endif

#include <wx/wx.h>
#include <wx/tglbtn.h>
#include <stdint.h>
#include "mapper/mapper.h"

#include <string>

//#define LOG_MSG_TO_STDOUT
//#define LOG_MSG_TO_WINDOW

#if defined(LOG_MSG_TO_WINDOW)
#define printf(...) (wxLogMessage(__VA_ARGS__))
#elif defined(LOG_MSG_TO_STDOUT)
#else
#define printf(...)
#endif

// comment this out to enable lots of printing to stdout



const int ID_MENU = 10000;

//----------------------------------------------------------------------------
// MyFrame
//----------------------------------------------------------------------------

class MyFrame: public wxFrame
{
public:
    MyFrame( wxWindow *parent, wxWindowID id, const wxString &title,
        const wxPoint& pos = wxDefaultPosition,
        const wxSize& size = wxDefaultSize,
        long style = wxDEFAULT_FRAME_STYLE );
private:
	wxFlexGridSizer *grid;
	int grid_count;
	wxScrolledWindow *scroll;
	int parse_count;
	int parse_command_len;
	uint8_t parse_buf[4096];
private:
	void init_data(void);
	void new_size(void);
	void rebuild_grid();
	void add_item_to_grid(int row, int col, wxWindow *item);
	//void add_item_to_grid(int col, wxWindow *item);
	void add_pin(int pin);
	void create_signal(int pin);
	void delete_pin(int pin);
	wxString std2wx(std::string s);
	void UpdateStatus(void);
	void Parse(const uint8_t *buf, int len);
	void DoMessage(void);
	void OnSaveFile(wxCommandEvent &event);
	int searchPinByCreatedOrder(int grid_row);
	void OnLoadFile(wxCommandEvent &event);
	void OnAbout(wxCommandEvent &event);
	void OnQuit(wxCommandEvent &event);
	void OnIdle(wxIdleEvent &event);
	void OnCloseWindow(wxCloseEvent &event);
	void OnSize(wxSizeEvent &event);
	void OnPort(wxCommandEvent &event);
	void OnToggleButton(wxCommandEvent &event);
	void OnEEPROM(wxCommandEvent &event);
	void OnAddPin(wxCommandEvent &event);
	void OnButton(wxCommandEvent &event);
	//void OnTextChanged(wxCommandEvent &event);
	void sendName(int pin);
	std::string wx2std(wxString s);
	void OnSliderDrag(wxScrollEvent &event);
	void OnModeChange(wxCommandEvent &event);
	void createSignal(int pin, int mode);
	void OnKey(wxKeyEvent& event);

    static void MapperSignalHandler(mapper_signal msig, mapper_db_signal props,
                                    int instance_id, void *value, int count,
                                    mapper_timetag_t *time);
	DECLARE_EVENT_TABLE()
};


class MyMenu: public wxMenu
{
public:
	MyMenu(const wxString& title = _(""), long style = 0);
	void OnShowPortList(wxMenuEvent &event);
	void OnHighlight(wxMenuEvent &event);
};



//----------------------------------------------------------------------------
// MyApp
//----------------------------------------------------------------------------

class MyApp: public wxApp
{
public:
	MyApp();
	virtual bool OnInit();
	virtual int OnExit();
};

#endif
