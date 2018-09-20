#ifndef NDN_VIDEO_CONSUMER__H
#define NDN_VIDEO_CONSUMER__H

#include "ndn-consumer-zipf-mandelbrot.h"
#include <map>
namespace ns3 {
namespace ndn {

class VideoConsumer: public ConsumerZipfMandelbrot
{
	/*
	 * author: Wenjie Li
	 * Inherited from in-built Zipf distribution. Change on the format
	 */
public:
  static TypeId GetTypeId ();

  VideoConsumer ();
  virtual ~VideoConsumer ();

  void SendPacket(uint32_t fileseq, uint32_t chunkseq);

  void SendVideofile();

  virtual void
  OnTimeout (uint32_t sequenceNumber);

protected:
  virtual void
  ScheduleNextPacket ();

  // from App
  virtual void
  StartApplication ();

  virtual void
  StopApplication ();

  void ScheduleNextVideoChunk(uint32_t, uint32_t);
  void ScheduleNextVideoFile();
private:
  double 	m_bitrateSize;
  double	m_cacheProfit;
  std::string m_bitrate;


};

}
}
#endif /* NDN_VIDEO_CONSUMER__H */
