/*
 *
 * Author: Wenjie Li
 * Date: 2016-04-10
 */

#ifndef NDN_APP_VIDEO_TRACER_H
#define NDN_APP_VIDEO_TRACER_H

#include "ns3/ptr.h"
#include "ns3/simple-ref-count.h"
#include <ns3/nstime.h>
#include <ns3/event-id.h>
#include <ns3/node-container.h>
#include "ns3/ndn-data.h"

#include <boost/tuple/tuple.hpp>
#include <boost/shared_ptr.hpp>
#include <list>
#include <string>
#include <fstream>
#include <sstream>
#include <utility>
#include <tuple>

#include "mysql_connection.h"
#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/statement.h>

namespace ns3 {

class Node;
class Packet;

namespace ndn {

class App;
class VideoTracer : public SimpleRefCount<VideoTracer>
{
public:
  /**
   * @brief Helper method to install tracers on all simulation nodes
   *
   * @param file File to which traces will be written.  If filename is -, then std::out is used
   *
   * @returns a tuple of reference to output stream and list of tracers. !!! Attention !!! This tuple needs to be preserved
   *          for the lifetime of simulation, otherwise SEGFAULTs are inevitable
   *
   */
  static void
  InstallAll (const std::string &file, bool needHeader);

  /*
   * Record traces for all nodes on MySQL database;
   */
  static void
  InstallAllSql (sql::Driver* d, const std::string& ip, int port,
		  	  	 const std::string& database, const std::string& tableRequest, const std::string& tableSwitch,
				 const std::string& tableDelay,
		  	  	 const std::string& user, const std::string& password, const int expidx);


  static Ptr<VideoTracer>
  Install (Ptr<Node> node, boost::shared_ptr<std::ostream> outputStream);


  static Ptr<VideoTracer>
  InstallSql (Ptr<Node>, boost::shared_ptr<std::ostringstream>,
		  	  	  	  	 boost::shared_ptr<std::ostringstream>,
						 boost::shared_ptr<std::ostringstream>);


  static void
  Destroy ();

  /**
   * @brief Trace constructor that attaches to all applications on the node using node's pointer
   * @param os    reference to the output stream
   * @param node  pointer to the node
   */
  VideoTracer (boost::shared_ptr<std::ostream> os, Ptr<Node> node);

  VideoTracer (boost::shared_ptr<std::ostringstream>,
		  	   boost::shared_ptr<std::ostringstream>,
			   boost::shared_ptr<std::ostringstream>,
			   Ptr<Node>);

  /**
   * @brief Trace constructor that attaches to all applications on the node using node's name
   * @param os        reference to the output stream
   * @param nodeName  name of the node registered using Names::Add
   */
  VideoTracer (boost::shared_ptr<std::ostream> os, const std::string &node);

  /**
   * @brief Destructor
   */
  ~VideoTracer ();

  /**
   * @brief Print head of the trace (e.g., for post-processing)
   *
   * @param os reference to output stream
   */
  void
  PrintHeader (std::ostream &os) const;

  static sql::Statement 	*stmt;
  static sql::Driver 		*driver;
  static sql::Connection 	*con;



private:
  void
  Connect ();

  void
  VideoPlayTrace (std::tuple<uint32_t,uint32_t, uint32_t>id,
		  	  	  std::string bitrate,
				  std::string next_bitrate,
				  int64_t delay,
				  double buffer,
				  double reward,
				  uint32_t hopcount,
				  //uint32_t rank,
				  uint32_t tranPhase);

  void
  VideoSwitchTrace(Ptr<App> app, uint32_t appid, uint32_t fileid, uint32_t switchup, uint32_t switchdown, uint32_t tranPhase);

  void
  QDelayByHop(Ptr<const Data> data, std::string br, uint32_t tranPhase);


private:
  std::string m_node;
  Ptr<Node> m_nodePtr;
  boost::shared_ptr<std::ostream> m_os;
  boost::shared_ptr<std::ostringstream> m_ss;

  boost::shared_ptr<std::ostringstream> m_ss_switch;
  boost::shared_ptr<std::ostringstream> m_ss_delay;

  static std::list< boost::tuple< boost::shared_ptr<std::ostream>, std::list<Ptr<VideoTracer> > > > g_tracers;
  static std::list< boost::tuple< boost::shared_ptr<std::ostringstream>,
  	  	  	  	  	  	  	  	  boost::shared_ptr<std::ostringstream>,
								  boost::shared_ptr<std::ostringstream>,
  	  	  	  	  	  	  	  	  std::list<Ptr<VideoTracer> > > > g_tracers_ss;

  static std::string		tablename_Request;
  static int				insertitem_Request;
  static std::string		tablename_Switch;
  static int				insertitem_Switch;
  static std::string		tablename_Delay;
  static int				insertitem_Delay;
  static std::string		DatabaseName;

  static int				tableid;
  static bool				usedfordestruction;

};

} // namespace ndn
} // namespace ns3

#endif
