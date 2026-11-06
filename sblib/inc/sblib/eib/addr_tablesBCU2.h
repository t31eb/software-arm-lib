/*
 *  addr_tables.h - BCU communication address tables.
 *
 *  Copyright (c) 2014 Stefan Taferner <stefan.taferner@gmx.at>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 3 as
 *  published by the Free Software Foundation.
 */
#ifndef SBLIB_KNX_ADDR_TABLES_BCU2_H_
#define SBLIB_KNX_ADDR_TABLES_BCU2_H_

#include <sblib/eib/addr_tables.h>

class BCU2;

class AddrTablesBCU2 : public AddrTables ///\todo derive from AddrTablesBCU1, to get rid of indexOfAddr(int addr)
{
public:
    explicit AddrTablesBCU2(BCU2* bcuInstance) : bcu(bcuInstance) {}

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
    int indexOfAddr(int addr) override;

    /**
     * Get the address table. The address table contains the configured group addresses
     * and our own physical address.
     *
     * @return The pointer to the address table.
     *
     * @brief The first byte of the table contains the number of entries. The rest of
     * the table consists of the addresses: 2 bytes per address.
     */
    uint8_t* addrTable() override;

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
    uint8_t* assocTable() override;

    [[nodiscard]] bool readAssocTable(uint32_t offset, uint8_t* buffer, uint32_t length) override;

    [[nodiscard]] bool readAddrTable(uint32_t offset, uint8_t* buffer, uint32_t length) override;

    /**
     * Write a part of the address table, without a pointer into the memory.
     * Marks the user EEPROM as modified if the table is there.
     *
     * @return true if the bytes were written, false if there is no table or the bytes are outside of the user memory.
     */
    bool writeAddrTable(uint32_t offset, const uint8_t* buffer, uint32_t length);

    /**
     * From now on @ref indexOfAddr searches a copy of the group addresses in RAM (2 bytes per address).
     *
     * @details The bus interrupt calls @ref indexOfAddr for every group telegram. If the user EEPROM can't be read in an
     *          interrupt (@ref UserEepromBackend::readableInInterrupt, e.g. an SPI EEPROM), @ref BCU2 enables the copy.
     *          It is read again by @ref refreshGroupAddresses, which @ref BCU2::storeLoadState calls whenever the load
     *          state of the address table changes. A table changed without load control stays unseen until the next
     *          start.
     */
    void enableGroupAddressCache();

    /** Reads the group addresses into the RAM copy again, if it is enabled. Not in an interrupt. */
    void refreshGroupAddresses();

private:
    BCU2* bcu;
    bool groupAddressCacheEnabled = false;
    /** RAM copy of the group addresses: [0] number of addresses, then the addresses (high byte first in the table).
     *  Replaced as a whole, the bus interrupt reads the pointer once. */
    uint16_t* volatile groupAddresses = nullptr;
};

#endif /* SBLIB_KNX_ADDR_TABLES_BCU2_H_ */
