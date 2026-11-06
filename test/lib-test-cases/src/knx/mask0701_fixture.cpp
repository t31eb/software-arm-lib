/*
 * Test fixture for characterization tests of the MASK0701 memory paths
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 */

#include "mask0701_fixture.h"

#include <sblib/eib/apci.h>
#include <sblib/eib/userEepromFlash.h>
#include <sblib/eib/userEepromSpi.h>
#include <sblib/internal/iap.h>
#include <sblib/timer.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>

namespace
{
constexpr size_t BUFFER_SIZE = 32;
constexpr uint8_t MEM_LOAD_CONTROL_LENGTH = DMP_LOADSTATE_MACHINE_WRITE_RCO_MEM_LENGTH;
constexpr uint32_t LOAD_CONTROL_ADDRESS = 0x104;
constexpr uint8_t COM_CONFIG = COMCONF_COMM | COMCONF_READ | COMCONF_WRITE | COMCONF_TRANS | COMCONF_UPDATE;
}

Mask0701Fixture::Mask0701Fixture(std::vector<Range> ranges) :
    mapperRanges(std::move(ranges))
{
    IAP_Init_Flash(0xff);
    spiChip().powerOff(); // a power cycle: no write cycle, chip select or write enable of the test before is left
    std::fill(spiChip().memory.begin(), spiChip().memory.end(), 0xff);
    spiChip().powerOn();
    // The emulated chip sets millis() to its own clock on every access; it must not run behind a time set by a test
    // before, the driver would see the time go backwards and report a hanging write cycle.
    spiChip().timeNs = std::max<uint64_t>(spiChip().timeNs, static_cast<uint64_t>(millis()) * 1000000ull);
    boot();
}

Mask0701Fixture::~Mask0701Fixture()
{
    shutdown();
}

void Mask0701Fixture::boot()
{
    if (!mapperRanges.empty() && !spiBackend() && !flashBackend())
    {
        memMapper = new MemMapper(0xe000, 0x1000, false);
        for (const auto& [start, length] : mapperRanges)
        {
            REQUIRE(memMapper->addRange(start, length) == MemMapper::Error::Success);
        }
    }

    if (spiBackend())
    {
        spiChip().powerOn();
        auto* eeprom = new SpiEeprom(spiChip(), SPI_EEPROM_M95512R); // lives as long as the test program, like the bcu
        spiUserEeprom = new UserEepromSpi(*eeprom, UserEepromMASK0701::SIZE);
        bcu = new MASK0701(spiUserEeprom);
        // The MemMapper range moves into the same chip, as an extension of the user memory
        REQUIRE(mapperRanges.size() <= 1);
        for (const auto& [start, length] : mapperRanges)
        {
            extension = new UserMemoryExtension(start, length, new UserEepromSpi(*eeprom, length, SPI_EXTENSION));
        }
    }
    else if (flashBackend())
    {
        // A new cache on every boot: a restart loses the RAM. Like the backends, it lives as long as the test program.
        auto* cache = new FlashPageCache();
        bcu = new MASK0701(new UserEepromFlash(UserEepromMASK0701::SIZE, *cache));
        // The MemMapper range stays in the flash, where the MemMapper keeps it, through the same cache
        REQUIRE(mapperRanges.size() <= 1);
        for (const auto& [start, length] : mapperRanges)
        {
            extension = new UserMemoryExtension(start, length, new UserEepromFlash(length, *cache, FLASH_EXTENSION));
        }
    }
    else
    {
        bcu = new MASK0701();
    }
    bcuUnderTest = bcu;
    bcu->begin(MANUFACTURER, DEVICE_TYPE, VERSION);
    bcu->setMemMapper(memMapper);
    if (extension != nullptr)
    {
        bcu->setUserMemoryExtension(extension);
    }
}

void Mask0701Fixture::shutdown()
{
    bcuUnderTest = nullptr;
    delete bcu;
    bcu = nullptr;
    delete memMapper;
    memMapper = nullptr;
    extension = nullptr; // like the backends, lives as long as the test program
}

void Mask0701Fixture::restart()
{
    shutdown();
    boot();
}

bool Mask0701Fixture::memoryWrite(const uint32_t address, const std::vector<uint8_t>& data)
{
    uint8_t telegram[BUFFER_SIZE] = {};
    uint8_t sendBuffer[BUFFER_SIZE] = {};

    REQUIRE(data.size() <= 15);
    telegram[7] = static_cast<uint8_t>(data.size());
    telegram[8] = HIGH_BYTE(address);
    telegram[9] = lowByte(address);
    std::copy(data.begin(), data.end(), &telegram[10]);
    return bcu->processApci(APCI_MEMORY_WRITE_PDU, telegram, static_cast<uint8_t>(10 + data.size()), sendBuffer);
}

void Mask0701Fixture::memoryWriteChunked(const uint32_t address, const std::vector<uint8_t>& data)
{
    constexpr size_t CHUNK_SIZE = 12;

    for (size_t pos = 0; pos < data.size(); pos += CHUNK_SIZE)
    {
        const auto first = data.begin() + static_cast<std::ptrdiff_t>(pos);
        const auto last = first + static_cast<std::ptrdiff_t>(std::min(CHUNK_SIZE, data.size() - pos));
        memoryWrite(address + static_cast<uint32_t>(pos), std::vector<uint8_t>(first, last));
    }
}

Mask0701Fixture::ReadResult Mask0701Fixture::memoryRead(const uint32_t address, const uint8_t count)
{
    uint8_t telegram[BUFFER_SIZE] = {};
    uint8_t sendBuffer[BUFFER_SIZE] = {};

    telegram[7] = count;
    telegram[8] = HIGH_BYTE(address);
    telegram[9] = lowByte(address);
    REQUIRE(bcu->processApci(APCI_MEMORY_READ_PDU, telegram, 10, sendBuffer));

    const uint8_t responseCount = sendBuffer[7] & 0x0f;
    REQUIRE(makeWord(sendBuffer[8], sendBuffer[9]) == address);
    return {responseCount, std::vector<uint8_t>(&sendBuffer[10], &sendBuffer[10 + responseCount])};
}

Mask0701Fixture::ReadResult Mask0701Fixture::propertyRead(const uint8_t objectIdx, const uint8_t propertyId,
                                                          const uint8_t count, const uint8_t start)
{
    uint8_t telegram[BUFFER_SIZE] = {};
    uint8_t sendBuffer[BUFFER_SIZE] = {};

    telegram[8] = objectIdx;
    telegram[9] = propertyId;
    telegram[10] = static_cast<uint8_t>(count << 4);
    telegram[11] = start;
    sendBuffer[5] = 0;
    REQUIRE(bcu->processApci(APCI_PROPERTY_VALUE_READ_PDU, telegram, 12, sendBuffer));

    const uint8_t responseCount = sendBuffer[10] >> 4;
    const uint8_t dataLength = (sendBuffer[5] & 0x0f) - 5;
    return {responseCount, std::vector<uint8_t>(&sendBuffer[12], &sendBuffer[12 + dataLength])};
}

Mask0701Fixture::ReadResult Mask0701Fixture::propertyWrite(const uint8_t objectIdx, const uint8_t propertyId,
                                                           const std::vector<uint8_t>& data, const uint8_t count,
                                                           const uint8_t start)
{
    uint8_t telegram[BUFFER_SIZE] = {};
    uint8_t sendBuffer[BUFFER_SIZE] = {};

    telegram[8] = objectIdx;
    telegram[9] = propertyId;
    telegram[10] = static_cast<uint8_t>(count << 4);
    telegram[11] = start;
    std::copy(data.begin(), data.end(), &telegram[12]);

    // propertyValueWriteTelegram() takes the data out of the bus receive buffer
    std::memcpy(const_cast<uint8_t*>(bcu->bus->telegram), telegram, sizeof(telegram));
    bcu->bus->telegramLen = static_cast<uint8_t>(13 + data.size());
    REQUIRE(bcu->processApci(APCI_PROPERTY_VALUE_WRITE_PDU, telegram, bcu->bus->telegramLen, sendBuffer));
    bcu->bus->telegramLen = 0;

    const uint8_t responseCount = sendBuffer[10] >> 4;
    const uint8_t dataLength = (sendBuffer[5] & 0x0f) - 5;
    return {responseCount, std::vector<uint8_t>(&sendBuffer[12], &sendBuffer[12 + dataLength])};
}

bool Mask0701Fixture::loadControlMem(const uint8_t objectIdx, const uint8_t loadControl)
{
    std::vector<uint8_t> data(MEM_LOAD_CONTROL_LENGTH, 0);
    data[0] = static_cast<uint8_t>((objectIdx << 4) | loadControl);
    return memoryWrite(LOAD_CONTROL_ADDRESS, data);
}

bool Mask0701Fixture::allocTaskSegmentMem(const uint8_t objectIdx, const uint16_t address)
{
    std::vector<uint8_t> data(MEM_LOAD_CONTROL_LENGTH, 0);
    data[0] = static_cast<uint8_t>((objectIdx << 4) | LC_ADDITIONAL_LOAD_CONTROLS);
    data[1] = ST_ALLOC_ABS_TASK_SEG;
    data[DMP_LOADSTATE_MACHINE_WRITE_RCO_MEM_PAYLOAD_OFFSET + 0] = HIGH_BYTE(address);
    data[DMP_LOADSTATE_MACHINE_WRITE_RCO_MEM_PAYLOAD_OFFSET + 1] = lowByte(address);
    data[DMP_LOADSTATE_MACHINE_WRITE_RCO_MEM_PAYLOAD_OFFSET + 3] = HIGH_BYTE(MANUFACTURER);
    data[DMP_LOADSTATE_MACHINE_WRITE_RCO_MEM_PAYLOAD_OFFSET + 4] = lowByte(MANUFACTURER);
    data[DMP_LOADSTATE_MACHINE_WRITE_RCO_MEM_PAYLOAD_OFFSET + 5] = HIGH_BYTE(DEVICE_TYPE);
    data[DMP_LOADSTATE_MACHINE_WRITE_RCO_MEM_PAYLOAD_OFFSET + 6] = lowByte(DEVICE_TYPE);
    data[DMP_LOADSTATE_MACHINE_WRITE_RCO_MEM_PAYLOAD_OFFSET + 7] = VERSION;
    return memoryWrite(LOAD_CONTROL_ADDRESS, data);
}

std::vector<uint8_t> Mask0701Fixture::addressTableBytes()
{
    return {
        3, // number of entries including the physical address
        HIGH_BYTE(OWN_ADDRESS), lowByte(OWN_ADDRESS),
        HIGH_BYTE(GROUP_ADDRESS_1), lowByte(GROUP_ADDRESS_1),
        HIGH_BYTE(GROUP_ADDRESS_2), lowByte(GROUP_ADDRESS_2),
        HIGH_BYTE(PHANTOM_GROUP_ADDRESS), lowByte(PHANTOM_GROUP_ADDRESS), // not part of the table anymore
    };
}

std::vector<uint8_t> Mask0701Fixture::assocTableBytes()
{
    return {
        3,    // number of entries
        1, 1, // group address index, com object number
        2, 2,
        1, 3,
    };
}

std::vector<uint8_t> Mask0701Fixture::comTableBytes()
{
    constexpr uint16_t VALUE_1 = OBJECT_RAM + 1;
    constexpr uint16_t VALUE_2 = OBJECT_RAM + 2;
    constexpr uint16_t VALUE_3 = OBJECT_RAM + 4;

    return {
        4,                                                             // number of com objects
        HIGH_BYTE(FLAGS_TABLE), lowByte(FLAGS_TABLE),                  // pointer to the ram flags, high byte first
        HIGH_BYTE(OBJECT_RAM), lowByte(OBJECT_RAM), COM_CONFIG, BIT_1, // data pointer, high byte first; config; type
        HIGH_BYTE(VALUE_1), lowByte(VALUE_1), COM_CONFIG, BYTE_1,
        HIGH_BYTE(VALUE_2), lowByte(VALUE_2), COM_CONFIG, BYTE_2,
        HIGH_BYTE(VALUE_3), lowByte(VALUE_3), COM_CONFIG, BYTE_2,
    };
}

void Mask0701Fixture::downloadTables()
{
    const struct
    {
        uint8_t objectIdx;
        uint16_t address;
        std::vector<uint8_t> table;
    } objects[] = {
        {OT_ADDR_TABLE, ADDR_TABLE, addressTableBytes()},
        {OT_ASSOC_TABLE, ASSOC_TABLE, assocTableBytes()},
        {OT_APPLICATION, COM_TABLE, comTableBytes()},
    };

    for (const auto& [objectIdx, address, table] : objects)
    {
        loadControlMem(objectIdx, LC_START_LOADING);
        allocTaskSegmentMem(objectIdx, address);
        memoryWriteChunked(address, table);
        loadControlMem(objectIdx, LC_LOAD_COMPLETED);
    }
}

uint8_t Mask0701Fixture::mirrorByte(const uint32_t address) const
{
    uint8_t value = 0;
    REQUIRE(bcu->userEeprom->readBlock(address, &value, 1));
    return value;
}

bool Mask0701Fixture::flashBackend()
{
    const char* backend = std::getenv("SBLIB_TEST_USER_EEPROM");
    return (backend != nullptr) && (std::strcmp(backend, "flash") == 0);
}

uint8_t Mask0701Fixture::flashByte(const uint32_t address)
{
    if (spiBackend())
    {
        return spiChip().memory[UserEepromSpi::HEADER_SIZE + address - USER_EEPROM_FIRST];
    }
    return FLASH[0xF000 + address - USER_EEPROM_FIRST];
}

bool Mask0701Fixture::spiBackend()
{
    const char* backend = std::getenv("SBLIB_TEST_USER_EEPROM");
    return (backend != nullptr) && (std::strcmp(backend, "spi") == 0);
}

SpiEepromEmulator& Mask0701Fixture::spiChip()
{
    // A short write cycle keeps the tests fast, the waiting is the same as with 5 ms
    static SpiEepromEmulator chip(SPI_EEPROM_M95512R, 50);
    return chip;
}
