CXX = g++
CXXFLAGS = -std=c++17 -pthread
LDFLAGS = -lcurl
TARGET = weather
SOURCES = APIManager.cpp location.cpp main.cpp weather.cpp weatherDataProcessor.cpp weatherEnhancement.cpp weatherStorageManager.cpp

all:
	$(CXX) $(CXXFLAGS) $(SOURCES) -o $(TARGET) $(LDFLAGS)

clean:
	rm -f $(TARGET)
