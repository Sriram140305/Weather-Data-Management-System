WEATHER DATA MANAGEMENT SYSTEM - FINAL GCC 7.5 / JSON 2.1.1 VERSION

TARGET ENVIRONMENT
------------------
- GCC/G++ 7.5.0
- C++17
- nlohmann-json 2.1.1
- libcurl
- Linux/PuTTY server

SOURCE FIXES INCLUDED
---------------------
1. APIManager.cpp reads WEATHER_API_KEY from the environment.
2. APIManager.cpp includes curl before APIManager.h to avoid Windows byte/std::byte
   namespace-order problems.
3. Location search has URL encoding and CURL/HTTP failure handling.
4. weather.cpp checks that the API key is configured.
5. SSL peer and host verification are enabled in weather.cpp and weatherEnhancement.cpp.
6. Login/Register follows the requested loop:
   Login -> check users.csv -> success enters main menu;
   failure returns to Login/Register;
   Register -> save users.csv -> returns to Login/Register.
7. main.cpp exits on menu option 17.
8. Forecast/retry/automatic refresh remain in one Weather Intelligence & Monitoring phase.
9. JSON usage avoids newer contains()/value() APIs and uses old-compatible operations.
10. No C++20-only features or std::filesystem are required.

BUILD ON P U T T Y / LINUX
--------------------------
export WEATHER_API_KEY="YOUR_REAL_WEATHERAPI_KEY"

g++ --version

g++ -std=c++17 -pthread *.cpp -o weather -lcurl

./weather

IMPORTANT
---------
Do not commit or share the real API key.
Do not copy users.csv containing real credentials into a public repository.
The final source does not contain the API key.

WINDOWS/MSYS2
-------------
If testing in UCRT64:
g++ -std=c++17 *.cpp -o weather.exe -lcurl
.\weather.exe

For the PuTTY/GCC 7.5 server, use the Linux build command above.
