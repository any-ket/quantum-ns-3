/*
 * Copyright (c) 2007 INESC Porto
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: Gustavo J. A. M. Carneiro  <gjc@inescporto.pt>
 */

#include "ns3/olsr-header.h"
#include "ns3/olsr-repositories.h"
#include "ns3/packet.h"
#include "ns3/test.h"

using namespace ns3;

/**
 * @ingroup olsr-test
 * @ingroup tests
 *
 * Check Emf olsr time conversion
 */
class OlsrEmfTestCase : public TestCase
{
  public:
    OlsrEmfTestCase();
    void DoRun() override;
};

OlsrEmfTestCase::OlsrEmfTestCase()
    : TestCase("Check Emf olsr time conversion")
{
}

void
OlsrEmfTestCase::DoRun()
{
    for (int time = 1; time <= 30; time++)
    {
        uint8_t emf = olsr::SecondsToEmf(time);
        double seconds = olsr::EmfToSeconds(emf);
        NS_TEST_ASSERT_MSG_EQ((seconds < 0 || std::fabs(seconds - time) > 0.1), false, "100");
    }
}

/**
 * @ingroup olsr-test
 * @ingroup tests
 *
 * Check Mid olsr messages
 */
class OlsrMidTestCase : public TestCase
{
  public:
    OlsrMidTestCase();
    void DoRun() override;
};

OlsrMidTestCase::OlsrMidTestCase()
    : TestCase("Check Mid olsr messages")
{
}

void
OlsrMidTestCase::DoRun()
{
    Packet packet;

    {
        olsr::PacketHeader hdr;
        olsr::MessageHeader msg1;
        olsr::MessageHeader::Mid& mid1 = msg1.GetMid();
        olsr::MessageHeader msg2;
        olsr::MessageHeader::Mid& mid2 = msg2.GetMid();

        // MID message #1
        {
            std::vector<Ipv4Address>& addresses = mid1.interfaceAddresses;
            addresses.clear();
            addresses.emplace_back("1.2.3.4");
            addresses.emplace_back("1.2.3.5");
        }

        msg1.SetTimeToLive(255);
        msg1.SetOriginatorAddress(Ipv4Address("11.22.33.44"));
        msg1.SetVTime(Seconds(9));
        msg1.SetMessageSequenceNumber(7);

        // MID message #2
        {
            std::vector<Ipv4Address>& addresses = mid2.interfaceAddresses;
            addresses.clear();
            addresses.emplace_back("2.2.3.4");
            addresses.emplace_back("2.2.3.5");
        }

        msg2.SetTimeToLive(254);
        msg2.SetOriginatorAddress(Ipv4Address("12.22.33.44"));
        msg2.SetVTime(Seconds(10));
        msg2.SetMessageType(olsr::MessageHeader::MID_MESSAGE);
        msg2.SetMessageSequenceNumber(7);

        // Build an OLSR packet header
        hdr.SetPacketLength(hdr.GetSerializedSize() + msg1.GetSerializedSize() +
                            msg2.GetSerializedSize());
        hdr.SetPacketSequenceNumber(123);

        // Now add all the headers in the correct order
        packet.AddHeader(msg2);
        packet.AddHeader(msg1);
        packet.AddHeader(hdr);
    }

    {
        olsr::PacketHeader hdr;
        packet.RemoveHeader(hdr);
        NS_TEST_ASSERT_MSG_EQ(hdr.GetPacketSequenceNumber(), 123, "200");
        uint32_t sizeLeft = hdr.GetPacketLength() - hdr.GetSerializedSize();
        {
            olsr::MessageHeader msg1;

            packet.RemoveHeader(msg1);

            NS_TEST_ASSERT_MSG_EQ(msg1.GetTimeToLive(), 255, "201");
            NS_TEST_ASSERT_MSG_EQ(msg1.GetOriginatorAddress(), Ipv4Address("11.22.33.44"), "202");
            NS_TEST_ASSERT_MSG_EQ(msg1.GetVTime(), Seconds(9), "203");
            NS_TEST_ASSERT_MSG_EQ(msg1.GetMessageType(), olsr::MessageHeader::MID_MESSAGE, "204");
            NS_TEST_ASSERT_MSG_EQ(msg1.GetMessageSequenceNumber(), 7, "205");

            olsr::MessageHeader::Mid& mid1 = msg1.GetMid();
            NS_TEST_ASSERT_MSG_EQ(mid1.interfaceAddresses.size(), 2, "206");
            NS_TEST_ASSERT_MSG_EQ(*mid1.interfaceAddresses.begin(), Ipv4Address("1.2.3.4"), "207");

            sizeLeft -= msg1.GetSerializedSize();
            NS_TEST_ASSERT_MSG_EQ((sizeLeft > 0), true, "208");
        }
        {
            // now read the second message
            olsr::MessageHeader msg2;

            packet.RemoveHeader(msg2);

            NS_TEST_ASSERT_MSG_EQ(msg2.GetTimeToLive(), 254, "209");
            NS_TEST_ASSERT_MSG_EQ(msg2.GetOriginatorAddress(), Ipv4Address("12.22.33.44"), "210");
            NS_TEST_ASSERT_MSG_EQ(msg2.GetVTime(), Seconds(10), "211");
            NS_TEST_ASSERT_MSG_EQ(msg2.GetMessageType(), olsr::MessageHeader::MID_MESSAGE, "212");
            NS_TEST_ASSERT_MSG_EQ(msg2.GetMessageSequenceNumber(), 7, "213");

            olsr::MessageHeader::Mid mid2 = msg2.GetMid();
            NS_TEST_ASSERT_MSG_EQ(mid2.interfaceAddresses.size(), 2, "214");
            NS_TEST_ASSERT_MSG_EQ(*mid2.interfaceAddresses.begin(), Ipv4Address("2.2.3.4"), "215");

            sizeLeft -= msg2.GetSerializedSize();
            NS_TEST_ASSERT_MSG_EQ(sizeLeft, 0, "216");
        }
    }
}

/**
 * @ingroup olsr-test
 * @ingroup tests
 *
 * Check Hello olsr messages
 */
class OlsrHelloTestCase : public TestCase
{
  public:
    OlsrHelloTestCase();
    void DoRun() override;
};

OlsrHelloTestCase::OlsrHelloTestCase()
    : TestCase("Check Hello olsr messages")
{
}

void
OlsrHelloTestCase::DoRun()
{
    Packet packet;
    olsr::MessageHeader msgIn;
    olsr::MessageHeader::Hello& helloIn = msgIn.GetHello();

    helloIn.SetHTime(Seconds(7));
    helloIn.willingness = olsr::Willingness::HIGH;

    {
        olsr::MessageHeader::Hello::LinkMessage lm1;
        lm1.linkCode = 2;
        lm1.neighborInterfaceAddresses.emplace_back("1.2.3.4");
        lm1.neighborInterfaceAddresses.emplace_back("1.2.3.5");
        helloIn.linkMessages.push_back(lm1);

        olsr::MessageHeader::Hello::LinkMessage lm2;
        lm2.linkCode = 3;
        lm2.neighborInterfaceAddresses.emplace_back("2.2.3.4");
        lm2.neighborInterfaceAddresses.emplace_back("2.2.3.5");
        helloIn.linkMessages.push_back(lm2);
    }

    packet.AddHeader(msgIn);

    olsr::MessageHeader msgOut;
    packet.RemoveHeader(msgOut);
    olsr::MessageHeader::Hello& helloOut = msgOut.GetHello();

    NS_TEST_ASSERT_MSG_EQ(helloOut.GetHTime(), Seconds(7), "300");
    NS_TEST_ASSERT_MSG_EQ(helloOut.willingness, olsr::Willingness::HIGH, "301");
    NS_TEST_ASSERT_MSG_EQ(helloOut.linkMessages.size(), 2, "302");

    NS_TEST_ASSERT_MSG_EQ(helloOut.linkMessages[0].linkCode, 2, "303");
    NS_TEST_ASSERT_MSG_EQ(helloOut.linkMessages[0].neighborInterfaceAddresses[0],
                          Ipv4Address("1.2.3.4"),
                          "304");
    NS_TEST_ASSERT_MSG_EQ(helloOut.linkMessages[0].neighborInterfaceAddresses[1],
                          Ipv4Address("1.2.3.5"),
                          "305");

    NS_TEST_ASSERT_MSG_EQ(helloOut.linkMessages[1].linkCode, 3, "306");
    NS_TEST_ASSERT_MSG_EQ(helloOut.linkMessages[1].neighborInterfaceAddresses[0],
                          Ipv4Address("2.2.3.4"),
                          "307");
    NS_TEST_ASSERT_MSG_EQ(helloOut.linkMessages[1].neighborInterfaceAddresses[1],
                          Ipv4Address("2.2.3.5"),
                          "308");

    NS_TEST_ASSERT_MSG_EQ(packet.GetSize(), 0, "All bytes in packet were not read");
}

/**
 * @ingroup olsr-test
 * @ingroup tests
 *
 * Check Tc olsr messages
 */
class OlsrTcTestCase : public TestCase
{
  public:
    OlsrTcTestCase();
    void DoRun() override;
};

OlsrTcTestCase::OlsrTcTestCase()
    : TestCase("Check Tc olsr messages")
{
}

void
OlsrTcTestCase::DoRun()
{
    Packet packet;
    olsr::MessageHeader msgIn;
    olsr::MessageHeader::Tc& tcIn = msgIn.GetTc();

    tcIn.ansn = 0x1234;
    tcIn.neighborAddresses.emplace_back("1.2.3.4");
    tcIn.neighborAddresses.emplace_back("1.2.3.5");
    packet.AddHeader(msgIn);

    olsr::MessageHeader msgOut;
    packet.RemoveHeader(msgOut);
    olsr::MessageHeader::Tc& tcOut = msgOut.GetTc();

    NS_TEST_ASSERT_MSG_EQ(tcOut.ansn, 0x1234, "400");
    NS_TEST_ASSERT_MSG_EQ(tcOut.neighborAddresses.size(), 2, "401");

    NS_TEST_ASSERT_MSG_EQ(tcOut.neighborAddresses[0], Ipv4Address("1.2.3.4"), "402");
    NS_TEST_ASSERT_MSG_EQ(tcOut.neighborAddresses[1], Ipv4Address("1.2.3.5"), "403");

    NS_TEST_ASSERT_MSG_EQ(packet.GetSize(), 0, "404");
}

/**
 * @ingroup olsr-test
 * @ingroup tests
 *
 * Check Hna olsr messages
 */
class OlsrHnaTestCase : public TestCase
{
  public:
    OlsrHnaTestCase();
    void DoRun() override;
};

OlsrHnaTestCase::OlsrHnaTestCase()
    : TestCase("Check Hna olsr messages")
{
}

void
OlsrHnaTestCase::DoRun()
{
    Packet packet;
    olsr::MessageHeader msgIn;
    olsr::MessageHeader::Hna& hnaIn = msgIn.GetHna();

    hnaIn.associations.push_back(
        olsr::MessageHeader::Hna::Association{Ipv4Address("1.2.3.4"), Ipv4Mask("255.255.255.0")});
    hnaIn.associations.push_back(
        olsr::MessageHeader::Hna::Association{Ipv4Address("1.2.3.5"), Ipv4Mask("255.255.0.0")});
    packet.AddHeader(msgIn);

    olsr::MessageHeader msgOut;
    packet.RemoveHeader(msgOut);
    olsr::MessageHeader::Hna& hnaOut = msgOut.GetHna();

    NS_TEST_ASSERT_MSG_EQ(hnaOut.associations.size(), 2, "500");

    NS_TEST_ASSERT_MSG_EQ(hnaOut.associations[0].address, Ipv4Address("1.2.3.4"), "501");
    NS_TEST_ASSERT_MSG_EQ(hnaOut.associations[0].mask, Ipv4Mask("255.255.255.0"), "502");

    NS_TEST_ASSERT_MSG_EQ(hnaOut.associations[1].address, Ipv4Address("1.2.3.5"), "503");
    NS_TEST_ASSERT_MSG_EQ(hnaOut.associations[1].mask, Ipv4Mask("255.255.0.0"), "504");

    NS_TEST_ASSERT_MSG_EQ(packet.GetSize(), 0, "All bytes in packet were not read");
}

/**
 * @ingroup olsr-test
 * @ingroup tests
 *
 * Check olsr header messages
 */
class OlsrTestSuite : public TestSuite
{
  public:
    OlsrTestSuite();
};

OlsrTestSuite::OlsrTestSuite()
    : TestSuite("routing-olsr-header", Type::UNIT)
{
    AddTestCase(new OlsrHnaTestCase(), TestCase::Duration::QUICK);
    AddTestCase(new OlsrTcTestCase(), TestCase::Duration::QUICK);
    AddTestCase(new OlsrHelloTestCase(), TestCase::Duration::QUICK);
    AddTestCase(new OlsrMidTestCase(), TestCase::Duration::QUICK);
    AddTestCase(new OlsrEmfTestCase(), TestCase::Duration::QUICK);
}

static OlsrTestSuite g_olsrTestSuite; //!< Static variable for test initialization
