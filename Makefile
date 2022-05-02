CC = clang
CXX = clang++
INCLUDE_DIR = Include/
LIB_DIR = Lib/
FRAMEWORKS = -framework OpenGL -framework Cocoa -framework IOKit -lglfw3
CXXFLAGS = -std=c++17

#$(LIB_DIR)/%.o: %.c
#	$(CC) $(CXXFLAGS) -c $< -o $@

all: boids-3d chessboard colorful-letter dla-fractal

%.o: Lib/%.c
	$(CC) -I $(INCLUDE_DIR) -c -o macos/$@ $<

%.o: Lib/%.cpp
	$(CXX) -I $(INCLUDE_DIR) $(FRAMEWORKS) $(CXXFLAGS) -c -o macos/$@ $<

boids-3d: 
	$(CXX) -I $(INCLUDE_DIR) -L $(LIB_DIR) $(FRAMEWORKS) $(CXXFLAGS) macos/*.o boids-3d.cpp -o macos/boids-3d

chessboard: 
	$(CXX) -I $(INCLUDE_DIR) -L $(LIB_DIR) $(FRAMEWORKS) $(CXXFLAGS) macos/*.o Chessboard.cpp -o macos/chessboard

colorful-letter: 
	$(CXX) -I $(INCLUDE_DIR) -L $(LIB_DIR) $(FRAMEWORKS) $(CXXFLAGS) macos/*.o ColorfulLetter.cpp -o macos/colorful-letter

dla-fractal: 
	$(CXX) -I $(INCLUDE_DIR) -L $(LIB_DIR) $(FRAMEWORKS) $(CXXFLAGS) macos/*.o dlaFractal.cpp -o macos/dla-fractal