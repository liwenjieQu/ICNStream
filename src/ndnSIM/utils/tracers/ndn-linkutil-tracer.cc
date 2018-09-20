
#include "ndn-linkutil-tracer.h"

#include "ns3/node.h"
#include "ns3/packet.h"
#include "ns3/config.h"
#include "ns3/callback.h"
#include "ns3/names.h"
#include "ns3/ndn-face.h"
#include "ns3/ndn-interest.h"
#include "ns3/ndn-data.h"
#include "ns3/ndn-forwarding-strategy.h"

#include "ns3/simulator.h"
#include "ns3/node-list.h"
#include "ns3/log.h"

#include <boost/lexical_cast.hpp>
#include <boost/make_shared.hpp>

#include <fstream>
#include <iomanip>

NS_LOG_COMPONENT_DEFINE ("ndn.LinkUtilTracer");

using namespace std;

namespace ns3 {
namespace ndn {

static std::list< boost::tuple< boost::shared_ptr<std::ostream>, std::list<Ptr<LinkTracer> > > > g_tracers;

template<class T>
static inline void
NullDeleter (T *ptr)
{
}

void LinkTracer::Destroy()
{
	g_tracers.clear ();
}

void LinkTracer::InstallAll (const std::string &file)
{
  using namespace boost;
  using namespace std;

  std::list<Ptr<LinkTracer> > tracers;
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
      Ptr<LinkTracer> trace = Install (*node, outputStream);
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

Ptr<LinkTracer>
LinkTracer::Install (Ptr<Node> node, boost::shared_ptr<std::ostream> outputStream)
{
  NS_LOG_DEBUG ("Node: " << node->GetId ());

  Ptr<LinkTracer> trace = Create<LinkTracer> (outputStream, node);

  return trace;
}


LinkTracer::LinkTracer (boost::shared_ptr<std::ostream> os, Ptr<Node> node)
: m_nodePtr (node)
, m_os (os)
{
  m_node = boost::lexical_cast<string> (m_nodePtr->GetId ());

  Connect ();

  string name = Names::FindName (node);
  if (!name.empty ())
  {
      m_node = name;
  }
}

LinkTracer::LinkTracer (boost::shared_ptr<std::ostream> os, const std::string &node)
: m_node (node)
, m_os (os)
{
  Connect ();
}

LinkTracer::~LinkTracer ()
{
};

void LinkTracer::Connect()
{
	Ptr<ForwardingStrategy> fw = m_nodePtr->GetObject<ForwardingStrategy> ();
	fw->TraceConnectWithoutContext("LinkData", MakeCallback (&LinkTracer::OnLinkData, this));
}

void LinkTracer::PrintHeader (std::ostream &os) const
{
	os << std::setw(12) << "Time"
	   << std::setw(8) << "Source"
	   << std::setw(8) << "Des"
	   << std::setw(8) << "FileId"
	   << std::setw(8) << "ChunkId"
	   << std::setw(10) << "Size(B)"
	   << std::setw(12) << "Delay(ms)";

}

void LinkTracer::OnLinkData(Ptr<const Data> data, uint32_t sourceid, uint32_t desid, Time delay)
{
	*m_os << std::fixed;

	*m_os << std::setw(12) << std::setprecision(4) << Simulator::Now ().ToDouble (Time::S)
	      << std::setw(8) << sourceid
		  << std::setw(8) << desid
		  << std::setw(8) << data->GetName ().get (-2).toNumber()
		  << std::setw(8) << data->GetName ().get (-1).toNumber()
		  << std::setw(10) << data->GetPayload()->GetSize()
		  << std::setw(12) << std::setprecision(1) << delay.ToDouble(Time::MS) << std::endl;
}

}
}

