/*
 * Characterization tests of BcuDefault::processApciMemoryOperation() on a MASK0701
 *
 * These tests document the current behavior, even where it looks wrong.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 */

#include "mask0701_fixture.h"

using Data = std::vector<uint8_t>;

static const Data PATTERN = {0xCA, 0xFE, 0xBA, 0xBE};

TEST_CASE("MASK0701 memory ranges", "[poc][memory]")
{
    Mask0701Fixture f;
    f.bcu->userEeprom->modified(false); // the first start on empty flash marks it as modified, see [loadstate]

    SECTION("user EEPROM, first and last byte")
    {
        for (const uint32_t address : {Mask0701Fixture::USER_EEPROM_FIRST + 0x20, Mask0701Fixture::USER_EEPROM_LAST - 3})
        {
            REQUIRE(f.memoryWrite(address, PATTERN) == false); // no response without DEVCTRL_MEM_AUTO_RESPONSE
            REQUIRE(f.bcu->userEeprom->isModified());
            REQUIRE(f.mirrorByte(address) == PATTERN[0]);
            REQUIRE(f.mirrorByte(address + 3) == PATTERN[3]);

            const auto result = f.memoryRead(address, 4);
            REQUIRE(result.count == 4);
            REQUIRE(result.data == PATTERN);
        }
    }

    SECTION("MemMapper, the mirror stays untouched")
    {
        constexpr uint32_t address = Mask0701Fixture::MAPPER_START + 0x10;
        f.memoryWrite(address, PATTERN);
        REQUIRE_FALSE(f.bcu->userEeprom->isModified());

        const auto result = f.memoryRead(address, 4);
        REQUIRE(result.count == 4);
        REQUIRE(result.data == PATTERN);
    }

    SECTION("MemMapper over a 256 byte page boundary")
    {
        constexpr uint32_t address = Mask0701Fixture::MAPPER_START + 0xFE;
        f.memoryWrite(address, PATTERN);

        const auto result = f.memoryRead(address, 4);
        REQUIRE(result.count == 4);
        REQUIRE(result.data == PATTERN);
    }

    SECTION("user RAM")
    {
        constexpr uint32_t address = Mask0701Fixture::RAM_START + 0x10;
        f.memoryWrite(address, PATTERN);
        REQUIRE_FALSE(f.bcu->userEeprom->isModified());
        REQUIRE(f.bcu->userRam->userRamData[0x10] == PATTERN[0]);

        const auto result = f.memoryRead(address, 4);
        REQUIRE(result.count == 4);
        REQUIRE(result.data == PATTERN);
    }

    SECTION("status address outside of the user RAM range")
    {
        const auto result = f.memoryRead(0x60, 1);
        REQUIRE(result.count == 1);
        REQUIRE(result.data[0] == f.bcu->userRam->status());
    }

    SECTION("unknown address")
    {
        REQUIRE(f.memoryRead(0x2000, 4).count == 0);
        f.memoryWrite(0x2000, PATTERN);
        REQUIRE_FALSE(f.bcu->userEeprom->isModified());
    }
}

TEST_CASE("MASK0701 memory lengths", "[poc][memory]")
{
    Mask0701Fixture f;
    uint8_t buffer[16] = {};

    SECTION("length zero is an error")
    {
        REQUIRE_FALSE(f.bcu->processApciMemoryOperation(Mask0701Fixture::USER_EEPROM_FIRST, buffer, 0, true));
        REQUIRE_FALSE(f.bcu->processApciMemoryOperation(Mask0701Fixture::USER_EEPROM_FIRST, buffer, 0, false));
        REQUIRE(f.memoryRead(Mask0701Fixture::USER_EEPROM_FIRST, 0).count == 0);
    }

    SECTION("address overflow is an error")
    {
        REQUIRE_FALSE(f.bcu->processApciMemoryOperation(0xFFFFFFFF, buffer, 2, true));
    }

    SECTION("the response repeats 15 bytes, the longest count of a telegram")
    {
        const auto result = f.memoryRead(Mask0701Fixture::USER_EEPROM_FIRST, 15);
        REQUIRE(result.count == 15);
    }
}

TEST_CASE("MASK0701 memory operations over range boundaries", "[poc][memory]")
{
    SECTION("end of the user EEPROM into unmapped memory: all or nothing on read, partly on write")
    {
        Mask0701Fixture f;
        constexpr uint32_t address = Mask0701Fixture::USER_EEPROM_LAST - 1;
        f.bcu->userEeprom->modified(false);

        REQUIRE(f.memoryRead(address, 4).count == 0);

        f.memoryWrite(address, PATTERN);
        REQUIRE(f.mirrorByte(address) == PATTERN[0]);
        REQUIRE(f.mirrorByte(address + 1) == PATTERN[1]);
        REQUIRE(f.bcu->userEeprom->isModified()); // partly written and marked as modified, although the operation failed
    }

    SECTION("end of the user RAM into unmapped memory: partly on write")
    {
        Mask0701Fixture f;
        constexpr uint32_t address = Mask0701Fixture::RAM_END - 1;

        REQUIRE(f.memoryRead(address, 4).count == 0);

        f.memoryWrite(address, PATTERN);
        REQUIRE(f.bcu->userRam->userRamData[Mask0701Fixture::RAM_END - Mask0701Fixture::RAM_START] == PATTERN[1]);
    }

    SECTION("end of the MemMapper into unmapped memory: byte by byte, partly on write")
    {
        Mask0701Fixture f;
        constexpr uint32_t address = Mask0701Fixture::MAPPER_END - 1;

        REQUIRE(f.memoryRead(address, 4).count == 0);

        f.memoryWrite(address, PATTERN);
        const auto result = f.memoryRead(address, 2);
        REQUIRE(result.count == 2);
        REQUIRE(result.data == Data {PATTERN[0], PATTERN[1]});
    }

    SECTION("user EEPROM into a MemMapper range")
    {
        Mask0701Fixture f({{Mask0701Fixture::USER_EEPROM_LAST + 1, 0x100}});
        constexpr uint32_t address = Mask0701Fixture::USER_EEPROM_LAST - 1;

        f.memoryWrite(address, PATTERN);
        REQUIRE(f.mirrorByte(address) == PATTERN[0]);

        const auto result = f.memoryRead(address, 4);
        REQUIRE(result.count == 4);
        REQUIRE(result.data == PATTERN);
    }

    SECTION("MemMapper range into the user EEPROM")
    {
        Mask0701Fixture f({{Mask0701Fixture::USER_EEPROM_FIRST - 0x100, 0x100}});
        constexpr uint32_t address = Mask0701Fixture::USER_EEPROM_FIRST - 2;

        f.memoryWrite(address, PATTERN);
        REQUIRE(f.mirrorByte(Mask0701Fixture::USER_EEPROM_FIRST) == PATTERN[2]);

        const auto result = f.memoryRead(address, 4);
        REQUIRE(result.count == 4);
        REQUIRE(result.data == PATTERN);
    }

    SECTION("a MemMapper range hides the user EEPROM it overlaps")
    {
        Mask0701Fixture f({{Mask0701Fixture::USER_EEPROM_FIRST + 0x100, 0x100}});
        constexpr uint32_t address = Mask0701Fixture::USER_EEPROM_FIRST + 0x100;

        f.memoryWrite(address, PATTERN);
        REQUIRE(f.mirrorByte(address) == 0);
        REQUIRE(f.memoryRead(address, 4).data == PATTERN);
    }
}

TEST_CASE("MASK0701 memory write with auto response", "[poc][memory]")
{
    Mask0701Fixture f;
    f.bcu->userRam->deviceControl() = DEVCTRL_MEM_AUTO_RESPONSE;

    SECTION("successful write is answered")
    {
        REQUIRE(f.memoryWrite(Mask0701Fixture::USER_EEPROM_FIRST + 0x20, PATTERN));
    }

    SECTION("failed write stays without response")
    {
        REQUIRE_FALSE(f.memoryWrite(0x2000, PATTERN));
    }
}

TEST_CASE("MASK0701 load control and load state addresses", "[poc][memory]")
{
    Mask0701Fixture f;

    SECTION("load state address reads the load state bytes")
    {
        constexpr uint32_t LOAD_STATE_ADDRESS = 0xB6E9;
        for (uint8_t i = 0; i < INTERFACE_OBJECT_COUNT; i++)
        {
            f.bcu->userEeprom->loadState()[i] = static_cast<uint8_t>(0x10 + i);
        }
        for (uint8_t i = 0; i < INTERFACE_OBJECT_COUNT; i++)
        {
            REQUIRE(f.memoryRead(LOAD_STATE_ADDRESS + i, 1).data == Data {static_cast<uint8_t>(0x10 + i)});
        }
        REQUIRE(f.memoryRead(LOAD_STATE_ADDRESS + INTERFACE_OBJECT_COUNT, 1).count == 0);

        // length is not limited, so this reads behind the load states into the table pointers
        f.bcu->userEeprom->addrTabAddr() = 0x4000;
        const auto result = f.memoryRead(LOAD_STATE_ADDRESS + 7, 3);
        REQUIRE(result.count == 3);
        REQUIRE(result.data == Data {0x17, 0x00, 0x40});
    }

    SECTION("load state address can't be written")
    {
        f.memoryWrite(0xB6E9, {LS_LOADED});
        REQUIRE(f.bcu->userEeprom->loadState()[0] == LS_UNLOADED);
    }

    SECTION("load control of an unknown object is refused")
    {
        f.bcu->userRam->deviceControl() = DEVCTRL_MEM_AUTO_RESPONSE;
        REQUIRE(f.loadControlMem(INTERFACE_OBJECT_COUNT, LC_START_LOADING) == false);
        REQUIRE(f.loadControlMem(OT_ADDR_TABLE, LC_START_LOADING) == true);
    }
}
