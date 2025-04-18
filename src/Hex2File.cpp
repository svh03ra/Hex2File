/*
This repository is licensed under the GNU General Public License.
Free to use, modify, or create your own fork, provided that you agree to and comply with the terms of the license.

(C) 2025 - svh03ra, all rights reserved.
*/

#include <iostream>
#include <fstream>
#include <string>
#include <iomanip>
#include <sstream>
#include <filesystem>
#include <map>
#include <chrono>
#include <thread>
#include <atomic>
#include <mutex>
#include <conio.h>
#include <algorithm>
#include <vector>
#include <future>
#include <filesystem>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <sys/statvfs.h>
#endif

const size_t CHUNK_SIZE_HEX = 2 * 1024 * 1024; // 2MB Chunk
const size_t PROGRESS_BAR_WIDTH = 50;
bool debug = false;

void debugPrint(const std::string& info) {
    if (debug) {
        std::cout << "[DEBUG]: " << info << std::endl;
    }
}

void printUsage() {
    std::cout << "Usage: Hex2File.exe --inputHex=<path> --outputFile=<path>\n";
    std::cout << "Converts a hex file to a raw byte file.\n\n";
    std::cout << "Options:\n";
    std::cout << "  --inputHex / -i=<path>     Path to the input hex (.hex or .bin) file.\n";
    std::cout << "  --outputFile / -o=<path>   Path to the output file where raw data will be written.\n";
    std::cout << "  --help / -h                Show this help message.\n";
    std::cout << "  --debug / -d               Enable debug mode for detailed output.\n";
}

void printVersion() {
	std::cout << "Hex2File - Version: 1.0 (Compiled Build: 1:00 AM, 19-Apr-25)\n";
	std::cout << "By svh03ra, using experiment with ChatGPT!\n\n";
	std::cout << "Open-Source available on Github: https://github.com/svh03ra/Hex2File\n";
}

void printIntroMessage() {
    std::cout << "Hex2File: Easily convert hex files into actual binary files.\n";
    std::cout << "Useful for creating installers, asset files, and handling large byte sequences.\n\n";
    std::cout << "Supports a variety of formats like .png, .mp3, .mp4, and more.\n\n";
    std::cout << "Take me a bytes if you can!\n";
    std::cout << "Type --help to see all commands and get started.\n";
}

std::string formatSize(size_t bytes) {
    std::ostringstream oss;
    if (bytes >= (1ull << 30))
        oss << std::fixed << std::setprecision(2) << (bytes / (double)(1ull << 30)) << " GB";
    else if (bytes >= (1 << 20))
        oss << std::fixed << std::setprecision(2) << (bytes / (double)(1 << 20)) << " MB";
    else if (bytes >= (1 << 10))
        oss << std::fixed << std::setprecision(2) << (bytes / (double)(1 << 10)) << " KB";
    else
        oss << bytes << " B";
    return oss.str();
}

void printProgress(size_t current, size_t total) {
    double progress = (double)current / total;
    int pos = static_cast<int>(PROGRESS_BAR_WIDTH * progress);

    std::ostringstream oss;
    oss << "Calculating Bytes: " << formatSize(current) << " / " << formatSize(total) << ": [";

    for (int i = 0; i < PROGRESS_BAR_WIDTH; ++i) {
        if (i < pos) oss << "=";
        else if (i == pos) oss << ">";
        else oss << " ";
    }
    oss << "] " << std::fixed << std::setprecision(1) << (progress * 100.0) << "%";

    std::cout << "\r" << std::flush;
    std::cout << oss.str() << std::flush;
}

std::string getAbsolutePath(const std::string& path) {
    std::filesystem::path fsPath(path);
    return fsPath.is_absolute() ? path : std::filesystem::absolute(fsPath).string();
}

bool isHexChar(char c) {
    return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
}

// Function to handle the Tab + ESC key combination to exit
bool checkTabEscExit() {
    // Fix: clear buffer until no more input to avoid stale keys
    while (_kbhit()) {
        int key = _getch();  // Get the first key
        if (key == '\t') {   // Check for Tab
            // Wait for ESC within a short interval (up to 200ms)
            auto start = std::chrono::steady_clock::now();
            while (true) {
                if (_kbhit()) {
                    int secondKey = _getch();
                    if (secondKey == 27) {  // ESC key (ASCII 27)
                        return true;  // Return true to exit
                    } else {
                        break;
                    }
                }
                auto now = std::chrono::steady_clock::now();
                if (std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count() > 200) break;
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
    }
    return false;
}

// Convert a hex chunk to a byte vector using multi-core/threading
void hexStringToBytesParallel(const std::string& hex, std::vector<char>& out, unsigned int numThreads) {
    size_t hexLen = hex.size();
    size_t numBytes = hexLen / 2;
    out.resize(numBytes);

    std::vector<std::thread> threads;
    auto convert = [&](size_t start, size_t end) {
        for (size_t i = start; i < end; ++i) {
            std::string byteString = hex.substr(i * 2, 2);
            out[i] = static_cast<char>(std::stoi(byteString, nullptr, 16));
        }
    };
    size_t chunkPerThread = numBytes / numThreads;
    size_t leftover = numBytes % numThreads;

    size_t begin = 0;
    for (unsigned int t = 0; t < numThreads; ++t) {
        size_t end = begin + chunkPerThread + ((t < leftover) ? 1 : 0);
        threads.emplace_back(convert, begin, end);
        begin = end;
    }
    for (auto& th : threads) th.join();
}

bool processFile(const std::string& inputPath, const std::string& outputPath) {
    debugPrint("Checking if input file exists: " + inputPath);
    if (!std::filesystem::exists(inputPath)) {
        std::cerr << "[ERROR]: Hex file not found! (" << inputPath << ")" << std::endl;
        return false;
    }

    std::ifstream input(inputPath);
    if (!input.is_open()) {
        std::cerr << "[ERROR]: Unable to open input hex file!" << std::endl;
        return false;
    }
    debugPrint("Input hex file opened: " + inputPath);

    std::ofstream output(outputPath, std::ios::binary);
    if (!output.is_open()) {
        std::cerr << "[ERROR]: Unable to open output file!" << std::endl;
        return false;
    }
    debugPrint("Output file opened: " + outputPath);
    
    // Debug storage info
    try {
        std::filesystem::space_info space = std::filesystem::space(outputPath);

        double available = static_cast<double>(space.available);
        double free = static_cast<double>(space.free);
        double capacity = static_cast<double>(space.capacity);

        std::ostringstream stream;
        stream << std::fixed << std::setprecision(2);

        // Print available, free, and total capacity disk space
        if (available >= 1024.0 * 1024.0 * 1024.0) {
            stream << "Disk space available: " 
                << (available / (1024.0 * 1024.0 * 1024.0)) << " GB\n"
                << "[DEBUG]: Disk space free: " 
                << (free / (1024.0 * 1024.0 * 1024.0)) << " GB\n"
                << "[DEBUG]: Disk space capacity: " 
                << (capacity / (1024.0 * 1024.0 * 1024.0)) << " GB";
        } else {
            stream << "Disk space available: " 
                << (available / (1024.0 * 1024.0)) << " MB\n"
                << "[DEBUG]: Disk space free: " 
                << (free / (1024.0 * 1024.0)) << " MB\n"
                << "[DEBUG]: Disk space capacity: " 
                << (capacity / (1024.0 * 1024.0 * 1024.0)) << " GB";
        }
        debugPrint(stream.str());
    } catch (const std::exception& e) {
        debugPrint(std::string("Failed to get disk space info: ") + e.what());
    }

    std::cout << "Starting to Convert file..." << std::endl;
    std::cout << "Press Tab + ESC to exit when you're tired enough.\n" << std::endl;

    // Display Total Number of CPU Cores
    unsigned int numCores = std::thread::hardware_concurrency();
    if (numCores < 1) numCores = 2;
    std::cout << "Total Number of CPU Cores: " << numCores << std::endl;

    std::string line, hexBuffer;
    auto startTime = std::chrono::steady_clock::now();
    size_t writtenBytes = 0;
    long lastPrintedElapsed = -1; // NEW
    double lastBytesPerSec = 0.0; // NEW: for showing transfer rate

    while (std::getline(input, line)) {
        //debugPrint("Reading line: " + line);
        line.erase(std::remove_if(line.begin(), line.end(), ::isspace), line.end());
        //debugPrint("Line after removing whitespace: " + line);

        for (char c : line) {
            if (!isHexChar(c)) {
                std::cerr << "[ERROR]: Not a valid hex file!" << std::endl;
                debugPrint("Invalid hex character detected: " + std::string(1, c));
                return false;
            }
        }

        hexBuffer += line;
        debugPrint("Hex buffer size after append: " + std::to_string(hexBuffer.size()));

        // Multi-core optimized chunk processing
        while (hexBuffer.size() >= CHUNK_SIZE_HEX) {
            std::string chunkHex = hexBuffer.substr(0, CHUNK_SIZE_HEX);
            hexBuffer.erase(0, CHUNK_SIZE_HEX);

            std::vector<char> chunkBytes;
            hexStringToBytesParallel(chunkHex, chunkBytes, numCores);
            output.write(chunkBytes.data(), chunkBytes.size());
            writtenBytes += chunkBytes.size();

            if (!output) {
                std::cerr << "[ERROR]: Unable to write data in your disk. It may be full or corrupted.\n" << std::endl;
                return false;
            }
            // Check if Tab + ESC is pressed to exit
            if (checkTabEscExit()) {
                std::cout << "\nExiting conversion as requested by user (Tab + ESC).\n" << std::endl;
                return false;
            }

            // LIVE TIMER DISPLAY - NEW
            auto currentTime = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(currentTime - startTime).count();
            if (elapsed != lastPrintedElapsed) {
                lastPrintedElapsed = elapsed;

                size_t totalEstimatedBytes = writtenBytes + hexBuffer.size() / 2;
                double bytesPerSec = (elapsed > 0) ? static_cast<double>(writtenBytes) / elapsed : 0.0;
                size_t estRemainSecs = (bytesPerSec > 0) ? static_cast<size_t>((totalEstimatedBytes - writtenBytes) / bytesPerSec) : 0;

                int hrs = static_cast<int>(elapsed / 3600);
                int mins = static_cast<int>((elapsed % 3600) / 60);
                int secs = static_cast<int>(elapsed % 60);

                std::ostringstream timerInfo;
                timerInfo << "Conversion Progress Status: [Elapsed: "
                        << std::setw(2) << std::setfill('0') << hrs << ":"
                        << std::setw(2) << std::setfill('0') << mins << ":"
                        << std::setw(2) << std::setfill('0') << secs << " | ";

                // Calculate remaining time if any
                if (estRemainSecs > 0) {
                    int rhrs = static_cast<int>(estRemainSecs / 3600);
                    int rmins = static_cast<int>((estRemainSecs % 3600) / 60);
                    int rsecs = static_cast<int>(estRemainSecs % 60);

                    timerInfo << "Remaining: " << rhrs << "h " << rmins << "m " << rsecs << "s";
                } else {
                    timerInfo << "Remaining: 00h 00m 00s";
                }

                // Show speed
                timerInfo << " | Speed Rate: ";
                if (bytesPerSec >= 1024.0 * 1024.0 * 1024.0) {
                    timerInfo << std::fixed << std::setprecision(2)
                            << (bytesPerSec / (1024.0 * 1024.0 * 1024.0)) << " GB/s]";
                } else if (bytesPerSec >= 1024.0 * 1024.0) {
                    timerInfo << std::fixed << std::setprecision(2)
                            << (bytesPerSec / (1024.0 * 1024.0)) << " MB/s]";
                } else if (bytesPerSec >= 1024.0) {
                    timerInfo << std::fixed << std::setprecision(2)
                            << (bytesPerSec / 1024.0) << " KB/s]";
                } else {
                    timerInfo << std::fixed << std::setprecision(2)
                            << bytesPerSec << " B/s";
                }

                // Ensure output fits fixed width (if you want the text output to stay within the terminal width)
                std::string outStr = timerInfo.str();
                outStr.resize(120, ' '); // Adjust width as needed (120 chars here for example)
    
                // Print the progress in the same line (overwrites previous)
                std::cout << "\r" << outStr << std::flush;
            }
            // debugPrint still runs at 512 KB
            if (writtenBytes % (512 * 1024) == 0) {
                printProgress(writtenBytes, writtenBytes + hexBuffer.size() / 2);
				debugPrint("\nWritten bytes so far: " + std::to_string(writtenBytes));                
            }
        }
    }

    // Final chunk (if any)
    if (!hexBuffer.empty()) {
        if (hexBuffer.size() % 2 != 0) {
            std::cerr << "[ERROR]: Odd number of hex characters!" << std::endl;
            debugPrint("Hex buffer leftover: " + hexBuffer);
            return false;
        }
        std::vector<char> chunkBytes;
        hexStringToBytesParallel(hexBuffer, chunkBytes, numCores);
        output.write(chunkBytes.data(), chunkBytes.size());
        writtenBytes += chunkBytes.size();
    }

    printProgress(writtenBytes, writtenBytes);
    std::cout << std::endl;

    // FINAL TIME REPORT - NEW
    auto endTime = std::chrono::steady_clock::now();
    auto totalElapsed = std::chrono::duration_cast<std::chrono::seconds>(endTime - startTime).count();

    int thrs = static_cast<int>(totalElapsed / 3600);
    int tmins = static_cast<int>((totalElapsed % 3600) / 60);
    int tsecs = static_cast<int>(totalElapsed % 60);

    std::cout << "Conversion Elapsed: "
            << std::setw(2) << std::setfill('0') << thrs << ":"
            << std::setw(2) << std::setfill('0') << tmins << ":"
            << std::setw(2) << std::setfill('0') << tsecs << std::endl;

    double totalBytesPerSec = (totalElapsed > 0) ? static_cast<double>(writtenBytes) / totalElapsed : 0.0;

    // Show Data Transfer rate speed
    std::cout << "Data Transfer rate speed: ";
    if (totalBytesPerSec >= 1024.0 * 1024.0 * 1024.0) {
        std::cout << std::fixed << std::setprecision(2)
                << (totalBytesPerSec / (1024.0 * 1024.0 * 1024.0)) << " GB/s" << std::endl;
    } else if (totalBytesPerSec >= 1024.0 * 1024.0) {
        std::cout << std::fixed << std::setprecision(2)
                << (totalBytesPerSec / (1024.0 * 1024.0)) << " MB/s" << std::endl;
    } else if (totalBytesPerSec >= 1024.0) {
        std::cout << std::fixed << std::setprecision(2)
                << (totalBytesPerSec / 1024.0) << " KB/s" << std::endl;
    } else {
        std::cout << std::fixed << std::setprecision(2)
                << totalBytesPerSec << " B/s" << std::endl;
    }
    std::cout << "Conversion successful! Output written to " << outputPath << std::endl;
    debugPrint("Final written byte count: " + std::to_string(writtenBytes));
    return true;
}

int main(int argc, char* argv[]) {
    std::map<std::string, std::string> args;

    if (argc < 2) {
        printIntroMessage();
        return 0;
    }

    bool inputHexFlagFound = false;
    bool outputFileFlagFound = false;
    bool inputHexBeforeOutput = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        debugPrint("Parsing argument: " + arg);

        std::string lowerArg = arg;
        std::transform(lowerArg.begin(), lowerArg.end(), lowerArg.begin(), ::tolower);

        if (lowerArg == "--help" || lowerArg == "-help" || lowerArg == "-h") {
            printUsage();
            return 0;
        }
        if (lowerArg == "--version" || lowerArg == "-version" || lowerArg == "-v") {
            printVersion();
            return 0;
        }
        if (lowerArg == "--debug" || lowerArg == "-debug" || lowerArg == "-d") {
            debug = true;
            debugPrint("Debug mode enabled.");
        } 
        else if (lowerArg == "--inputhex" || lowerArg == "-inputhex") {
            std::cerr << "[ERROR]: Missing required path after --inputHex!\n";
            std::cerr << "Use a required to type commands! --inputHex=<path> --outputFile=<path>\n\n";
            std::cerr << "You must type --help to see all commands.\n";
            return 1;
        } 
        else if (lowerArg == "--outputfile" || lowerArg == "-outputfile" || lowerArg == "-o" || lowerArg == "--outputfile=" || lowerArg == "-outputfile=" || lowerArg == "-o=") {
            std::cerr << "[ERROR]: Invaild arguments way!\n";
            std::cerr << "Use a required to type commands! --inputHex=<path> --outputFile=<path>\n\n";
            std::cerr << "You must type --help to see all commands.\n";
            return 1;
        }
        else if (lowerArg.find("--inputhex=") == 0 || lowerArg.find("-inputhex=") == 0 || lowerArg == "-i") {
            std::string value = arg.substr(arg.find("=") + 1);
            if (value.empty()) {
                std::cerr << "[ERROR]: Empty path given for --inputHex!\n";
                std::cerr << "Use a required to type commands! --inputHex=<path> --outputFile=<path>\n\n";
                std::cerr << "You must type --help to see all commands.\n";
                return 1;
            }
            args["--inputHex"] = value;
            inputHexFlagFound = true;
            if (outputFileFlagFound) {
                inputHexBeforeOutput = true;
            }
        } 
        else if (lowerArg.find("--outputfile=") == 0 || lowerArg.find("-outputfile=") == 0 || lowerArg == "-o") {
            std::string value = arg.substr(arg.find("=") + 1);
            if (value.empty()) {
                std::cerr << "[ERROR]: Empty path given for --outputFile!\n";
                std::cerr << "Use a required to type commands! --inputHex=<path> --outputFile=<path>\n\n";
                std::cerr << "You must type --help to see all commands.\n";
                return 1;
            }
            args["--outputFile"] = value;
            outputFileFlagFound = true;
            if (inputHexFlagFound) {
                inputHexBeforeOutput = false;
            }
        } 
        else {
            std::cerr << "[ERROR]: Unknown argument: " << arg << std::endl;
            std::cerr << "You must type --help to see all commands.\n";
            return 1;
        }
    }

    // New backwards argument order error
    if (!args.count("--inputHex") && !args.count("--outputFile") &&
        inputHexFlagFound && outputFileFlagFound && inputHexBeforeOutput) {
        std::cerr << "[ERROR]: Invaild way arguments! \n";
        std::cerr << "Use a required to type commands! --inputHex=<path> --outputFile=<path>\n\n";
        std::cerr << "You must type --help to see all commands.\n";
        return 1;
    }

    // Handling missing arguments and flags
    if (!inputHexFlagFound || !outputFileFlagFound) {
        std::cerr << "[ERROR]: Missing required arguments!\n";
        std::cerr << "Use a required to type commands! --inputHex=<path> --outputFile=<path>\n\n";
        std::cerr << "You must type --help to see all commands.\n";
        return 1;
    }

    // Debugging path arguments
    debugPrint("Input Path: " + args["--inputHex"]);
    debugPrint("Output Path: " + args["--outputFile"]);

    // Extra debugging for the order of the flags
    if (inputHexFlagFound && outputFileFlagFound) {
        if (inputHexBeforeOutput) {
            debugPrint("--inputHex flag found before --outputFile flag.");
        } else {
            debugPrint("--outputFile flag found before --inputHex flag.");
        }
    }

    // Check for invalid argument order (reversed flags with values)
    if (inputHexFlagFound && outputFileFlagFound && inputHexBeforeOutput) {
        std::cerr << "[ERROR]: Invalid argument ways!\nUse a required to type commands! --inputHex=<path> --outputFile=<path>\n\n";
        std::cerr << "You must type --help to see all commands.\n";
        return 1;
    }

    return processFile(args["--inputHex"], args["--outputFile"]) ? 0 : 1;
}
