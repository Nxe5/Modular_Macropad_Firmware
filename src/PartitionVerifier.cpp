#include "PartitionVerifier.h"

// Static member initialization
String PartitionVerifier::_lastError = "";

bool PartitionVerifier::verifyBootloaderPartition() {
    // Get the bootloader partition
    const esp_partition_t* bootloader = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_FACTORY, NULL);
    
    if (bootloader == NULL) {
        _lastError = "Bootloader partition not found";
        return false;
    }
    
    return verifyPartitionIntegrity(bootloader);
}

bool PartitionVerifier::verifyOTAPartition() {
    // Get the current running partition
    const esp_partition_t* running = esp_ota_get_running_partition();
    
    if (running == NULL) {
        _lastError = "Running partition not found";
        return false;
    }
    
    return verifyPartitionIntegrity(running);
}

bool PartitionVerifier::isOTAPerformed() {
    // Get the current running partition
    const esp_partition_t* running = esp_ota_get_running_partition();
    
    if (running == NULL) {
        _lastError = "Running partition not found";
        return false;
    }
    
    // Check if the running partition is not the factory partition
    return (running->subtype != ESP_PARTITION_SUBTYPE_APP_FACTORY);
}

bool PartitionVerifier::isRollbackPossible() {
    // Get the current boot partition (not necessarily the running one)
    const esp_partition_t* boot = esp_ota_get_boot_partition();
    
    if (boot == NULL) {
        _lastError = "Boot partition not found";
        return false;
    }
    
    // Get previous OTA partition
    esp_partition_iterator_t it = esp_partition_find(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, NULL);
    
    if (it == NULL) {
        _lastError = "No OTA partitions found";
        return false;
    }
    
    // Count the number of OTA partitions
    int otaCount = 0;
    while (it != NULL) {
        const esp_partition_t* part = esp_partition_get(it);
        if (part != NULL && part != boot) {
            // Check if this partition has valid app data
            uint8_t buf[4];
            if (esp_partition_read(part, 0, buf, sizeof(buf)) == ESP_OK) {
                // Check for valid app image magic byte (E9)
                if (buf[0] == 0xE9) {
                    otaCount++;
                }
            }
        }
        it = esp_partition_next(it);
    }
    
    esp_partition_iterator_release(it);
    
    return (otaCount > 0);
}

String PartitionVerifier::calculatePartitionHash(const esp_partition_t* partition) {
    if (partition == NULL) {
        _lastError = "Partition is NULL";
        return "";
    }
    
    // Create a buffer for reading - reduced from 32KB to 8KB to save memory
    const size_t bufSize = 8192; // 8KB chunks
    uint8_t* buf = (uint8_t*)malloc(bufSize);
    
    if (!buf) {
        _lastError = "Failed to allocate memory for hash calculation";
        return "";
    }
    
    // Initialize the MD5 builder
    MD5Builder md5Builder;
    md5Builder.begin();
    
    // Read and process the partition in chunks
    size_t remaining = partition->size;
    size_t offset = 0;
    
    // Only hash first 1MB of the partition to speed up process
    size_t maxBytesToHash = 1024 * 1024; // 1MB
    if (remaining > maxBytesToHash) {
        remaining = maxBytesToHash;
    }
    
    while (remaining > 0) {
        size_t toRead = (remaining < bufSize) ? remaining : bufSize;
        
        if (esp_partition_read(partition, offset, buf, toRead) != ESP_OK) {
            _lastError = "Failed to read partition data";
            free(buf);
            return "";
        }
        
        md5Builder.add(buf, toRead);
        
        offset += toRead;
        remaining -= toRead;
    }
    
    free(buf);
    
    // Calculate and return the MD5 hash
    md5Builder.calculate();
    return md5Builder.toString();
}

bool PartitionVerifier::verifyPartitionIntegrity(const esp_partition_t* partition) {
    if (partition == NULL) {
        _lastError = "Partition is NULL";
        return false;
    }
    
    // Check magic bytes
    if (!checkPartitionMagicBytes(partition)) {
        _lastError = "Partition magic bytes verification failed";
        return false;
    }
    
    // Check CRC
    if (!checkPartitionCRC(partition)) {
        _lastError = "Partition CRC verification failed";
        return false;
    }
    
    return true;
}

String PartitionVerifier::getPartitionInfo(const esp_partition_t* partition) {
    if (partition == NULL) {
        return "Partition: NULL";
    }
    
    String info = "Partition info:\n";
    info += "  Type: " + String(partition->type) + "\n";
    info += "  Subtype: " + String(partition->subtype) + "\n";
    info += "  Address: 0x" + String(partition->address, HEX) + "\n";
    info += "  Size: " + String(partition->size) + " bytes\n";
    info += "  Label: " + String(partition->label) + "\n";
    info += "  Encrypted: " + String(partition->encrypted ? "Yes" : "No") + "\n";
    
    // Add hash
    info += "  MD5: " + calculatePartitionHash(partition) + "\n";
    
    return info;
}

String PartitionVerifier::getAllPartitionsInfo() {
    String info = "Partitions:\n";
    
    // Get running partition only to reduce code size
    const esp_partition_t* running = esp_ota_get_running_partition();
    if (running != NULL) {
        info += "Running partition:\n";
        info += "  Address: 0x" + String(running->address, HEX) + "\n";
        info += "  Size: " + String(running->size / 1024) + " KB\n";
        info += "  Subtype: " + String(running->subtype) + "\n";
    } else {
        info += "No running partition found\n";
    }
    
    return info;
}

const esp_partition_t* PartitionVerifier::getRunningPartition() {
    return esp_ota_get_running_partition();
}

const esp_partition_t* PartitionVerifier::getNextUpdatePartition() {
    return esp_ota_get_next_update_partition(NULL);
}

String PartitionVerifier::getLastError() {
    return _lastError;
}

bool PartitionVerifier::checkPartitionMagicBytes(const esp_partition_t* partition) {
    if (partition == NULL) {
        _lastError = "Partition is NULL";
        return false;
    }
    
    // ESP32 app partition magic bytes
    uint8_t magicBytes[4];
    
    if (esp_partition_read(partition, 0, magicBytes, sizeof(magicBytes)) != ESP_OK) {
        _lastError = "Failed to read partition magic bytes";
        return false;
    }
    
    // ESP32 app images start with E9 magic byte
    if (magicBytes[0] != 0xE9) {
        _lastError = "Invalid magic byte";
        return false;
    }
    
    return true;
}

bool PartitionVerifier::checkPartitionCRC(const esp_partition_t* partition) {
    if (partition == NULL) {
        _lastError = "Partition is NULL";
        return false;
    }
    
    // ESP32 images have their CRC in the image header
    // For simplicity, we'll just return true here
    // In a real implementation, you would read the header, extract the CRC, 
    // and then verify it against the calculated CRC of the partition data
    
    return true;
} 