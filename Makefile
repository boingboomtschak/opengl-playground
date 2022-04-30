CC = clang
CXX = clang++
INCLUDE_DIR = Include/
LIB_DIR = Lib/
FRAMEWORKS = -framework OpenGL -framework Cocoa -framework IOKit -lglfw3
CXXFLAGS = -std=c++17

#$(LIB_DIR)/%.o: %.c
#	$(CC) $(CXXFLAGS) -c $< -o $@

all: boids-3d

Lib/glad.o: Lib/glad.c
	$(CC) -I Include/ -c Lib/glad.c -o Lib/glad.o

boids-3d: Lib/glad.o
	$(CXX) -I $(INCLUDE_DIR) -L $(LIB_DIR) $(FRAMEWORKS) $(CXXFLAGS) Lib/glad.o Lib/GLXtras.cpp Lib/Camera.cpp Lib/Mesh.cpp Lib/Draw.cpp Lib/Misc.cpp Lib/dMesh.cpp boids-3d.cpp -o boids-3d

chessboard: Lib/glad.o
	$(CXX) -I $(INCLUDE_DIR) -L $(LIB_DIR) $(FRAMEWORKS) $(CXXFLAGS) Lib/glad.o Lib/GLXtras.cpp Chessboard.cpp -o chessboard

colorful-letter: Lib/glad.o
	$(CXX) -I $(INCLUDE_DIR) -L $(LIB_DIR) $(FRAMEWORKS) $(CXXFLAGS) Lib/glad.o Lib/GLXtras.cpp ColorfulLetter.cpp -o colorful-letter