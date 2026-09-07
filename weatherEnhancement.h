#ifndef WEATHER_ENHANCEMENT_H
#define WEATHER_ENHANCEMENT_H

#include <string>

class WeatherEnhancementManager
{
public:
    // FR-01: user authentication
    bool login();

    // FR-04: forecast data, FR-10: smart alerts, FR-12: insights,
    // FR-13/14: report and export, FR-09: risk assessment
    void showWeatherIntelligence();

    // FR-15: API performance monitoring / FR-16: logging
    void showApiPerformance();

    // FR-20: retry/fault handling demonstration
    void testApiWithRetry();

    // Concurrency / automatic weather monitoring
    void automaticWeatherRefresh();
};

#endif
