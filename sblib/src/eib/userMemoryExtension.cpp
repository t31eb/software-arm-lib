/*
 *  userMemoryExtension.cpp - A further range of non-volatile configuration memory beside the user EEPROM.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 3 as
 *  published by the Free Software Foundation.
 */

#include <sblib/eib/userMemoryExtension.h>
#include <sblib/bits.h>
#include <sblib/utils.h>

UserMemoryExtension::UserMemoryExtension(const uint32_t start, const uint32_t size, UserEepromBackend* backend) :
    Memory(start, size),
    storage(backend)
{
    if (backend->size() != size)
    {
        fatalError();
    }
}

uint8_t& UserMemoryExtension::operator[](uint32_t /*address*/) const
{
    fatalError();
    static uint8_t none = 0;
    return none;
}

uint8_t& UserMemoryExtension::operator[](const uint32_t address)
{
    return const_cast<uint8_t&>(static_cast<const UserMemoryExtension&>(*this).operator[](address));
}

uint8_t UserMemoryExtension::getUInt8(const uint32_t address) const
{
    return read<uint8_t>(address, ByteOrder::littleEndian);
}

uint16_t UserMemoryExtension::getUInt16(const uint32_t address) const
{
    // like UserEeprom::getUInt16: the byte at address is the high byte
    return read<uint16_t>(address, ByteOrder::bigEndian);
}

bool UserMemoryExtension::readBlock(uint32_t virtAddress, uint8_t* buffer, const uint32_t length) const
{
    if (length == 0)
    {
        return true;
    }
    if (!inRange(virtAddress, virtAddress + length - 1))
    {
        return false;
    }
    normalizeAddress(&virtAddress);
    storage->read(virtAddress, buffer, length);
    return true;
}

bool UserMemoryExtension::writeBlock(uint32_t virtAddress, const uint8_t* buffer, const uint32_t length)
{
    if (length == 0)
    {
        return true;
    }
    if (!inRange(virtAddress, virtAddress + length - 1))
    {
        return false;
    }
    normalizeAddress(&virtAddress);
    storage->write(virtAddress, buffer, length);
    return true;
}
