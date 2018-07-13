#include "stdafx.h"

CMemoryPool<CNPacket> CNPacket::m_PacketPool(0);
list<CNPacket *> CNPacket::m_Chaser;