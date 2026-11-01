/******************************************************************************
 * @addtogroup SBLIB_MAIN_GROUP Main Group Description
 * @defgroup SBLIB_SUB_GROUP_1 Sub Group 1 Description
 * @ingroup SBLIB_MAIN_GROUP
 * @brief   
 * @details 
 *
 *
 * @{
 *
 * @file   memory.h
 * @author Darthyson <darth@maptrack.de>
 ******************************************************************************/

/*
 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License version 3 as
 published by the Free Software Foundation.
 ---------------------------------------------------------------------------*/

#ifndef SBLIB_KNX_MEMORY_H_
#define SBLIB_KNX_MEMORY_H_

#include <cstdint>
#include <type_traits>

/**
 * Byte order of a multibyte value in memory.
 */
enum class ByteOrder : uint8_t
{
    littleEndian, //!< Lowest address holds the least significant byte
    bigEndian     //!< Lowest address holds the most significant byte
};

/**
 * Class for basic memory operations.
 *
 * @details All accesses use a @c virtAddress, the address ETS uses, not an address of the ARM.
 *          @ref readBlock and @ref writeBlock are the primitives every memory has to implement,
 *          @ref read and @ref write are built upon them. The caller provides the buffer, so no pointer into
 *          the memory leaves this class and it doesn't matter where the data physically is.
 */
class Memory
{
public:
    Memory() = delete;
    virtual ~Memory() = default;
    Memory(uint32_t start, uint32_t size);

    /**
     * Access the memory like an ordinary array
     *
     * @param address  The address of the data byte to access.
     * @return The data byte at the given address.
     * @note @ref startAddress will be subtracted from @ref address
     */
    virtual uint8_t& operator[](uint32_t address) const = 0;

    /**
     * Access the memory like an ordinary array
     *
     * @param address  The address of the data byte to access.
     * @return The data byte at the given address.
     * @note @ref startAddress will be subtracted from @ref address
     */
    virtual uint8_t& operator[](uint32_t address) = 0;

    /**
     * Gets a byte from the memory at given @ref address
     *
     * @param address   Address to get the byte from
     * @return          Content as byte at the given @ref address
     * @note @ref startAddress will be subtracted from @ref address
     */
    [[nodiscard]] virtual uint8_t getUInt8(uint32_t address) const = 0;

    /**
     * Gets a word from the memory at given @ref address and address + 1
     *
     * @param address   Address to get the byte from
     * @return          Content as word at the @ref address
     * @note @ref startAddress will be subtracted from @ref address
     */
    [[nodiscard]] virtual uint16_t getUInt16(uint32_t address) const = 0;

    /**
     * Copies a block of bytes out of the memory.
     *
     * @param virtAddress   Address of the first byte
     * @param buffer        Buffer of the caller, receives @p length bytes
     * @param length        Number of bytes to read
     * @return              True if successful, false if the block isn't completely inside of the memory.
     *                      Then @p buffer stays untouched. A @p length of 0 does nothing and is successful.
     */
    [[nodiscard]] virtual bool readBlock(uint32_t virtAddress, uint8_t* buffer, uint32_t length) const = 0;

    /**
     * Copies a block of bytes into the memory.
     *
     * @param virtAddress   Address of the first byte
     * @param buffer        Buffer of the caller, provides @p length bytes
     * @param length        Number of bytes to write
     * @return              True if successful, false if the block isn't completely inside of the memory.
     *                      Then the memory stays untouched. A @p length of 0 does nothing and is successful.
     */
    [[nodiscard]] virtual bool writeBlock(uint32_t virtAddress, const uint8_t* buffer, uint32_t length) = 0;

    /**
     * Reads a value, works on any address without alignment restrictions.
     *
     * @tparam T            uint8_t, uint16_t or uint32_t
     * @param virtAddress   Address of the first byte of the value
     * @param byteOrder     Byte order of the value in memory
     * @return              The value, 0 if it isn't completely inside of the memory
     */
    template <typename T>
    [[nodiscard]] T read(uint32_t virtAddress, ByteOrder byteOrder) const;

    /**
     * Writes a value, works on any address without alignment restrictions.
     *
     * @tparam T            uint8_t, uint16_t or uint32_t
     * @param virtAddress   Address of the first byte of the value
     * @param value         The value
     * @param byteOrder     Byte order of the value in memory
     * @return              True if successful, false if the value isn't completely inside of the memory
     */
    template <typename T>
    [[nodiscard]] bool write(uint32_t virtAddress, T value, ByteOrder byteOrder);

    /**
     * Checks, if an @ref address is accessible in the memory
     *
     * @param address   Address to check for valid access
     * @return          True if @ref address is accessible, otherwise false
     */
    [[nodiscard]] virtual bool inRange(uint32_t address) const;

    /**
     * Checks, if a given address range is accessible in the memory
     *
     * @param start     Start address of the range to check
     * @param end       End address of the range to check
     * @return          True if range is accessible, otherwise false
     */
    [[nodiscard]] virtual bool inRange(uint32_t start, uint32_t end) const;

    /**
     * Get the start address of the memory
     *
     * @return Start address of the memory
     */
    [[nodiscard]] uint32_t startAddr() const;

    /**
     * Get the end address of the memory
     *
     * @return end address of the memory
     */
    [[nodiscard]] uint32_t endAddr() const;

    /**
     * Get the size of the memory
     *
     * @return size of the memory in bytes
     */
    [[nodiscard]] uint32_t size() const;

protected:
    uint32_t startAddress; //!< virtual start address of the memory
    uint32_t endAddress;   //!< virtual end address of the memory
    uint32_t sizeTotal;    //!< size in bytes of the memory

    /**
     * Changes the given absolute memory address to the real internal index
     *
     * @param address The absolute memory address
     */
    void normalizeAddress(uint32_t* address) const { (*address -= startAddress); }

private:
    template <typename T>
    static constexpr bool isSupportedValueType = std::is_same_v<T, uint8_t> || std::is_same_v<T, uint16_t> || std::is_same_v<T, uint32_t>;
};

template <typename T>
T Memory::read(const uint32_t virtAddress, const ByteOrder byteOrder) const
{
    static_assert(isSupportedValueType<T>, "Memory::read supports uint8_t, uint16_t and uint32_t");

    uint8_t bytes[sizeof(T)];
    if (!readBlock(virtAddress, bytes, sizeof(T)))
    {
        return 0;
    }

    uint32_t value = 0;
    for (uint32_t i = 0; i < sizeof(T); i++)
    {
        const uint32_t index = (byteOrder == ByteOrder::bigEndian) ? i : (sizeof(T) - 1 - i);
        value = (value << 8) | bytes[index];
    }
    return static_cast<T>(value);
}

template <typename T>
bool Memory::write(const uint32_t virtAddress, const T value, const ByteOrder byteOrder)
{
    static_assert(isSupportedValueType<T>, "Memory::write supports uint8_t, uint16_t and uint32_t");

    uint8_t bytes[sizeof(T)];
    uint32_t remaining = value;
    for (uint32_t i = 0; i < sizeof(T); i++)
    {
        const uint32_t index = (byteOrder == ByteOrder::littleEndian) ? i : (sizeof(T) - 1 - i);
        bytes[index] = static_cast<uint8_t>(remaining);
        remaining >>= 8;
    }
    return writeBlock(virtAddress, bytes, sizeof(T));
}


#endif /* SBLIB_KNX_MEMORY_H_ */
/** @}*/
