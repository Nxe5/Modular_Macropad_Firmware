#pragma once

#include <Arduino.h>
#include <FS.h>
#include <LittleFS.h>
#include <USBCDC.h>

extern USBCDC USBSerial;

class FileSystemUtils {
public:
    // Initialize the filesystem with optional formatting if mounting fails
    static bool begin(bool formatIfFailed = true) {
        if (LittleFS.begin(false)) {
            USBSerial.println("LittleFS mounted successfully");
            return true;
        }
        
        if (formatIfFailed) {
            USBSerial.println("Failed to mount LittleFS, formatting...");
            if (LittleFS.format()) {
                if (LittleFS.begin(false)) {
                    USBSerial.println("LittleFS formatted and mounted successfully");
                    return true;
                }
            }
            USBSerial.println("LittleFS format failed");
        }
        
        return false;
    }
    
    // List directory contents
    static void listDir(const char *dirname, uint8_t levels = 0) {
        USBSerial.printf("Listing directory: %s\n", dirname);

        File root = LittleFS.open(dirname);
        if (!root) {
            USBSerial.println("Failed to open directory");
            return;
        }
        if (!root.isDirectory()) {
            USBSerial.println("Not a directory");
            return;
        }

        File file = root.openNextFile();
        while (file) {
            if (file.isDirectory()) {
                USBSerial.print("  DIR : ");
                USBSerial.println(file.name());
                if (levels) {
                    listDir(file.path(), levels - 1);
                }
            } else {
                USBSerial.print("  FILE: ");
                USBSerial.print(file.name());
                USBSerial.print("  SIZE: ");
                USBSerial.println(file.size());
            }
            file = root.openNextFile();
        }
    }

    // Create directory and all parent directories if needed
    static bool createDirPath(const char *path) {
        USBSerial.printf("Creating directory path: %s\n", path);
        
        // Handle root directory
        if (strcmp(path, "/") == 0) {
            return true;
        }
        
        // Create copy of path for modification
        char *pathStr = strdup(path);
        if (!pathStr) {
            USBSerial.println("Memory allocation failed");
            return false;
        }
        
        bool success = true;
        
        // Create path segments iteratively
        char *ptr = strchr(pathStr, '/');
        while (ptr) {
            *ptr = 0; // Null-terminate the current segment
            
            // Skip empty segments (e.g., consecutive slashes)
            if (strlen(pathStr) > 0) {
                if (!LittleFS.exists(pathStr)) {
                    if (!LittleFS.mkdir(pathStr)) {
                        USBSerial.printf("Failed to create directory: %s\n", pathStr);
                        success = false;
                        break;
                    }
                }
            }
            
            *ptr = '/'; // Restore the slash
            ptr = strchr(ptr + 1, '/');
        }
        
        // Create the final directory segment if path doesn't end with '/'
        if (success && strlen(pathStr) > 0 && !LittleFS.exists(pathStr)) {
            if (!LittleFS.mkdir(pathStr)) {
                USBSerial.printf("Failed to create directory: %s\n", pathStr);
                success = false;
            }
        }
        
        free(pathStr);
        return success;
    }
    
    // Remove directory and all empty parent directories
    static bool removeDirPath(const char *path) {
        USBSerial.printf("Removing directory path: %s\n", path);
        
        bool success = LittleFS.rmdir(path);
        if (!success) {
            USBSerial.printf("Failed to remove directory: %s\n", path);
            return false;
        }
        
        // Remove empty parent directories
        char *pathStr = strdup(path);
        if (!pathStr) {
            USBSerial.println("Memory allocation failed");
            return false;
        }
        
        char *ptr = strrchr(pathStr, '/');
        while (ptr) {
            *ptr = 0; // Null-terminate at the last slash
            
            // Skip empty segments or root
            if (strlen(pathStr) == 0) {
                break;
            }
            
            // Try to remove this directory segment
            if (!LittleFS.rmdir(pathStr)) {
                // Stop if we can't remove this directory (probably not empty)
                break;
            }
            
            ptr = strrchr(pathStr, '/');
        }
        
        free(pathStr);
        return true;
    }
    
    // Read entire file into a String
    static String readFile(const char *path) {
        USBSerial.printf("Reading file: %s\n", path);
        
        File file = LittleFS.open(path, "r");
        if (!file || file.isDirectory()) {
            USBSerial.println("Failed to open file for reading");
            return String();
        }
        
        String content = file.readString();
        file.close();
        
        USBSerial.printf("Read %d bytes\n", content.length());
        return content;
    }
    
    // Write string to file, creating directories as needed
    static bool writeFile(const char *path, const String &content) {
        USBSerial.printf("Writing file: %s (%d bytes)\n", path, content.length());
        
        // Create parent directories if needed
        if (strchr(path, '/')) {
            char *pathCopy = strdup(path);
            if (!pathCopy) {
                USBSerial.println("Memory allocation failed");
                return false;
            }
            
            char *lastSlash = strrchr(pathCopy, '/');
            if (lastSlash) {
                *lastSlash = 0; // Null-terminate at the last slash
                if (strlen(pathCopy) > 0) {
                    createDirPath(pathCopy);
                }
            }
            
            free(pathCopy);
        }
        
        // Open file for writing
        File file = LittleFS.open(path, "w");
        if (!file) {
            USBSerial.println("Failed to open file for writing");
            return false;
        }
        
        // Write content
        size_t written = file.print(content);
        file.close();
        
        if (written != content.length()) {
            USBSerial.printf("Write incomplete: %d/%d bytes\n", written, content.length());
            return false;
        }
        
        USBSerial.println("File written successfully");
        return true;
    }
    
    // Append string to file, creating file if it doesn't exist
    static bool appendFile(const char *path, const String &content) {
        // If file doesn't exist, create it
        if (!LittleFS.exists(path)) {
            return writeFile(path, content);
        }
        
        USBSerial.printf("Appending to file: %s (%d bytes)\n", path, content.length());
        
        File file = LittleFS.open(path, "a");
        if (!file) {
            USBSerial.println("Failed to open file for appending");
            return false;
        }
        
        size_t written = file.print(content);
        file.close();
        
        if (written != content.length()) {
            USBSerial.printf("Append incomplete: %d/%d bytes\n", written, content.length());
            return false;
        }
        
        USBSerial.println("Appended successfully");
        return true;
    }
    
    // Delete file and remove any empty parent directories
    static bool deleteFileAndDirs(const char *path) {
        USBSerial.printf("Deleting file with path cleanup: %s\n", path);
        
        if (!LittleFS.remove(path)) {
            USBSerial.println("Failed to delete file");
            return false;
        }
        
        // Remove empty parent directories
        char *pathCopy = strdup(path);
        if (!pathCopy) {
            USBSerial.println("Memory allocation failed");
            return true; // File was still deleted
        }
        
        char *lastSlash = strrchr(pathCopy, '/');
        if (lastSlash) {
            *lastSlash = 0; // Null-terminate at the last slash
            
            // Skip if path is empty or root
            if (strlen(pathCopy) > 0 && strcmp(pathCopy, "/") != 0) {
                removeDirPath(pathCopy);
            }
        }
        
        free(pathCopy);
        return true;
    }
    
    // Rename file
    static bool renameFile(const char *oldPath, const char *newPath) {
        USBSerial.printf("Renaming file: %s to %s\n", oldPath, newPath);
        
        // Create parent directories for the new path if needed
        if (strchr(newPath, '/')) {
            char *pathCopy = strdup(newPath);
            if (!pathCopy) {
                USBSerial.println("Memory allocation failed");
                return false;
            }
            
            char *lastSlash = strrchr(pathCopy, '/');
            if (lastSlash) {
                *lastSlash = 0; // Null-terminate at the last slash
                if (strlen(pathCopy) > 0) {
                    createDirPath(pathCopy);
                }
            }
            
            free(pathCopy);
        }
        
        if (!LittleFS.rename(oldPath, newPath)) {
            USBSerial.println("Rename failed");
            return false;
        }
        
        USBSerial.println("File renamed successfully");
        return true;
    }
    
    // Check if file exists
    static bool fileExists(const char *path) {
        return LittleFS.exists(path);
    }
    
    // Get file size
    static size_t fileSize(const char *path) {
        File file = LittleFS.open(path, "r");
        if (!file || file.isDirectory()) {
            return 0;
        }
        
        size_t size = file.size();
        file.close();
        return size;
    }
    
    // Test filesystem performance
    static void testPerformance(const char *path, size_t blockSize = 512, size_t blockCount = 100) {
        USBSerial.printf("Testing filesystem performance with %s\n", path);
        
        uint8_t *buffer = (uint8_t *)malloc(blockSize);
        if (!buffer) {
            USBSerial.println("Failed to allocate buffer for testing");
            return;
        }
        
        // Fill buffer with test pattern
        for (size_t i = 0; i < blockSize; i++) {
            buffer[i] = i & 0xFF;
        }
        
        // Write test
        File file = LittleFS.open(path, "w");
        if (!file) {
            USBSerial.println("Failed to open file for write test");
            free(buffer);
            return;
        }
        
        uint32_t writeStart = millis();
        for (size_t i = 0; i < blockCount; i++) {
            if (i % 10 == 0) {
                USBSerial.print(".");
            }
            file.write(buffer, blockSize);
        }
        uint32_t writeTime = millis() - writeStart;
        file.close();
        
        // Calculate and display write performance
        float writeSpeed = (blockSize * blockCount) / (writeTime / 1000.0f) / 1024.0f;
        USBSerial.printf("\nWrite: %u bytes in %lu ms (%.2f KB/s)\n", 
            blockSize * blockCount, writeTime, writeSpeed);
        
        // Read test
        file = LittleFS.open(path, "r");
        if (!file) {
            USBSerial.println("Failed to open file for read test");
            free(buffer);
            return;
        }
        
        uint32_t readStart = millis();
        size_t bytesRead = 0;
        while (bytesRead < blockSize * blockCount) {
            size_t toRead = min(blockSize, blockSize * blockCount - bytesRead);
            size_t read = file.read(buffer, toRead);
            
            if (read == 0) break;
            bytesRead += read;
            
            if (bytesRead % (blockSize * 10) == 0) {
                USBSerial.print(".");
            }
        }
        uint32_t readTime = millis() - readStart;
        file.close();
        
        // Calculate and display read performance
        float readSpeed = bytesRead / (readTime / 1000.0f) / 1024.0f;
        USBSerial.printf("\nRead: %u bytes in %lu ms (%.2f KB/s)\n", 
            bytesRead, readTime, readSpeed);
        
        // Cleanup
        LittleFS.remove(path);
        free(buffer);
    }
}; 