/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2011 UCLA
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Xiaoyan Hu <x......u@gmail.com>
 *         Alexander Afanasyev <alexander.afanasyev@ucla.edu>
 */

#include "ndn-cs-tracer.h"
#include "ns3/node.h"
#include "ns3/packet.h"
#include "ns3/config.h"
#include "ns3/names.h"
#include "ns3/callback.h"

#include "ns3/ndn-app.h"
#include "ns3/ndn-interest.h"
#include "ns3/ndn-data.h"
#include "ns3/ndn-content-store.h"
#include "ns3/simulator.h"
#include "ns3/node-list.h"
#include "ns3/log.h"

#include <boost/lexical_cast.hpp>

#include <fstream>
#include <string>
#include <exception>
#include <iomanip>

NS_LOG_COMPONENT_DEFINE ("ndn.CsTracer");

using namespace std;

namespace ns3 {
namespace ndn {

sql::Driver* CsTracer::driver = nullptr;
sql::Connection* CsTracer::con = nullptr;
sql::Statement* CsTracer::stmt = nullptr;
int CsTracer::insertitem;
std::string CsTracer::tablename;
int CsTracer::tableid;
bool CsTracer::usedfordestruction(false);

std::list< boost::tuple< boost::shared_ptr<std::ostream>, std::list<Ptr<CsTracer> > > > CsTracer::g_tracers;
std::list< boost::tuple< boost::shared_ptr<std::ostringstream>, std::list<Ptr<CsTracer> > > > CsTracer::g_tracers_ss;

template<class T>
static inline void
NullDeleter (T *ptr)
{
}

void
CsTracer::Destroy ()
{
  g_tracers.clear ();
  g_tracers_ss.clear();
}

void
CsTracer::InstallAll (const std::string &file, Time averagingPeriod/* = Seconds (0.5)*/)
{
  using namespace boost;
  using namespace std;
  
  std::list<Ptr<CsTracer> > tracers;
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
      Ptr<CsTracer> trace = Install (*node, outputStream, averagingPeriod);
      tracers.push_back (trace);
    }

  if (tracers.size () > 0)
    {
      // *m_l3RateTrace << "# "; // not necessary for R's read.table
      tracers.front ()->PrintHeader (*outputStream);
      *outputStream << "\n";
    }

  g_tracers.push_back (boost::make_tuple (outputStream, tracers));
}

void
CsTracer::Install (const NodeContainer &nodes, const std::string &file, Time averagingPeriod/* = Seconds (0.5)*/)
{
  using namespace boost;
  using namespace std;

  std::list<Ptr<CsTracer> > tracers;
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

  for (NodeContainer::Iterator node = nodes.Begin ();
       node != nodes.End ();
       node++)
    {
      Ptr<CsTracer> trace = Install (*node, outputStream, averagingPeriod);
      tracers.push_back (trace);
    }

  if (tracers.size () > 0)
    {
      // *m_l3RateTrace << "# "; // not necessary for R's read.table
      tracers.front ()->PrintHeader (*outputStream);
      *outputStream << "\n";
    }

  g_tracers.push_back (boost::make_tuple (outputStream, tracers));
}

void
CsTracer::Install (Ptr<Node> node, const std::string &file, Time averagingPeriod/* = Seconds (0.5)*/)
{
  using namespace boost;
  using namespace std;

  std::list<Ptr<CsTracer> > tracers;
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

  Ptr<CsTracer> trace = Install (node, outputStream, averagingPeriod);
  tracers.push_back (trace);

  if (tracers.size () > 0)
    {
      // *m_l3RateTrace << "# "; // not necessary for R's read.table
      tracers.front ()->PrintHeader (*outputStream);
      *outputStream << "\n";
    }

  g_tracers.push_back (boost::make_tuple (outputStream, tracers));
}

void
CsTracer::InstallAllSql (sql::Driver* d, const std::string& ip, const std::string& database,
		                 const std::string& table,
						 const std::string& user,
						 const std::string& password,
						 const int expidx,
						 Time averagingPeriod)
{
	using namespace boost;
	using namespace std;

	try{
		CsTracer::usedfordestruction = true;
		driver = d;
		/*Horus*/
		con = driver->connect("tcp://"+ip+":3306", user, password);
		con->setSchema(database);
		stmt = con->createStatement();

		tablename = table;
		tableid = expidx;
		stmt->execute("CREATE TABLE IF NOT EXISTS " + tablename + " (Time DECIMAL(12,6), Node SMALLINT, FileID SMALLINT, ChunkID SMALLINT, " +
				"`BitRate(Kbps)` VARCHAR(4), Packets SMALLINT, ExpID SMALLINT);");
		insertitem = 0;

	}
	catch (sql::SQLException &e) {
		cout << "# ERR: SQLException in " << __FILE__;
		cout << "(" << __FUNCTION__ << ") on line "
		     << __LINE__ << endl;
		cout << "# ERR: " << e.what();
		cout << " (MySQL error code: " << e.getErrorCode();
		cout << ", SQLState: " << e.getSQLState() << " )" << endl;
	}

	std::list<Ptr<CsTracer> > tracers;
	boost::shared_ptr<std::ostringstream> outputStream (new std::ostringstream());

	for (NodeList::Iterator node = NodeList::Begin (); node != NodeList::End (); node++)
	{
		Ptr<CsTracer> trace = InstallSql(*node, outputStream, averagingPeriod);
		tracers.push_back (trace);
	}
	g_tracers_ss.push_back (boost::make_tuple (outputStream, tracers));
}


Ptr<CsTracer>
CsTracer::Install (Ptr<Node> node,
                   boost::shared_ptr<std::ostream> outputStream,
                   Time averagingPeriod/* = Seconds (0.5)*/)
{
  NS_LOG_DEBUG ("Node: " << node->GetId ());

  Ptr<CsTracer> trace = Create<CsTracer> (outputStream, node);
  trace->SetAveragingPeriod (averagingPeriod);

  return trace;
}

Ptr<CsTracer>
CsTracer::InstallSql (Ptr<Node> node,
                   boost::shared_ptr<std::ostringstream> outputStream,
                   Time averagingPeriod/* = Seconds (0.5)*/)
{
  NS_LOG_DEBUG ("Node: " << node->GetId ());
  Ptr<CsTracer> trace = Create<CsTracer> (outputStream, node);
  trace->SetAveragingPeriod (averagingPeriod);

  return trace;
}
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

CsTracer::CsTracer (boost::shared_ptr<std::ostream> os, Ptr<Node> node)
: m_nodePtr (node)
, m_os (os)
, m_ss (nullptr)
{
  m_node = boost::lexical_cast<string> (m_nodePtr->GetId ());
  Connect ();
  string name = Names::FindName (node);
  if (!name.empty ())
    {
      m_node = name;
    }
}

CsTracer::CsTracer (boost::shared_ptr<std::ostringstream> os, Ptr<Node> node)
: m_nodePtr (node)
, m_os (nullptr)
, m_ss (os)
{
  m_node = boost::lexical_cast<string> (m_nodePtr->GetId ());
  Connect ();
  string name = Names::FindName (node);
  if (!name.empty ())
    {
      m_node = name;
    }
}


CsTracer::CsTracer (boost::shared_ptr<std::ostream> os, const std::string &node)
: m_node (node)
, m_os (os)
{
  Connect ();
}

CsTracer::~CsTracer ()
{
	if(insertitem != 0)
	{
		insertitem = 0;
		try{
			*m_ss << ";";

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
	if(CsTracer::usedfordestruction)
	{
		CsTracer::usedfordestruction = false;
		delete CsTracer::stmt;
		delete CsTracer::con;
	}
};


void
CsTracer::Connect ()
{
  Ptr<ContentStore> cs = m_nodePtr->GetObject<ContentStore> ();
  cs->TraceConnectWithoutContext ("CacheHits",   MakeCallback (&CsTracer::CacheHits,   this));
  cs->TraceConnectWithoutContext ("CacheMisses", MakeCallback (&CsTracer::CacheMisses, this));

  Reset ();  
}


void
CsTracer::SetAveragingPeriod (const Time &period)
{
  m_period = period;
  m_printEvent.Cancel ();
  m_printEvent = Simulator::Schedule (m_period, &CsTracer::PeriodicPrinter, this);
}

void
CsTracer::PeriodicPrinter ()
{
  Print (*m_os);
  Reset ();
  
  m_printEvent = Simulator::Schedule (m_period, &CsTracer::PeriodicPrinter, this);
}

void
CsTracer::PrintHeader (std::ostream &os) const
{
  os << "Time" << ","
     << "Node" << ","
     << "FileID" << ","
	 << "ChunkID" << ","
	 << "BitRate(Kbps)" << ","
     << "CacheHits" << ","
	 << "CacheMisses";
}

void
CsTracer::Reset ()
{
  m_stats.Reset();
  m_hits_detail.clear();
}

#define PRINTER(printName, fieldName)           \
  os << time.ToDouble (Time::S) << "\t"         \
  << m_node << "\t"                             \
  << printName << "\t"                          \
  << m_stats.fieldName << "\n";


void
CsTracer::Print (std::ostream &os) const
{
  Time time = Simulator::Now ();
  std::string tail = "kbps";

/*
  os << time.ToDouble (Time::S) << ","
	 << m_nodePtr->GetId() << ","
	 << 0 << ","
	 << 0 << ","
	 << 0 << ","
	 << m_stats.m_cacheMisses << "\n";
  os << time.ToDouble (Time::S) << ","
	 << m_nodePtr->GetId() << ","
	 << 0 << ","
	 << 0 << ","
	 << 1 << ","
	 << m_stats.m_cacheHits << "\n";
*/

  for(auto iter = m_hits_detail.begin(); iter != m_hits_detail.end(); iter++)
  {
	  if(m_os != nullptr)
	  {
		  std::size_t found = iter->first.m_bitrate.find(tail);
		  os << time.ToDouble (Time::S) << ","
			 << m_nodePtr->GetId() << ","
			 << iter->first.m_file << ","
			 << iter->first.m_chunk << ","
			 << std::stoi(iter->first.m_bitrate.substr(2, found - 2)) << ","
			 << iter->second << "\n";
	  }
	  else if(m_ss != nullptr)
	  {
		  std::size_t found = iter->first.m_bitrate.find(tail);
		  if(insertitem == 0)
			{
				*m_ss << "INSERT INTO " << tablename //<< " (Time,AppId,FileId,ChunkId,`BitRate(Kbps)`,`Throughput(Kbps)`,HopCount,Buffer,TableID)"
						<<" VALUES(" << std::setprecision(6) << Simulator::Now ().ToDouble (Time::S) << ","
				        << m_nodePtr->GetId() << ","
				        << iter->first.m_file << ","
						<< iter->first.m_chunk <<","
				        << std::stoi(iter->first.m_bitrate.substr(2, found - 2)) << ","
						<< iter->second << ","
						<< CsTracer::tableid << ")";
			}
		  else
			{
				*m_ss << ",(" << std::setprecision(6) << Simulator::Now ().ToDouble (Time::S) << ","
				        << m_nodePtr->GetId() << ","
				        << iter->first.m_file << ","
						<< iter->first.m_chunk <<","
				        << std::stoi(iter->first.m_bitrate.substr(2, found - 2)) << ","
						<< iter->second << ","
						<< CsTracer::tableid << ")";
			}
			insertitem++;
			if(insertitem == 50)
			{
				insertitem = 0;
				try{
					*m_ss << ";";

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
		  NS_LOG_WARN("Log error in CS Tracer !");
  }
}

void 
CsTracer::CacheHits (Ptr<const Interest> r, Ptr<const Data> d)
{
  m_stats.m_cacheHits ++;
  m_hits_detail[VideoIndex {r->GetName()}]++;
}

void 
CsTracer::CacheMisses (Ptr<const Interest>)
{
  m_stats.m_cacheMisses ++;
}


} // namespace ndn
} // namespace ns3
