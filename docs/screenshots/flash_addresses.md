# ESP32-S3 Flash Burn Addresses
## 📋 Core Flash Burn Address List (Mandatory)
|Firmware File Name | Corresponding Burn Address | Notes |
|--------------|--------------|----------|
|bootloader.bin | 0x0 | Bootloader, essential for ESP32-S3 startup |
|partition-table.bin | 0x8000 | Partition table, defines storage ranges for firmware, data, and other partitions |
|ch9350_led_switch.bin | 0x10000 | Project core firmware (implements KVM switch functionality) |
|nvs_partition.bin (Optional) | 0x9000 | NVS (Non-Volatile Storage) partition, used to save device configuration parameters|
## ⚙️ Mandatory Flash Burn Parameter Configuration
In addition to matching the addresses, the following parameters must also be consistent; otherwise, it will result in flash failure or device startup failure:

1. Crystal Oscillator Frequency: 80MHz (Recommended default value for ESP32-S3, do not change to 40MHz)
2. Flash Mode: DIO (Compatible with most ESP32-S3 development board flash memory, avoid using QIO mode)
3. Flash Size: 2MB (If your development board has 4MB/8MB flash memory, you can select the corresponding size, and the partition table needs to be adapted synchronously)
4. Baud Rate: 115200 (Stable transmission; excessively high baud rate may cause transmission errors, while excessively low baud rate will result in long flash time)

## 🚨 Important Notes

1.  The burn addresses of all firmware files cannot be modified arbitrarily; they must be consistent with the partition table configuration during compilation, otherwise the device cannot start normally.
2.  If you recompile the firmware using ESP-IDF, ensure that the addresses in the partition.csv partition table file are consistent with this document.
3.  During flashing, follow the sequence of "select the address first → then add the corresponding firmware file" to avoid mismatching files and addresses.
4.  If you receive prompts such as "startup failure" or "partition table error" after flashing, first check whether the address configuration and flash size parameters are correct.

## 📎 Related Documents

    ESP32-S3 Official Partition Table Documentation
    Project Flashing Tutorial: Root directory README.md (English) / docs/README_zh.md (Chinese)
