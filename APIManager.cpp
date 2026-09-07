

#include <curl/curl.h>
#include <string>
#include <cctype>
#include <cstdlib>
#include <iostream>

#include "APIManager.h"
using namespace std;

// ------------------------------------------------------------
// CURL callback
// ------------------------------------------------------------
static size_t SearchWriteCallback(
    void* contents,
    size_t size,
    size_t nmemb,
    string* output)
{
    size_t totalSize = size * nmemb;

    if (output != nullptr)
    {
        output->append(
            static_cast<char*>(contents),
            totalSize
        );
    }

    return totalSize;
}

// ------------------------------------------------------------
// URL encode
// ------------------------------------------------------------
static string urlEncode(const string& value)
{
    const char* hex = "0123456789ABCDEF";
    string encoded;

    for (unsigned char c : value)
    {
        if (isalnum(c) ||
            c == '-' ||
            c == '_' ||
            c == '.' ||
            c == '~')
        {
            encoded += static_cast<char>(c);
        }
        else
        {
            encoded += '%';
            encoded += hex[(c >> 4) & 0x0F];
            encoded += hex[c & 0x0F];
        }
    }

    return encoded;
}

// ------------------------------------------------------------
// API KEY
// ------------------------------------------------------------
string APIManager::getApiKey()
{
    const char* key = getenv("WEATHER_API_KEY");

    if (key != nullptr && key[0] != '\0')
    {
        return string(key);
    }

    return "";
}

// ------------------------------------------------------------
// CURRENT WEATHER URL
// ------------------------------------------------------------
string APIManager::buildURL(string city)
{
    return "https://api.weatherapi.com/v1/current.json?key=" +
           getApiKey() +
           "&q=" +
           urlEncode(city);
}

// ------------------------------------------------------------
// LOCATION SEARCH URL
// ------------------------------------------------------------
string APIManager::buildSearchURL(string query)
{
    return "https://api.weatherapi.com/v1/search.json?key=" +
           getApiKey() +
           "&q=" +
           urlEncode(query);
}

// ------------------------------------------------------------
// LOCATION SEARCH API
// ------------------------------------------------------------
string APIManager::searchLocations(string query)
{
    string response;

    if (query.empty())
    {
        return "";
    }

    if (getApiKey().empty())
    {
        cout << "API key is not configured. Set WEATHER_API_KEY before running.\n";
        return "";
    }

    CURL* curl = curl_easy_init();

    if (curl == nullptr)
    {
        return "";
    }

    string url = buildSearchURL(query);

    curl_easy_setopt(
        curl,
        CURLOPT_URL,
        url.c_str()
    );

    curl_easy_setopt(
        curl,
        CURLOPT_WRITEFUNCTION,
        SearchWriteCallback
    );

    curl_easy_setopt(
        curl,
        CURLOPT_WRITEDATA,
        &response
    );

    // Follow redirects if the API endpoint redirects.
    curl_easy_setopt(
        curl,
        CURLOPT_FOLLOWLOCATION,
        1L
    );

    // Prevent the program from waiting forever.
    curl_easy_setopt(
        curl,
        CURLOPT_CONNECTTIMEOUT,
        10L
    );

    curl_easy_setopt(
        curl,
        CURLOPT_TIMEOUT,
        20L
    );

    // Identify the application to the server.
    curl_easy_setopt(
        curl,
        CURLOPT_USERAGENT,
        "WeatherDataManagementSystem/1.0"
    );

    // SSL verification.
    curl_easy_setopt(
        curl,
        CURLOPT_SSL_VERIFYPEER,
        1L
    );

    curl_easy_setopt(
        curl,
        CURLOPT_SSL_VERIFYHOST,
        2L
    );

    CURLcode result = curl_easy_perform(curl);

    long statusCode = 0;

    curl_easy_getinfo(
        curl,
        CURLINFO_RESPONSE_CODE,
        &statusCode
    );

    curl_easy_cleanup(curl);

    if (result != CURLE_OK)
    {
        cout << "CURL Error: " << curl_easy_strerror(result) << "\n";
        return "";
    }

    if (statusCode != 200)
    {
        cout << "Weather API HTTP Status: " << statusCode << "\n";
        if (!response.empty())
        {
            cout << "API Response: " << response << "\n";
        }
        return "";
    }

    return response;
}
