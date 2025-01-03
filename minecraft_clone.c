#include <stdio.h>
#include <stdlib.h>
#include <GL/glut.h>
#include <math.h>

#define WIDTH 800
#define HEIGHT 600
#define WORLD_SIZE 128
#define HEIGHT_SCALE 20.0f
#define MOVE_SPEED 0.5f
#define TURN_SPEED 1.5f

typedef enum {PLAINS, DESERT, FOREST, MOUNTAINS} WorldType;
WorldType selectedWorldType;

float world[WORLD_SIZE][WORLD_SIZE];
float cameraX, cameraY, cameraZ;
float cameraAngleX = 30.0f, cameraAngleY = 45.0f;

// Perlin noise for heightmap generation
float perlinNoise2D(float x, float y) {
    int n = (int)x + (int)y * 57;
    n = (n << 13) ^ n;
    return (1.0 - ((n * (n * n * 15731 + 789221) + 1376312589) & 0x7fffffff) / 1073741824.0);
}

// Generate terrain based on selected world type
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

// Set camera above the terrain at a random point
void setCameraAboveTerrain() {
    int randX = rand() % WORLD_SIZE;
    int randZ = rand() % WORLD_SIZE;
    cameraX = randX;
    cameraZ = randZ;
    cameraY = world[randX][randZ] + 30.0f;  // Camera height above terrain
}

// Initialize OpenGL settings
void initGL() {
    glClearColor(0.5f, 0.7f, 1.0f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(60.0, (double)WIDTH / (double)HEIGHT, 0.1, 500.0);
    glMatrixMode(GL_MODELVIEW);
}

// Generate the world and position the camera
void generateWorld() {
    selectedWorldType = (WorldType)(rand() % 4);
    generateTerrain();
    setCameraAboveTerrain();
}

// Handle keyboard input for camera movement
void handleKeys(unsigned char key, int x, int y) {
    switch (key) {
        case 'w':
            cameraX += sin(cameraAngleY * M_PI / 180.0f) * MOVE_SPEED;
            cameraZ -= cos(cameraAngleY * M_PI / 180.0f) * MOVE_SPEED;
            break;
        case 's':
            cameraX -= sin(cameraAngleY * M_PI / 180.0f) * MOVE_SPEED;
            cameraZ += cos(cameraAngleY * M_PI / 180.0f) * MOVE_SPEED;
            break;
        case 'a':
            cameraAngleY -= TURN_SPEED;
            break;
        case 'd':
            cameraAngleY += TURN_SPEED;
            break;
        case 'q':
            cameraY -= MOVE_SPEED;
            break;
        case 'e':
            cameraY += MOVE_SPEED;
            break;
    }
    glutPostRedisplay();
}

// Render the terrain
void display() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();
    gluLookAt(cameraX, cameraY, cameraZ,
              cameraX, cameraY - 10.0f, cameraZ - 10.0f,
              0.0f, 1.0f, 0.0f);

    for (int x = 0; x < WORLD_SIZE - 1; x++) {
        for (int z = 0; z < WORLD_SIZE - 1; z++) {
            float height = world[x][z];
            float nextHeightX = world[x + 1][z];
            float nextHeightZ = world[x][z + 1];

            glBegin(GL_QUADS);
                glColor3f(0.3f + height / HEIGHT_SCALE, 0.8f, 0.3f);
                glVertex3f(x, height, z);
                glVertex3f(x + 1, nextHeightX, z);
                glVertex3f(x + 1, nextHeightX, z + 1);
                glVertex3f(x, nextHeightZ, z + 1);
            glEnd();
        }
    }
    glutSwapBuffers();
}

// Idle loop to keep refreshing display
void idle() {
    glutPostRedisplay();
}

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(WIDTH, HEIGHT);
    glutCreateWindow("Minecraft Clone 3D");
    
    initGL();
    generateWorld();
    
    glutDisplayFunc(display);
    glutIdleFunc(idle);
    glutKeyboardFunc(handleKeys);
    
    glutMainLoop();
    return 0;
}

