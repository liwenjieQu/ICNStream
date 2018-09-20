/*
 * Author by Wenjie Li
 */

#ifndef NDN_LINKUTIL_TRACER_H
#define NDN_LINKUTIL_TRACER_H

#include "ns3/ptr.h"
#include "ns3/simple-ref-count.h"
#include <ns3/nstime.h>
#include <ns3/event-id.h>
#include <ns3/node-container.h>

#include <boost/tuple/tuple.hpp>
#include <boost/shared_ptr.hpp>

#include <string>

namespace ns3 {

class Node;

namespace ndn {

class Face;
class Interest;
class Data;

class LinkTracer : public SimpleRefCount<LinkTracer>
{
public:
	static void InstallAll (const std::string &file);

	static Ptr<LinkTracer>
	Install (Ptr<Node> node, boost::shared_ptr<std::ostream> outputStream);

	LinkTracer(boost::shared_ptr<std::ostream> os, Ptr<Node> node);

	LinkTracer (boost::shared_ptr<std::ostream> os, const std::string &node);

	~LinkTracer ();

	static void Destroy();

	void PrintHeader (std::ostream &os) const;

private:

  void Connect ();

  void OnLinkData(Ptr<const Data>, uint32_t, uint32_t, Time);

  std::string m_node;
  Ptr<Node> m_nodePtr;
  boost::shared_ptr<std::ostream> m_os;
};
}
}

#endif
