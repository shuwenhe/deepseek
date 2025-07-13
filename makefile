CXX = g++
CXXFLAGS = -std=c++17 -Wall -O2
LDFLAGS = -lcurl -lboost_system -lpthread

TARGET = deepseek_proxy
SRC = deepseek_proxy.cpp

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(TARGET)

