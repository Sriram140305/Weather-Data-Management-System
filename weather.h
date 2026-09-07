#ifndef WEATHER_H
#define WEATHER_H

#include <string>

using namespace std;


// Fetch weather and return formatted dashboard
string fetchWeather(string city);


// Get the latest weather values fetched from API
float getLastTemperature();

float getLastHumidity();

float getLastWindSpeed();

float getLastPressure();

string getLastCondition();

string getLastLocation();


// Check whether valid weather data is available
bool isWeatherDataAvailable();

#endif