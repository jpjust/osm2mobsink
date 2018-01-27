/*
 * OpenStreetMap to MobSink conversion tool.
 * Copyright (C) 2017 - 2018 João Paulo Just Peixoto <just1982@gmail.com>
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
#include <Path.h>
#include <Point.h>
#include <wx/xml/xml.h>
#include <map>
#include <vector>
#include <math.h>

using namespace std;

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
		wxPrintf(wxT("Conversion successful!\n"));
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
	parser.Found(wxT("s"), &this->defaultspeed);

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
	float minlat = 0, maxlat = 0, minlon = 0, maxlon = 0;

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
			wxSize map_size = GetMapSize(minlat, minlon, maxlat, maxlon);
			this->map_width = this->map_width == 0 ? map_size.x : this->map_width;

			if (this->map_height == 0)
				this->map_height = map_size.y;

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
			pathflow flow = PATHFLOW_BI;
			float speedlimit = 0;
			wxString name = wxEmptyString;

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

					// Is this an one-way road?
					if ((nodechild->GetAttribute(wxT("k")) == wxT("oneway")) && (nodechild->GetAttribute(wxT("v")) == wxT("yes")))
						flow = PATHFLOW_AB;

					// Does it have a speed limit?
					if (nodechild->GetAttribute(wxT("k")) == wxT("maxspeed"))
						speedlimit = atof(nodechild->GetAttribute(wxT("v")));

					// Does it have a name?
					if (nodechild->GetAttribute(wxT("k")) == wxT("name"))
						name = nodechild->GetAttribute(wxT("v"));
				}

				nodechild = nodechild->GetNext();
			}

			// If this way is a highway, insert it
			if (insert_way)
			{
				for (unsigned int i = 0; i < way.size(); i++)
				{
					way.at(i).SetName(name);

					// If it is an one-way road, set its attribute
					if (flow == PATHFLOW_AB)
						way.at(i).SetFlow(flow);

					// Set its speed limit
					if (speedlimit > 0)
						way.at(i).InsertControl(1, speedlimit, 1, false);
				}

				paths.insert(paths.end(), way.begin(), way.end());
			}
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
	root->AddAttribute(wxT("speedlimit"), wxString::Format(wxT("%ld"), this->defaultspeed));

	// Insert the paths in the root XML node
	for (unsigned int i = 0; i < paths.size(); i++)
	{
		wxXmlNode *newnode = new wxXmlNode(root, wxXML_ELEMENT_NODE, wxT("path"));

		newnode->AddAttribute(wxT("name"), paths.at(i).GetName());
		newnode->AddAttribute(wxT("xa"), wxString::Format(wxT("%f"), paths.at(i).GetPointA().GetX()));
		newnode->AddAttribute(wxT("ya"), wxString::Format(wxT("%f"), paths.at(i).GetPointA().GetY()));
		newnode->AddAttribute(wxT("xb"), wxString::Format(wxT("%f"), paths.at(i).GetPointB().GetX()));
		newnode->AddAttribute(wxT("yb"), wxString::Format(wxT("%f"), paths.at(i).GetPointB().GetY()));
		if (paths.at(i).GetFlow() == PATHFLOW_AB)
			newnode->AddAttribute(wxT("flow"), wxT("ab"));

		// Set the speed limit if any
		if (paths.at(i).GetPathControl()->find(1) != paths.at(i).GetPathControl()->end())
		{
			wxXmlNode *traffic = new wxXmlNode(newnode, wxXML_ELEMENT_NODE, wxT("traffic"));
			traffic->AddAttribute(wxT("time"), wxT("1"));
			traffic->AddAttribute(wxT("speedlimit"), wxString::Format(wxT("%f"), paths.at(i).GetPathControl()->at(1).speedlimit));
			traffic->AddAttribute(wxT("traffic"), wxT("1"));
		}
	}

	outputdoc.SetRoot(root);
	bool saved = outputdoc.Save(output);

	return saved;
}

// Get map size in meters from latitude and longitude
wxSize OSM2MobSinkApp::GetMapSize(float lat_a, float lon_a, float lat_b, float lon_b)
{
	float lat_avg = (fabsf(lat_a) + fabsf(lat_b)) / 2;
	float lat_delta = fabsf(lat_b - lat_a);
	float lon_delta = fabsf(lon_b - lon_a);
	float width = cosf(lat_avg) * lon_delta * 111320;
	float height = lat_delta * 110574;
	return wxSize(width, height);
}
