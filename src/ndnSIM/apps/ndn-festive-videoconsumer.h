/*
 * Author: Wenjie Li
 * Implement FESTIVE rate adaptation algorithm, proposed by
 * Junchen Jiang et. cl.
 * URL: http://dl.acm.org/citation.cfm?id=2413189
 * Conference: CoNEXT
 */

#ifndef NDN_FESTIVE_SCHEME_H
#define NDN_FESTIVE_SCHEME_H

#include "ndn-consumer-zipf-mandelbrot.h"
#include "ns3/nstime.h"
#include <string>
#include <list>
#include <deque>
#include <vector>
#include <map>
#include <utility>
#include <tuple>

namespace ns3 {
namespace ndn {

/*
 * Compare class VideoClient and VideoConsumer,
 * VideoClient achieves rate adaptation control and reacts to the real-time bandwidth measurement
 */

class VideoClient: public ConsumerZipfMandelbrot
{
public:
  static TypeId GetTypeId ();

  VideoClient ();
  virtual ~VideoClient();

  void SendPacket(uint32_t fileseq, uint32_t chunkseq, std::string bitrate);
  void SendVideofile();


  virtual void
  OnTimeout (uint32_t sequenceNumber);

  virtual void
  OnData (Ptr<const Data> contentObject);

  virtual void
  WillSendOutInterest (uint32_t sequenceNumber);

  virtual inline void
  EnableRecord()
  {
	  m_enableRecord = true;
  };

  virtual inline void
  DisableRecord()
  {
	  m_enableRecord = false;
  }

  virtual inline void
  IncreaseTransition()
  {
	  m_transition++;
	  m_rewardcount = 0;
	  m_rewardvalue = 0;

  };

  virtual inline void
  ResetCounter() {m_transition = 0;};

  virtual inline void
  ResetCounter(uint32_t n) {m_transition = n;};

  virtual std::pair<uint32_t, uint32_t>
  DelayHistory(const std::string& BR, uint32_t hop);

  virtual std::pair<uint32_t, double>
  GetCondition();

protected:
  virtual void
  ScheduleNextPacket ();

  // from App
  virtual void
  StartApplication ();

  virtual void
  StopApplication ();

  void ScheduleNextVideoFile();
  void CheckRetxTimeout (double);

  double  BandwidthEstimator();
  double  SmoothBandwidth(double);
  std::string QuantizeToBitrate(double);
  void    ScheduleNextVideoChunk(uint32_t, uint32_t, double);
  double  RandomSchedule();

private:
  std::string ReferenceBR(double);
  std::string DelayedUpdate(std::string, double);
  uint32_t    NumberofSwitches();

  void		  UpdateDelay(const std::string&, uint8_t, const Time&, Ptr<const Data>);
  //uint64_t    SetBRBoundary(uint8_t&);
  uint64_t    SetBRBoundary(const std::string& reqbr);
  void 		  AddToCondition(double);

private:
  bool 		m_enableRecord;
  uint32_t 	m_transition; // Used to record the transition phase
  uint32_t 	m_appid;
  double	m_cacheProfit;
  double	m_weight;
  double	m_dropthres;

  uint32_t        m_historyWindow;
  std::list<double>   m_estimatedBandwidth;
  std::list<std::string>m_selectedBandwidth;
  bool          m_fileinTransmission;
  std::string     m_currentBitRate;
  std::string     m_previousBitRate;
  std::string     m_retransmitBitRate;//used to record the first choice of currentBitRate during retransmission


  /* In seconds */
  double        m_currentbuff;
  double        m_targetbuff;
  double		m_retxbuff; //last buffer size before retransmission
  bool          m_recoverFromFreeze;
  uint32_t        m_ProducerRank;

  Time          m_lastwatchpoint;
  uint32_t        m_numSwitchUp;
  uint32_t        m_numSwitchDown;

  uint32_t		m_rewardcount;
  double		m_rewardvalue;


  std::map<std::string, std::vector<std::deque<uint32_t> > >  m_delaybyhop; //unit: MS!!!!2016-07-14
  std::map<std::string, std::vector<uint32_t> >         m_recentinsertTime;
  std::map<std::string, std::vector<std::vector<uint32_t> > >  m_delayHistory; //unit: MS!!!!2017-03-02 // Record the Entire History of delay by hop

  TracedCallback< std::tuple<uint32_t /* appid */, uint32_t /* fileid */, uint32_t /* chunkid */>,
                  std::string /* current selected bitrate */,
				  std::string /* next selected bitrate */,
				  int64_t /* delay measurement*/, //Time /* delay */,
				  double /* buffer size */,
				  double /* reward */,
				  uint32_t /*hop count*/,
				  uint32_t /*transition phase*/> m_videoPlayTrace;

  TracedCallback<Ptr<App> /* app */, uint32_t, /*appid*/ uint32_t /* fileid */,
          uint32_t /* switch up */, uint32_t /* switch down*/, uint32_t /*transition phase*/> m_switchTrace;

  TracedCallback<Ptr<const Data>, std::string, uint32_t /*transition phase*/> m_qdelayTrace;
private:
  static uint32_t NumApps;
  const static uint32_t DelayWindow = 5;

};



}
}
#endif
