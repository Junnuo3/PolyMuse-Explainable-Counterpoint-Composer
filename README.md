# PolyMuse

A counterpoint composition tool that helps you learn and practice writing counterpoint. Play notes on your MIDI keyboard and get real-time feedback or generate counterpoint automatically.

## What is Counterpoint?

Counterpoint is the art of combining two or more independent melodic lines. This app helps you learn traditional counterpoint rules by either checking your own compositions or automatically generating valid counterpoint.

## Features

- **Tutor Mode**: Play two notes and see if they follow counterpoint rules
- **Generator Mode**: Automatically generates valid counterpoint for your input
- **Rule Checking**: Detects parallel fifths, octaves, dissonances, and more

## Building

1. **Clone the repository:**
   ```bash
   git clone <repository-url>
   cd Counterpoints
   ```

2. **Create build directory and configure:**
   ```bash
   mkdir build && cd build
   cmake ..
   ```

3. **Build:**
   ```bash
   cmake --build .
   ```

4. **Run:**
   - **macOS**: Open `build/Counterpoints_artefacts/Counterpoints.app`
   - **Windows**: Run `build/Counterpoints_artefacts/Release/Counterpoints.exe`
   - **Linux**: Run `build/Counterpoints_artefacts/Counterpoints`

### Platform-specific Build Options

**macOS (Xcode):**
```bash
cmake .. -G Xcode
open Counterpoints.xcodeproj
```

**Windows (Visual Studio):**
```bash
cmake .. -G "Visual Studio 16 2019"
cmake --build . --config Release
```

**Linux:**
```bash
cmake .. -G "Unix Makefiles"
make -j$(nproc)
```

## Usage

1. Launch the application
2. Connect a MIDI keyboard (optional - you can also use the on-screen piano roll)
3. Select your MIDI input device from the dropdown
4. Choose a mode:
   - **Tutor Mode**: Play two notes to see if they follow counterpoint rules
   - **Generator Mode**: Play a note and the app generates valid counterpoint automatically
5. In Generator Mode, use "Generate Above" or "Generate Below" to control direction
6. Click "Reset Phrase" to clear the current phrase and start over

## Project Structure

```
Source/
├── MainComponent      # Main UI and MIDI handling
├── CounterpointEngine  # Generates counterpoint notes
├── RuleChecker        # Validates counterpoint rules
├── PianoRoll          # Visual note editor
└── MidiManager        # MIDI input/output
```

## Acknowledgments

Built with the [JUCE framework](https://juce.com/).
