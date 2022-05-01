CC = clang
CXX = clang++
INCLUDE_DIR = Include/
LIB_DIR = Lib/
FRAMEWORKS = -framework OpenGL -framework Cocoa -framework IOKit -lglfw3
CXXFLAGS = -std=c++17

#$(LIB_DIR)/%.o: %.c
#	$(CC) $(CXXFLAGS) -c $< -o $@

all: boids-3d chessboard colorful-letter

glad: Lib/glad.c
	$(CC) -I Include/ -c Lib/glad.c -o macos/glad.o

boids-3d: glad
	$(CXX) -I $(INCLUDE_DIR) -L $(LIB_DIR) $(FRAMEWORKS) $(CXXFLAGS) macos/glad.o Lib/GLXtras.cpp Lib/Camera.cpp Lib/Mesh.cpp Lib/Draw.cpp Lib/Misc.cpp Lib/dMesh.cpp boids-3d.cpp -o macos/boids-3d

chessboard: glad
	$(CXX) -I $(INCLUDE_DIR) -L $(LIB_DIR) $(FRAMEWORKS) $(CXXFLAGS) macos/glad.o Lib/GLXtras.cpp Chessboard.cpp -o macos/chessboard

colorful-letter: glad
	$(CXX) -I $(INCLUDE_DIR) -L $(LIB_DIR) $(FRAMEWORKS) $(CXXFLAGS) macos/glad.o Lib/GLXtras.cpp ColorfulLetter.cpp -o macos/colorful-letter