### Firmware Compilation
- Download and unzip (or git clone) this (`OpenGarage-Firmware`) repository.
- Install the latest LTS version of Node.js (https://nodejs.org/) if you don't have it already. Then go to the `OpenGarage-Firmware/OpenGarage` folder, and run `npm install html-minifier-terser`.
- Install Visual Studio Code (VS Code) (https://code.visualstudio.com/).
- Launch VS Code and install the **PlatformIO** extension.
- Click `File -> Open Folder` and select the `OpenGarage` folder. A `platformio.ini` file is provided in the folder, which includes all libraries and settings needed to compile the firmware.
- Click `PlatformIO: Build` (with the checkmark icon ✓) at the bottom of the screen to build the firmware.

### Making Changes to the Firmware and the UI
- You can modify any firmware source files directly.
- The built-in web UI lives in the `html` subfolder (original, human-readable HTML files).
- You do not need to run any scripts manually. When you click `PlatformIO: Build`, the `run_prebuild.py` automatically runs the Node.js script `compress_htmls.mjs`. This tool minifies, compresses, and converts your HTML files into firmware program strings (in `htmls.j`), which are then compiled into the final firmware.
- After editing files under `html`, just build again (PlatformIO will re-run the prebuild step).
