/*
 *  userEepromSpi.cpp - The user EEPROM in an external SPI EEPROM or FRAM.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 3 as
 *  published by the Free Software Foundation.
 */

#include <sblib/eib/userEepromSpi.h>
#include <algorithm>
#include <cstring>

namespace
{
constexpr uint8_t MAGIC[4] = {'S', 'B', 'U', 'E'};
constexpr uint32_t HEADER_BYTES = 10;    //!< used bytes of the header: 8 bytes and the CRC
constexpr uint32_t COMPARE_CHUNK = 32;   //!< bytes read at once to compare before writing, on the stack
constexpr uint32_t FORMAT_CHUNK = 128;   //!< zeros written at once when formatting, on the stack

/** CRC-16/CCITT-FALSE (polynomial 0x1021, start 0xffff) */
uint16_t crc16(const uint8_t* data, const uint32_t length)
{
    uint16_t crc = 0xffff;
    for (uint32_t i = 0; i < length; i++)
    {
        crc ^= static_cast<uint16_t>(data[i] << 8);
        for (int bit = 0; bit < 8; bit++)
        {
            crc = (crc & 0x8000) ? static_cast<uint16_t>((crc << 1) ^ 0x1021) : static_cast<uint16_t>(crc << 1);
        }
    }
    return crc;
}
}

UserEepromSpi::UserEepromSpi(SpiEeprom& eeprom, const uint32_t size, const uint32_t chipAddress) :
    UserEepromBackend(size),
    eeprom(eeprom),
    chipAddress(chipAddress)
{
}

void UserEepromSpi::makeHeader(uint8_t* header) const
{
    memcpy(header, MAGIC, sizeof(MAGIC));
    header[4] = LAYOUT_VERSION;
    header[5] = 0;
    header[6] = static_cast<uint8_t>(size());
    header[7] = static_cast<uint8_t>(size() >> 8);
    const uint16_t crc = crc16(header, 8);
    header[8] = static_cast<uint8_t>(crc);
    header[9] = static_cast<uint8_t>(crc >> 8);
}

void UserEepromSpi::begin()
{
    started = true;
    if ((chipAddress + HEADER_SIZE + size() > eeprom.size()) || !eeprom.begin())
    {
        chipFailed = true;
        return;
    }
    reload();
}

void UserEepromSpi::reload()
{
    if (!started || chipFailed)
    {
        return;
    }
    uint8_t expected[HEADER_BYTES];
    uint8_t header[HEADER_BYTES];
    makeHeader(expected);
    if (!eeprom.read(chipAddress, header, HEADER_BYTES))
    {
        chipFailed = true;
        return;
    }
    valid = (memcmp(header, expected, HEADER_BYTES) == 0);
}

void UserEepromSpi::read(const uint32_t offset, uint8_t* buffer, const uint32_t length) const
{
    if (!started)
    {
        earlyReadCount++;
    }
    if (!started || chipFailed || !valid)
    {
        memset(buffer, 0, length);
        return;
    }
    // The chip has no state the caller can see, reading doesn't change the backend
    if (!const_cast<SpiEeprom&>(eeprom).read(chipAddress + HEADER_SIZE + offset, buffer, length))
    {
        memset(buffer, 0, length);
    }
}

void UserEepromSpi::write(const uint32_t offset, const uint8_t* buffer, const uint32_t length)
{
    if (!started || chipFailed)
    {
        return;
    }
    if (!valid)
    {
        bool allZero = true;
        for (uint32_t i = 0; i < length; i++)
        {
            allZero = allZero && (buffer[i] == 0);
        }
        if (allZero)
        {
            return; // reads as zeros already
        }
        format();
        if (chipFailed)
        {
            return;
        }
    }
    writeChanged(offset, buffer, length);
}

void UserEepromSpi::writeChanged(uint32_t chipOffset, const uint8_t* buffer, uint32_t length)
{
    uint8_t current[COMPARE_CHUNK];
    while ((length > 0) && !chipFailed)
    {
        const uint32_t count = std::min(length, COMPARE_CHUNK);
        const uint32_t address = chipAddress + HEADER_SIZE + chipOffset;
        if (!eeprom.read(address, current, count))
        {
            chipFailed = true;
            return;
        }
        if ((memcmp(current, buffer, count) != 0) && !eeprom.write(address, buffer, count))
        {
            chipFailed = true;
            return;
        }
        chipOffset += count;
        buffer += count;
        length -= count;
    }
}

void UserEepromSpi::format()
{
    // Chunks end at the write pages of an EEPROM (128 bytes), so every page takes one write cycle
    uint8_t zeros[FORMAT_CHUNK] = {};
    uint32_t address = chipAddress + HEADER_SIZE;
    const uint32_t end = address + size();
    while (address < end)
    {
        const uint32_t count = std::min(FORMAT_CHUNK - (address % FORMAT_CHUNK), end - address);
        if (!eeprom.write(address, zeros, count))
        {
            chipFailed = true;
            return;
        }
        address += count;
    }
    uint8_t header[HEADER_BYTES];
    makeHeader(header);
    if (!eeprom.write(chipAddress, header, HEADER_BYTES))
    {
        chipFailed = true;
        return;
    }
    valid = true;
}

void UserEepromSpi::flush()
{
    if (started && !chipFailed && !eeprom.waitReady())
    {
        chipFailed = true;
    }
}
