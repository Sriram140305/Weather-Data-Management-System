#ifndef WEATHERSTORAGE_H
#define WEATHERSTORAGE_H

#include <string>
using namespace std;

class WeatherStorageManager {
public:
    void saveWeatherData(string city, float temp, float humidity, float pressure);
    void viewStoredData();
    void backupData();
    void clearStorage();
};

#endif
