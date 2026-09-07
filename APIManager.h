#ifndef APIMANAGER_H
#define APIMANAGER_H

#include <string>

using namespace std;

class APIManager
{
public:
    string getApiKey();
    string buildURL(string city);
    string buildSearchURL(string query);
    string searchLocations(string query);
};

#endif
