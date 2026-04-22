# Equalizer Plug-In

## [RELEASE 1 AVAILABLE](../../releases/latest)

## Project Overview
This repository contains a basic audio equalizer plugin developed in C++ using the JUCE framework. The plugin is currently under active development with continuous updates focused on adding modules and building a more complex and consistent system.

## Getting Started

### Prerequisites
- JUCE framework installed
- C++17 or later compiler
- CMake (version 3.15 or higher)
- Platform-specific requirements:
  - **Windows**: Visual Studio 2019 or later
  - **macOS**: Xcode 12 or later
  - **Linux**: GCC or Clang

### Accessing the Code
Clone this repository:
```
git clone https://github.com/aitorpitarchfontcuberta/Equalizer_Plug-In.git
cd Equalizer_Plug-In
```

## Building the Plugin

### Option 1: Using Projucer (Recommended)
1. Open Projucer
2. Load the `EQ.jucer` file
3. Select your target platform in the exporters
4. Click "Create" to generate project files
5. Open the generated project in your IDE and build

### Option 2: Command Line Build

**Windows (Visual Studio):**
```
cd Builds/VisualStudio2026
cmake --build . --config Release
```

**macOS:**
```
cd Builds/MacOSX
cmake --build . --config Release
```

**Linux:**
```
mkdir build
cd build
cmake ..
make
```

### Build Outputs
- **EQ_StandalonePlugin**: Located in `Builds/[Platform]/bin/Release/`
- **EQ_VST3**: Located in standard VST3 plugin directories:
  - Windows: `C:\Program Files\Common Files\VST3\`
  - macOS: `~/Library/Audio/Plug-Ins/VST3/`
  - Linux: `~/.vst3/`

## Usage

### Standalone Application
Run the standalone executable directly. It includes a built-in audio interface for testing and processing audio files.

### VST3 Plugin
1. Copy the VST3 plugin to the appropriate directory for your operating system (see Build Outputs above)
2. Rescan plugins in your DAW
3. Load the "EQ" plugin in an audio track
4. Adjust the equalizer controls in real-time

## Documentation
For detailed information about updates, features, and development progress, refer to the `Documentation_ENG.pdf` or `Documentation_ESP.pdf` files in this repository. This document is continuously updated with the latest changes and improvements.

## Project Structure
```
Equalizer_Plug-In/
├── Source/
│   ├── PluginProcessor.cpp
│   ├── PluginProcessor.h
│   ├── PluginEditor.cpp
│   └── PluginEditor.h
├── Builds/
│   ├── VisualStudio2026/
│   ├── MacOSX/
│   └── Linux/
├── EQ.jucer
├── provaEQ.filtergraph
└── README.md
```

## Troubleshooting

**Plugin not found in DAW:**
- Ensure the plugin is built in Release mode
- Check that the plugin is in the correct VST3 directory
- Rescan plugins in your DAW settings

**Build errors:**
- Verify JUCE modules are correctly linked in EQ.jucer
- Check that your compiler version meets the minimum requirements
- Ensure all dependencies are installed

## Author / Contact

Project author: Aitor Pitarch

[GitHub](https://github.com/aitorpitarchfontcuberta)    [Linkedin](https://linkedin.com/in/aitor-pitarch-fontcuberta-a9970a367)

For questions about the project, simulation details, or to request raw data/scripts, open an issue in this repository or contact me via LinkedIn.
