#include <ns3/core-module.h>
#include <ns3/network-module.h>
#include <ns3/internet-module.h>
#include <ns3/applications-module.h>
#include <ns3/csma-helper.h>
#include <ns3/mobility-module.h>
#include <ns3/nstime.h>
#include <ns3/point-to-point-module.h>
#include <ns3/ipv4-global-routing-helper.h>
#include <ns3/netanim-module.h>
#include <fstream>
#include <cstdlib>

#define UDP_SINK_PORT 9001
#define MAX_SIMULATION_TIME 10.0
#define NUMBER_OF_BOTS 50
#define NUMBER_OF_ATTACKERS 5

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("DDoS_Scattered");

std::map<Ipv4Address, int> attackCount;
std::ofstream csvOutput("scratch/ddos_dataset.csv");

void LogPacketInfo(Ptr<const Packet> packet, const Address &from)
{
    InetSocketAddress senderAddr = InetSocketAddress::ConvertFrom(from);
    Ipv4Address source = senderAddr.GetIpv4();
    uint16_t sourcePort = senderAddr.GetPort();

    std::string destIp = "10.0.0.2";
    std::string protocol = "UDP";
    uint16_t destPort = UDP_SINK_PORT;
    uint32_t packetSize = packet->GetSize();
    double time = Simulator::Now().GetSeconds();

    int phase = 0; // Before attack
    if (time >= 1.0 && time <= MAX_SIMULATION_TIME - 1)
    {
        if (attackCount[source] > 200)
            phase = 1; // During attack
    }
    else
    {
        phase = 2; // After attack
    }

    csvOutput << time << "," << source << "," << destIp << "," << protocol << ","
              << sourcePort << "," << destPort << "," << packetSize << "," << phase << std::endl;

    attackCount[source]++;
}

void SendToFLDDoS()
{
    std::ofstream attackFile("scratch/ddos_attack_data.txt");
    if (!attackFile.is_open())
    {
        std::cerr << "Error: Could not open file to write attack data." << std::endl;
        return;
    }
    for (const auto &entry : attackCount)
    {
        attackFile << entry.first << " " << entry.second << std::endl;
    }
    attackFile.close();

    // Run the Python FL script with the attack data file
    system("python3 scratch/fl.py scratch/ddos_attack_data.txt");
}

void CheckForDDoS()
{
    for (auto &entry : attackCount)
    {
        double rate = entry.second;
        if (rate > 200)
        {
            std::cout << "🚨 DDoS Attack Detected! Attacker: " << entry.first
                      << " - Rate: " << rate << " packets/sec" << std::endl;
        }
    }
    SendToFLDDoS();
    attackCount.clear();
    Simulator::Schedule(Seconds(1), &CheckForDDoS);
}

int main(int argc, char *argv[])
{
    CommandLine cmd;
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);

    NodeContainer nodes;
    nodes.Create(3);
    NodeContainer botNodes;
    botNodes.Create(NUMBER_OF_BOTS);

    PointToPointHelper pp;
    pp.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
    pp.SetChannelAttribute("Delay", StringValue("1ms"));

    NetDeviceContainer d02 = pp.Install(nodes.Get(0), nodes.Get(1));
    NetDeviceContainer d12 = pp.Install(nodes.Get(1), nodes.Get(2));

    InternetStackHelper stack;
    stack.Install(nodes);
    stack.Install(botNodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.0.0.0", "255.255.255.0");
    ipv4.Assign(d02);
    ipv4.Assign(d12);

    for (int i = 0; i < NUMBER_OF_BOTS; ++i)
    {
        NetDeviceContainer botDevice = pp.Install(botNodes.Get(i), nodes.Get(1));
        ipv4.Assign(botDevice);
        ipv4.NewNetwork();
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    PacketSinkHelper UDPsink("ns3::UdpSocketFactory",
                             Address(InetSocketAddress(Ipv4Address::GetAny(), UDP_SINK_PORT)));
    ApplicationContainer UDPSinkApp = UDPsink.Install(nodes.Get(2));
    UDPSinkApp.Start(Seconds(0.0));
    UDPSinkApp.Stop(Seconds(MAX_SIMULATION_TIME));
    UDPSinkApp.Get(0)->TraceConnectWithoutContext("Rx", MakeCallback(&LogPacketInfo));

    OnOffHelper ddosAttack("ns3::UdpSocketFactory",
                           Address(InetSocketAddress(Ipv4Address("10.0.0.2"), UDP_SINK_PORT)));
    ddosAttack.SetConstantRate(DataRate("20480kb/s"));

    ApplicationContainer attackerApps;
    for (int i = 0; i < NUMBER_OF_ATTACKERS; ++i)
    {
        attackerApps.Add(ddosAttack.Install(botNodes.Get(i)));
    }
    attackerApps.Start(Seconds(1.0));
    attackerApps.Stop(Seconds(MAX_SIMULATION_TIME - 1));

    OnOffHelper normalTraffic("ns3::UdpSocketFactory",
                              Address(InetSocketAddress(Ipv4Address("10.0.0.2"), UDP_SINK_PORT)));
    normalTraffic.SetConstantRate(DataRate("512kb/s"));

    ApplicationContainer normalApps;
    for (int i = NUMBER_OF_ATTACKERS; i < NUMBER_OF_BOTS; ++i)
    {
        normalApps.Add(normalTraffic.Install(botNodes.Get(i)));
    }
    normalApps.Start(Seconds(1.0));
    normalApps.Stop(Seconds(MAX_SIMULATION_TIME - 1));

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                  "X", StringValue("ns3::UniformRandomVariable[Min=0|Max=100]"),
                                  "Y", StringValue("ns3::UniformRandomVariable[Min=0|Max=100]"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);
    mobility.Install(botNodes);

    Simulator::Schedule(Seconds(1), &CheckForDDoS);

    csvOutput << "Time,SourceIP,DestIP,Protocol,SourcePort,DestPort,PacketSize,Phase" << std::endl;

    AnimationInterface anim("DDoS_Scattered.xml");

    Simulator::Run();
    Simulator::Destroy();

    csvOutput.close();

    return 0;
}
