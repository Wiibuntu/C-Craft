#include "inventory.h"
#include <SDL2/SDL.h>
#include <iostream>
#include <vector>
#include <cmath>

#include "cube.h"    // for addCube(...) to generate block vertices
#include "math.h"    // for Mat4, Vec3, perspectiveMatrix, lookAtMatrix, identityMatrix, etc.
#include "texture.h" // if your globals or texture loading are declared there
#include "world.h"   // for BLOCK_GRASS, BLOCK_STONE, etc.

// These symbols must be defined in exactly one .cpp (e.g., main.cpp) but declared extern in a shared header:
extern GLuint worldShader;   // The 3D shader used by your main scene
extern GLuint texID;         // The block texture
extern int SCREEN_WIDTH;     // e.g., 1280
extern int SCREEN_HEIGHT;    // e.g., 720

// If you have a separate UI shader for 2D rectangles:
extern GLuint uiShader;      // e.g., used for drawing pause menus, rectangles, etc.

// -----------------------------------------------------------------------------------
// A local helper to rotate a matrix around the Y-axis, if your math library lacks one.
// -----------------------------------------------------------------------------------
static Mat4 rotateYMatrix(const Mat4 &m, float angle)
{
    Mat4 rot = identityMatrix();
    float c = cosf(angle);
    float s = sinf(angle);

    // Assuming column-major:
    rot.m[0]  =  c;
    rot.m[2]  =  s;
    rot.m[8]  = -s;
    rot.m[10] =  c;

    return multiplyMatrix(m, rot);
}

// -----------------------------------------------------------------------------------
// (Optional) A simple 2D rectangle-draw function if you have a UI pipeline
// -----------------------------------------------------------------------------------
static void drawRect2D(float x, float y, float w, float h,
                       float r, float g, float b, float a,
                       int screenW, int screenH)
{
    // Build a simple quad and an orthographic projection
    float vertices[12] = {
        x,   y,
        x+w, y,
        x+w, y+h,
        x,   y,
        x+w, y+h,
        x,   y+h
    };

    // We'll need a VAO/VBO for drawing
    static GLuint s_vao=0, s_vbo=0;
    static bool s_init=false;
    if(!s_init){
        glGenVertexArrays(1, &s_vao);
        glGenBuffers(1, &s_vbo);
        s_init=true;
    }

    glBindVertexArray(s_vao);
    glBindBuffer(GL_ARRAY_BUFFER, s_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2*sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // Create an orthographic matrix
    float left=0.0f, right=(float)screenW;
    float bottom=0.0f, top=(float)screenH;
    Mat4 ortho={};
    ortho.m[0]  =  2.0f/(right-left);
    ortho.m[5]  =  2.0f/(top-bottom);
    ortho.m[10] = -1.0f;
    ortho.m[15] =  1.0f;
    ortho.m[12] = -(right+left)/(right-left);
    ortho.m[13] = -(top+bottom)/(top-bottom);

    // Use your existing UI shader
    glUseProgram(uiShader);

    // Pass the ortho matrix
    GLint pLoc= glGetUniformLocation(uiShader,"uProj");
    glUniformMatrix4fv(pLoc, 1, GL_FALSE, ortho.m);

    // Pass color
    GLint cLoc= glGetUniformLocation(uiShader,"uColor");
    glUniform4f(cLoc, r,g,b,a);

    glDrawArrays(GL_TRIANGLES, 0, 6);
}

// -----------------------------------------------------------------------------------
// A helper to draw a single block in a small sub-viewport
// -----------------------------------------------------------------------------------
static void drawBlockPreview(int blockID, float x, float y, float size)
{
    // Save current viewport
    GLint oldViewport[4];
    glGetIntegerv(GL_VIEWPORT, oldViewport);

    // Set a small viewport in [x, x+size], [y, y+size]
    glViewport((GLint)x, (GLint)y, (GLsizei)size, (GLsizei)size);

    // Clear just the depth in that region
    glClear(GL_DEPTH_BUFFER_BIT);

    // Reuse the main 3D shader and texture
    glUseProgram(worldShader);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texID);
    GLint uniTex= glGetUniformLocation(worldShader, "ourTexture");
    glUniform1i(uniTex, 0);

    // Build a small perspective camera
    Mat4 proj= perspectiveMatrix(45.0f*(3.14159f/180.0f),
                                 1.0f, 0.1f, 100.0f);
    Vec3 eye= {0.0f, 0.0f, 2.0f};  // Closer to the block, so it appears bigger
    Vec3 ctr= {0.0f, 0.0f, 0.0f};
    Vec3 up = {0.0f, 1.0f, 0.0f};
    Mat4 view= lookAtMatrix(eye, ctr, up);

    // Rotate around Y
    float timeSec= (float)SDL_GetTicks()*0.001f;
    Mat4 model= identityMatrix();
    model= rotateYMatrix(model, timeSec);

    // Combine
    Mat4 mvp= multiplyMatrix(proj, multiplyMatrix(view, model));
    GLint mvpLoc= glGetUniformLocation(worldShader, "MVP");
    glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, mvp.m);

    // Create a small VAO for a single 1x1 block
    std::vector<float> verts;
    verts.reserve(36*5);
    addCube(verts, 0.0f, 0.0f, 0.0f, (BlockType)blockID);

    GLuint vbo=0, vao=0;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, verts.size()*sizeof(float),
                 verts.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,5*sizeof(float),(void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,5*sizeof(float),(void*)(3*sizeof(float)));
    glEnableVertexAttribArray(1);

    // Draw
    glDrawArrays(GL_TRIANGLES, 0, 36);

    // Cleanup
    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);

    // Restore old viewport
    glViewport(oldViewport[0], oldViewport[1],
               oldViewport[2], oldViewport[3]);
}

// -----------------------------------------------------------------------------------
// Inventory constructor
// -----------------------------------------------------------------------------------
Inventory::Inventory()
    : m_isOpen(false)
    , m_selectedBlock(BLOCK_GRASS)
{
    // Populate the inventory with whichever blocks you want
    m_items.push_back(BLOCK_GRASS);
    m_items.push_back(BLOCK_DIRT);
    m_items.push_back(BLOCK_STONE);
    m_items.push_back(BLOCK_SAND);
    m_items.push_back(BLOCK_TREE_LOG);
    m_items.push_back(BLOCK_LEAVES);
}

// -----------------------------------------------------------------------------------
// Toggle open/close
// -----------------------------------------------------------------------------------
void Inventory::toggle()
{
    m_isOpen = !m_isOpen;
}

// -----------------------------------------------------------------------------------
// Update the inventory logic (mouse input, selecting blocks, etc.)
// -----------------------------------------------------------------------------------
void Inventory::update(float /*dt*/, Camera &/*camera*/)
{
    if (!m_isOpen) return;

    int mouseX, mouseY;
    Uint32 mState= SDL_GetMouseState(&mouseX, &mouseY);
    bool leftClick= (mState & SDL_BUTTON(SDL_BUTTON_LEFT))!=0;

    if(leftClick)
    {
        // invert Y for our bottom-left style UI
        int invY= SCREEN_HEIGHT - mouseY;

        // We'll define the size of our entire inventory region
        float regionWidth  = 400.0f;  // Adjust as desired
        float regionHeight = 200.0f;

        // Center these region coords in the window
        float regionX= (SCREEN_WIDTH  - regionWidth ) * 0.5f;
        float regionY= (SCREEN_HEIGHT - regionHeight) * 0.5f;

        // We'll place the block previews in a single row near the center
        float itemSize= 64.0f;
        float spacing = 10.0f;
        size_t count= m_items.size();

        // Compute how wide the row of items will be
        float rowWidth= (count*itemSize) + (count-1)*spacing;
        // We'll horizontally center them inside regionWidth
        float startX= regionX + (regionWidth - rowWidth)*0.5f;
        // We'll place them near the center of the region's height
        float startY= regionY + (regionHeight*0.5f - itemSize*0.5f);

        // Check each item to see if the mouse is inside
        for(size_t i=0; i<count; i++){
            float x = startX + i*(itemSize + spacing);
            float y = startY;
            float w = itemSize;
            float h = itemSize;

            // bounding box test
            if(mouseX >= x && mouseX <= (x+w) &&
               invY   >= y && invY   <= (y+h))
            {
                m_selectedBlock= m_items[i];
                std::cout << "[Inventory] Selected block: "
                          << m_selectedBlock << std::endl;
            }
        }
    }
}

// -----------------------------------------------------------------------------------
// Render the inventory overlay in the center of the window
// -----------------------------------------------------------------------------------
void Inventory::render()
{
    if(!m_isOpen) return;

    // Disable depth so the overlay is always on top
    glDisable(GL_DEPTH_TEST);

    // We'll define the region size (centered)
    float regionWidth  = 400.0f;
    float regionHeight = 200.0f;

    float regionX= (SCREEN_WIDTH  - regionWidth ) * 0.5f;
    float regionY= (SCREEN_HEIGHT - regionHeight) * 0.5f;

    // 1) Draw a semi-transparent background rectangle
    //    (Requires you have an active UI shader, e.g. uiShader)
    glUseProgram(uiShader);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // For instance:
    drawRect2D(regionX, regionY, regionWidth, regionHeight,
               0.0f, 0.0f, 0.0f, 0.7f,
               SCREEN_WIDTH, SCREEN_HEIGHT);

    glDisable(GL_BLEND);

    // 2) Now draw the row of block previews in the center
    float itemSize= 64.0f;
    float spacing = 10.0f;
    size_t count= m_items.size();
    float rowWidth= (count*itemSize) + (count-1)*spacing;
    float startX= regionX + (regionWidth - rowWidth)*0.5f;
    float startY= regionY + (regionHeight*0.5f - itemSize*0.5f);

    // We'll reuse the same 3D pipeline from the main scene
    for(size_t i=0; i<count; i++){
        float x= startX + i*(itemSize + spacing);
        float y= startY;
        drawBlockPreview(m_items[i], x, y, itemSize);
    }

    // Re-enable depth
    glEnable(GL_DEPTH_TEST);
}

