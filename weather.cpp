#include <iostream>
#include <string>
#include <fstream>
#include <ctime>
#include <chrono>
#include <thread>
#include <mutex>

#include <curl/curl.h>
#include <nlohmann/json.hpp>

#include "APIManager.h"
#include "weather.h"

using namespace std;
using json = nlohmann::json;


// ============================================
// LAST WEATHER DATA
// ============================================

float lastTemperature = 0.0f;
float lastHumidity = 0.0f;
float lastWindSpeed = 0.0f;
float lastPressure = 0.0f;

string lastCondition = "";
string lastLocation = "";

bool weatherDataAvailable = false;

static mutex weatherFileMutex;


// ============================================
// CURL CALLBACK
// ============================================

size_t WriteCallback(
    void* contents,
    size_t size,
    size_t nmemb,
    string* output)
{
    size_t totalSize = size * nmemb;

    output->append(
        static_cast<char*>(contents),
        totalSize
    );

    return totalSize;
}


// ============================================
// GET LAST TEMPERATURE
// ============================================

float getLastTemperature()
{
    return lastTemperature;
}


// ============================================
// GET LAST HUMIDITY
// ============================================

float getLastHumidity()
{
    return lastHumidity;
}


// ============================================
// GET LAST WIND SPEED
// ============================================

float getLastWindSpeed()
{
    return lastWindSpeed;
}


// ============================================
// GET LAST PRESSURE
// ============================================

float getLastPressure()
{
    return lastPressure;
}


// ============================================
// GET LAST CONDITION
// ============================================

string getLastCondition()
{
    return lastCondition;
}


// ============================================
// GET LAST LOCATION
// ============================================

string getLastLocation()
{
    return lastLocation;
}


// ============================================
// CHECK WEATHER DATA
// ============================================

bool isWeatherDataAvailable()
{
    return weatherDataAvailable;
}


// ============================================
// FETCH WEATHER
// ============================================

string fetchWeather(string city)
{
    string response;

    APIManager api;

    if (api.getApiKey().empty())
    {
        weatherDataAvailable = false;
        return "Error: WEATHER_API_KEY is not configured.";
    }

    string url = api.buildURL(city);


    cout << "\nGenerated URL: "
         << url
         << endl;


    cout << "\n----------------------------------------\n";

    cout << "Connecting to Weather API...\n";

    cout << "----------------------------------------\n";


    // ========================================
    // INITIALIZE CURL
    // ========================================

    CURL* curl = curl_easy_init();


    if (curl == nullptr)
    {
        weatherDataAvailable = false;

        return "Error: Unable to initialize CURL.";
    }


    // ========================================
    // CURL SETTINGS
    // ========================================

    curl_easy_setopt(
        curl,
        CURLOPT_URL,
        url.c_str()
    );


    curl_easy_setopt(
        curl,
        CURLOPT_WRITEFUNCTION,
        WriteCallback
    );


    curl_easy_setopt(
        curl,
        CURLOPT_WRITEDATA,
        &response
    );


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

    curl_easy_setopt(
        curl,
        CURLOPT_CONNECTTIMEOUT,
        10L
    );

    curl_easy_setopt(
        curl,
        CURLOPT_TIMEOUT,
        30L
    );


    // ========================================
    // API REQUEST WITH RETRY
    // ========================================

    CURLcode res = CURLE_FAILED_INIT;

    int attempts = 0;
    double lastResponseTimeMs = 0.0;


    while (attempts < 3)
    {
        response.clear();

        attempts++;

        auto requestStart = chrono::steady_clock::now();
        res = curl_easy_perform(curl);
        auto requestEnd = chrono::steady_clock::now();

        double responseTimeMs =
            chrono::duration<double, milli>(requestEnd - requestStart).count();

        lastResponseTimeMs = responseTimeMs;

        cout << "API Response Time : "
             << responseTimeMs
             << " ms"
             << endl;

        if (res == CURLE_OK)
        {
            break;
        }

        cout << "Retry Attempt : "
             << attempts
             << endl;

        if (attempts < 3)
        {
            cout << "Retrying after 5 seconds..." << endl;
            this_thread::sleep_for(chrono::seconds(5));
        }
    }


    // ========================================
    // GET HTTP STATUS
    // ========================================

    long statusCode = 0;


    curl_easy_getinfo(
        curl,
        CURLINFO_RESPONSE_CODE,
        &statusCode
    );


    cout << "\nAPI Status : "
         << statusCode
         << endl;

    {
        ofstream performanceLog("api_performance.log", ios::app);
        if (performanceLog.is_open())
        {
            performanceLog << city
                           << " | status=" << statusCode
                           << " | attempt=" << attempts
                           << " | response_time_ms=" << lastResponseTimeMs
                           << endl;
        }
    }


    // ========================================
    // CURL ERROR
    // ========================================

    if (res != CURLE_OK)
    {
        weatherDataAvailable = false;


        ofstream errorLog(
            "error.log",
            ios::app
        );


        errorLog << city
                 << " | CURL Error | "
                 << curl_easy_strerror(res)
                 << endl;


        errorLog.close();


        curl_easy_cleanup(curl);


        return "Error Fetching Weather Data";
    }


    // ========================================
    // API STATUS CHECK
    // ========================================

    if (statusCode != 200)
    {
        weatherDataAvailable = false;


        ofstream errorLog(
            "error.log",
            ios::app
        );


        errorLog << city
                 << " | API Error | Status Code: "
                 << statusCode
                 << endl;


        errorLog.close();


        curl_easy_cleanup(curl);


        return "Failed To Retrieve Weather Data";
    }


    cout << "\nWeather Data Retrieved Successfully\n";


    curl_easy_cleanup(curl);


    // ========================================
    // PARSE JSON
    // ========================================

    try
    {
        json data = json::parse(response);


        // ====================================
        // LOCATION
        // ====================================

        string location =
            data["location"]["name"].get<string>();


        string region =
            data["location"]["region"].get<string>();


        string country =
            data["location"]["country"].get<string>();


        // ====================================
        // WEATHER DATA
        // ====================================

        float temp =
            data["current"]["temp_c"].get<float>();


        float humidity =
            data["current"]["humidity"].get<float>();


        float wind =
            data["current"]["wind_kph"].get<float>();


        float pressure =
            data["current"]["pressure_mb"].get<float>();


        string condition =
            data["current"]["condition"]["text"]
                .get<string>();


        // ====================================
        // STORE LATEST WEATHER DATA
        // ====================================

        lastTemperature = temp;

        lastHumidity = humidity;

        lastWindSpeed = wind;

        lastPressure = pressure;

        lastCondition = condition;

        lastLocation = location;

        weatherDataAvailable = true;


        // ====================================
        // SAVE TO CSV
        // ====================================

        lock_guard<mutex> storageLock(weatherFileMutex);

        ofstream file(
            "weather_storage.csv",
            ios::app
        );


        if (file.is_open())
        {
            char buffer[100];

            time_t now = time(nullptr);


            strftime(
                buffer,
                sizeof(buffer),
                "%Y-%m-%d %H:%M:%S",
                localtime(&now)
            );


            file << location << ","
                 << temp << ","
                 << humidity << ","
                 << pressure << ","
                 << buffer
                 << endl;


            file.close();


            cout << "\nWeather Data Automatically Saved"
                 << " To weather_storage.csv\n";
        }
        else
        {
            cout << "\nWarning: Unable To Open "
                 << "weather_storage.csv\n";
        }


        // ====================================
        // CREATE DASHBOARD
        // ====================================

        string dashboard;


        dashboard +=
            "\n========== WEATHER DASHBOARD ==========\n";


        dashboard +=
            "Location    : "
            + location
            + "\n";


        dashboard +=
            "Region      : "
            + region
            + "\n";


        dashboard +=
            "Country     : "
            + country
            + "\n";


        dashboard +=
            "Temperature : "
            + to_string(temp)
            + " C\n";


        dashboard +=
            "Humidity    : "
            + to_string(humidity)
            + "%\n";


        dashboard +=
            "Condition   : "
            + condition
            + "\n";


        dashboard +=
            "Wind Speed  : "
            + to_string(wind)
            + " km/h\n";


        dashboard +=
            "Pressure    : "
            + to_string(pressure)
            + " mb\n";


        dashboard +=
            "=======================================\n";


        // ====================================
        // API LOG
        // ====================================

        ofstream apiLog(
            "api.log",
            ios::app
        );


        apiLog << city
               << " | Success"
               << endl;


        apiLog.close();


        return dashboard;
    }


    // ========================================
    // JSON ERROR
    // ========================================

    catch (const json::exception& e)
    {
        weatherDataAvailable = false;


        ofstream errorLog(
            "error.log",
            ios::app
        );


        errorLog << city
                 << " | JSON Parse Error | "
                 << e.what()
                 << endl;


        errorLog.close();


        return "Error: Invalid JSON response from Weather API.";
    }
}