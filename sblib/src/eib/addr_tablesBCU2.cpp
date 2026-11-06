/*
 * addr_tablesBCU2.cpp
 *
 *  Created on: 20.11.2021
 *      Author: dridders
 */

#include <sblib/eib/addr_tablesBCU2.h>
#include <sblib/eib/bcu2.h>
#include <sblib/bits.h>

#include <algorithm>

int AddrTablesBCU2::indexOfAddr(const int addr)
{
    const uint16_t* cached = groupAddresses;
    if (cached != nullptr)
    {
        for (uint16_t i = 0; i < cached[0]; i++)
        {
            if (cached[1 + i] == addr)
            {
                return i + 1;
            }
        }
        return -1;
    }

    uint8_t num = 0;
    if (!readAddrTable(0, &num, 1))
    {
        return -1;
    }

    const int addrHigh = addr >> 8;
    const int addrLow = addr & 255;

    // num includes the own physical address, see KNX Spec. 3/5/1 4.16.3.3
    // Entry i starts at offset 1 + 2 * i, entry 0 is the own physical address.
    uint8_t chunk[2 * TABLE_READ_CHUNK];
    for (int first = 1; first < num; first += TABLE_READ_CHUNK)
    {
        const int entries = std::min(TABLE_READ_CHUNK, num - first);
        if (!readAddrTable(1 + 2 * first, chunk, 2 * entries))
        {
            return -1;
        }
        for (int i = 0; i < entries; ++i)
        {
            if (chunk[2 * i] == addrHigh && chunk[2 * i + 1] == addrLow)
                return first + i;
        }
    }

    return -1;
}

uint8_t* AddrTablesBCU2::addrTable()
{
    return bcu->userMemoryPtr(bcu->userEeprom->addrTabAddr());
}

uint8_t* AddrTablesBCU2::assocTable()
{
    return bcu->userMemoryPtr(bcu->userEeprom->assocTabAddr());
}

bool AddrTablesBCU2::readAddrTable(const uint32_t offset, uint8_t* buffer, const uint32_t length)
{
    return bcu->readUserMemory(bcu->userEeprom->addrTabAddr() + offset, buffer, length);
}

bool AddrTablesBCU2::writeAddrTable(const uint32_t offset, const uint8_t* buffer, const uint32_t length)
{
    return bcu->writeUserMemory(bcu->userEeprom->addrTabAddr() + offset, buffer, length);
}

bool AddrTablesBCU2::readAssocTable(const uint32_t offset, uint8_t* buffer, const uint32_t length)
{
    return bcu->readUserMemory(bcu->userEeprom->assocTabAddr() + offset, buffer, length);
}

void AddrTablesBCU2::enableGroupAddressCache()
{
    groupAddressCacheEnabled = true;
    refreshGroupAddresses();
}

void AddrTablesBCU2::refreshGroupAddresses()
{
    if (!groupAddressCacheEnabled)
    {
        return;
    }

    uint8_t num = 0;
    if (!readAddrTable(0, &num, 1))
    {
        num = 0;
    }
    // num includes the own physical address, entry 0
    const uint16_t count = (num > 0) ? num - 1 : 0;
    auto* fresh = new uint16_t[1 + count];
    fresh[0] = 0;
    uint8_t chunk[2 * TABLE_READ_CHUNK];
    for (int first = 1; first < num; first += TABLE_READ_CHUNK)
    {
        const int entries = std::min(TABLE_READ_CHUNK, num - first);
        if (!readAddrTable(1 + 2 * first, chunk, 2 * entries))
        {
            break;
        }
        for (int i = 0; i < entries; ++i)
        {
            fresh[1 + fresh[0]++] = static_cast<uint16_t>((chunk[2 * i] << 8) | chunk[2 * i + 1]);
        }
    }

    // The bus interrupt can't interrupt itself: after the pointer is replaced, nobody uses the old copy anymore
    uint16_t* old = groupAddresses;
    groupAddresses = fresh;
    delete[] old;
}
