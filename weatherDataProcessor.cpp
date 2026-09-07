#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>

#include "weatherDataProcessor.h"

using namespace std;


// ============================================
// LOAD STORED WEATHER DATA
// ============================================

void WeatherDataProcessor::loadStoredWeatherData()
{
    // Clear old data first.
    // This prevents duplicate records when
    // the CSV file is loaded more than once.

    temperatures.clear();
    humidities.clear();
    pressures.clear();


    ifstream file("weather_storage.csv");

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


        stringstream ss(line);

        string city;
        string tempString;
        string humidityString;
        string pressureString;
        string dateTime;


        // ========================================
        // CSV FORMAT
        // ========================================
        //
        // city,temperature,humidity,pressure,date
        //
        // Example:
        //
        // Chennai,32,70,1005,2026-08-27 22:30:00
        //


        getline(ss, city, ',');

        getline(ss, tempString, ',');

        getline(ss, humidityString, ',');

        getline(ss, pressureString, ',');

        getline(ss, dateTime);


        try
        {
            float temp = stof(tempString);

            float humidity = stof(humidityString);

            float pressure = stof(pressureString);


            // ====================================
            // VALIDATE DATA
            // ====================================

            if (temp < -100 || temp > 70)
            {
                continue;
            }

            if (humidity < 0 || humidity > 100)
            {
                continue;
            }

            if (pressure <= 0)
            {
                continue;
            }


            // ====================================
            // STORE DATA
            // ====================================

            temperatures.push_back(temp);

            humidities.push_back(humidity);

            pressures.push_back(pressure);
        }
        catch (...)
        {
            // Ignore invalid CSV records
            continue;
        }
    }


    file.close();
}


// ============================================
// AVERAGE TEMPERATURE
// ============================================

void WeatherDataProcessor::calculateAverageTemperature()
{
    if (temperatures.empty())
    {
        cout << "No Data Available\n";
        return;
    }


    float sum = 0;


    for (float t : temperatures)
    {
        sum += t;
    }


    cout << "\nAverage Temperature: "
         << sum / temperatures.size()
         << " °C\n";
}


// ============================================
// MAXIMUM TEMPERATURE
// ============================================

void WeatherDataProcessor::findMaximumTemperature()
{
    if (temperatures.empty())
    {
        cout << "No Data Available\n";
        return;
    }


    float maxTemp = temperatures[0];


    for (float t : temperatures)
    {
        if (t > maxTemp)
        {
            maxTemp = t;
        }
    }


    cout << "\nMaximum Temperature: "
         << maxTemp
         << " °C\n";
}


// ============================================
// MINIMUM TEMPERATURE
// ============================================

void WeatherDataProcessor::findMinimumTemperature()
{
    if (temperatures.empty())
    {
        cout << "No Data Available\n";
        return;
    }


    float minTemp = temperatures[0];


    for (float t : temperatures)
    {
        if (t < minTemp)
        {
            minTemp = t;
        }
    }


    cout << "\nMinimum Temperature: "
         << minTemp
         << " °C\n";
}


// ============================================
// AVERAGE HUMIDITY
// ============================================

void WeatherDataProcessor::calculateAverageHumidity()
{
    if (humidities.empty())
    {
        cout << "No Data Available\n";
        return;
    }


    float sum = 0;


    for (float h : humidities)
    {
        sum += h;
    }


    cout << "\nAverage Humidity: "
         << sum / humidities.size()
         << " %\n";
}


// ============================================
// AVERAGE PRESSURE
// ============================================

void WeatherDataProcessor::calculateAveragePressure()
{
    if (pressures.empty())
    {
        cout << "No Data Available\n";
        return;
    }


    float sum = 0;


    for (float p : pressures)
    {
        sum += p;
    }


    cout << "\nAverage Pressure: "
         << sum / pressures.size()
         << " mb\n";
}


// ============================================
// MAXIMUM HUMIDITY
// ============================================

void WeatherDataProcessor::findMaximumHumidity()
{
    if (humidities.empty())
    {
        cout << "No Data Available\n";
        return;
    }


    float maxHumidity = humidities[0];


    for (float h : humidities)
    {
        if (h > maxHumidity)
        {
            maxHumidity = h;
        }
    }


    cout << "\nMaximum Humidity: "
         << maxHumidity
         << " %\n";
}


// ============================================
// MINIMUM HUMIDITY
// ============================================

void WeatherDataProcessor::findMinimumHumidity()
{
    if (humidities.empty())
    {
        cout << "No Data Available\n";
        return;
    }


    float minHumidity = humidities[0];


    for (float h : humidities)
    {
        if (h < minHumidity)
        {
            minHumidity = h;
        }
    }


    cout << "\nMinimum Humidity: "
         << minHumidity
         << " %\n";
}


// ============================================
// TOTAL RECORDS
// ============================================

void WeatherDataProcessor::showTotalRecords()
{
    cout << "\nTotal Records: "
         << temperatures.size()
         << endl;
}


// ============================================
// WEATHER REPORT
// ============================================

void WeatherDataProcessor::generateWeatherReport()
{
    // Refresh data from CSV before
    // generating the report.

    loadStoredWeatherData();


    cout << "\n========== WEATHER REPORT ==========\n";


    showTotalRecords();


    calculateAverageTemperature();


    findMaximumTemperature();


    findMinimumTemperature();


    calculateAverageHumidity();


    findMaximumHumidity();


    findMinimumHumidity();


    calculateAveragePressure();


    cout << "====================================\n";
}