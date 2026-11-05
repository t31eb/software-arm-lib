/*
 * Tests of ComObjectLayout on a MASK0701: the application provides type and value address of com objects
 * without writing into the com object table.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 */

#include "mask0701_fixture.h"

#include <sblib/eib/apci.h>

#include <vector>

using F = Mask0701Fixture;

namespace
{
constexpr uint16_t OBJECT_2_VALUE = F::OBJECT_RAM + 0x20; //!< somewhere else in the user RAM
constexpr uint16_t FLAGS = F::FLAGS_TABLE + 0x10;

/** Knows object 2 only: 1 byte instead of the 2 bytes of the table */
class TestLayout : public ComObjectLayout
{
public:
    bool object(const int objno, uint8_t& type, uint16_t& valueAddress) override
    {
        if (objno != 2)
        {
            return false;
        }
        type = BYTE_1;
        valueAddress = OBJECT_2_VALUE;
        return true;
    }

    uint16_t flagsTableAddress() override { return flagsAddress; }

    uint16_t flagsAddress = 0;
};

std::vector<uint8_t> comTable(const F& f)
{
    std::vector<uint8_t> bytes;
    for (uint32_t a = F::COM_TABLE; a < F::COM_TABLE + F::comTableBytes().size(); a++)
    {
        bytes.push_back(f.mirrorByte(a));
    }
    return bytes;
}

uint8_t* ram(const F& f, const uint16_t address)
{
    return &f.bcu->userRam->userRamData[address - F::RAM_START];
}
}

TEST_CASE("MASK0701 com object layout of the application", "[poc][comobjects]")
{
    F f;
    f.downloadTables();
    f.bcu->userEeprom->modified(false);
    ComObjects* com = f.bcu->comObjects;
    TestLayout layout;
    com->setObjectLayout(&layout);
    const auto table = comTable(f);

    SECTION("the layout sets type and value address, the flags stay from the table")
    {
        REQUIRE(com->objectConfig(2).type == BYTE_1);
        REQUIRE(com->objectConfig(2).config == f.mirrorByte(F::COM_TABLE + 3 + 2 * 4 + 2));
        REQUIRE(com->objectValuePtr(2) == ram(f, OBJECT_2_VALUE));
        REQUIRE(com->objectSize(2) == 1);
    }

    SECTION("objects the layout doesn't know come from the table")
    {
        REQUIRE(com->objectConfig(3).type == BYTE_2);
        REQUIRE(com->objectValuePtr(3) == ram(f, F::OBJECT_RAM + 4));
    }

    SECTION("values are read and written at the address of the layout")
    {
        com->objectWrite(2, 0x5A);
        REQUIRE(*ram(f, OBJECT_2_VALUE) == 0x5A);
        REQUIRE(com->objectRead(2) == 0x5A);
    }

    SECTION("a group write ends at the address of the layout, with the size of the layout")
    {
        uint8_t telegram[] = {0xBC, 0x11, 0x01, 0x08, 0x02, 0xE3, 0x00, 0x80, 0x12, 0x34};
        com->processGroupTelegram(F::GROUP_ADDRESS_2, APCI_GROUP_VALUE_WRITE_PDU, telegram);
        REQUIRE(*ram(f, OBJECT_2_VALUE) == 0x12);
        REQUIRE(com->objectRead(2) == 0x12);
    }

    SECTION("the flags table can be moved")
    {
        REQUIRE(com->objectFlagsTable() == ram(f, F::FLAGS_TABLE)); // 0: from the table
        layout.flagsAddress = FLAGS;
        REQUIRE(com->objectFlagsTable() == ram(f, FLAGS));
    }

    SECTION("nothing is written into the com object table")
    {
        com->objectWrite(2, 0x5A);
        layout.flagsAddress = FLAGS;
        com->objectWrite(1, 0x11);
        REQUIRE(comTable(f) == table);
        REQUIRE_FALSE(f.bcu->userEeprom->isModified());
    }

    com->setObjectLayout(nullptr);
}
