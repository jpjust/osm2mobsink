/*
 * OpenStreetMap to MobSink convertion tool.
 * Copyright (C) 2017 João Paulo Just Peixoto <just1982@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef INCLUDE_OSM2MOBSINKAPP_H_
#define INCLUDE_OSM2MOBSINKAPP_H_

#define DEFAULT_SPEED 50

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
	long int map_width = 0;
	long int map_height = 0;
	long int defaultspeed = DEFAULT_SPEED;
};

// Command line arguments
static const wxCmdLineEntryDesc g_cmdLineDesc[] =
{
	{ wxCMD_LINE_SWITCH, ("h"),  ("help"),   ("displays help on the command line parameters"), wxCMD_LINE_VAL_NONE,	wxCMD_LINE_OPTION_HELP },

	{ wxCMD_LINE_OPTION, ("i"),  ("input"),  ("load OSM XML data from input file") },
	{ wxCMD_LINE_OPTION, ("o"),  ("output"), ("save MobSink XML network to output file") },
	{ wxCMD_LINE_OPTION, ("nw"), ("width"),	 ("set the default MobSink network width"), wxCMD_LINE_VAL_NUMBER },
	{ wxCMD_LINE_OPTION, ("nh"), ("height"), ("set the default MobSink network height"), wxCMD_LINE_VAL_NUMBER },
	{ wxCMD_LINE_OPTION, ("s"),  ("speed"),  ("set the default MobSink network speed limit (default: 50)"), wxCMD_LINE_VAL_NUMBER },

	{ wxCMD_LINE_NONE }
};

IMPLEMENT_APP(OSM2MobSinkApp)

#endif /* INCLUDE_OSM2MOBSINKAPP_H_ */
