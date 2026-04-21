CXX := clang++
CXXFLAGS := -std=c++17 -Wall -Wextra -pedantic -pthread
TARGET := watch_simulator
SRC := src/main.cpp

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) $(SRC) -o $(TARGET)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)
