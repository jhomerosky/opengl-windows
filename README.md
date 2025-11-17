# About
The purpose of this project is to teach myself computer graphics with OpenGL using as few external libraries as possible. I am interested in writing from scratch and, although I am writing this in C++, I am intentionally staying close to C style code.

# Building
I am developing the project on Windows using VSCode so I have included my tasks.json file for building the project using g++. The project uses GLFW for window management and input handling, and GLAD for loading OpenGL functions.

# C++ GLFW Project Setup for Visual Studio Code in tasks.json
```json
{
	"version": "2.0.0",
	"tasks": [
        {
            "type": "cppbuild",
            "label": "C/C++: g++.exe build active file",
            "command": "C:/msys64/mingw64/bin/g++.exe",
            "args": [
                "-g",
                "-std=c++17",
                "-fopenmp",
                "-I${workspaceFolder}/include",
                "-L${workspaceFolder}/lib",
                "${workspaceFolder}/src/main.cpp",
                "${workspaceFolder}/src/glad.c",
                "-lglfw3dll",
                "-o",
                "${workspaceFolder}/run.exe"
            ],
            "options": {
                "cwd": "${workspaceFolder}"
            },
            "problemMatcher": [
                "$gcc"
            ],
            "group": {
                "kind": "build",
                "isDefault": true
            },
            "detail": "compiler: C:/msys64/mingw64/bin/g++.exe"
        }
    ]
}
```