/*
 *  addr_tables.h - BCU communication address tables.
 *
 *  Copyright (c) 2014 Stefan Taferner <stefan.taferner@gmx.at>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 3 as
 *  published by the Free Software Foundation.
 */
#ifndef SBLIB_KNX_ADDR_TABLES_H_
#define SBLIB_KNX_ADDR_TABLES_H_

#include <cstdint>
#include <sblib/types.h>

/**
 * Number of table entries read at once when scanning the address or association table.
 * Bigger values need more stack, smaller ones more read calls (with an external EEPROM one transaction each).
 */
constexpr int TABLE_READ_CHUNK = 16;

class AddrTables
{
public:
    virtual ~AddrTables() = default;
    /**
     * Get the index of a group address in the address table.
     *
     * @param addr - the address to find.
     * @return The index of the address, -1 if not found.
     *
     * @brief The address table contains the configured group addresses and our
     * own physical address. This function skips the own physical address and
     * only scans the group addresses.
     */
    virtual int indexOfAddr(int addr) = 0;

    /**
     * Get the address table. The address table contains the configured group addresses
     * and our own physical address.
     *
     * @return The pointer to the address table.
     *
     * @brief The first byte of the table contains the number of entries. The rest of
     * the table consists of the addresses: 2 bytes per address.
     */
    virtual uint8_t* addrTable() = 0;

    /**
     * Get the association table. The association table connects group addresses
     * with communication objects.
     *
     * @return The pointer to the association table.
     *
     * @brief The first byte of the table contains the number of entries. The rest of
     * the table consists of the associations - 2 bytes per association:
     * 1 byte addr-table index, 1 byte com-object number.
     */
    virtual uint8_t* assocTable() = 0;

    /**
     * Read a part of the association table, without a pointer into the memory.
     *
     * @param offset Offset of the first byte to read, 0 is the number of entries.
     * @param buffer Buffer of at least @p length bytes, provided by the caller.
     * @param length Number of bytes to read.
     * @return true if the bytes were read, false if there is no table or the bytes are outside of the user memory.
     */
    [[nodiscard]] virtual bool readAssocTable(uint32_t offset, uint8_t* buffer, uint32_t length) = 0;

    /**
     * Read a part of the address table, without a pointer into the memory.
     *
     * @param offset Offset of the first byte to read, 0 is the number of entries.
     * @param buffer Buffer of at least @p length bytes, provided by the caller.
     * @param length Number of bytes to read.
     * @return true if the bytes were read, false if there is no table or the bytes are outside of the user memory.
     */
    [[nodiscard]] virtual bool readAddrTable(uint32_t offset, uint8_t* buffer, uint32_t length) = 0;

    /**
     * Get total number of address entries
     * @return Total number of address entries including own address
     */
    virtual uint16_t addrCount();
};

#endif /* SBLIB_KNX_ADDR_TABLES_H_ */
