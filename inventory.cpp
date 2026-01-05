#include "inventory.h"
#include <SDL2/SDL.h>
#include <iostream>
#include <vector>
#include <cmath>

#include "cube.h"     // BlockType + getBlockIconUV
#include "math.h"     // Mat4
#include "shader.h"   // createShaderProgram
#include "texture.h"  // loadTexture

// External symbols defined elsewhere.
extern GLuint texID;        // world atlas (texture.png)
extern int SCREEN_WIDTH;
extern int SCREEN_HEIGHT;
extern GLuint uiShader;     // solid-color UI shader (already initialized in main)

// Simple 2D textured shader for drawing UI quads (frame.png + block atlas)
static const char* uiTexVertSrc2D = R"(
#version 330 core
layout(location=0) in vec2 aPos;
layout(location=1) in vec2 aUV;
uniform mat4 uProj;
out vec2 vUV;
void main(){
    vUV = aUV;
    gl_Position = uProj * vec4(aPos, 0.0, 1.0);
}
)";

static const char* uiTexFragSrc2D = R"(
#version 330 core
in vec2 vUV;
out vec4 FragColor;
uniform sampler2D uTex;
void main(){
    FragColor = texture(uTex, vUV);
}
)";

static GLuint s_uiTexShader2D = 0;
static GLuint s_uiTexVAO2D = 0;
static GLuint s_uiTexVBO2D = 0;
static GLuint s_frameTex = 0;
static bool s_uiTexInit = false;

static Mat4 orthoPixels(int w, int h) {
    Mat4 proj = {};
    proj.m[0]  = 2.0f/(float)w;
    proj.m[5]  = 2.0f/(float)h;
    proj.m[10] = -1.0f;
    proj.m[15] = 1.0f;
    proj.m[12] = -1.0f;
    proj.m[13] = -1.0f;
    return proj;
}

static void ensureTexUIInit() {
    if(s_uiTexInit) return;
    s_uiTexInit = true;

    s_uiTexShader2D = createShaderProgram(uiTexVertSrc2D, uiTexFragSrc2D);

    glGenVertexArrays(1, &s_uiTexVAO2D);
    glGenBuffers(1, &s_uiTexVBO2D);

    glBindVertexArray(s_uiTexVAO2D);
    glBindBuffer(GL_ARRAY_BUFFER, s_uiTexVBO2D);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 24, nullptr, GL_DYNAMIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), (void*)(2*sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);

    // Inventory needs the same frame used in the HUD
    s_frameTex = loadTexture("frame.png");
    if(!s_frameTex) {
        std::cerr << "[Inventory] frame.png failed to load!" << std::endl;
    }
}

// Simple 2D rectangle drawing function (solid color)
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

    static GLuint s_vao = 0, s_vbo = 0;
    static bool s_init = false;
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

    Mat4 ortho = orthoPixels(screenW, screenH);

    glUseProgram(uiShader);
    GLint pLoc = glGetUniformLocation(uiShader, "uProj");
    glUniformMatrix4fv(pLoc, 1, GL_FALSE, ortho.m);
    GLint cLoc = glGetUniformLocation(uiShader, "uColor");
    glUniform4f(cLoc, r, g, b, a);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

static void drawTexturedRectUV(GLuint tex, float x, float y, float w, float h,
                               float u0, float v0, float u1, float v1)
{
    if(!tex) return;
    ensureTexUIInit();

    float v[24] = {
        // pos      // uv
        x,   y,     u0, v0,
        x+w, y,     u1, v0,
        x+w, y+h,   u1, v1,

        x,   y,     u0, v0,
        x+w, y+h,   u1, v1,
        x,   y+h,   u0, v1
    };

    glUseProgram(s_uiTexShader2D);
    Mat4 proj = orthoPixels(SCREEN_WIDTH, SCREEN_HEIGHT);
    glUniformMatrix4fv(glGetUniformLocation(s_uiTexShader2D, "uProj"), 1, GL_FALSE, proj.m);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);
    glUniform1i(glGetUniformLocation(s_uiTexShader2D, "uTex"), 0);

    glBindVertexArray(s_uiTexVAO2D);
    glBindBuffer(GL_ARRAY_BUFFER, s_uiTexVBO2D);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(v), v);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

static void drawTexturedRect(GLuint tex, float x, float y, float w, float h)
{
    drawTexturedRectUV(tex, x, y, w, h, 0.0f, 0.0f, 1.0f, 1.0f);
}

static void drawBlockIcon(int blockID, float x, float y, float sizePx)
{
    if(blockID == BLOCK_NONE) return;

    float uv[4][2];
    if(!getBlockIconUV((BlockType)blockID, uv)) return;

    // Inset inside the frame
    float inset = std::max(4.0f, sizePx * 0.16f);
    float ix = x + inset;
    float iy = y + inset;
    float is = sizePx - inset * 2.0f;
    if(is < 4.0f) is = sizePx;

    drawTexturedRectUV(texID, ix, iy, is, is,
                       uv[0][0], uv[0][1],
                       uv[2][0], uv[2][1]);
}

// Inventory constructor: includes the block types.
Inventory::Inventory()
    : m_isOpen(false)
    , m_selectedBlock(BLOCK_NONE)
{
    // Original blocks
    m_items.push_back(BLOCK_GRASS);
    m_items.push_back(BLOCK_DIRT);
    m_items.push_back(BLOCK_STONE);
    m_items.push_back(BLOCK_SAND);
    m_items.push_back(BLOCK_TREE_LOG);
    m_items.push_back(BLOCK_LEAVES);
    m_items.push_back(BLOCK_WATER);
    m_items.push_back(BLOCK_BEDROCK);
    // New blocks
    m_items.push_back(BLOCK_WOODEN_PLANKS);
    m_items.push_back(BLOCK_COBBLESTONE);
    m_items.push_back(BLOCK_GRAVEL);
    m_items.push_back(BLOCK_BRICKS);
    m_items.push_back(BLOCK_GLASS);
    m_items.push_back(BLOCK_SPONGE);
    m_items.push_back(BLOCK_WOOL_WHITE);
    m_items.push_back(BLOCK_WOOL_RED);
    m_items.push_back(BLOCK_WOOL_BLACK);
    m_items.push_back(BLOCK_WOOL_GREY);
    m_items.push_back(BLOCK_WOOL_PINK);
    m_items.push_back(BLOCK_WOOL_LIME_GREEN);
    m_items.push_back(BLOCK_WOOL_GREEN);
    m_items.push_back(BLOCK_WOOL_BROWN);
    m_items.push_back(BLOCK_WOOL_YELLOW);
    m_items.push_back(BLOCK_WOOL_LIGHT_BLUE);
    m_items.push_back(BLOCK_WOOL_BLUE);
    m_items.push_back(BLOCK_WOOL_PURPLE);
    m_items.push_back(BLOCK_WOOL_VIOLET);
    m_items.push_back(BLOCK_WOOL_TURQUOISE);
    m_items.push_back(BLOCK_WOOL_ORANGE);
}

void Inventory::toggle()
{
    m_isOpen = !m_isOpen;
}

void Inventory::update(float /*dt*/, Camera &/*camera*/)
{
    if(!m_isOpen) return;

    int mouseX, mouseY;
    Uint32 mState = SDL_GetMouseState(&mouseX, &mouseY);
    bool leftClick = (mState & SDL_BUTTON(SDL_BUTTON_LEFT)) != 0;
    if(!leftClick) return;

    float itemSize = 64.0f;
    float spacing = 10.0f;
    int columns = 9;
    size_t count = m_items.size();
    int rows = (count + columns - 1) / columns;

    float gridWidth = columns * itemSize + (columns - 1) * spacing;
    float gridHeight = rows * itemSize + (rows - 1) * spacing;
    float margin = 20.0f;

    float regionWidth = gridWidth + 2 * margin;
    float regionHeight = gridHeight + 2 * margin;
    float regionX = (SCREEN_WIDTH - regionWidth) * 0.5f;
    float regionY = (SCREEN_HEIGHT - regionHeight) * 0.5f;

    // Top-left corner of grid (drawing from the top)
    float startX = regionX + margin;
    float startY = regionY + regionHeight - margin - itemSize;

    int invMouseY = SCREEN_HEIGHT - mouseY;

    for(size_t i = 0; i < count; i++){
        int row = (int)(i / columns);
        int col = (int)(i % columns);
        float x = startX + col * (itemSize + spacing);
        float y = startY - row * (itemSize + spacing);

        if(mouseX >= x && mouseX <= (x + itemSize) &&
           invMouseY >= y && invMouseY <= (y + itemSize))
        {
            m_selectedBlock = m_items[i];
            std::cout << "[Inventory] Selected block: " << m_selectedBlock << std::endl;
        }
    }
}

void Inventory::render()
{
    if(!m_isOpen) return;

    ensureTexUIInit();

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    float itemSize = 64.0f;
    float spacing = 10.0f;
    int columns = 9;
    size_t count = m_items.size();
    int rows = (count + columns - 1) / columns;

    float gridWidth = columns * itemSize + (columns - 1) * spacing;
    float gridHeight = rows * itemSize + (rows - 1) * spacing;
    float margin = 20.0f;

    float regionWidth = gridWidth + 2 * margin;
    float regionHeight = gridHeight + 2 * margin;
    float regionX = (SCREEN_WIDTH - regionWidth) * 0.5f;
    float regionY = (SCREEN_HEIGHT - regionHeight) * 0.5f;

    // Background panel
    drawRect2D(regionX, regionY, regionWidth, regionHeight,
               0.0f, 0.0f, 0.0f, 0.7f,
               SCREEN_WIDTH, SCREEN_HEIGHT);

    // Top-left of the grid (items are drawn from top down)
    float startX = regionX + margin;
    float startY = regionY + regionHeight - margin - itemSize;

    int mouseX, mouseY;
    SDL_GetMouseState(&mouseX, &mouseY);
    int invMouseY = SCREEN_HEIGHT - mouseY;

    for(size_t i = 0; i < count; i++){
        int row = (int)(i / columns);
        int col = (int)(i % columns);
        float x = startX + col * (itemSize + spacing);
        float y = startY - row * (itemSize + spacing);

        bool hovered = (mouseX >= x && mouseX <= (x + itemSize) &&
                        invMouseY >= y && invMouseY <= (y + itemSize));

        // Hover highlight
        if(hovered) {
            drawRect2D(x - 2.0f, y - 2.0f, itemSize + 4.0f, itemSize + 4.0f,
                       1.0f, 1.0f, 1.0f, 0.20f,
                       SCREEN_WIDTH, SCREEN_HEIGHT);
        }

        // Selected highlight
        if(m_items[i] == m_selectedBlock) {
            drawRect2D(x - 2.0f, y - 2.0f, itemSize + 4.0f, itemSize + 4.0f,
                       1.0f, 1.0f, 1.0f, 0.35f,
                       SCREEN_WIDTH, SCREEN_HEIGHT);
        }

        // Frame + static block texture
        drawTexturedRect(s_frameTex, x, y, itemSize, itemSize);
        drawBlockIcon(m_items[i], x, y, itemSize);
    }

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}
