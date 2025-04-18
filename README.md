# Hex2File
**Hex2File** is a powerful utility for converting hexadecimal data into actual files, designed with commands!

With supports for handling large byte streams and leverages multi-threading and multi-core processing to ensure fast and efficient conversions.

*Crafted with ♥ during a ChatGPT experiment.*
## Usage:
```Hex2File --inputHex=<path> --outputFile=<path>```

You must provide both `--inputHex` and `--outputFile` arguments with valid paths to perform a conversion.  
If you wish to use it with `--debug`, it will print out technical details during the conversion process.

Use `--help` to display all available commands and options.

#
You can explore example hex files to practice with here: [Example](https://github.com/svh03ra/Hex2File/tree/main/example)
## Building Instructions:
You need to install [MSYS2](https://www.msys2.org/) to continuously next steps.

Once installed, open the **MINGW64** terminal and update all packages before continuing with the installation:
```pacman -Suy && pacman -S mingw-w64-x86_64-gcc```

After updating, download the C++ source file from this repository then compile the file using the following command:

```g++ -std=c++17 -O2 -o Hex2File.exe Hex2File.cpp -lstdc++fs```

**Required to run:** The following DLLs must be placed alongside the executable, You can find them in the `MINGW64` directory of your MSYS2 installation:
- libgcc_s_seh-1.dll
- libstdc++-6.dll
- libwinpthread-1.dll

Or, if you wish to create a fully self-contained executable also you can compile it using the `-static` flag.
# License:
This repository is licensed under the GNU General Public License. You can view the full license here: [GNU License](https://github.com/svh03ra/Hex2File/blob/main/LICENSE)





