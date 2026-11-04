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

bool AddrTablesBCU2::readAssocTable(const uint32_t offset, uint8_t* buffer, const uint32_t length)
{
    return bcu->readUserMemory(bcu->userEeprom->assocTabAddr() + offset, buffer, length);
}
