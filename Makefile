# Linux/WSL: native g++ compilation, not fully tested yet for Linux

CC := g++
CXXFLAGS := -g -std=c++17 -fopenmp
INCLUDES := -I./include
LIBS := -lglfw -lGL -ldl -lpthread -lm -lX11
SOURCES := ./src/main.cpp ./src/glad.c
OUTPUT := ./run

.PHONY: all clean

all: $(OUTPUT)

$(OUTPUT): $(SOURCES)
	$(CC) $(CXXFLAGS) $(INCLUDES) $(SOURCES) $(LIBS) -o $(OUTPUT)

clean:
	rm -f $(OUTPUT)

