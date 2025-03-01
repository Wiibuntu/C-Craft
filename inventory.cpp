#include "inventory.h"
#include <SDL2/SDL.h>
#include <iostream>
#include <vector>
#include <cmath>

#include "cube.h"    // for addCube(...) to generate block vertices
#include "math.h"    // for Mat4, Vec3, perspectiveMatrix, lookAtMatrix, identityMatrix, etc.
#include "texture.h" // for texture functions
#include "world.h"   // for BLOCK_GRASS, BLOCK_STONE, etc.

// These symbols must be defined in exactly one .cpp (e.g., main.cpp) but declared extern in a shared header:
extern GLuint worldShader;   // The 3D shader used by your main scene
extern GLuint texID;         // The block texture
extern int SCREEN_WIDTH;     // e.g., 1280
extern int SCREEN_HEIGHT;    // e.g., 720

// If you have a separate UI shader for 2D rectangles:
extern GLuint uiShader;      // e.g., used for drawing pause menus, rectangles, etc.

//
// Helper: Rotate matrix around Y-axis
//
static Mat4 rotateYMatrix(const Mat4 &m, float angle)
{
    Mat4 rot = identityMatrix();
    float c = cosf(angle);
    float s = sinf(angle);
    // Only rotate around Y axis
    rot.m[0]  =  c;
    rot.m[2]  =  s;
    rot.m[8]  = -s;
    rot.m[10] =  c;
    return multiplyMatrix(m, rot);
}

//
// Simple 2D rectangle drawing function
//
static void drawRect2D(float x, float y, float w, float h,
                       float r, float g, float b, float a,
                       int screenW, int screenH)
{
    float vertices[12] = {
        x,   y,
        x+w, y,
        x+w, y+h,
        x,   y,
        x+w, y+h,
        x,   y+h
    };

    static GLuint s_vao=0, s_vbo=0;
    static bool s_init=false;
    if(!s_init){
        glGenVertexArrays(1, &s_vao);
        glGenBuffers(1, &s_vbo);
        s_init = true;
    }

    glBindVertexArray(s_vao);
    glBindBuffer(GL_ARRAY_BUFFER, s_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2*sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    float left = 0.0f, right = (float)screenW;
    float bottom = 0.0f, top = (float)screenH;
    Mat4 ortho = {};
    ortho.m[0]  =  2.0f/(right-left);
    ortho.m[5]  =  2.0f/(top-bottom);
    ortho.m[10] = -1.0f;
    ortho.m[15] =  1.0f;
    ortho.m[12] = -(right+left)/(right-left);
    ortho.m[13] = -(top+bottom)/(top-bottom);

    glUseProgram(uiShader);
    GLint pLoc = glGetUniformLocation(uiShader, "uProj");
    glUniformMatrix4fv(pLoc, 1, GL_FALSE, ortho.m);
    GLint cLoc = glGetUniformLocation(uiShader, "uColor");
    glUniform4f(cLoc, r, g, b, a);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

//
// Helper to draw a block preview
// If hovered, the block rotates; otherwise it remains static.
// We use cached VAO/VBO for efficiency.
//
static void drawBlockPreview(int blockID, float x, float y, float size, bool hovered)
{
    GLint oldViewport[4];
    glGetIntegerv(GL_VIEWPORT, oldViewport);
    glViewport((GLint)x, (GLint)y, (GLsizei)size, (GLsizei)size);
    glClear(GL_DEPTH_BUFFER_BIT);

    glUseProgram(worldShader);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texID);
    GLint uniTex = glGetUniformLocation(worldShader, "ourTexture");
    glUniform1i(uniTex, 0);

    // Set up a small perspective camera with a 45-degree FOV
    Mat4 proj = perspectiveMatrix(45.0f*(3.14159f/180.0f), 1.0f, 0.1f, 100.0f);
    Vec3 eye = {0.0f, 0.0f, 2.0f};
    Vec3 ctr = {0.0f, 0.0f, 0.0f};
    Vec3 up = {0.0f, 1.0f, 0.0f};
    Mat4 view = lookAtMatrix(eye, ctr, up);

    Mat4 model = identityMatrix();
    if(hovered)
    {
        // When hovered, rotate the block
        float angle = (float)SDL_GetTicks()*0.001f;
        model = rotateYMatrix(model, angle);
    }
    // Else: leave the model matrix as identity (static)

    Mat4 mvp = multiplyMatrix(proj, multiplyMatrix(view, model));
    GLint mvpLoc = glGetUniformLocation(worldShader, "MVP");
    glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, mvp.m);

    // Use cached VAO/VBO for the preview cube
    static GLuint previewVAO = 0, previewVBO = 0;
    static bool previewInitialized = false;
    if(!previewInitialized) {
        glGenVertexArrays(1, &previewVAO);
        glGenBuffers(1, &previewVBO);
        previewInitialized = true;
    }
    std::vector<float> verts;
    verts.reserve(36*5);
    addCube(verts, 0.0f, 0.0f, 0.0f, (BlockType)blockID);

    glBindVertexArray(previewVAO);
    glBindBuffer(GL_ARRAY_BUFFER, previewVBO);
    glBufferData(GL_ARRAY_BUFFER, verts.size()*sizeof(float), verts.data(), GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5*sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5*sizeof(float), (void*)(3*sizeof(float)));
    glEnableVertexAttribArray(1);

    glDrawArrays(GL_TRIANGLES, 0, 36);
    glViewport(oldViewport[0], oldViewport[1], oldViewport[2], oldViewport[3]);
}

//
// Inventory constructor: now adds BLOCK_BEDROCK to the inventory.
//
Inventory::Inventory()
    : m_isOpen(false)
    , m_selectedBlock(BLOCK_GRASS)
{
    m_items.push_back(BLOCK_GRASS);
    m_items.push_back(BLOCK_DIRT);
    m_items.push_back(BLOCK_STONE);
    m_items.push_back(BLOCK_SAND);
    m_items.push_back(BLOCK_TREE_LOG);
    m_items.push_back(BLOCK_LEAVES);
    m_items.push_back(BLOCK_WATER);
    m_items.push_back(BLOCK_BEDROCK); // Added bedrock to the inventory
}

void Inventory::toggle()
{
    m_isOpen = !m_isOpen;
}

void Inventory::update(float /*dt*/, Camera &/*camera*/)
{
    if (!m_isOpen) return;

    int mouseX, mouseY;
    Uint32 mState = SDL_GetMouseState(&mouseX, &mouseY);
    bool leftClick = (mState & SDL_BUTTON(SDL_BUTTON_LEFT)) != 0;

    if(leftClick)
    {
        // Invert Y for bottom-left style UI coordinates
        int invY = SCREEN_HEIGHT - mouseY;

        float regionWidth  = 400.0f;
        float regionHeight = 200.0f;
        float regionX = (SCREEN_WIDTH  - regionWidth ) * 0.5f;
        float regionY = (SCREEN_HEIGHT - regionHeight) * 0.5f;

        float itemSize = 64.0f;
        float spacing = 10.0f;
        size_t count = m_items.size();
        float rowWidth = (count * itemSize) + (count - 1) * spacing;
        float startX = regionX + (regionWidth - rowWidth) * 0.5f;
        float startY = regionY + (regionHeight * 0.5f - itemSize * 0.5f);

        for(size_t i = 0; i < count; i++){
            float x = startX + i * (itemSize + spacing);
            float y = startY;
            float w = itemSize;
            float h = itemSize;
            if(mouseX >= x && mouseX <= (x + w) &&
               invY   >= y && invY   <= (y + h))
            {
                m_selectedBlock = m_items[i];
                std::cout << "[Inventory] Selected block: " << m_selectedBlock << std::endl;
            }
        }
    }
}

void Inventory::render()
{
    if(!m_isOpen) return;
    glDisable(GL_DEPTH_TEST);
    float regionWidth  = 400.0f;
    float regionHeight = 200.0f;
    float regionX = (SCREEN_WIDTH  - regionWidth ) * 0.5f;
    float regionY = (SCREEN_HEIGHT - regionHeight) * 0.5f;
    glUseProgram(uiShader);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    drawRect2D(regionX, regionY, regionWidth, regionHeight,
               0.0f, 0.0f, 0.0f, 0.7f,
               SCREEN_WIDTH, SCREEN_HEIGHT);
    glDisable(GL_BLEND);
    float itemSize = 64.0f;
    float spacing = 10.0f;
    size_t count = m_items.size();
    float rowWidth = (count * itemSize) + (count - 1) * spacing;
    float startX = regionX + (regionWidth - rowWidth) * 0.5f;
    float startY = regionY + (regionHeight * 0.5f - itemSize * 0.5f);

    // Determine mouse position for hover detection
    int mouseX, mouseY;
    Uint32 mState = SDL_GetMouseState(&mouseX, &mouseY);
    int invMouseY = SCREEN_HEIGHT - mouseY;

    for(size_t i = 0; i < count; i++){
        float x = startX + i * (itemSize + spacing);
        float y = startY;
        bool hovered = (mouseX >= x && mouseX <= (x + itemSize) && invMouseY >= y && invMouseY <= (y + itemSize));
        // Draw the preview with rotation only if hovered; otherwise static.
        drawBlockPreview(m_items[i], x, y, itemSize, hovered);
    }
    glEnable(GL_DEPTH_TEST);
}

