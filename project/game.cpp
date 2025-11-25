#define _CRT_SECURE_NO_WARNINGS
#include "glad/glad.h" 
#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include <cstdio>

#define GLM_FORCE_RADIANS
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/type_ptr.hpp"

#include <iostream>
#include <fstream>
#include <string>
#include <algorithm>

// struct for keys
struct Key {
	int x;
	int y;
	char id;
	bool picked = false;
};

struct Door {
	int x;
	int y;
	char id;
	char key_id;
	bool unlocked = false;

};
// struct for storing map information
struct Map {
	int width;
	int height;
	std::vector<std::string> grid; 
	// coordinates for start position and goal position
	int startX = -1;
	int startY = -1;
	int goalX = -1;
	int goalY = -1;
	// for creating a grid in C++ https://stackoverflow.com/questions/61766469/create-grid-in-c

	std::vector<Key> keys;
	std::vector<Door> doors;
};



int screenWidth = 800;
int screenHeight = 600;
float timePast = 0;
const float PLAYER_RADIUS = 0.35f;

bool DEBUG_ON = true;
GLuint InitShader(const char* vShaderFileName, const char* fShaderFileName);
bool fullscreen = false;
void Win2PPM(int width, int height);
float rand01() {
	return rand() / (float)RAND_MAX;
}

bool loadMap(const std::string filename, Map& map) {
	// referenced https://cplusplus.com/reference/vector/vector/ for functions in vector library
	FILE* file = fopen(filename.c_str(), "r");
	// open file
	if (!file) {
		printf("ERROR: Could not open %s\n", filename.c_str());
		return false;
	}

	// read line for width and height of the map
	if (fscanf(file, "%d %d\n", &map.width, &map.height) != 2) {
		printf("ERROR: Could not read map dimensions");
		fclose(file);
		return false;
	}
	
	printf("Map dimensions: %d x %d\n", map.width, map.height);
	// clear vector of possible previous data, and reserve space for the grid
	map.grid.clear();
	map.grid.reserve(map.height);

	char buffer[1024];
	for (int i = 0; i < map.height; i++) {
		if (!fgets(buffer, sizeof(buffer), file)) {
			printf("ERROR: Failed to read row\n");
			fclose(file);
			return false;
		}

		buffer[strcspn(buffer, "\r\n")] = 0;

		if (strlen(buffer) != map.width) {
			printf("ERROR: Row has wrong length\n");
			fclose(file);
			return false;
		}

		std::string row = buffer;
		map.grid.push_back(row);
		for (int col = 0; col < row.size(); col++) {
			char ch = row[col];
			if (ch >= 'a' && ch <= 'e') {
				Key key;
				key.x = col;
				key.y = i;
				key.id = ch;     
				map.keys.push_back(key);
			}

			if (ch >= 'A' && ch <= 'E') {
				Door door;
				door.x = col;
				door.y = i;
				door.id = ch;
				door.key_id = std::tolower(ch); // https://www.geeksforgeeks.org/cpp/tolower-function-in-cpp/
				map.doors.push_back(door);
			}
			if (ch == 'S') {
				map.startX = col;
				map.startY = i;
			}

			if (ch == 'G') {
				map.goalX = col;
				map.goalY = i;
			}
		}	
		printf("%s\n", buffer);
	}
	printf("Number of keys: %d\n", map.keys.size());
	printf("Number of doors: %d\n", map.doors.size());
	fclose(file);
	return true;

}


// huge help from this article
// https://learnopengl.com/In-Practice/2D-Game/Collisions/Collision-detection
bool wallCollision(float px, float py, Map& map) {
	//printf("Coordinate of check : %f %f\n", px, py);
	int rowMin = (int)floor(py - PLAYER_RADIUS);
	int rowMax = (int)floor(py + PLAYER_RADIUS);
	int colMin = (int)floor(px + PLAYER_RADIUS);
	int colMax = (int)floor(px + PLAYER_RADIUS);

	for (int r = rowMin; r <= rowMax; ++r) {
		for (int c = colMin; c <= colMax; ++c) {
	/*for (int r = 0; r < map.width; ++r) {
		for (int c = 0; c < map.height; ++c) {*/
			int mapRow = map.height - 1 - r;
			int mapCol = c;

			// treat out of bounds like collision
			if (mapRow < 0 || mapRow >= map.height || mapCol < 0 || mapCol >= map.width) {
				//printf("Out of bounds\n");
				return true;
			}
			

			// WALL TILE CHECK
			if (map.grid[mapRow][mapCol] == 'W') {

				// cube bounding box
				float cubeXMin = (float)c;
				float cubeXMax = (float)c + 1.0f;
				float cubeYMin = (float)r;
				float cubeYMax = (float)r + 1.0f;
				//printf("cubeXMin: %f cubeXMax: %f\n", cubeXMin, cubeXMax);
				//printf("cubeYMin: %f cubeYMax: %f\n", cubeYMin, cubeYMax);

				// player bounding box
				float pxMin = px + PLAYER_RADIUS;
				float pxMax = px - PLAYER_RADIUS;
				float pyMin = py + PLAYER_RADIUS;
				float pyMax = py - PLAYER_RADIUS;
				//printf("px: %f\n", px);
				//printf("py: %f\n", py);

				if (pxMin > cubeXMin && pxMax < cubeXMax &&
					pyMin > cubeYMin && pyMax < cubeYMax) {
					printf("Collision detected\n");
					return true;
				}
			}

			// KEY TILE CHECK
			if (map.grid[mapRow][mapCol] >= 'a' && map.grid[mapRow][mapCol] <= 'e') {
				for (auto& key : map.keys) {
					if (key.id == map.grid[mapRow][mapCol]) {
						if (!key.picked) {
							key.picked = true;
							printf("Key %c has been picked up\n", key.id);
							return false;
						}
						else {
							float keyXMin = (float)c;
							float keyXMax = (float)c + 0.3f;
							float keyYMin = (float)r;
							float keyYMax = (float)r + 0.3f;

							// player bounding box
							float pxMin = px + PLAYER_RADIUS;
							float pxMax = px - PLAYER_RADIUS;
							float pyMin = py + PLAYER_RADIUS;
							float pyMax = py - PLAYER_RADIUS;

							if (pxMin > keyXMin && pxMax < keyXMax &&
								pyMin > keyYMin && pyMax < keyYMax) {
								printf("Key detected\n");
								return false;
							}
						}
					}
				}
					return false;
			}

			// DOOR TILE CHECK
			if (map.grid[mapRow][mapCol] >= 'A' && map.grid[mapRow][mapCol] <= 'E') {
				for (auto& door : map.doors) {
					if (door.id == map.grid[mapRow][mapCol]) {
						for (auto& key : map.keys) {
							if (key.id == door.key_id && key.picked) {
								door.unlocked = true;
								printf("Door has been unlocked!\n");
								return false;
							}
						}
					}
				}
				printf("Need key to open door\n");
				return true;
			}
		}
	}
	printf("No collision\n");
	return false;
}

int main(int argc, char* argv[]) {
	if (argc < 2) {
		printf("Need map file\n");
		return 1;
	}

	SDL_Init(SDL_INIT_VIDEO);

	//Print the version of SDL we are using (should be 3.x or higher)
	const int sdl_linked = SDL_GetVersion();
	printf("\nCompiled against SDL version %d.%d.%d ...\n", SDL_VERSIONNUM_MAJOR(SDL_VERSION), SDL_VERSIONNUM_MINOR(SDL_VERSION), SDL_VERSIONNUM_MICRO(SDL_VERSION));
	printf("Linking against SDL version %d.%d.%d.\n", SDL_VERSIONNUM_MAJOR(sdl_linked), SDL_VERSIONNUM_MINOR(sdl_linked), SDL_VERSIONNUM_MICRO(sdl_linked));

	//Ask SDL to get a recent version of OpenGL (3.2 or greater)
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);

    //Create a window (title, width, height, flags)
    SDL_Window* window = SDL_CreateWindow("My OpenGL Program", screenWidth, screenHeight, SDL_WINDOW_OPENGL);
    if (!window) {
        printf("SDL_CreateWindow Error: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    //Create a context to draw in
    SDL_GLContext context = SDL_GL_CreateContext(window);

	//Load OpenGL extentions with GLAD
	if (gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress)) {
		printf("\nOpenGL loaded\n");
		printf("Vendor:   %s\n", glGetString(GL_VENDOR));
		printf("Renderer: %s\n", glGetString(GL_RENDERER));
		printf("Version:  %s\n\n", glGetString(GL_VERSION));
	}
	else {
		printf("ERROR: Failed to initialize OpenGL context.\n");
		return -1;
	}
	
	// loading models
	//Load Model 1 (teapot)
	std::ifstream modelFile;
	modelFile.open("models/teapot.txt");
	int numLines = 0;
	if (modelFile.is_open()) {
		modelFile >> numLines;
	}
	float* model1 = new float[numLines];
	for (int i = 0; i < numLines; i++) {
		modelFile >> model1[i];
	}
	printf("%d\n", numLines);
	int numVertsTeapot = numLines / 8;
	modelFile.close();

	//Load Model 2 (knot)
	modelFile.open("models/knot.txt");
	numLines = 0;
	if (modelFile.is_open()) {
		modelFile >> numLines;
	}
	float* model2 = new float[numLines];
	for (int i = 0; i < numLines; i++) {
		modelFile >> model2[i];
	}
	printf("%d\n", numLines);
	int numVertsKnot = numLines / 8;
	modelFile.close();

	// Load model 3 (cube)
	modelFile.open("models/cube.txt");
	numLines = 0;
	if (modelFile.is_open()) {
		modelFile >> numLines;
	}
	float* model3 = new float[numLines];
	for (int i = 0; i < numLines; i++) {
		modelFile >> model3[i];
	}
	printf("%d\n", numLines);
	int numVertsCube = numLines / 8;
	modelFile.close();

	// Load model 4 (sphere)
	modelFile.open("models/sphere.txt");
	numLines = 0;
	if (modelFile.is_open()) {
		modelFile >> numLines;
	}
	float* model4 = new float[numLines];
	for (int i = 0; i < numLines; i++) {
		modelFile >> model4[i];
	}
	printf("%d\n", numLines);
	int numVertsSphere = numLines / 8;
	modelFile.close();

	int totalNumVerts = numVertsTeapot + numVertsKnot + numVertsCube + numVertsSphere;
	float* modelData = new float[totalNumVerts * 8];
	int startVertTeapot = 0;  //The teapot is the first model in the VBO
	int startVertKnot = numVertsTeapot; //The knot starts right after the teapot
	int startVertCube = startVertKnot + numVertsKnot;
	int startVertSphere = startVertCube + numVertsCube;

	std::copy(model1, model1 + numVertsTeapot * 8, 
		modelData + startVertTeapot * 8);
	std::copy(model2, model2 + numVertsKnot * 8, 
		modelData + startVertKnot * 8);
	std::copy(model3, model3 + numVertsCube * 8, 
		modelData + startVertCube * 8);
	std::copy(model4, model4 + numVertsSphere * 8, 
		modelData + startVertSphere * 8);

	//// Allocate Texture 0 (Wood) ///////
	SDL_Surface* surface = SDL_LoadBMP("wood.bmp");
	if (surface == NULL) { //If it failed, print the error
		printf("Error: \"%s\"\n", SDL_GetError()); return 1;
	}
	GLuint tex0;
	glGenTextures(1, &tex0);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, tex0);

	//What to do outside 0-1 range
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	//Load the texture into memory
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, surface->w, surface->h, 0, GL_BGR, GL_UNSIGNED_BYTE, surface->pixels);
	glGenerateMipmap(GL_TEXTURE_2D); //Mip maps the texture

	SDL_DestroySurface(surface);
	//// End Allocate Texture ///////
	
	//// Allocate Texture 1 (Brick) ///////
	SDL_Surface* surface1 = SDL_LoadBMP("brick.bmp");
	if (surface1 == NULL) { //If it failed, print the error
		printf("Error: \"%s\"\n", SDL_GetError()); return 1;
	}
	GLuint tex1;
	glGenTextures(1, &tex1);

	//Load the texture into memory
	glActiveTexture(GL_TEXTURE1);

	glBindTexture(GL_TEXTURE_2D, tex1);
	//What to do outside 0-1 range
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, surface1->w, surface1->h, 0, GL_BGR, GL_UNSIGNED_BYTE, surface1->pixels);
	glGenerateMipmap(GL_TEXTURE_2D); //Mip maps the texture

	SDL_DestroySurface(surface1);
	//// End Allocate Texture ///////
	
	//Build a Vertex Array Object (VAO) to store mapping of shader attributse to VBO
	GLuint vao;
	glGenVertexArrays(1, &vao); //Create a VAO
	glBindVertexArray(vao); //Bind the above created VAO to the current context

	//Allocate memory on the graphics card to store geometry (vertex buffer object)
	GLuint vbo[1];
	glGenBuffers(1, vbo);  //Create 1 buffer called vbo
	glBindBuffer(GL_ARRAY_BUFFER, vbo[0]); //Set the vbo as the active array buffer (Only one buffer can be active at a time)
	glBufferData(GL_ARRAY_BUFFER, totalNumVerts * 8 * sizeof(float), modelData, GL_STATIC_DRAW); //upload vertices to vbo
	//GL_STATIC_DRAW means we won't change the geometry, GL_DYNAMIC_DRAW = geometry changes infrequently
	//GL_STREAM_DRAW = geom. changes frequently.  This effects which types of GPU memory is used

	int texturedShader = InitShader("textured-Vertex.glsl", "textured-Fragment.glsl");

	//Tell OpenGL how to set fragment shader input 
	GLint posAttrib = glGetAttribLocation(texturedShader, "position");
	glVertexAttribPointer(posAttrib, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), 0);
	//Attribute, vals/attrib., type, isNormalized, stride, offset
	glEnableVertexAttribArray(posAttrib);

	GLint normAttrib = glGetAttribLocation(texturedShader, "inNormal");
	glVertexAttribPointer(normAttrib, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(5 * sizeof(float)));
	glEnableVertexAttribArray(normAttrib);

	GLint texAttrib = glGetAttribLocation(texturedShader, "inTexcoord");
	glEnableVertexAttribArray(texAttrib);
	glVertexAttribPointer(texAttrib, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));

	GLint uniView = glGetUniformLocation(texturedShader, "view");
	GLint uniProj = glGetUniformLocation(texturedShader, "proj");

	glBindVertexArray(0); //Unbind the VAO in case we want to create a new one	


	glEnable(GL_DEPTH_TEST);

	// load map
	Map map;
	std::string mapFileName = argv[1];
	if (!loadMap(mapFileName, map)) return -1;

	SDL_Event windowEvent;
	bool quit = false;

	// FIRST PERSON POV
	int startRow = map.height - 1 - map.startY;
	int startCol = map.startX;
	float eyeHeight = 0.2f;
	glm::vec3 eye(startCol + 0.5f, startRow + 0.5f, eyeHeight);

	glm::vec3 forward(0.0f, 1.0f, 0.0f);
	glm::vec3 center = eye + forward;
	glm::vec3 up(0.0f, 0.0f, 1.0f);
	float camMove = 0.05f;

	// VARIABLES FOR CAMERA
	float yaw = 90.0f;
	glm::vec3 right = glm::normalize(glm::cross(forward, up));

	glm::vec3 flatForward(cos(glm::radians(yaw)), sin(glm::radians(yaw)), 0.0f);
	flatForward = glm::normalize(flatForward);
	glm::vec3 attempt;

	while (!quit) {
		while (SDL_PollEvent(&windowEvent)) {  //inspect all events in the queue
			if (windowEvent.type == SDL_EVENT_QUIT) quit = true;
			if (windowEvent.type == SDL_EVENT_KEY_UP && windowEvent.key.key == SDLK_ESCAPE)
				quit = true; //Exit event loop
			if (windowEvent.type == SDL_EVENT_KEY_UP && windowEvent.key.key == SDLK_F) { //If "f" is pressed
				fullscreen = !fullscreen;
				SDL_SetWindowFullscreen(window, fullscreen ? SDL_WINDOW_FULLSCREEN : 0); //Toggle fullscreen 
			}
			if (windowEvent.type == SDL_EVENT_KEY_UP && windowEvent.key.key == SDLK_ESCAPE)
				quit = true;
			if (windowEvent.type == SDL_EVENT_KEY_UP && windowEvent.key.key == SDLK_Q)
				quit = true;

			
			// help with keyboard / camera movement
			// https://www.researchgate.net/figure/Definition-of-pitch-roll-and-yaw-angle-for-camera-state-estimation_fig15_273225757
			if (windowEvent.type == SDL_EVENT_KEY_DOWN) {
				glm::vec3 flatForward(cos(glm::radians(yaw)), sin(glm::radians(yaw)), 0.0f);
				flatForward = glm::normalize(flatForward);

				//glm::vec3 step(0.0f);
				switch (windowEvent.key.key) {
					// move fowards
					case SDLK_UP:
					case SDLK_W:
						// have an attempted move and clamp the movement
						attempt = eye + flatForward * camMove;
						attempt.x = glm::clamp(attempt.x, PLAYER_RADIUS, (float)map.width - PLAYER_RADIUS);
						attempt.y = glm::clamp(attempt.y, PLAYER_RADIUS, (float)map.height - PLAYER_RADIUS);
						printf("Attempted move: x = %.3f, y = %.3f\n", attempt.x, attempt.y);
						// if we didn't collide into anything, check if we are at the goal
						if (!wallCollision(attempt.x, attempt.y, map)) {
							eye = attempt;
							float dx = eye.x - map.goalX;
							float dy = eye.y - (map.height - 1 - map.goalY + 0.5f);
							if (dx * dx + dy * dy <= 0.25f) {
								SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "Goal reached!", "You've reached your goal!", window);
								quit = true;
							}
						}
						break;
					// move backwards
					case SDLK_DOWN:
					case SDLK_S:
						// have an attempted move and clamp the movement
						attempt = eye - flatForward * camMove; 
						attempt.x = glm::clamp(attempt.x, PLAYER_RADIUS, (float)map.width - PLAYER_RADIUS);
						attempt.y = glm::clamp(attempt.y, PLAYER_RADIUS, (float)map.height - PLAYER_RADIUS);
						printf("Attempted move: x = %.3f, y = %.3f\n", attempt.x, attempt.y);
						// if we didn't collide into anything, check if we are at the goal
						if (!wallCollision(attempt.x, attempt.y, map)) {
							eye = attempt;
							float dx = eye.x - map.goalX;
							float dy = eye.y - (map.height - 1 - map.goalY + 0.5f);
							if (dx * dx + dy * dy <= 0.25f) {
								SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "Goal reached!", "You've reached your goal!", window);
								quit = true;
							}
						}
						break;
					// rotate camera left and right
					case SDLK_RIGHT:
					case SDLK_D:
						yaw -= 5.0f;
						break;
					case SDLK_LEFT:
					case SDLK_A:
						yaw += 5.0f;
						break;
				}
			}
		}

		// UPDATE GLOBAL CAMERA VECTORS
		forward.x = cos(glm::radians(yaw));
		forward.y = sin(glm::radians(yaw));
		forward.z = 0.0f;
		forward = glm::normalize(forward);

		right = glm::normalize(glm::cross(forward, up));
		center = eye + forward;
		
		glClearColor(0.2f, 0.4f, 0.8f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		glUseProgram(texturedShader);

		timePast = SDL_GetTicks() / 1000.f;

		// DEBUG CAMERA POV
		//glm::mat4 view = glm::lookAt(
		//	glm::vec3(map.width / 2.0f, map.height / 2.0f, 20.f),  // above center
		//	glm::vec3(map.width / 2.0f, map.height / 2.0f, 0.f),   // look down
		//	glm::vec3(0.f, 1.f, 0.f)
		//);
		//glUniformMatrix4fv(uniView, 1, GL_FALSE, glm::value_ptr(view));

		//glm::mat4 proj = glm::perspective(glm::radians(45.0f), screenWidth / (float)screenHeight, 0.1f, 100.0f); 
		//glUniformMatrix4fv(uniProj, 1, GL_FALSE, glm::value_ptr(proj));

		// FIRST PERSON CAMERA POV
		glm::mat4 view = glm::lookAt(eye, center, up);
		glUniformMatrix4fv(uniView, 1, GL_FALSE, glm::value_ptr(view));

		glm::mat4 proj = glm::perspective(glm::radians(60.0f), screenWidth / (float)screenHeight, 0.1f, 100.0f);
		glUniformMatrix4fv(uniProj, 1, GL_FALSE, glm::value_ptr(proj));

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, tex0);
		glUniform1i(glGetUniformLocation(texturedShader, "tex0"), 0);

		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, tex1);
		glUniform1i(glGetUniformLocation(texturedShader, "tex1"), 1);

		glBindVertexArray(vao);

		// TEST INSERT CUBE HERE
		/*GLint uniTexID = glGetUniformLocation(texturedShader, "texID");

		glm::mat4 model = glm::mat4(1);
		GLint uniModel = glGetUniformLocation(texturedShader, "model");
		glUniformMatrix4fv(uniModel, 1, GL_FALSE, glm::value_ptr(model));
		glUniform1i(uniTexID, 0);

		glDrawArrays(GL_TRIANGLES, startVertCube, numVertsCube);*/

		GLint uniModel = glGetUniformLocation(texturedShader, "model");
		GLint uniTexID = glGetUniformLocation(texturedShader, "texID");
		GLint uniColor = glGetUniformLocation(texturedShader, "inColor");
		glm::vec3 colVec(0, 0, 0);
		glUniform3fv(uniColor, 1, glm::value_ptr(colVec));

		// DRAW GEOMETRIES ON MAP
		for (int row = 0; row < map.height; row++) {
			for (int col = 0; col < map.width; col++) {
				// draw each floor tile while traversing through map grid
				int flippedRow = map.height - 1 - row;
				glm::mat4 floorModel = glm::mat4(1.0f);
				floorModel = glm::translate(floorModel, glm::vec3(col + 0.5f, flippedRow + 0.5f, -0.45f - 0.1 / 2.0f));
				floorModel = glm::scale(floorModel, glm::vec3(1.0f, 1.0f, 0.1f));
				glUniformMatrix4fv(uniModel, 1, GL_FALSE, glm::value_ptr(floorModel));
				glUniform1i(uniTexID, -1);
				glm::vec3 colVec(0,0,0);
				glUniform3fv(uniColor, 1, glm::value_ptr(colVec));
				glDrawArrays(GL_TRIANGLES, startVertCube, numVertsCube);

				char c = map.grid[row][col];
				// WALL
				if (c == 'W') {
					int flippedRow = map.height - 1 - row;
					/*glm::mat4 wallModel = glm::mat4(1.0f);
					wallModel = glm::translate(wallModel, glm::vec3(col, flippedRow, 0));
					wallModel = glm::scale(wallModel, glm::vec3(1.0f));*/

					//DEBUG BOUNDING WALL CUBE BOX
					glm::mat4 wallModel = glm::mat4(1.0f);
					wallModel = glm::translate(wallModel, glm::vec3(col + 0.5f, flippedRow + 0.5f, 0.0f)); // center at middle of cell
					wallModel = glm::scale(wallModel, glm::vec3(1.0f, 1.0f, 1.0f));
				

					glUniformMatrix4fv(uniModel, 1, GL_FALSE, glm::value_ptr(wallModel));
					glUniform1i(uniTexID, 1);
					glDrawArrays(GL_TRIANGLES, startVertCube, numVertsCube);
				}

				// DOOR
				if (c >= 'A' && c <= 'E') {
					int flippedRow = map.height - 1 - row;
					glm::mat4 doorModel = glm::mat4(1.0f);
					for (auto& door : map.doors) {
						// if door is unlocked, do not render it
						if (door.unlocked) {
							continue;
						}
						else {
							doorModel = glm::translate(doorModel, glm::vec3(col + 0.5f, flippedRow + 0.5f, 0));
							glUniformMatrix4fv(uniModel, 1, GL_FALSE, glm::value_ptr(doorModel));
							glUniform1i(uniTexID, 0);
							glDrawArrays(GL_TRIANGLES, startVertKnot, numVertsKnot);
						}
					}
				}

				// KEY
				if (c >= 'a' && c <= 'e') {
					int flippedRow = map.height - 1 - row;
					glm::mat4 keyModel = glm::mat4(1.0f);
					for (auto& key : map.keys) {
						if (key.id == c) {
							// if key has been picked up, render it infront of us
							if (key.picked) {
								glm::vec3 holdPos = eye + forward * 0.5f + glm::vec3(0.0f, 0.0f, -0.1f);
								keyModel = glm::translate(keyModel, holdPos);
								keyModel = glm::rotate(
									keyModel,
									glm::radians(yaw - 90.0f), 
									glm::vec3(0, 0, 1)
								);
								keyModel = glm::scale(keyModel, glm::vec3(0.4f));
								glUniformMatrix4fv(uniModel, 1, GL_FALSE, glm::value_ptr(keyModel));
								glUniform1i(uniTexID, -1);
								glm::vec3 colVec(0.5f, 0.5f, 0.5f);
								glUniform3fv(uniColor, 1, glm::value_ptr(colVec));
								glDrawArrays(GL_TRIANGLES, startVertTeapot, numVertsTeapot);
								continue;
							}
							// else render it normally , where it is in the map
							else {
								keyModel = glm::translate(keyModel, glm::vec3(col + 0.5f, flippedRow + 0.5f, 0));
								keyModel = glm::rotate(keyModel, timePast * glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 1.0f));
								glUniformMatrix4fv(uniModel, 1, GL_FALSE, glm::value_ptr(keyModel));
								glUniform1i(uniTexID, -1);
								glm::vec3 colVec(0.5f, 0.5f, 0.5f);
								glUniform3fv(uniColor, 1, glm::value_ptr(colVec));
								glDrawArrays(GL_TRIANGLES, startVertTeapot, numVertsTeapot);
							}
						}
					}
		
				}

				// GOAL
				if (c == 'G') {
					int flippedRow = map.height - 1 - row;
					glm::mat4 goalModel = glm::mat4(1.0f);
					goalModel = glm::translate(goalModel, glm::vec3(col + 0.5f, flippedRow + 0.5f, 0));
					goalModel = glm::scale(goalModel, glm::vec3(0.2f));
					glUniformMatrix4fv(uniModel, 1, GL_FALSE, glm::value_ptr(goalModel));
					glUniform1i(uniTexID, -1);
					glm::vec3 colVec(rand01(), rand01(), rand01());
					glUniform3fv(uniColor, 1, glm::value_ptr(colVec));
					glDrawArrays(GL_TRIANGLES, startVertSphere, numVertsSphere);
				}
			}
		}

		SDL_GL_SwapWindow(window);
	}

	delete[] modelData;
	delete[] model1;
	delete[] model2;
	delete[] model3;
	delete[] model4;

	//Clean Up
	glDeleteProgram(texturedShader);
	glDeleteBuffers(1, vbo);
	glDeleteVertexArrays(1, &vao);

	SDL_GL_DestroyContext(context);
	SDL_Quit();
	return 0;
}

// Create a NULL-terminated string by reading the provided file
static char* readShaderSource(const char* shaderFile) {
	FILE* fp;
	long length;
	char* buffer;

	// open the file containing the text of the shader code
	fp = fopen(shaderFile, "r");

	// check for errors in opening the file
	if (fp == NULL) {
		printf("can't open shader source file %s\n", shaderFile);
		return NULL;
	}

	// determine the file size
	fseek(fp, 0, SEEK_END); // move position indicator to the end of the file;
	length = ftell(fp);  // return the value of the current position

	// allocate a buffer with the indicated number of bytes, plus one
	buffer = new char[length + 1];

	// read the appropriate number of bytes from the file
	fseek(fp, 0, SEEK_SET);  // move position indicator to the start of the file
	fread(buffer, 1, length, fp); // read all of the bytes

	// append a NULL character to indicate the end of the string
	buffer[length] = '\0';

	// close the file
	fclose(fp);

	// return the string
	return buffer;
}
// Create a GLSL program object from vertex and fragment shader files
GLuint InitShader(const char* vShaderFileName, const char* fShaderFileName) {
	GLuint vertex_shader, fragment_shader;
	GLchar* vs_text, * fs_text;
	GLuint program;

	// check GLSL version
	printf("GLSL version: %s\n\n", glGetString(GL_SHADING_LANGUAGE_VERSION));

	// Create shader handlers
	vertex_shader = glCreateShader(GL_VERTEX_SHADER);
	fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);

	// Read source code from shader files
	vs_text = readShaderSource(vShaderFileName);
	fs_text = readShaderSource(fShaderFileName);

	// error check
	if (vs_text == NULL) {
		printf("Failed to read from vertex shader file %s\n", vShaderFileName);
		exit(1);
	}
	else if (DEBUG_ON) {
		printf("Vertex Shader:\n=====================\n");
		printf("%s\n", vs_text);
		printf("=====================\n\n");
	}
	if (fs_text == NULL) {
		printf("Failed to read from fragent shader file %s\n", fShaderFileName);
		exit(1);
	}
	else if (DEBUG_ON) {
		printf("\nFragment Shader:\n=====================\n");
		printf("%s\n", fs_text);
		printf("=====================\n\n");
	}

	// Load Vertex Shader
	const char* vv = vs_text;
	glShaderSource(vertex_shader, 1, &vv, NULL);  //Read source
	glCompileShader(vertex_shader); // Compile shaders

	// Check for errors
	GLint  compiled;
	glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &compiled);
	if (!compiled) {
		printf("Vertex shader failed to compile:\n");
		if (DEBUG_ON) {
			GLint logMaxSize, logLength;
			glGetShaderiv(vertex_shader, GL_INFO_LOG_LENGTH, &logMaxSize);
			printf("printing error message of %d bytes\n", logMaxSize);
			char* logMsg = new char[logMaxSize];
			glGetShaderInfoLog(vertex_shader, logMaxSize, &logLength, logMsg);
			printf("%d bytes retrieved\n", logLength);
			printf("error message: %s\n", logMsg);
			delete[] logMsg;
		}
		exit(1);
	}

	// Load Fragment Shader
	const char* ff = fs_text;
	glShaderSource(fragment_shader, 1, &ff, NULL);
	glCompileShader(fragment_shader);
	glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &compiled);

	//Check for Errors
	if (!compiled) {
		printf("Fragment shader failed to compile\n");
		if (DEBUG_ON) {
			GLint logMaxSize, logLength;
			glGetShaderiv(fragment_shader, GL_INFO_LOG_LENGTH, &logMaxSize);
			printf("printing error message of %d bytes\n", logMaxSize);
			char* logMsg = new char[logMaxSize];
			glGetShaderInfoLog(fragment_shader, logMaxSize, &logLength, logMsg);
			printf("%d bytes retrieved\n", logLength);
			printf("error message: %s\n", logMsg);
			delete[] logMsg;
		}
		exit(1);
	}

	// Create the program
	program = glCreateProgram();

	// Attach shaders to program
	glAttachShader(program, vertex_shader);
	glAttachShader(program, fragment_shader);

	// Link and set program to use
	glLinkProgram(program);

	return program;
}