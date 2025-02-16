#include <SDL2/SDL.h>
#include <GL/glew.h>
#include <iostream>
#include <vector>
#include <cmath>
#include <map>
#include <tuple>
#include <cstdlib>  // for rand(), srand
#include "math.h"
#include "shader.h"
#include "cube.h"
#include "camera.h"
#include "texture.h"
#include "noise.h"
#include "world.h"

// -----------------------------
// Window / screen
// -----------------------------
const int SCREEN_WIDTH  = 1280;
const int SCREEN_HEIGHT = 720;

// Terrain parameters
const int g_maxHeight      = 24;   // Max terrain height
const int chunkSize        = 16;   // Each chunk is 16x16 (X,Z)
const int renderDistance   = 8;    // In chunks

// Player collision / kill plane
const float playerWidth    = 0.6f;
const float playerHeight   = 1.8f;
const float WORLD_FLOOR_LIMIT = -10.0f;

// Gravity & jump
static const float GRAVITY    = -9.81f;
static const float JUMP_SPEED =  5.0f;

// A chunk holds geometry (VAO/VBO)
struct Chunk {
    int chunkX, chunkZ;
    std::vector<float> vertices; // Triangles (x,y,z, u,v)
    GLuint VAO, VBO;
};

std::map<std::pair<int,int>, Chunk> chunks; // chunk storage

// -----------------------------
// Biome definitions
// -----------------------------
enum Biome {
    BIOME_PLAINS,
    BIOME_DESERT,
    BIOME_EXTREME_HILLS,
    BIOME_FOREST
};

// Return which biome we are in at x,z
Biome getBiome(int x, int z)
{
    // Combine two Perlin noises so we get smaller biome patches
    float freq1 = 0.0035f;
    float freq2 = 0.0037f;
    float n1 = perlinNoise(x*freq1,       z*freq1);
    float n2 = perlinNoise((x+1000)*freq2,(z+1000)*freq2);
    float combined = 0.5f * (n1 + n2); // ~[-1..1]

    // Example thresholds
    if (combined < -0.4f)
        return BIOME_DESERT;
    else if (combined < -0.1f)
        return BIOME_PLAINS;
    else if (combined <  0.2f)
        return BIOME_FOREST;
    else
        return BIOME_EXTREME_HILLS;
}

// Return if a block type collides
bool blockHasCollision(BlockType t) {
    // Leaves are non-solid
    return (t != BLOCK_LEAVES);
}

// Check if there's a solid block at (bx,by,bz)
bool isSolidBlock(int bx, int by, int bz)
{
    auto key = std::make_tuple(bx, by, bz);
    // Check extra blocks first (trees, etc.)
    if (extraBlocks.find(key) != extraBlocks.end()) {
        BlockType t = extraBlocks[key];
        if (blockHasCollision(t))
            return true;
    }
    // Then terrain
    extern int getTerrainHeightAt(int,int);
    int terrainHeight = getTerrainHeightAt(bx, bz);
    return (by >= 0 && by <= terrainHeight);
}

// Axis-aligned bounding box collision
bool checkCollision(const Vec3 &pos)
{
    float half = playerWidth * 0.5f;
    float minX = pos.x - half;
    float maxX = pos.x + half;
    float minY = pos.y;
    float maxY = pos.y + playerHeight;
    float minZ = pos.z - half;
    float maxZ = pos.z + half;

    int startX = (int)std::floor(minX);
    int endX   = (int)std::floor(maxX);
    int startY = (int)std::floor(minY);
    int endY   = (int)std::floor(maxY);
    int startZ = (int)std::floor(minZ);
    int endZ   = (int)std::floor(maxZ);

    for (int bx = startX; bx <= endX; bx++) {
        for (int by = startY; by <= endY; by++) {
            for (int bz = startZ; bz <= endZ; bz++) {
                if (isSolidBlock(bx, by, bz)) {
                    // Overlap check
                    if (maxX > bx && minX < bx+1 &&
                        maxY > by && minY < by+1 &&
                        maxZ > bz && minZ < bz+1)
                    {
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

// -----------------------------
// Height function
// -----------------------------
int getTerrainHeightAt(int x, int z)
{
    Biome b = getBiome(x,z);
    float n;
    if (b == BIOME_EXTREME_HILLS) {
        n = fbmNoise(x*0.005f, z*0.005f, 6, 2.0f, 0.5f);
    } else {
        n = fbmNoise(x*0.01f, z*0.01f, 6, 2.0f, 0.5f);
    }
    float normalized = 0.5f * (n+1.0f); // [0..1]
    int height = (int)(normalized*g_maxHeight);
    return height;
}

// -----------------------------
// Chunk generation
// -----------------------------
Chunk generateChunk(int cx, int cz)
{
    Chunk chunk;
    chunk.chunkX = cx;
    chunk.chunkZ = cz;

    std::vector<float> verts;
    verts.reserve(16*16*36*5);

    for (int lx=0; lx<16; lx++) {
        for (int lz=0; lz<16; lz++) {
            int wx = cx*16 + lx;
            int wz = cz*16 + lz;

            Biome biome = getBiome(wx,wz);
            int height = getTerrainHeightAt(wx,wz);

            for (int y=0; y<=height; y++) {
                BlockType type;
                if (biome == BIOME_DESERT) {
                    // top 2 = sand, next 3 = dirt, rest = stone
                    const int sandLayers=2;
                    const int dirtLayers=3;
                    if (y >= height - sandLayers)
                        type = BLOCK_SAND;
                    else if (y >= height - (sandLayers + dirtLayers))
                        type = BLOCK_DIRT;
                    else
                        type = BLOCK_STONE;
                } else {
                    // top=grass, next 6=dirt, rest=stone
                    if (y == height)
                        type = BLOCK_GRASS;
                    else if ((height-y)<=6)
                        type = BLOCK_DIRT;
                    else
                        type = BLOCK_STONE;
                }
                addCube(verts,(float)wx,(float)y,(float)wz,type);
            }

            // tree spawning logic
            if (biome != BIOME_DESERT) {
                // forest => 1 in 10, other => 1 in 50
                bool isForest = (biome == BIOME_FOREST);
                int chance = (isForest?10:50);
                if ((rand()%chance)==0) {
                    int trunkH = 4 + (rand()%3); 
                    for (int ty=height+1; ty<=height+trunkH; ty++) {
                        addCube(verts,(float)wx,(float)ty,(float)wz, BLOCK_TREE_LOG);
                        extraBlocks[{wx,ty,wz}] = BLOCK_TREE_LOG;
                    }
                    int topY= height+trunkH;
                    for (int lx2=wx-1; lx2<=wx+1; lx2++) {
                        for (int lz2=wz-1; lz2<=wz+1; lz2++) {
                            if (lx2==wx && lz2==wz) continue;
                            addCube(verts,(float)lx2,(float)topY,(float)lz2, BLOCK_LEAVES);
                            extraBlocks[{lx2, topY, lz2}] = BLOCK_LEAVES;
                        }
                    }
                    // crown
                    addCube(verts,(float)wx,(float)(topY+1),(float)wz, BLOCK_LEAVES);
                    extraBlocks[{wx, topY+1, wz}] = BLOCK_LEAVES;
                }
            }
        }
    }

    chunk.vertices = verts;
    glGenVertexArrays(1,&chunk.VAO);
    glGenBuffers(1,&chunk.VBO);
    glBindVertexArray(chunk.VAO);
    glBindBuffer(GL_ARRAY_BUFFER, chunk.VBO);
    glBufferData(GL_ARRAY_BUFFER,
                 verts.size()*sizeof(float),
                 verts.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,5*sizeof(float),(void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,5*sizeof(float),(void*)(3*sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
    return chunk;
}

// -----------------------------
// 3D Shaders
// -----------------------------
const char* worldVertSrc = R"(
#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec2 aTex;
out vec2 TexCoord;
uniform mat4 MVP;
void main(){
    gl_Position= MVP* vec4(aPos,1.0);
    TexCoord= aTex;
}
)";

const char* worldFragSrc = R"(
#version 330 core
in vec2 TexCoord;
out vec4 FragColor;
uniform sampler2D ourTexture;
void main(){
    FragColor= texture(ourTexture, TexCoord);
}
)";

// -----------------------------
// 2D UI Pipeline
// -----------------------------
const char* uiVertSrc= R"(
#version 330 core
layout(location=0) in vec2 aPos;
uniform mat4 uProj;
void main(){
    gl_Position= uProj* vec4(aPos,0.0,1.0);
}
)";

const char* uiFragSrc= R"(
#version 330 core
out vec4 FragColor;
uniform vec4 uColor;
void main(){
    FragColor= uColor;
}
)";

GLuint uiShader=0;
GLuint uiVAO=0, uiVBO=0;

// Build an orthographic projection for 2D
static Mat4 ortho2D(float left, float right, float bottom, float top)
{
    Mat4 mat={};
    mat.m[0]= 2.0f/(right-left);
    mat.m[5]= 2.0f/(top-bottom);
    mat.m[10]= -1.0f;
    mat.m[15]= 1.0f;
    mat.m[12]= -(right+left)/(right-left);
    mat.m[13]= -(top+bottom)/(top-bottom);
    return mat;
}

// Initialize the 2D UI pipeline
void initUI()
{
    uiShader= createShaderProgram(uiVertSrc, uiFragSrc);
    glGenVertexArrays(1,&uiVAO);
    glGenBuffers(1,&uiVBO);
    glBindVertexArray(uiVAO);
    glBindBuffer(GL_ARRAY_BUFFER, uiVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float)*12, nullptr, GL_DYNAMIC_DRAW);

    glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,2*sizeof(float),(void*)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);
}

// Draw a single colored rectangle in 2D at (x,y) width w,h
void drawRect2D(float x, float y, float w, float h,
                float r, float g, float b, float a,
                int screenW, int screenH)
{
    float v[12] = {
        x,   y,
        x+w, y,
        x+w, y+h,
        x,   y,
        x+w, y+h,
        x,   y+h
    };
    glBindVertexArray(uiVAO);
    glBindBuffer(GL_ARRAY_BUFFER, uiVBO);
    glBufferSubData(GL_ARRAY_BUFFER,0,sizeof(v),v);

    // set ortho
    Mat4 proj= ortho2D(0,(float)screenW, 0,(float)screenH);
    GLint pLoc= glGetUniformLocation(uiShader,"uProj");
    glUniformMatrix4fv(pLoc,1,GL_FALSE,proj.m);

    // color
    GLint cLoc= glGetUniformLocation(uiShader,"uColor");
    glUniform4f(cLoc,r,g,b,a);

    glDrawArrays(GL_TRIANGLES,0,6);
}

// -----------------------------
// Pause menu (2D overlay) logic
// Returns 1=back,2=quit,0=none
// -----------------------------
int drawPauseMenu(int screenW,int screenH)
{
    // disable depth
    glDisable(GL_DEPTH_TEST);
    // alpha blend
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glUseProgram(uiShader);

    // background
    drawRect2D(0,0,(float)screenW,(float)screenH,
               0.0f,0.0f,0.0f,0.5f,
               screenW,screenH);

    // "Back to game" button
    float bx=300, by=250, bw=200, bh=50;
    drawRect2D(bx, by, bw, bh,
               0.2f,0.6f,1.0f,1.0f,
               screenW, screenH);

    // "Quit game" button
    float qx=300, qy=150, qw=200, qh=50;
    drawRect2D(qx, qy, qw, qh,
               1.0f,0.3f,0.3f,1.0f,
               screenW, screenH);

    // check mouse
    int mx,my;
    Uint32 mState= SDL_GetMouseState(&mx,&my);
    bool leftDown= (mState & SDL_BUTTON(SDL_BUTTON_LEFT))!=0;
    int invY= screenH- my;

    int result=0;
    if(leftDown){
        if(mx>=bx && mx<=(bx+bw) && invY>=by && invY<=(by+bh)) {
            result=1;
        }
        else if(mx>=qx && mx<=(qx+qw) && invY>=qy && invY<=(qy+qh)) {
            result=2;
        }
    }

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);

    return result;
}

// -----------------------------
// Fly mode indicator
// We'll just draw a small colored box in top-left corner:
// green for "Flying: ON" and red for "Flying: OFF".
// In a real game, you'd use text rendering to show the words.
// -----------------------------
void drawFlyIndicator(bool isFlying, int screenW, int screenH)
{
    // We'll place a small 20x20 box at top-left.
    float w= 20.0f, h=20.0f;
    float x= 5.0f,  y= (float)screenH- h -5.0f;

    float r=1.0f, g=0.0f, b=0.0f; // red by default
    if(isFlying){
        r= 0.1f; g=1.0f; b=0.1f; // green
    }

    glDisable(GL_DEPTH_TEST);
    glUseProgram(uiShader);
    drawRect2D(x,y,w,h, r,g,b,1.0f, screenW,screenH);
    glEnable(GL_DEPTH_TEST);
}

// -----------------------------
// main.cpp
// -----------------------------
int main(int /*argc*/, char* /*argv*/[])
{
    // Attempt load
    float loadedX=0.0f, loadedY=30.0f, loadedZ=0.0f;
    int loadedSeed=0;
    bool loadedOk= loadWorld("saved_world.txt", loadedSeed, loadedX, loadedY, loadedZ);
    if(loadedOk){
        setNoiseSeed(loadedSeed);
        std::cout<<"[World] Loaded with seed="<<loadedSeed
                 <<" player("<<loadedX<<","<<loadedY<<","<<loadedZ<<")\n";
    } else {
        unsigned int rseed= (unsigned int)time(nullptr);
        std::cout<<"[World] No saved world, random seed="<<rseed<<"\n";
        setNoiseSeed(rseed);
        srand(rseed);
        loadedSeed= (int)rseed;
    }

    if(SDL_Init(SDL_INIT_VIDEO)<0){
        std::cerr<<"SDL_Init Error: "<<SDL_GetError()<<std::endl;
        return -1;
    }
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);

    SDL_Window* window= SDL_CreateWindow("Voxel Engine",
                        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                        SCREEN_WIDTH,SCREEN_HEIGHT,
                        SDL_WINDOW_OPENGL|SDL_WINDOW_SHOWN);
    if(!window){
        std::cerr<<"SDL_CreateWindow Error: "<<SDL_GetError()<<std::endl;
        SDL_Quit();
        return -1;
    }
    SDL_GLContext glContext= SDL_GL_CreateContext(window);
    if(!glContext){
        std::cerr<<"SDL_GL_CreateContext Error: "<<SDL_GetError()<<std::endl;
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }

    glewExperimental=GL_TRUE;
    GLenum glewErr= glewInit();
    if(glewErr!=GLEW_OK){
        std::cerr<<"GLEW Error: "<< glewGetErrorString(glewErr)<<std::endl;
        SDL_GL_DeleteContext(glContext);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }
    SDL_GL_SetSwapInterval(1); // attempt vsync

    glEnable(GL_DEPTH_TEST);

    // 3D pipeline
    GLuint worldShader= createShaderProgram(worldVertSrc, worldFragSrc);
    GLuint texID= loadTexture("texture.png");
    if(!texID){
        std::cerr<<"Texture failed to load!\n";
        SDL_GL_DeleteContext(glContext);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }

    // 2D UI pipeline
    initUI();

    // Generate chunk for spawn
    int spawnChunkX= (int)std::floor(loadedX/chunkSize);
    int spawnChunkZ= (int)std::floor(loadedZ/chunkSize);
    auto chunkKey= std::make_pair(spawnChunkX, spawnChunkZ);
    if(chunks.find(chunkKey)==chunks.end()){
        chunks[chunkKey]= generateChunk(spawnChunkX, spawnChunkZ);
    }

    // Camera setup
    Camera camera;
    camera.position= {loadedX, loadedY, loadedZ};
    camera.yaw   = -3.14f/2;
    camera.pitch =  0.0f;

    bool paused= false;
    bool isFlying= false; // new: track if fly mode is ON

    float verticalVelocity= 0.0f;

    // Lock/hide mouse for normal FPS look
    SDL_SetRelativeMouseMode(SDL_TRUE);

    Uint32 lastTime= SDL_GetTicks();
    bool running= true;
    SDL_Event ev;

    while(running){
        Uint32 now= SDL_GetTicks();
        float dt= (now- lastTime)/1000.0f;
        lastTime= now;

        // Poll events
        while(SDL_PollEvent(&ev)){
            if(ev.type== SDL_QUIT){
                running=false;
            }
            else if(ev.type== SDL_KEYDOWN){
                // Esc => pause toggle
                if(ev.key.keysym.sym== SDLK_ESCAPE){
                    paused= !paused;
                    if(paused) {
                        SDL_SetRelativeMouseMode(SDL_FALSE);
                    } else {
                        SDL_SetRelativeMouseMode(SDL_TRUE);
                    }
                }
                // F => toggle fly
                else if(ev.key.keysym.sym== SDLK_f){
                    isFlying= !isFlying;
                    if(isFlying){
                        // turning on => no gravity
                        verticalVelocity=0.0f;
                        std::cout<<"[Fly] Fly mode ON\n";
                    }
                    else {
                        // turning off => normal gravity
                        std::cout<<"[Fly] Fly mode OFF\n";
                    }
                }
                // Jump => only if not paused & not flying
                else if(!paused && !isFlying && ev.key.keysym.sym== SDLK_SPACE){
                    // check block underfoot
                    int footX= (int)std::floor(camera.position.x);
                    int footY= (int)std::floor(camera.position.y-0.1f);
                    int footZ= (int)std::floor(camera.position.z);
                    if(isSolidBlock(footX, footY, footZ)){
                        verticalVelocity= JUMP_SPEED;
                    }
                }
            }
            // Mouse motion => rotate camera if not paused
            else if(ev.type==SDL_MOUSEMOTION){
                if(!paused){
                    float sens= 0.002f;
                    camera.yaw   += ev.motion.xrel*sens;
                    camera.pitch -= ev.motion.yrel*sens;
                    // clamp pitch
                    if(camera.pitch> 1.57f)  camera.pitch=1.57f;
                    if(camera.pitch<-1.57f)  camera.pitch=-1.57f;
                }
            }
        }

        if(paused){
            // Re-draw 3D scene, then menu
            glClearColor(0.53f,0.81f,0.92f,1.0f);
            glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
            glUseProgram(worldShader);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, texID);
            GLint tLoc= glGetUniformLocation(worldShader,"ourTexture");
            glUniform1i(tLoc,0);

            // Basic camera
            Vec3 eyePos= camera.position;
            eyePos.y += 1.6f;
            Vec3 viewDir= {
                cos(camera.yaw)*cos(camera.pitch),
                sin(camera.pitch),
                sin(camera.yaw)*cos(camera.pitch)
            };
            Vec3 camTgt= add(eyePos, viewDir);

            Mat4 view= lookAtMatrix(eyePos, camTgt,{0,1,0});
            Mat4 proj= perspectiveMatrix(
                45.0f*3.14159f/180.0f,
                (float)SCREEN_WIDTH/(float)SCREEN_HEIGHT,
                0.1f, 100.0f
            );
            Mat4 pv= multiplyMatrix(proj, view);

            // draw chunks
            int pcx= (int)std::floor(camera.position.x/chunkSize);
            int pcz= (int)std::floor(camera.position.z/chunkSize);

            for(auto &pair: chunks){
                int cX= pair.first.first;
                int cZ= pair.first.second;
                if(std::abs(cX-pcx)> renderDistance
                   || std::abs(cZ-pcz)> renderDistance)
                    continue;

                Chunk &ch= pair.second;
                Mat4 mvp= multiplyMatrix(pv, identityMatrix());
                GLint mvpLoc= glGetUniformLocation(worldShader,"MVP");
                glUniformMatrix4fv(mvpLoc,1,GL_FALSE,mvp.m);
                glBindVertexArray(ch.VAO);
                glDrawArrays(GL_TRIANGLES,0,(GLsizei)(ch.vertices.size()/5));
            }

            // Pause overlay
            int clicked= drawPauseMenu(SCREEN_WIDTH, SCREEN_HEIGHT);
            if(clicked==1){
                // Back
                paused=false;
                SDL_SetRelativeMouseMode(SDL_TRUE);
            }
            else if(clicked==2){
                // Quit => save & exit
                saveWorld("saved_world.txt", loadedSeed,
                          camera.position.x,
                          camera.position.y,
                          camera.position.z);
                running=false;
            }

            SDL_GL_SwapWindow(window);
            continue;
        }

        // If not paused => update logic
        const Uint8* keys= SDL_GetKeyboardState(nullptr);
        float speed= 10.0f* dt;

        // Basic forward / right vectors
        Vec3 forward= { cos(camera.yaw), 0, sin(camera.yaw) };
        forward= normalize(forward);
        Vec3 right= normalize(cross(forward,{0,1,0}));

        // Horizontal movement
        Vec3 horizontalDelta= {0,0,0};
        if(keys[SDL_SCANCODE_W])
            horizontalDelta = add(horizontalDelta, multiply(forward, speed));
        if(keys[SDL_SCANCODE_S])
            horizontalDelta = subtract(horizontalDelta, multiply(forward, speed));
        if(keys[SDL_SCANCODE_A])
            horizontalDelta = subtract(horizontalDelta, multiply(right, speed));
        if(keys[SDL_SCANCODE_D])
            horizontalDelta = add(horizontalDelta, multiply(right, speed));

        // Apply horizontal collision
        Vec3 newPosH= camera.position;
        newPosH.x += horizontalDelta.x;
        newPosH.z += horizontalDelta.z;
        if(!checkCollision(newPosH)){
            camera.position.x= newPosH.x;
            camera.position.z= newPosH.z;
        }

        // If in fly mode => no gravity; space/shift move up/down
        if(isFlying){
            // No gravity
            verticalVelocity=0.0f;

            float flySpeed= 10.0f* dt; // or differ from normal movement
            // Space => up, Shift => down
            if(keys[SDL_SCANCODE_SPACE]) {
                Vec3 upPos= camera.position;
                upPos.y += flySpeed;
                if(!checkCollision(upPos)) {
                    camera.position.y= upPos.y;
                }
            }
            if(keys[SDL_SCANCODE_LSHIFT]) {
                Vec3 downPos= camera.position;
                downPos.y -= flySpeed;
                if(!checkCollision(downPos)) {
                    camera.position.y= downPos.y;
                }
            }
        }
        else {
            // Normal gravity
            verticalVelocity += GRAVITY* dt;
            float dy = verticalVelocity* dt;
            Vec3 newPosV= camera.position;
            newPosV.y += dy;
            if(!checkCollision(newPosV)){
                camera.position.y= newPosV.y;
            } else {
                verticalVelocity= 0.0f;
            }
        }

        // Kill plane
        if(camera.position.y < WORLD_FLOOR_LIMIT){
            std::cout<<"[World] Player fell below kill plane => reset.\n";
            camera.position.y= 30.0f; 
            // or back to loaded pos if you prefer
            verticalVelocity=0.0f;
        }

        // Chunk loading
        int pcx= (int)std::floor(camera.position.x/chunkSize);
        int pcz= (int)std::floor(camera.position.z/chunkSize);
        for(int cx= pcx-renderDistance; cx<= pcx+renderDistance; cx++){
            for(int cz= pcz-renderDistance; cz<= pcz+renderDistance; cz++){
                auto key= std::make_pair(cx,cz);
                if(chunks.find(key)==chunks.end()){
                    chunks[key]= generateChunk(cx,cz);
                }
            }
        }

        // Render
        glClearColor(0.53f,0.81f,0.92f,1.0f);
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        glUseProgram(worldShader);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texID);
        GLint uniTex= glGetUniformLocation(worldShader,"ourTexture");
        glUniform1i(uniTex,0);

        // Setup camera
        Vec3 eyePos= camera.position;
        eyePos.y += 1.6f; // eye offset
        Vec3 viewDir= {
            cos(camera.yaw)*cos(camera.pitch),
            sin(camera.pitch),
            sin(camera.yaw)*cos(camera.pitch)
        };
        Vec3 camTarget= add(eyePos, viewDir);

        Mat4 view= lookAtMatrix(eyePos, camTarget, {0,1,0});
        Mat4 proj= perspectiveMatrix(
            45.0f*3.14159f/180.0f,
            (float)SCREEN_WIDTH/(float)SCREEN_HEIGHT,
            0.1f, 100.0f
        );
        Mat4 pv= multiplyMatrix(proj, view);

        for(auto &pair: chunks){
            int cX= pair.first.first;
            int cZ= pair.first.second;
            if(std::abs(cX-pcx)> renderDistance
               || std::abs(cZ-pcz)> renderDistance)
                continue;

            Chunk &ch= pair.second;
            Mat4 mvp= multiplyMatrix(pv, identityMatrix());
            GLint mvpLoc= glGetUniformLocation(worldShader,"MVP");
            glUniformMatrix4fv(mvpLoc,1,GL_FALSE,mvp.m);

            glBindVertexArray(ch.VAO);
            glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(ch.vertices.size()/5));
        }

        // After 3D, draw the small "flying" indicator in top-left
        glUseProgram(uiShader);
        drawFlyIndicator(isFlying, SCREEN_WIDTH, SCREEN_HEIGHT);

        SDL_GL_SwapWindow(window);
    }

    // On exit, save
    saveWorld("saved_world.txt", loadedSeed,
              camera.position.x, camera.position.y, camera.position.z);

    glDeleteProgram(worldShader);
    glDeleteProgram(uiShader);
    glDeleteVertexArrays(1,&uiVAO);
    glDeleteBuffers(1,&uiVBO);

    SDL_GL_DeleteContext(glContext);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}

