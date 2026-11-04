/*
 * Characterization tests of ComObjectsBCU1::processGroupTelegram() on a MASK0701
 *
 * These tests document the current behavior, even where it looks wrong.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 */

#include "mask0701_fixture.h"

#include <sblib/eib/apci.h>
#include <sblib/eib/knx_lpdu.h>

#include <vector>

using F = Mask0701Fixture;
using Data = std::vector<uint8_t>;

namespace
{
/**
 * A group telegram with two data bytes, as the bus layer hands it to the com objects.
 * @p shortValue is the 6 bit value in the APCI byte, used by objects of up to 6 bits.
 */
Data groupTelegram(const uint16_t groupAddress, const uint16_t apci, const uint8_t data0, const uint8_t data1,
                   const uint8_t shortValue)
{
    return {0xBC, 0x11, 0x01, HIGH_BYTE(groupAddress), lowByte(groupAddress), 0xE3,
            static_cast<uint8_t>(apci >> 8), static_cast<uint8_t>((apci & 0xC0) | (shortValue & 0x3F)), data0, data1};
}

/** Address of the config byte of a com object in the fixture's com object table */
uint32_t configAddress(const int objno)
{
    return F::COM_TABLE + 3 + objno * 4 + 2;
}

uint8_t flags(const F& f, const int objno)
{
    const uint8_t both = f.bcu->comObjects->objectFlagsTable()[objno >> 1];
    return (objno & 1) ? (both >> 4) : (both & 0x0F);
}

/** Values and flags of all four com objects */
std::vector<unsigned int> state(const F& f)
{
    std::vector<unsigned int> result;
    for (int objno = 0; objno < 4; objno++)
    {
        result.push_back(f.bcu->comObjects->objectRead(objno));
        result.push_back(flags(f, objno));
    }
    return result;
}

/** Hands a group telegram to the com objects like BcuDefault does; @p apci without data bits */
void receive(const F& f, const uint16_t groupAddress, const uint16_t apci, const uint8_t shortValue = 0)
{
    Data telegram = groupTelegram(groupAddress, apci, 0x12, 0x34, shortValue);
    f.bcu->comObjects->processGroupTelegram(groupAddress, apci, telegram.data());
}
}

TEST_CASE("MASK0701 receiving of group telegrams", "[poc][comobjects]")
{
    F f;
    f.downloadTables();
    REQUIRE(f.bcu->applicationRunning());
    ComObjects* com = f.bcu->comObjects;
    const auto before = state(f);

    SECTION("a group write updates every associated object with its own size")
    {
        receive(f, F::GROUP_ADDRESS_1, APCI_GROUP_VALUE_WRITE_PDU);
        REQUIRE(com->objectRead(1) == 0x12);   // 1 byte object takes the first data byte
        REQUIRE(com->objectRead(3) == 0x1234); // 2 byte object
        REQUIRE(flags(f, 1) == COMFLAG_UPDATE);
        REQUIRE(flags(f, 3) == COMFLAG_UPDATE);
        REQUIRE(com->objectRead(2) == 0);
        REQUIRE(flags(f, 2) == 0);
    }

    SECTION("a group write to the second group address updates only its object")
    {
        receive(f, F::GROUP_ADDRESS_2, APCI_GROUP_VALUE_WRITE_PDU);
        REQUIRE(com->objectRead(2) == 0x1234);
        REQUIRE(flags(f, 2) == COMFLAG_UPDATE);
        REQUIRE(com->objectRead(1) == 0);
        REQUIRE(com->objectRead(3) == 0);
    }

    SECTION("a 1 bit object takes the value from the APCI byte")
    {
        f.memoryWrite(F::ASSOC_TABLE + 3, {2, 0}); // second association: group address 2 -> object 0 (1 bit)
        receive(f, F::GROUP_ADDRESS_2, APCI_GROUP_VALUE_WRITE_PDU, 0x01);
        REQUIRE(com->objectRead(0) == 1);
        REQUIRE(flags(f, 0) == COMFLAG_UPDATE);
    }

    SECTION("unknown group addresses change nothing")
    {
        receive(f, 0x0803, APCI_GROUP_VALUE_WRITE_PDU);
        REQUIRE(state(f) == before);
    }

    SECTION("the group address behind the end of the address table is not found")
    {
        receive(f, F::PHANTOM_GROUP_ADDRESS, APCI_GROUP_VALUE_WRITE_PDU);
        REQUIRE(state(f) == before);
    }

    SECTION("the physical address in the first entry of the address table is no group address")
    {
        receive(f, F::OWN_ADDRESS, APCI_GROUP_VALUE_WRITE_PDU);
        REQUIRE(state(f) == before);
    }

    SECTION("an object without write enable ignores a group write")
    {
        f.memoryWrite(configAddress(2), {COMCONF_COMM | COMCONF_READ | COMCONF_TRANS | COMCONF_UPDATE});
        receive(f, F::GROUP_ADDRESS_2, APCI_GROUP_VALUE_WRITE_PDU);
        REQUIRE(state(f) == before);
    }

    SECTION("an object without communication enable ignores everything")
    {
        f.memoryWrite(configAddress(2), {COMCONF_READ | COMCONF_WRITE | COMCONF_TRANS | COMCONF_UPDATE});
        receive(f, F::GROUP_ADDRESS_2, APCI_GROUP_VALUE_WRITE_PDU);
        receive(f, F::GROUP_ADDRESS_2, APCI_GROUP_VALUE_RESPONSE_PDU);
        REQUIRE(state(f) == before);
    }

    SECTION("a group response updates an object with update enable")
    {
        receive(f, F::GROUP_ADDRESS_2, APCI_GROUP_VALUE_RESPONSE_PDU);
        REQUIRE(com->objectRead(2) == 0x1234);
        REQUIRE(flags(f, 2) == COMFLAG_UPDATE);
    }

    SECTION("a group response is ignored without update enable, even with write enable")
    {
        f.memoryWrite(configAddress(2), {COMCONF_COMM | COMCONF_READ | COMCONF_WRITE | COMCONF_TRANS});
        receive(f, F::GROUP_ADDRESS_2, APCI_GROUP_VALUE_RESPONSE_PDU);
        REQUIRE(state(f) == before);
    }

    SECTION("an empty association table finds nothing")
    {
        f.memoryWrite(F::ASSOC_TABLE, {0});
        receive(f, F::GROUP_ADDRESS_1, APCI_GROUP_VALUE_WRITE_PDU);
        REQUIRE(state(f) == before);
    }

    SECTION("the association table is scanned to its end, also behind a match")
    {
        // group address 1 is associated with objects 1 (entry 1) and 3 (entry 3), see the section above;
        // here the order is reversed, both objects are still updated
        f.memoryWrite(F::ASSOC_TABLE + 1, {1, 3, 2, 2, 1, 1});
        receive(f, F::GROUP_ADDRESS_1, APCI_GROUP_VALUE_WRITE_PDU);
        REQUIRE(com->objectRead(1) == 0x12);
        REQUIRE(com->objectRead(3) == 0x1234);
    }
}
