/*
 * Tests for the access methods of class Memory on the user EEPROM and the user RAM of a MASK0701
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 */

#include "mask0701_fixture.h"

#include <algorithm>

using F = Mask0701Fixture;
using Data = std::vector<uint8_t>;

namespace
{
constexpr uint32_t EEPROM_ADDRESS = 0x4101; // odd on purpose, read() and write() have no alignment restrictions
constexpr uint32_t RAM_ADDRESS = F::RAM_START + 0x101;
const Data BYTES = {0x12, 0x34, 0x56, 0x78};

template <typename M>
void checkTypedAccess(M& memory, const uint32_t address)
{
    Data buffer(4);

    REQUIRE(memory.writeBlock(address, BYTES.data(), 4));

    REQUIRE(memory.template read<uint8_t>(address, ByteOrder::littleEndian) == 0x12);
    REQUIRE(memory.template read<uint8_t>(address, ByteOrder::bigEndian) == 0x12);
    REQUIRE(memory.template read<uint16_t>(address, ByteOrder::littleEndian) == 0x3412);
    REQUIRE(memory.template read<uint16_t>(address, ByteOrder::bigEndian) == 0x1234);
    REQUIRE(memory.template read<uint32_t>(address, ByteOrder::littleEndian) == 0x78563412);
    REQUIRE(memory.template read<uint32_t>(address, ByteOrder::bigEndian) == 0x12345678);

    REQUIRE(memory.template write<uint32_t>(address, 0xA1B2C3D4, ByteOrder::bigEndian));
    REQUIRE(memory.readBlock(address, buffer.data(), 4));
    REQUIRE(buffer == Data {0xA1, 0xB2, 0xC3, 0xD4});

    REQUIRE(memory.template write<uint32_t>(address, 0xA1B2C3D4, ByteOrder::littleEndian));
    REQUIRE(memory.readBlock(address, buffer.data(), 4));
    REQUIRE(buffer == Data {0xD4, 0xC3, 0xB2, 0xA1});

    REQUIRE(memory.template write<uint16_t>(address, 0xE5F6, ByteOrder::bigEndian));
    REQUIRE(memory.readBlock(address, buffer.data(), 2));
    REQUIRE(buffer[0] == 0xE5);
    REQUIRE(buffer[1] == 0xF6);

    REQUIRE(memory.template write<uint16_t>(address, 0xE5F6, ByteOrder::littleEndian));
    REQUIRE(memory.readBlock(address, buffer.data(), 2));
    REQUIRE(buffer[0] == 0xF6);
    REQUIRE(buffer[1] == 0xE5);

    REQUIRE(memory.template write<uint8_t>(address, 0x7A, ByteOrder::bigEndian));
    REQUIRE(memory.readBlock(address, buffer.data(), 1));
    REQUIRE(buffer[0] == 0x7A);
}

template <typename M>
void checkLimits(M& memory, const uint32_t first, const uint32_t last)
{
    const Data pattern = {0xA5, 0xA5, 0xA5, 0xA5};
    Data buffer = {0x11, 0x22, 0x33, 0x44};
    const Data untouched = buffer;

    SECTION("first and last byte")
    {
        REQUIRE(memory.writeBlock(first, pattern.data(), 4));
        REQUIRE(memory.readBlock(first, buffer.data(), 4));
        REQUIRE(buffer == pattern);
        REQUIRE(memory.writeBlock(last - 3, pattern.data(), 4));
        REQUIRE(memory.readBlock(last - 3, buffer.data(), 4));
        REQUIRE(buffer == pattern);
    }

    SECTION("a block over the limits fails without touching anything")
    {
        Data before(2);
        REQUIRE(memory.readBlock(last - 1, before.data(), 2));

        REQUIRE_FALSE(memory.readBlock(last - 1, buffer.data(), 4));
        REQUIRE_FALSE(memory.readBlock(first - 1, buffer.data(), 4));
        REQUIRE(buffer == untouched);

        REQUIRE_FALSE(memory.writeBlock(last - 1, pattern.data(), 4));
        REQUIRE_FALSE(memory.writeBlock(first - 1, pattern.data(), 4));

        Data after(2);
        REQUIRE(memory.readBlock(last - 1, after.data(), 2));
        REQUIRE(after == before);
    }

    SECTION("length zero does nothing, also outside of the memory")
    {
        REQUIRE(memory.readBlock(last + 1, buffer.data(), 0));
        REQUIRE(memory.writeBlock(last + 1, pattern.data(), 0));
        REQUIRE(buffer == untouched);
    }

    SECTION("address overflow")
    {
        REQUIRE_FALSE(memory.readBlock(0xFFFFFFFF, buffer.data(), 2));
        REQUIRE_FALSE(memory.writeBlock(0xFFFFFFFF, pattern.data(), 2));
    }

    SECTION("typed access outside of the memory")
    {
        REQUIRE(memory.template read<uint32_t>(last - 1, ByteOrder::bigEndian) == 0);
        REQUIRE_FALSE(memory.template write<uint32_t>(last - 1, 1, ByteOrder::bigEndian));
    }
}
}

TEST_CASE("Memory access methods on the user EEPROM", "[poc][memory]")
{
    F f;
    UserEeprom& eeprom = *f.bcu->userEeprom;

    SECTION("typed access")
    {
        checkTypedAccess(eeprom, EEPROM_ADDRESS);
    }

    SECTION("limits")
    {
        checkLimits(eeprom, F::USER_EEPROM_FIRST, F::USER_EEPROM_LAST);
    }

    SECTION("read is possible over a const reference")
    {
        const UserEeprom& constEeprom = eeprom;
        REQUIRE(eeprom.write<uint8_t>(EEPROM_ADDRESS, 0x5A, ByteOrder::littleEndian));
        REQUIRE(constEeprom.read<uint8_t>(EEPROM_ADDRESS, ByteOrder::littleEndian) == 0x5A);
    }

    SECTION("only a successful write with data marks the mirror as modified")
    {
        eeprom.modified(false);
        const uint8_t value = 0x42;

        REQUIRE(eeprom.writeBlock(EEPROM_ADDRESS, &value, 0));
        REQUIRE_FALSE(eeprom.isModified());
        REQUIRE_FALSE(eeprom.writeBlock(F::USER_EEPROM_LAST + 1, &value, 1));
        REQUIRE_FALSE(eeprom.isModified());

        Data buffer(1);
        REQUIRE(eeprom.readBlock(EEPROM_ADDRESS, buffer.data(), 1));
        REQUIRE_FALSE(eeprom.isModified());

        REQUIRE(eeprom.writeBlock(EEPROM_ADDRESS, &value, 1));
        REQUIRE(eeprom.isModified());
    }

    SECTION("a block written over the new method survives a restart")
    {
        REQUIRE(eeprom.writeBlock(EEPROM_ADDRESS, BYTES.data(), 4));
        REQUIRE(f.bcu->flushUserMemory(UsrCallbackType::flash));
        f.restart();

        Data buffer(4);
        REQUIRE(f.bcu->userEeprom->readBlock(EEPROM_ADDRESS, buffer.data(), 4));
        REQUIRE(buffer == BYTES);
    }
}

TEST_CASE("Memory access methods on the user RAM", "[poc][memory]")
{
    F f;
    UserRam& ram = *f.bcu->userRam;

    SECTION("typed access")
    {
        checkTypedAccess(ram, RAM_ADDRESS);
    }

    SECTION("limits")
    {
        checkLimits(ram, F::RAM_START, F::RAM_END);
    }

    SECTION("the status address works, also behind the user RAM range")
    {
        ram.status() = 0x5A;
        REQUIRE(ram.read<uint8_t>(0x60, ByteOrder::littleEndian) == 0x5A);
        REQUIRE(ram.write<uint8_t>(0x60, 0xA5, ByteOrder::littleEndian));
        REQUIRE(ram.status() == 0xA5);
        REQUIRE(ram.read<uint16_t>(0x60, ByteOrder::littleEndian) == 0);
    }
}

TEST_CASE("Memory access methods agree with the existing access paths", "[poc][memory]")
{
    F f;
    f.downloadTables();
    UserEeprom& eeprom = *f.bcu->userEeprom;

    SECTION("table pointers are little endian")
    {
        REQUIRE(eeprom.read<uint16_t>(F::ADDR_TAB_ADDR, ByteOrder::littleEndian) == f.bcu->userEeprom->addrTabAddr());
        REQUIRE(eeprom.read<uint16_t>(F::ASSOC_TAB_ADDR, ByteOrder::littleEndian) == f.bcu->userEeprom->assocTabAddr());
        REQUIRE(eeprom.read<uint16_t>(F::COMMS_TAB_ADDR, ByteOrder::littleEndian) == f.bcu->userEeprom->commsTabAddr());
    }

    SECTION("pointers inside of the com object table are big endian")
    {
        REQUIRE(eeprom.read<uint16_t>(F::COM_TABLE + 1, ByteOrder::bigEndian) == F::FLAGS_TABLE);
        REQUIRE(eeprom.read<uint16_t>(F::COM_TABLE + 3, ByteOrder::bigEndian) == F::OBJECT_RAM);
    }

    SECTION("getUInt8() and getUInt16() are the same as read()")
    {
        for (const uint32_t address : {F::USER_EEPROM_FIRST, uint32_t {F::ADDR_TABLE}, F::USER_EEPROM_LAST - 1})
        {
            REQUIRE(eeprom.read<uint8_t>(address, ByteOrder::bigEndian) == eeprom.getUInt8(address));
            REQUIRE(eeprom.read<uint16_t>(address, ByteOrder::bigEndian) == eeprom.getUInt16(address));
        }
    }

    SECTION("the whole user EEPROM reads the same as with A_Memory_Read")
    {
        for (uint32_t address = F::USER_EEPROM_FIRST; address + 12 <= F::USER_EEPROM_LAST; address += 12)
        {
            Data buffer(12);
            REQUIRE(eeprom.readBlock(address, buffer.data(), 12));
            REQUIRE(f.memoryRead(address, 12).data == buffer);
        }
    }

    SECTION("the whole user RAM behaves the same as with A_Memory_Read/Write, including the shadowed status")
    {
        UserRam& ram = *f.bcu->userRam;
        Data bytes(0x304);
        std::generate(bytes.begin(), bytes.end(), [i = 0]() mutable { return static_cast<uint8_t>(++i * 3); });

        // the same writes over both ways on two devices have to leave the same state
        F other;
        UserRam& otherRam = *other.bcu->userRam;
        for (uint32_t pos = 0; pos + 12 <= bytes.size(); pos += 12)
        {
            const Data part(bytes.begin() + pos, bytes.begin() + pos + 12);
            REQUIRE(ram.writeBlock(F::RAM_START + pos, part.data(), 12));
            other.memoryWrite(F::RAM_START + pos, part);
        }
        REQUIRE(ram.status() == otherRam.status());
        REQUIRE(std::equal(ram.userRamData, ram.userRamData + bytes.size(), otherRam.userRamData));

        for (uint32_t pos = 0; pos + 12 <= bytes.size(); pos += 12)
        {
            Data buffer(12);
            REQUIRE(ram.readBlock(F::RAM_START + pos, buffer.data(), 12));
            REQUIRE(other.memoryRead(F::RAM_START + pos, 12).data == buffer);
        }
    }
}
