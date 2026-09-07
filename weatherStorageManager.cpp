#include <iostream>
#include <fstream>
#include <ctime>
#include "weatherStorageManager.h"

using namespace std;

void WeatherStorageManager::saveWeatherData(
    string city, float temp, float humidity, float pressure) {

    ofstream file("weather_storage.csv", ios::app);

    if (file.is_open()) {
        char buffer[100];
        time_t now = time(nullptr);
        tm localTime{};

#if defined(_WIN32)
        if (localtime_s(&localTime, &now) != 0) {
            file.close();
            return;
        }
#else
        if (localtime_r(&now, &localTime) == nullptr) {
            file.close();
            return;
        }
#endif

        strftime(buffer, sizeof(buffer),
                 "%Y-%m-%d %H:%M:%S", &localTime);

        file << city << ","
             << temp << ","
             << humidity << ","
             << pressure << ","
             << buffer << endl;

        file.close();

        cout << "\nWeather Data Saved Successfully\n";
    }
}

void WeatherStorageManager::viewStoredData() {
    ifstream file("weather_storage.csv");
    string line;

    cout << "\n========== STORED WEATHER DATA ==========\n";

    while (getline(file, line)) {
        cout << line << endl;
    }

    file.close();
}

void WeatherStorageManager::backupData() {
    ifstream source("weather_storage.csv");
    ofstream backup("weather_backup.csv");

    string line;

    while (getline(source, line)) {
        backup << line << endl;
    }

    source.close();
    backup.close();

    cout << "\nBackup Created Successfully\n";
}

void WeatherStorageManager::clearStorage() {
    ofstream file("weather_storage.csv");
    file.close();

    cout << "\nStorage Cleared Successfully\n";
}
