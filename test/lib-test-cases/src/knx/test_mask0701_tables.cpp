/*
 * Characterization tests of the address, association and com object table on a MASK0701
 *
 * These tests document the current behavior, even where it looks wrong.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 */

#include "mask0701_fixture.h"

using F = Mask0701Fixture;
using Data = std::vector<uint8_t>;

TEST_CASE("MASK0701 tables after a download", "[poc][tables]")
{
    F f;
    f.downloadTables();
    ComObjects* com = f.bcu->comObjects;
    AddrTables* tables = f.bcu->addrTables;

    SECTION("table pointers are stored little endian in the mirror")
    {
        REQUIRE(f.memoryRead(F::ADDR_TAB_ADDR, 2).data == Data {lowByte(F::ADDR_TABLE), HIGH_BYTE(F::ADDR_TABLE)});
        REQUIRE(f.memoryRead(F::ASSOC_TAB_ADDR, 2).data == Data {lowByte(F::ASSOC_TABLE), HIGH_BYTE(F::ASSOC_TABLE)});
        REQUIRE(f.memoryRead(F::COMMS_TAB_ADDR, 2).data == Data {lowByte(F::COM_TABLE), HIGH_BYTE(F::COM_TABLE)});
        REQUIRE(f.bcu->userEeprom->addrTabAddr() == F::ADDR_TABLE); // host and LPC are both little endian
    }

    SECTION("tables point into the mirror")
    {
        REQUIRE(tables->addrTable() == &f.bcu->userEeprom->userEepromData[F::ADDR_TABLE - F::USER_EEPROM_FIRST]);
        REQUIRE(tables->assocTable() == &f.bcu->userEeprom->userEepromData[F::ASSOC_TABLE - F::USER_EEPROM_FIRST]);
        REQUIRE(com->objectConfigTable() == &f.bcu->userEeprom->userEepromData[F::COM_TABLE - F::USER_EEPROM_FIRST]);
    }

    SECTION("group addresses are big endian, position 1 is the first group address")
    {
        REQUIRE(tables->indexOfAddr(F::GROUP_ADDRESS_1) == 1);
        REQUIRE(tables->indexOfAddr(F::GROUP_ADDRESS_2) == 2);
        REQUIRE(tables->indexOfAddr(0x1234) == -1);
        REQUIRE(tables->indexOfAddr(0x0108) == -1); // GROUP_ADDRESS_1 in little endian
    }

    SECTION("indexOfAddr() stops at the end of the table, the count includes the physical address")
    {
        REQUIRE(tables->indexOfAddr(F::PHANTOM_GROUP_ADDRESS) == -1);
    }

    SECTION("addrCount() is the first byte of the table")
    {
        REQUIRE(tables->addrCount() == 3);
    }

    SECTION("firstObjectAddr() uses the association table")
    {
        REQUIRE(com->firstObjectAddr(1) == F::GROUP_ADDRESS_1);
        REQUIRE(com->firstObjectAddr(2) == F::GROUP_ADDRESS_2);
        REQUIRE(com->firstObjectAddr(3) == F::GROUP_ADDRESS_1);
        REQUIRE(com->firstObjectAddr(0) == 0); // no association
    }

    SECTION("com object configuration")
    {
        REQUIRE(com->objectCount() == 4);
        REQUIRE(com->objectConfig(2).type == BYTE_2);
        REQUIRE((com->objectConfig(2).config & COMCONF_COMM) == COMCONF_COMM);
        REQUIRE(com->objectType(0) == BIT_1);
        REQUIRE(com->objectSize(0) == 1);
        REQUIRE(com->objectSize(2) == 2);
    }

    SECTION("le_ptr is fixed to big endian, flags and value pointers are read high byte first")
    {
        REQUIRE(com->le_ptr == BIG_ENDIAN);
        REQUIRE(com->objectFlagsTable() == &f.bcu->userRam->userRamData[F::FLAGS_TABLE - F::RAM_START]);
        REQUIRE(com->objectValuePtr(1) == &f.bcu->userRam->userRamData[F::OBJECT_RAM + 1 - F::RAM_START]);
    }

    SECTION("le_ptr changes only flags and value pointers, not the table pointers")
    {
        com->le_ptr = LITTLE_ENDIAN;
        REQUIRE(com->objectValuePtr(1) == nullptr);  // 0x0107 reversed is 0x0107 -> 0x0107 is no mapped address
        REQUIRE(com->objectFlagsTable() == nullptr); // 0xc506 is no mapped address
        REQUIRE(com->objectCount() == 4);
        REQUIRE(tables->indexOfAddr(F::GROUP_ADDRESS_1) == 1);
    }

    SECTION("a group write ends in the RAM value, the configuration is read from the mirror")
    {
        uint8_t telegram[] = {0xBC, 0x11, 0x01, 0x08, 0x02, 0xE3, 0x00, 0x80, 0x12, 0x34};
        com->processGroupTelegram(F::GROUP_ADDRESS_2, APCI_GROUP_VALUE_WRITE_PDU, telegram);
        REQUIRE(com->objectRead(2) == 0x1234);
        REQUIRE(com->nextUpdatedObject() == 2);
        REQUIRE(com->nextUpdatedObject() == INVALID_OBJECT_NUMBER);
    }
}

TEST_CASE("MASK0701 load states inside the table area (issue #112)", "[poc][tables]")
{
    F f;

    SECTION("system bytes are in the middle of the area ETS uses for tables")
    {
        REQUIRE(F::LOAD_STATE == 0x4270);
        REQUIRE(F::COMMS_TAB_ADDR == 0x427C);
        REQUIRE(reinterpret_cast<uint8_t*>(f.bcu->userEeprom->loadState()) ==
                &f.bcu->userEeprom->userEepromData[0x370]);
    }

    SECTION("a table with 240 group addresses and its association table overwrite load states and pointers")
    {
        f.downloadTables();

        // 240 entries with 2 bytes each behind an association table starting at 0x41E3
        const Data table(1 + 2 * 240, 0xA5);
        f.memoryWriteChunked(0x41E3, table);

        REQUIRE(f.bcu->userEeprom->loadState()[OT_ADDR_TABLE] == 0xA5);
        REQUIRE(f.bcu->userEeprom->addrTabAddr() == 0xA5A5);
        REQUIRE(f.bcu->addrTables->addrTable() == nullptr);
        REQUIRE_FALSE(f.bcu->applicationRunning());
    }
}

TEST_CASE("SYSTEMB address table has a big endian word as length", "[poc][tables]")
{
    IAP_Init_Flash(0xff);
    SYSTEMB bcu;

    constexpr uint16_t ADDR_TABLE_ADDRESS = 0x3400;
    uint8_t* table = &bcu.userEeprom->userEepromData[ADDR_TABLE_ADDRESS - bcu.userEeprom->startAddr()];
    bcu.userEeprom->addrTabAddr() = ADDR_TABLE_ADDRESS;
    table[0] = 0x01;
    table[1] = 0x02;
    REQUIRE(bcu.addrTables->addrCount() == 0x0102);
}
