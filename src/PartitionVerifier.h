#ifndef PARTITION_VERIFIER_H
#define PARTITION_VERIFIER_H

#include <Arduino.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <MD5Builder.h>

class PartitionVerifier {
public:
    // Check the bootloader partition
    static bool verifyBootloaderPartition();
    
    // Check the OTA partition
    static bool verifyOTAPartition();
    
    // Check if OTA has been performed
    static bool isOTAPerformed();
    
    // Check if rollback is possible
    static bool isRollbackPossible();
    
    // Calculate partition hash
    static String calculatePartitionHash(const esp_partition_t* partition);
    
    // Verify partition data integrity
    static bool verifyPartitionIntegrity(const esp_partition_t* partition);
    
    // Get partition info as string
    static String getPartitionInfo(const esp_partition_t* partition);
    
    // Get info about all partitions
    static String getAllPartitionsInfo();
    
    // Get the running partition
    static const esp_partition_t* getRunningPartition();
    
    // Get the next update partition
    static const esp_partition_t* getNextUpdatePartition();
    
    // Get the last error message
    static String getLastError();

private:
    // Error message
    static String _lastError;
    
    // Helper methods
    static bool checkPartitionMagicBytes(const esp_partition_t* partition);
    static bool checkPartitionCRC(const esp_partition_t* partition);
};

#endif // PARTITION_VERIFIER_H 