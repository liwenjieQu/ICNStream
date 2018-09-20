/*
 *
 * Author: Wenjie Li <liwenjie@cs.queensu.ca>
 * ContentStore implementation to separate contents in the cache.
 * Ideal in the scenario of priority queue or cache multisection for bitrates.
 *
 * Created on 2016-07-13
 */

#ifndef NDN_CONTENT_STORE_WITH_MULTISECTION_H
#define NDN_CONTENT_STORE_WITH_MULTISECTION_H

#include "ndn-content-store.h"
#include "ns3/packet.h"
#include "ns3/ndn-interest.h"
#include "ns3/ndn-data.h"
#include "ns3/random-variable.h"

#include "content-store-impl.h"

#include <boost/foreach.hpp>
#include <iterator>
#include <string>
#include <map>
#include <cmath>
#include <memory>

#include "ns3/log.h"
#include "ns3/uinteger.h"

#include "../../utils/trie/trie-with-policy-multisection.h"
using namespace std;

namespace ns3 {
namespace ndn {
namespace cs {

template<class Policy>
class ContentStoreMulSec: public ContentStore, protected ndnSIM::trie_with_policy_multisection<
		Name,
		ndnSIM::smart_pointer_payload_traits<
				EntryImpl<ContentStoreMulSec<Policy> >, Entry>, Policy> {
public:
	typedef ndnSIM::trie_with_policy_multisection<Name,
			ndnSIM::smart_pointer_payload_traits<
					EntryImpl<ContentStoreMulSec<Policy> >, Entry>, Policy> super;

	typedef EntryImpl<ContentStoreMulSec<Policy> > entry;

	static TypeId
	GetTypeId();

	ContentStoreMulSec(){

	};

	virtual ~ContentStoreMulSec() {

	};

	// from ContentStore
	virtual inline Ptr<Data>
	Lookup(Ptr<const Interest> interest);

	virtual inline Ptr<Data>
	Lookup(Ptr<const Interest> interest, double& r);

	virtual inline bool
	Add(Ptr<const Data> data);

	virtual inline void
	Print(std::ostream &os) const;

	virtual Ptr<Entry>
	Begin();

	virtual Ptr<Entry>
	End();

	virtual Ptr<Entry>
	Next(Ptr<Entry>);

	virtual
	void FillinCacheRun();

	virtual inline uint32_t
	GetSize() const
	{
		return 0;
	};//legacy code. For compatibility.

	virtual uint32_t GetCapacity(); // size limit

	virtual uint32_t GetCapacity(const string& s);



	///////////////////////////////////////
	////////Multisection//////////////////
	//////////////////////////////////////
	virtual void
	AdjustSectionRatio(const std::string&, const std::string&, double = 0.05);


	virtual bool
	SetSectionRatio(const std::string[], double[], size_t);

	virtual bool
	SetSectionRatio(double[], size_t);

	virtual void
	InitContentStore(const std::string[], size_t);

	virtual double
	GetReward();

	virtual double
	GetSectionRatio(const std::string&);

	virtual void
	ClearCachePartition();

	virtual void
	ReportPartitionStatus();

protected:

	void
	SetMaxSize(uint32_t maxSize);

	uint32_t
	GetMaxSize() const;

	void
	AdjustMaintanenceList(const std::string& BR, uint32_t newsize);

protected:
	/// @brief trace of for entry additions (fired every time entry is successfully added to the cache): first parameter is pointer to the CS entry
	TracedCallback<Ptr<const Entry> > 	m_didAddEntry;

private:
	void  RatioTest();

	static LogComponent g_log; ///< @brief Logging variable

	/*
	 * Added by Wenjie
	 * To implement
	 */
	uint32_t 			m_totalsize; 		// Total size of cache regardless of bitrates (in bytes)
	uint32_t 			m_numBitrates;		// Number of bitrates considered in the caching system
	std::map<std::string, double> m_BRper; 		// The section percentage for different bitrates

};

//////////////////////////////////////////
////////// Implementation ////////////////
//////////////////////////////////////////

template<class Policy>
LogComponent ContentStoreMulSec<Policy>::g_log = LogComponent(
		("ndn.cs.multisection." + Policy::GetName()).c_str());

template<class Policy>
TypeId ContentStoreMulSec<Policy>::GetTypeId() {
	static TypeId tid =
			TypeId(("ns3::ndn::cs::multisection::" + Policy::GetName()).c_str())
			.SetGroupName("Ndn")
			.SetParent<ContentStore>()
			.AddConstructor<ContentStoreMulSec<Policy> >()

			.AddAttribute("MaxSize",
					"Set maximum number of bytes in ContentStore. If 0, limit is not enforced",
					StringValue("100"),
					MakeUintegerAccessor(&ContentStoreMulSec<Policy>::GetMaxSize,
							&ContentStoreMulSec<Policy>::SetMaxSize),
					MakeUintegerChecker<uint32_t>())


			.AddTraceSource("DidAddEntry",
					"Trace fired every time entry is successfully added to the cache",
					MakeTraceSourceAccessor(&ContentStoreMulSec<Policy>::m_didAddEntry));

	return tid;
}
template <typename Policy>
double ContentStoreMulSec<Policy>::GetSectionRatio(const std::string& BR)
{
	return m_BRper.at(BR);
}
template<class Policy>
double ContentStoreMulSec<Policy>::GetReward()
{
	double r = this->m_reward * 1e3 / m_numreq; // Average based on Request Rate
	return r;
}

template<class Policy>
void ContentStoreMulSec<Policy>::AdjustSectionRatio(const std::string& inc, const std::string& dec, double granularity)
{
	m_BRper[inc] += granularity;
	m_BRper[dec] -= granularity;

	uint32_t s = static_cast<uint32_t>(m_BRper[inc] * m_totalsize);
	AdjustMaintanenceList(inc, s);

	s = static_cast<uint32_t>(m_BRper[dec] * m_totalsize);
	AdjustMaintanenceList(dec, s);
}


template<class Policy>
bool ContentStoreMulSec<Policy>::SetSectionRatio(const std::string BR[], double ratio[], size_t num)
{
	if(num != m_numBitrates)
		return false;
	for(uint32_t i = 0; i < num; i++)
	{
		m_BRper[BR[i]] = ratio[i];
		AdjustMaintanenceList(BR[i], static_cast<uint32_t>(ratio[i] * m_totalsize));
	}
	return true;
}

template<class Policy>
bool ContentStoreMulSec<Policy>::SetSectionRatio(double ratio[], size_t num)
{
	if(num != m_numBitrates)
		return false;
	for(uint32_t i = 0; i < num; i++)
	{
		m_BRper[m_BRinfo->GetBRFromRank(i + 1)] = ratio[i];
		AdjustMaintanenceList(m_BRinfo->GetBRFromRank(i + 1), static_cast<uint32_t>(ratio[i] * m_totalsize));
	}
	return true;
}


template<class Policy>
void ContentStoreMulSec<Policy>::InitContentStore(const std::string BR[], size_t numBitrates)
{
	this->m_reward = 0;

	this->initMaintenanceLists(BR, numBitrates);
	m_numBitrates = numBitrates;
	double *initratio = new double [numBitrates];
	for (uint32_t i = 0; i < numBitrates; i++)
		initratio[i] = 1 / static_cast<double>(numBitrates);
	SetSectionRatio(BR, initratio, numBitrates);
	delete [] initratio;
	initratio = nullptr;

	//Simulator::Schedule("200s", &(typename ContentStoreMulSec<Policy>::RatioTest), this);
}
template<typename Policy>
void ContentStoreMulSec<Policy>::RatioTest()
{
	/*
	 * Only for Debugging
	 * Test the cache ration in contentstore
	 *
	 */
	/////////////////////////////////////////////////////
	  if(m_node->GetId() == 16)//Test a random router
	  {
		  for(uint32_t i = 1; i <= m_BRinfo->GetTableSize(); i++)
		  {
			  typename super::policy_container::const_iterator item = this->getPolicy(m_BRinfo->GetBRFromRank(i)).begin();
			  NS_LOG_INFO("Node: " << m_node->GetId() << "\tBitrate: " << m_BRinfo->GetBRFromRank(i));
			  for (; item != this->getPolicy(m_BRinfo->GetBRFromRank(i)).end();
					item++) {
				  NS_LOG_INFO(item->payload()->GetName());
			  }
		  }
	  }
}
template<typename Policy>
void ContentStoreMulSec<Policy>::ReportPartitionStatus()
{
	if(m_noCachePartition)
	{
		if(Simulator::Now() > m_period)
		{
			uint32_t numBitRate = m_BRinfo->GetTableSize();
			std::shared_ptr<double> sp(new double [numBitRate],
						std::default_delete<double[]>());
			FillinCacheRun();
			std::string br;
			double sum = 0;
			for(uint32_t i = 0; i < numBitRate; i++)
				sp.get()[i] = 0;

			for(auto iter = inCacheRun.begin(); iter != inCacheRun.end(); iter++)
			{
				br = ExtractBitrate(*iter);
				uint32_t idx = m_BRinfo->GetRankFromBR(br);
				sp.get()[idx - 1] += m_BRinfo->GetChunkSize(br);
				sum += m_BRinfo->GetChunkSize(br);
			}

			for(uint32_t i = 0; i < numBitRate; i++)
				sp.get()[i] = sp.get()[i] / sum;
			m_cachePartitionTrace(m_transition, m_reward*1e3/m_numreq, sp);
			m_transition++;
		}
	}
}

template<class Policy>
void ContentStoreMulSec<Policy>::ClearCachePartition()
{
	super::clear();
}

template<class Policy>
void ContentStoreMulSec<Policy>::AdjustMaintanenceList(const std::string& BR, uint32_t newsize)
{
	this->getPolicy(BR).set_max_size(newsize);
}

template<class Policy>
Ptr<Data> ContentStoreMulSec<Policy>::Lookup(Ptr<const Interest> interest) {
	NS_LOG_FUNCTION(this << interest->GetName ());

	NS_LOG_INFO("ContentStoreMulSec::Lookup(Interest) should not be called!");
	return 0;
}

template<class Policy>
Ptr<Data> ContentStoreMulSec<Policy>::Lookup(Ptr<const Interest> interest, double& r) {
	NS_LOG_FUNCTION(this << interest->GetName ());

	typename super::const_iterator node;
	std::string BR = this->ExtractBitrate(interest->GetName());

	if (interest->GetExclude() == 0) {
		node = this->deepest_prefix_match(interest->GetName(), BR);
	} else {
		node = this->deepest_prefix_match_if_next_level(interest->GetName(),
				isNotExcluded(*interest->GetExclude()), BR);
	}

	if (node != this->end()) {
		NS_LOG_INFO("Cache Hit at: "<<m_node->GetId()<<" Name: "<<interest->GetName().toUri());
		this->m_cacheHitsTrace(interest, node->payload()->GetData());

		Ptr<Data> copy = Create<Data>(*node->payload()->GetData());
		ConstCast<Packet>(copy->GetPayload())->RemoveAllPacketTags();
		r = CalReward(ConstCast<Interest>(interest), true);

		NS_LOG_DEBUG("[CS]: Cache Hit at Node" << m_node->GetId()
				<< " For BR:" << ExtractBitrate(interest->GetName()) << " REWARD: " << static_cast<int>(r));

		return copy;
	} else {
		NS_LOG_INFO("Cache Miss at: "<<m_node->GetId()<<" Name: "<<interest->GetName().toUri());
		this->m_cacheMissesTrace(interest);
		r = CalReward(ConstCast<Interest>(interest), false);

		NS_LOG_DEBUG("[CS]: Cache Miss at Node" << m_node->GetId()
				<< " For BR:" << ExtractBitrate(interest->GetName()) << " REWARD: " << static_cast<int>(r));

		return 0;
	}
}

template<class Policy>
uint32_t ContentStoreMulSec<Policy>::GetCapacity()
{
	return GetMaxSize();
}


//Bug Fix by Wenjie
//Date: 08/15/2016
template<class Policy>
uint32_t ContentStoreMulSec<Policy>::GetCapacity(const string& s)
{
	return this->getPolicy(s).get_max_size();
}

template<class Policy>
bool
ContentStoreMulSec<Policy>::Add (Ptr<const Data> data)
{
	/*
  NS_LOG_FUNCTION (this << data->GetName ());

  Ptr<entry> newEntry = Create<entry>(this, data);
  std::string BR = this->ExtractBitrate(data->GetName());

  //if(Simulator::Now() <= m_period)
  //{
	  std::pair<typename super::iterator, bool> result = super::insert(
				data->GetName(), newEntry, BR, data->GetPayload()->GetSize());

////////////////////////////////////////////////////////
///////////////////////////////////////////////////////
////////////Show Cached Contents (Debug)///////////////
///////////////////////////////////////////////////////
/*
		if(m_node->GetId() == 8)//Test a random router
		  {
			for(uint32_t i = 1; i <= m_BRinfo->GetTableSize(); i++)
			{
				  typename super::policy_container::const_iterator item = this->getPolicy(m_BRinfo->GetBRFromRank(i)).begin();
				  NS_LOG_INFO("Node: " << m_node->GetId() << "\tBitrate: " << m_BRinfo->GetBRFromRank(i));
				  for (; item != this->getPolicy(m_BRinfo->GetBRFromRank(i)).end();
						item++) {
					  NS_LOG_INFO(item->payload()->GetName());
				  }
			}
		  }

/////////////////////////////////////////////////////////

		if (result.first != super::end()) {
			if (result.second) {
				newEntry->SetTrie(result.first);

				m_didAddEntry(newEntry);
				return true;
			}
		}
  //}
  return false;

  */
	NS_LOG_FUNCTION(this << data->GetName ());

	  Ptr<entry> newEntry = Create<entry>(this, data);
	  std::string BR = this->ExtractBitrate(data->GetName());

	//if (Simulator::Now() <= this->m_period)
	//{
		bool go_on = true;
		//double avgn = ((data->GetTSI() - 1) * m_nonedgesize + m_edgesize) / static_cast<double>(data->GetTSI());
		double avgn = static_cast<double>(GetMaxSize());
		double p = (static_cast<double>(data->GetAcc()) / (this->m_timein * avgn)) * (static_cast<double>(data->GetTSB()) / data->GetTSI());
		//std::cout << p << std::endl;
		UniformVariable prob(0.0, 1.0);
		if(prob.GetValue() > p)
			go_on = false;

		if(go_on)
		{
			  std::pair<typename super::iterator, bool> result = super::insert(
						data->GetName(), newEntry, BR, data->GetPayload()->GetSize());
			  if (result.first != super::end()) {
				if (result.second) {
					newEntry->SetTrie(result.first);

					this->m_didAddEntry(newEntry);
					return true;
				}
			}
		}
	//}
	return false;
}

template<class Policy>
void ContentStoreMulSec<Policy>::FillinCacheRun()
{
	this->inCacheRun.clear();
	super::GetCachedName(this->inCacheRun);
}

template<class Policy>
void ContentStoreMulSec<Policy>::Print(std::ostream &os) const {
	for(uint32_t i = 1; i <= m_BRinfo->GetTableSize(); i++)
	{
		typename super::policy_container::const_iterator item = this->getPolicy(m_BRinfo->GetBRFromRank(i)).begin();
		os << "Node: " << m_node->GetId() << "\tBitrate: " << m_BRinfo->GetBRFromRank(i) <<
				"\tSize:" << this->getPolicy(m_BRinfo->GetBRFromRank(i)).get_max_size () << std::endl;
		for (; item != this->getPolicy(m_BRinfo->GetBRFromRank(i)).end();
				item++) {
			os << item->payload()->GetName() << std::endl;
		}
		os << std::endl;
	}
}

template<class Policy>
void ContentStoreMulSec<Policy>::SetMaxSize(uint32_t maxSize) {
	//this->getPolicy().set_max_size(maxSize);
	m_totalsize = maxSize;
}

template<class Policy>
uint32_t ContentStoreMulSec<Policy>::GetMaxSize() const {
	//return this->getPolicy().get_max_size();
	return this->m_totalsize;
}

template<class Policy>
Ptr<Entry> ContentStoreMulSec<Policy>::Begin() {
	typename super::parent_trie::recursive_iterator item(super::getTrie()), end(
			0);
	for (; item != end; item++) {
		if (item->payload() == 0)
			continue;
		break;
	}

	if (item == end)
		return End();
	else
		return item->payload();
}

template<class Policy>
Ptr<Entry> ContentStoreMulSec<Policy>::End() {
	return 0;
}

template<class Policy>
Ptr<Entry> ContentStoreMulSec<Policy>::Next(Ptr<Entry> from) {
	if (from == 0)
		return 0;

	typename super::parent_trie::recursive_iterator item(
			*StaticCast<entry>(from)->to_iterator()), end(0);

	for (item++; item != end; item++) {
		if (item->payload() == 0)
			continue;
		break;
	}

	if (item == end)
		return End();
	else
		return item->payload();
}

} // namespace cs
} // namespace ndn
} // namespace ns3

#endif // NDN_CONTENT_STORE_IMPL_H_
