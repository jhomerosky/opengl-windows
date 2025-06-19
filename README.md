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