SHELL := /bin/bash
CXX := g++
CXXFLAGS := -std=c++11 -O2 -Wall
LIBS := -lSDL2 -lGLEW -lGL

OBJ := main.o shader.o texture.o math.o noise.o cube.o world.o inventory.o

all: voxel

voxel: $(OBJ)
	$(CXX) $(CXXFLAGS) -o voxel $(OBJ) $(LIBS)

main.o: main.cpp shader.h texture.h math.h noise.h cube.h camera.h world.h inventory.h
	$(CXX) $(CXXFLAGS) -c main.cpp

shader.o: shader.cpp shader.h
	$(CXX) $(CXXFLAGS) -c shader.cpp

texture.o: texture.cpp texture.h
	$(CXX) $(CXXFLAGS) -c texture.cpp

math.o: math.cpp math.h
	$(CXX) $(CXXFLAGS) -c math.cpp

noise.o: noise.cpp noise.h
	$(CXX) $(CXXFLAGS) -c noise.cpp

cube.o: cube.cpp cube.h
	$(CXX) $(CXXFLAGS) -c cube.cpp

world.o: world.cpp world.h noise.h cube.h
	$(CXX) $(CXXFLAGS) -c world.cpp
	
inventory.o: inventory.cpp inventory.h
	$(CXX) $(CXXFLAGS) -c inventory.cpp	

clean:
	rm -f *.o voxel

