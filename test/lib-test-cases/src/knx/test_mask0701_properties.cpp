/*
 * Characterization tests of the two ways to the same bytes on a MASK0701:
 * A_Memory_Read/Write and A_PropertyValue_Read/Write
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

namespace
{
struct PropertyCase
{
    uint8_t objectIdx;
    PropertyID id;
    uint32_t address;  //!< virtual address of the property value
    Data memory;       //!< bytes in memory
    Data onTheWire;    //!< the same value in a A_PropertyValue_Response
};

const std::vector<PropertyCase> propertyCases = {
    // PDT_UNSIGNED_INT is reversed, memory is little endian and the telegram big endian
    {OT_DEVICE, PID_SERVICE_CONTROL, F::USER_EEPROM_FIRST + 0x386, {0x34, 0x12}, {0x12, 0x34}},
    {OT_ADDR_TABLE, PID_TABLE_REFERENCE, F::ADDR_TAB_ADDR, {0x00, 0x40}, {0x40, 0x00}},
    {OT_ASSOC_TABLE, PID_TABLE_REFERENCE, F::ASSOC_TAB_ADDR, {0x10, 0x40}, {0x40, 0x10}},
    {OT_APPLICATION, PID_TABLE_REFERENCE, F::COMMS_TAB_ADDR, {0x20, 0x40}, {0x40, 0x20}},
    // PDT_GENERIC_xx is copied byte by byte
    {OT_DEVICE, PID_HARDWARE_TYPE, F::USER_EEPROM_FIRST + 0x390, {1, 2, 3, 4, 5, 6}, {1, 2, 3, 4, 5, 6}},
    {OT_ADDR_TABLE, PID_MCB_TABLE, F::USER_EEPROM_FIRST + 0x3A4, {1, 2, 3, 4, 5, 6, 7, 8}, {1, 2, 3, 4, 5, 6, 7, 8}},
    {OT_ASSOC_TABLE, PID_MCB_TABLE, F::USER_EEPROM_FIRST + 0x3AC, {1, 2, 3, 4, 5, 6, 7, 8}, {1, 2, 3, 4, 5, 6, 7, 8}},
    {OT_APPLICATION, PID_MCB_TABLE, F::USER_EEPROM_FIRST + 0x3B4, {1, 2, 3, 4, 5, 6, 7, 8}, {1, 2, 3, 4, 5, 6, 7, 8}},
};
}

TEST_CASE("MASK0701 properties and memory access the same bytes", "[poc][properties]")
{
    SECTION("memory write -> property read")
    {
        for (const auto& [objectIdx, id, address, memory, onTheWire] : propertyCases)
        {
            F f;
            f.memoryWrite(address, memory);
            const auto result = f.propertyRead(objectIdx, id);
            INFO("object " << static_cast<int>(objectIdx) << " property " << static_cast<int>(id));
            REQUIRE(result.count == 1);
            REQUIRE(result.data == onTheWire);
        }
    }

    SECTION("property write -> memory read")
    {
        for (const auto& [objectIdx, id, address, memory, onTheWire] : propertyCases)
        {
            if (id == PID_TABLE_REFERENCE)
            {
                continue; // not writable
            }

            F f;
            f.bcu->userEeprom->modified(false);
            f.propertyWrite(objectIdx, id, onTheWire);
            INFO("object " << static_cast<int>(objectIdx) << " property " << static_cast<int>(id));
            REQUIRE(f.bcu->userEeprom->isModified());
            REQUIRE(f.memoryRead(address, static_cast<uint8_t>(memory.size())).data == memory);
        }
    }

    SECTION("PDT_GENERIC_xx: property write followed by property read returns the value")
    {
        F f;
        const Data value = {1, 2, 3, 4, 5, 6};
        REQUIRE(f.propertyWrite(OT_DEVICE, PID_HARDWARE_TYPE, value).data == value);
        REQUIRE(f.propertyRead(OT_DEVICE, PID_HARDWARE_TYPE).data == value);
    }

    SECTION("PDT_UNSIGNED_INT: property write followed by property read returns the value")
    {
        F f;
        const Data value = {0x12, 0x34};
        REQUIRE(f.propertyWrite(OT_DEVICE, PID_SERVICE_CONTROL, value).data == value);
        REQUIRE(f.propertyRead(OT_DEVICE, PID_SERVICE_CONTROL).data == value);
    }
}

TEST_CASE("MASK0701 property errors", "[poc][properties]")
{
    F f;
    f.bcu->userEeprom->modified(false);

    SECTION("read only property is refused and changes nothing")
    {
        const auto before = f.memoryRead(F::ADDR_TAB_ADDR, 2).data;
        REQUIRE(f.propertyWrite(OT_ADDR_TABLE, PID_TABLE_REFERENCE, {0x12, 0x34}).count == 0);
        REQUIRE(f.memoryRead(F::ADDR_TAB_ADDR, 2).data == before);
        REQUIRE_FALSE(f.bcu->userEeprom->isModified());
    }

    SECTION("unknown property and unknown object")
    {
        REQUIRE(f.propertyRead(OT_DEVICE, 0xFE).count == 0);
        REQUIRE(f.propertyRead(NUM_PROP_OBJECTS, PID_LOAD_STATE_CONTROL).count == 0);
    }

    SECTION("more than 10 bytes are refused")
    {
        REQUIRE(f.propertyRead(OT_DEVICE, PID_HARDWARE_TYPE, 2).count == 0);
    }
}
