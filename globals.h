#ifndef GLOBALS_H
#define GLOBALS_H

#include <GL/glew.h>
#include <unordered_map>
#include <utility>
#include <tuple>
#include <functional>

// Custom hash for std::pair<int, int>
struct PairHash {
    std::size_t operator()(const std::pair<int,int>& p) const {
        return std::hash<int>()(p.first) ^ (std::hash<int>()(p.second) << 1);
    }
};

// Custom hash for std::tuple<int, int, int>
struct TupleHash {
    std::size_t operator()(const std::tuple<int,int,int>& t) const {
        std::size_t h1 = std::hash<int>()(std::get<0>(t));
        std::size_t h2 = std::hash<int>()(std::get<1>(t));
        std::size_t h3 = std::hash<int>()(std::get<2>(t));
        return h1 ^ (h2 << 1) ^ (h3 << 2);
    }
};

extern GLuint worldShader;
extern GLuint texID;
extern int SCREEN_WIDTH;
extern int SCREEN_HEIGHT;

#endif

