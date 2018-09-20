#include "ndn-app-video-tracer.h"
#include "ns3/node.h"
#include "ns3/packet.h"
#include "ns3/config.h"
#include "ns3/names.h"
#include "ns3/callback.h"

#include "ns3/ndn-app.h"
#include "ns3/ndn-interest.h"
#include "ns3/ndn-data.h"
#include "ns3/simulator.h"
#include "ns3/node-list.h"
#include "ns3/log.h"

#include <boost/lexical_cast.hpp>
#include <boost/make_shared.hpp>

#include <iomanip>
#include <iostream>
#include <exception>

NS_LOG_COMPONENT_DEFINE ("ndn.VideoTracer");

using namespace std;

namespace ns3 {
namespace ndn {


sql::Driver* VideoTracer::driver = nullptr;
sql::Connection* VideoTracer::con = nullptr;
sql::Statement* VideoTracer::stmt = nullptr;


int VideoTracer::insertitem_Request;
std::string VideoTracer::tablename_Request;
int VideoTracer::insertitem_Switch;
std::string VideoTracer::tablename_Switch;
std::string VideoTracer::tablename_Delay;
int VideoTracer::insertitem_Delay;
std::string VideoTracer::DatabaseName;


int VideoTracer::tableid;
bool VideoTracer::usedfordestruction(false);

std::list< boost::tuple< boost::shared_ptr<std::ostream>, std::list<Ptr<VideoTracer> > > > VideoTracer::g_tracers;
std::list< boost::tuple< boost::shared_ptr<std::ostringstream>,
						 boost::shared_ptr<std::ostringstream>,
						 boost::shared_ptr<std::ostringstream>,
						std::list<Ptr<VideoTracer> > > > VideoTracer::g_tracers_ss;

template<class T>
static inline void
NullDeleter (T *ptr)
{
}

void
VideoTracer::Destroy ()
{
  g_tracers.clear ();
  g_tracers_ss.clear();
}

void
VideoTracer::InstallAll (const std::string &file, bool needHeader)
{
  using namespace boost;
  using namespace std;

  std::list<Ptr<VideoTracer> > tracers;
  boost::shared_ptr<std::ostream> outputStream;
  if (file != "-")
    {
      boost::shared_ptr<std::ofstream> os (new std::ofstream ());
      os->open (file.c_str (), std::ios_base::out | std::ios_base::trunc);

      if (!os->is_open ())
        {
          NS_LOG_ERROR ("File " << file << " cannot be opened for writing. Tracing disabled");
          return;
        }

      outputStream = os;
    }
  else
    {
      outputStream = boost::shared_ptr<std::ostream> (&std::cout, NullDeleter<std::ostream>);
    }

  for (NodeList::Iterator node = NodeList::Begin ();
       node != NodeList::End ();
       node++)
    {
      Ptr<VideoTracer> trace = Install (*node, outputStream);
      tracers.push_back (trace);
    }

  if (tracers.size () > 0 && needHeader)
    {
      // *m_l3RateTrace << "# "; // not necessary for R's read.table
      tracers.front ()->PrintHeader (*outputStream);
      *outputStream << "\n";
    }

  g_tracers.push_back (boost::make_tuple (outputStream, tracers));
}

void
VideoTracer::InstallAllSql(sql::Driver* d, const std::string& ip, int port, const std::string& database,
		  	  	  	  	  const std::string& tableRequest,
						  const std::string& tableSwitch,
						  const std::string& tableDelay,
						  const std::string& user,
						  const std::string& password,
						  const int expidx)
{
	using namespace boost;
	using namespace std;

	try{
		DatabaseName = database;
		driver = d;
		VideoTracer::usedfordestruction = true;

		sql::ConnectOptionsMap properties;
		properties["hostName"] = "tcp://" + ip;
		properties["userName"] = user;
		properties["password"] = password;
		properties["port"] = port;
		properties["schema"] = DatabaseName;
		properties["OPT_RECONNECT"] = true;

		con = driver->connect(properties);
		stmt = con->createStatement();

		tablename_Request = tableRequest;
		tablename_Switch = tableSwitch;
		tablename_Delay = tableDelay;
		tableid = expidx;
		stmt->execute("CREATE TABLE IF NOT EXISTS " + tablename_Request + " (Transition SMALLINT, AppID SMALLINT, FileID SMALLINT, ChunkID SMALLINT, " +
				"`BitRate(Kbps)` VARCHAR(4), `NextBR(Kbps)` VARCHAR(4), Delay INT, Buffer DECIMAL(8,4), Reward DECIMAL(8,6), HopCount TINYINT, ExpID SMALLINT);");
		stmt->execute("CREATE TABLE IF NOT EXISTS " + tablename_Switch + " (Transition SMALLINT, AppID SMALLINT, FileID SMALLINT, " +
						"SwitchUp TINYINT, SwitchDown TINYINT, ExpID SMALLINT);");
		//stmt->execute("CREATE TABLE IF NOT EXISTS " + tablename_Delay + " (ExpID SMALLINT, Transition SMALLINT, NodeID SMALLINT, `BitRate(Kbps)` VARCHAR(4), " +
			//			"Hop1 INT, Hop2 INT, Hop3 INT, Hop4 INT, Hop5 INT, Hop6 INT, Hop7 INT, Hop8 INT, Hop9 INT);");
		insertitem_Request = 0;
		insertitem_Switch = 0;
		insertitem_Delay = 0;

	}
	catch (sql::SQLException &e) {
		cout << "# ERR: SQLException in " << __FILE__;
		cout << "(" << __FUNCTION__ << ") on line "
		     << __LINE__ << endl;
		cout << "# ERR: " << e.what();
		cout << " (MySQL error code: " << e.getErrorCode();
		cout << ", SQLState: " << e.getSQLState() << " )" << endl;
	}

	std::list<Ptr<VideoTracer> > tracers;
	boost::shared_ptr<std::ostringstream> OS_Request (new std::ostringstream());
	boost::shared_ptr<std::ostringstream> OS_Switch (new std::ostringstream());
	boost::shared_ptr<std::ostringstream> OS_Delay (new std::ostringstream());

	for (NodeList::Iterator node = NodeList::Begin (); node != NodeList::End (); node++)
	{
		Ptr<VideoTracer> trace = InstallSql(*node, OS_Request, OS_Switch, OS_Delay);
		tracers.push_back (trace);
	}
	g_tracers_ss.push_back (boost::make_tuple (OS_Request, OS_Switch, OS_Delay, tracers));
}


Ptr<VideoTracer>
VideoTracer::Install (Ptr<Node> node,
                         boost::shared_ptr<std::ostream> outputStream)
{
  NS_LOG_DEBUG ("Node: " << node->GetId ());
  Ptr<VideoTracer> trace = Create<VideoTracer> (outputStream, node);
  return trace;
}

Ptr<VideoTracer>
VideoTracer::InstallSql(Ptr<Node> node, boost::shared_ptr<std::ostringstream> OS_Request,
		boost::shared_ptr<std::ostringstream> OS_Switch,
		boost::shared_ptr<std::ostringstream> OS_Delay)
{
	NS_LOG_DEBUG ("Node: " << node->GetId ());
	Ptr<VideoTracer> trace = Create<VideoTracer> (OS_Request, OS_Switch, OS_Delay, node);
	return trace;
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

VideoTracer::VideoTracer (boost::shared_ptr<std::ostream> os, Ptr<Node> node)
: m_nodePtr (node)
, m_os (os)
, m_ss (0)
, m_ss_switch (0)
, m_ss_delay(0)
{
  m_node = boost::lexical_cast<string> (m_nodePtr->GetId ());
  Connect ();
  string name = Names::FindName (node);
  if (!name.empty ())
  {
      m_node = name;
  }
}

VideoTracer::VideoTracer (boost::shared_ptr<std::ostringstream> os_req,
		boost::shared_ptr<std::ostringstream> os_swi,
		boost::shared_ptr<std::ostringstream> os_delay,
		Ptr<Node> node)
: m_nodePtr (node)
, m_os (0)
, m_ss (os_req)
, m_ss_switch(os_swi)
, m_ss_delay (os_delay)
{
  m_node = boost::lexical_cast<string> (m_nodePtr->GetId ());
  Connect ();
  string name = Names::FindName (node);
  if (!name.empty ())
  {
      m_node = name;
  }
}

VideoTracer::VideoTracer (boost::shared_ptr<std::ostream> os, const std::string &node)
: m_node (node)
, m_os (os)
, m_ss (0)
{
  Connect ();
}

VideoTracer::~VideoTracer ()
{
	if(insertitem_Request != 0)
	{
		insertitem_Request = 0;
		try{
			*m_ss << ";";
			if(!con->isValid())
			{
				delete VideoTracer::stmt;
				stmt = nullptr;
				con->reconnect();
				con->setSchema(DatabaseName);
				stmt = con->createStatement();
			}
			stmt->execute(m_ss->str());
		} catch (sql::SQLException &e) {
			std::cout << "# ERR: SQLException in " << __FILE__;
			std::cout << "(" << __FUNCTION__ << ") on line "
			     << __LINE__ << endl;
			std::cout << "# ERR: " << e.what();
			std::cout << " (MySQL error code: " << e.getErrorCode();
			std::cout << ", SQLState: " << e.getSQLState() << " )" << std::endl;
		}
		m_ss->clear();
		m_ss->str("");
	}
	if(insertitem_Switch != 0)
	{
		insertitem_Switch = 0;
		try{
			*m_ss_switch << ";";
			if(!con->isValid())
			{
				delete VideoTracer::stmt;
				stmt = nullptr;
				con->reconnect();
				con->setSchema(DatabaseName);
				stmt = con->createStatement();
			}
			stmt->execute(m_ss_switch->str());
		} catch (sql::SQLException &e) {
			std::cout << "# ERR: SQLException in " << __FILE__;
			std::cout << "(" << __FUNCTION__ << ") on line "
			     << __LINE__ << endl;
			std::cout << "# ERR: " << e.what();
			std::cout << " (MySQL error code: " << e.getErrorCode();
			std::cout << ", SQLState: " << e.getSQLState() << " )" << std::endl;
		}
		m_ss_switch->clear();
		m_ss_switch->str("");
	}
	if(insertitem_Delay != 0)
	{
		insertitem_Delay = 0;
		try{
			*m_ss_delay << ";";
			if(!con->isValid())
			{
				delete VideoTracer::stmt;
				stmt = nullptr;
				con->reconnect();
				con->setSchema(DatabaseName);
				stmt = con->createStatement();
			}
			stmt->execute(m_ss_delay->str());
		} catch (sql::SQLException &e) {
			std::cout << "# ERR: SQLException in " << __FILE__;
			std::cout << "(" << __FUNCTION__ << ") on line "
			     << __LINE__ << endl;
			std::cout << "# ERR: " << e.what();
			std::cout << " (MySQL error code: " << e.getErrorCode();
			std::cout << ", SQLState: " << e.getSQLState() << " )" << std::endl;
		}
		m_ss_delay->clear();
		m_ss_delay->str("");
	}
	if(VideoTracer::usedfordestruction)
	{
		VideoTracer::usedfordestruction = false;
		delete VideoTracer::stmt;
		delete VideoTracer::con;

		stmt = nullptr;
		con = nullptr;
	}
};


void
VideoTracer::Connect ()
{
  Config::ConnectWithoutContext ("/NodeList/"+m_node+"/ApplicationList/*/VideoPlaybackStatus",
                                 MakeCallback (&VideoTracer::VideoPlayTrace, this));
  Config::ConnectWithoutContext ("/NodeList/"+m_node+"/ApplicationList/*/VideoSwitch",
                                 MakeCallback (&VideoTracer::VideoSwitchTrace, this));
  //Config::ConnectWithoutContext ("/NodeList/"+m_node+"/ApplicationList/*/DelaybyHop",
    //                             MakeCallback (&VideoTracer::QDelayByHop, this));
}

/*
 * Write to a csv file format
 */
void
VideoTracer::PrintHeader (std::ostream &os) const
{
  os << "Time,"
     << "FileId,"
	 << "ChunkId,"
	 << "BitRate(Kbps),"
	 << "Throughput(Kbps),"
     //<< "DelayMS,"
	 << "HopCount,"
	 << "Buffer,"
	 << "SwitchUp,"
	 << "SwitchDown";
}

void
VideoTracer::VideoPlayTrace (std::tuple<uint32_t, uint32_t, uint32_t>id,
							std::string bitrate,
							std::string next_bitrate,
							int64_t delay,
							double buffer,
							double reward,
							uint32_t hopcount,
							//uint32_t rank,
							uint32_t tranPhase)
{
	std::string tail = "kbps";
	std::size_t found = bitrate.find(tail);
	std::size_t foundnext = next_bitrate.find(tail);

	if(m_os != nullptr)
	{
		*m_os << tranPhase << ","
			//<< std::setprecision(6) << reqTime.ToDouble (Time::S) << ","
			<< std::get<0>(id) << ","
			<< std::get<1>(id) << ","
			<< std::get<2>(id) << ","
	        << std::stoi(bitrate.substr(0, found)) << ","
			<< std::stoi(next_bitrate.substr(0, foundnext)) << ","
			<< delay << ","
	        //<< delay.ToDouble (Time::MS) << ","
	        << std::setprecision(4) << buffer << ","
			<< std::setprecision(4) << reward << ","
			<< hopcount << "\n";
	}
	else if(m_ss != nullptr)
	{
		if(insertitem_Request == 0)
		{
			*m_ss << "INSERT INTO " << tablename_Request //<< " (Time,AppId,FileId,ChunkId,`BitRate(Kbps)`,`Throughput(Kbps)`,HopCount,Buffer,TableID)"
					<<" VALUES(" << tranPhase << ","
					//<< std::setprecision(6) << reqTime.ToDouble (Time::S) << ","
					<< std::get<0>(id) << ","
					<< std::get<1>(id) << ","
					<< std::get<2>(id) << ","
			        << std::stoi(bitrate.substr(0, found)) << ","
					<< std::stoi(next_bitrate.substr(0, foundnext)) << ","
					<< delay << ","
			        //<< delay.ToDouble (Time::MS) << ","
			        << std::setprecision(4) << buffer << ","
					<< std::setprecision(4) << reward << ","
					<< hopcount << ","
					//<< rank << ","
					<< VideoTracer::tableid << ")";
		}
		else
		{
			*m_ss << ",(" << tranPhase << ","
					//<< std::setprecision(6) << reqTime.ToDouble (Time::S) << ","
					<< std::get<0>(id) << ","
					<< std::get<1>(id) << ","
					<< std::get<2>(id) << ","
			        << std::stoi(bitrate.substr(0, found)) << ","
					<< std::stoi(next_bitrate.substr(0, foundnext)) << ","
					<< delay << ","
			        //<< delay.ToDouble (Time::MS) << ","
			        << std::setprecision(4) << buffer << ","
					<< std::setprecision(4) << reward << ","
					<< hopcount << ","
					//<< rank << ","
					<< VideoTracer::tableid << ")";
		}
		insertitem_Request++;
		if(insertitem_Request >= 3000)
		{
			insertitem_Request = 0;
			try{
				*m_ss << ";";
				if(!con->isValid())
				{
					delete VideoTracer::stmt;
					stmt = nullptr;
					con->reconnect();
					con->setSchema(DatabaseName);
					stmt = con->createStatement();
				}
				stmt->execute(m_ss->str());
			} catch (sql::SQLException &e) {
				std::cout << "# ERR: SQLException in " << __FILE__;
				std::cout << "(" << __FUNCTION__ << ") on line "
				     << __LINE__ << endl;
				std::cout << "# ERR: " << e.what();
				std::cout << " (MySQL error code: " << e.getErrorCode();
				std::cout << ", SQLState: " << e.getSQLState() << " )" << std::endl;
			}
			m_ss->clear();
			m_ss->str("");
		}
	}
	else
	{
		NS_LOG_WARN("Log error in Video Tracer !");
	}
}

void
VideoTracer::VideoSwitchTrace(Ptr<App> app, uint32_t appid, uint32_t fileid, uint32_t switchup, uint32_t switchdown, uint32_t tranPhase)
{
	if(m_ss_switch != nullptr)
	{
		if(insertitem_Switch == 0)
		{
			*m_ss_switch << "INSERT INTO " << tablename_Switch //<< " (Time,AppId,FileId,SwitchUp,SwitchDown,TableID)"
					<<" VALUES("
					<< tranPhase << ","
					<< appid << ","
					<< fileid <<","
			        << switchup << ","
					<< switchdown << ","
					<< VideoTracer::tableid << ")";
		}
		else
		{
			*m_ss_switch << ",("
					<< tranPhase << ","
					<< appid << ","
					<< fileid <<","
			        << switchup << ","
					<< switchdown << ","
					<< VideoTracer::tableid << ")";
		}
		insertitem_Switch++;
		if(insertitem_Switch >= 1000)
		{
			insertitem_Switch = 0;
			try{
				*m_ss_switch << ";";
				if(!con->isValid())
				{
					delete VideoTracer::stmt;
					stmt = nullptr;
					con->reconnect();
					con->setSchema(DatabaseName);
					stmt = con->createStatement();
				}
				stmt->execute(m_ss_switch->str());
			} catch (sql::SQLException &e) {
				std::cout << "# ERR: SQLException in " << __FILE__;
				std::cout << "(" << __FUNCTION__ << ") on line "
				     << __LINE__ << endl;
				std::cout << "# ERR: " << e.what();
				std::cout << " (MySQL error code: " << e.getErrorCode();
				std::cout << ", SQLState: " << e.getSQLState() << " )" << std::endl;
			}
			m_ss_switch->clear();
			m_ss_switch->str("");
		}
	}
}

void
VideoTracer::QDelayByHop(Ptr<const Data> data, std::string br, uint32_t tranPhase)
{
	std::string tail = "kbps";
	std::size_t found = br.find(tail);
	uint8_t len = data->GetHops();
	if(m_ss_delay != nullptr)
	{
		if(insertitem_Delay == 0)
		{
			*m_ss_delay << "INSERT INTO " << tablename_Delay //<< " (TableID,Time,NodeID,BitRate,Hop2, Hop3, ...)"
					<<" VALUES(" << VideoTracer::tableid<<","
					<< tranPhase << ","
					<< m_nodePtr->GetId() << ","
					<< std::stoi(br.substr(0, found)) << ",";
		}
		else
		{
			*m_ss_delay << ",(" << VideoTracer::tableid<<","
					<< tranPhase << ","
					<< m_nodePtr->GetId() << ","
					<< std::stoi(br.substr(0, found)) << ",";
		}
		if(len > 9)
		{
			NS_LOG_ERROR("Too many hops in the simulation. Update MySQL Table");
		}
		else
		{
			if(len == 1)
				*m_ss_delay << data->m_mostRecentDelay.ToInteger(Time::MS);
			else
			{
				*m_ss_delay << data->m_mostRecentDelay.ToInteger(Time::MS) << ",";
				for(uint8_t j = 2; j < len; j++)
					*m_ss_delay << data->GetDelay(j - 2) << ",";
				*m_ss_delay << data->GetDelay(len - 2);
			}
			if(len == 9)
				*m_ss_delay << ")";
			else
			{
				*m_ss_delay << ",";
				for(uint8_t j = len + 1; j < 9; j++)
					*m_ss_delay << "NULL" << ",";
				*m_ss_delay << "NULL" << ")";
			}
		}
		insertitem_Delay++;
		if(insertitem_Delay >= 200)
		{
			insertitem_Delay = 0;
			try{
				*m_ss_delay << ";";
				if(!con->isValid())
				{
					delete VideoTracer::stmt;
					stmt = nullptr;
					con->reconnect();
					con->setSchema(DatabaseName);
					stmt = con->createStatement();
				}
				stmt->execute(m_ss_delay->str());
			} catch (sql::SQLException &e) {
				std::cout << "# ERR: SQLException in " << __FILE__;
				std::cout << "(" << __FUNCTION__ << ") on line "
				     << __LINE__ << endl;
				std::cout << "# ERR: " << e.what();
				std::cout << " (MySQL error code: " << e.getErrorCode();
				std::cout << ", SQLState: " << e.getSQLState() << " )" << std::endl;
			}
			m_ss_delay->clear();
			m_ss_delay->str("");
		}
	}
}


} // namespace ndn
} // namespace ns3
