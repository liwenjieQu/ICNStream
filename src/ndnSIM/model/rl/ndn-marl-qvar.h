/*
 * Author: Wenjie Li
 * Date: 2016-12-05
 * ndn-marl.qvar.h contains a QVariable class which defines the Q-fuction used in the MARL
 */

#ifndef NDN_RL_MARL_QVAR_H
#define NDN_RL_MARL_QVAR_H

#include "ndn-agent-dependent.h"
#include "ndn-agent-basic.h"
#include "ns3/object.h"
#include "ns3/ptr.h"
#include "ns3/node.h"

#include "boost/functional/hash.hpp"

#include <vector>

namespace ns3{
namespace ndn{

class QVariable
{
public:
	QVariable(Ptr<Node> nodeptr,
			Ptr<DependentMARLState> s, Ptr<DependentMARLAction> a);

	QVariable(const std::vector<int8_t>& state, const std::vector<int8_t>& action);

	~QVariable();
	bool operator==(const QVariable& index) const;

	std::vector<int8_t> 	m_state_arr;
	std::vector<int8_t> 	m_action_arr;
};

struct QHasher
{
	std::size_t operator()(const QVariable& index) const;
};

}
}
#endif

