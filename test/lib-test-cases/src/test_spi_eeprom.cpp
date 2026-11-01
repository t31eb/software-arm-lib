/*
 * Tests for the SpiEeprom driver and the emulated memory chip
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 */

#include <spi_eeprom_emulator.h>

#include <sblib/timer.h>

#include <catch.hpp> // If possible, include catch.hpp as last header

#include <algorithm>
#include <numeric>

using Data = std::vector<uint8_t>;

namespace
{
constexpr uint8_t LOADED = 0x01;

Data pattern(const size_t length, const uint8_t seed = 1)
{
    Data data(length);
    std::iota(data.begin(), data.end(), seed);
    return data;
}

void busCommand(SpiEepromEmulator& chip, const Data& bytes)
{
    chip.select();
    chip.transfer(bytes.data(), nullptr, static_cast<uint32_t>(bytes.size()));
    chip.deselect();
}

uint8_t busStatus(SpiEepromEmulator& chip)
{
    const uint8_t rdsr = 0x05;
    uint8_t status;
    chip.select();
    chip.transfer(&rdsr, nullptr, 1);
    chip.transfer(nullptr, &status, 1);
    chip.deselect();
    return status;
}

/** Writes the tables, waits for them and writes the load state afterwards, like a download does it. */
bool download(SpiEeprom& eeprom, const Data& tables, const uint32_t tablesAddress, const uint32_t loadStateAddress)
{
    return eeprom.write(tablesAddress, tables.data(), static_cast<uint32_t>(tables.size())) && eeprom.waitReady() &&
           eeprom.write(loadStateAddress, &LOADED, 1) && eeprom.waitReady();
}
}

TEST_CASE("SpiEepromEmulator", "[poc][spi]")
{
    setMillis(0);
    SpiEepromEmulator chip = SpiEepromEmulator::m95512r();

    SECTION("WRITE without WREN is ignored")
    {
        busCommand(chip, {0x02, 0x00, 0x10, 0xAA});
        REQUIRE(chip.memory[0x10] == 0xFF);
        REQUIRE(chip.violations == 1);
    }

    SECTION("a write crossing a page boundary wraps around")
    {
        busCommand(chip, {0x06});
        busCommand(chip, {0x02, 0x00, 0x7F, 0xAA, 0xBB});
        REQUIRE(chip.memory[0x7F] == 0xAA);
        REQUIRE(chip.memory[0x80] == 0xFF);
        REQUIRE(chip.memory[0x00] == 0xBB);
        REQUIRE(chip.violations == 1);
    }

    SECTION("write cycle: only the status answers, afterwards WEL is cleared")
    {
        busCommand(chip, {0x06});
        REQUIRE(busStatus(chip) == 0x02);
        busCommand(chip, {0x02, 0x00, 0x10, 0xAA});
        REQUIRE(busStatus(chip) == 0x03);

        busCommand(chip, {0x06});
        REQUIRE(chip.violations == 1);

        chip.timeNs += 5000000;
        REQUIRE(busStatus(chip) == 0x00);
    }

    SECTION("power loss before chip select goes high writes nothing, in the write cycle destroys the page")
    {
        busCommand(chip, {0x06});
        chip.select();
        const Data bytes = {0x02, 0x00, 0x10, 0xAA};
        chip.transfer(bytes.data(), nullptr, 4);
        chip.powerOff();
        REQUIRE(chip.memory[0x10] == 0xFF);

        chip.powerOn();
        REQUIRE((busStatus(chip) & 0x02) == 0); // WEL is lost

        const Data other(chip.memory.begin(), chip.memory.begin() + 128);
        busCommand(chip, {0x06});
        busCommand(chip, {0x02, 0x00, 0x10, 0xAA});
        chip.powerOff();
        REQUIRE_FALSE(std::equal(other.begin(), other.end(), chip.memory.begin()));
        REQUIRE(std::all_of(chip.memory.begin() + 128, chip.memory.end(), [](const uint8_t b) { return b == 0xFF; }));
    }

    SECTION("FRAM writes every byte on arrival")
    {
        SpiEepromEmulator fram = SpiEepromEmulator::mb85rs256b();
        busCommand(fram, {0x06});
        fram.select();
        const Data bytes = {0x02, 0x00, 0x10, 0xAA, 0xBB};
        fram.transfer(bytes.data(), nullptr, 4);
        fram.powerOff();
        REQUIRE(fram.memory[0x10] == 0xAA);
        REQUIRE(fram.memory[0x11] == 0xFF);
    }
}

TEST_CASE("SpiEeprom begin", "[poc][spi]")
{
    setMillis(0);
    SpiEepromEmulator chip = SpiEepromEmulator::m95512r();
    SpiEeprom eeprom(chip, SPI_EEPROM_M95512R);

    SECTION("chip answers")
    {
        REQUIRE(eeprom.begin());
        REQUIRE(eeprom.size() == 0x10000);
        REQUIRE(chip.violations == 0);
    }

    SECTION("no chip, MISO high")
    {
        chip.powerOff();
        REQUIRE_FALSE(eeprom.begin());
    }

    SECTION("block protect bits set")
    {
        chip.blockProtect = 1;
        REQUIRE_FALSE(eeprom.begin());
    }

    SECTION("chip doesn't accept write enable")
    {
        chip.ignoreWriteEnable = true;
        REQUIRE_FALSE(eeprom.begin());
    }

    SECTION("chip hangs in a write cycle")
    {
        busCommand(chip, {0x06});
        busCommand(chip, {0x02, 0x00, 0x10, 0xAA});
        chip.stuckInWriteCycle = true;
        REQUIRE_FALSE(eeprom.begin());
    }
}

TEST_CASE("SpiEeprom read and write", "[poc][spi]")
{
    setMillis(0);

    SECTION("M95512-R")
    {
        SpiEepromEmulator chip = SpiEepromEmulator::m95512r();
        SpiEeprom eeprom(chip, SPI_EEPROM_M95512R);
        REQUIRE(eeprom.begin());
        const auto setupCycles = chip.writeCycles;

        SECTION("inside of a page")
        {
            const Data data = pattern(16);
            REQUIRE(eeprom.write(0x1010, data.data(), 16));
            REQUIRE(chip.writeCycles == setupCycles + 1);

            Data buffer(16);
            REQUIRE(eeprom.read(0x1010, buffer.data(), 16));
            REQUIRE(buffer == data);
        }

        SECTION("over page boundaries: one write cycle per page, in order and without wrap around")
        {
            const Data data = pattern(300);
            REQUIRE(eeprom.write(0x107F, data.data(), 300)); // 1 + 128 + 128 + 43 bytes
            REQUIRE(chip.writeCycles == setupCycles + 4);

            Data buffer(300);
            REQUIRE(eeprom.read(0x107F, buffer.data(), 300));
            REQUIRE(buffer == data);
            REQUIRE(chip.memory[0x107E] == 0xFF);
            REQUIRE(chip.memory[0x107F + 300] == 0xFF);
        }

        SECTION("exactly one page")
        {
            const Data data = pattern(128);
            REQUIRE(eeprom.write(0x1000, data.data(), 128));
            REQUIRE(chip.writeCycles == setupCycles + 1);
        }

        SECTION("reading right after writing waits for the write cycle")
        {
            const uint8_t value = 0x42;
            uint8_t buffer = 0;
            const uint64_t start = chip.timeNs;
            REQUIRE(eeprom.write(0x2000, &value, 1));
            REQUIRE(eeprom.read(0x2000, &buffer, 1));
            REQUIRE(buffer == value);
            REQUIRE(chip.timeNs - start >= 5000000);
        }

        SECTION("a read of a single byte has the cost of the access profile")
        {
            uint8_t buffer;
            const uint64_t start = chip.timeNs;
            REQUIRE(eeprom.read(0, &buffer, 1));
            REQUIRE(chip.timeNs - start == 4 * 667); // command, address, data
        }

        SECTION("last byte of the chip")
        {
            const Data data = pattern(2);
            REQUIRE(eeprom.write(0xFFFE, data.data(), 2));
            Data buffer(2);
            REQUIRE(eeprom.read(0xFFFE, buffer.data(), 2));
            REQUIRE(buffer == data);
        }

        SECTION("outside of the chip nothing happens on the bus")
        {
            const Data data = pattern(4);
            Data buffer(4);
            const auto bytes = chip.busBytes;
            REQUIRE_FALSE(eeprom.write(0xFFFE, data.data(), 4));
            REQUIRE_FALSE(eeprom.read(0xFFFE, buffer.data(), 4));
            REQUIRE_FALSE(eeprom.read(0x10000, buffer.data(), 1));
            REQUIRE_FALSE(eeprom.read(0xFFFFFFFF, buffer.data(), 2));
            REQUIRE(chip.busBytes == bytes);
        }

        SECTION("length zero")
        {
            const auto bytes = chip.busBytes;
            REQUIRE(eeprom.write(0x20000, nullptr, 0));
            REQUIRE(eeprom.read(0x20000, nullptr, 0));
            REQUIRE(chip.busBytes == bytes);
        }

        SECTION("chip stops accepting writes")
        {
            const Data data = pattern(4);
            chip.ignoreWriteEnable = true;
            REQUIRE_FALSE(eeprom.write(0x1000, data.data(), 4));
            REQUIRE(chip.memory[0x1000] == 0xFF);
        }

        SECTION("a hanging write cycle is reported after the timeout")
        {
            const Data data = pattern(4);
            REQUIRE(eeprom.write(0x1000, data.data(), 4));
            chip.stuckInWriteCycle = true;
            const uint32_t start = millis();
            REQUIRE_FALSE(eeprom.waitReady());
            REQUIRE(millis() - start > SPI_EEPROM_M95512R.writeTimeoutMs);
            REQUIRE_FALSE(eeprom.read(0x1000, nullptr, 1));
        }

        REQUIRE(chip.violations == 0);
    }

    SECTION("MB85RS256B: no pages and no waiting")
    {
        SpiEepromEmulator chip = SpiEepromEmulator::mb85rs256b();
        SpiEeprom eeprom(chip, SPI_FRAM_MB85RS256B);
        REQUIRE(eeprom.begin());
        REQUIRE(eeprom.size() == 0x8000);

        const Data data = pattern(1000);
        const auto transactions = chip.transactions;
        REQUIRE(eeprom.write(0x07F0, data.data(), 1000));
        REQUIRE(chip.transactions == transactions + 3); // WREN, RDSR, WRITE

        Data buffer(1000);
        REQUIRE(eeprom.read(0x07F0, buffer.data(), 1000));
        REQUIRE(buffer == data);
        REQUIRE_FALSE(eeprom.read(0x7FFF, buffer.data(), 2));
        REQUIRE(chip.violations == 0);
    }
}

TEST_CASE("SpiEeprom power loss at every byte of a download", "[poc][spi]")
{
    // A short write cycle, every status poll is a further point of failure which adds nothing new
    constexpr uint32_t WRITE_TIME_US = 50;
    constexpr uint32_t TABLES = 0x4000;
    constexpr uint32_t LOAD_STATE = 0x4270;
    const Data tables = pattern(600, 7);

    const auto run = [&](const SpiEepromType& type, const uint32_t writeTimeUs) {
        uint64_t totalBytes = 0;
        {
            setMillis(0);
            SpiEepromEmulator chip(type, writeTimeUs);
            SpiEeprom eeprom(chip, type);
            REQUIRE(eeprom.begin());
            const auto start = chip.busBytes;
            REQUIRE(download(eeprom, tables, TABLES, LOAD_STATE));
            totalBytes = chip.busBytes - start;
            REQUIRE(chip.violations == 0);
        }
        REQUIRE(totalBytes > tables.size());

        uint64_t loadedAfterFailure = 0;
        for (uint64_t failAt = 0; failAt <= totalBytes; failAt++)
        {
            setMillis(0);
            SpiEepromEmulator chip(type, writeTimeUs);
            SpiEeprom eeprom(chip, type);
            REQUIRE(eeprom.begin());

            chip.powerFailAfterBytes(failAt);
            [[maybe_unused]] const bool completed = download(eeprom, tables, TABLES, LOAD_STATE);

            chip.powerOn();
            SpiEeprom rebooted(chip, type);
            REQUIRE(rebooted.begin());

            // What counts is the content, not what the driver reported before the power was gone
            uint8_t loadState = 0;
            REQUIRE(rebooted.read(LOAD_STATE, &loadState, 1));
            if (loadState == LOADED)
            {
                Data content(tables.size());
                REQUIRE(rebooted.read(TABLES, content.data(), static_cast<uint32_t>(content.size())));
                INFO("power loss at byte " << failAt << " of " << totalBytes);
                REQUIRE(content == tables);
                loadedAfterFailure++;
            }
            REQUIRE(chip.violations == 0);
        }
        return loadedAfterFailure;
    };

    SECTION("M95512-R")
    {
        REQUIRE(run(SPI_EEPROM_M95512R, WRITE_TIME_US) > 0);
    }

    SECTION("MB85RS256B")
    {
        REQUIRE(run(SPI_FRAM_MB85RS256B, 0) > 0);
    }
}
