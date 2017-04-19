/*
 * OSM2MobSinkApp.h
 *
 *  Created on: 18 de abr de 2017
 *      Author: just
 */

#ifndef INCLUDE_OSM2MOBSINKAPP_H_
#define INCLUDE_OSM2MOBSINKAPP_H_

#include <wx/wx.h>
#include <wx/cmdline.h>

class OSM2MobSinkApp: public wxApp
{
private:
	bool OnInit();
	virtual void OnInitCmdLine(wxCmdLineParser& parser);
	virtual bool OnCmdLineParsed(wxCmdLineParser& parser);
	bool Convert(wxString input, wxString output);

	wxString inputfile;
	wxString outputfile;
};

// Command line arguments
static const wxCmdLineEntryDesc g_cmdLineDesc [] =
{
    {
        wxCMD_LINE_SWITCH, ("h"), ("help"), ("displays help on the command line parameters"),
        wxCMD_LINE_VAL_NONE, wxCMD_LINE_OPTION_HELP
    },
    { wxCMD_LINE_OPTION, ("i"), ("input"), ("load OSM XML data from input file") },
    { wxCMD_LINE_OPTION, ("o"), ("output"), ("save MobSink XML network to output file") },

    { wxCMD_LINE_NONE }
};

IMPLEMENT_APP(OSM2MobSinkApp)

#endif /* INCLUDE_OSM2MOBSINKAPP_H_ */
