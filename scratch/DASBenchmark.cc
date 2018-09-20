/*
 * Author: Wenjie Li
 * Date: 2016-12-07
 * Written for testing MARL cache partitioning
 */
#include <string>
#include <fstream>
#include <sstream>
#include <vector>
#include <sstream>
#include <cmath>
#include <iomanip>

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ndnSIM-module.h"
//#include "ns3/brite-module.h"
#include "ns3/name.h"

#include <boost/lexical_cast.hpp>

#include "mysql_connection.h"
#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/statement.h>
#include <cppconn/resultset.h>

using namespace ns3;
using namespace std;

//Basic Video Streaming Parameters
const uint32_t TotalBitrate = 4;
uint32_t TotalNode = 17;
// Duration: 4sec
const double m_chunksize[TotalBitrate] = {500, 1250, 2500, 4000};
double EdgeCacheSize;
double nonEdgeCacheSize;
const string myprefix = "/Adaptive-video"; // a single byte will dramatically change the cache partition?
const string m_bitratename[TotalBitrate] = {"1000kbps", "2500kbps", "5000kbps", "8000kbps"};

// Video Related Parameters
string TotalChunk = "50";
string TotalFile = "200";
string ProbCache = "0.98";

string Zipf = "0.8";
double CachePercentage = 0.1;
double CacheRatio = 1;

// Festive Related Parameters
string windowsize = "5";
string dropthres = "0.8";
string combineweight = "8";

uint32_t TotalUser = 4;
string LastmileBW = "20000Kbps";
double RequestRatePerUser = 0.0025;

string CacheMethod = "CE2";
string Replace = "Lfu";
double timein_ProbCache = 10;

string IP = "";
string DB = "";

uint32_t TopoChoice = 0;
uint32_t iterationTimes = 5;

double rewardParam = 1;
uint32_t rewardDesign = 1;//Default = 1(Legacy design since ICNP)

uint32_t ExpID = 0;
string MySQLUsername;
string MySqlPwd;
string triggertime = "40000s";
string roundtime = "15000s"; //Time period for each iteration
string totaltime = "55000s";//triggertime + roundtime;
uint32_t Seed = 1;

void CalculateCacheSize(string, string, uint32_t, uint32_t, double, double);
void InstallProtocol(ndn::StackHelper&, ndn::StackHelper&,
					 NodeContainer&, NodeContainer&);
void InstallConsumers(ndn::StackHelper&, ndn::AppHelper&, NodeContainer&);
NodeContainer CreateConsumerNodes(NodeContainer& EdgeNodes, uint32_t TotalUsers);

void SetupLastmileLink(Ptr<Node> a, Ptr<Node> b);

void DatabaseIndex(sql::Statement* stmt)
{
	try{

		stmt->execute(std::string("CREATE TABLE IF NOT EXISTS SimIndex(")
					+"ExpID SMALLINT,"
					+"CachePer DECIMAL(3,2),"
					+"CacheRatio DECIMAL(4,2),"
					+"Zipf DECIMAL(4,2),"
					+"TotalFile SMALLINT,"
					+"User SMALLINT,"
					+"CacheMethod VARCHAR(13),"
					+"Topology SMALLINT,"
					+"RewardParam DECIMAL(6,2),"
					+"DesignChoice SMALLINT,"
					+"Run SMALLINT);");
		ostringstream ss;

		ss << "INSERT INTO SimIndex VALUES(" << ExpID  << ","
							<< CachePercentage << ","
							<< CacheRatio << ","
							<< Zipf   << ","
							<< TotalFile << ","
							<< TotalUser << ",'"
							<< ((CacheMethod == "CE2" && Replace == "Lru") ? "CE2(LRU)" : CacheMethod) << "',"
							<< TopoChoice << ","
							<< rewardParam << ","
							<< rewardDesign << ","
							<< Seed <<");";
		stmt->execute(ss.str());
	}
	catch (sql::SQLException &e) {
		cout << "# ERR: SQLException in " << __FILE__;
		cout << "(" << __FUNCTION__ << ") on line "
		     << __LINE__ << endl;
		cout << "# ERR: " << e.what();
		cout << " (MySQL error code: " << e.getErrorCode();
		cout << ", SQLState: " << e.getSQLState() << " )" << endl;
	}

}

int main (int argc, char *argv[])
{
//	LogComponentEnable("ndn.cs.ContentStore", LOG_LEVEL_DEBUG);
//	LogComponentEnable("ndn.VideoClient", LOG_LEVEL_WARN);
//	LogComponentEnable("ndn.VideoProducer", LOG_LEVEL_INFO);
//	LogComponentEnable("ndn.fw", LOG_LEVEL_DEBUG);
//	LogComponentEnable("ndn.pit.Entry", LOG_LEVEL_DEBUG);
//	LogComponentEnable("ndn.cs.OLfu", LOG_LEVEL_DEBUG);
//	LogComponentEnable("ndn.cs.multisection.OLru", LOG_LEVEL_DEBUG);
//	LogComponentEnable("ndn.rl.marl.AggregatedState.AggregatedAction", LOG_LEVEL_DEBUG);
//	LogComponentEnable("ndn.MARLHelper", LOG_LEVEL_DEBUG);
//	LogComponentEnable("ndn.rl.agent.depedent", LOG_LEVEL_DEBUG);

	//LogComponentEnable("ndn.VideoTracer", LOG_LEVEL_WARN);
	//LogComponentEnable("ndn.Consumer", LOG_LEVEL_DEBUG);
	//LogComponentEnable("script.VideoClientTest", LOG_LEVEL_INFO);
	//LogComponentEnable("AnnotatedTopologyReader", LOG_LEVEL_INFO);
	//LogComponentEnable("ndn.GlobalRoutingHelper", LOG_LEVEL_INFO);
	//LogComponentEnable("ndn.VideoHeuristicHelper", LOG_LEVEL_INFO);
	//LogComponentEnable("ndn.VideoStatistics", LOG_LEVEL_DEBUG);
	//LogComponentEnable("ndn.VideoCacheDecision", LOG_LEVEL_DEBUG);
	//LogComponentEnable("ndn.PopularitySummary", LOG_LEVEL_DEBUG);
//	LogComponentEnable("ndn.cs.Nocache", LOG_LEVEL_DEBUG);

	CommandLine cmd;
	cmd.AddValue("TotalFile", "", TotalFile);
	cmd.AddValue("ChunkProfit", "The probability of watching the next video segment", ProbCache);
	cmd.AddValue("TotalNode", "", TotalNode);
	cmd.AddValue("CacheRatio", "The relative cache size of edge and nonedge routers", CacheRatio);
	cmd.AddValue("TriggerTime", "Set the clock since when the performance is measured", triggertime);

	cmd.AddValue("LastMileBandWidth", "", LastmileBW);
	cmd.AddValue("RequestRatePerUser", "", RequestRatePerUser);
	cmd.AddValue("TopologyChoice", "", TopoChoice);
	cmd.AddValue("Timein", "Parameters used to control ProbCache", timein_ProbCache);

	cmd.AddValue("Zipf_alpha", "Skewness parameter to control Zipf distribution", Zipf);
	cmd.AddValue("TotalChunk", "", TotalChunk);
	cmd.AddValue("CacheMethod", "", CacheMethod);
	cmd.AddValue("ReplaceApproach", "", Replace);

	cmd.AddValue("Run", "", Seed);
	cmd.AddValue("CachePercentage", "", CachePercentage);
	cmd.AddValue("RateDrop", "The drop threshold of rate control", dropthres);
	cmd.AddValue("CombineWeight", "The w", combineweight);
	cmd.AddValue("TotalUser", "The number of video consumers", TotalUser);

	cmd.AddValue("RewardParam", "Parameter used to adjust reward value", rewardParam);
	cmd.AddValue("DesignChoice", "Reward Design Pattern", rewardDesign);
	cmd.AddValue("IterationTimes", "The number of times of executing iterations (CPVB)", iterationTimes);

	cmd.AddValue("ExpID", "", ExpID);
	cmd.AddValue("Username", "", MySQLUsername);
	cmd.AddValue("Pwd", "", MySqlPwd);
	cmd.AddValue("IP", "MySQL IP Address", IP);
	cmd.AddValue("Database", "Database Name", DB);

	cmd.Parse (argc, argv);

	ns3::RngSeedManager::SetSeed(std::pow(Seed, 4));

	AnnotatedTopologyReader topologyReader("");

	std::string topopath = "./Topology/topology" + boost::lexical_cast<std::string>(TopoChoice) + ".txt";
	topologyReader.SetFileName(topopath);
	topologyReader.Read();

	NodeContainer EdgeNodes = topologyReader.GetEdgeNodes();
	NodeContainer IntmNodes = topologyReader.GetIntermediateNodes();
	Ptr<Node> ServerNode = topologyReader.GetServerNodes().Get(0);

	CalculateCacheSize(TotalChunk, TotalFile, TotalNode, EdgeNodes.size(), CachePercentage, CacheRatio);

	ndn::StackHelper Noncachedndnhelper;
	ndn::StackHelper nosection_e, nosection_i;

	if(CacheMethod != "StreamCache")
	{
		nosection_e.SetForwardingStrategy("ns3::ndn::fw::BestRoute");
		nosection_e.SetContentStore("ns3::ndn::cs::" + CacheMethod + "::" + Replace,
			"MaxSize", boost::lexical_cast<std::string>(static_cast<uint32_t>(EdgeCacheSize * 1e3)),
			"StartMeasureTime", triggertime,
			"TimeIn", std::to_string(timein_ProbCache),
			"RewardUnit", boost::lexical_cast<std::string>(rewardParam),
			"RewardDesign", boost::lexical_cast<std::string>(rewardDesign)
			);//in bytes
		nosection_e.SetPopSummary("ns3::ndn::PopularitySummary",
			"NumberofChunks", TotalChunk,
			"NumberOfContents", TotalFile);

		nosection_i.SetForwardingStrategy("ns3::ndn::fw::BestRoute");
		nosection_i.SetContentStore("ns3::ndn::cs::" + CacheMethod + "::" + Replace,
				"MaxSize", boost::lexical_cast<std::string>(static_cast<uint32_t>(nonEdgeCacheSize * 1e3)),
				"StartMeasureTime", triggertime,
				"TimeIn", std::to_string(timein_ProbCache),
				"RewardUnit", boost::lexical_cast<std::string>(rewardParam),
				"RewardDesign", boost::lexical_cast<std::string>(rewardDesign)
				);//in bytes
		nosection_i.SetPopSummary("ns3::ndn::PopularitySummary",
				"NumberofChunks", TotalChunk,
				"NumberOfContents", TotalFile);
	}
	else
	{
		nosection_e.SetForwardingStrategy("ns3::ndn::fw::BestRoute");
		nosection_e.SetContentStore("ns3::ndn::cs::AdptiveVideo::" + Replace,
				"MaxSize", boost::lexical_cast<std::string>(static_cast<uint32_t>(EdgeCacheSize * 1e3)),
				"StartMeasureTime", triggertime,
				"RewardUnit", boost::lexical_cast<std::string>(rewardParam),
				"RewardDesign", boost::lexical_cast<std::string>(rewardDesign)
				);//in bytes
		nosection_e.SetCacheDecision("ns3::ndn::VideoCacheDecision",
				"Prefix", myprefix
				);
		nosection_e.SetPopSummary("ns3::ndn::PopularitySummary",
				"NumberofChunks", TotalChunk,
				"NumberOfContents", TotalFile);

		nosection_i.SetForwardingStrategy("ns3::ndn::fw::BestRoute");
		nosection_i.SetContentStore("ns3::ndn::cs::AdptiveVideo::" + Replace,
						"MaxSize", boost::lexical_cast<std::string>(static_cast<uint32_t>(nonEdgeCacheSize * 1e3)),
						"StartMeasureTime", triggertime,
						"RewardUnit", boost::lexical_cast<std::string>(rewardParam),
						"RewardDesign", boost::lexical_cast<std::string>(rewardDesign)
						);//in bytes
		nosection_i.SetCacheDecision("ns3::ndn::VideoCacheDecision",
						"Prefix", myprefix
						);
		nosection_i.SetPopSummary("ns3::ndn::PopularitySummary",
						"NumberofChunks", TotalChunk,
						"NumberOfContents", TotalFile);
	}

	/* Configure Server and users */
	Noncachedndnhelper.SetForwardingStrategy("ns3::ndn::fw::BestRoute");
	Noncachedndnhelper.SetContentStore("ns3::ndn::cs::Nocache",
			"StartMeasureTime", triggertime,
			"RewardUnit", boost::lexical_cast<std::string>(rewardParam),
			"RewardDesign", boost::lexical_cast<std::string>(rewardDesign));
	Noncachedndnhelper.Install(ServerNode, m_bitratename, TotalBitrate, false);


	ndn::AppHelper producerHelper ("ns3::ndn::VideoProducer");
	producerHelper.SetPrefix (myprefix);
	producerHelper.SetAttribute("RewardUnit", DoubleValue(rewardParam));
	producerHelper.SetAttribute("RewardDesign", UintegerValue(rewardDesign));
	producerHelper.Install(ServerNode);

	ndn::AppHelper consumerHelper ("ns3::ndn::VideoClient");
	consumerHelper.SetPrefix(myprefix);
	consumerHelper.SetAttribute("NumberOfContents", StringValue(TotalFile));
	consumerHelper.SetAttribute("NumberofChunks", StringValue(TotalChunk));
	consumerHelper.SetAttribute("SkewnessFactor", StringValue(Zipf));
	consumerHelper.SetAttribute("ProbCache", StringValue(ProbCache));
	consumerHelper.SetAttribute("StartMeasureTime", StringValue(triggertime));
	consumerHelper.SetAttribute("HistoryWindowSize", StringValue(windowsize));
//	consumerHelper.SetAttribute("TargetBuffer", StringValue(targetbuff));
	consumerHelper.SetAttribute("CombineWeight", StringValue(combineweight));
	consumerHelper.SetAttribute("DropThreshold", StringValue(dropthres));
	if(CacheMethod == "StreamCache")
		consumerHelper.SetAttribute("NoCachePartition", BooleanValue(false));

	sql::Driver* driver = nullptr;
	driver = get_driver_instance();

	NodeContainer ConsumerNodes = CreateConsumerNodes(EdgeNodes, TotalUser);
	InstallProtocol(nosection_e, nosection_i, EdgeNodes, IntmNodes);

	/* Install Video application on clients */
	InstallConsumers(Noncachedndnhelper, consumerHelper, ConsumerNodes);

	ndn::DASHeuristicHelper AlgHelper(CacheMethod, myprefix, EdgeNodes, IntmNodes, roundtime, iterationTimes);
	Simulator::Schedule (Time(triggertime), &ndn::DASHeuristicHelper::Run, &AlgHelper);

	ndn::GlobalRoutingHelper ndnGlobalRoutingHelper;
	ndnGlobalRoutingHelper.InstallAll ();
	ndnGlobalRoutingHelper.AddOrigin(myprefix, ServerNode);
	ndnGlobalRoutingHelper.CalculateRoutes();


	sql::Connection* con = nullptr;
	sql::Statement* stmt = nullptr;
	sql::ConnectOptionsMap properties;
	properties["hostName"] = "tcp://" + IP;
	properties["userName"] = MySQLUsername;
	properties["password"] = MySqlPwd;
	int dbport = (IP == "127.0.0.1") ? 9001 : 3306;
	properties["port"] = dbport;
	properties["schema"] = DB;
	properties["OPT_RECONNECT"] = true;

	try{
		con = driver->connect(properties);
		//con->setSchema(DB);
		stmt = con->createStatement();
	}
	catch (sql::SQLException &e) {
		cout << "# ERR: SQLException in " << __FILE__;
		cout << "(" << __FUNCTION__ << ") on line "
		     << __LINE__ << endl;
		cout << "# ERR: " << e.what();
		cout << " (MySQL error code: " << e.getErrorCode();
		cout << ", SQLState: " << e.getSQLState() << " )" << endl;
	}

	ndn::VideoTracer::InstallAllSql(driver, IP, dbport, DB, "UserRequest", "BRSwitch", "", MySQLUsername, MySqlPwd,
									ExpID);
	DatabaseIndex(stmt);
	delete stmt;
	stmt = nullptr;

	if(CacheMethod != "StreamCache")
		Simulator::Stop (Time(totaltime));

	Simulator::Run ();
	Simulator::Destroy ();

	delete con;
	con = nullptr;
	return 0;
}

void CalculateCacheSize(string TotalChunk, string TotalFile, uint32_t TotalNode, uint32_t NumLeaf,
		double CachePercentage, double CacheRatio)
{
	double total = 0;
	for(uint32_t i = 0; i < TotalBitrate; i++)
		total += m_chunksize[i];
	total = total * boost::lexical_cast<uint32_t>(TotalChunk) * boost::lexical_cast<uint32_t>(TotalFile) * CachePercentage;

	nonEdgeCacheSize = total / (TotalNode - NumLeaf + NumLeaf * CacheRatio);
	EdgeCacheSize = CacheRatio * nonEdgeCacheSize;
}

void InstallProtocol(ndn::StackHelper& nosection_e,ndn::StackHelper& nosection_i,
					 NodeContainer& EdgeNodes, NodeContainer& IntmNodes)
{

	for(NodeContainer::Iterator i = EdgeNodes.Begin(); i != EdgeNodes.End(); i++)
	{
		if(CacheMethod == "StreamCache")
			nosection_e.StreamCacheInstall(*i, true, m_bitratename, TotalBitrate);
		else
			nosection_e.Install(*i, m_bitratename, TotalBitrate, false);
	}
	for(NodeContainer::Iterator i = IntmNodes.Begin(); i != IntmNodes.End(); i++)
	{
		if(CacheMethod == "StreamCache")
			nosection_i.StreamCacheInstall(*i, false, m_bitratename, TotalBitrate);
		else
			nosection_i.Install(*i, m_bitratename, TotalBitrate, false);
	}
}

//TODO: Set the request rate from users; Set up nodes which install apps
//Current version only used for debugging
void InstallConsumers(ndn::StackHelper& clientstackhelper, ndn::AppHelper& consumerHelper, NodeContainer& ConsumerNodes)
{//
	for(NodeContainer::Iterator i = ConsumerNodes.Begin(); i != ConsumerNodes.End(); i++)
	{
		clientstackhelper.Install(*i, m_bitratename, TotalBitrate, false);
		consumerHelper.SetAttribute("Frequency", DoubleValue(RequestRatePerUser));
		consumerHelper.Install(*i);
	}
}

NodeContainer CreateConsumerNodes(NodeContainer& EdgeNodes, uint32_t TotalUsers)
{
	NodeContainer consumers;

	for(NodeContainer::Iterator i = EdgeNodes.Begin(); i != EdgeNodes.End(); i++)
	{
		for(uint32_t j = 0; j < TotalUsers; j++)
		{
			Ptr<Node> client = CreateObject<Node>();
			SetupLastmileLink(*i, client);
			consumers.Add(client);
		}
	}
	return consumers;
}

void SetupLastmileLink(Ptr<Node> a, Ptr<Node> b)
{
	PointToPointHelper p2p;
	//setup the 'lastmile' link carefully!!!
	p2p.SetChannelAttribute("Delay", StringValue("0"));
	p2p.SetDeviceAttribute("DataRate", StringValue(LastmileBW));
	p2p.SetDeviceAttribute("Mtu", UintegerValue(1e8));
	p2p.Install(a, b);
}
