# type/c++

A minimalist, high-performance typing trainer written in C++ with a web-based UI.

## Features
- **Modes:** Time, Word count, and Zen mode.
- **Banks:** C++, English 200, Oxford 3k, and Custom word bank support.
- **Theming:** Terminal, Dracula, Nord, Monokai, and Amoled.
- **Stats:** Local session history and WPM tracking via Chart.js.
- **Persistence:** Stats and custom banks are saved locally in app data.

## Prerequisites
- C++17 (or higher) compatible compiler (GCC, Clang, or MSVC).
- [cpp-httplib](https://github.com/yhirose/cpp-httplib) (`httplib.h`) in your include path.
- `template.hpp` (referenced in your source).

## Build

### Linux / macOS
```bash
g++ -O3 main.cpp -o typecpp -lpthread -lstdc++fs
./typecpp
```

### Windows (MinGW/MSVC)
```bash
g++ -O3 main.cpp -o typecpp.exe -lws2_32
./typecpp.exe
```
*Note: Ensure `ws2_32.lib` is linked on Windows for networking.*

## Usage
1. Run the executable.
2. The app will automatically open `http://localhost:8080` in your default browser.
3. **Shortcuts:**
   - `Tab`: Restart test
   - `Esc`: Pause/End test
   - `Ctrl + Backspace`: Delete word
   - `Shift + F`: Toggle Zen mode

## Data Storage
- **Windows:** `%LOCALAPPDATA%/type_cpp`
- **macOS:** `~/Library/Application Support/type_cpp`
- **Linux:** `~/.local/share/type_cpp` or `$XDG_DATA_HOME`
