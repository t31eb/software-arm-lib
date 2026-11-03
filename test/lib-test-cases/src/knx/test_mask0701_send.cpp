/*
 * Characterization tests of ComObjects::sendNextGroupTelegram() on a MASK0701
 *
 * These tests document the current behavior, even where it looks wrong.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 */

#include "mask0701_fixture.h"

#include <sblib/eib/apci.h>
#include <sblib/eib/knx_lpdu.h>
#include <sblib/eib/knx_npdu.h>

using F = Mask0701Fixture;
using Data = std::vector<uint8_t>;

namespace
{
/** Leaves the INIT state of the bus, so telegrams can be sent */
void startBus(const F& f)
{
    _LPC_TMR16B1.IR = 4;
    f.bcu->bus->timerInterruptHandler();
    _LPC_TMR16B1.IR = 4;
    f.bcu->bus->timerInterruptHandler();
}

/** The telegram the bus is sending, empty if none */
Data sending(const F& f)
{
    const uint8_t* telegram = f.bcu->bus->sendCurTelegram;
    if (telegram == nullptr)
    {
        return {};
    }
    return Data(telegram, telegram + telegramSize(telegram));
}

/** Finishes sending like the bus interrupt does, including the acknowledge of the receiver */
void finishSending(const F& f)
{
    Bus* bus = f.bcu->bus;
    LPC_TMR16B1->IR = 0x00; // kein Capture-Ereignis: der nächste Interrupt ist ein Zeitablauf
    if (bus->state == Bus::WAIT_50BT_FOR_NEXT_RX_OR_PENDING_TX_OR_IDLE)
    {
        bus->timerInterruptHandler();
    }
    if (bus->state == Bus::SEND_START_BIT)
    {
        _LPC_TMR16B1.TC += 10;
        bus->state = Bus::SEND_END_OF_TX;
        bus->timerInterruptHandler();
        if (bus->state == Bus::SEND_WAIT_FOR_RX_ACK_WINDOW)
        {
            bus->telegram[0] = bus->currentByte = SB_BUS_ACK;
            bus->telegramLen = 1;
            bus->state = Bus::RECV_WAIT_FOR_STARTBIT_OR_TELEND;
            bus->nextByteIndex = 1;
            bus->parity = 1;
            _LPC_TMR16B1.IR = 0;
            bus->timerInterruptHandler();
            bus->telegramLen = 0;
            LPC_TMR16B1->IR = 0x00;
        }
    }
    REQUIRE(bus->sendCurTelegram == nullptr);
}

uint16_t destination(const Data& telegram)
{
    return makeWord(telegram[3], telegram[4]);
}

uint16_t apci(const Data& telegram)
{
    return makeWord(telegram[6] & 0x03, telegram[7] & 0xC0);
}

uint8_t flags(const F& f, const int objno)
{
    const uint8_t both = f.bcu->comObjects->objectFlagsTable()[objno >> 1];
    return (objno & 1) ? (both >> 4) : (both & 0x0F);
}
}

TEST_CASE("MASK0701 sending of group telegrams", "[poc][comobjects]")
{
    F f;
    f.downloadTables();
    REQUIRE(f.bcu->applicationRunning());
    startBus(f);
    ComObjects* com = f.bcu->comObjects;

    SECTION("nothing to send")
    {
        REQUIRE_FALSE(com->sendNextGroupTelegram());
        REQUIRE(sending(f).empty());
    }

    SECTION("objectWrite sends a group write to the first associated group address")
    {
        com->objectWrite(1, 0x42);
        REQUIRE(flags(f, 1) == COMFLAG_TRANSREQ);
        REQUIRE(com->sendNextGroupTelegram());
        const Data telegram = sending(f);
        REQUIRE(destination(telegram) == F::GROUP_ADDRESS_1);
        REQUIRE(apci(telegram) == APCI_GROUP_VALUE_WRITE_PDU);
        REQUIRE(telegram[8] == 0x42);
        finishSending(f);
        // the transmission status is cleared, but the error bit is set (status check is commented out in the code)
        REQUIRE(flags(f, 1) == COMFLAG_ERROR);
    }

    SECTION("the sent value also updates other objects with the same group address, with their own size")
    {
        com->objectWrite(1, 0x42); // 1 byte; object 3 on the same group address has 2 bytes
        REQUIRE(com->sendNextGroupTelegram());
        finishSending(f);
        REQUIRE((flags(f, 3) & COMFLAG_UPDATE) == COMFLAG_UPDATE);
        REQUIRE(com->objectRead(3) == 0x4200); // the size of the receiving object counts
    }

    SECTION("requestObjectRead sends a group read")
    {
        com->requestObjectRead(2);
        REQUIRE(com->sendNextGroupTelegram());
        const Data telegram = sending(f);
        REQUIRE(destination(telegram) == F::GROUP_ADDRESS_2);
        REQUIRE(apci(telegram) == APCI_GROUP_VALUE_READ_PDU);
        finishSending(f);
    }

    SECTION("an object without group address is not sent and stays requested")
    {
        com->objectWrite(0, 1);
        REQUIRE_FALSE(com->sendNextGroupTelegram());
        REQUIRE(sending(f).empty());
        REQUIRE(flags(f, 0) == COMFLAG_TRANSREQ);
    }

    SECTION("an object without communication enable is not sent")
    {
        // config byte of object 2: table + 1 count + 2 flags pointer + 4 per object, data pointer 2 bytes before
        f.memoryWrite(F::COM_TABLE + 3 + 2 * 4 + 2, {COMCONF_READ | COMCONF_WRITE | COMCONF_TRANS});
        com->objectWrite(2, 0x1234);
        REQUIRE_FALSE(com->sendNextGroupTelegram());
        REQUIRE(flags(f, 2) == COMFLAG_TRANSREQ);
    }

    SECTION("pending objects are sent one per call, the scan continues behind the last sent object")
    {
        com->objectWrite(2, 0x1234);
        com->objectWrite(1, 0x11);
        std::vector<uint16_t> order;
        for (int i = 0; i < 4 && com->sendNextGroupTelegram(); i++)
        {
            order.push_back(destination(sending(f)));
            finishSending(f);
        }
        REQUIRE(order == std::vector<uint16_t> {F::GROUP_ADDRESS_1, F::GROUP_ADDRESS_2});

        // the third call found nothing and moved the index behind the last object;
        // the next call wraps around to 0 and finds a new request for object 1 right away
        com->objectWrite(1, 0x22);
        REQUIRE(com->sendNextGroupTelegram());
        REQUIRE(destination(sending(f)) == F::GROUP_ADDRESS_1);
        finishSending(f);
    }
}
