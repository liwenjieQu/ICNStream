/*
 * Author: Wenjie Li
 * Date: 2016
 */
#include "ndn-marl.h"
#include "ns3/log.h"
#include "ns3/traced-callback.h"
#include "ns3/ndn-content-store.h"
#include "ns3/ndn-bitrate.h"

#include "ns3/boolean.h"
#include "ns3/string.h"
#include "ns3/double.h"
#include "ns3/uinteger.h"

#include <set>
NS_LOG_COMPONENT_DEFINE ("ndn.rl.marl");

namespace ns3{
namespace ndn{

NS_OBJECT_ENSURE_REGISTERED (MARLnfa);

MARLnfa::MARLnfa()
{
	m_newstart = true;
	m_visits = 0;
}

void
MARLnfa::SetLogger(std::ofstream* f)
{
	m_log = f;
}

MARLnfa::~MARLnfa()
{

}

TypeId
MARLnfa::GetTypeId()
{
	static TypeId tid =
			TypeId(("ns3::ndn::rl"))
			.SetGroupName("Ndn")
			.SetParent<Object>()
			.AddConstructor<MARLnfa>()

			// m_experimentalAggregation
			.AddAttribute("ExperimentalAggregation", "Use Aggressive Aggregation on Neighborhood Actions",
						BooleanValue (false),
						MakeBooleanAccessor (&MARLnfa::m_experimentalAggregation),
						MakeBooleanChecker ())

			.AddAttribute("GranularityPlan", "Plan of setting State Granularity",
						StringValue("1"),
						MakeUintegerAccessor(&MARLnfa::m_granPlan),
						MakeUintegerChecker<uint32_t>())

			.AddAttribute("GuidedHeuristics", "",
						StringValue("0"),
						MakeUintegerAccessor(&MARLnfa::m_useGuidedHeuristics),
						MakeUintegerChecker<uint32_t>())
			;

	return tid;
}

void
MARLnfa::UpdateCurrentState()
{

}

bool
MARLnfa::UpdateCurrentAction(bool policy)
{
	return true;
}

bool
MARLnfa::Transition()
{
	return false;

}

bool
MARLnfa::ManualSetCurrentAction(int up, int down)
{
	return true;
}

void MARLnfa::DoDispose ()
{
	m_node = 0;
	m_cs = 0;

	delete [] m_csinitpartition;
	m_csinitpartition = nullptr;

	Object::DoDispose ();
}

void MARLnfa::NotifyNewAggregate ()
{
  if (m_node == 0)
  {
      m_node = GetObject<Node>();
      m_csinitpartition = new double [m_node->GetObject<NDNBitRate>()->GetTableSize()];
  }
  if (m_cs == 0)
  {
      m_cs = GetObject<ContentStore> ();
  }
  Object::NotifyNewAggregate ();
}

void MARLnfa::ResetToInit()
{
	if(m_cs != 0)
		m_cs->ResetReward();
}

std::size_t
MARLnfa::GetQStatus() const
{
	return 0;
}



}
}
