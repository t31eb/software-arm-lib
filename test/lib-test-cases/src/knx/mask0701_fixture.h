/*
 * Test fixture for characterization tests of the MASK0701 memory paths
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 */

#ifndef TEST_SBLIB_KNX_MASK0701_FIXTURE_H_
#define TEST_SBLIB_KNX_MASK0701_FIXTURE_H_

#include <protocol.h> // grants access to private and protected members of the bcu classes
#include <spi_eeprom_emulator.h>

#include <sblib/eib/userEepromFlash.h>
#include <sblib/eib/userEepromSpi.h>

#include <sblib/bits.h>
#include <sblib/mem_mapper.h>

#include <cstdint>
#include <utility>
#include <vector>

/**
 * A MASK0701 bcu set up like the MultiSensorAktor application does it.
 *
 * @details All telegrams are passed directly to @ref MASK0701::processApci, so tests see the ETS
 *          memory path without the transport layer. The emulated flash survives @ref restart,
 *          which makes it the equivalent of a power cycle.
 * @warning Never trigger a code path which calls fatalError(), it loops forever on the host.
 */
class Mask0701Fixture
{
public:
    using Range = std::pair<uint32_t, uint32_t>; //!< virtual start address and length of a MemMapper range

    static constexpr int MANUFACTURER = 0x13A;
    static constexpr int DEVICE_TYPE = 0x01;
    static constexpr int VERSION = 0x0B;

    static constexpr uint32_t USER_EEPROM_FIRST = 0x3F00;
    static constexpr uint32_t USER_EEPROM_LAST = 0x4AFF;
    static constexpr uint32_t RAM_START = 0x05FC;
    static constexpr uint32_t RAM_END = 0x08FF;
    static constexpr uint32_t MAPPER_START = 0x6000;
    static constexpr uint32_t MAPPER_END = 0x6EFF;

    static constexpr uint32_t LOAD_STATE = USER_EEPROM_FIRST + 0x370;     //!< first load state byte in the virtual address space
    static constexpr uint32_t ADDR_TAB_ADDR = USER_EEPROM_FIRST + 0x378;  //!< address of the address table pointer
    static constexpr uint32_t ASSOC_TAB_ADDR = USER_EEPROM_FIRST + 0x37A; //!< address of the association table pointer
    static constexpr uint32_t COMMS_TAB_ADDR = USER_EEPROM_FIRST + 0x37C; //!< address of the com object table pointer

    static constexpr uint16_t ADDR_TABLE = 0x4000;
    static constexpr uint16_t ASSOC_TABLE = 0x4010;
    static constexpr uint16_t COM_TABLE = 0x4020;
    static constexpr uint16_t FLAGS_TABLE = 0x06C5; //!< same address as the MultiSensorAktor uses
    static constexpr uint16_t OBJECT_RAM = 0x0700;

    static constexpr uint16_t OWN_ADDRESS = 0x1105;
    static constexpr uint16_t GROUP_ADDRESS_1 = 0x0801;
    static constexpr uint16_t GROUP_ADDRESS_2 = 0x0802;
    static constexpr uint16_t PHANTOM_GROUP_ADDRESS = 0xABCD; //!< the two bytes behind the address table

    /** Result of a A_Memory_Read or A_PropertyValue_Read */
    struct ReadResult
    {
        uint8_t count;             //!< count field of the response
        std::vector<uint8_t> data; //!< the data bytes of the response
    };

    /**
     * Erases the emulated flash and boots a bcu.
     *
     * @param ranges MemMapper ranges, default is the range the MultiSensorAktor uses.
     *               An empty list means no MemMapper at all.
     */
    explicit Mask0701Fixture(std::vector<Range> ranges = {{MAPPER_START, MAPPER_END - MAPPER_START + 1}});
    ~Mask0701Fixture();

    Mask0701Fixture(const Mask0701Fixture&) = delete;
    Mask0701Fixture& operator=(const Mask0701Fixture&) = delete;

    /** Simulates a power cycle: the RAM is lost, the emulated flash stays as it is. */
    void restart();

    /** @return true if the bcu wants to send a response. */
    bool memoryWrite(uint32_t address, const std::vector<uint8_t>& data);

    /** Writes any amount of data split into telegrams with at most 12 data bytes, like ETS does. */
    void memoryWriteChunked(uint32_t address, const std::vector<uint8_t>& data);

    ReadResult memoryRead(uint32_t address, uint8_t count);

    ReadResult propertyRead(uint8_t objectIdx, uint8_t propertyId, uint8_t count = 1, uint8_t start = 1);
    ReadResult propertyWrite(uint8_t objectIdx, uint8_t propertyId, const std::vector<uint8_t>& data, uint8_t count = 1,
                             uint8_t start = 1);

    /** Sends a load control over the deprecated DMP_LoadStateMachineWrite_RCo_Mem (address 0x104). */
    bool loadControlMem(uint8_t objectIdx, uint8_t loadControl);

    /** Sends an Additional Load Control AllocAbsTaskSegment over DMP_LoadStateMachineWrite_RCo_Mem. */
    bool allocTaskSegmentMem(uint8_t objectIdx, uint16_t address);

    /** A small address, association and com object table as bytes, see the constants above. */
    static std::vector<uint8_t> addressTableBytes();
    static std::vector<uint8_t> assocTableBytes();
    static std::vector<uint8_t> comTableBytes();

    /** Runs the load procedure of ETS for all three tables, data written with A_Memory_Write. */
    void downloadTables();

    /** Byte of the user EEPROM as the bcu sees it (the RAM mirror or the cache of the flash backend), for assertions only. */
    [[nodiscard]] uint8_t mirrorByte(uint32_t address) const;

    /**
     * @return true if the MASK0701 tests run with the user EEPROM in the flash (@ref UserEepromFlash) instead of the
     *         RAM mirror: environment variable SBLIB_TEST_USER_EEPROM=flash.
     */
    [[nodiscard]] static bool flashBackend();

    /**
     * @return true if the MASK0701 tests run with the user EEPROM in an emulated SPI EEPROM M95512-R
     *         (@ref UserEepromSpi): environment variable SBLIB_TEST_USER_EEPROM=spi.
     */
    [[nodiscard]] static bool spiBackend();

    /** The emulated SPI chip, it keeps its memory over @ref restart and is erased (0xff) by the constructor. */
    static SpiEepromEmulator& spiChip();

    UserEepromSpi* spiUserEeprom = nullptr; //!< the backend if @ref spiBackend
    /**
     * With @ref spiBackend the MemMapper range is an extension of the user memory in the chip, at @ref SPI_EXTENSION.
     * With @ref flashBackend it is one in the flash at @ref FLASH_EXTENSION, sharing the cache of the user EEPROM.
     */
    UserMemoryExtension* extension = nullptr;
    static constexpr uint32_t SPI_EXTENSION = 0x1000;   //!< chip address of the header of the extension
    static constexpr uint32_t FLASH_EXTENSION = 0xE100; //!< flash offset of the extension, where the MemMapper keeps it

    /**
     * Persistent byte of the user EEPROM at virtual address @p address: in the emulated flash, or in the emulated
     * SPI chip if the tests run with @ref spiBackend.
     */
    [[nodiscard]] static uint8_t flashByte(uint32_t address);

    MASK0701* bcu = nullptr;
    MemMapper* memMapper = nullptr;

private:
    void boot();
    void shutdown();

    std::vector<Range> mapperRanges;
};

#endif /* TEST_SBLIB_KNX_MASK0701_FIXTURE_H_ */
