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

#include <OSM2MobSinkApp.h>
#include <wx/xml/xml.h>
#include "Point.h"
#include "Path.h"
#include <map>
#include <vector>

using namespace std;

// Default MobSink network size
#define NET_WIDTH 800

// Program initialization
bool OSM2MobSinkApp::OnInit()
{
	if (!wxApp::OnInit())
		return false;

	SetAppName(wxT("OpenStreetMap to MobSink converter"));
	SetVendorName(wxT("LARA"));
	wxLocale app_locale(wxLANGUAGE_ENGLISH_US);

	// Do the conversion
	if (Convert(inputfile, outputfile))
		wxPrintf(wxT("Convertion succesfull!\n"));
	else
		wxPrintf(wxT("The input file could not be converted. Check if it is a valid OpenStreetMap XML file.\n"));

	return false;
}

// Command line init
void OSM2MobSinkApp::OnInitCmdLine(wxCmdLineParser& parser)
{
	parser.SetDesc(g_cmdLineDesc);
	// must refuse '/' as parameter starter or cannot use "/path" style paths
	parser.SetSwitchChars(wxT("-"));
}

// Command line parser
bool OSM2MobSinkApp::OnCmdLineParsed(wxCmdLineParser& parser)
{
	// Get command line arguments
	bool input = parser.Found(wxT("i"), &this->inputfile);
	bool output = parser.Found(wxT("o"), &this->outputfile);
	parser.Found(wxT("nh"), &this->map_height);
	parser.Found(wxT("nw"), &this->map_width);

	// Verify if everything is OK
	if (input && output)
	{
		// All parameters were set. Start conversion.
		wxPrintf(wxT("OpenStreetMap file: %s\n"), this->inputfile.c_str());
		wxPrintf(wxT("MobSink file: %s\n"), this->outputfile.c_str());
	}
	else
	{
		// Some parameter is missing. Display an error message.
		wxPrintf(wxT("You must specify input and output files. Use -h argument to get help.\n"));
		return false;
	}

	return true;
}

// Convert a OpenStreetMap XML file to MobSink XML
bool OSM2MobSinkApp::Convert(wxString input, wxString output)
{
	// Open the XML file
	wxXmlDocument inputdoc(input);

	if ((!inputdoc.IsOk()) || (inputdoc.GetRoot()->GetName() != wxT("osm")))
		return false;

	wxXmlNode *root = inputdoc.GetRoot();
	if (root->GetName() != wxT("osm"))
		return false;

	// Prepare some structures
	map<int, Point> nodes;
	vector<Path> paths;
	float minlat, maxlat, minlon, maxlon;

	// Read map data
	wxXmlNode *child = root->GetChildren();
	while (child)
	{
		// Boundaries
		if (child->GetName() == wxT("bounds"))
		{
			minlat = atof(child->GetAttribute(wxT("minlat")));
			maxlat = atof(child->GetAttribute(wxT("maxlat")));
			minlon = atof(child->GetAttribute(wxT("minlon")));
			maxlon = atof(child->GetAttribute(wxT("maxlon")));

			// After reading the boundaries of the map, it's time to determine the MobSink network size
			// If no height or width were specified, calculate default values
			this->map_width =
					this->map_width == 0 ? NET_WIDTH : this->map_width;

			if (this->map_height == 0)
			{
				// Get ratio of map width over OSM width and set map height
				float ratio = this->map_width / (maxlon - minlon);
				this->map_height = ratio * (maxlat - minlat);
			}

		}
		// Nodes
		else if (child->GetName() == wxT("node"))
		{
			int id = atoi(child->GetAttribute(wxT("id")));
			float lat = atof(child->GetAttribute(wxT("lat")));
			float lon = atof(child->GetAttribute(wxT("lon")));

			// Discard this node if it is outside the boundaries of the exported map
			if ((lat < minlat) || (lat > maxlat) || (lon < minlon) || (lon > maxlon))
			{
				child = child->GetNext();
				continue;
			}

			// Normalize the coordinates
			lat -= minlat;
			lon -= minlon;

			// Correct the vertical mirroring (in MobSink, the y coordinates starts from the top)
			lat = (maxlat - minlat) - lat;

			// Make it proportional to MobSink network size
			lat = (this->map_height * lat) / (maxlat - minlat);
			lon = (this->map_width * lon) / (maxlon - minlon);

			Point p(lon, lat);
			nodes[id] = p;
		}
		// Ways
		else if (child->GetName() == wxT("way"))
		{
			wxXmlNode *nodechild = child->GetChildren();
			vector<Path> way;
			Point a, b;
			int id;
			bool insert_way = false;

			// Get the first node
			while (nodechild && nodechild->GetName() == wxT("nd"))
			{
				id = atoi(nodechild->GetAttribute(wxT("ref")));
				if (nodes.find(id) == nodes.end())
				{
					nodechild = nodechild->GetNext();
					continue;
				}

				a = nodes[id];
				nodechild = nodechild->GetNext();
				break;
			}

			// Get the following nodes and create paths
			while (nodechild)
			{
				// Nodes
				if (nodechild->GetName() == wxT("nd"))
				{
					id = atoi(nodechild->GetAttribute(wxT("ref")));
					if (nodes.find(id) == nodes.end())
					{
						nodechild = nodechild->GetNext();
						continue;
					}

					b = nodes[id];
					Path p(a, b);
					way.push_back(p);
					a = b;
				}
				// Tags
				else if (nodechild->GetName() == wxT("tag"))
				{
					// Only insert a way if it is a highway (roads, streets, etc.)
					if (nodechild->GetAttribute(wxT("k")) == wxT("highway"))
						insert_way = true;
				}

				nodechild = nodechild->GetNext();
			}

			// If this way is a highway, insert it
			if (insert_way)
				paths.insert(paths.end(), way.begin(), way.end());
		}

		child = child->GetNext();
	}

	// At this point, we have all the paths.
	// Now it's time to create the output file.
	wxXmlDocument outputdoc;
	root = new wxXmlNode(NULL, wxXML_ELEMENT_NODE, wxT("network"));

	// Insert network size
	root->AddAttribute(wxT("width"), wxString::Format(wxT("%ld"), this->map_width));
	root->AddAttribute(wxT("height"), wxString::Format(wxT("%ld"), this->map_height));

	// Insert the paths in the root XML node
	for (unsigned int i = 0; i < paths.size(); i++)
	{
		wxXmlNode *newnode = new wxXmlNode(root, wxXML_ELEMENT_NODE,
				wxT("path"));

		newnode->AddAttribute(wxT("xa"), wxString::Format(wxT("%f"), paths.at(i).GetPointA().GetX()));
		newnode->AddAttribute(wxT("ya"), wxString::Format(wxT("%f"), paths.at(i).GetPointA().GetY()));
		newnode->AddAttribute(wxT("xb"), wxString::Format(wxT("%f"), paths.at(i).GetPointB().GetX()));
		newnode->AddAttribute(wxT("yb"), wxString::Format(wxT("%f"), paths.at(i).GetPointB().GetY()));
	}

	outputdoc.SetRoot(root);
	bool saved = outputdoc.Save(output);

	return saved;
}
