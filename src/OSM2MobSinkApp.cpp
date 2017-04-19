/*
 * OSM2MobSinkApp.cpp
 *
 *  Created on: 18 de abr de 2017
 *      Author: just
 */

#include <OSM2MobSinkApp.h>
#include <wx/xml/xml.h>
#include "Point.h"
#include "Path.h"
#include <map>
#include <vector>

using namespace std;

// Multiplier for coordinates normalization (higher = less precision)
#define MULT 100000

// Program initialization
bool OSM2MobSinkApp::OnInit()
{
    if (!wxApp::OnInit())
        return false;

    SetAppName(wxT("OpenStreetMap to MobSink converter"));
    SetVendorName(wxT("NetMedia"));
    wxLocale app_locale(wxLANGUAGE_ENGLISH_US);

    // Do the conversion
    Convert(inputfile, outputfile);

    return false;
}

// Command line init
void OSM2MobSinkApp::OnInitCmdLine(wxCmdLineParser& parser)
{
    parser.SetDesc (g_cmdLineDesc);
    // must refuse '/' as parameter starter or cannot use "/path" style paths
    parser.SetSwitchChars (wxT("-"));
}

// Command line parser
bool OSM2MobSinkApp::OnCmdLineParsed(wxCmdLineParser& parser)
{
    // Get command line arguments
    bool input = parser.Found(wxT("i"), &this->inputfile);
    bool output = parser.Found(wxT("o"), &this->outputfile);

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
        wxPrintf(wxT("If you want to use command line, you must set all parameters! Use -h option to get help.\n"));
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
        }
        // Nodes
        else if (child->GetName() == wxT("node"))
        {
        	int id = atoi(child->GetAttribute(wxT("id")));
        	float lat = atof(child->GetAttribute(wxT("lat"))) - minlat;
        	float lon = atof(child->GetAttribute(wxT("lon"))) - minlon;

        	// Correct the vertical mirroring (in MobSink, the y coordinates starts from the top)
        	lat = (maxlat - minlat) - lat;

        	// Apply the MULT
        	lat *= MULT;
        	lon *= MULT;

        	Point p(lon, lat);
        	nodes[id] = p;
        }
        // Ways
        else if (child->GetName() == wxT("way"))
        {
        	wxXmlNode *nodechild = child->GetChildren();
        	Point a, b;

        	// Get the first node
        	if (nodechild && nodechild->GetName() == wxT("nd"))
        	{
        		a = nodes[atoi(nodechild->GetAttribute(wxT("ref")))];
        		nodechild = nodechild->GetNext();
        	}

        	// Get the following nodes and create paths
        	while (nodechild)
        	{
        		if (nodechild->GetName() == wxT("nd"))
        		{
        			b = nodes[atoi(nodechild->GetAttribute(wxT("ref")))];
        			Path p(a, b);
        			paths.push_back(p);
        			a = b;
        		}

        		nodechild = nodechild->GetNext();
        	}
        }

        child = child->GetNext();
    }

    // At this point, we have all the paths.
    // Now it's time to create the output file.
    wxXmlDocument outputdoc;
    root = new wxXmlNode(NULL, wxXML_ELEMENT_NODE, wxT("network"));

    // Insert the paths in the root XML node
    for (unsigned int i = 0; i < paths.size(); i++)
    {
        wxXmlNode *newnode = new wxXmlNode(root, wxXML_ELEMENT_NODE, wxT("path"));

        newnode->AddAttribute(wxT("xa"), wxString::Format(wxT("%f"), paths.at(i).GetPointA().GetX()));
        newnode->AddAttribute(wxT("ya"), wxString::Format(wxT("%f"), paths.at(i).GetPointA().GetY()));
        newnode->AddAttribute(wxT("xb"), wxString::Format(wxT("%f"), paths.at(i).GetPointB().GetX()));
        newnode->AddAttribute(wxT("yb"), wxString::Format(wxT("%f"), paths.at(i).GetPointB().GetY()));
    }

    outputdoc.SetRoot(root);
    bool saved = outputdoc.Save(output);

    return saved;
}
