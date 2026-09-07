#ifndef WEATHERDATAPROCESSOR_H
#define WEATHERDATAPROCESSOR_H

#include <vector>
#include <string>

using namespace std;

class WeatherDataProcessor
{
private:

    vector<float> temperatures;
    vector<float> humidities;
    vector<float> pressures;

public:

    // Load previously saved weather data
    void loadStoredWeatherData();

    // Temperature functions
    void calculateAverageTemperature();
    void findMaximumTemperature();
    void findMinimumTemperature();

    // Humidity functions
    void calculateAverageHumidity();
    void findMaximumHumidity();
    void findMinimumHumidity();

    // Pressure function
    void calculateAveragePressure();

    // Record information
    void showTotalRecords();

    // Complete report
    void generateWeatherReport();
};

#endif