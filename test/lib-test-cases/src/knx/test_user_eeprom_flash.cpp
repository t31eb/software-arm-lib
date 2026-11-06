/*
 * Tests of the user EEPROM backend in the flash with a cache of one page (UserEepromFlash).
 *
 * The RAM mirror (UserEepromMirror) with a page size of a whole sector is the reference: both backends have to
 * show the same bytes and leave the same flash content, so a device can switch between them without a download.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 */

#include <catch.hpp> // If possible, include catch.hpp as last header
#include <iap_emu.h>
#include <sblib/eib/userEepromFlash.h>
#include <sblib/eib/userEepromMirror.h>
#include <sblib/internal/iap.h>

#include <cstring>
#include <random>
#include <vector>

namespace
{
constexpr uint32_t SIZE = 3072;            //!< user EEPROM of MASK0701
constexpr uint32_t SECTOR = 0xF000;        //!< last flash sector of the emulated 64 KB flash
constexpr uint32_t PAGE = FLASH_PAGE_SIZE;

std::vector<uint8_t> flashContent()
{
    return {&FLASH[SECTOR], &FLASH[SECTOR + SIZE]};
}

std::vector<uint8_t> readAll(const UserEepromBackend& backend)
{
    std::vector<uint8_t> bytes(SIZE);
    backend.read(0, bytes.data(), SIZE);
    return bytes;
}

void write(UserEepromBackend& backend, const uint32_t offset, const std::vector<uint8_t>& bytes)
{
    backend.write(offset, bytes.data(), static_cast<uint32_t>(bytes.size()));
}

/** Counts the calls, like BcuDefault pausing the bus */
class CountingGuard : public FlashGuard
{
public:
    void pauseForFlash() override { pauses++; }
    void resumeAfterFlash() override { resumes++; }
    int pauses = 0;
    int resumes = 0;
};

/** A flash with a valid user EEPROM of the mirror: byte i is i * 7 */
void prepareValidFlash()
{
    IAP_Init_Flash(0xff);
    UserEepromMirror mirror(SIZE, FLASH_SECTOR_SIZE);
    for (uint32_t i = 0; i < SIZE; i++)
    {
        const auto value = static_cast<uint8_t>(i * 7);
        mirror.write(i, &value, 1);
    }
    mirror.flush();
}
}

TEST_CASE("User EEPROM in the flash with a cache of one page", "[userEeprom][flashBackend]")
{
    SECTION("an empty flash reads as zeros and isn't touched by writes before flush")
    {
        IAP_Init_Flash(0xff);
        FlashPageCache cache;
        UserEepromFlash flash(SIZE, cache);
        REQUIRE(readAll(flash) == std::vector<uint8_t>(SIZE, 0));

        iap_program_log_count = 0;
        write(flash, 0x370, {1, 2, 3});
        REQUIRE(iap_program_log_count == 0);
        REQUIRE(FLASH[SECTOR + 0x370] == 0xff);
    }

    SECTION("the first flush of an empty flash leaves the same flash as the mirror")
    {
        IAP_Init_Flash(0xff);
        UserEepromMirror mirror(SIZE, FLASH_SECTOR_SIZE);
        write(mirror, 0x370, {1, 2, 3});
        mirror.flush();
        const auto expected = flashContent();

        IAP_Init_Flash(0xff);
        FlashPageCache cache;
        UserEepromFlash flash(SIZE, cache);
        write(flash, 0x370, {1, 2, 3});
        flash.flush();
        REQUIRE(flashContent() == expected);
        REQUIRE(FLASH[SECTOR + SIZE - 1] == 0);
        REQUIRE(flash.pageWrites() == SIZE / PAGE);
    }

    SECTION("a flash written by the mirror is read unchanged")
    {
        prepareValidFlash();
        const auto mirrorBytes = readAll(UserEepromMirror(SIZE, FLASH_SECTOR_SIZE));
        FlashPageCache cache;
        UserEepromFlash flash(SIZE, cache);
        REQUIRE(readAll(flash) == mirrorBytes);
    }

    SECTION("flush writes back only the changed page")
    {
        prepareValidFlash();
        FlashPageCache cache;
        UserEepromFlash flash(SIZE, cache);
        iap_program_log_count = 0;
        write(flash, 0x371, {0xAA, 0xBB});
        REQUIRE(readAll(flash)[0x371] == 0xAA);
        flash.flush();
        REQUIRE(iap_program_log_count == 1);
        REQUIRE(iap_program_log[0] == SECTOR + 0x300);
        REQUIRE(FLASH[SECTOR + 0x372] == 0xBB);
        REQUIRE(FLASH[SECTOR + 0x370] == static_cast<uint8_t>(0x370 * 7));
        REQUIRE(FLASH[SECTOR + 0x400] == static_cast<uint8_t>(0x400 * 7)); // next page unchanged

        iap_program_log_count = 0;
        flash.flush();
        REQUIRE(iap_program_log_count == 0);
    }

    SECTION("writing to another page writes the changed page back, asking the guard")
    {
        prepareValidFlash();
        FlashPageCache cache;
        UserEepromFlash flash(SIZE, cache);
        CountingGuard guard;
        flash.setFlashGuard(&guard);
        iap_program_log_count = 0;

        write(flash, 0x010, {0x11});
        write(flash, 0x020, {0x22}); // same page
        REQUIRE(iap_program_log_count == 0);
        write(flash, 0x110, {0x33});
        REQUIRE(iap_program_log_count == 1);
        REQUIRE(iap_program_log[0] == SECTOR);
        REQUIRE(FLASH[SECTOR + 0x010] == 0x11);
        REQUIRE(guard.pauses == 1);
        REQUIRE(guard.resumes == 1);

        flash.flush(); // the caller of flush pauses the bus itself
        REQUIRE(iap_program_log_count == 2);
        REQUIRE(guard.pauses == 1);
    }

    SECTION("a write across a page border writes back the first page")
    {
        prepareValidFlash();
        FlashPageCache cache;
        UserEepromFlash flash(SIZE, cache);
        iap_program_log_count = 0;
        write(flash, 0x1FE, {1, 2, 3, 4});
        REQUIRE(iap_program_log_count == 1);
        flash.flush();
        REQUIRE(iap_program_log_count == 2);
        REQUIRE(std::vector<uint8_t>(&FLASH[SECTOR + 0x1FE], &FLASH[SECTOR + 0x202]) == std::vector<uint8_t>{1, 2, 3, 4});
    }

    SECTION("unchanged bytes don't cause a write back")
    {
        prepareValidFlash();
        FlashPageCache cache;
        UserEepromFlash flash(SIZE, cache);
        iap_program_log_count = 0;
        write(flash, 0x370, {static_cast<uint8_t>(0x370 * 7)});
        write(flash, 0x470, {static_cast<uint8_t>(0x470 * 7)});
        flash.flush();
        REQUIRE(iap_program_log_count == 0);
    }

    SECTION("reload discards the changes in the cache, but not those written back already")
    {
        prepareValidFlash();
        FlashPageCache cache;
        UserEepromFlash flash(SIZE, cache);
        write(flash, 0x270, {0xDD});
        write(flash, 0x370, {0xEE}); // writes back the page with 0x270
        flash.reload();
        REQUIRE(readAll(flash)[0x270] == 0xDD);
        REQUIRE(readAll(flash)[0x370] == static_cast<uint8_t>(0x370 * 7));
    }

    SECTION("the last byte reads as 0 after a flush, like with the mirror")
    {
        prepareValidFlash();
        FlashPageCache cache;
        UserEepromFlash flash(SIZE, cache);
        write(flash, SIZE - 1, {0x55});
        REQUIRE(readAll(flash)[SIZE - 1] == 0x55);
        flash.flush();
        REQUIRE(readAll(flash)[SIZE - 1] == 0);
    }
}

TEST_CASE("A range of the flash with a cache shared with the user EEPROM", "[userEeprom][flashBackend]")
{
    // Like the MultiSensorAktor without EEPROM: the segment 0x6000-0x6EFF at 0xE100, where its MemMapper keeps it
    constexpr uint32_t RANGE = 0xE100;
    constexpr uint32_t RANGE_SIZE = 0xF00;
    const auto rangeContent = [] { return std::vector<uint8_t>(&FLASH[RANGE], &FLASH[RANGE + RANGE_SIZE]); };

    SECTION("the range reads the flash as it is, an erased flash as 0xff")
    {
        IAP_Init_Flash(0xff);
        FLASH[RANGE + 0x10] = 0x42;
        FlashPageCache cache;
        UserEepromFlash range(RANGE_SIZE, cache, RANGE);
        std::vector<uint8_t> bytes(RANGE_SIZE);
        range.read(0, bytes.data(), RANGE_SIZE);
        std::vector<uint8_t> expected(RANGE_SIZE, 0xff);
        expected[0x10] = 0x42;
        REQUIRE(bytes == expected);
    }

    SECTION("a write back erases and programs only its page, without a valid marker")
    {
        IAP_Init_Flash(0xff);
        FLASH[0xE000] = 0x9e; // e.g. the allocation table of a MemMapper before
        FlashPageCache cache;
        UserEepromFlash range(RANGE_SIZE, cache, RANGE);
        iap_program_log_count = 0;
        write(range, RANGE_SIZE - 2, {0x55, 0x66});
        REQUIRE(FLASH[RANGE + RANGE_SIZE - 1] == 0xff);
        range.flush();
        REQUIRE(iap_program_log_count == 1);
        REQUIRE(iap_program_log[0] == RANGE + RANGE_SIZE - PAGE);
        REQUIRE(FLASH[RANGE + RANGE_SIZE - 2] == 0x55);
        REQUIRE(FLASH[RANGE + RANGE_SIZE - 1] == 0x66); // no marker
        REQUIRE(FLASH[RANGE + RANGE_SIZE - 3] == 0xff); // the rest of the page as it was
        REQUIRE(FLASH[0xE000] == 0x9e);
        range.reload();
        std::vector<uint8_t> bytes(2);
        range.read(RANGE_SIZE - 2, bytes.data(), 2);
        REQUIRE(bytes == std::vector<uint8_t>{0x55, 0x66});
    }

    SECTION("a write to the other backend writes the changed page back, asking the guard of the writer")
    {
        prepareValidFlash();
        FlashPageCache cache;
        UserEepromFlash user(SIZE, cache);
        UserEepromFlash range(RANGE_SIZE, cache, RANGE);
        CountingGuard userGuard;
        CountingGuard rangeGuard;
        user.setFlashGuard(&userGuard);
        range.setFlashGuard(&rangeGuard);
        iap_program_log_count = 0;

        write(user, 0x370, {0xAA});
        write(range, 0x010, {0xBB}); // the page of the user EEPROM goes to the flash
        REQUIRE(iap_program_log_count == 1);
        REQUIRE(iap_program_log[0] == SECTOR + 0x300);
        REQUIRE(FLASH[SECTOR + 0x370] == 0xAA);
        REQUIRE(rangeGuard.pauses == 1);
        REQUIRE(userGuard.pauses == 0);
        REQUIRE(readAll(user)[0x370] == 0xAA); // from the flash now

        user.flush(); // the cache holds a page of the range
        REQUIRE(iap_program_log_count == 1);
        range.flush();
        REQUIRE(iap_program_log_count == 2);
        REQUIRE(iap_program_log[1] == RANGE);
        REQUIRE(FLASH[RANGE + 0x010] == 0xBB);
    }

    SECTION("the changed page of the range goes to the flash before the user EEPROM formats its sector")
    {
        IAP_Init_Flash(0xff);
        FlashPageCache cache;
        UserEepromFlash user(SIZE, cache);
        UserEepromFlash range(RANGE_SIZE, cache, RANGE);
        write(range, 0x200, {1, 2, 3});
        write(user, 0x100, {4});
        user.flush();
        REQUIRE(FLASH[RANGE + 0x200] == 1);
        REQUIRE(FLASH[RANGE + 0x202] == 3);
        REQUIRE(readAll(user)[0x100] == 4);
        REQUIRE(FLASH[SECTOR + SIZE - 1] == 0);
        auto expected = std::vector<uint8_t>(RANGE_SIZE, 0xff);
        expected[0x200] = 1;
        expected[0x201] = 2;
        expected[0x202] = 3;
        REQUIRE(rangeContent() == expected);
    }

    SECTION("reload of one backend keeps the changes of the other in the cache")
    {
        prepareValidFlash();
        FlashPageCache cache;
        UserEepromFlash user(SIZE, cache);
        UserEepromFlash range(RANGE_SIZE, cache, RANGE);
        write(range, 0x020, {0x77});
        user.reload();
        range.flush();
        REQUIRE(FLASH[RANGE + 0x020] == 0x77);
    }
}

TEST_CASE("User EEPROM in the flash behaves like the mirror", "[userEeprom][flashBackend]")
{
    // Random writes, reads, flushes and restarts (flush and reload) on both backends, each with its own flash.
    // A reload without flush differs on purpose: the flash backend keeps the pages it has written back already.
    std::mt19937 random(0x0701);
    for (int run = 0; run < 20; run++)
    {
        IAP_Init_Flash(0xff);
        std::vector<uint8_t> mirrorFlash(&FLASH[0], &FLASH[SECTOR + FLASH_SECTOR_SIZE]);
        std::vector<uint8_t> flashFlash = mirrorFlash;
        const auto useFlash = [](const std::vector<uint8_t>& flash) { memcpy(FLASH, flash.data(), flash.size()); };
        const auto keepFlash = [](std::vector<uint8_t>& flash) { memcpy(flash.data(), FLASH, flash.size()); };

        UserEepromMirror mirror(SIZE, FLASH_SECTOR_SIZE);
        FlashPageCache cache;
        UserEepromFlash flash(SIZE, cache);

        for (int step = 0; step < 300; step++)
        {
            const uint32_t action = random() % 20;
            if (action < 14)
            {
                // not the last byte, the valid marker (see "the last byte reads as 0 after a flush")
                const uint32_t offset = random() % (SIZE - 1);
                const uint32_t length = 1 + random() % std::min<uint32_t>(16, SIZE - 1 - offset);
                std::vector<uint8_t> bytes(length);
                for (auto& b : bytes)
                {
                    b = static_cast<uint8_t>(random() % 4 == 0 ? 0 : random());
                }
                useFlash(mirrorFlash);
                write(mirror, offset, bytes);
                keepFlash(mirrorFlash);
                useFlash(flashFlash);
                write(flash, offset, bytes);
                keepFlash(flashFlash);
            }
            else if (action < 18)
            {
                useFlash(mirrorFlash);
                mirror.flush();
                keepFlash(mirrorFlash);
                useFlash(flashFlash);
                flash.flush();
                keepFlash(flashFlash);
                const std::vector<uint8_t> mirrorImage(&mirrorFlash[SECTOR], &mirrorFlash[SECTOR + SIZE]);
                if (flashFlash[SECTOR + SIZE - 1] == 0xff)
                {
                    // Nothing changed yet: the mirror writes a valid image of zeros, the flash backend nothing
                    REQUIRE(mirrorImage == std::vector<uint8_t>(SIZE, 0));
                }
                else
                {
                    REQUIRE(std::vector<uint8_t>(&flashFlash[SECTOR], &flashFlash[SECTOR + SIZE]) == mirrorImage);
                }
            }
            else
            {
                useFlash(mirrorFlash);
                mirror.flush();
                mirror.reload();
                keepFlash(mirrorFlash);
                useFlash(flashFlash);
                flash.flush();
                flash.reload();
                keepFlash(flashFlash);
            }
            useFlash(mirrorFlash);
            const auto expected = readAll(mirror);
            useFlash(flashFlash);
            const auto actual = readAll(flash);
            uint32_t firstDifference = SIZE;
            for (uint32_t i = 0; i < SIZE; i++)
            {
                if (actual[i] != expected[i])
                {
                    firstDifference = i;
                    break;
                }
            }
            CAPTURE(run, step, action, firstDifference);
            REQUIRE(firstDifference == SIZE);
        }
    }
}
