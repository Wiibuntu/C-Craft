#include <stdio.h>
#include <stdlib.h>
#include <GL/glew.h>
#include <GL/glut.h>
#include <math.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

typedef enum {PLAINS, DESERT, FOREST, MOUNTAINS} WorldType;
WorldType selectedWorldType;

void generateTerrain();
void loadTextures();
void setCameraAboveTerrain();

void generateWorld() {
    selectedWorldType = (WorldType)(rand() % 4);  // Randomly select a world type
    generateTerrain();                            // Generate the terrain based on the world type
    setCameraAboveTerrain();                      // Set camera position above the terrain
    loadTextures();                               // Load textures for the world
}

#define WIDTH 800
#define HEIGHT 600
#define WORLD_SIZE 128
#define HEIGHT_SCALE 20.0f
#define MOVE_SPEED 0.5f
#define TURN_SPEED 1.5f
#define MOUSE_SENSITIVITY 0.1f

float world[WORLD_SIZE][WORLD_SIZE];
float cameraX, cameraY, cameraZ;
float cameraAngleX = 0.0f, cameraAngleY = 0.0f;
GLuint textures[4];

int lastMouseX = WIDTH / 2;
int lastMouseY = HEIGHT / 2;

float perlinNoise2D(float x, float y) {
    int n = (int)x + (int)y * 57;
    n = (n << 13) ^ n;
    return (1.0 - ((n * (n * n * 15731 + 789221) + 1376312589) & 0x7fffffff) / 1073741824.0);
}

void loadTextures() {
    int width, height, channels;
    unsigned char* image;

    glGenTextures(4, textures);

    const char* texturePaths[4] = {
        "textures/grass.png",
        "textures/sand.png",
        "textures/forest.png",
        "textures/rock.png"
    };

    for (int i = 0; i < 4; i++) {
        glBindTexture(GL_TEXTURE_2D, textures[i]);
        image = stbi_load(texturePaths[i], &width, &height, &channels, STBI_rgb_alpha);
        if (image) {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image);
            gluBuild2DMipmaps(GL_TEXTURE_2D, GL_RGBA, width, height, GL_RGBA, GL_UNSIGNED_BYTE, image);
        } else {
            printf("Failed to load texture %s\n", texturePaths[i]);
            exit(1);
        }
        stbi_image_free(image);
    }
}

void generateTerrain() {
    float frequency;
    switch (selectedWorldType) {
        case DESERT:
            frequency = 8;
            break;
        case FOREST:
            frequency = 15;
            break;
        case MOUNTAINS:
            frequency = 25;
            break;
        default:
            frequency = 10;
            break;
    }

    for (int x = 0; x < WORLD_SIZE; x++) {
        for (int z = 0; z < WORLD_SIZE; z++) {
            float nx = (float)x / WORLD_SIZE - 0.5f;
            float nz = (float)z / WORLD_SIZE - 0.5f;
            world[x][z] = (perlinNoise2D(nx * frequency, nz * frequency) + 1) / 2 * HEIGHT_SCALE;
        }
    }
}

void setCameraAboveTerrain() {
    int randX = rand() % WORLD_SIZE;
    int randZ = rand() % WORLD_SIZE;
    cameraX = randX;
    cameraZ = randZ;
    cameraY = world[randX][randZ] + 10.0f;
}

void initGL() {
    glewExperimental = GL_TRUE;
    GLenum err = glewInit();
    if (err != GLEW_OK) {
        printf("GLEW initialization failed: %s\n", glewGetErrorString(err));
        exit(1);
    }
    glClearColor(0.5f, 0.7f, 1.0f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_TEXTURE_2D);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(60.0, (double)WIDTH / (double)HEIGHT, 0.1, 500.0);
    glMatrixMode(GL_MODELVIEW);
}

void handleKeys(unsigned char key, int x, int y) {
    if (key == 27) {  // ESC key
        exit(0);
    }
    float rad = cameraAngleY * M_PI / 180.0f;
    switch (key) {
        case 'w':
            cameraX += sin(rad) * MOVE_SPEED;
            cameraZ -= cos(rad) * MOVE_SPEED;
            break;
        case 's':
            cameraX -= sin(rad) * MOVE_SPEED;
            cameraZ += cos(rad) * MOVE_SPEED;
            break;
        case 'a':
            cameraX += cos(rad) * MOVE_SPEED;
            cameraZ += sin(rad) * MOVE_SPEED;
            break;
        case 'd':
            cameraX -= cos(rad) * MOVE_SPEED;
            cameraZ -= sin(rad) * MOVE_SPEED;
            break;
    }
    glutPostRedisplay();
}

void handleMouseMovement(int x, int y) {
    static int firstMouse = 1;
    if (firstMouse) {
        lastMouseX = x;
        lastMouseY = y;
        firstMouse = 0;
    }

    int dx = x - lastMouseX;
    int dy = y - lastMouseY;

    cameraAngleY += dx * MOUSE_SENSITIVITY;
    cameraAngleX -= dy * MOUSE_SENSITIVITY;

    if (cameraAngleX > 89.0f) cameraAngleX = 89.0f;
    if (cameraAngleX < -89.0f) cameraAngleX = -89.0f;

    lastMouseX = x;
    lastMouseY = y;

    // Lock mouse to window boundaries
    if (x <= 10 || x >= WIDTH - 10 || y <= 10 || y >= HEIGHT - 10) {
        glutWarpPointer(WIDTH / 2, HEIGHT / 2);
        lastMouseX = WIDTH / 2;
        lastMouseY = HEIGHT / 2;
    }
    glutPostRedisplay();
}


void display() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();
    gluLookAt(cameraX, cameraY, cameraZ,
              cameraX + sin(cameraAngleY * M_PI / 180.0f),
              cameraY - tan(cameraAngleX * M_PI / 180.0f),
              cameraZ - cos(cameraAngleY * M_PI / 180.0f),
              0.0f, 1.0f, 0.0f);

    glBindTexture(GL_TEXTURE_2D, textures[selectedWorldType]);

    for (int x = 0; x < WORLD_SIZE - 1; x++) {
        for (int z = 0; z < WORLD_SIZE - 1; z++) {
            float height = world[x][z];
            float nextHeightX = world[x + 1][z];
            float nextHeightZ = world[x][z + 1];

            glBegin(GL_QUADS);
    // Top face
    glNormal3f(0.0f, 1.0f, 0.0f);
    glTexCoord2f(0.0f, 0.0f); glVertex3f(x, height, z);
    glTexCoord2f(1.0f, 0.0f); glVertex3f(x + 1, height, z);
    glTexCoord2f(1.0f, 1.0f); glVertex3f(x + 1, height, z + 1);
    glTexCoord2f(0.0f, 1.0f); glVertex3f(x, height, z + 1);

    // Front face
    glNormal3f(0.0f, 0.0f, 1.0f);
    glTexCoord2f(0.0f, 0.0f); glVertex3f(x, 0, z + 1);
    glTexCoord2f(1.0f, 0.0f); glVertex3f(x + 1, 0, z + 1);
    glTexCoord2f(1.0f, (height / 48.0f)); glVertex3f(x + 1, height, z + 1);
    glTexCoord2f(0.0f, (height / 48.0f)); glVertex3f(x, height, z + 1);

    // Right face
    glNormal3f(1.0f, 0.0f, 0.0f);
    glTexCoord2f(0.0f, 0.0f); glVertex3f(x + 1, 0, z);
    glTexCoord2f(1.0f, 0.0f); glVertex3f(x + 1, 0, z + 1);
    glTexCoord2f(1.0f, (height / 48.0f)); glVertex3f(x + 1, height, z + 1);
    glTexCoord2f(0.0f, (height / 48.0f)); glVertex3f(x + 1, height, z);

    // Left face
    glNormal3f(-1.0f, 0.0f, 0.0f);
    glTexCoord2f(0.0f, 0.0f); glVertex3f(x, 0, z);
    glTexCoord2f(1.0f, 0.0f); glVertex3f(x, 0, z + 1);
    glTexCoord2f(1.0f, (height / 48.0f)); glVertex3f(x, height, z + 1);
    glTexCoord2f(0.0f, (height / 48.0f)); glVertex3f(x, height, z);
glEnd();
        }
    }
    glutSwapBuffers();
}

void idle() {
    glutPostRedisplay();
}

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(WIDTH, HEIGHT);
    glutCreateWindow("Minecraft Clone 3D");
    glutSetCursor(GLUT_CURSOR_NONE);  // Hide cursor for immersive control
    
    initGL();
    generateWorld();
    
    glutDisplayFunc(display);
    glutIdleFunc(idle);
    glutKeyboardFunc(handleKeys);
    glutPassiveMotionFunc(handleMouseMovement);
    
    glutMainLoop();
    return 0;
}

