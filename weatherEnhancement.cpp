#include <curl/curl.h>
#include <nlohmann/json.hpp>

#include "weatherEnhancement.h"
#include "APIManager.h"
#include "weather.h"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

using namespace std;
using json = nlohmann::json;

namespace
{
    mutex logMutex;
    mutex storageMutex;

    struct WeatherSnapshot
    {
        string location;
        string region;
        string country;
        double latitude = 0.0;
        double longitude = 0.0;

        double temperature = 0.0;
        double feelsLike = 0.0;
        double humidity = 0.0;
        double windKph = 0.0;
        double pressure = 0.0;
        double precipitation = 0.0;
        double uv = 0.0;
        string condition;

        string sunrise;
        string sunset;

        json forecast;
        json alerts;
    };

    string nowString()
    {
        time_t now = time(nullptr);
        tm localTime{};
#if defined(_WIN32)
        localtime_s(&localTime, &now);
#else
        localtime_r(&now, &localTime);
#endif
        char buffer[32];
        strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &localTime);
        return buffer;
    }

    void logLine(const string& fileName, const string& message)
    {
        lock_guard<mutex> lock(logMutex);
        ofstream file(fileName, ios::app);
        if (file.is_open())
        {
            file << nowString() << " | " << message << '\n';
        }
    }

    size_t writeCallback(void* contents, size_t size, size_t nmemb, string* output)
    {
        size_t total = size * nmemb;
        if (output != nullptr)
        {
            output->append(static_cast<char*>(contents), total);
        }
        return total;
    }

    string urlEncode(const string& value)
    {
        const char* hex = "0123456789ABCDEF";
        string result;
        for (unsigned char c : value)
        {
            if ((c >= 'a' && c <= 'z') ||
                (c >= 'A' && c <= 'Z') ||
                (c >= '0' && c <= '9') ||
                c == '-' || c == '_' || c == '.' || c == '~')
            {
                result += static_cast<char>(c);
            }
            else
            {
                result += '%';
                result += hex[(c >> 4) & 0x0F];
                result += hex[c & 0x0F];
            }
        }
        return result;
    }

    string buildForecastUrl(const string& query)
    {
        APIManager api;
        return "https://api.weatherapi.com/v1/forecast.json?key=" +
               api.getApiKey() +
               "&q=" + urlEncode(query) +
               "&days=7&aqi=no&alerts=yes";
    }

    bool requestJsonWithRetry(const string& url, string& response,
                              long& statusCode, double& elapsedMs,
                              int maxAttempts = 3)
    {
        response.clear();
        statusCode = 0;
        elapsedMs = 0.0;

        CURL* curl = curl_easy_init();
        if (curl == nullptr)
        {
            return false;
        }

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);

        bool success = false;

        for (int attempt = 1; attempt <= maxAttempts; ++attempt)
        {
            response.clear();
            auto start = chrono::steady_clock::now();
            CURLcode result = curl_easy_perform(curl);
            auto end = chrono::steady_clock::now();

            elapsedMs = chrono::duration<double, milli>(end - start).count();
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &statusCode);

            logLine("api_performance.log",
                    "attempt=" + to_string(attempt) +
                    " status=" + to_string(statusCode) +
                    " response_time_ms=" + to_string(elapsedMs));

            if (result == CURLE_OK && statusCode == 200)
            {
                success = true;
                break;
            }

            logLine("error.log",
                    "API retry attempt=" + to_string(attempt) +
                    " curl=" + string(curl_easy_strerror(result)) +
                    " status=" + to_string(statusCode));

            if (attempt < maxAttempts)
            {
                // Requirement document specifies a 5-second retry interval.
                this_thread::sleep_for(chrono::seconds(5));
            }
        }

        curl_easy_cleanup(curl);
        return success;
    }

    // Compatibility helpers for nlohmann-json 2.1.1.
    bool hasJsonKey(const json& object, const string& key)
    {
        return object.is_object() && object.find(key) != object.end();
    }

    template <typename T>
    T jsonValue(const json& object, const string& key, const T& fallback)
    {
        if (!object.is_object())
        {
            return fallback;
        }

        json::const_iterator it = object.find(key);

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

    bool parseSnapshot(const json& data, WeatherSnapshot& w)
    {
        try
        {
            w.location = jsonValue<string>(data["location"], "name", "");
            w.region = jsonValue<string>(data["location"], "region", "");
            w.country = jsonValue<string>(data["location"], "country", "");
            w.latitude = jsonValue<double>(data["location"], "lat", 0.0);
            w.longitude = jsonValue<double>(data["location"], "lon", 0.0);

            const auto& current = data["current"];
            w.temperature = jsonValue<double>(current, "temp_c", 0.0);
            w.feelsLike = jsonValue<double>(current, "feelslike_c", 0.0);
            w.humidity = jsonValue<double>(current, "humidity", 0.0);
            w.windKph = jsonValue<double>(current, "wind_kph", 0.0);
            w.pressure = jsonValue<double>(current, "pressure_mb", 0.0);
            w.precipitation = jsonValue<double>(current, "precip_mm", 0.0);
            w.uv = jsonValue<double>(current, "uv", 0.0);
            w.condition = jsonValue<string>(current["condition"], "text", "Unknown");

            w.forecast = hasJsonKey(data, "forecast") ? data["forecast"] : json();
            w.alerts = hasJsonKey(data, "alerts") ? data["alerts"] : json();

            if (hasJsonKey(w.forecast, "forecastday") &&
                w.forecast["forecastday"].is_array() &&
                !w.forecast["forecastday"].empty())
            {
                const auto& firstDay = w.forecast["forecastday"][0];
                if (hasJsonKey(firstDay, "astro"))
                {
                    w.sunrise = jsonValue<string>(firstDay["astro"], "sunrise", "Unknown");
                    w.sunset = jsonValue<string>(firstDay["astro"], "sunset", "Unknown");
                }
            }

            return !w.location.empty();
        }
        catch (const json::exception&)
        {
            return false;
        }
    }

    string severity(double value, double threshold, bool highIsBad)
    {
        double distance = highIsBad ? value - threshold : threshold - value;
        if (distance >= 10.0)
            return "HIGH";
        if (distance >= 5.0)
            return "MEDIUM";
        return "LOW";
    }

    void printAlert(const string& name, const string& detail, const string& level)
    {
        cout << "\n[" << level << "] " << name << "\n";
        cout << "    " << detail << '\n';
    }

    void generateSmartAlerts(const WeatherSnapshot& w, bool printOutput)
    {
        bool any = false;

        // Thresholds taken from the functionality document.
        if (w.temperature > 40.0)
        {
            any = true;
            printAlert("Heat Alert",
                       "Temperature = " + to_string(w.temperature) + " C (threshold > 40 C)",
                       severity(w.temperature, 40.0, true));
        }

        if (w.temperature < 10.0)
        {
            any = true;
            printAlert("Temperature Alert",
                       "Temperature = " + to_string(w.temperature) + " C (threshold < 10 C)",
                       severity(w.temperature, 10.0, false));
        }

        if (w.precipitation > 40.0)
        {
            any = true;
            printAlert("Heavy Rain Alert",
                       "Rainfall = " + to_string(w.precipitation) + " mm (threshold > 40 mm)",
                       severity(w.precipitation, 40.0, true));
        }

        if (w.windKph > 40.0)
        {
            any = true;
            printAlert("Strong Wind Alert",
                       "Wind speed = " + to_string(w.windKph) + " km/h (threshold > 40 km/h)",
                       severity(w.windKph, 40.0, true));
        }

        if (w.humidity > 80.0)
        {
            any = true;
            printAlert("High Humidity Alert",
                       "Humidity = " + to_string(w.humidity) + "% (threshold > 80%)",
                       severity(w.humidity, 80.0, true));
        }

        if (!any && printOutput)
        {
            cout << "\nNo Smart Weather Alerts Triggered.\n";
        }

        if (any)
        {
            logLine("alert.log", w.location + " | Smart alert(s) generated");
        }
    }

    void showForecast(const WeatherSnapshot& w)
    {
        cout << "\n========== HOURLY FORECAST ==========" << '\n';

        if (!hasJsonKey(w.forecast, "forecastday"))
        {
            cout << "Forecast data unavailable.\n";
            return;
        }

        int shown = 0;
        for (const auto& day : w.forecast["forecastday"])
        {
            if (!hasJsonKey(day, "hour"))
                continue;

            for (const auto& hour : day["hour"])
            {
                string time = jsonValue<string>(hour, "time", "");
                double temp = jsonValue<double>(hour, "temp_c", 0.0);
                double rainChance = jsonValue<double>(hour, "chance_of_rain", 0.0);
                double wind = jsonValue<double>(hour, "wind_kph", 0.0);
                string condition = jsonValue<string>(hour["condition"], "text", "Unknown");

                cout << time
                     << " | " << fixed << setprecision(1) << temp << " C"
                     << " | Rain: " << rainChance << "%"
                     << " | Wind: " << wind << " km/h"
                     << " | " << condition << '\n';

                ++shown;
                if (shown >= 12)
                    return;
            }
        }
    }

    void showSevenDayForecast(const WeatherSnapshot& w)
    {
        cout << "\n========== 7-DAY FORECAST ==========" << '\n';

        if (!hasJsonKey(w.forecast, "forecastday"))
        {
            cout << "Forecast data unavailable.\n";
            return;
        }

        for (const auto& day : w.forecast["forecastday"])
        {
            string date = jsonValue<string>(day, "date", "");
            const auto& d = day["day"];

            cout << date
                 << " | Min: " << fixed << setprecision(1) << jsonValue<double>(d, "mintemp_c", 0.0) << " C"
                 << " | Max: " << jsonValue<double>(d, "maxtemp_c", 0.0) << " C"
                 << " | Avg: " << jsonValue<double>(d, "avgtemp_c", 0.0) << " C"
                 << " | Rain: " << jsonValue<double>(d, "daily_chance_of_rain", 0.0) << "%"
                 << " | UV: " << jsonValue<double>(d, "uv", 0.0)
                 << " | " << jsonValue<string>(d["condition"], "text", "Unknown")
                 << '\n';
        }
    }

    void showApiAlerts(const WeatherSnapshot& w)
    {
        cout << "\n========== WEATHER API ALERTS ==========" << '\n';

        if (!hasJsonKey(w.alerts, "alert") || !w.alerts["alert"].is_array() ||
            w.alerts["alert"].empty())
        {
            cout << "No alerts supplied by the Weather API.\n";
            return;
        }

        for (const auto& alert : w.alerts["alert"])
        {
            cout << "\nHeadline : " << jsonValue<string>(alert, "headline", "Unknown") << '\n';
            cout << "Event    : " << jsonValue<string>(alert, "event", "Unknown") << '\n';
            cout << "Severity : " << jsonValue<string>(alert, "severity", "Unknown") << '\n';
            cout << "Effective: " << jsonValue<string>(alert, "effective", "Unknown") << '\n';
            cout << "Expires  : " << jsonValue<string>(alert, "expires", "Unknown") << '\n';
            cout << "Description: " << jsonValue<string>(alert, "desc", "No description") << '\n';
        }
    }

    void showRiskAssessment(const WeatherSnapshot& w)
    {
        int riskScore = 0;

        if (w.temperature > 40.0 || w.temperature < 10.0) riskScore += 3;
        else if (w.temperature > 35.0 || w.temperature < 15.0) riskScore += 1;

        if (w.precipitation > 40.0) riskScore += 3;
        else if (w.precipitation > 20.0) riskScore += 1;

        if (w.windKph > 40.0) riskScore += 3;
        else if (w.windKph > 30.0) riskScore += 1;

        if (w.humidity > 80.0) riskScore += 2;

        string level = "LOW";
        if (riskScore >= 6) level = "HIGH";
        else if (riskScore >= 3) level = "MEDIUM";

        cout << "\n========== WEATHER RISK ASSESSMENT ==========" << '\n';
        cout << "Risk Score : " << riskScore << '\n';
        cout << "Risk Level : " << level << '\n';
    }

    void showInsights(const WeatherSnapshot& w)
    {
        cout << "\n========== WEATHER INSIGHTS ==========" << '\n';
        cout << "Feels Like       : " << w.feelsLike << " C\n";
        cout << "UV Index         : " << w.uv << '\n';
        cout << "Precipitation    : " << w.precipitation << " mm\n";
        cout << "Sunrise          : " << w.sunrise << '\n';
        cout << "Sunset           : " << w.sunset << '\n';

        if (w.feelsLike > w.temperature + 3.0)
            cout << "Insight           : It feels noticeably warmer than the actual temperature.\n";
        else if (w.feelsLike < w.temperature - 3.0)
            cout << "Insight           : It feels noticeably cooler than the actual temperature.\n";
        else
            cout << "Insight           : Feels-like temperature is close to the actual temperature.\n";

        if (w.uv >= 8.0)
            cout << "UV Advice         : Very high UV conditions; use strong sun protection.\n";
        else if (w.uv >= 6.0)
            cout << "UV Advice         : High UV conditions; sun protection is recommended.\n";
        else
            cout << "UV Advice         : UV level is not in the high-alert range.\n";

        if (w.precipitation > 0.0)
            cout << "Rain Insight      : Measurable precipitation is currently reported.\n";
        else
            cout << "Rain Insight      : No current precipitation is reported.\n";
    }

    void exportReport(const WeatherSnapshot& w)
    {
        ofstream file("weather_report.csv");
        if (!file.is_open())
        {
            cout << "\nUnable to create weather_report.csv\n";
            return;
        }

        file << "Section,Date/Time,Location,Temperature C,Feels Like C,Humidity %,Wind km/h,Pressure mb,Precipitation mm,UV,Condition,Sunrise,Sunset\n";
        file << "Current," << nowString() << ','
             << w.location << ',' << w.temperature << ',' << w.feelsLike << ','
             << w.humidity << ',' << w.windKph << ',' << w.pressure << ','
             << w.precipitation << ',' << w.uv << ',' << w.condition << ','
             << w.sunrise << ',' << w.sunset << '\n';

        if (hasJsonKey(w.forecast, "forecastday"))
        {
            for (const auto& day : w.forecast["forecastday"])
            {
                const auto& d = day["day"];
                file << "Forecast," << jsonValue<string>(day, "date", "") << ','
                     << w.location << ','
                     << jsonValue<double>(d, "avgtemp_c", 0.0) << ',' << ',' << ',' << ',' << ',' << ','
                     << jsonValue<double>(d, "uv", 0.0) << ','
                     << jsonValue<string>(d["condition"], "text", "") << ',' << ',' << '\n';
            }
        }

        file.close();
        cout << "\nReport exported to weather_report.csv\n";
        logLine("system.log", w.location + " | Weather report exported");
    }

    void showHistoricalTrend()
    {
        ifstream file("weather_storage.csv");
        if (!file.is_open())
        {
            cout << "\nNo historical weather data available.\n";
            return;
        }

        struct Record { double temp, humidity, pressure; };
        vector<Record> records;
        string line;

        while (getline(file, line))
        {
            stringstream ss(line);
            string city, temp, humidity, pressure, timestamp;
            if (!getline(ss, city, ',')) continue;
            if (!getline(ss, temp, ',')) continue;
            if (!getline(ss, humidity, ',')) continue;
            if (!getline(ss, pressure, ',')) continue;
            getline(ss, timestamp);

            try
            {
                records.push_back({stod(temp), stod(humidity), stod(pressure)});
            }
            catch (...) {}
        }

        if (records.size() < 2)
        {
            cout << "\nAt least two historical records are required for trend analysis.\n";
            return;
        }

        const auto& first = records.front();
        const auto& last = records.back();

        auto trend = [](double a, double b)
        {
            if (b > a + 0.01) return string("RISING");
            if (b < a - 0.01) return string("FALLING");
            return string("STABLE");
        };

        cout << "\n========== HISTORICAL TREND ANALYSIS ==========" << '\n';
        cout << "Temperature : " << trend(first.temp, last.temp)
             << " (" << first.temp << " -> " << last.temp << " C)\n";
        cout << "Humidity    : " << trend(first.humidity, last.humidity)
             << " (" << first.humidity << " -> " << last.humidity << " %)\n";
        cout << "Pressure    : " << trend(first.pressure, last.pressure)
             << " (" << first.pressure << " -> " << last.pressure << " mb)\n";
    }

    bool fetchForecast(const string& query, WeatherSnapshot& snapshot)
    {
        string response;
        long status = 0;
        double elapsed = 0.0;

        APIManager api;
        if (api.getApiKey().empty())
        {
            cout << "\nAPI key is not configured. Set WEATHER_API_KEY before running.\n";
            logLine("error.log", query + " | Forecast skipped | API key not configured");
            return false;
        }

        string url = buildForecastUrl(query);
        cout << "\nRequesting forecast data...\n";

        if (!requestJsonWithRetry(url, response, status, elapsed, 3))
        {
            cout << "\nForecast request failed after 3 attempts.\n";
            logLine("error.log", query + " | Forecast API failed after retries");
            return false;
        }

        try
        {
            json data = json::parse(response);
            if (!parseSnapshot(data, snapshot))
            {
                cout << "\nInvalid forecast JSON data.\n";
                logLine("error.log", query + " | Forecast JSON parse/structure error");
                return false;
            }
        }
        catch (const json::exception& e)
        {
            cout << "\nJSON Error: " << e.what() << '\n';
            logLine("error.log", query + " | JSON Error | " + string(e.what()));
            return false;
        }

        logLine("api.log", query + " | Forecast success | response_ms=" + to_string(elapsed));
        return true;
    }

    void appendRefreshRecord(const WeatherSnapshot& w)
    {
        lock_guard<mutex> lock(storageMutex);
        ofstream file("weather_storage.csv", ios::app);
        if (file.is_open())
        {
            file << w.location << ','
                 << w.temperature << ','
                 << w.humidity << ','
                 << w.pressure << ','
                 << nowString() << '\n';
        }
    }
}

bool WeatherEnhancementManager::login()
{
    while (true)
    {
        cout << "\n========== USER AUTHENTICATION ==========\n";
        cout << "1. Login\n";
        cout << "2. Register\n";
        cout << "0. Exit\n";
        cout << "\nEnter Choice: ";

        int choice = -1;

        if (!(cin >> choice))
        {
            cin.clear();
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            cout << "\nInvalid choice. Please enter 0, 1 or 2.\n";
            continue;
        }

        if (choice == 0)
        {
            return false;
        }

        if (choice == 2)
        {
            string username;
            string password;

            cout << "\n========== USER REGISTRATION ==========\n";
            cout << "Enter Username: ";
            cin >> username;
            cout << "Enter Password: ";
            cin >> password;

            if (username.empty() || password.empty())
            {
                cout << "\nUsername and password cannot be empty.\n";
                continue;
            }

            bool exists = false;
            ifstream users("users.csv");
            string line;

            while (getline(users, line))
            {
                string storedUser;
                string storedPassword;
                stringstream ss(line);

                getline(ss, storedUser, ',');
                getline(ss, storedPassword);

                if (storedUser == username)
                {
                    exists = true;
                    break;
                }
            }
            users.close();

            if (exists)
            {
                cout << "\nUsername already exists. Registration failed.\n";
                continue;
            }

            ofstream create("users.csv", ios::app);
            if (!create.is_open())
            {
                cout << "\nUnable to open users.csv.\n";
                logLine("error.log", "Registration failed | Cannot open users.csv");
                continue;
            }

            create << username << ',' << password << '\n';
            create.close();

            cout << "\nRegistration Successful!\n";
            cout << "Account stored in users.csv.\n";
            cout << "Returning to Login/Register menu...\n";
            logLine("system.log", username + " | Registration successful");
            continue;
        }

        if (choice == 1)
        {
            string username;
            string password;

            cout << "\n========== USER LOGIN ==========\n";
            cout << "Enter Username: ";
            cin >> username;
            cout << "Enter Password: ";
            cin >> password;

            ifstream file("users.csv");
            if (!file.is_open())
            {
                cout << "\nNo registered users found. Please register first.\n";
                logLine("error.log", "Login failed | users.csv not found");
                continue;
            }

            string line;
            bool authenticated = false;

            while (getline(file, line))
            {
                string storedUser;
                string storedPassword;
                stringstream ss(line);

                getline(ss, storedUser, ',');
                getline(ss, storedPassword);

                if (storedUser == username && storedPassword == password)
                {
                    authenticated = true;
                    break;
                }
            }

            file.close();

            if (authenticated)
            {
                cout << "\nLogin Successful!\n";
                cout << "Welcome, " << username << "!\n";
                logLine("system.log", username + " | Login successful");
                return true;
            }

            cout << "\nInvalid Username or Password.\n";
            cout << "Returning to Login/Register menu...\n";
            logLine("error.log", username + " | Login failed");
            continue;
        }

        cout << "\nInvalid choice. Please enter 0, 1 or 2.\n";
    }
}

void WeatherEnhancementManager::showWeatherIntelligence()
{
    string query;

    cout << "\nEnter City / Coordinates: ";
    cin.ignore(numeric_limits<streamsize>::max(), '\n');
    getline(cin, query);

    if (query.empty())
    {
        cout << "Search value cannot be empty.\n";
        return;
    }

    WeatherSnapshot snapshot;
    if (!fetchForecast(query, snapshot))
        return;

    cout << "\n========== CURRENT WEATHER DETAILS ==========" << '\n';
    cout << "Location       : " << snapshot.location << '\n';
    cout << "Region         : " << snapshot.region << '\n';
    cout << "Country        : " << snapshot.country << '\n';
    cout << "Temperature    : " << snapshot.temperature << " C\n";
    cout << "Feels Like     : " << snapshot.feelsLike << " C\n";
    cout << "Humidity       : " << snapshot.humidity << " %\n";
    cout << "Wind Speed     : " << snapshot.windKph << " km/h\n";
    cout << "Pressure       : " << snapshot.pressure << " mb\n";
    cout << "Precipitation  : " << snapshot.precipitation << " mm\n";
    cout << "UV Index       : " << snapshot.uv << '\n';
    cout << "Condition      : " << snapshot.condition << '\n';
    cout << "Sunrise        : " << snapshot.sunrise << '\n';
    cout << "Sunset         : " << snapshot.sunset << '\n';

    showForecast(snapshot);
    showSevenDayForecast(snapshot);
    showApiAlerts(snapshot);
    generateSmartAlerts(snapshot, true);
    showRiskAssessment(snapshot);
    showInsights(snapshot);
    showHistoricalTrend();

    cout << "\nExport this report to CSV? (1=Yes, 0=No): ";
    int exportChoice;
    if (cin >> exportChoice && exportChoice == 1)
    {
        exportReport(snapshot);
    }
}

void WeatherEnhancementManager::showApiPerformance()
{
    ifstream file("api_performance.log");
    if (!file.is_open())
    {
        cout << "\nNo API performance records available yet.\n";
        return;
    }

    string line;
    vector<double> times;
    int successes = 0;
    int attempts = 0;

    while (getline(file, line))
    {
        ++attempts;
        size_t pos = line.find("response_time_ms=");
        if (pos != string::npos)
        {
            try
            {
                times.push_back(stod(line.substr(pos + 17)));
            }
            catch (...) {}
        }
        if (line.find("status=200") != string::npos)
            ++successes;
    }

    cout << "\n========== API PERFORMANCE MONITOR ==========" << '\n';
    cout << "Requests/Attempts : " << attempts << '\n';
    cout << "Successful (200)  : " << successes << '\n';

    if (!times.empty())
    {
        double sum = 0.0;
        for (double t : times) sum += t;
        cout << fixed << setprecision(2);
        cout << "Average Response  : " << sum / times.size() << " ms\n";
        cout << "Last Response     : " << times.back() << " ms\n";
    }

    if (attempts > 0)
    {
        cout << "Availability       : "
             << fixed << setprecision(2)
             << (100.0 * successes / attempts)
             << "%\n";
    }
}

void WeatherEnhancementManager::testApiWithRetry()
{
    string city;
    cout << "\nEnter City: ";
    cin >> city;

    WeatherSnapshot snapshot;
    if (fetchForecast(city, snapshot))
    {
        cout << "\nAPI request succeeded. Retry mechanism is operational.\n";
    }
    else
    {
        cout << "\nAPI request failed after the configured retry attempts.\n";
    }
}

void WeatherEnhancementManager::automaticWeatherRefresh()
{
    string city;
    int intervalSeconds;
    int updateCount;

    cout << "\nEnter City: ";
    cin >> city;

    cout << "Refresh Interval (seconds, minimum 5): ";
    cin >> intervalSeconds;

    cout << "Number Of Updates: ";
    cin >> updateCount;

    if (intervalSeconds < 5 || updateCount <= 0 || updateCount > 100)
    {
        cout << "\nInvalid refresh settings.\n";
        return;
    }

    cout << "\nAutomatic monitoring started.\n";
    cout << "The monitoring worker runs in a separate thread.\n";

    thread worker([city, intervalSeconds, updateCount]()
    {
        for (int i = 1; i <= updateCount; ++i)
        {
            WeatherSnapshot snapshot;
            cout << "\n[Refresh " << i << "/" << updateCount << "]\n";

            if (fetchForecast(city, snapshot))
            {
                cout << "Location      : " << snapshot.location << '\n';
                cout << "Temperature   : " << snapshot.temperature << " C\n";
                cout << "Humidity      : " << snapshot.humidity << " %\n";
                cout << "Wind Speed    : " << snapshot.windKph << " km/h\n";
                cout << "Condition     : " << snapshot.condition << '\n';

                appendRefreshRecord(snapshot);
                generateSmartAlerts(snapshot, true);
                logLine("weather.log", snapshot.location + " | Automatic weather update");
            }

            if (i < updateCount)
            {
                this_thread::sleep_for(chrono::seconds(intervalSeconds));
            }
        }

        cout << "\nAutomatic monitoring completed.\n";
        logLine("system.log", city + " | Automatic monitoring completed");
    });

    worker.join();
}
