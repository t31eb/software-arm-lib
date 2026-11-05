/*
 * com_objectsBCU2.cpp
 *
 *  Created on: 19.11.2021
 *      Author: dridders
 */

#include <sblib/eib/com_objectsBCU2.h>
#include <sblib/eib/bcu2.h>
#include <sblib/bits.h>

#include <cstddef>

namespace
{
/** Offset of the entry of com object @p objno in the table: behind the count and the flags table pointer */
constexpr uint32_t entryOffset(const int objno)
{
    return 1 + sizeof(ComConfigBCU2::DataPtrType) + objno * sizeof(ComConfigBCU2);
}
}

uint8_t* ComObjectsBCU2::objectValuePtr(const int objno)
{
    uint8_t type;
    uint16_t valueAddress;
    if (objectLayout != nullptr && objectLayout->object(objno, type, valueAddress))
    {
        return ((BcuDefault*)bcu)->userMemoryPtr(valueAddress);
    }

    // TODO Should handle userRam.segment0addr and userRam.segment1addr here
    // if (cfg.config & COMCONF_VALUE_TYPE) // 0 if segment 0, !=0 if segment 1
    uint8_t addr[sizeof(ComConfigBCU2::DataPtrType)];
    if (!readObjectConfigTable(entryOffset(objno) + offsetof(ComConfigBCU2, dataPtr), addr, sizeof(addr)))
    {
        return nullptr; // before, a missing table dereferenced a null pointer
    }
    if (le_ptr == LITTLE_ENDIAN)
        return ((BcuDefault*)bcu)->userMemoryPtr(makeWord(addr[1], addr[0]));
    else
        return ((BcuDefault*)bcu)->userMemoryPtr(makeWord(addr[0], addr[1]));
}

uint8_t* ComObjectsBCU2::objectConfigTable()
{
    const uint16_t comObjTableAddr = ((BCU2*)bcu)->userEeprom->commsTabAddr();
    return ((BcuDefault*)bcu)->userMemoryPtr(comObjTableAddr);
}

bool ComObjectsBCU2::readObjectConfigTable(const uint32_t offset, uint8_t* buffer, const uint32_t length)
{
    const uint16_t comObjTableAddr = ((BCU2*)bcu)->userEeprom->commsTabAddr();
    if (comObjTableAddr == 0)
    {
        return false;
    }
    return ((BcuDefault*)bcu)->readUserMemory(comObjTableAddr + offset, buffer, length);
}

uint8_t* ComObjectsBCU2::objectFlagsTable()
{
    if (objectLayout != nullptr && objectLayout->flagsTableAddress() != 0)
    {
        return ((BcuDefault*)bcu)->userMemoryPtr(objectLayout->flagsTableAddress());
    }

    uint8_t configTable[3];
    if (!readObjectConfigTable(0, configTable, sizeof(configTable)))
    {
        return nullptr; // before, a missing table dereferenced a null pointer
    }
    uint16_t flagsTableAddress;

    if (le_ptr == LITTLE_ENDIAN)
    {
        flagsTableAddress = makeWord(configTable[2], configTable[1]);
    }
    else
    {
        flagsTableAddress = makeWord(configTable[1], configTable[2]);
    }
    return ((BcuDefault*)bcu)->userMemoryPtr(flagsTableAddress);
}

ComConfig ComObjectsBCU2::objectConfig(const int objno)
{
    ComConfig config {0, 0};
    uint8_t bytes[sizeof(ComConfig)];
    if (readObjectConfigTable(entryOffset(objno) + offsetof(ComConfigBCU2, baseConfig), bytes, sizeof(bytes)))
    {
        config.config = bytes[0];
        config.type = bytes[1];
    }
    uint8_t type;
    uint16_t valueAddress;
    if (objectLayout != nullptr && objectLayout->object(objno, type, valueAddress))
    {
        config.type = type; // the flags (config) stay as ETS configured them
    }
    return config; // before, a missing table dereferenced a null pointer
}

void ComObjectsBCU2::printObjectConfigTable()
{
#ifdef DUMP_COM_OBJ
        uint16_t comObjTableAddr = ((BCU2*)bcu)->userEeprom->commsTabAddr();
        serial.println("ObjectConfigTable:");
        serial.println("   address      : 0x", comObjTableAddr, HEX, 4);
        if (comObjTableAddr == 0)
        {
            serial.println("invalid address!");
            return;
        }
        uint8_t* currentTablePosition = ((BcuDefault*)bcu)->userMemoryPtr(comObjTableAddr);
        if (currentTablePosition == nullptr)
        {
            serial.println("invalid currentTablePosition!");
            return;
        }

        uint8_t currentSize = *currentTablePosition;
        serial.println("   #com objects : ", currentSize, DEC, 3);
        currentTablePosition++; // 1 byte #com objects
        uint16_t ramFlagsTablePointer;
        if (le_ptr == LITTLE_ENDIAN)
        {
            ramFlagsTablePointer = makeWord(*(currentTablePosition + 1), *currentTablePosition);
        }
        else
        {
            ramFlagsTablePointer = makeWord(*currentTablePosition, *(currentTablePosition + 1));
        }
        currentTablePosition += 2; // 2 bytes RAM-Flags-Pointer
        serial.println("   RAM-Flags-Ptr: 0x", ramFlagsTablePointer, HEX, 4);
        for (uint8_t i = 0; i < currentSize; i++)
        {
            uint16_t data;
            if (le_ptr == LITTLE_ENDIAN)
            {
                data = makeWord(*(currentTablePosition + 1), *currentTablePosition);
            }
            else
            {
                data = makeWord(*currentTablePosition, *(currentTablePosition + 1));
            }
            currentTablePosition += 2; // 2 bytes data pointer
            uint8_t config = *currentTablePosition;
            currentTablePosition++; // 1 byte config
            uint8_t type = *currentTablePosition;
            currentTablePosition++; // 1 byte type
            serial.print("#", i, DEC, 3);
            serial.print(": data 0x", data, HEX, 4);
            serial.print(" config (0x", config, HEX, 2);
            serial.print("): ");
            printComObjectConfig(config);
            serial.print(" type: ");
            printComObjectType(type);
            serial.println();
        }
#endif
}
