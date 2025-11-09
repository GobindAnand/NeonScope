# NeonScope

NeonScope is a JUCE-based VST3 effect that leaves audio untouched while exposing a neon-themed visual scope. Two global controls set the input sensitivity and the visual decay, while 16 animated bands plus independent left/right meters give a quick read on program energy.

## Building

1. Install JUCE 7 and make it available as a subdirectory (e.g. `git submodule add https://github.com/juce-framework/JUCE.git JUCE`).
2. Configure with CMake using the included `CMakeLists.txt`:
   ```bash
   cmake -B build -S . -DJUCE_BUILD_EXAMPLES=OFF
   cmake --build build
   ```
3. The resulting VST3 bundle appears in `build/NeonScope_artefacts/VST3/NeonScope.vst3`.

## Loading in FL Studio

1. Copy `NeonScope.vst3` into a folder FL Studio scans for VST3 plug-ins (e.g. `%ProgramFiles%/Common Files/VST3` on Windows or `~/Library/Audio/Plug-Ins/VST3` on macOS).
2. In FL Studio, open *Options → Manage plugins*, add the folder if needed, and press *Find plugins*.
3. NeonScope shows up under the *Effects* category; load it on any insert slot to visualize that track while passing audio through unchanged.
