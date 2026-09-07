#include <iostream>
#include <fstream>
#include <vector>
#include <map>
#include <algorithm>
#include <ctime>
#include <cctype>
#include <string>
#include <limits>
#include <sstream>

#include <nlohmann/json.hpp>

#include "APIManager.h"

#include "location.h"
#include "weather.h"


// nlohmann-json 2.1.1 compatibility helper.
template <typename T>
T jsonValue(const nlohmann::json& object,
            const string& key,
            const T& fallback)
{
    if (!object.is_object())
    {
        return fallback;
    }

    nlohmann::json::const_iterator it = object.find(key);

    if (it == object.end() || it->is_null())
    {
        return fallback;
    }

    try
    {
        return it->get<T>();
    }
    catch (...)
    {
        return fallback;
    }
}

using namespace std;


// ============================================
// GLOBAL DATA
// ============================================

vector<string> favoriteCities;

vector<string> recentSearches;

map<string, int> searchCount;


// ============================================
// CONVERT CITY NAME TO LOWERCASE
// ============================================

string normalizeCityName(const string& city)
{
    string result = city;

    for (char& c : result)
    {
        c = static_cast<char>(
            tolower(
                static_cast<unsigned char>(c)
            )
        );
    }

    return result;
}


// ============================================
// CHECK IF CITY ALREADY EXISTS
// ============================================

bool cityExists(
    const vector<string>& cities,
    const string& city)
{
    string normalizedCity =
        normalizeCityName(city);


    for (const string& existing : cities)
    {
        if (normalizeCityName(existing)
            == normalizedCity)
        {
            return true;
        }
    }


    return false;
}


// ============================================
// LOAD FAVORITE CITIES
// ============================================

void LocationManager::loadFavoriteCities()
{
    favoriteCities.clear();


    ifstream file("favorites.csv");


    if (!file.is_open())
    {
        return;
    }


    string city;


    while (getline(file, city))
    {
        // Remove carriage return if present
        if (!city.empty() &&
            city.back() == '\r')
        {
            city.pop_back();
        }


        // Ignore empty lines
        if (city.empty())
        {
            continue;
        }


        // Prevent duplicate cities
        if (!cityExists(favoriteCities, city))
        {
            favoriteCities.push_back(city);
        }
    }


    file.close();
}


// ============================================
// LOAD SEARCH HISTORY
// ============================================

void LocationManager::loadSearchHistory()
{
    recentSearches.clear();

    searchCount.clear();


    ifstream file("location_history.csv");


    if (!file.is_open())
    {
        return;
    }


    string line;


    while (getline(file, line))
    {
        if (line.empty())
        {
            continue;
        }


        size_t commaPosition =
            line.find(',');


        string city;


        if (commaPosition != string::npos)
        {
            city =
                line.substr(
                    0,
                    commaPosition
                );
        }
        else
        {
            city = line;
        }


        if (!city.empty())
        {
            recentSearches.push_back(city);

            searchCount[city]++;
        }
    }


    // Keep only latest 10 searches

    if (recentSearches.size() > 10)
    {
        recentSearches.erase(
            recentSearches.begin(),
            recentSearches.end() - 10
        );
    }


    file.close();
}


// ============================================
// SEARCH LOCATION
// ============================================

void LocationManager::searchLocation()
{
    string query;

    cout << "\nEnter City / Town Search: ";

    cin.ignore(
        numeric_limits<streamsize>::max(),
        '\n'
    );

    getline(cin, query);

    // Remove leading/trailing spaces
    size_t first = query.find_first_not_of(" \t");

    if (first == string::npos)
    {
        cout << "\nSearch text cannot be empty.\n";
        return;
    }

    size_t last = query.find_last_not_of(" \t");

    query = query.substr(
        first,
        last - first + 1
    );

    cout << "\nSearching locations...\n";

    APIManager api;

    string response =
        api.searchLocations(query);

    if (response.empty())
    {
        cout << "\nUnable To Retrieve Location Search Results.\n";
        cout << "Please check your API key or internet connection.\n";
        return;
    }

    vector<LocationResult> locations;

    try
    {
        nlohmann::json data =
            nlohmann::json::parse(response);

        if (!data.is_array())
        {
            cout << "\nInvalid Location Search Response.\n";
            return;
        }

        for (const auto& item : data)
        {
            LocationResult location;

            location.name =
                jsonValue<string>(item, "name", "");

            location.region =
                jsonValue<string>(item, "region", "");

            location.country =
                jsonValue<string>(item, "country", "");

            location.latitude =
                jsonValue<double>(item, "lat", 0.0);

            location.longitude =
                jsonValue<double>(item, "lon", 0.0);

            if (!location.name.empty())
            {
                locations.push_back(location);
            }
        }
    }
    catch (const nlohmann::json::exception&)
    {
        cout << "\nError: Invalid JSON response from Location Search API.\n";
        return;
    }

    if (locations.empty())
    {
        cout << "\nNo matching locations found for \""
             << query
             << "\".\n";
        return;
    }

    // --------------------------------------------------------
    // Rank results so local Indian / Tamil Nadu results appear
    // earlier when possible.
    // --------------------------------------------------------
    string normalizedQuery =
        normalizeCityName(query);

    stable_sort(
        locations.begin(),
        locations.end(),
        [&normalizedQuery](const LocationResult& a,
                           const LocationResult& b)
        {
            auto rankLocation =
                [&normalizedQuery](const LocationResult& location)
                {
                    string name =
                        normalizeCityName(location.name);

                    string region =
                        normalizeCityName(location.region);

                    string country =
                        normalizeCityName(location.country);

                    int rank = 0;

                    // Exact name match
                    if (name == normalizedQuery)
                    {
                        rank -= 100;
                    }
                    // Name starts with search text
                    else if (
                        name.rfind(
                            normalizedQuery,
                            0
                        ) == 0)
                    {
                        rank -= 50;
                    }

                    // Prefer India
                    if (country == "india")
                    {
                        rank -= 20;
                    }

                    // Prefer Tamil Nadu
                    if (region == "tamil nadu")
                    {
                        rank -= 30;
                    }

                    return rank;
                };

            return rankLocation(a) <
                   rankLocation(b);
        }
    );

    // WeatherAPI normally returns a small list.
    // Limit the display to keep the menu readable.
    const size_t maxResults = 10;

    if (locations.size() > maxResults)
    {
        locations.resize(maxResults);
    }

    cout << "\n===== LOCATION SEARCH RESULTS =====\n";

    for (size_t i = 0;
         i < locations.size();
         ++i)
    {
        cout << "\n"
             << i + 1
             << ". "
             << locations[i].name;

        if (!locations[i].region.empty())
        {
            cout << ", "
                 << locations[i].region;
        }

        if (!locations[i].country.empty())
        {
            cout << ", "
                 << locations[i].country;
        }

        cout << "\n   Latitude : "
             << locations[i].latitude;

        cout << "\n   Longitude: "
             << locations[i].longitude
             << "\n";
    }

    cout << "\n0. Cancel Search\n";

    int selection;

    cout << "\nSelect Location: ";

    if (!(cin >> selection))
    {
        cin.clear();

        cin.ignore(
            numeric_limits<streamsize>::max(),
            '\n'
        );

        cout << "\nInvalid selection.\n";
        return;
    }

    if (selection == 0)
    {
        cout << "\nLocation Search Cancelled.\n";
        return;
    }

    if (selection < 1 ||
        static_cast<size_t>(selection) >
            locations.size())
    {
        cout << "\nInvalid location selection.\n";
        return;
    }

    LocationResult selected =
        locations[
            static_cast<size_t>(selection - 1)
        ];

    // --------------------------------------------------------
    // Use the exact latitude and longitude selected by the user.
    // The weather function accepts the coordinate pair through
    // its existing city/query parameter.
    // --------------------------------------------------------
    ostringstream coordinateQuery;

    coordinateQuery
        << selected.latitude
        << ","
        << selected.longitude;

    string selectedCity =
        selected.name;

    cout << "\nSelected Location: "
         << selectedCity;

    if (!selected.region.empty())
    {
        cout << ", "
             << selected.region;
    }

    if (!selected.country.empty())
    {
        cout << ", "
             << selected.country;
    }

    cout << "\nLatitude : "
         << selected.latitude;

    cout << "\nLongitude: "
         << selected.longitude
         << "\n";

    cout << "\nFetching Weather Data...\n";

    // --------------------------------------------------------
    // Add the VERIFIED city name to search history.
    // --------------------------------------------------------
    recentSearches.push_back(selectedCity);

    if (recentSearches.size() > 10)
    {
        recentSearches.erase(
            recentSearches.begin()
        );
    }

    searchCount[selectedCity]++;

    ofstream file(
        "location_history.csv",
        ios::app
    );

    if (file.is_open())
    {
        time_t now = time(0);

        file << selectedCity
             << ","
             << ctime(&now);

        file.close();
    }

    // --------------------------------------------------------
    // Fetch weather using exact coordinates.
    // Example:
    // q=11.4775,77.8696
    // --------------------------------------------------------
    cout << fetchWeather(
        coordinateQuery.str()
    )
    << endl;
}


// ============================================
// ADD FAVORITE CITY
// ============================================

void LocationManager::addFavoriteCity()
{
    string city;


    cout << "\nEnter Favorite City: ";

    cin >> city;


    // Check duplicate
    // Case-insensitive

    if (cityExists(favoriteCities, city))
    {
        cout << "\nCity already exists "
             << "in Favorites.\n";

        return;
    }


    // Add to vector

    favoriteCities.push_back(city);


    // Save to CSV

    ofstream file(
        "favorites.csv",
        ios::app
    );


    if (file.is_open())
    {
        file << city << endl;

        file.close();


        cout << "\nFavorite City Added "
             << "Successfully.\n";
    }
    else
    {
        cout << "\nError: Unable to save "
             << "Favorite City.\n";
    }
}


// ============================================
// VIEW FAVORITE CITIES
// ============================================

void LocationManager::viewFavoriteCities()
{
    cout << "\n===== FAVORITE CITIES =====\n";


    if (favoriteCities.empty())
    {
        cout << "No Favorite Cities Found.\n";
        cout << "Enter 0 to exit.\n";
        return;
    }


    for (size_t i = 0;
         i < favoriteCities.size();
         i++)
    {
        cout << i + 1
             << ". "
             << favoriteCities[i]
             << endl;
    }

    cout << "\n0. Exit\n";
    cout << "Enter the number of a favorite city to open: ";

    int selection;

    if (!(cin >> selection))
    {
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        cout << "\nInvalid selection.\n";
        return;
    }

    if (selection == 0)
    {
        cout << "\nFavorite City View Closed.\n";
        return;
    }

    if (selection < 1 ||
        static_cast<size_t>(selection) > favoriteCities.size())
    {
        cout << "\nInvalid favorite city selection.\n";
        return;
    }

    const string& selectedCity =
        favoriteCities[static_cast<size_t>(selection - 1)];

    cout << "\nOpening actions for: "
             << selectedCity
             << "\n";
    cout << fetchWeather(selectedCity)
             << endl;
}


// ============================================
// REMOVE FAVORITE CITY
// ============================================

void LocationManager::removeFavoriteCity()
{
    cout << "\n===== REMOVE FAVORITE CITY =====\n";

    if (favoriteCities.empty())
    {
        cout << "No Favorite Cities Found.\n";
        return;
    }

    for (size_t i = 0; i < favoriteCities.size(); ++i)
    {
        cout << i + 1 << ". " << favoriteCities[i] << endl;
    }

    cout << "0. Cancel\n";

    int selection;
    cout << "\nSelect City To Remove: ";

    if (!(cin >> selection))
    {
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        cout << "\nInvalid selection.\n";
        return;
    }

    if (selection == 0)
    {
        cout << "\nRemoval Cancelled.\n";
        return;
    }

    if (selection < 1 || static_cast<size_t>(selection) > favoriteCities.size())
    {
        cout << "\nInvalid favorite city selection.\n";
        return;
    }

    string removedCity = favoriteCities[static_cast<size_t>(selection - 1)];
    favoriteCities.erase(favoriteCities.begin() + (selection - 1));

    ofstream file("favorites.csv", ios::trunc);

    if (!file.is_open())
    {
        cout << "\nError: Unable to update favorites.csv.\n";
        return;
    }

    for (const string& city : favoriteCities)
    {
        file << city << endl;
    }

    file.close();

    cout << "\nFavorite City Removed Successfully: "
         << removedCity << endl;
}


// ============================================
// RECENT SEARCHES
// ============================================

void LocationManager::showRecentSearches()
{
    cout << "\n===== RECENT SEARCHES =====\n";


    if (recentSearches.empty())
    {
        cout << "No Recent Searches Found.\n";

        return;
    }


    for (int i =
             static_cast<int>(
                 recentSearches.size()
             ) - 1;
         i >= 0;
         i--)
    {
        cout << recentSearches[i]
             << endl;
    }
}


// ============================================
// MOST SEARCHED CITY
// ============================================

void LocationManager::showMostSearchedCity()
{
    string city;

    int maxCount = 0;


    for (const auto& item :
         searchCount)
    {
        if (item.second > maxCount)
        {
            city = item.first;

            maxCount = item.second;
        }
    }


    cout << "\n===== MOST SEARCHED CITY =====\n";


    if (city.empty())
    {
        cout << "No Search History Available.\n";

        return;
    }


    cout << "City : "
         << city
         << endl;


    cout << "Search Count : "
         << maxCount
         << endl;
}


// ============================================
// WEATHER COMFORT SCORE
// ============================================

void LocationManager::weatherComfortScore()
{
    string city;


    cout << "\nEnter City: ";

    cin >> city;


    cout << "\nFetching Weather Data...\n";


    string result =
        fetchWeather(city);


    if (!isWeatherDataAvailable())
    {
        cout << result << endl;

        return;
    }


    float temperature =
        getLastTemperature();


    float humidity =
        getLastHumidity();


    string condition =
        getLastCondition();


    // ========================================
    // TEMPERATURE SCORE
    // ========================================

    int temperatureScore;


    if (temperature >= 20 &&
        temperature <= 28)
    {
        temperatureScore = 40;
    }
    else if (temperature >= 15 &&
             temperature <= 32)
    {
        temperatureScore = 30;
    }
    else if (temperature >= 10 &&
             temperature <= 35)
    {
        temperatureScore = 20;
    }
    else
    {
        temperatureScore = 10;
    }


    // ========================================
    // HUMIDITY SCORE
    // ========================================

    int humidityScore;


    if (humidity >= 40 &&
        humidity <= 60)
    {
        humidityScore = 30;
    }
    else if (humidity >= 30 &&
             humidity <= 70)
    {
        humidityScore = 20;
    }
    else if (humidity >= 20 &&
             humidity <= 80)
    {
        humidityScore = 10;
    }
    else
    {
        humidityScore = 5;
    }


    // ========================================
    // CONDITION SCORE
    // ========================================

    int conditionScore = 30;


    string lowerCondition =
        condition;


    for (char& c :
         lowerCondition)
    {
        c = static_cast<char>(
            tolower(
                static_cast<unsigned char>(c)
            )
        );
    }


    if (lowerCondition.find("rain")
            != string::npos ||
        lowerCondition.find("storm")
            != string::npos ||
        lowerCondition.find("snow")
            != string::npos)
    {
        conditionScore = 10;
    }
    else if (
        lowerCondition.find("cloud")
        != string::npos)
    {
        conditionScore = 20;
    }
    else if (
        lowerCondition.find("clear")
        != string::npos ||
        lowerCondition.find("sun")
        != string::npos)
    {
        conditionScore = 30;
    }


    // ========================================
    // FINAL SCORE
    // ========================================

    int score =
        temperatureScore +
        humidityScore +
        conditionScore;


    if (score > 100)
    {
        score = 100;
    }


    cout << "\n===== WEATHER COMFORT SCORE =====\n";


    cout << "City        : "
         << getLastLocation()
         << endl;


    cout << "Temperature : "
         << temperature
         << " C"
         << endl;


    cout << "Humidity    : "
         << humidity
         << "%"
         << endl;


    cout << "Condition   : "
         << condition
         << endl;


    cout << "Score       : "
         << score
         << "/100"
         << endl;


    if (score >= 90)
    {
        cout << "Excellent Weather\n";
    }
    else if (score >= 75)
    {
        cout << "Good Weather\n";
    }
    else if (score >= 50)
    {
        cout << "Moderate Weather\n";
    }
    else
    {
        cout << "Poor Weather\n";
    }
}


// ============================================
// TRAVEL RECOMMENDATION
// ============================================

void LocationManager::travelRecommendation()
{
    string city;


    cout << "\nEnter City: ";

    cin >> city;


    cout << "\nFetching Weather Data...\n";


    string result =
        fetchWeather(city);


    if (!isWeatherDataAvailable())
    {
        cout << result << endl;

        return;
    }


    float temperature =
        getLastTemperature();


    float humidity =
        getLastHumidity();


    float wind =
        getLastWindSpeed();


    string condition =
        getLastCondition();


    string lowerCondition =
        condition;


    for (char& c :
         lowerCondition)
    {
        c = static_cast<char>(
            tolower(
                static_cast<unsigned char>(c)
            )
        );
    }


    bool badCondition =
        lowerCondition.find("rain")
            != string::npos ||
        lowerCondition.find("storm")
            != string::npos ||
        lowerCondition.find("snow")
            != string::npos;


    cout << "\n===== TRAVEL ADVISOR =====\n";


    cout << "City        : "
         << getLastLocation()
         << endl;


    cout << "Temperature : "
         << temperature
         << " C"
         << endl;


    cout << "Humidity    : "
         << humidity
         << "%"
         << endl;


    cout << "Wind Speed  : "
         << wind
         << " km/h"
         << endl;


    cout << "Condition   : "
         << condition
         << endl;


    // ========================================
    // RECOMMENDATION
    // ========================================

    if (badCondition)
    {
        cout << "\nTravel Recommendation:\n";

        cout << "Not Recommended For "
             << "Outdoor Activities.\n";

        cout << "Weather conditions may "
             << "affect travel.\n";
    }
    else if (temperature >= 20 &&
             temperature <= 32 &&
             humidity <= 75 &&
             wind <= 35)
    {
        cout << "\nTravel Recommendation:\n";

        cout << "Suitable For Travel.\n";

        cout << "Outdoor Activities "
             << "Recommended.\n";

        cout << "Weather Conditions "
             << "Are Favorable.\n";
    }
    else
    {
        cout << "\nTravel Recommendation:\n";

        cout << "Travel Is Possible, "
             << "But Use Caution.\n";

        cout << "Check Weather Conditions "
             << "Before Outdoor Activities.\n";
    }
}


// ============================================
// COMPARE TWO CITIES
// ============================================

void LocationManager::compareCities()
{
    string city1;

    string city2;


    cout << "\nEnter First City : ";

    cin >> city1;


    cout << "Enter Second City : ";

    cin >> city2;


    // ========================================
    // FETCH FIRST CITY
    // ========================================

    cout << "\nFetching Weather For "
         << city1
         << "...\n";


    string result1 =
        fetchWeather(city1);


    if (!isWeatherDataAvailable())
    {
        cout << result1 << endl;

        return;
    }


    float temperature1 =
        getLastTemperature();

    float humidity1 =
        getLastHumidity();

    float wind1 =
        getLastWindSpeed();

    float pressure1 =
        getLastPressure();

    string condition1 =
        getLastCondition();

    string location1 =
        getLastLocation();


    // ========================================
    // FETCH SECOND CITY
    // ========================================

    cout << "\nFetching Weather For "
         << city2
         << "...\n";


    string result2 =
        fetchWeather(city2);


    if (!isWeatherDataAvailable())
    {
        cout << result2 << endl;

        return;
    }


    float temperature2 =
        getLastTemperature();

    float humidity2 =
        getLastHumidity();

    float wind2 =
        getLastWindSpeed();

    float pressure2 =
        getLastPressure();

    string condition2 =
        getLastCondition();

    string location2 =
        getLastLocation();


    // ========================================
    // DISPLAY CITY 1
    // ========================================

    cout << "\n========================================\n";

    cout << "          CITY COMPARISON\n";

    cout << "========================================\n";


    cout << "\n----------- "
         << location1
         << " -----------\n";


    cout << "Temperature : "
         << temperature1
         << " C\n";


    cout << "Humidity    : "
         << humidity1
         << " %\n";


    cout << "Wind Speed  : "
         << wind1
         << " km/h\n";


    cout << "Pressure    : "
         << pressure1
         << " mb\n";


    cout << "Condition   : "
         << condition1
         << "\n";


    // ========================================
    // DISPLAY CITY 2
    // ========================================

    cout << "\n----------- "
         << location2
         << " -----------\n";


    cout << "Temperature : "
         << temperature2
         << " C\n";


    cout << "Humidity    : "
         << humidity2
         << " %\n";


    cout << "Wind Speed  : "
         << wind2
         << " km/h\n";


    cout << "Pressure    : "
         << pressure2
         << " mb\n";


    cout << "Condition   : "
         << condition2
         << "\n";


    // ========================================
    // TEMPERATURE COMPARISON
    // ========================================

    cout << "\n========================================\n";

    cout << "          COMPARISON RESULTS\n";

    cout << "========================================\n";


    cout << "\nTemperature:\n";


    if (temperature1 > temperature2)
    {
        cout << location1
             << " is hotter by "
             << temperature1 - temperature2
             << " C\n";
    }
    else if (temperature2 > temperature1)
    {
        cout << location2
             << " is hotter by "
             << temperature2 - temperature1
             << " C\n";
    }
    else
    {
        cout << "Both cities have the same temperature.\n";
    }


    // ========================================
    // HUMIDITY COMPARISON
    // ========================================

    cout << "\nHumidity:\n";


    if (humidity1 < humidity2)
    {
        cout << location1
             << " has lower humidity.\n";
    }
    else if (humidity2 < humidity1)
    {
        cout << location2
             << " has lower humidity.\n";
    }
    else
    {
        cout << "Both cities have the same humidity.\n";
    }


    // ========================================
    // WIND COMPARISON
    // ========================================

    cout << "\nWind Speed:\n";


    if (wind1 < wind2)
    {
        cout << location1
             << " has lower wind speed.\n";
    }
    else if (wind2 < wind1)
    {
        cout << location2
             << " has lower wind speed.\n";
    }
    else
    {
        cout << "Both cities have the same wind speed.\n";
    }


    // ========================================
    // PRESSURE COMPARISON
    // ========================================

    cout << "\nPressure:\n";


    if (pressure1 > pressure2)
    {
        cout << location1
             << " has higher pressure.\n";
    }
    else if (pressure2 > pressure1)
    {
        cout << location2
             << " has higher pressure.\n";
    }
    else
    {
        cout << "Both cities have the same pressure.\n";
    }


    // ========================================
    // OVERALL WEATHER SCORE
    // ========================================

    int score1 = 0;

    int score2 = 0;


    // ----------------------------------------
    // TEMPERATURE
    // ----------------------------------------

    if (temperature1 >= 20 &&
        temperature1 <= 30)
    {
        score1 += 2;
    }
    else if (temperature1 >= 15 &&
             temperature1 <= 35)
    {
        score1 += 1;
    }


    if (temperature2 >= 20 &&
        temperature2 <= 30)
    {
        score2 += 2;
    }
    else if (temperature2 >= 15 &&
             temperature2 <= 35)
    {
        score2 += 1;
    }


    // ----------------------------------------
    // HUMIDITY
    // ----------------------------------------

    if (humidity1 >= 40 &&
        humidity1 <= 60)
    {
        score1 += 2;
    }
    else if (humidity1 >= 30 &&
             humidity1 <= 70)
    {
        score1 += 1;
    }


    if (humidity2 >= 40 &&
        humidity2 <= 60)
    {
        score2 += 2;
    }
    else if (humidity2 >= 30 &&
             humidity2 <= 70)
    {
        score2 += 1;
    }


    // ----------------------------------------
    // WIND
    // ----------------------------------------

    if (wind1 <= 20)
    {
        score1 += 2;
    }
    else if (wind1 <= 35)
    {
        score1 += 1;
    }


    if (wind2 <= 20)
    {
        score2 += 2;
    }
    else if (wind2 <= 35)
    {
        score2 += 1;
    }


    // ----------------------------------------
    // CONDITION
    // ----------------------------------------

    string lowerCondition1 =
        condition1;


    string lowerCondition2 =
        condition2;


    for (char& c :
         lowerCondition1)
    {
        c = static_cast<char>(
            tolower(
                static_cast<unsigned char>(c)
            )
        );
    }


    for (char& c :
         lowerCondition2)
    {
        c = static_cast<char>(
            tolower(
                static_cast<unsigned char>(c)
            )
        );
    }


    if (lowerCondition1.find("rain")
            == string::npos &&
        lowerCondition1.find("storm")
            == string::npos &&
        lowerCondition1.find("snow")
            == string::npos)
    {
        score1 += 2;
    }


    if (lowerCondition2.find("rain")
            == string::npos &&
        lowerCondition2.find("storm")
            == string::npos &&
        lowerCondition2.find("snow")
            == string::npos)
    {
        score2 += 2;
    }


    // ========================================
    // FINAL RESULT
    // ========================================

    cout << "\n========================================\n";

    cout << "           FINAL RESULT\n";

    cout << "========================================\n";


    cout << "\n"
         << location1
         << " Weather Score : "
         << score1
         << "/8\n";


    cout << location2
         << " Weather Score : "
         << score2
         << "/8\n";


    if (score1 > score2)
    {
        cout << "\nBetter Weather: "
             << location1
             << "\n";
    }
    else if (score2 > score1)
    {
        cout << "\nBetter Weather: "
             << location2
             << "\n";
    }
    else
    {
        cout << "\nBoth cities have similar "
             << "overall weather conditions.\n";
    }


    cout << "\n========================================\n";
}