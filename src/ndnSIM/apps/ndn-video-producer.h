/*
 * author: Wenjie Li
 * DASH-Offline
 *
 */

#ifndef NDN_VIDEO_PRODUCER_H
#define NDN_VIDEO_PRODUCER_H

#include "ndn-app.h"
#include "ns3/ptr.h"
#include "ns3/ndn-name.h"
#include "ns3/ndn-data.h"
#include <map>
#include <string>

namespace ns3 {
namespace ndn {

/**
 * @ingroup ndn-apps
 * @brief A simple Interest-sink applia simple Interest-sink application
 *
 * A simple Interest-sink applia simple Interest-sink application,
 * which replying every incoming Interest with Data packet with a specified
 * size and name same as in Interest.cation, which replying every incoming Interest
 * with Data packet with a specified size and name same as in Interest.
 */
class VideoProducer : public App
{
public:
  static TypeId
  GetTypeId (void);

  VideoProducer();

  // inherited from NdnApp
  void OnInterest (Ptr<const Interest> interest);

protected:
  // inherited from Application base class.
  virtual void
  StartApplication ();    // Called at time specified by Start

  virtual void
  StopApplication ();     // Called at time specified by Stop

private:
  std::string ExtractBitrate(const Name&);
  double 	  GetReward(Ptr<const Interest> interest);

  Name m_prefix;
  Name m_postfix;
  Time m_freshness;
  uint32_t 	m_design;
  double   	m_unit;
  //uint32_t m_signature;
  //Name m_keyLocator;
  //std::map<std::string, double> m_dict_ratetosize;
};

}
}

#endif
