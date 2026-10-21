# Reverse Reverb (JUCE, VST3 / AU / Standalone)

Reverse reverb that swells *into* the sound, with a capture → preview → drag-to-DAW workflow.

## Build (no compiler setup): GitHub Actions
1. Create a GitHub repo, push this folder.
2. Open the **Actions** tab → *Build plugin* → when green, download the artifact
   (`ReverseReverb-Windows-VST3` or `ReverseReverb-macOS-VST3-AU`).
3. Windows: copy `Reverse Reverb.vst3` (a folder) to `C:\Program Files\Common Files\VST3\`.
   macOS: copy to `~/Library/Audio/Plug-Ins/VST3/` (and `Components/` for the AU),
   then run `xattr -cr "<plugin path>"` because the build is unsigned.
4. Rescan plugins in your DAW.

Local build: `cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build --config Release`
(needs CMake 3.22+, a C++17 compiler; JUCE 8.0.8 downloads automatically).
Before shipping, set your company name / codes in `CMakeLists.txt`.

## How it works
* **Engine** = `reverse( reverb( reverse(x) ) )`: an 8-line feedback delay network with exact RT60,
  run on the reversed sound, then reversed again, so the tail builds up and lands on the hit.
  The whole input is never simply reversed.
* **Live swells** (LIVE SWELLS on): onsets are detected, each gets its swell rendered on a background
  thread and mixed in so it peaks on the hit. A swell can't exist before the sound happens, so the dry
  signal is delayed by a fixed latency (3.1 s, reported to the host for delay compensation; the DAW
  aligns it on playback, but live monitoring will lag). Switch LIVE SWELLS off for zero latency.
  Tone/Width/Size/Depth/Length/Pre-delay/Mode apply to each new swell; Dry/Wet and Output are instant.
* **Preview**: CAPTURE listens, starts on the first sound, records for the set length (seconds, or
  1/2/4/8 bars locked to the host tempo), then renders automatically. REGENERATE re-renders after you
  change settings (the status line tells you when the preview is out of date). PLAY auditions it.
* **Export**: the render is a 24-bit stereo WAV with the actual processed sound, peak-limited to
  -0.2 dBFS so it never clips. DRAG TO DAW hands that file to the host; EXPORT WAV saves a copy.
* **Pre-delay** = gap between the end of the swell and the sound.

## Controls
Drag a knob; double-click the knob or its number to type a value; Alt + double-click resets.
All parameters are automatable. Presets: 9 factory + SAVE/LOAD of your own (`.rrpreset`).

## Honest status
* The DSP (reverse swell shape, level, latency alignment) is verified by `Tests/EngineTest.cpp`
  (`-DRR_BUILD_TESTS=ON`), and the Linux VST3 passes pluginval at strictness 5.
* Windows and macOS builds come from CI and haven't been run in a DAW by me.
* **Drag-and-drop to a DAW is not tested in any host.** It uses JUCE's native file drag, which works
  in many hosts but not all (some refuse drops from plugin windows). If it's refused, use EXPORT WAV.
* Captured audio isn't saved in the project; settings are.
* Capture at one sample rate is discarded if the DAW's rate changes.
