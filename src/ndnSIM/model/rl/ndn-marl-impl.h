/*
 * Author: Wenjie Li
 * Date: 2016-12-04
 */

/*
 * ndn-marl.h defines the multi-agent reinforcement learning framework
 * to do cache partition for adaptive video streaming
 *
 * ndn-marl.h defines a pure virtual class
 * ndn-marl-greedy.h defines the specific framework (using greedy algorithm)
 */

#ifndef NDN_RL_MARL_H
#define NDN_RL_MARL_H

#include "ndn-marl.h"
#include "ns3/ndn-agent-aggregate.h"
#include "ns3/object.h"
#include "ns3/ptr.h"
#include "ns3/node.h"
#include "ns3/log.h"
#include "ns3/traced-callback.h"
#include "ns3/ndn-bitrate.h"
#include "ns3/ndn-content-store.h"
#include "ns3/random-variable.h"

#include "ns3/boolean.h"
#include "ns3/string.h"
#include "ns3/double.h"

#include <unordered_map>
#include <set>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>

namespace ns3 {
namespace ndn {

template<typename State, typename Action>
class MARLnfaImpl: public MARLnfa
{
public:

	static
	TypeId GetTypeId ();

	MARLnfaImpl(){};

	virtual
	~MARLnfaImpl(){};

	virtual void AddToHset(Ptr<Node>); 		// Add Elements(Node) to the dependency set
	virtual std::size_t GetQStatus() const;	// Check the size of Q-Table

	virtual void ResetToInit();
	virtual void UpdateCurrentState();
	virtual bool UpdateCurrentAction(bool);
	virtual bool ManualSetCurrentAction(int, int);
	virtual bool Transition();				// Apply Transition in states


	virtual inline Ptr<const DependentMARLState> GetCurrentState() const
	{
		return ConstCast<DependentMARLState>(m_currentstate);
	}

	virtual inline Ptr<const DependentMARLState> GetNextState() const
	{
		return ConstCast<DependentMARLState>(m_nextstate);
	}

	virtual inline Ptr<const DependentMARLAction> GetCurrentAction() const
	{
		return ConstCast<DependentMARLAction>(m_currentaction);
	}

	virtual inline Ptr<const DependentMARLAction> GetOptAction() const
	{
		return ConstCast<DependentMARLAction>(m_lastoptaction);
	}

	virtual void OutputQStatus(std::fstream* qRecorder);
	virtual void InputQStatus(std::fstream* qRecorder);
private:
	/*
	 * Find the Approximate Global Optimum
	 * This Optimum is reached following the algorithm elaborated in Sigcomm'17 (draft) paper
	 */
	double ReachOptimum(Ptr<DependentMARLState>);
	bool UpdateQValue(double optvalue);

private:
	double		m_learningrate;
	double		m_discount;
	std::unordered_map<QVariable, double, QHasher> m_Qfunc;

	Ptr<DependentMARLAction>	m_currentaction;
	Ptr<DependentMARLState>		m_currentstate;

	Ptr<DependentMARLAction> 	m_lastoptaction;
	Ptr<DependentMARLState>		m_nextstate;

private:
	static LogComponent g_log; ///< @brief Logging variable

protected:
	virtual void NotifyNewAggregate (); ///< @brief Even when object is aggregated to another Object
	virtual void DoDispose (); ///< @brief Do cleanup
};


//////////////////////////////////////////
////////// Implementation ////////////////
//////////////////////////////////////////

template<typename State, typename Action>
LogComponent MARLnfaImpl<State,Action>::g_log = LogComponent(("ndn.rl.marl."
		+ State::GetName() + "." + Action::GetName()).c_str());

template<typename State, typename Action>
TypeId MARLnfaImpl<State,Action>::GetTypeId() {
	static TypeId tid =
			TypeId(("ns3::ndn::rl::" + State::GetName() + "::" + Action::GetName()).c_str())
			.SetGroupName("Ndn")
			.SetParent<MARLnfa>()
			.AddConstructor<MARLnfaImpl<State,Action> >()

			.AddAttribute("LearningRate", "Represented by alpha in the paper",
						DoubleValue (0.2),
						MakeDoubleAccessor (&MARLnfaImpl<State,Action>::m_learningrate),
						MakeDoubleChecker<double> ())

			.AddAttribute("Discount", "Represented by gamma in the paper",
						DoubleValue (0.6),
						MakeDoubleAccessor (&MARLnfaImpl<State,Action>::m_discount),
						MakeDoubleChecker<double> ())
						;

	return tid;
}

template<typename State, typename Action>
void MARLnfaImpl<State,Action>::DoDispose ()
{
	m_lastoptaction = 0;
	m_nextstate = 0;

	m_currentaction = 0;
	m_currentstate = 0;

	delete [] m_csinitpartition;
	m_csinitpartition = nullptr;

	Object::DoDispose ();
}

template<typename State, typename Action>
void MARLnfaImpl<State,Action>::NotifyNewAggregate ()
{
  if (m_node == 0)
  {
      m_node = GetObject<Node>();
      m_lastoptaction = Create<Action>(m_node);
      m_nextstate = Create<State>(m_node, m_granPlan);

      m_currentaction = Create<Action>(m_node);
      m_currentstate = Create<State>(m_node, m_granPlan);

      m_csinitpartition = new double [m_node->GetObject<NDNBitRate>()->GetTableSize()];
  }
  if (m_cs == 0)
  {
      m_cs = GetObject<ContentStore> ();
  }

  Object::NotifyNewAggregate ();
}

template<typename State, typename Action>
std::size_t MARLnfaImpl<State, Action>::GetQStatus() const
{
	return m_Qfunc.size();
}

template<typename State, typename Action>
void MARLnfaImpl<State,Action>::ResetToInit()
{
	MARLnfa::ResetToInit();

	Ptr<BasicMARLState> localptr = ConstCast<BasicMARLState>(m_currentstate->GetLocalState());
	localptr->ResetToInit();

	// Randomly assign the cache partition
	Ptr<NDNBitRate> BRinfo = m_node->GetObject<NDNBitRate>();
	uint32_t BRnum = BRinfo->GetTableSize();
	double* ratiosarr =  new double [BRnum];

//	Generate the random initial cache partition for each episode. // Deserted
/*
	std::vector<double> ratios;

	ratios.push_back(0);

	UniformVariable rndnum;
	uint32_t limit = static_cast<uint32_t>(1 / localptr->GetGranularity());

	for(uint32_t i = 1; i < BRnum; i++)
	{
		bool noconflict = false;
		uint32_t num;
		while(!noconflict)
		{
			noconflict = true;
			num = rndnum.GetInteger(0, limit);
			for(uint32_t j = 1; j < i; j++)
			{
				if(ratios[j] == num)
					noconflict = false;
			}
		}
		ratios.push_back(num);
	}
	std::sort(ratios.begin(), ratios.end());

	for(uint32_t i = 0; i < BRnum - 1; i++)
	{
		ratiosarr[i] = (ratios[i + 1] - ratios[i]) * localptr->GetGranularity();
	}
	ratiosarr[BRnum - 1] = (limit - ratios[BRnum - 1]) * localptr->GetGranularity();

*/
	if(m_newstart)
	{
		for(uint32_t i = 0; i < BRnum; i++)
		{
			ratiosarr[i] = m_cs->GetSectionRatio(BRinfo->GetBRFromRank(i+1));
		}
		localptr->AlignPartitionArr(ratiosarr);
		for(uint32_t i = 0; i < BRnum; i++)
			m_csinitpartition[i] = ratiosarr[i];
		m_newstart = false;
	}
	else
	{
		for(uint32_t i = 0; i < BRnum; i++)
			ratiosarr[i]= m_csinitpartition[i];
		localptr->SetPartition(ratiosarr);
	}
	// Make sure the partition in ContentStore is the same as the partition in local BasicMARLState
	m_cs->SetSectionRatio(ratiosarr, BRnum);

	Ptr<BasicMARLState> nextptr = ConstCast<BasicMARLState>(m_nextstate->GetLocalState());
	nextptr->ResetToInit();

	delete [] ratiosarr;
	ratiosarr = nullptr;
}

template<typename State, typename Action>
void MARLnfaImpl<State,Action>::AddToHset(Ptr<Node> nodeptr)
{
	MARLnfa::AddToHset(nodeptr);
	Ptr<MARLnfa> marlptr = nodeptr->GetObject<MARLnfa>();
	if(marlptr != 0)
	{
		m_currentstate->SetNeighborState(marlptr->GetCurrentState());
		m_currentaction->SetNeighborAction(marlptr->GetCurrentAction());

		m_nextstate->SetNeighborState(marlptr->GetNextState());
		m_lastoptaction->SetNeighborAction(marlptr->GetOptAction());

	}

}
//When greedy algorithm for one video-producer scenario is applied,
//The sequence of calling UpdateCurrentState() must be taken into extra consideration.
template<typename State, typename Action>
void MARLnfaImpl<State,Action>::UpdateCurrentState()
{
	Ptr<BasicMARLState> nextptr = ConstCast<BasicMARLState>(m_nextstate->GetLocalState());
	if(nextptr->CheckInit())
	{
		// Update the current request state
		m_currentstate->UpdateRequest();
	}
	else
	{
		m_currentstate->Assign(m_nextstate);
	}
	//if(m_node->GetId() == 8)
		//std::cout << "[Request]: " << m_currentstate->GetRequestStatus()->PrintState() << std::endl;
	// Generate the aggregated state in m_neighborstates if we choose aggregation
	m_currentstate->SummarizeNeighborState();


	if(m_log != nullptr)
	{
		*m_log << "[Node:" << m_node->GetId() << "(state)] Local: " << m_currentstate->GetLocalState()->PrintState() << std::endl;
		*m_log << "[Node:" << m_node->GetId() << "(state)] Request: " << m_currentstate->GetRequestStatus()->PrintState() << std::endl;
		auto nbptr =  m_currentstate->GetNeighborStates().begin();
		*m_log << "[Node:" << m_node->GetId() << "(state)] Neighbor: ";
		for(; nbptr != m_currentstate->GetNeighborStates().end(); nbptr++)
			*m_log << "(Node:" << nbptr->first << ") " << nbptr->second->PrintState() << std::endl;
	}

}

template<typename State, typename Action>
bool MARLnfaImpl<State,Action>::UpdateCurrentAction(bool exploitation)
{
	// Bug Fix 2016-12-17 Logic error here
	//////Update Action//////////////////////////
	if(exploitation && !m_nextstate->GetLocalState()->CheckInit())
		m_currentaction->Assign(m_lastoptaction);
	else if(!exploitation)
	{
		if(m_useGuidedHeuristics == 0)
			m_currentaction->RandomSetLocalAction();
		else
			m_currentaction->GuidedSetLocalAction();
	}
	else
	{
		ReachOptimum(m_currentstate);
		m_currentaction->Assign(m_lastoptaction);
	}
	m_currentaction->SummarizeNeighborAction(m_experimentalAggregation);
	////////////////////////////////////////////
//For Debugging
	//if(m_node->GetId() == 8)
		//NS_LOG_DEBUG("[UpdateAction]: " << m_currentaction->GetLocalAction()->PrintAction());
		//std::cout << "[UpdateAction]: " << m_currentaction->GetLocalAction()->PrintAction() << std::endl;
	//////Apply Action//////////////////////////
	if(m_nextstate->GetLocalState()->CheckInit())
		m_nextstate->Assign(m_currentstate);
	bool flag = m_nextstate->ApplyAction(m_currentaction);
	if(flag && m_log != nullptr)
	{
		*m_log << "[Node:" << m_node->GetId() << "(action)] Local: " << m_currentaction->GetLocalAction()->PrintAction() << std::endl;
		auto nbptr = m_currentaction->GetNeighborActions().begin();
		*m_log << "[Node:" << m_node->GetId() << "(action)] Neighbor: ";
		for(; nbptr != m_currentaction->GetNeighborActions().end(); nbptr++)
			*m_log << "(Node:" << nbptr->first << ") " << nbptr->second->PrintAction() << std::endl;
	}
	return flag;
}
template<typename State, typename Action>
bool MARLnfaImpl<State,Action>::ManualSetCurrentAction(int up, int down)
{
	int8_t* action = new int8_t [m_node->GetObject<NDNBitRate>()->GetTableSize()];
	for(uint32_t i = 0; i < m_node->GetObject<NDNBitRate>()->GetTableSize(); i++)
		action[i] = 0;
	if(up >= 0 && down >= 0)
	{
		action[up] = 1;
		action[down] = -1;
	}
	m_currentaction->SetLocalAction(action);
	delete [] action;
	action = nullptr;

	m_currentaction->SummarizeNeighborAction(m_experimentalAggregation);
	if(m_nextstate->GetLocalState()->CheckInit())
		m_nextstate->Assign(m_currentstate);
	return m_nextstate->ApplyAction(m_currentaction);
}
template<typename State, typename Action>
bool MARLnfaImpl<State,Action>::Transition()
{
	// Generate the aggregated state in m_neighborstates if we choose aggregation
	m_nextstate->SummarizeNeighborState();

	// Update the current request state
	m_nextstate->UpdateRequest();

	//Choose the Global optimum (action and Q-Value)
	double q = ReachOptimum(m_nextstate);

	return UpdateQValue(q);

}

template<typename State, typename Action>
double MARLnfaImpl<State,Action>::ReachOptimum(Ptr<DependentMARLState> probeState)
{
	m_lastoptaction->SummarizeNeighborAction(m_experimentalAggregation);
	uint32_t limit = probeState->GetLocalState()->GetNumBR();
	int8_t* arr = new int8_t [limit];
	int8_t* opt_arr = new int8_t [limit];
	bool entry_in_map = false;
	double maxvalue;

	for(uint32_t i = 0 ; i< limit; i++)
	{
		arr[i] = 0;
		opt_arr[i] = 0;
	}
	m_lastoptaction->SetLocalAction(arr);
	auto targetQ = m_Qfunc.find(QVariable{m_node, probeState, m_lastoptaction});
	if(targetQ != m_Qfunc.end())
	{
		maxvalue = targetQ->second;
		entry_in_map = true;
		for(uint32_t i = 0 ; i< limit; i++)
			opt_arr[i] = arr[i];
	}

	for(uint32_t i = 0; i < limit; i++)
	{
		for(uint32_t j = 0; j < limit; j++)
		{
			if(i != j)
			{
				arr[i] = 1;
				arr[j] = -1;
				m_lastoptaction->SetLocalAction(arr);
				auto targetQ = m_Qfunc.find(QVariable{m_node, probeState, m_lastoptaction});
				if(targetQ != m_Qfunc.end())
				{
					if(!entry_in_map)
					{
						maxvalue = targetQ->second;
						entry_in_map = true;
						for(uint32_t k = 0 ; k< limit; k++)
							opt_arr[k] = arr[k];
					}
					else
					{
						double instvalue = targetQ->second;
						if(instvalue > maxvalue)
						{
							maxvalue = instvalue;
							for(uint32_t k = 0 ; k< limit; k++)
								opt_arr[k] = arr[k];
						}
					}
				}
				arr[i] = 0;
				arr[j] = 0;
			}
		}
	}
	m_lastoptaction->SetLocalAction(opt_arr);
	
	delete [] arr;
	delete [] opt_arr;

	arr = nullptr;
	opt_arr = nullptr;

	if(entry_in_map)
	{
		//if(m_node->GetId() == 1)
			//NS_LOG_DEBUG("[Optimal]: Optimal Action :\n"<<
					//m_lastoptaction->GetLocalAction()->PrintAction());
		return maxvalue;
	}
	else
	{
		//if(m_node->GetId() == 1)
			//NS_LOG_DEBUG("[Optimal]: Transition to New State:\n"<<
					//probeState->GetLocalState()->PrintState());
		return 0;
	}
}

template<typename State, typename Action>
bool MARLnfaImpl<State,Action>::UpdateQValue(double optvalue)
{
	std::unordered_map<QVariable, double, QHasher>::iterator updateQ
					= m_Qfunc.find(QVariable{m_node, m_currentstate, m_currentaction});
	bool status;
	if(updateQ != m_Qfunc.end())
	{
		/*
		if(m_node->GetId() == 1)
		NS_LOG_DEBUG("[QFuction]: QUpdate:\n" << "LocalState: "<<
				m_currentstate->GetLocalState()->PrintState() << "\n"
				<< "Request: " << m_currentstate->GetRequestStatus()->PrintState() << "\n"
				<< "NextState: " << m_nextstate->GetLocalState()->PrintState() << "\n"
				<< "NextReqest: " << m_nextstate->GetRequestStatus()->PrintState());
		*/
		double Q = updateQ->second;
		Q = Q + m_learningrate * (m_cs->GetReward() + m_discount * optvalue - Q);
		updateQ->second = Q;
		status = true;
		m_visits++;
	}
	else
	{
		// Need to confirm how to initialize Q value:
		// The sutton's book says this Q-value can be initialized randomly.
		/*
		if(m_node->GetId() == 1)
		{
			NS_LOG_DEBUG("[QFuction]: NewQ:\n" << "LocalState: "<<
				m_currentstate->GetLocalState()->PrintState() << "\n"
				<< "Request: " << m_currentstate->GetRequestStatus()->PrintState() << "\n"
				<< "NextState: " << m_nextstate->GetLocalState()->PrintState() << "\n"
				<< "NextReqest: " << m_nextstate->GetRequestStatus()->PrintState());
			const int8_t* tempa = m_currentaction->GetLocalAction()->GetChoices();
			const int* temps = m_currentstate->GetLocalState()->GetIntPartition();
			if(temps[0] == 100 &&
					(tempa[0] == 0 && (tempa[1] != 0 || tempa[2] != 0 || tempa[3] != 0))
			)
				std::cout << "Bug in UpdateAction" << std::endl;

		}
		*/
		m_Qfunc[QVariable{m_node, m_currentstate, m_currentaction}] =
				m_learningrate * (m_cs->GetReward() + m_discount * optvalue);
		status = false;
	}
	if(m_log != nullptr)
		*m_log << "Reward![Node:" << m_node->GetId() << "]: " << m_cs->GetReward() << std::endl;
	return status;
}

template<typename State, typename Action>
void MARLnfaImpl<State,Action>::OutputQStatus(std::fstream* qRecorder)
{
	auto qPtr = m_Qfunc.begin();
	for(; qPtr != m_Qfunc.end(); qPtr++)
	{
		*qRecorder << "S\t" << qPtr->first.m_state_arr.size() << "\t";
		for(uint32_t i = 0; i < qPtr->first.m_state_arr.size(); i++)
			*qRecorder << static_cast<int32_t>((qPtr->first.m_state_arr)[i]) << "\t";
		*qRecorder << "\nA\t" << qPtr->first.m_action_arr.size() << "\t";
		for(uint32_t i = 0; i < qPtr->first.m_action_arr.size(); i++)
			*qRecorder << static_cast<int32_t>((qPtr->first.m_action_arr)[i]) << "\t";
		*qRecorder << "\nR\t";
		*qRecorder << std::fixed << std::setprecision(6) << qPtr->second << std::endl << "!\n";
	}
	*qRecorder << "%\n";
}

template<typename State, typename Action>
void MARLnfaImpl<State,Action>::InputQStatus(std::fstream* qRecorder)
{
	std::vector<int8_t> state;
	std::vector<int8_t> action;
	double reward;

	std::string line;
	while(std::getline(*qRecorder,line))
	{
		if (line == "") continue;
		if (line == "%") break;
		if (line == "!") // Create a QVariable
		{
			m_Qfunc[QVariable{state, action}] = reward;
			state.clear();
			action.clear();
			reward = 0;
			continue;
		}

		char type;
		std::istringstream lineBuffer (line);

		lineBuffer >> type;
		if(type == 'R')
			lineBuffer >> reward;
		if(type == 'S')
		{
			uint32_t v;
			int temp;
			lineBuffer >> v;
			for(uint32_t i = 0; i < v; i++)
			{
				lineBuffer >> temp;
				state.push_back(static_cast<int8_t>(temp));
			}

		}
		if(type == 'A')
		{
			uint32_t v;
			int temp;
			lineBuffer >> v;
			for(uint32_t i = 0; i < v; i++)
			{
				lineBuffer >> temp;
				action.push_back(static_cast<int8_t>(temp));
			}
		}
	}
}

}
}
#endif

