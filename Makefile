# ==============================
# Project: C-Craft
# ==============================

APP_NAME := voxel

SRC := \
	main.cpp \
	shader.cpp \
	texture.cpp \
	math.cpp \
	noise.cpp \
	cube.cpp \
	world.cpp \
	inventory.cpp

# ==============================
# Common flags
# ==============================
CXXFLAGS_COMMON := -std=c++11 -O2 -Wall

# ==============================
# Linux build (native)
# ==============================
LINUX_CXX      := g++
LINUX_OBJDIR   := build/linux
LINUX_OBJ      := $(addprefix $(LINUX_OBJDIR)/,$(SRC:.cpp=.o))
LINUX_CXXFLAGS := $(CXXFLAGS_COMMON)
LINUX_LDFLAGS  := -lSDL2 -lGLEW -lGL -pthread

# ==============================
# Windows build (MinGW cross)
# ==============================
WIN_CXX      := x86_64-w64-mingw32-g++
WIN_EXE      := $(APP_NAME).exe
WIN_OBJDIR   := build/windows
WIN_OBJ      := $(addprefix $(WIN_OBJDIR)/,$(SRC:.cpp=.o))

# MinGW sysroot on Manjaro/Arch (where mingw-w64-sdl2 and mingw-w64-glew install)
WIN_PREFIX ?= /usr/x86_64-w64-mingw32

WIN_INCLUDES := \
	-I$(WIN_PREFIX)/include \
	-I$(WIN_PREFIX)/include/SDL2

WIN_LIBDIRS := \
	-L$(WIN_PREFIX)/lib

WIN_CXXFLAGS := $(CXXFLAGS_COMMON) $(WIN_INCLUDES)
WIN_LDFLAGS  := $(WIN_LIBDIRS) \
	-lmingw32 \
	-lSDL2main \
	-lSDL2 \
	-lglew32 \
	-lopengl32 \
	-lgdi32 \
	-lwinmm

# ==============================
# Targets
# ==============================
.PHONY: all linux windows clean help

all: help

help:
	@echo "Use:"
	@echo "  make linux    - Build for Linux"
	@echo "  make windows  - Build for Windows (MinGW on Manjaro)"
	@echo "  make clean"
	@echo ""
	@echo "Manjaro deps (AUR): mingw-w64-sdl2 mingw-w64-glew"

# ------------------------------
# Linux target
# ------------------------------
linux: $(APP_NAME)

$(APP_NAME): $(LINUX_OBJDIR) $(LINUX_OBJ)
	$(LINUX_CXX) $(LINUX_OBJ) -o $@ $(LINUX_LDFLAGS)
	@echo "Linux build complete: ./$(APP_NAME)"

$(LINUX_OBJDIR):
	mkdir -p $(LINUX_OBJDIR)

$(LINUX_OBJDIR)/%.o: %.cpp | $(LINUX_OBJDIR)
	$(LINUX_CXX) $(LINUX_CXXFLAGS) -c $< -o $@

# ------------------------------
# Windows target
# ------------------------------
windows: $(WIN_EXE)

$(WIN_EXE): $(WIN_OBJDIR) $(WIN_OBJ)
	$(WIN_CXX) $(WIN_OBJ) -o $@ $(WIN_CXXFLAGS) $(WIN_LDFLAGS)
	@echo "Windows build complete: $(WIN_EXE)"
	@echo "Copy assets next to the exe: texture.png hand.png BG.png"
	@echo "If you link dynamically, also copy SDL2.dll and glew32.dll."

$(WIN_OBJDIR):
	mkdir -p $(WIN_OBJDIR)

$(WIN_OBJDIR)/%.o: %.cpp | $(WIN_OBJDIR)
	$(WIN_CXX) $(WIN_CXXFLAGS) -c $< -o $@

# ------------------------------
# Clean
# ------------------------------
clean:
	rm -rf build
	rm -f $(APP_NAME) $(WIN_EXE)

