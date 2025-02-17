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
#include "inventory.h"

// -----------------------------------------------------------------------------
// GLOBALS
// -----------------------------------------------------------------------------
int SCREEN_WIDTH  = 1280; 
int SCREEN_HEIGHT = 720;

// The terrain chunk dimension
static const int chunkSize      = 16;
// Render distance in chunks
static const int renderDistance = 8;

// Player bounding box
static const float playerWidth  = 0.6f;
static const float playerHeight = 1.8f;
static const float WORLD_FLOOR_LIMIT = -10.0f;

// Gravity & jump
static const float GRAVITY    = -9.81f;
static const float JUMP_SPEED =  5.0f;

// The main 3D shader and block texture
GLuint worldShader = 0;
GLuint texID       = 0;

// A separate UI shader for 2D overlays
GLuint uiShader = 0;
GLuint uiVAO    = 0;
GLuint uiVBO    = 0;

// -----------------------------------------------------------------------------
// REPLACING BLOCK_AIR WITH A SENTINEL
// We define a negative BlockType to mark "removed" blocks
// so we don't need to add BLOCK_AIR to our enum.
// -----------------------------------------------------------------------------
static const BlockType BLOCK_REMOVED = (BlockType)(-1);

// -----------------------------------------------------------------------------
// A chunk holds its vertex data + VAO
// -----------------------------------------------------------------------------
struct Chunk {
    int chunkX, chunkZ;
    std::vector<float> vertices; 
    GLuint VAO, VBO;
};

std::map<std::pair<int,int>, Chunk> chunks; // chunk storage

// -----------------------------------------------------------------------------
// Biome definitions
// -----------------------------------------------------------------------------
enum Biome {
    BIOME_PLAINS,
    BIOME_DESERT,
    BIOME_EXTREME_HILLS,
    BIOME_FOREST
};

// For retrieving chunk coords from world coords
static void getChunkCoords(int bx, int bz, int &cx, int &cz)
{
    cx= bx/16;
    if(bx<0 && bx%16!=0) cx--;
    cz= bz/16;
    if(bz<0 && bz%16!=0) cz--;
}

// -----------------------------------------------------------------------------
// Biome & terrain
// -----------------------------------------------------------------------------
static Biome getBiome(int x, int z)
{
    float freq1 = 0.0035f;
    float freq2 = 0.0037f;
    float n1 = perlinNoise(x*freq1,       z*freq1);
    float n2 = perlinNoise((x+1000)*freq2,(z+1000)*freq2);
    float combined = 0.5f * (n1 + n2);

    if (combined < -0.4f)
        return BIOME_DESERT;
    else if (combined < -0.1f)
        return BIOME_PLAINS;
    else if (combined <  0.2f)
        return BIOME_FOREST;
    else
        return BIOME_EXTREME_HILLS;
}

// Returns whether a block is "solid" for collision
static bool blockHasCollision(BlockType t)
{
    // Leaves are pass-through
    return (t != BLOCK_LEAVES);
}

// Check if there's a solid block at (bx,by,bz)
bool isSolidBlock(int bx, int by, int bz)
{
    auto key = std::make_tuple(bx,by,bz);
    // Check extraBlocks first
    if(extraBlocks.find(key) != extraBlocks.end()){
        BlockType t= extraBlocks[key];
        if(t == BLOCK_REMOVED) return false; // "removed" means empty
        return blockHasCollision(t);
    }
    // Then terrain
    extern int getTerrainHeightAt(int,int);
    int height = getTerrainHeightAt(bx,bz);
    if(by >= 0 && by <= height){
        // Figure out what block it is for collisions
        // If you want leaves or something to be pass-through, do so.
        // For now, we say any terrain block is solid except leaves,
        // but we didn't define leaves for base terrain. So it's solid.
        return true;
    }
    return false;
}

int getTerrainHeightAt(int x, int z)
{
    Biome b= getBiome(x,z);
    float n;
    if(b == BIOME_EXTREME_HILLS){
        n= fbmNoise(x*0.005f, z*0.005f, 6, 2.0f, 0.5f);
    } else {
        n= fbmNoise(x*0.01f, z*0.01f, 6, 2.0f, 0.5f);
    }
    float normalized= 0.5f*(n+1.0f);
    int h= (int)(normalized*24); // maxHeight=24
    return h;
}

// -----------------------------------------------------------------------------
// Check collision for the player
// -----------------------------------------------------------------------------
static bool checkCollision(const Vec3 &pos)
{
    float half= playerWidth*0.5f;
    float minX= pos.x - half;
    float maxX= pos.x + half;
    float minY= pos.y;
    float maxY= pos.y + playerHeight;
    float minZ= pos.z - half;
    float maxZ= pos.z + half;

    int startX= (int)std::floor(minX);
    int endX  = (int)std::floor(maxX);
    int startY= (int)std::floor(minY);
    int endY  = (int)std::floor(maxY);
    int startZ= (int)std::floor(minZ);
    int endZ  = (int)std::floor(maxZ);

    for(int bx= startX; bx<= endX; bx++){
        for(int by= startY; by<= endY; by++){
            for(int bz= startZ; bz<= endZ; bz++){
                if(isSolidBlock(bx,by,bz)){
                    // Overlap check
                    if(maxX > bx && minX < bx+1 &&
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

// -----------------------------------------------------------------------------
// generateChunk(...) - used for initial chunk creation
// -----------------------------------------------------------------------------
static Chunk generateChunk(int cx, int cz)
{
    Chunk chunk;
    chunk.chunkX= cx;
    chunk.chunkZ= cz;

    std::vector<float> verts;
    verts.reserve(16*16*36*5);

    for(int lx=0; lx<16; lx++){
        for(int lz=0; lz<16; lz++){
            int wx= cx*16 + lx;
            int wz= cz*16 + lz;
            Biome biome= getBiome(wx,wz);
            int height= getTerrainHeightAt(wx,wz);

            // base terrain
            for(int y=0; y<= height; y++){
                BlockType type;
                if(biome == BIOME_DESERT){
                    // top2=sand, next3=dirt, rest=stone
                    const int sandLayers=2;
                    const int dirtLayers=3;
                    if(y >= height - sandLayers)
                        type= BLOCK_SAND;
                    else if(y >= height-(sandLayers+dirtLayers))
                        type= BLOCK_DIRT;
                    else
                        type= BLOCK_STONE;
                } else {
                    // top=grass, next6=dirt, rest=stone
                    if(y== height)
                        type= BLOCK_GRASS;
                    else if((height-y)<=6)
                        type= BLOCK_DIRT;
                    else
                        type= BLOCK_STONE;
                }
                addCube(verts,(float)wx,(float)y,(float)wz,type);
            }

            // possibly spawn a tree
            if (biome != BIOME_DESERT) {
                bool isForest= (biome==BIOME_FOREST);
                int chance= (isForest? 10:50);
                if((rand()%chance)==0){
                    int trunkH= 4+(rand()%3);
                    for(int ty= height+1; ty<= height+ trunkH; ty++){
                        addCube(verts,(float)wx,(float)ty,(float)wz, BLOCK_TREE_LOG);
                        extraBlocks[{wx,ty,wz}] = BLOCK_TREE_LOG;
                    }
                    int topY= height+ trunkH;
                    for(int lx2= wx-1; lx2<= wx+1; lx2++){
                        for(int lz2= wz-1; lz2<= wz+1; lz2++){
                            if(lx2== wx && lz2== wz) continue;
                            addCube(verts,(float)lx2,(float)topY,(float)lz2, BLOCK_LEAVES);
                            extraBlocks[{lx2, topY, lz2}]= BLOCK_LEAVES;
                        }
                    }
                    // crown
                    addCube(verts,(float)wx,(float)(topY+1),(float)wz, BLOCK_LEAVES);
                    extraBlocks[{wx, topY+1, wz}] = BLOCK_LEAVES;
                }
            }
        }
    }

    chunk.vertices= verts;
    glGenVertexArrays(1,&chunk.VAO);
    glGenBuffers(1,&chunk.VBO);
    glBindVertexArray(chunk.VAO);
    glBindBuffer(GL_ARRAY_BUFFER, chunk.VBO);
    glBufferData(GL_ARRAY_BUFFER, 
                 verts.size()*sizeof(float),
                 verts.data(), 
                 GL_STATIC_DRAW);

    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,5*sizeof(float),(void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,5*sizeof(float),(void*)(3*sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
    return chunk;
}

// -----------------------------------------------------------------------------
// Rebuild geometry for an existing chunk, merging base terrain + extraBlocks
// -----------------------------------------------------------------------------
static void rebuildChunk(int cx, int cz)
{
    Chunk &chunk= chunks[{cx,cz}];

    std::vector<float> verts;
    verts.reserve(16*16*36*5);

    for(int lx=0; lx<16; lx++){
        for(int lz=0; lz<16; lz++){
            int wx= cx*16 + lx;
            int wz= cz*16 + lz;
            int height= getTerrainHeightAt(wx,wz);

            for(int y=0; y<=height; y++){
                // if there's an override in extraBlocks...
                auto it= extraBlocks.find({wx,y,wz});
                if(it != extraBlocks.end()){
                    BlockType overrideT= it->second;
                    if(overrideT == BLOCK_REMOVED){
                        // skip geometry (the block was destroyed)
                        continue;
                    }
                    // otherwise, draw the override block
                    addCube(verts,(float)wx,(float)y,(float)wz, overrideT);
                }
                else {
                    // normal terrain
                    Biome biome= getBiome(wx,wz);
                    BlockType type;
                    if(biome== BIOME_DESERT){
                        const int sandLayers=2;
                        const int dirtLayers=3;
                        if(y >= height - sandLayers)
                            type= BLOCK_SAND;
                        else if(y >= height-(sandLayers+dirtLayers))
                            type= BLOCK_DIRT;
                        else
                            type= BLOCK_STONE;
                    } else {
                        if(y== height)
                            type= BLOCK_GRASS;
                        else if((height-y)<=6)
                            type= BLOCK_DIRT;
                        else
                            type= BLOCK_STONE;
                    }
                    addCube(verts,(float)wx,(float)y,(float)wz,type);
                }
            }
        }
    }

    // Then handle blocks above terrain (trees, user-placed, etc.)
    // If it's "BLOCK_REMOVED", skip it.
    for(auto &kv : extraBlocks){
        int bx= std::get<0>(kv.first);
        int by= std::get<1>(kv.first);
        int bz= std::get<2>(kv.first);
        BlockType bt= kv.second;

        // belongs to this chunk?
        int ccx= bx/16; 
        if(bx<0 && bx%16!=0) ccx--;
        int ccz= bz/16;
        if(bz<0 && bz%16!=0) ccz--;

        if(ccx==cx && ccz==cz){
            // skip if it's removed
            if(bt == BLOCK_REMOVED) continue;
            // otherwise, draw
            addCube(verts, (float)bx,(float)by,(float)bz, bt);
        }
    }

    chunk.vertices= verts;

    glBindVertexArray(chunk.VAO);
    glBindBuffer(GL_ARRAY_BUFFER, chunk.VBO);
    glBufferData(GL_ARRAY_BUFFER,
                 verts.size()*sizeof(float),
                 verts.data(),
                 GL_STATIC_DRAW);

    glBindVertexArray(0);
}

// -----------------------------------------------------------------------------
// Basic UI pipeline
// -----------------------------------------------------------------------------
static const char* uiVertSrc= R"(
#version 330 core
layout(location=0) in vec2 aPos;
uniform mat4 uProj;
void main(){
    gl_Position= uProj* vec4(aPos,0.0,1.0);
}
)";

static const char* uiFragSrc= R"(
#version 330 core
out vec4 FragColor;
uniform vec4 uColor;
void main(){
    FragColor= uColor;
}
)";

static void initUI()
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

static void drawRect2D(float x,float y,float w,float h,
                       float r,float g,float b,float a,
                       int screenW,int screenH)
{
    float v[12]={
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

    // basic orthographic
    Mat4 proj={};
    proj.m[0]= 2.0f/(float)screenW;
    proj.m[5]= 2.0f/(float)screenH;
    proj.m[10]= -1.0f;
    proj.m[15]= 1.0f;
    proj.m[12]= -1.0f;
    proj.m[13]= -1.0f;

    glUseProgram(uiShader);
    GLint pLoc= glGetUniformLocation(uiShader,"uProj");
    glUniformMatrix4fv(pLoc,1,GL_FALSE,proj.m);

    GLint cLoc= glGetUniformLocation(uiShader,"uColor");
    glUniform4f(cLoc,r,g,b,a);

    glDrawArrays(GL_TRIANGLES,0,6);
}

// Pause menu
static int drawPauseMenu(int screenW,int screenH)
{
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glUseProgram(uiShader);
    // background
    drawRect2D(0,0,(float)screenW,(float)screenH,
               0.0f,0.0f,0.0f,0.5f,
               screenW,screenH);

    // "Back to game" button
    float bx=300,by=250,bw=200,bh=50;
    drawRect2D(bx,by,bw,bh,
               0.2f,0.6f,1.0f,1.0f,
               screenW,screenH);

    // "Quit game" button
    float qx=300,qy=150,qw=200,qh=50;
    drawRect2D(qx,qy,qw,qh,
               1.0f,0.3f,0.3f,1.0f,
               screenW,screenH);

    int mx,my;
    Uint32 mState= SDL_GetMouseState(&mx,&my);
    bool leftDown= (mState & SDL_BUTTON(SDL_BUTTON_LEFT))!=0;
    int invY= screenH-my;

    int result=0;
    if(leftDown){
        if(mx>=bx && mx<=(bx+bw) && invY>=by && invY<=(by+bh))
            result=1; // back
        else if(mx>=qx && mx<=(qx+qw) && invY>=qy && invY<=(qy+qh))
            result=2; // quit
    }

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    return result;
}

// Fly indicator
static void drawFlyIndicator(bool isFlying,int screenW,int screenH)
{
    float w=20.0f,h=20.0f;
    float x=5.0f;
    float y=(float)screenH-h-5.0f;

    float r=1.0f,g=0.0f,b=0.0f;
    if(isFlying){ r=0.1f;g=1.0f;b=0.1f;}

    glDisable(GL_DEPTH_TEST);
    glUseProgram(uiShader);
    drawRect2D(x,y,w,h, r,g,b,1.0f, screenW,screenH);
    glEnable(GL_DEPTH_TEST);
}

// -----------------------------------------------------------------------------
// Simple raycast for block picking
// -----------------------------------------------------------------------------
static bool raycastBlock(const Vec3 &start,const Vec3 &dir,float maxDist,
                         int &outX,int &outY,int &outZ)
{
    float step=0.1f;
    float traveled=0.0f;

    while(traveled < maxDist){
        Vec3 pos= add(start, multiply(dir, traveled));
        int bx=(int)std::floor(pos.x);
        int by=(int)std::floor(pos.y);
        int bz=(int)std::floor(pos.z);

        if(isSolidBlock(bx,by,bz)){
            outX= bx; 
            outY= by; 
            outZ= bz;
            return true;
        }
        traveled+= step;
    }
    return false;
}

// -----------------------------------------------------------------------------
// 3D world shaders
// -----------------------------------------------------------------------------
static const char* worldVertSrc= R"(
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

static const char* worldFragSrc= R"(
#version 330 core
in vec2 TexCoord;
out vec4 FragColor;
uniform sampler2D ourTexture;
void main(){
    FragColor= texture(ourTexture, TexCoord);
}
)";

// -----------------------------------------------------------------------------
// MAIN
// -----------------------------------------------------------------------------
int main(int /*argc*/, char* /*argv*/[])
{
    // attempt load
    float loadedX=0.0f, loadedY=30.0f, loadedZ=0.0f;
    int loadedSeed=0;
    bool loadedOk= loadWorld("saved_world.txt", loadedSeed, loadedX, loadedY, loadedZ);
    if(loadedOk){
        setNoiseSeed(loadedSeed);
        std::cout<<"[World] Loaded seed="<<loadedSeed
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
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    SDL_Window* window= SDL_CreateWindow("Voxel Engine",
        SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,
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
    if(glewErr != GLEW_OK){
        std::cerr<<"GLEW Error: "<< glewGetErrorString(glewErr)<<std::endl;
        SDL_GL_DeleteContext(glContext);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }
    SDL_GL_SetSwapInterval(1);

    glEnable(GL_DEPTH_TEST);

    // Create the 3D pipeline
    worldShader= createShaderProgram(worldVertSrc, worldFragSrc);
    texID= loadTexture("texture.png");
    if(!texID){
        std::cerr<<"Texture failed to load!\n";
        SDL_GL_DeleteContext(glContext);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }

    // 2D UI pipeline
    initUI();

    // Initialize inventory
    Inventory inventory;

    // Generate chunk for spawn
    int spawnChunkX= (int)std::floor(loadedX/(float)chunkSize);
    int spawnChunkZ= (int)std::floor(loadedZ/(float)chunkSize);
    auto chunkKey= std::make_pair(spawnChunkX, spawnChunkZ);
    if(chunks.find(chunkKey)== chunks.end()){
        chunks[chunkKey]= generateChunk(spawnChunkX, spawnChunkZ);
    }

    // Camera
    Camera camera;
    camera.position= {loadedX,loadedY,loadedZ};
    camera.yaw  = -3.14f*0.5f;
    camera.pitch= 0.0f;

    bool paused= false;
    bool isFlying= false;
    float verticalVel=0.0f;

    // Lock mouse
    SDL_SetRelativeMouseMode(SDL_TRUE);

    Uint32 lastTime= SDL_GetTicks();
    bool running= true;
    SDL_Event ev;

    while(running){
        Uint32 now= SDL_GetTicks();
        float dt= (now-lastTime)*0.001f;
        lastTime= now;

        while(SDL_PollEvent(&ev)){
            if(ev.type== SDL_QUIT){
                running= false;
            }
            else if(ev.type== SDL_KEYDOWN){
                // ESC => pause
                if(ev.key.keysym.sym== SDLK_ESCAPE){
                    paused= !paused;
                    if(inventory.isOpen()) inventory.toggle();
                    if(paused){
                        SDL_SetRelativeMouseMode(SDL_FALSE);
                    } else {
                        SDL_SetRelativeMouseMode(SDL_TRUE);
                    }
                }
                // F => toggle fly
                else if(ev.key.keysym.sym== SDLK_f){
                    isFlying= !isFlying;
                    if(isFlying){
                        verticalVel=0.0f;
                        std::cout<<"[Fly] ON\n";
                    } else {
                        std::cout<<"[Fly] OFF\n";
                    }
                }
                // E => open/close inventory if not paused
                else if(!paused && ev.key.keysym.sym== SDLK_e){
                    inventory.toggle();
                    if(inventory.isOpen()) SDL_SetRelativeMouseMode(SDL_FALSE);
                    else SDL_SetRelativeMouseMode(SDL_TRUE);
                }
                // SPACE => jump if not paused/inventory/flying
                else if(!paused && !inventory.isOpen() && !isFlying &&
                        ev.key.keysym.sym== SDLK_SPACE)
                {
                    int footX= (int)std::floor(camera.position.x);
                    int footY= (int)std::floor(camera.position.y-0.1f);
                    int footZ= (int)std::floor(camera.position.z);
                    if(isSolidBlock(footX,footY,footZ)){
                        verticalVel= JUMP_SPEED;
                    }
                }
            }
            else if(ev.type== SDL_MOUSEMOTION){
                if(!paused && !inventory.isOpen()){
                    float sens= 0.002f;
                    camera.yaw   += ev.motion.xrel*sens;
                    camera.pitch -= ev.motion.yrel*sens;
                    if(camera.pitch>1.57f)  camera.pitch= 1.57f;
                    if(camera.pitch<-1.57f) camera.pitch=-1.57f;
                }
            }
            // If user clicks => place/destroy blocks
            else if(!paused && !inventory.isOpen() && ev.type== SDL_MOUSEBUTTONDOWN){
                // Raycast out up to 5 units
                Vec3 eyePos= camera.position;
                eyePos.y+= 1.6f;
                Vec3 viewDir= {
                    cos(camera.yaw)*cos(camera.pitch),
                    sin(camera.pitch),
                    sin(camera.yaw)*cos(camera.pitch)
                };
                viewDir= normalize(viewDir);

                int bx,by,bz;
                if(raycastBlock(eyePos, viewDir, 5.0f, bx,by,bz)){
                    // Destroy => left click
                    if(ev.button.button== SDL_BUTTON_LEFT){
                        auto it= extraBlocks.find({bx,by,bz});
                        if(it!= extraBlocks.end()){
                            // It's in extraBlocks => remove it
                            extraBlocks.erase(it);
                        } else {
                            // Base terrain => override with BLOCK_REMOVED
                            extraBlocks[{bx,by,bz}]= BLOCK_REMOVED;
                        }
                        // Rebuild chunk
                        int cx,cz;
                        getChunkCoords(bx,bz,cx,cz);
                        rebuildChunk(cx,cz);
                    }
                    // Place => right click
                    else if(ev.button.button== SDL_BUTTON_RIGHT){
                        // We find the cell in front of the block we hit
                        // We'll do a small step back approach
                        float stepBack= 0.05f;
                        float traveled= 0.0f;
                        float maxT= 5.0f;
                        bool placed=false;
                        while(traveled<maxT){
                            Vec3 pos= add(eyePos, multiply(viewDir, traveled));
                            int cbx=(int)std::floor(pos.x);
                            int cby=(int)std::floor(pos.y);
                            int cbz=(int)std::floor(pos.z);

                            if(cbx==bx && cby==by && cbz==bz){
                                // Step back one iteration
                                float tb= traveled-stepBack;
                                if(tb<0) break;
                                Vec3 placePos= add(eyePos, multiply(viewDir,tb));
                                int pbx=(int)std::floor(placePos.x);
                                int pby=(int)std::floor(placePos.y);
                                int pbz=(int)std::floor(placePos.z);
                                // if that cell is free, place block
                                if(!isSolidBlock(pbx,pby,pbz)){
                                    int bToPlace= inventory.getSelectedBlock();
                                    extraBlocks[{pbx,pby,pbz}]= (BlockType)bToPlace;
                                    int cpx,cpz;
                                    getChunkCoords(pbx,pbz,cpx,cpz);
                                    rebuildChunk(cpx,cpz);
                                }
                                placed= true;
                                break;
                            }
                            traveled+= 0.1f;
                        }
                        // if placed=false => no valid spot found
                    }
                }
            }
        }

        // If paused => draw pause menu
        if(paused){
            glClearColor(0.53f,0.81f,0.92f,1.0f);
            glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
            glUseProgram(worldShader);

            // minimal world draw
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, texID);
            GLint tLoc= glGetUniformLocation(worldShader,"ourTexture");
            glUniform1i(tLoc,0);

            Vec3 eyePos= camera.position;
            eyePos.y+=1.6f;
            Vec3 viewDir={
                cos(camera.yaw)*cos(camera.pitch),
                sin(camera.pitch),
                sin(camera.yaw)*cos(camera.pitch)
            };
            Vec3 camTgt= add(eyePos,viewDir);

            Mat4 view= lookAtMatrix(eyePos, camTgt,{0,1,0});
            Mat4 proj= perspectiveMatrix(45.0f*(3.14159f/180.0f),
                               (float)SCREEN_WIDTH/(float)SCREEN_HEIGHT,
                               0.1f,100.0f);
            Mat4 pv= multiplyMatrix(proj,view);

            int pcx= (int)std::floor(camera.position.x/ (float)chunkSize);
            int pcz= (int)std::floor(camera.position.z/ (float)chunkSize);
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

            int clicked= drawPauseMenu(SCREEN_WIDTH, SCREEN_HEIGHT);
            if(clicked==1){
                paused= false;
                SDL_SetRelativeMouseMode(SDL_TRUE);
            }
            else if(clicked==2){
                // save & exit
                saveWorld("saved_world.txt",loadedSeed,
                          camera.position.x,
                          camera.position.y,
                          camera.position.z);
                running= false;
            }
            SDL_GL_SwapWindow(window);
            continue;
        }

        // If inventory is open => skip normal movement
        if(!inventory.isOpen()){
            // normal game update
            const Uint8* keys= SDL_GetKeyboardState(nullptr);
            float speed= 10.0f* dt;

            Vec3 forward= { cos(camera.yaw),0,sin(camera.yaw)};
            forward= normalize(forward);
            Vec3 right= normalize(cross(forward,{0,1,0}));

            Vec3 horizDelta={0,0,0};
            if(keys[SDL_SCANCODE_W]) horizDelta= add(horizDelta, multiply(forward,speed));
            if(keys[SDL_SCANCODE_S]) horizDelta= subtract(horizDelta,multiply(forward,speed));
            if(keys[SDL_SCANCODE_A]) horizDelta= subtract(horizDelta,multiply(right,speed));
            if(keys[SDL_SCANCODE_D]) horizDelta= add(horizDelta,multiply(right,speed));

            Vec3 newPosH= camera.position;
            newPosH.x+= horizDelta.x;
            newPosH.z+= horizDelta.z;
            if(!checkCollision(newPosH)){
                camera.position.x= newPosH.x;
                camera.position.z= newPosH.z;
            }

            if(isFlying){
                verticalVel=0.0f;
                float flySpd= 10.0f* dt;
                if(keys[SDL_SCANCODE_SPACE]){
                    Vec3 upPos= camera.position;
                    upPos.y+= flySpd;
                    if(!checkCollision(upPos)) camera.position.y= upPos.y;
                }
                if(keys[SDL_SCANCODE_LSHIFT]){
                    Vec3 dPos= camera.position;
                    dPos.y-= flySpd;
                    if(!checkCollision(dPos)) camera.position.y= dPos.y;
                }
            } else {
                // normal gravity
                verticalVel+= GRAVITY* dt;
                float dy= verticalVel* dt;
                Vec3 newPosV= camera.position;
                newPosV.y+= dy;
                if(!checkCollision(newPosV)){
                    camera.position.y= newPosV.y;
                } else {
                    verticalVel=0.0f;
                }
            }

            // kill plane
            if(camera.position.y< WORLD_FLOOR_LIMIT){
                std::cout<<"[World] Fell below kill plane => reset\n";
                camera.position.y= 30.0f;
                verticalVel=0.0f;
            }
        }

        // update inventory every frame
        inventory.update(dt, camera);

        // chunk loading
        int pcx= (int)std::floor(camera.position.x/(float)chunkSize);
        int pcz= (int)std::floor(camera.position.z/(float)chunkSize);
        for(int cx= pcx-renderDistance; cx<= pcx+renderDistance; cx++){
            for(int cz= pcz-renderDistance; cz<= pcz+renderDistance; cz++){
                auto key= std::make_pair(cx,cz);
                if(chunks.find(key)==chunks.end()){
                    chunks[key]= generateChunk(cx,cz);
                }
            }
        }

        // render
        glClearColor(0.53f,0.81f,0.92f,1.0f);
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        glUseProgram(worldShader);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texID);
        GLint uniTex= glGetUniformLocation(worldShader,"ourTexture");
        glUniform1i(uniTex,0);

        Vec3 eyePos= camera.position;
        eyePos.y+=1.6f;
        Vec3 viewDir={
            cos(camera.yaw)*cos(camera.pitch),
            sin(camera.pitch),
            sin(camera.yaw)*cos(camera.pitch)
        };
        Vec3 camTgt= add(eyePos, viewDir);

        Mat4 view= lookAtMatrix(eyePos, camTgt,{0,1,0});
        Mat4 proj= perspectiveMatrix(45.0f*(3.14159f/180.0f),
                      (float)SCREEN_WIDTH/(float)SCREEN_HEIGHT,
                      0.1f,100.0f);
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
            glDrawArrays(GL_TRIANGLES,0,(GLsizei)(ch.vertices.size()/5));
        }

        // Fly indicator
        glUseProgram(uiShader);
        drawFlyIndicator(isFlying, SCREEN_WIDTH, SCREEN_HEIGHT);

        // If inventory open => render it
        inventory.render();

        SDL_GL_SwapWindow(window);
    }

    // On exit => save
    saveWorld("saved_world.txt",loadedSeed,
              camera.position.x,
              camera.position.y,
              camera.position.z);

    glDeleteProgram(worldShader);
    glDeleteProgram(uiShader);
    glDeleteVertexArrays(1,&uiVAO);
    glDeleteBuffers(1,&uiVBO);

    SDL_GL_DeleteContext(glContext);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}

