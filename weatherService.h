#ifndef WEATHERSERVICE_H
#define WEATHERSERVICE_H

#include <string>
using namespace std;

class WeatherService {
public:
    string fetchWeather(string city);
};

#endif
