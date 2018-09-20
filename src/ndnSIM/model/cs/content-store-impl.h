/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2011 University of California, Los Angeles
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
 * Author: Alexander Afanasyev <alexander.afanasyev@ucla.edu>
 */

#ifndef NDN_CONTENT_STORE_IMPL_H_
#define NDN_CONTENT_STORE_IMPL_H_

#include "ndn-content-store.h"
#include "ns3/packet.h"
#include "ns3/ndn-interest.h"
#include "ns3/ndn-data.h"
#include "ns3/random-variable.h"
#include "ns3/nstime.h"
#include <boost/foreach.hpp>
#include <iterator>
#include <memory>

#include "ns3/log.h"
#include "ns3/uinteger.h"
#include "ns3/string.h"

#include "../../utils/trie/trie-with-policy.h"
using namespace std;

namespace ns3 {
namespace ndn {
namespace cs {

/**
 * @ingroup ndn-cs
 * @brief Cache entry implementation with additional references to the base container
 */
template<class CS>
class EntryImpl: public Entry {
public:
	typedef Entry base_type;

public:
	EntryImpl(Ptr<ContentStore> cs, Ptr<const Data> data) :
			Entry(cs, data), item_(0) {
	}

	void SetTrie(typename CS::super::iterator item) {
		item_ = item;
	}

	typename CS::super::iterator to_iterator() {
		return item_;
	}
	typename CS::super::const_iterator to_iterator() const {
		return item_;
	}

private:
	typename CS::super::iterator item_;
};

/**
 * @ingroup ndn-cs
 * @brief Base implementation of NDN content store
 */
template<class Policy>
class ContentStoreImpl: public ContentStore, protected ndnSIM::trie_with_policy<
		Name,
		ndnSIM::smart_pointer_payload_traits<
				EntryImpl<ContentStoreImpl<Policy> >, Entry>, Policy> {
public:
	typedef ndnSIM::trie_with_policy<Name,
			ndnSIM::smart_pointer_payload_traits<
					EntryImpl<ContentStoreImpl<Policy> >, Entry>, Policy> super;

	typedef EntryImpl<ContentStoreImpl<Policy> > entry;

	static TypeId
	GetTypeId();

	ContentStoreImpl() {
	}
	;
	virtual ~ContentStoreImpl() {
	}
	;

	// from ContentStore

	virtual inline Ptr<Data>
	Lookup(Ptr<const Interest> interest);

	virtual Ptr<Data>
	Lookup (Ptr<const Interest> interest, double& r);

	virtual inline bool
	Add(Ptr<const Data> data);

	// Added to implement DFA Transition (Deserted)
	/*
	virtual inline bool
	RemoveByMDP(Ptr<Name> name);

	virtual inline bool
	AddByMDP(Ptr<Name> name);

	virtual uint32_t GetCurrentSize() const;
	*/

	virtual inline void
	Print(std::ostream &os) const;

	virtual uint32_t
	GetSize() const;

	virtual Ptr<Entry>
	Begin();

	virtual Ptr<Entry>
	End();

	virtual Ptr<Entry>
	Next(Ptr<Entry>);

	const typename super::policy_container &
	GetPolicy() const {
		return super::getPolicy();
	}

	typename super::policy_container &
	GetPolicy() {
		return super::getPolicy();
	}

	virtual uint32_t GetCapacity();

	virtual uint32_t GetCapacity(const string& s);

	virtual
	void FillinCacheRun();

	virtual
	void FillinCacheRun(std::vector<ns3::ndn::Name>&);

	virtual void ReportPartitionStatus();
/*
public:

	virtual
	void ClearCachedContent();

	virtual
	void SetContentInCache(std::vector<ns3::ndn::Name> cs);

	virtual
	void InstallCacheEntity();

	std::string ExtractBitrate(const Name& n);

	uint32_t 			m_useProbCache;
*/

protected:
	void
	SetMaxSize(uint32_t maxSize);

	uint32_t
	GetMaxSize() const;

protected:
	/// @brief trace of for entry additions (fired every time entry is successfully added to the cache): first parameter is pointer to the CS entry
	TracedCallback<Ptr<const Entry> > m_didAddEntry;

private:
	static LogComponent g_log; ///< @brief Logging variable
};

//////////////////////////////////////////
////////// Implementation ////////////////
//////////////////////////////////////////

template<class Policy>
LogComponent ContentStoreImpl<Policy>::g_log = LogComponent(
		("ndn.cs.CE2." + Policy::GetName()).c_str());

template<class Policy>
TypeId ContentStoreImpl<Policy>::GetTypeId() {
	static TypeId tid =
			TypeId(("ns3::ndn::cs::CE2::" + Policy::GetName()).c_str())
			.SetGroupName("Ndn")
			.SetParent<ContentStore>()
			.AddConstructor<ContentStoreImpl<Policy> >()

			.AddAttribute("MaxSize",
					"Set maximum number of entries in ContentStore. If 0, limit is not enforced",
					StringValue("100"),
					MakeUintegerAccessor(&ContentStoreImpl<Policy>::GetMaxSize,
							&ContentStoreImpl<Policy>::SetMaxSize),
					MakeUintegerChecker<uint32_t>())

			.AddTraceSource("DidAddEntry",
					"Trace fired every time entry is successfully added to the cache",
					MakeTraceSourceAccessor(&ContentStoreImpl<Policy>::m_didAddEntry));

	return tid;
}

struct isNotExcluded {
	inline isNotExcluded(const Exclude &exclude) :
			m_exclude(exclude) {
	}

	bool operator ()(const name::Component &comp) const {
		return !m_exclude.isExcluded(comp);
	}

private:
	const Exclude &m_exclude;
};

template<class Policy>
Ptr<Data> ContentStoreImpl<Policy>::Lookup(Ptr<const Interest> interest) {
	NS_LOG_FUNCTION(this << interest->GetName ());

	typename super::const_iterator node;
	if (interest->GetExclude() == 0) {
		node = this->deepest_prefix_match(interest->GetName());
	} else {
		node = this->deepest_prefix_match_if_next_level(interest->GetName(),
				isNotExcluded(*interest->GetExclude()));
	}

	if (node != this->end()) {
		//NS_LOG_DEBUG("Cache Hit at: "<<this->GetNodeID()<<" Seq: "<<interest->GetName());
		this->m_cacheHitsTrace(interest, node->payload()->GetData());

		Ptr<Data> copy = Create<Data>(*node->payload()->GetData());
		ConstCast<Packet>(copy->GetPayload())->RemoveAllPacketTags();

		// Record the local reward only for performance evaluation
		CalReward(ConstCast<Interest>(interest), true);

		return copy;
	} else {
		this->m_cacheMissesTrace(interest);
		CalReward(ConstCast<Interest>(interest), false);

		return 0;
	}
}

template<class Policy>
Ptr<Data> ContentStoreImpl<Policy>::Lookup(Ptr<const Interest> interest, double& r) {
	NS_LOG_FUNCTION(this << interest->GetName ());

	typename super::const_iterator node;
	if (interest->GetExclude() == 0) {
		node = this->deepest_prefix_match(interest->GetName());
	} else {
		node = this->deepest_prefix_match_if_next_level(interest->GetName(),
				isNotExcluded(*interest->GetExclude()));
	}

	if (node != this->end()) {
		//NS_LOG_DEBUG("Cache Hit at: "<<this->GetNodeID()<<" Seq: "<<interest->GetName());
		this->m_cacheHitsTrace(interest, node->payload()->GetData());

		Ptr<Data> copy = Create<Data>(*node->payload()->GetData());
		ConstCast<Packet>(copy->GetPayload())->RemoveAllPacketTags();

		// Record the local reward only for performance evaluation
		r = CalReward(ConstCast<Interest>(interest), true);
		copy->SetAccumulatedReward(r);

		return copy;
	} else {
		this->m_cacheMissesTrace(interest);
		r = CalReward(ConstCast<Interest>(interest), false);

		return 0;
	}
}

template<class Policy>
uint32_t ContentStoreImpl<Policy>::GetCapacity()
{
	return GetMaxSize();
}
template<class Policy>
uint32_t ContentStoreImpl<Policy>::GetCapacity(const string& s)
{
	return GetMaxSize();
}

template<class Policy>
bool
ContentStoreImpl<Policy>::Add (Ptr<const Data> data)
{
  NS_LOG_FUNCTION (this << data->GetName ());

  Ptr<entry> newEntry = Create<entry>(this, data);

  if(Simulator::Now() <= m_period) //m_period is needed only when cache is periodically updated (Compared with StreamCache)
  {
	  std::pair<typename super::iterator, bool> result = super::insert(
				data->GetName(), newEntry, data->GetPayload()->GetSize());
		if (result.first != super::end()) {
			if (result.second) {
				newEntry->SetTrie(result.first);

				m_didAddEntry(newEntry);
				return true;
			}
		}
  }
  return false;
}

template<class Policy>
void ContentStoreImpl<Policy>::FillinCacheRun()
{
	this->inCacheRun.clear();
	super::GetCachedName(this->inCacheRun);
}

template<class Policy>
void ContentStoreImpl<Policy>::FillinCacheRun(std::vector<ns3::ndn::Name>& v)
{
	super::GetCachedName(v);
}

template<class Policy>
void ContentStoreImpl<Policy>::ReportPartitionStatus()
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
		ResetReward();
	}
	Simulator::Schedule(Time(Seconds(600)), &ContentStoreImpl<Policy>::ReportPartitionStatus, this);
}

//Added to implement DFA Transition (Deserted)
/*
template<class Policy>
uint32_t ContentStoreImpl<Policy>::GetCurrentSize() const{
	return this->getPolicy().get_current_size();
}

template<class Policy>
bool ContentStoreImpl<Policy>::AddByMDP(Ptr<Name> name)
{
	Ptr<Data> data = Create<Data>(Create<Packet>(m_BRinfo->GetChunkSize(ExtractBitrate(*name)) * 1e3));
	data->SetName(name);
	Ptr<entry> newEntry = Create<entry>(this, data);

	std::pair<typename super::iterator, bool> result = super::insert(
						*name, newEntry, data->GetPayload()->GetSize());
	if (result.first != super::end()) {
		if (result.second) {
			newEntry->SetTrie(result.first);

			m_didAddEntry(newEntry);
			return true;
		}
	}
	return false;
}

template<class Policy>
bool ContentStoreImpl<Policy>::RemoveByMDP(Ptr<Name> name)
{
	typename super::iterator result = super::find_exact(*name);
	if(result != super::end())
	{
		super::erase(result);
		return true;
	}
	return false;
}
*/

template<class Policy>
void ContentStoreImpl<Policy>::Print(std::ostream &os) const {
	for (typename super::policy_container::const_iterator item =
			this->getPolicy().begin(); item != this->getPolicy().end();
			item++) {
		os << item->payload()->GetName() << std::endl;
	}
}

template<class Policy>
void ContentStoreImpl<Policy>::SetMaxSize(uint32_t maxSize) {
	this->getPolicy().set_max_size(maxSize);
}

template<class Policy>
uint32_t ContentStoreImpl<Policy>::GetMaxSize() const {
	return this->getPolicy().get_max_size();
}

template<class Policy>
uint32_t ContentStoreImpl<Policy>::GetSize() const {
	return this->getPolicy().size();
}

template<class Policy>
Ptr<Entry> ContentStoreImpl<Policy>::Begin() {
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
Ptr<Entry> ContentStoreImpl<Policy>::End() {
	return 0;
}

template<class Policy>
Ptr<Entry> ContentStoreImpl<Policy>::Next(Ptr<Entry> from) {
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
