#ifndef LOCATION_H
#define LOCATION_H

#include <string>
using namespace std;

// ------------------------------------------------------------
// Location information returned by the Search API
// ------------------------------------------------------------
struct LocationResult
{
    string name;
    string region;
    string country;
    double latitude = 0.0;
    double longitude = 0.0;
};

class LocationManager
{
public:

    void searchLocation();

    void addFavoriteCity();
    void viewFavoriteCities();
    void removeFavoriteCity();

    void showRecentSearches();
    void showMostSearchedCity();

    void weatherComfortScore();
    void travelRecommendation();
    void compareCities();

    void loadFavoriteCities();
    void loadSearchHistory();
};

#endif
