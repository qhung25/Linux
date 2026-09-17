#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/error-model.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("LfnCaseStudy");

int main (int argc, char *argv[])
{
  std::string tcpVariant = "TcpCubic";
  double errorRate = 0.00001; // Tỉ lệ lỗi bit do nhiễu sóng
  double simulationTime = 50.0; // Chạy 50 giây để BBR có thời gian thăm dò

  CommandLine cmd;
  cmd.AddValue ("tcpVariant", "Giao thức TCP (TcpCubic hoặc TcpBbr)", tcpVariant);
  cmd.AddValue ("errorRate", "Tỉ lệ rớt gói do nhiễu", errorRate);
  cmd.Parse (argc, argv);

  // Thiết lập thuật toán TCP
  if (tcpVariant == "TcpBbr") {
    Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue (TcpBbr::GetTypeId ()));
  } else {
    Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue (TcpCubic::GetTypeId ()));
  }

  // Tăng kích thước Buffer để đáp ứng mạng Ống dài và to (LFN)
  // BDP = 1Gbps * 600ms = 75MB. Cấu hình buffer 100MB
  Config::SetDefault ("ns3::TcpSocket::RcvBufSize", UintegerValue (100000000));
  Config::SetDefault ("ns3::TcpSocket::SndBufSize", UintegerValue (100000000));

  NodeContainer nodes;
  nodes.Create (2);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("1Gbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("300ms")); // RTT = 600ms

  NetDeviceContainer devices = p2p.Install (nodes);

  // Thêm mô hình nhiễu vào thiết bị nhận (Node 1)
  Ptr<RateErrorModel> em = CreateObject<RateErrorModel> ();
  em->SetAttribute ("ErrorRate", DoubleValue (errorRate));
  em->SetAttribute ("ErrorUnit", StringValue ("ERROR_UNIT_PACKET"));
  devices.Get(1)->SetAttribute ("ReceiveErrorModel", PointerValue (em));

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  // Tạo Server nhận (Node 1)
  uint16_t port = 50000;
  Address sinkLocalAddress (InetSocketAddress (Ipv4Address::GetAny (), port));
  PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", sinkLocalAddress);
  ApplicationContainer sinkApp = sinkHelper.Install (nodes.Get (1));
  sinkApp.Start (Seconds (0.0));
  sinkApp.Stop (Seconds (simulationTime));

  // Tạo ứng dụng truyền file liên tục (Node 0)
  BulkSendHelper sourceHelper ("ns3::TcpSocketFactory", InetSocketAddress (interfaces.GetAddress (1), port));
  sourceHelper.SetAttribute ("MaxBytes", UintegerValue (0)); // Truyền vô hạn trong thời gian mô phỏng
  ApplicationContainer sourceApp = sourceHelper.Install (nodes.Get (0));
  sourceApp.Start (Seconds (1.0));
  sourceApp.Stop (Seconds (simulationTime - 1.0));

  // Kích hoạt xuất pcap để phân tích bằng Wireshark
  p2p.EnablePcapAll ("lfn-study");

  Simulator::Stop (Seconds (simulationTime));
  Simulator::Run ();

  // Tính toán và in ra Throughput
  Ptr<PacketSink> sink1 = DynamicCast<PacketSink> (sinkApp.Get (0));
  double totalBytes = sink1->GetTotalRx ();
  double throughput = (totalBytes * 8.0) / ((simulationTime - 2.0) * 1000000.0); // Mbps

  std::cout << "--- Kết quả Mô phỏng ---" << std::endl;
  std::cout << "Thuật toán TCP : " << tcpVariant << std::endl;
  std::cout << "Dữ liệu nhận   : " << totalBytes / (1024 * 1024) << " MB" << std::endl;
  std::cout << "Thông lượng    : " << throughput << " Mbps" << std::endl;

  Simulator::Destroy ();
  return 0;
}
