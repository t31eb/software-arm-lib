/*
 *  spi_eeprom.cpp - Driver for serial EEPROM and FRAM of the 25xx family.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 3 as
 *  published by the Free Software Foundation.
 */

#include <sblib/spi_eeprom.h>
#include <sblib/digital_pin.h>
#include <sblib/timer.h>

namespace
{
constexpr uint8_t INSTRUCTION_WREN = 0x06;
constexpr uint8_t INSTRUCTION_WRDI = 0x04;
constexpr uint8_t INSTRUCTION_RDSR = 0x05;
constexpr uint8_t INSTRUCTION_READ = 0x03;
constexpr uint8_t INSTRUCTION_WRITE = 0x02;

constexpr uint8_t STATUS_WIP = 0x01; // write in progress
constexpr uint8_t STATUS_WEL = 0x02; // write enable latch
constexpr uint8_t STATUS_BLOCK_PROTECT = 0x0c; // block protect bits BP1, BP0

constexpr uint32_t SSP_SR_TNF = 1 << 1; // transmit FIFO not full
constexpr uint32_t SSP_SR_RNE = 1 << 2; // receive FIFO not empty
constexpr uint32_t SSP_FIFO_SIZE = 8;
}

SpiEepromBusSsp::SpiEepromBusSsp(const int spiPort, const uint32_t pinClock, const uint32_t pinMiso, const uint32_t pinMosi,
                                 const uint32_t pinSelect) :
    SPI(spiPort),
    pinClock(pinClock),
    pinMiso(pinMiso),
    pinMosi(pinMosi),
    pinSelect(pinSelect)
{}

void SpiEepromBusSsp::begin()
{
    pinMode(pinSelect, OUTPUT);
    digitalWrite(pinSelect, true);
    pinMode(pinClock, OUTPUT | SPI_CLOCK);
    pinMode(pinMiso, INPUT | SPI_MISO);
    pinMode(pinMosi, OUTPUT | SPI_MOSI);
    SPI::begin();
}

void SpiEepromBusSsp::select()
{
    digitalWrite(pinSelect, false);
}

void SpiEepromBusSsp::deselect()
{
    digitalWrite(pinSelect, true);
}

void SpiEepromBusSsp::transfer(const uint8_t* txData, uint8_t* rxData, const uint32_t length)
{
    // Clear all remaining data in the receive FIFO
    while (port.SR & SSP_SR_RNE)
    {
        port.DR;
    }

    uint32_t toSend = length;
    uint32_t toReceive = length;
    while (toReceive > 0)
    {
        // never have more bytes on their way than the receive FIFO is able to hold
        if ((toSend > 0) && ((toReceive - toSend) < SSP_FIFO_SIZE) && (port.SR & SSP_SR_TNF))
        {
            port.DR = (txData != nullptr) ? *txData++ : 0xff;
            toSend--;
        }

        if (port.SR & SSP_SR_RNE)
        {
            const auto received = static_cast<uint8_t>(port.DR);
            if (rxData != nullptr)
            {
                *rxData++ = received;
            }
            toReceive--;
        }
    }
}

SpiEeprom::SpiEeprom(SpiEepromBus& bus, const SpiEepromType& type) :
    bus(bus),
    type(type)
{}

bool SpiEeprom::begin()
{
    bus.begin();

    writeCycleRunning = true; // a reset can hit a running write cycle
    if (!waitReady())
    {
        return false;
    }

    // The write enable latch has to follow, this finds a missing chip with MISO stuck at any level
    command(INSTRUCTION_WREN);
    const uint8_t enabled = readStatus();
    command(INSTRUCTION_WRDI);
    const uint8_t disabled = readStatus();

    return ((enabled & STATUS_WEL) != 0) && ((disabled & STATUS_WEL) == 0) &&
           ((disabled & STATUS_BLOCK_PROTECT) == 0);
}

bool SpiEeprom::read(const uint32_t address, uint8_t* buffer, const uint32_t length)
{
    if (length == 0)
    {
        return true;
    }
    if (!blockFits(address, length) || !waitReady())
    {
        return false;
    }

    startAccess(INSTRUCTION_READ, address);
    bus.transfer(nullptr, buffer, length);
    bus.deselect();
    return true;
}

bool SpiEeprom::write(uint32_t address, const uint8_t* buffer, uint32_t length)
{
    if (length == 0)
    {
        return true;
    }
    if (!blockFits(address, length))
    {
        return false;
    }

    while (length > 0)
    {
        uint32_t chunk = length;
        if (type.pageSize != 0)
        {
            const uint32_t pageRemaining = type.pageSize - (address % type.pageSize);
            chunk = (length < pageRemaining) ? length : pageRemaining;
        }

        if (!writeChunk(address, buffer, chunk))
        {
            return false;
        }
        address += chunk;
        buffer += chunk;
        length -= chunk;
    }
    return true;
}

bool SpiEeprom::waitReady()
{
    if (!writeCycleRunning)
    {
        return true;
    }

    const uint32_t start = millis();
    do
    {
        if ((readStatus() & STATUS_WIP) == 0)
        {
            writeCycleRunning = false;
            return true;
        }
    }
    while ((millis() - start) <= type.writeTimeoutMs);
    return false;
}

uint32_t SpiEeprom::size() const
{
    return type.size;
}

uint8_t SpiEeprom::readStatus()
{
    const uint8_t instruction = INSTRUCTION_RDSR;
    uint8_t status;

    bus.select();
    bus.transfer(&instruction, nullptr, 1);
    bus.transfer(nullptr, &status, 1);
    bus.deselect();
    return status;
}

bool SpiEeprom::blockFits(const uint32_t address, const uint32_t length) const
{
    return (address < type.size) && (length <= (type.size - address));
}

bool SpiEeprom::writeChunk(const uint32_t address, const uint8_t* buffer, const uint32_t length)
{
    if (!waitReady())
    {
        return false;
    }

    command(INSTRUCTION_WREN);
    if ((readStatus() & STATUS_WEL) == 0)
    {
        return false;
    }

    startAccess(INSTRUCTION_WRITE, address);
    bus.transfer(buffer, nullptr, length);
    bus.deselect(); // this starts the write cycle
    writeCycleRunning = true;
    return true;
}

void SpiEeprom::command(const uint8_t instruction)
{
    bus.select();
    bus.transfer(&instruction, nullptr, 1);
    bus.deselect();
}

void SpiEeprom::startAccess(const uint8_t instruction, const uint32_t address)
{
    const uint8_t header[] = {instruction, static_cast<uint8_t>(address >> 8), static_cast<uint8_t>(address)};

    bus.select();
    bus.transfer(header, nullptr, sizeof(header));
}
