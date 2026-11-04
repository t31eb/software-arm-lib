/*
 *  addr_tables.cpp - BCU communication address tables.
 *
 *  Copyright (c) 2014 Stefan Taferner <stefan.taferner@gmx.at>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 3 as
 *  published by the Free Software Foundation.
 */

#include <sblib/eib/addr_tables.h>

uint16_t AddrTables::addrCount()
{
    uint8_t count = 0;
    if (!readAddrTable(0, &count, 1))
    {
        return 0; // before, a missing table dereferenced a null pointer
    }
    return count;
}
