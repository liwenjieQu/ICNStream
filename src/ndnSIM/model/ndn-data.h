/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2011-2013 University of California, Los Angeles
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
 * Author: Ilya Moiseenko <iliamo@cs.ucla.edu>
 *         Alexander Afanasyev <alexander.afanasyev@ucla.edu>
 */

#ifndef _NDN_CONTENT_OBJECT_HEADER_H_
#define _NDN_CONTENT_OBJECT_HEADER_H_

#include "ns3/simple-ref-count.h"
#include "ns3/nstime.h"
#include "ns3/packet.h"
#include "ns3/ptr.h"

#include <ns3/ndn-name.h>

namespace ns3 {
namespace ndn {

/**
 * @ingroup ndn
 * @brief Data header
 */
class Data : public SimpleRefCount<Data>
{
public:
  /**
   * @brief Constructor
   *
   * Creates a null header
   **/
  Data (Ptr<Packet> payload = Create<Packet> ());

  /**
   * @brief Copy constructor
   */
  Data (const Data &other);
  /**
   * \brief Set content object name
   *
   * Sets name of the content object
   **/
  void
  SetName (Ptr<Name> name);

  /**
   * @brief Another, less efficient, variant of setting content object name
   *
   * Sets name of the content object
   */
  void
  SetName (const Name &name);
  
  /**
   * @brief Get name of the content object
   */
  const Name&
  GetName () const;

  /**
   * @brief Get smart pointer to the interest name (to avoid extra memory usage)
   */
  Ptr<const Name>
  GetNamePtr () const;

  /**
   * @brief Set content object timestamp
   * @param timestamp timestamp
   */
  void
  SetTimestamp (const Time &timestamp);

  /**
   * @brief Get timestamp of the content object
   */
  Time
  GetTimestamp () const;

  /**
   * @brief Set freshness of the content object
   * @param freshness Freshness, 0s means infinity
   */
  void
  SetFreshness (const Time &freshness);

  /**
   * @brief Get freshness of the content object
   */
  Time
  GetFreshness () const;

  /**
   * @brief Set "fake" signature on the content object
   * @param signature  uint32_t number, simulating content object signature
   *
   * Values for the signature totally depend on the application
   */
  void
  SetSignature (uint32_t signature);

  /**
   * @brief Get "fake" signature of the content object
   *
   * Values for the signature totally depend on the application
   */
  uint32_t
  GetSignature () const;

  /**
   * @brief Set key locator
   * @param keyLocator name of the key
   */
  void
  SetKeyLocator (Ptr<Name> keyLocator);

  /**
   * @brief Get key locator
   *
   * Note that only <KeyName> option for the key locator is supported
   */
  Ptr<const Name>
  GetKeyLocator () const;
  
  //////////////////////////////////////////////////////////////////
  /**
   * @brief Get payload of data packet
   *
   * This payload can also carry packet tags
   */
  void
  SetPayload (Ptr<Packet> payload);

  /**
   * @brief Set payload of data packet
   *
   * This payload can also carry packet tags
   */
  Ptr<const Packet>
  GetPayload () const;
  
  /**
   * @brief Get wire formatted packet
   *
   * If wire formatted packet has not been set before, 0 will be returned
   */
  inline Ptr<const Packet>
  GetWire () const;

  /**
   * @brief Set (cache) wire formatted packet
   */
  inline void
  SetWire (Ptr<const Packet> packet) const;

  /**
   * @brief Print Interest in plain-text to the specified output stream
   */
  void
  Print (std::ostream &os) const;
  
  inline void SetTSI(int8_t n)
  {
	  m_TSI = n;
	  m_wire = 0;
  }
  inline void SetTSB(int8_t n)
  {
	  m_TSB = n;
	  m_wire = 0;
  }
  inline void SetAcc(uint32_t n)
  {
	  ProbeCache_ACC = n;
	  m_wire = 0;
  }
  inline void AddTSB()
  {
	  m_TSB++;
	  m_wire = 0;
  }
  inline void AddAcc(uint32_t n)
  {
	  ProbeCache_ACC += n;
	  m_wire = 0;
  }
  inline void DecAcc(uint32_t n)
  {
	  ProbeCache_ACC -= n;
	  m_wire = 0;
  }
  inline void InitAcc()
  {
	  ProbeCache_ACC = 0;
	  m_wire = 0;
  }
  inline uint32_t GetAcc() const
  {
	  return ProbeCache_ACC;
  }
  inline int8_t GetTSI() const
  {
	  return m_TSI;
  }
  inline int8_t GetTSB() const
  {
	  return m_TSB;
  }


  inline uint8_t GetHops() const
  {
	  return m_hops;
  }

  inline void SetHops(uint8_t h)
  {
	  m_hops = h;
	  m_wire = 0;
  }
  inline void AddOneHop()
  {
	  m_hops++;
	  m_wire = 0;
  }
  inline uint32_t GetDelay(int index) const
  {
	  return m_delays[index];
  }
  inline void SetDelay(int index, uint32_t v)
  {
	  m_delays[index] = v;
	  m_wire = 0;
  }

  inline void SetAccumulatedReward(double r)
  {
  	  m_reward += static_cast<int16_t>(r*1e3);
  }

  inline double GetAccumulatedReward() const
  {
	  return static_cast<double>(m_reward)/1e3;
  }

  inline void SetReward(uint16_t r)
  {
	  m_reward = static_cast<int16_t>(r);
	  m_wire = 0;
  }
  inline int16_t GetReward() const
  {
	  return m_reward;
  }

/*
  inline void SetPFlag(int8_t n)
  {
	  m_ProducerFlag = n;
	  m_wire = 0;
  }
  inline int8_t GetPFlag() const
  {
	  return m_ProducerFlag;
  }
*/


private:
  // NO_ASSIGN
  Data &
  operator = (const Data &other) { return *this; }
  
private:
  Ptr<Name> 	m_name;
  Time 			m_freshness;
  Time 			m_timestamp;
  uint32_t 		m_signature; // 0, means no signature, any other value application dependent (not a real signature)
  Ptr<Packet> 	m_payload;
  Ptr<Name> 	m_keyLocator;


  int8_t 		m_TSI;
  int8_t 		m_TSB;
  //int8_t			m_ProducerFlag;	//Flag to indicate Data is retrieved directly from Producer (m_ProducerFlag = 1) otherwise, = 0
  uint8_t 		m_hops;			//Number of hops this Data has traveled.
  int16_t		m_reward;
  uint32_t 		ProbeCache_ACC;
public:
  Time			m_mostRecentDelay;
  const static int MaxHop = 16;

private:
  uint32_t		m_delays[MaxHop];		//Delay by Each hop

  mutable Ptr<const Packet> m_wire;
};

inline std::ostream &
operator << (std::ostream &os, const Data &d)
{
  d.Print (os);
  return os;
}

/**
 * @brief Class for Data parsing exception
 */
class DataException {};

inline Ptr<const Packet>
Data::GetWire () const
{
  return m_wire;
}

inline void
Data::SetWire (Ptr<const Packet> packet) const
{
  m_wire = packet;
}


} // namespace ndn
} // namespace ns3

#endif // _NDN_CONTENT_OBJECT_HEADER_H_
