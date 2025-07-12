CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra
INCLUDES = -I/usr/local/include
LIBS = -lcpr -lssl -lcrypto -lpthread
TARGET = deepseek_proxy
SRC = main.cpp

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $^ $(LIBS)

clean:
	rm -f $(TARGET)

