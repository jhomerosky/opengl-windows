# About
The purpose of this project is to teach myself computer graphics with OpenGL from first principles using as few external libraries as possible. This project is mostly written in C but compiled as C++ so that I can leverage a few C++ features. I am not using any of the C++ STL. This project is not a demonstration of "clean code" programming principles like OOP or RAII. I am interested in both exploring the performance benefits gained by not strictly following them and also discovering why they exist in the first place.

Here are a list of the external dependencies:
* GLAD - Load OpenGL functions
* GLFW - Create window and handle input
* khrplatform.h - Required by OpenGL/GLAD
* stb_image.h - Load a variety of image formats


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