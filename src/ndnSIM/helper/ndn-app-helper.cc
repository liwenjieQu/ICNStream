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
 * Author: Ilya Moiseenko <iliamo@cs.ucla.edu>
 */

#include "ndn-app-helper.h"
#include "ns3/log.h"
#include "ns3/string.h"
#include "ns3/names.h"
#include "ns3/ndn-app.h"
#include "ns3/ndn-bitrate.h"

#ifdef NS3_MPI
#include "ns3/mpi-interface.h"
#endif

NS_LOG_COMPONENT_DEFINE ("ndn.AppHelper");

namespace ns3 {
namespace ndn {

AppHelper::AppHelper (const std::string &app)
{
  m_factory.SetTypeId (app);
  m_bitrate_factory.SetTypeId ("ns3::ndn::VideoBitRate");
}

void
AppHelper::SetPrefix (const std::string &prefix)
{
  m_factory.Set ("Prefix", StringValue(prefix));
}

void 
AppHelper::SetAttribute (std::string name, const AttributeValue &value)
{
  m_factory.Set (name, value);
}
    
ApplicationContainer
AppHelper::Install (Ptr<Node> node)
{
  ApplicationContainer apps;

  Ptr<Application> app = InstallPriv (node);
  if (app != 0)
    apps.Add (app);
  
  return apps;
}
    
ApplicationContainer
AppHelper::Install (std::string nodeName)
{
  Ptr<Node> node = Names::Find<Node> (nodeName);
  return Install (node);
}
    
ApplicationContainer
AppHelper::Install (NodeContainer c)
{
  ApplicationContainer apps;
  for (NodeContainer::Iterator i = c.Begin (); i != c.End (); ++i)
    {
      Ptr<Application> app = InstallPriv (*i);
      if (app != 0)
        apps.Add (app);
    }
    
  return apps;
}
    
Ptr<Application>
AppHelper::InstallPriv (Ptr<Node> node)
{
#ifdef NS3_MPI
  if (MpiInterface::IsEnabled () &&
      node->GetSystemId () != MpiInterface::GetSystemId ())
    {
      // don't create an app if MPI is enabled and node is not in the correct partition
      return 0;
    }
#endif
  
  Ptr<Application> app = m_factory.Create<Application> ();        
  node->AddApplication (app);
        
  return app;
}

void
AppHelper::SetBitrateInfo(Ptr<Node> node, const std::string brArray[], int size)
{
	Ptr<NDNBitRate> m_bitinfo = node->GetObject<NDNBitRate>();
	for(int i = 0; i < size; i++)
	{
		m_bitinfo->AddBitRate(brArray[i]);
	}
}

ApplicationContainer
AppHelper::VideoInstall (Ptr<Node> node,const std::string brArray[], int size)
{
  ApplicationContainer apps;
  if(node->GetNApplications() == 0)
	  node->AggregateObject(m_bitrate_factory.Create<NDNBitRate>());

  Ptr<Application> app = InstallPriv (node);
  SetBitrateInfo(node,brArray,size);
  if (app != 0)
    apps.Add (app);

  return apps;
}

/*
ApplicationContainer
AppHelper::VideoInstall (NodeContainer c, const std::string brArray[], int size)
{
  ApplicationContainer apps;
  for (NodeContainer::Iterator i = c.Begin (); i != c.End (); ++i)
    {
	  (*i)->AggregateObject(m_bitrate_factory.Create<NDNBitRate>());
      Ptr<Application> app = InstallPriv (*i);
      SetBitrateInfo(*i,brArray,size);
      if (app != 0)
        apps.Add (app);
    }

  return apps;
}
*/

} // namespace ndn
} // namespace ns3
