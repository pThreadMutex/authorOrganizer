#include <iostream>
#include <filesystem>
#include <string>
#include <vector>
#include <regex>
#include <cctype>
#include <unordered_map>
#include <system_error>
#include <limits>

#ifdef _WIN32
#include <conio.h>
#include <windows.h>
#endif

namespace fs = std::filesystem;

struct FileResult {
    std::string filename;
    bool moved = false;
    std::string moveError;
    bool linked = false;
    std::string linkError;
};

void waitForKeyPress() {
    std::cout << "\nPress any key to exit...";
#ifdef _WIN32
    _getch();
#else
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    std::cin.get();
#endif
    std::cout << "\n";
}

std::string readLineUTF8() {
#ifdef _WIN32
    HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
    DWORD mode = 0;
    if (GetConsoleMode(hIn, &mode)) {
        std::wstring wstr(1024, L'\0');
        DWORD charsRead = 0;
        if (ReadConsoleW(hIn, wstr.data(), static_cast<DWORD>(wstr.size() - 1), &charsRead, nullptr)) {
            wstr.resize(charsRead);
            while (!wstr.empty() && (wstr.back() == L'\r' || wstr.back() == L'\n')) {
                wstr.pop_back();
            }
            if (wstr.empty()) return "";

            int utf8Len = WideCharToMultiByte(CP_UTF8, 0, wstr.data(), static_cast<int>(wstr.size()), nullptr, 0, nullptr, nullptr);
            std::string utf8Str(utf8Len, '\0');
            WideCharToMultiByte(CP_UTF8, 0, wstr.data(), static_cast<int>(wstr.size()), utf8Str.data(), utf8Len, nullptr, nullptr);
            return utf8Str;
        }
    }
#endif
    std::string line;
    std::getline(std::cin, line);
    return line;
}

// Single shared map (preserves uppercase/lowercase)
static const std::unordered_map<std::string, char> kAccentMap = {
    // Lowercase
    { "\xC3\xA0", 'a' },{ "\xC3\xA1", 'a' },{ "\xC3\xA2", 'a' },{ "\xC3\xA3", 'a' },{ "\xC3\xA4", 'a' },{ "\xC3\xA5", 'a' },
    { "\xC3\xA8", 'e' },{ "\xC3\xA9", 'e' },{ "\xC3\xAA", 'e' },{ "\xC3\xAB", 'e' },
    { "\xC3\xAC", 'i' },{ "\xC3\xAD", 'i' },{ "\xC3\xAE", 'i' },{ "\xC3\xAF", 'i' },
    { "\xC3\xB2", 'o' },{ "\xC3\xB3", 'o' },{ "\xC3\xB4", 'o' },{ "\xC3\xB5", 'o' },{ "\xC3\xB6", 'o' },
    { "\xC3\xB9", 'u' },{ "\xC3\xBA", 'u' },{ "\xC3\xBB", 'u' },{ "\xC3\xBC", 'u' },
    { "\xC3\xA7", 'c' },{ "\xC3\xB1", 'n' },
    // Uppercase
    { "\xC3\x80", 'A' },{ "\xC3\x81", 'A' },{ "\xC3\x82", 'A' },{ "\xC3\x83", 'A' },{ "\xC3\x84", 'A' },{ "\xC3\x85", 'A' },
    { "\xC3\x88", 'E' },{ "\xC3\x89", 'E' },{ "\xC3\x8A", 'E' },{ "\xC3\x8B", 'E' },
    { "\xC3\x8C", 'I' },{ "\xC3\x8D", 'I' },{ "\xC3\x8E", 'I' },{ "\xC3\x8F", 'I' },
    { "\xC3\x92", 'O' },{ "\xC3\x93", 'O' },{ "\xC3\x94", 'O' },{ "\xC3\x95", 'O' },{ "\xC3\x96", 'O' },
    { "\xC3\x99", 'U' },{ "\xC3\x9A", 'U' },{ "\xC3\x9B", 'U' },{ "\xC3\x9C", 'U' },
    { "\xC3\x87", 'C' },{ "\xC3\x91", 'N' }
};

// Strips UTF-8 accent marks while keeping original uppercase/lowercase and punctuation intact
std::string stripAccentsPreserveCase(const std::string& input) {
    std::string result;
    result.reserve(input.size());

    for (size_t i = 0; i < input.size(); ) {
        unsigned char c = static_cast<unsigned char>(input[i]);

        if ((c & 0xE0) == 0xC0 && i + 1 < input.size()) {
            std::string seq = input.substr(i, 2);
            auto it = kAccentMap.find(seq);
            if (it != kAccentMap.end()) {
                result.push_back(it->second);
            }
            else {
                result += seq;
            }
            i += 2;
        }
        else {
            result.push_back(input[i]);
            i += 1;
        }
    }
    return result;
}

// Normalizes UTF-8 accented characters to basic lowercase ASCII and strips separators for matching
std::string normalizeForSearch(const std::string& input) {
    std::string result;
    result.reserve(input.size());

    for (size_t i = 0; i < input.size(); ) {
        unsigned char c = static_cast<unsigned char>(input[i]);

        // 2-byte UTF-8 sequence
        if ((c & 0xE0) == 0xC0 && i + 1 < input.size()) {
            std::string seq = input.substr(i, 2);
            if (seq != "\xC2\xA0") { // Skip UTF-8 non-breaking space
                auto it = kAccentMap.find(seq);
                if (it != kAccentMap.end()) {
                    // Convert the mapped ASCII char ('A' -> 'a') to lowercase
                    result.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(it->second))));
                }
                else {
                    result += seq;
                }
            }
            i += 2;
        }
        // 3-byte UTF-8 sequence (skip dashes/spaces in U+2000..U+206F)
        else if ((c & 0xF0) == 0xE0 && i + 2 < input.size()) {
            if (!(c == 0xE2 && static_cast<unsigned char>(input[i + 1]) == 0x80)) {
                result += input.substr(i, 3);
            }
            i += 3;
        }
        // Standard 1-byte ASCII
        else {
            if (std::isalnum(c)) {
                result.push_back(static_cast<char>(std::tolower(c)));
            }
            i += 1;
        }
    }
    return result;
}

int main(int argc, char* argv[]) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif

    fs::path targetDir = (argc > 1) ? fs::path(argv[1]) : fs::current_path();

    std::error_code ec;
    if (!fs::exists(targetDir, ec) || !fs::is_directory(targetDir, ec)) {
        std::cout << "Error: Invalid directory path: " << targetDir << std::endl;
        waitForKeyPress();
        return 1;
    }

    std::cout << "Target folder: " << fs::canonical(targetDir, ec) << std::endl;
    std::cout << "Enter author name: ";

    std::string authorInput = readLineUTF8();

    if (authorInput.empty()) {
        std::cout << "Error: Author name cannot be empty." << std::endl;
        waitForKeyPress();
        return 1;
    }

    // 1. Collect matching files first (top-level only, no subdirectories)
    std::string normalizedAuthor = normalizeForSearch(authorInput);
    if (normalizedAuthor.empty()) {
        std::cout << "Error: Author name must contain at least one letter or number." << std::endl;
        waitForKeyPress();
        return 1;
    }

    std::vector<fs::path> matchedFiles;

    for (const auto& entry : fs::directory_iterator(targetDir, ec)) {
        if (!entry.is_regular_file(ec)) continue;

        auto u8name = entry.path().filename().u8string();
        std::string filename(reinterpret_cast<const char*>(u8name.data()), u8name.size());
        std::string normalizedFilename = normalizeForSearch(filename);

        if (normalizedFilename.find(normalizedAuthor) != std::string::npos) {
            matchedFiles.push_back(entry.path());
        }
    }

    if (matchedFiles.empty()) {
        std::cout << "\nNo matching files found for \"" << authorInput << "\". No folder was created." << std::endl;
        waitForKeyPress();
        return 0;
    }

    // 2. Create destination folder named after the user input with accents removed (case preserved)
    std::string cleanFolderName = stripAccentsPreserveCase(authorInput);
    fs::path destDir = targetDir / fs::u8path(cleanFolderName);
    ec.clear();

    if (fs::exists(destDir, ec)) {
        if (fs::is_directory(destDir, ec)) {
            std::cout << "\nDestination folder already exists: " << destDir << std::endl;
        } else {
            std::cout << "\nError: A file with the name \"" << authorInput << "\" already exists." << std::endl;
            waitForKeyPress();
            return 1;
        }
    } else {
        if (fs::create_directory(destDir, ec)) {
            std::cout << "\nSuccess: Folder \"" << destDir.filename().string() << "\" created successfully." << std::endl;
        } else {
            std::cout << "\nError: Failed to create folder \"" << destDir.filename().string()
                      << "\" (" << ec.message() << ")." << std::endl;
            waitForKeyPress();
            return 1;
        }
    }

    // 3. Move each file (with accents stripped from its name) and leave an original-named hard link behind
    std::vector<FileResult> results;
    results.reserve(matchedFiles.size());

    for (const auto& originalPath : matchedFiles) {
        FileResult res;
        auto u8name = originalPath.filename().u8string();
        res.filename = std::string(reinterpret_cast<const char*>(u8name.data()), u8name.size());

        // Remove accent marks from the moved file's name while preserving case
        std::string cleanFilename = stripAccentsPreserveCase(res.filename);
        fs::path newPath = destDir / fs::u8path(cleanFilename);

        // Move the file
        ec.clear();
        fs::rename(originalPath, newPath, ec);
        if (!ec) {
            res.moved = true;

            // Create a hard link at the original path pointing to the moved file
            ec.clear();
            fs::create_hard_link(newPath, originalPath, ec);
            if (!ec) {
                res.linked = true;
            } else {
                res.linked = false;
                res.linkError = ec.message();
            }
        } else {
            res.moved = false;
            res.moveError = ec.message();
        }

        results.push_back(res);
    }

    // 4. Final summary report to std::cout
    std::cout << "\n--- Processing Summary (" << results.size() << " matches) ---\n" << std::endl;
    for (const auto& r : results) {
        std::cout << "File: " << r.filename << std::endl;
        if (r.moved) {
            std::cout << "  -> Moved to folder: YES" << std::endl;
        } else {
            std::cout << "  -> Moved to folder: NO (" << r.moveError << ")" << std::endl;
        }

        if (r.linked) {
            std::cout << "  -> Hard link created: YES" << std::endl;
        } else if (r.moved) {
            std::cout << "  -> Hard link created: NO (" << r.linkError << ")" << std::endl;
        } else {
            std::cout << "  -> Hard link created: SKIPPED (file was not moved)" << std::endl;
        }

        std::cout << std::endl;
    }

    waitForKeyPress();
    return 0;
}
