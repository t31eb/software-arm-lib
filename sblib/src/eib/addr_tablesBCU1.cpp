/*
 * addr_tablesBCU1.cpp
 *
 *  Created on: 20.11.2021
 *      Author: dridders
 */

#include <sblib/eib/addr_tablesBCU1.h>

#include <cstring>

int AddrTablesBCU1::indexOfAddr(const int addr)
{
    uint8_t* tab = addrTable();
    int num = 0;

    if (tab)
        num = *tab;
    tab += 3;

    const int addrHigh = addr >> 8;
    const int addrLow = addr & 255;

    for (int i = 1; i <= num; ++i, tab += 2)
    {
        if (tab[0] == addrHigh && tab[1] == addrLow)
            return i;
    }

    return -1;
}

uint8_t* AddrTablesBCU1::addrTable()
{
    // The table starts with its size, one byte before the entries. Temporary until addrTable() returns no pointer.
    return userEeprom->addrTab() - 1;
}

uint8_t* AddrTablesBCU1::assocTable()
{
    return userEeprom->userEepromData + userEeprom->assocTabPtr();
}

bool AddrTablesBCU1::readAddrTable(const uint32_t offset, uint8_t* buffer, const uint32_t length)
{
    // BCU1 is not converted, the table stays where addrTable() points to
    memcpy(buffer, addrTable() + offset, length);
    return true;
}

bool AddrTablesBCU1::readAssocTable(const uint32_t offset, uint8_t* buffer, const uint32_t length)
{
    return userEeprom->readBlock(userEeprom->startAddr() + userEeprom->assocTabPtr() + offset, buffer, length);
}
