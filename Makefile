# Makefile for the Voxel Engine

CXX      = g++
CXXFLAGS = -std=c++11 -Wall -I.
LDFLAGS  = -lSDL2 -lGLEW -lGL

# List all source files
SRCS = main.cpp math.cpp shader.cpp cube.cpp
OBJS = $(SRCS:.cpp=.o)

TARGET = voxel_engine

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJS) $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean
