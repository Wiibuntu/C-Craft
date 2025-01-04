all: minecraft_clone

minecraft_clone: Main.o Game.o World.o Block.o Renderer.o Player.o
	g++ Main.o Game.o World.o Block.o Renderer.o Player.o -o minecraft_clone -lglfw -lGL

Main.o: Main.cpp
	g++ -c Main.cpp

Game.o: Game.cpp
	g++ -c Game.cpp

World.o: World.cpp
	g++ -c World.cpp

Block.o: Block.cpp
	g++ -c Block.cpp

Renderer.o: Renderer.cpp
	g++ -c Renderer.cpp

Player.o: Player.cpp
	g++ -c Player.cpp

clean:
	rm -f *.o minecraft_clone
