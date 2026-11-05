/*
 * userEeprom.cpp
 *
 *  Created on: 19.11.2021
 *      Author: dridders
 */

#include <sblib/eib/userEeprom.h>
#include <sblib/eib/userEepromMirror.h>
#include <sblib/timer.h>
#include <sblib/bits.h>
#include <sblib/utils.h>


void UserEeprom::readUserEeprom()
{
    storage->reload();
    modified(false);
}

void UserEeprom::writeUserEeprom()
{
    if (!isModified())
    {
        return;
    }

    storage->flush();
    modified(false);
}

UserEeprom::UserEeprom(const uint32_t start, const uint32_t size, const uint32_t flashSize) :
    UserEeprom(start, size, new UserEepromMirror(size, flashSize))
{
}

UserEeprom::UserEeprom(const uint32_t start, const uint32_t size, UserEepromBackend* backend) :
    Memory(start, size),
    userEepromData(backend->mirror()),
    storage(backend)
{
    modified(false);
}

uint8_t& UserEeprom::directAccess_8(const uint32_t index) const
{
    if (!inRange(startAddr() + index) || (userEepromData == nullptr))
    {
        fatalError();
    }
    return this->userEepromData[index];
}

uint8_t UserEeprom::field8(const uint32_t offset) const
{
    uint8_t value;
    storage->read(offset, &value, 1);
    return value;
}

void UserEeprom::setField8(const uint32_t offset, const uint8_t value)
{
    storage->write(offset, &value, 1);
}

uint16_t UserEeprom::field16(const uint32_t offset) const
{
    return makeWord(field8(offset + 1), field8(offset));
}

void UserEeprom::setField16(const uint32_t offset, const uint16_t value)
{
    setField8(offset, lowByte(value));
    setField8(offset + 1, HIGH_BYTE(value));
}

void UserEeprom::readField(const uint32_t offset, uint8_t* buffer, const uint32_t length) const
{
    storage->read(offset, buffer, length);
}

void UserEeprom::writeField(const uint32_t offset, const uint8_t* buffer, const uint32_t length)
{
    storage->write(offset, buffer, length);
}

uint8_t& UserEeprom::operator[](uint32_t address) const
{
    if (userEepromData == nullptr)
    {
        fatalError();
    }
    normalizeAddress(&address);
    return userEepromData[address];
}

uint8_t& UserEeprom::operator[](const uint32_t address)
{
    /// 1. cast is static_cast<const UserEeprom&> to call operator[](..) const
    /// 2. cast is const_cast<uint8_t&>(..) to remove the const from the return of the operator[](...) const
    return const_cast<uint8_t&>(static_cast<const UserEeprom&>(*this).operator[](address));
}

uint8_t UserEeprom::getUInt8(uint32_t address) const
{
    normalizeAddress(&address);
    if (address > (size() - 1))
    {
        return 0;
    }
    return field8(address);
}

uint16_t UserEeprom::getUInt16(uint32_t address) const
{
    normalizeAddress(&address);
    if ((address + 1) > (size() - 1))
    {
        return 0;
    }
    return makeWord(field8(address), field8(address + 1));
}

bool UserEeprom::readBlock(uint32_t virtAddress, uint8_t* buffer, const uint32_t length) const
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

bool UserEeprom::writeBlock(uint32_t virtAddress, const uint8_t* buffer, const uint32_t length)
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
    modified(true);
    return true;
}

void UserEeprom::modified(const bool newModified)
{
    userEepromModified = newModified;
    if (userEepromModified)
    {
        writeUserEepromTime = millis() + 50;
    }
    else
    {
        writeUserEepromTime = 0;
    }
}

bool UserEeprom::writeDelayElapsed() const
{
    bool elapsed = userEepromModified;
    elapsed &= ((int)millis() - (int)writeUserEepromTime) > 0;
    return (elapsed);
}

bool UserEeprom::isModified() const
{
    return userEepromModified;
}
