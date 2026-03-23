# About
The purpose of this project is to teach myself computer graphics with OpenGL using as few external libraries as possible. I am interested in writing from scratch and, although I am writing this in C++, I am intentionally staying close to C style code.

The project uses GLFW for window management and input handling, and GLAD for loading OpenGL functions. I use stb_image.h for image loading for now.

# Build

**Windows:**
```cmd
make.bat        # Build the project
make.bat clean  # Remove build artifacts
```

**Linux or WSL:**
```bash
make        # Build the project
make clean  # Remove build artifacts
```

The build output is `run.exe` in the project root.