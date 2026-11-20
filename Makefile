CXX ?= g++
CPPFLAGS ?=
CXXFLAGS ?= -std=c++11 -O2 -Wall -Wextra -pedantic
TARGET = astana3d
ifeq ($(OS),Windows_NT)
    TARGET := $(TARGET).exe
    LDLIBS = -lfreeglut -lglew32 -lopengl32
else
    LDLIBS = -lglut -lGLEW -lGL
endif

SOURCES = src/main.cpp src/renderer.cpp src/views.cpp src/scene.cpp
OBJECTS = $(SOURCES:.cpp=.o)

.PHONY: all clean
all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@

src/%.o: src/%.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -c $< -o $@

-include $(OBJECTS:.o=.d)

clean:
	$(RM) $(OBJECTS) $(OBJECTS:.o=.d) $(TARGET)
