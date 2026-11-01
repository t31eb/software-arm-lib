/*
 * Emulation of a serial EEPROM or FRAM of the 25xx family on the bus of the SpiEeprom driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 */

#include <spi_eeprom_emulator.h>

#include <sblib/timer.h>

#include <algorithm>

namespace
{
constexpr uint64_t BYTE_TIME_NS = 667; // 8 bits at 12 MHz

constexpr uint8_t WREN = 0x06;
constexpr uint8_t WRDI = 0x04;
constexpr uint8_t RDSR = 0x05;
constexpr uint8_t READ = 0x03;
constexpr uint8_t WRITE = 0x02;
}

SpiEepromEmulator::SpiEepromEmulator(const SpiEepromType& type, const uint32_t writeTimeUs) :
    memory(type.size, 0xff),
    type(type),
    writeTimeUs(writeTimeUs),
    pageLatch(type.pageSize, 0xff),
    pageLatchUsed(type.pageSize, false)
{}

void SpiEepromEmulator::select()
{
    if (!isPowered)
    {
        return;
    }
    updateTime();
    selected = true;
    transactions++;
    phase = Phase::command;
    latchedBytes = 0;
    pendingEnable = 0;
    std::fill(pageLatchUsed.begin(), pageLatchUsed.end(), false);
}

void SpiEepromEmulator::deselect()
{
    if (!isPowered || !selected)
    {
        return;
    }
    selected = false;
    updateTime();

    if (pendingEnable == WREN)
    {
        writeEnabled = !ignoreWriteEnable;
    }
    else if (pendingEnable == WRDI)
    {
        writeEnabled = false;
    }

    if ((phase == Phase::command) || ((phase == Phase::data) && ((instruction == READ) || (instruction == RDSR))) ||
        (phase == Phase::ignore))
    {
        return;
    }
    if (phase == Phase::address)
    {
        violations++; // chip select went high in the middle of the address
        return;
    }

    // phase is data of a WRITE
    if (isFram())
    {
        writeEnabled = false;
        writeCycles++;
        return;
    }
    if (latchedBytes == 0)
    {
        return;
    }

    const uint32_t page = address - (address % type.pageSize);
    for (uint32_t i = 0; i < type.pageSize; i++)
    {
        if (pageLatchUsed[i])
        {
            memory[page + i] = pageLatch[i];
        }
    }
    writeCyclePage = page;
    writeCycleRunning = true;
    writeCycleEndNs = timeNs + writeTimeUs * 1000ull;
    writeCycles++;
}

void SpiEepromEmulator::transfer(const uint8_t* txData, uint8_t* rxData, const uint32_t length)
{
    for (uint32_t i = 0; i < length; i++)
    {
        if (powerFailArmed && (busBytes >= powerFailAtBusByte))
        {
            powerFailArmed = false;
            powerOff();
        }

        const uint8_t received = (txData != nullptr) ? txData[i] : 0xff;
        const uint8_t answer = isPowered ? exchange(received) : 0xff;
        if (rxData != nullptr)
        {
            rxData[i] = answer;
        }

        busBytes++;
        timeNs += BYTE_TIME_NS;
        updateTime();
    }
}

void SpiEepromEmulator::powerFailAfterBytes(const uint64_t byteCount)
{
    powerFailArmed = true;
    powerFailAtBusByte = busBytes + byteCount;
}

void SpiEepromEmulator::powerOff()
{
    updateTime();
    if (writeCycleRunning)
    {
        corruptPage();
    }
    isPowered = false;
    selected = false;
    writeCycleRunning = false;
    writeEnabled = false;
}

void SpiEepromEmulator::powerOn()
{
    isPowered = true;
    powerFailArmed = false;
}

uint8_t SpiEepromEmulator::status() const
{
    return static_cast<uint8_t>((inWriteCycle() ? 0x01 : 0x00) | (writeEnabled ? 0x02 : 0x00) | ((blockProtect & 0x03) << 2));
}

uint8_t SpiEepromEmulator::exchange(const uint8_t received)
{
    if (!selected)
    {
        violations++; // clock without chip select
        return 0xff;
    }

    switch (phase)
    {
        case Phase::command:
            startCommand(received);
            return 0xff;

        case Phase::address:
            address = (address << 8) | received;
            if (++addressBytes == 2)
            {
                address &= type.size - 1;
                phase = Phase::data;
            }
            return 0xff;

        case Phase::data:
            break;

        case Phase::ignore:
            return 0xff;
    }

    if (instruction == RDSR)
    {
        return status();
    }

    if (instruction == READ)
    {
        checkRollover();
        return memory[address++];
    }

    // WRITE
    if (isFram())
    {
        checkRollover();
        memory[address++] = received;
        latchedBytes++;
        return 0xff;
    }

    uint32_t offset = address % type.pageSize;
    offset = (offset + latchedBytes) % type.pageSize;
    if ((latchedBytes > 0) && (offset == 0))
    {
        violations++; // wraps around to the start of the page
    }
    pageLatch[offset] = received;
    pageLatchUsed[offset] = true;
    latchedBytes++;
    return 0xff;
}

void SpiEepromEmulator::checkRollover()
{
    if (address >= type.size)
    {
        violations++; // the chip continues at address 0
        address = 0;
    }
}

void SpiEepromEmulator::startCommand(const uint8_t newInstruction)
{
    instruction = newInstruction;
    phase = Phase::ignore;

    if (instruction == RDSR)
    {
        phase = Phase::data;
        return;
    }

    if (inWriteCycle())
    {
        violations++; // the chip ignores everything except RDSR in a write cycle
        return;
    }

    switch (instruction)
    {
        case WREN:
        case WRDI:
            pendingEnable = instruction;
            phase = Phase::command;
            break;

        case WRITE:
            if (!writeEnabled)
            {
                violations++;
                break;
            }
            [[fallthrough]];
        case READ:
            address = 0;
            addressBytes = 0;
            phase = Phase::address;
            break;

        default:
            violations++;
            break;
    }
}

void SpiEepromEmulator::updateTime()
{
    if (writeCycleRunning && !stuckInWriteCycle && (timeNs >= writeCycleEndNs))
    {
        writeCycleRunning = false;
        writeEnabled = false;
    }
    setMillis(static_cast<unsigned int>(timeNs / 1000000));
}

void SpiEepromEmulator::corruptPage()
{
    for (uint32_t i = 0; i < type.pageSize; i++)
    {
        corruption = (corruption << 1) ^ ((corruption & 0x80000000) ? 0x04C11DB7 : 0);
        memory[writeCyclePage + i] = static_cast<uint8_t>(corruption >> 24);
    }
}
