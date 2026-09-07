#include <iostream>
#include <string>
#include <limits>

#include "location.h"
#include "weatherStorageManager.h"
#include "weatherDataProcessor.h"
#include "weatherEnhancement.h"

using namespace std;

int main()
{
    // ============================================
    // CREATE OBJECTS
    // ============================================

    LocationManager lm;
    WeatherDataProcessor processor;
    WeatherStorageManager storage;
    WeatherEnhancementManager enhancement;

    // ============================================
    // USER LOGIN - FR-01
    // ============================================

    // login() manages the complete Login/Register flow.
    // It returns true only after successful authentication.
    // If the user chooses Exit, it returns false.
    if (!enhancement.login())
    {
        cout << "\nProgram terminated.\n";
        return 0;
    }

    // ============================================
    // LOAD SAVED DATA
    // ============================================

    lm.loadFavoriteCities();
    lm.loadSearchHistory();
    processor.loadStoredWeatherData();

    int choice = 0;

    // ============================================
    // MAIN MENU
    // ============================================

    do
    {
        cout << "\n========== WEATHER DATA MANAGEMENT SYSTEM ==========\n";
        cout << "1. Search Location\n";
        cout << "2. Add Favorite City\n";
        cout << "3. View Favorite Cities\n";
        cout << "4. Remove Favorite City\n";
        cout << "5. Recent Searches\n";
        cout << "6. Most Searched City\n";
        cout << "7. Weather Comfort Score\n";
        cout << "8. Travel Recommendation\n";
        cout << "9. Compare Cities\n";
        cout << "10. Weather Report\n";
        cout << "11. Save Weather Data\n";
        cout << "12. View Stored Data\n";
        cout << "13. Backup Data\n";
        cout << "14. Clear Storage\n";
        cout << "15. Weather Intelligence & Monitoring\n";
        cout << "16. Exit\n";
        cout << "\nEnter Choice: ";

        if (!(cin >> choice))
        {
            cin.clear();
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            cout << "\nInvalid Choice. Please enter a number.\n";
            continue;
        }

        switch (choice)
        {
            case 1:
                lm.searchLocation();
                break;

            case 2:
                lm.addFavoriteCity();
                break;

            case 3:
                lm.viewFavoriteCities();
                break;

            case 4:
                lm.removeFavoriteCity();
                break;

            case 5:
                lm.showRecentSearches();
                break;

            case 6:
                lm.showMostSearchedCity();
                break;

            case 7:
                lm.weatherComfortScore();
                break;

            case 8:
                lm.travelRecommendation();
                break;

            case 9:
                lm.compareCities();
                break;

            case 10:
                processor.generateWeatherReport();
                break;

            case 11:
            {
                string city;
                float temp;
                float humidity;
                float pressure;

                cout << "\nCity: ";
                cin >> city;

                cout << "Temperature: ";
                cin >> temp;

                if (temp < -100 || temp > 70)
                {
                    cout << "Invalid Temperature.\n";
                    break;
                }

                cout << "Humidity: ";
                cin >> humidity;

                if (humidity < 0 || humidity > 100)
                {
                    cout << "Invalid Humidity.\n";
                    break;
                }

                cout << "Pressure: ";
                cin >> pressure;

                if (pressure <= 0)
                {
                    cout << "Invalid Pressure.\n";
                    break;
                }

                storage.saveWeatherData(city, temp, humidity, pressure);
                break;
            }

            case 12:
                storage.viewStoredData();
                break;

            case 13:
                storage.backupData();
                break;

            case 14:
                storage.clearStorage();
                break;

            // ========================================
            // SINGLE WEATHER ENHANCEMENT PHASE
            // ========================================

            case 15:
            {
                int subChoice = -1;

                do
                {
                    cout << "\n========== WEATHER INTELLIGENCE & MONITORING ==========" << '\n';
                    cout << "1. Forecast + Current Details + Alerts + Insights\n";
                    cout << "2. API Performance Monitoring\n";
                    cout << "3. API Retry Test\n";
                    cout << "4. Automatic Weather Refresh\n";
                    cout << "0. Back To Main Menu\n";
                    cout << "\nEnter Choice: ";

                    if (!(cin >> subChoice))
                    {
                        cin.clear();
                        cin.ignore(numeric_limits<streamsize>::max(), '\n');
                        cout << "\nInvalid Choice.\n";
                        continue;
                    }

                    switch (subChoice)
                    {
                        case 1:
                            enhancement.showWeatherIntelligence();
                            break;

                        case 2:
                            enhancement.showApiPerformance();
                            break;

                        case 3:
                            enhancement.testApiWithRetry();
                            break;

                        case 4:
                            enhancement.automaticWeatherRefresh();
                            break;

                        case 0:
                            cout << "\nReturning to Main Menu...\n";
                            break;

                        default:
                            cout << "\nInvalid Choice.\n";
                            break;
                    }
                }
                while (subChoice != 0);

                break;
            }

            case 16:
                cout << "\nExiting Weather Data Management System...\n";
                break;

            default:
                cout << "\nInvalid Choice. Please try again.\n";
                break;
        }
    }
    while (choice != 16);

    return 0;
}
