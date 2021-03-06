/*
 * mmwave-ue-mac.cc
 *
 *  Created on: May 1, 2015
 *      Author: root
 */

#include "mmwave-ue-mac.h"
#include "mmwave-phy-sap.h"
#include <ns3/log.h>

namespace ns3
{
NS_LOG_COMPONENT_DEFINE ("MmWaveUeMac");

NS_OBJECT_ENSURE_REGISTERED (MmWaveUeMac);

uint8_t MmWaveUeMac::g_raPreambleId = 0;

///////////////////////////////////////////////////////////
// SAP forwarders
///////////////////////////////////////////////////////////


class UeMemberLteUeCmacSapProvider : public LteUeCmacSapProvider
{
public:
  UeMemberLteUeCmacSapProvider (MmWaveUeMac* mac);

  // inherited from LteUeCmacSapProvider
  virtual void ConfigureRach (RachConfig rc);
  virtual void StartContentionBasedRandomAccessProcedure ();
  virtual void StartNonContentionBasedRandomAccessProcedure (uint16_t rnti, uint8_t preambleId, uint8_t prachMask);
  virtual void AddLc (uint8_t lcId, LteUeCmacSapProvider::LogicalChannelConfig lcConfig, LteMacSapUser* msu);
  virtual void RemoveLc (uint8_t lcId);
  virtual void Reset ();

private:
  MmWaveUeMac* m_mac;
};


UeMemberLteUeCmacSapProvider::UeMemberLteUeCmacSapProvider (MmWaveUeMac* mac)
  : m_mac (mac)
{
}

void
UeMemberLteUeCmacSapProvider::ConfigureRach (RachConfig rc)
{
  m_mac->DoConfigureRach (rc);
}

  void
UeMemberLteUeCmacSapProvider::StartContentionBasedRandomAccessProcedure ()
{
  m_mac->DoStartContentionBasedRandomAccessProcedure ();
}

 void
UeMemberLteUeCmacSapProvider::StartNonContentionBasedRandomAccessProcedure (uint16_t rnti, uint8_t preambleId, uint8_t prachMask)
{
  m_mac->DoStartNonContentionBasedRandomAccessProcedure (rnti, preambleId, prachMask);
}


void
UeMemberLteUeCmacSapProvider::AddLc (uint8_t lcId, LogicalChannelConfig lcConfig, LteMacSapUser* msu)
{
  m_mac->AddLc (lcId, lcConfig, msu);
}

void
UeMemberLteUeCmacSapProvider::RemoveLc (uint8_t lcid)
{
  m_mac->DoRemoveLc (lcid);
}

void
UeMemberLteUeCmacSapProvider::Reset ()
{
  m_mac->DoReset ();
}


class UeMemberLteMacSapProvider : public LteMacSapProvider
{
public:
  UeMemberLteMacSapProvider (MmWaveUeMac* mac);

  // inherited from LteMacSapProvider
  virtual void TransmitPdu (TransmitPduParameters params);
  virtual void ReportBufferStatus (ReportBufferStatusParameters params);

private:
  MmWaveUeMac* m_mac;
};


UeMemberLteMacSapProvider::UeMemberLteMacSapProvider (MmWaveUeMac* mac)
  : m_mac (mac)
{
}

void
UeMemberLteMacSapProvider::TransmitPdu (TransmitPduParameters params)
{
  m_mac->DoTransmitPdu (params);
}


void
UeMemberLteMacSapProvider::ReportBufferStatus (ReportBufferStatusParameters params)
{
  m_mac->DoReportBufferStatus (params);
}


class MmWaveUePhySapUser;

class MacUeMemberPhySapUser : public MmWaveUePhySapUser
{
public:
	MacUeMemberPhySapUser (MmWaveUeMac* mac);

	virtual void ReceivePhyPdu (Ptr<Packet> p);

	virtual void ReceiveControlMessage (Ptr<MmWaveControlMessage> msg);

	virtual void SubframeIndication (uint32_t frameNo, uint32_t subframeNo, uint32_t slotNo);

private:
	MmWaveUeMac* m_mac;
};

MacUeMemberPhySapUser::MacUeMemberPhySapUser (MmWaveUeMac* mac)
:m_mac(mac)
{

}
void
MacUeMemberPhySapUser::ReceivePhyPdu (Ptr<Packet> p)
{
	m_mac->DoReceivePhyPdu(p);
}

void
MacUeMemberPhySapUser::ReceiveControlMessage (Ptr<MmWaveControlMessage> msg)
{
	m_mac->DoReceiveControlMessage(msg);
}

void
MacUeMemberPhySapUser::SubframeIndication (uint32_t frameNo, uint32_t subframeNo, uint32_t slotNo)
{
	m_mac->DoSubframeIndication(frameNo, subframeNo, slotNo);
}

//-----------------------------------------------------------------------

TypeId
MmWaveUeMac::GetTypeId (void)
{
	static TypeId tid = TypeId ("ns3::MmWaveUeMac")
			.SetParent<MmWaveMac> ()
			.AddConstructor<MmWaveUeMac> ()
	;
	return tid;
}

MmWaveUeMac::MmWaveUeMac (void)
: m_bsrPeriodicity (MilliSeconds (1)), // ideal behavior
  m_bsrLast (MilliSeconds (0)),
  m_freshUlBsr (false),
  m_harqProcessId (0),
  m_rnti (0),
  m_waitingForRaResponse (true)
{
	NS_LOG_FUNCTION (this);
	m_cmacSapProvider = new UeMemberLteUeCmacSapProvider (this);
	m_macSapProvider = new UeMemberLteMacSapProvider (this);
	m_phySapUser = new MacUeMemberPhySapUser (this);
	m_raPreambleUniformVariable = CreateObject<UniformRandomVariable> ();
}

MmWaveUeMac::~MmWaveUeMac (void)
{
  NS_LOG_FUNCTION (this);
}

void
MmWaveUeMac::DoDispose ()
{
  NS_LOG_FUNCTION (this);
  m_miUlHarqProcessesPacket.clear ();
  delete m_macSapProvider;
  delete m_cmacSapProvider;
  delete m_phySapUser;
  Object::DoDispose ();
}

void
MmWaveUeMac::SetCofigurationParameters (Ptr<MmWavePhyMacCommon> ptrConfig)
{
	m_phyMacConfig = ptrConfig;

  m_miUlHarqProcessesPacket.resize (m_phyMacConfig->GetNumHarqProcess ());
  for (uint8_t i = 0; i < m_miUlHarqProcessesPacket.size (); i++)
    {
      Ptr<PacketBurst> pb = CreateObject <PacketBurst> ();
      m_miUlHarqProcessesPacket.at (i) = pb;
    }
  m_miUlHarqProcessesPacketTimer.resize (m_phyMacConfig->GetNumHarqProcess (), 0);
}

Ptr<MmWavePhyMacCommon>
MmWaveUeMac::GetConfigurationParameters (void) const
{
	return m_phyMacConfig;
}

// forwarded from MAC SAP
void
MmWaveUeMac::DoTransmitPdu (LteMacSapProvider::TransmitPduParameters params)
{
	// TB UID passed back along with RLC data as HARQ process ID
	uint32_t tbMapKey = ((params.rnti & 0xFFFF) << 8) | (params.harqProcessId & 0xFF);
	std::map<uint32_t, struct MacPduInfo>::iterator it = m_macPduMap.find (tbMapKey);
	if (it == m_macPduMap.end ())
	{
		NS_FATAL_ERROR ("No MAC PDU storage element found for this TB UID/RNTI");
	}
	else
	{
		if(it->second.m_pdu == 0)
		{
			it->second.m_pdu = params.pdu;
		}
		else
		{
			it->second.m_pdu->AddAtEnd (params.pdu); // append to MAC PDU
		}
		MacSubheader subheader (params.lcid, params.pdu->GetSize ());
		it->second.m_macHeader.AddSubheader (subheader); // add RLC PDU sub-header into MAC header

		if (it->second.m_numRlcPdu == 1)
		{
			// wait for all RLC PDUs to be received
			it->second.m_pdu->AddHeader (it->second.m_macHeader);
			LteRadioBearerTag bearerTag (params.rnti, it->second.m_size, 0);
			it->second.m_pdu->AddPacketTag (bearerTag);
			m_miUlHarqProcessesPacket.at (m_harqProcessId)->AddPacket (params.pdu);
			m_miUlHarqProcessesPacketTimer.at (m_harqProcessId) = m_phyMacConfig->GetHarqTimeout ();
			m_phySapProvider->SendMacPdu (it->second.m_pdu);
			m_macPduMap.erase (it);  // delete map entry
		}
		else
		{
			it->second.m_numRlcPdu--; // decrement count of remaining RLC requests
		}
	}
}

void
MmWaveUeMac::DoReportBufferStatus (LteMacSapProvider::ReportBufferStatusParameters params)
{
  NS_LOG_FUNCTION (this << (uint32_t) params.lcid);

  std::map <uint8_t, LteMacSapProvider::ReportBufferStatusParameters>::iterator it;

  it = m_ulBsrReceived.find (params.lcid);
  if (it!=m_ulBsrReceived.end ())
    {
      // update entry
      (*it).second = params;
    }
  else
    {
      m_ulBsrReceived.insert (std::pair<uint8_t, LteMacSapProvider::ReportBufferStatusParameters> (params.lcid, params));
    }
  m_freshUlBsr = true;
}


void
MmWaveUeMac::SendReportBufferStatus (void)
{
  NS_LOG_FUNCTION (this);

  if (m_rnti == 0)
    {
      NS_LOG_INFO ("MAC not initialized, BSR deferred");
      return;
    }

  if (m_ulBsrReceived.size () == 0)
    {
      NS_LOG_INFO ("No BSR report to transmit");
      return;
    }
  MacCeElement bsr;
  bsr.m_rnti = m_rnti;
  bsr.m_macCeType = MacCeElement::BSR;

  // BSR is reported for each LCG
  std::map <uint8_t, LteMacSapProvider::ReportBufferStatusParameters>::iterator it;
  std::vector<uint32_t> queue (4, 0); // one value per each of the 4 LCGs, initialized to 0
  for (it = m_ulBsrReceived.begin (); it != m_ulBsrReceived.end (); it++)
    {
      uint8_t lcid = it->first;
      std::map <uint8_t, LcInfo>::iterator lcInfoMapIt;
      lcInfoMapIt = m_lcInfoMap.find (lcid);
      NS_ASSERT (lcInfoMapIt !=  m_lcInfoMap.end ());
      NS_ASSERT_MSG ((lcid != 0) || (((*it).second.txQueueSize == 0)
                                     && ((*it).second.retxQueueSize == 0)
                                     && ((*it).second.statusPduSize == 0)),
                     "BSR should not be used for LCID 0");
      uint8_t lcg = lcInfoMapIt->second.lcConfig.logicalChannelGroup;
      queue.at (lcg) += ((*it).second.txQueueSize + (*it).second.retxQueueSize + (*it).second.statusPduSize);
    }

  // FF API says that all 4 LCGs are always present
  bsr.m_macCeValue.m_bufferStatus.push_back (BufferSizeLevelBsr::BufferSize2BsrId (queue.at (0)));
  bsr.m_macCeValue.m_bufferStatus.push_back (BufferSizeLevelBsr::BufferSize2BsrId (queue.at (1)));
  bsr.m_macCeValue.m_bufferStatus.push_back (BufferSizeLevelBsr::BufferSize2BsrId (queue.at (2)));
  bsr.m_macCeValue.m_bufferStatus.push_back (BufferSizeLevelBsr::BufferSize2BsrId (queue.at (3)));

  // create the feedback to eNB
  Ptr<MmWaveBsrMessage> msg = Create<MmWaveBsrMessage> ();
  msg->SetBsr (bsr);
  m_phySapProvider->SendControlMessage (msg);
}

void
MmWaveUeMac::SetUeCmacSapUser (LteUeCmacSapUser* s)
{
  m_cmacSapUser = s;
}

LteUeCmacSapProvider*
MmWaveUeMac::GetUeCmacSapProvider (void)
{
  return m_cmacSapProvider;
}

void
MmWaveUeMac::RefreshHarqProcessesPacketBuffer (void)
{
  NS_LOG_FUNCTION (this);

  for (uint16_t i = 0; i < m_miUlHarqProcessesPacketTimer.size (); i++)
    {
      if (m_miUlHarqProcessesPacketTimer.at (i) == 0)
        {
          if (m_miUlHarqProcessesPacket.at (i)->GetSize () > 0)
            {
              // timer expired: drop packets in buffer for this process
              NS_LOG_INFO (this << " HARQ Proc Id " << i << " packets buffer expired");
              Ptr<PacketBurst> emptyPb = CreateObject <PacketBurst> ();
              m_miUlHarqProcessesPacket.at (i) = emptyPb;
            }
        }
      else
        {
          m_miUlHarqProcessesPacketTimer.at (i)--;
        }
    }
}

void
MmWaveUeMac::DoSubframeIndication (uint32_t frameNo, uint32_t subframeNo, uint32_t slotNo)
{
	NS_LOG_FUNCTION (this);
	m_frameNum = frameNo;
	m_sfNum = subframeNo;
	m_slotNum = slotNo;
	RefreshHarqProcessesPacketBuffer ();
	if ((Simulator::Now () >= m_bsrLast + m_bsrPeriodicity) && (m_freshUlBsr==true))
	{
		SendReportBufferStatus ();
		m_bsrLast = Simulator::Now ();
		m_freshUlBsr = false;
		m_harqProcessId = (m_harqProcessId + 1) % m_phyMacConfig->GetHarqTimeout();
	}
}

void
MmWaveUeMac::DoReceivePhyPdu (Ptr<Packet> p)
{
	NS_LOG_FUNCTION (this);
	LteRadioBearerTag tag;
	p->RemovePacketTag (tag);
	MmWaveMacPduHeader macHeader;
	p->RemoveHeader (macHeader);

	if (tag.GetRnti () == m_rnti) // packet is for the current user
	{
		std::vector<MacSubheader> macSubheaders = macHeader.GetSubheaders ();
		uint32_t currPos = 0;
		for (unsigned ipdu = 0; ipdu < macSubheaders.size (); ipdu++)
		{
			std::map <uint8_t, LcInfo>::const_iterator it = m_lcInfoMap.find (macSubheaders[ipdu].m_lcid);
			NS_ASSERT_MSG (it != m_lcInfoMap.end (), "received packet with unknown lcid");
			Ptr<Packet> rlcPdu;
			if((p->GetSize ()-currPos) < (uint32_t)macSubheaders[ipdu].m_size)
			{
				NS_LOG_ERROR ("Packet size less than specified in MAC header (actual= " \
				              <<p->GetSize ()<<" header= "<<(uint32_t)macSubheaders[ipdu].m_size<<")" );
			}
			else if ((p->GetSize ()-currPos) > (uint32_t)macSubheaders[ipdu].m_size)
			{
				NS_LOG_DEBUG ("Fragmenting MAC PDU (packet size greater than specified in MAC header (actual= " \
				              <<p->GetSize ()<<" header= "<<(uint32_t)macSubheaders[ipdu].m_size<<")" );
				rlcPdu = p->CreateFragment (currPos, p->GetSize ());
				currPos += p->GetSize ();
			}
			else
			{
				rlcPdu = p->CreateFragment (currPos, (uint32_t)macSubheaders[ipdu].m_size);
				currPos += (uint32_t)macSubheaders[ipdu].m_size;
			}
			it->second.macSapUser->ReceivePdu (rlcPdu);
		}
	}
}

void
MmWaveUeMac::RecvRaResponse (BuildRarListElement_s raResponse)
{
  NS_LOG_FUNCTION (this);
  m_waitingForRaResponse = false;
  m_rnti = raResponse.m_rnti;
  m_cmacSapUser->SetTemporaryCellRnti (m_rnti);
  m_cmacSapUser->NotifyRandomAccessSuccessful ();
}

std::map<uint32_t, struct MacPduInfo>::iterator MmWaveUeMac::AddToMacPduMap (TbInfoElement tb, unsigned activeLcs)
{

	unsigned frameInd = m_frameNum;
	unsigned sfInd = m_sfNum + m_phyMacConfig->GetUlSchedDelay ();
	if (sfInd > m_phyMacConfig->GetSubframesPerFrame ())
	{
		frameInd++;
		sfInd -= m_phyMacConfig->GetSubframesPerFrame ();
	}

	MacPduInfo macPduInfo (frameInd, sfInd, tb.m_slotInd+1, tb.m_tbSize, activeLcs);
	uint32_t tbMapKey = ((m_rnti & 0xFFFF) << 8) | (tb.m_harqProcess & 0xFF);
	std::map<uint32_t, struct MacPduInfo>::iterator it = \
			(m_macPduMap.insert (std::pair<uint32_t, struct MacPduInfo> (tbMapKey, macPduInfo))).first;
	return it;
}

void
MmWaveUeMac::DoReceiveControlMessage  (Ptr<MmWaveControlMessage> msg)
{
	NS_LOG_FUNCTION (this << msg);

	switch (msg->GetMessageType())
	{
	case (MmWaveControlMessage::DCI):
		{
		Ptr<MmWaveDciMessage> dciMsg = DynamicCast <MmWaveDciMessage> (msg);
		DciInfoElement dciInfoElem = dciMsg->GetDciInfoElement ();
		std::vector<TbInfoElement>::iterator tbIt = dciInfoElem.m_tbInfoElements.begin ();
		while (tbIt != dciInfoElem.m_tbInfoElements.end ())
		{
			bool ulSlot = ((dciInfoElem.m_tddBitmap >> tbIt->m_slotInd) & 0x1);
			if (ulSlot)
			{
				if (tbIt->m_ndi==1)
				{
					// New transmission -> empty pkt buffer queue (for deleting eventual pkts not acked )
					Ptr<PacketBurst> pb = CreateObject <PacketBurst> ();
					m_miUlHarqProcessesPacket.at (m_harqProcessId) = pb;
					// Retrieve data from RLC
					std::map <uint8_t, LteMacSapProvider::ReportBufferStatusParameters>::iterator itBsr;
					uint16_t activeLcs = 0;
					uint32_t statusPduMinSize = 0;
					for (itBsr = m_ulBsrReceived.begin (); itBsr != m_ulBsrReceived.end (); itBsr++)
					{
						if (((*itBsr).second.statusPduSize > 0) || ((*itBsr).second.retxQueueSize > 0) || ((*itBsr).second.txQueueSize > 0))
						{
							activeLcs++;
							if (((*itBsr).second.statusPduSize!=0)&&((*itBsr).second.statusPduSize < statusPduMinSize))
							{
								statusPduMinSize = (*itBsr).second.statusPduSize;
							}
							if (((*itBsr).second.statusPduSize!=0)&&(statusPduMinSize == 0))
							{
								statusPduMinSize = (*itBsr).second.statusPduSize;
							}
						}
					}
					if (activeLcs == 0)
					{
						NS_LOG_ERROR (this << " No active flows for this UL-DCI");
						return;
					}
//					std::map<uint32_t, struct MacPduInfo>::iterator macPduMapIt = AddToMacPduMap (*tbIt, activeLcs);
					AddToMacPduMap (*tbIt, activeLcs);
					std::map <uint8_t, LcInfo>::iterator lcIt;
					uint32_t bytesPerActiveLc = tbIt->m_tbSize / activeLcs;
					bool statusPduPriority = false;
					if ((statusPduMinSize != 0)&&(bytesPerActiveLc < statusPduMinSize))
					{
						// send only the status PDU which has highest priority
						statusPduPriority = true;
						NS_LOG_DEBUG (this << " Reduced resource -> send only Status, bytes " << statusPduMinSize);
						if (tbIt->m_tbSize < statusPduMinSize)
						{
							NS_FATAL_ERROR ("Insufficient Tx Opportunity for sending a status message");
						}
					}
					NS_LOG_LOGIC (this << " UE " << m_rnti << ": UL-CQI notified TxOpportunity of " << tbIt->m_tbSize << " => " << bytesPerActiveLc << " bytes per active LC" << " statusPduMinSize " << statusPduMinSize);
					for (lcIt = m_lcInfoMap.begin (); lcIt!=m_lcInfoMap.end (); lcIt++)
					{
						itBsr = m_ulBsrReceived.find ((*lcIt).first);
						NS_LOG_DEBUG (this << " Processing LC " << (uint32_t)(*lcIt).first << " bytesPerActiveLc " << bytesPerActiveLc);
						if ( (itBsr!=m_ulBsrReceived.end ()) &&
								( ((*itBsr).second.statusPduSize > 0) ||
										((*itBsr).second.retxQueueSize > 0) ||
										((*itBsr).second.txQueueSize > 0)) )
						{
							if ((statusPduPriority) && ((*itBsr).second.statusPduSize == statusPduMinSize))
							{
								MacSubheader subheader((*lcIt).first,(*itBsr).second.statusPduSize);
								(*lcIt).second.macSapUser->NotifyTxOpportunity (((*itBsr).second.statusPduSize - subheader.GetSize ()), 0, tbIt->m_harqProcess);
								NS_LOG_LOGIC (this << "\t" << bytesPerActiveLc << " send  " << (*itBsr).second.statusPduSize << " status bytes to LC " << (uint32_t)(*lcIt).first << " statusQueue " << (*itBsr).second.statusPduSize << " retxQueue" << (*itBsr).second.retxQueueSize << " txQueue" <<  (*itBsr).second.txQueueSize);
								(*itBsr).second.statusPduSize = 0;
								break;
							}
							else
							{
								uint32_t bytesForThisLc = bytesPerActiveLc;
								NS_LOG_LOGIC (this << "\t" << bytesPerActiveLc << " bytes to LC " << (uint32_t)(*lcIt).first << " statusQueue " << (*itBsr).second.statusPduSize << " retxQueue" << (*itBsr).second.retxQueueSize << " txQueue" <<  (*itBsr).second.txQueueSize);
								if (((*itBsr).second.statusPduSize > 0) && (bytesForThisLc > (*itBsr).second.statusPduSize))
								{
									MacSubheader subheader((*lcIt).first,(*itBsr).second.statusPduSize);
									(*lcIt).second.macSapUser->NotifyTxOpportunity (((*itBsr).second.statusPduSize - subheader.GetSize ()), 0, tbIt->m_harqProcess);
									bytesForThisLc -= (*itBsr).second.statusPduSize;
									NS_LOG_DEBUG (this << " serve STATUS " << (*itBsr).second.statusPduSize);
									(*itBsr).second.statusPduSize = 0;
								}
								else
								{
									if ((*itBsr).second.statusPduSize>bytesForThisLc)
									{
										NS_FATAL_ERROR ("Insufficient Tx Opportunity for sending a status message");
									}
								}

								if ((bytesForThisLc > 7) && // 7 is the min TxOpportunity useful for Rlc
										(((*itBsr).second.retxQueueSize > 0) ||
												((*itBsr).second.txQueueSize > 0)))
								{
									if ((*itBsr).second.retxQueueSize > 0)
									{
										NS_LOG_DEBUG (this << " serve retx DATA, bytes " << bytesForThisLc);
										MacSubheader subheader((*lcIt).first, bytesForThisLc);
										(*lcIt).second.macSapUser->NotifyTxOpportunity ((bytesForThisLc - subheader.GetSize ()), 0, tbIt->m_harqProcess);
										if ((*itBsr).second.retxQueueSize >= bytesForThisLc)
										{
											(*itBsr).second.retxQueueSize -= bytesForThisLc;
										}
										else
										{
											(*itBsr).second.retxQueueSize = 0;
										}
									}
									else if ((*itBsr).second.txQueueSize > 0)
									{
										uint16_t lcid = (*lcIt).first;
										uint32_t rlcOverhead;
										if (lcid == 1)
										{
											// for SRB1 (using RLC AM) it's better to
											// overestimate RLC overhead rather than
											// underestimate it and risk unneeded
											// segmentation which increases delay
											rlcOverhead = 4;
										}
										else
										{
											// minimum RLC overhead due to header
											rlcOverhead = 2;
										}
										NS_LOG_DEBUG (this << " serve tx DATA, bytes " << bytesForThisLc << ", RLC overhead " << rlcOverhead);
										MacSubheader subheader((*lcIt).first, bytesForThisLc);
										(*lcIt).second.macSapUser->NotifyTxOpportunity ((bytesForThisLc - subheader.GetSize ()), 0, tbIt->m_harqProcess);
										if ((*itBsr).second.txQueueSize >= bytesForThisLc - rlcOverhead)
										{
											(*itBsr).second.txQueueSize -= bytesForThisLc - rlcOverhead;
										}
										else
										{
											(*itBsr).second.txQueueSize = 0;
										}
									}
								}
								else
								{
									if ( ((*itBsr).second.retxQueueSize > 0) || ((*itBsr).second.txQueueSize > 0))
									{
										// resend BSR info for updating eNB peer MAC
										m_freshUlBsr = true;
									}
								}
								NS_LOG_LOGIC (this << "\t" << bytesPerActiveLc << "\t new queues " << (uint32_t)(*lcIt).first << " statusQueue " << (*itBsr).second.statusPduSize << " retxQueue" << (*itBsr).second.retxQueueSize << " txQueue" <<  (*itBsr).second.txQueueSize);
							}

						}
					}
				}
				else if (tbIt->m_ndi == 0)
				{
					// HARQ retransmission -> retrieve data from HARQ buffer
					NS_LOG_DEBUG (this << " UE MAC RETX HARQ " << (uint16_t)m_harqProcessId);
					Ptr<PacketBurst> pb = m_miUlHarqProcessesPacket.at (m_harqProcessId);
					for (std::list<Ptr<Packet> >::const_iterator j = pb->Begin (); j != pb->End (); ++j)
					{
						Ptr<Packet> pkt = (*j)->Copy ();
						// update packet tag
						MmWaveMacPduTag tag;
						pkt->RemovePacketTag (tag);
						tag.SetFrameNum (m_frameNum);
						tag.SetSubframeNum (m_sfNum);
						tag.SetSlotNum (m_slotNum);
						pkt->AddPacketTag (tag);
						m_phySapProvider->SendMacPdu (pkt);
					}
					m_miUlHarqProcessesPacketTimer.at (m_harqProcessId) = m_phyMacConfig->GetHarqTimeout();
				}
			}
			tbIt++;
		}
		break;
	}
	case (MmWaveControlMessage::RAR):
	{
		if (m_waitingForRaResponse == true)
		{
			Ptr<MmWaveRarMessage> rarMsg = DynamicCast<MmWaveRarMessage> (msg);
			NS_LOG_LOGIC (this << "got RAR with RA-RNTI " << (uint32_t) rarMsg->GetRaRnti () << ", expecting " << (uint32_t) m_raRnti);
			for (std::list<MmWaveRarMessage::Rar>::const_iterator it = rarMsg->RarListBegin ();
					it != rarMsg->RarListEnd ();
					++it)
			{
				if (it->rapId == m_raPreambleId)
				{
					RecvRaResponse (it->rarPayload);
				}
			}
		}
		break;
	}

	default:
		NS_LOG_LOGIC ("Control message not supported/expected");
	}
}

MmWaveUePhySapUser*
MmWaveUeMac::GetPhySapUser ()
{
	return m_phySapUser;
}

void
MmWaveUeMac::SetPhySapProvider (MmWavePhySapProvider* ptr)
{
	m_phySapProvider = ptr;
}

void
MmWaveUeMac::DoConfigureRach (LteUeCmacSapProvider::RachConfig rc)
{
  NS_LOG_FUNCTION (this);

}

void
MmWaveUeMac::DoStartContentionBasedRandomAccessProcedure ()
{
  NS_LOG_FUNCTION (this);
  RandomlySelectAndSendRaPreamble ();
}

void
MmWaveUeMac::RandomlySelectAndSendRaPreamble ()
{
  NS_LOG_FUNCTION (this);
  bool contention = true;
  SendRaPreamble (contention);
}

void
MmWaveUeMac::SendRaPreamble(bool contention)
{
	//m_raPreambleId = m_raPreambleUniformVariable->GetInteger (0, 64 - 1);
	m_raPreambleId = g_raPreambleId++;
	/*raRnti should be subframeNo -1 */
	m_raRnti = 1;
	m_phySapProvider->SendRachPreamble(m_raPreambleId, m_raRnti);
}

void
MmWaveUeMac::DoStartNonContentionBasedRandomAccessProcedure (uint16_t rnti, uint8_t preambleId, uint8_t prachMask)
{
	NS_LOG_FUNCTION (this << " rnti" << rnti);
	NS_ASSERT_MSG (prachMask == 0, "requested PRACH MASK = " << (uint32_t) prachMask << ", but only PRACH MASK = 0 is supported");
	m_rnti = rnti;
}

void
MmWaveUeMac::AddLc (uint8_t lcId,  LteUeCmacSapProvider::LogicalChannelConfig lcConfig, LteMacSapUser* msu)
{
	NS_LOG_FUNCTION (this << " lcId" << (uint32_t) lcId);
	NS_ASSERT_MSG (m_lcInfoMap.find (lcId) == m_lcInfoMap.end (), "cannot add channel because LCID " << lcId << " is already present");

	LcInfo lcInfo;
	lcInfo.lcConfig = lcConfig;
	lcInfo.macSapUser = msu;
	m_lcInfoMap[lcId] = lcInfo;
}

void
MmWaveUeMac::DoRemoveLc (uint8_t lcId)
{
	NS_LOG_FUNCTION (this << " lcId" << lcId);
}

LteMacSapProvider*
MmWaveUeMac::GetUeMacSapProvider (void)
{
	return m_macSapProvider;
}

void
MmWaveUeMac::DoReset ()
{
	NS_LOG_FUNCTION (this);
}
//////////////////////////////////////////////


}
