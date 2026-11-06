/*
 * Tests of the user EEPROM backend in an external SPI EEPROM or FRAM (UserEepromSpi), with the emulated chip.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 */

#include "mask0701_fixture.h"

#include <sblib/eib/userEepromSpi.h>
#include <sblib/timer.h>

#include <catch.hpp> // If possible, include catch.hpp as last header

#include <numeric>
#include <random>

using Data = std::vector<uint8_t>;

namespace
{
constexpr uint32_t SIZE = 3072;
constexpr uint32_t DATA = UserEepromSpi::HEADER_SIZE; //!< chip address of the first data byte
constexpr uint32_t WRITE_TIME_US = 50;               //!< short write cycles keep the tests fast

Data readAll(const UserEepromBackend& backend)
{
    Data bytes(SIZE);
    backend.read(0, bytes.data(), SIZE);
    return bytes;
}

void write(UserEepromBackend& backend, const uint32_t offset, const Data& bytes)
{
    backend.write(offset, bytes.data(), static_cast<uint32_t>(bytes.size()));
}

/** A chip and a backend on it, like after a reset */
struct Board
{
    explicit Board(SpiEepromEmulator& chip) : eeprom(chip, SPI_EEPROM_M95512R), backend(eeprom, SIZE) {}
    SpiEeprom eeprom;
    UserEepromSpi backend;
};
}

TEST_CASE("User EEPROM in an SPI EEPROM", "[userEeprom][spiBackend]")
{
    setMillis(0);
    SpiEepromEmulator chip(SPI_EEPROM_M95512R, WRITE_TIME_US);

    SECTION("before begin nothing is read from the chip, reads return zeros and are counted")
    {
        Board board(chip);
        uint8_t value = 0x55;
        board.backend.read(0x370, &value, 1);
        REQUIRE(value == 0);
        REQUIRE(board.backend.earlyReads() == 1);
        REQUIRE(chip.busBytes == 0);
    }

    SECTION("an empty chip reads as zeros, writing zeros doesn't format it")
    {
        Board board(chip);
        board.backend.begin();
        REQUIRE_FALSE(board.backend.failed());
        REQUIRE_FALSE(board.backend.formatted());
        REQUIRE(readAll(board.backend) == Data(SIZE, 0));

        const auto cycles = chip.writeCycles;
        write(board.backend, 0x10, {0, 0, 0});
        REQUIRE(chip.writeCycles == cycles);
        REQUIRE_FALSE(board.backend.formatted());
    }

    SECTION("the first write formats the chip: zeros first, the header last")
    {
        {
            Board board(chip);
            board.backend.begin();
            write(board.backend, 0x370, {1, 2, 3});
            board.backend.flush();
            REQUIRE(board.backend.formatted());
        }
        REQUIRE(Data(&chip.memory[0], &chip.memory[4]) == Data {'S', 'B', 'U', 'E'});
        REQUIRE(chip.memory[4] == UserEepromSpi::LAYOUT_VERSION);
        REQUIRE(chip.memory[6] == lowByte(SIZE));
        REQUIRE(chip.memory[7] == HIGH_BYTE(SIZE));
        REQUIRE(Data(&chip.memory[DATA + 0x370], &chip.memory[DATA + 0x373]) == Data {1, 2, 3});
        REQUIRE(chip.memory[DATA + 0x36F] == 0);
        REQUIRE(chip.memory[DATA + SIZE - 1] == 0);
        REQUIRE(chip.memory[DATA + SIZE] == 0xff); // behind the user EEPROM nothing is touched

        Board rebooted(chip);
        rebooted.backend.begin();
        REQUIRE(rebooted.backend.formatted());
        Data expected(SIZE, 0);
        expected[0x370] = 1;
        expected[0x371] = 2;
        expected[0x372] = 3;
        REQUIRE(readAll(rebooted.backend) == expected);
    }

    SECTION("formatting takes one write cycle per page")
    {
        Board board(chip);
        board.backend.begin();
        const auto cycles = chip.writeCycles;
        write(board.backend, 0, {1});
        // 25 pages of data (16 bytes header in front), the header, the byte
        REQUIRE(chip.writeCycles - cycles == 27);
    }

    SECTION("a corrupted header makes the chip read as empty")
    {
        {
            Board board(chip);
            board.backend.begin();
            write(board.backend, 0x10, {0x42});
        }
        chip.memory[8] ^= 0x01; // CRC
        Board rebooted(chip);
        rebooted.backend.begin();
        REQUIRE_FALSE(rebooted.backend.formatted());
        REQUIRE(readAll(rebooted.backend) == Data(SIZE, 0));
    }

    SECTION("a write like ETS does takes one write cycle, unchanged bytes none")
    {
        Board board(chip);
        board.backend.begin();
        write(board.backend, 0, {1});
        auto cycles = chip.writeCycles;
        write(board.backend, 0x400, {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12});
        REQUIRE(chip.writeCycles - cycles == 1);

        cycles = chip.writeCycles;
        write(board.backend, 0x400, {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12});
        REQUIRE(chip.writeCycles == cycles);
    }

    SECTION("no chip: failed, reads return zeros, writes are dropped")
    {
        chip.ignoreWriteEnable = true;
        Board board(chip);
        board.backend.begin();
        REQUIRE(board.backend.failed());
        write(board.backend, 0x10, {0x42});
        REQUIRE(readAll(board.backend) == Data(SIZE, 0));
        REQUIRE(chip.writeCycles == 0);
    }

    SECTION("random writes read back like a plain memory, also after a reset")
    {
        Data model(SIZE, 0);
        std::mt19937 random(0x25);
        auto board = std::make_unique<Board>(chip);
        board->backend.begin();
        for (int step = 0; step < 200; step++)
        {
            const uint32_t offset = random() % SIZE;
            const uint32_t length = 1 + random() % std::min<uint32_t>(16, SIZE - offset);
            Data bytes(length);
            for (auto& b : bytes)
            {
                b = static_cast<uint8_t>(random() % 3 == 0 ? 0 : random());
            }
            write(board->backend, offset, bytes);
            std::copy(bytes.begin(), bytes.end(), model.begin() + offset);
            if (step % 50 == 49)
            {
                board = std::make_unique<Board>(chip);
                board->backend.begin();
            }
            REQUIRE(readAll(board->backend) == model);
        }
        REQUIRE(chip.violations == 0);
    }
}

TEST_CASE("MASK0701 with the user EEPROM in an SPI EEPROM: power loss during a download", "[poc][spiBackend]")
{
    // Only with the SPI backend; the fixture keeps the chip over restarts
    if (!Mask0701Fixture::spiBackend())
    {
        return;
    }
    using F = Mask0701Fixture;
    SpiEepromEmulator& chip = F::spiChip();

    uint64_t downloadBytes = 0;
    {
        F f;
        const auto start = chip.busBytes;
        f.downloadTables();
        downloadBytes = chip.busBytes - start;
        REQUIRE(f.bcu->applicationRunning());
    }
    REQUIRE(downloadBytes > 0);

    // Every 7th bus byte is enough to hit each phase of every write, all bytes take too long
    const Data addressTable = F::addressTableBytes();
    uint64_t loadedAfterFailure = 0;
    for (uint64_t failAt = 0; failAt <= downloadBytes + 7; failAt += 7)
    {
        F f; // erases the chip and boots
        chip.powerFailAfterBytes(failAt);
        f.downloadTables();
        f.restart(); // powers the chip on again

        INFO("power loss at bus byte " << failAt << " of " << downloadBytes);
        REQUIRE(f.spiUserEeprom->earlyReads() == 0);
        if (f.bcu->applicationRunning())
        {
            REQUIRE(f.memoryRead(F::ADDR_TABLE, 7).data == Data(addressTable.begin(), addressTable.begin() + 7));
            loadedAfterFailure++;
        }
        REQUIRE(chip.violations == 0);
    }
    REQUIRE(loadedAfterFailure > 0);
}

TEST_CASE("MASK0701 with the user EEPROM in an SPI EEPROM: group addresses without SPI in the bus interrupt",
          "[poc][spiBackend]")
{
    // The bus interrupt calls indexOfAddr() for every group telegram; an SPI transfer there would break one of the
    // main program. With the SPI backend the group addresses are kept in RAM.
    if (!Mask0701Fixture::spiBackend())
    {
        return;
    }
    using F = Mask0701Fixture;
    SpiEepromEmulator& chip = F::spiChip();
    F f;
    AddrTables* tables = f.bcu->addrTables;
    REQUIRE(tables->indexOfAddr(F::GROUP_ADDRESS_1) == -1); // empty chip

    f.downloadTables(); // the load control of the address table refreshes the copy
    auto busBytes = chip.busBytes;
    REQUIRE(tables->indexOfAddr(F::GROUP_ADDRESS_1) == 1);
    REQUIRE(tables->indexOfAddr(F::GROUP_ADDRESS_2) == 2);
    REQUIRE(tables->indexOfAddr(F::PHANTOM_GROUP_ADDRESS) == -1);
    REQUIRE(chip.busBytes == busBytes);

    f.restart(); // begin() reads the copy
    busBytes = chip.busBytes;
    REQUIRE(tables != f.bcu->addrTables);
    REQUIRE(f.bcu->addrTables->indexOfAddr(F::GROUP_ADDRESS_2) == 2);
    REQUIRE(chip.busBytes == busBytes);
}
