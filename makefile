CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra
LDFLAGS = -lcpr -lssl -lcrypto -lpthread

TARGET = deepseek_call
SRC = deepseek_call.cpp

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRC) $(LDFLAGS)

clean:
	rm -f $(TARGET)

