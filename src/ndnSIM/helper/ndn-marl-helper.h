/*
 * Author: Wenjie Li
 * Date: 2016-12-06
 * MARLHelper setup the reinforcement learning environment on each node,
 * and arrange the coordination to reach the approximate optimum
 *
 */

#ifndef NDN_MARL_HELPER_H
#define NDN_MARL_HELPER_H

#include "ns3/ptr.h"
#include "ns3/nstime.h"
#include "ns3/node.h"
#include "ns3/object-factory.h"
#include "ns3/random-variable.h"

#include <iostream>
#include <map>
#include <list>
#include <deque>
#include <string>
#include <fstream>


#include "mysql_connection.h"
#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/statement.h>

namespace ns3{
namespace ndn{

class MARLHelper
{
public:
	MARLHelper(const std::string&,
			   const std::string&,
			   const std::string&,
			   uint32_t, uint32_t,
			   double);

	virtual ~MARLHelper();

	void SetMARLAttributes(const std::string &marltype,
						   const std::string &attr1, const AttributeValue &value1,
						   const std::string &attr2, const AttributeValue &value2,
						   const std::string &attr3, const AttributeValue &value3,
						   const std::string &attr4, const AttributeValue &value4,
						   const std::string &attr5, const AttributeValue &value5);

	void SetLogPath (const std::string & logpath,
					 const std::string & qpath);

	void
	InstallAll();

	/* Only work under a tree topology !!! */
	void
	SetTopologicalOrder(Ptr<Node> ServerNode);

	void
	StartRL();

public:
	void PrepareSqlUpdate(sql::Connection*, const std::string&,
						  const std::string&, const std::string&,
						  uint32_t, const std::string& name = "Sigcomm17");

	void GapForReplacement();


private:
	MARLHelper (const MARLHelper &);
	MARLHelper& operator =(const MARLHelper& o);

	void BeforeTransition();
	void AfterTransition();

	void BreadthFirstSearch(std::deque<Ptr<Node> >&);
	void DepthFirstSearch(Ptr<Node>, std::deque<Ptr<Node> >&);

	void InputQTable();
	void OutputQTable();

private:
	//bool 			m_measurement = false;	// m_measurement is set to be true when the simulator start tracers
	bool			m_init = true;
	//bool			m_result = true;		//The effectiveness of MARL learning result

	Time 			m_step_gap;		//Time period between two steps in one episode
	Time			m_episode_gap;	//Time period between two episodes
	Time			m_trigger;		//Initial time period to warm up the cache
	uint32_t		m_currstep = 0;
	uint32_t		m_steplimit;	//The number of transitions in a episode

	uint32_t		m_currepisode = 0;
	uint32_t		m_episodelimit; //The number of episodes in the MARL

	uint32_t		m_performance_rounds;	//The number of transitions where the performance is recorded
	double			m_epsilon;		// dynamic epsilon-greedy policy
private:
	ObjectFactory 	m_marlFactory;
	std::list<Ptr<Node> > m_topo_order_ptr;
	std::map<uint32_t, std::deque<Ptr<Node> > > m_HsetDict;
	UniformVariable m_SeqRng; //RNG

private:
	sql::Connection* con;
	std::string		m_TableName;
	std::string 	m_ColumnName;
	std::string		m_KeyName;
	std::string		m_databaseName;
	uint32_t		key;

	std::ofstream* 	m_log;
	std::fstream*	m_qrecorder;
	std::string		m_qTablePath;

};

}
}

#endif

