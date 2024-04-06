#include <iostream>
#include <string>
#include "cpputil.h"
#include "midi.h"
#include "towns.h"


void TownsMIDI::i8251Client::Tx(unsigned char data)
{
	owner->ByteSentFromVM(portNo,data);
}

////////////////////////////////////////////////////////////

void TownsMIDI::MIDICard::ByteSentFromVM(int port,unsigned char data)
{
	// Forward to outside_world
	std::cout << "MIDI Write Port:" << port+portBase << " Data:" << cpputil::Ubtox(data) << std::endl;
}

void TownsMIDI::MIDICard::IOWriteByte(unsigned int ioport,unsigned int data,uint64_t townsTime)
{
	if(true==enabled)
	{
		switch(ioport&7)
		{
		case (TOWNSIO_MIDI_CARD1_DATREG1&7): //0x0E50,         // MIDI card(MT-402 or 403) No.1 Out port 1 datReg1 (from Linux source)
			ports[0].VMWriteData(data,townsTime);
			break;
		case (TOWNSIO_MIDI_CARD1_CMDREG1&7): //0x0E51,         // MIDI card(MT-402 or 403) No.1 cmdReg1 (Linux source)
			ports[0].VMWriteCommnand(data);
			break;
		case (TOWNSIO_MIDI_CARD1_DATREG2&7): //0x0E54,         // MIDI card(MT-402 or 403) No.1
			ports[1].VMWriteData(data,townsTime);
			break;
		case (TOWNSIO_MIDI_CARD1_CMDREG2&7): //0x0E55,         // MIDI card(MT-402 or 403) No.1
			ports[1].VMWriteCommnand(data);
			break;
		case (TOWNSIO_MIDI_CARD1_FIFODAT&7): //0x0E52,         // MIDI card(MT-402 or 403) No.1
			break;
		case (TOWNSIO_MIDI_CARD1_FIFOREG&7): //0x0E53,         // MIDI card(MT-402 or 403) No.1
			fifoReg=data;
			break;
		}
		ports[0].Update(townsTime);
		ports[1].Update(townsTime);
		ForceTxEmpty();
	}
}
unsigned int TownsMIDI::MIDICard::IOReadByte(unsigned int ioport,uint64_t townsTime)
{
	unsigned char data=0xFF;
	if(true==enabled)
	{
		switch(ioport&7)
		{
		case (TOWNSIO_MIDI_CARD1_DATREG1&7): //0x0E50,         // MIDI card(MT-402 or 403) No.1 Out port 1 datReg1 (from Linux source)
			data=ports[0].VMReadData(townsTime);
			break;
		case (TOWNSIO_MIDI_CARD1_CMDREG1&7): //0x0E51,         // MIDI card(MT-402 or 403) No.1 cmdReg1 (Linux source)
			data=ports[0].VMReadState();
			break;
		case (TOWNSIO_MIDI_CARD1_DATREG2&7): //0x0E54,         // MIDI card(MT-402 or 403) No.1
			data=ports[1].VMReadData(townsTime);
			break;
		case (TOWNSIO_MIDI_CARD1_CMDREG2&7): //0x0E55,         // MIDI card(MT-402 or 403) No.1
			data=ports[1].VMReadState();
			break;
		case (TOWNSIO_MIDI_CARD1_FIFODAT&7): //0x0E52,         // MIDI card(MT-402 or 403) No.1
			break;
		case (TOWNSIO_MIDI_CARD1_FIFOREG&7): //0x0E53,         // MIDI card(MT-402 or 403) No.1
			data=fifoReg;
			break;
		}
		ports[0].Update(townsTime);
		ports[1].Update(townsTime);
	}
	return data;
}

////////////////////////////////////////////////////////////

TownsMIDI::TownsMIDI(class FMTownsCommon *townsPtr) : Device(townsPtr)
{
	this->townsPtr=townsPtr;
	state.cards[0].portBase=0;
	state.cards[1].portBase=2;
	state.cards[2].portBase=4;
	state.cards[3].portBase=6;
	state.cards[4].portBase=0;
}

void TownsMIDI::PowerOn(void)
{
	for(auto &card : state.cards)
	{
		card.ports[0].Reset();
		card.ports[1].Reset();
		card.ForceTxEmpty();
	}
}

void TownsMIDI::Reset(void)
{
	for(auto &card : state.cards)
	{
		card.ports[0].Reset();
		card.ports[1].Reset();
		card.ForceTxEmpty();
	}
}

void TownsMIDI::UpdateInterruptRequest(void)
{
	unsigned int writeReady=0;
	for(int i=0; i<4; ++i)
	{
		if(true==state.cards[i].enabled)
		{
			writeReady|=(3<<(i*2));
		}
	}
	// I don't know what to do with FMT-401 2nd Gen.

	state.writeINTOccured=writeReady;
	writeReady&=state.INTMaskSend;
	townsPtr->pic.SetInterruptRequestBit(TOWNSIRQ_MIDI,0!=writeReady);
}

void TownsMIDI::IOWriteByte(unsigned int ioport,unsigned int data)
{
	switch(ioport)
	{
	case TOWNSIO_MIDI_CARD1_DATREG1: //0x0E50,         // MIDI card(MT-402 or 403) No.1 Out port 1 datReg1 (from Linux source)
	case TOWNSIO_MIDI_CARD1_CMDREG1: //0x0E51,         // MIDI card(MT-402 or 403) No.1 cmdReg1 (Linux source)
	case TOWNSIO_MIDI_CARD1_DATREG2: //0x0E54,         // MIDI card(MT-402 or 403) No.1
	case TOWNSIO_MIDI_CARD1_CMDREG2: //0x0E55,         // MIDI card(MT-402 or 403) No.1
	case TOWNSIO_MIDI_CARD1_FIFODAT: //0x0E52,         // MIDI card(MT-402 or 403) No.1
	case TOWNSIO_MIDI_CARD1_FIFOREG: //0x0E53,         // MIDI card(MT-402 or 403) No.1
		state.cards[0].IOWriteByte(ioport,data,townsPtr->state.townsTime);
		break;

	case TOWNSIO_MIDI_CARD2_DATREG1: //0x0E58,         // MIDI card 2
	case TOWNSIO_MIDI_CARD2_CMDREG1: //0x0E59,         // MIDI card 2
	case TOWNSIO_MIDI_CARD2_DATREG2: //0x0E5C,         // MIDI card 2
	case TOWNSIO_MIDI_CARD2_CMDREG2: //0x0E5D,         // MIDI card 2
	case TOWNSIO_MIDI_CARD2_FIFODAT: //0x0E5A,         // MIDI card 2
	case TOWNSIO_MIDI_CARD2_FIFOREG: //0x0E5B,         // MIDI card 2
		state.cards[1].IOWriteByte(ioport,data,townsPtr->state.townsTime);
		break;

	case TOWNSIO_MIDI_CARD3EF_DATREG1: //0x0E60,       // MIDI card 3 [E][F] according to Linux midi.c
	case TOWNSIO_MIDI_CARD3EF_CMDREG1: //0x0E61,       // MIDI card 3 [E][F]
	case TOWNSIO_MIDI_CARD3EF_DATREG2: //0x0E64,       // MIDI card 3 [E][F] according to Linux midi.c
	case TOWNSIO_MIDI_CARD3EF_CMDREG2: //0x0E65,       // MIDI card 3 [E][F]
	case TOWNSIO_MIDI_CARD3EF_FIFODAT: //0x0E62,       // MIDI card 3 [E][F] according to Linux midi.c
	case TOWNSIO_MIDI_CARD3EF_FIFOREG: //0x0E63,       // MIDI card 3 [E][F]
		state.cards[2].IOWriteByte(ioport,data,townsPtr->state.townsTime);
		break;

	case TOWNSIO_MIDI_CARD3GH_DATREG1: //0x0E68,       // MIDI card 3 [G][H] according to Linux midi.c
	case TOWNSIO_MIDI_CARD3GH_CMDREG1: //0x0E69,       // MIDI card 3 [G][H]
	case TOWNSIO_MIDI_CARD3GH_DATREG2: //0x0E6C,       // MIDI card 3 [G][H] according to Linux midi.c
	case TOWNSIO_MIDI_CARD3GH_CMDREG2: //0x0E6D,       // MIDI card 3 [G][H]
	case TOWNSIO_MIDI_CARD3GH_FIFODAT: //0x0E6A,       // MIDI card 3 [G][H] according to Linux midi.c
	case TOWNSIO_MIDI_CARD3GH_FIFORET: //0x0E6B,       // MIDI card 3 [G][H]
		state.cards[3].IOWriteByte(ioport,data,townsPtr->state.townsTime);
		break;

	case TOWNSIO_MIDI_CARD1GEN2_DATREG1: //0x04A8,     // 2nd Gen MIDI-1 according to Linux midi.c
	case TOWNSIO_MIDI_CARD1GEN2_CMDREG1: //0x04A9,     // 2nd Gen MIDI-1
	case TOWNSIO_MIDI_CARD1GEN2_DATREG2: //0x04AC,     // 2nd Gen MIDI-1 according to Linux midi.c
	case TOWNSIO_MIDI_CARD1GEN2_CMDREG2: //0x04AD,     // 2nd Gen MIDI-1
	case TOWNSIO_MIDI_CARD1GEN2_FIFODAT: //0x04AA,     // 2nd Gen MIDI-1 according to Linux midi.c
	case TOWNSIO_MIDI_CARD1GEN2_FIFOREG: //0x04AB,     // 2nd Gen MIDI-1
		state.cards[4].IOWriteByte(ioport,data,townsPtr->state.townsTime);
		break;

	case TOWNSIO_MIDI_INT_MASK_SEND: //0x0E70,    // MIDI SEND interrupt MASK
		if(true==state.cards[0].enabled ||
		   true==state.cards[1].enabled ||
		   true==state.cards[2].enabled ||
		   true==state.cards[3].enabled)
		{
			state.INTMaskSend=data;
			UpdateInterruptRequest();
		}
		break;
	case TOWNSIO_MIDI_INT_MASK_RECEIVE: //0x0E71, // MIDI RECEIVE interrupt MASK
		if(true==state.cards[0].enabled ||
		   true==state.cards[1].enabled ||
		   true==state.cards[2].enabled ||
		   true==state.cards[3].enabled)
		{
			state.INTMaskReceive=data;
			UpdateInterruptRequest();
		}
		break;

	case TOWNSIO_MIDI_INT_MASK_SEND_FMT401GEN2: //0x4A0, // MIDI INT Mask for FMT-401 Second Gen (according to Linux midi.c)
		if(true==state.cards[4].enabled)
		{
			state.INTMaskSend=data;
			UpdateInterruptRequest();
		}
		break;
	case TOWNSIO_MIDI_INT_MASK_RECEIVE_FMT401GEN2: //0x4A1, // MIDI INT Mask for FMT-401 Second Gen (according to Linux midi.c)
		if(true==state.cards[4].enabled)
		{
			state.INTMaskReceive=data;
			UpdateInterruptRequest();
		}
		break;
	}
}

unsigned int TownsMIDI::IOReadByte(unsigned int ioport)
{
	switch(ioport)
	{
	case TOWNSIO_MIDI_CARD1_DATREG1: //0x0E50,         // MIDI card(MT-402 or 403) No.1 Out port 1 datReg1 (from Linux source)
	case TOWNSIO_MIDI_CARD1_CMDREG1: //0x0E51,         // MIDI card(MT-402 or 403) No.1 cmdReg1 (Linux source)
	case TOWNSIO_MIDI_CARD1_DATREG2: //0x0E54,         // MIDI card(MT-402 or 403) No.1
	case TOWNSIO_MIDI_CARD1_CMDREG2: //0x0E55,         // MIDI card(MT-402 or 403) No.1
	case TOWNSIO_MIDI_CARD1_FIFODAT: //0x0E52,         // MIDI card(MT-402 or 403) No.1
	case TOWNSIO_MIDI_CARD1_FIFOREG: //0x0E53,         // MIDI card(MT-402 or 403) No.1
		return state.cards[0].IOReadByte(ioport,townsPtr->state.townsTime);

	case TOWNSIO_MIDI_CARD2_DATREG1: //0x0E58,         // MIDI card 2
	case TOWNSIO_MIDI_CARD2_CMDREG1: //0x0E59,         // MIDI card 2
	case TOWNSIO_MIDI_CARD2_DATREG2: //0x0E5C,         // MIDI card 2
	case TOWNSIO_MIDI_CARD2_CMDREG2: //0x0E5D,         // MIDI card 2
	case TOWNSIO_MIDI_CARD2_FIFODAT: //0x0E5A,         // MIDI card 2
	case TOWNSIO_MIDI_CARD2_FIFOREG: //0x0E5B,         // MIDI card 2
		return state.cards[1].IOReadByte(ioport,townsPtr->state.townsTime);

	case TOWNSIO_MIDI_CARD3EF_DATREG1: //0x0E60,       // MIDI card 3 [E][F] according to Linux midi.c
	case TOWNSIO_MIDI_CARD3EF_CMDREG1: //0x0E61,       // MIDI card 3 [E][F]
	case TOWNSIO_MIDI_CARD3EF_DATREG2: //0x0E64,       // MIDI card 3 [E][F] according to Linux midi.c
	case TOWNSIO_MIDI_CARD3EF_CMDREG2: //0x0E65,       // MIDI card 3 [E][F]
	case TOWNSIO_MIDI_CARD3EF_FIFODAT: //0x0E62,       // MIDI card 3 [E][F] according to Linux midi.c
	case TOWNSIO_MIDI_CARD3EF_FIFOREG: //0x0E63,       // MIDI card 3 [E][F]
		return state.cards[2].IOReadByte(ioport,townsPtr->state.townsTime);

	case TOWNSIO_MIDI_CARD3GH_DATREG1: //0x0E68,       // MIDI card 3 [G][H] according to Linux midi.c
	case TOWNSIO_MIDI_CARD3GH_CMDREG1: //0x0E69,       // MIDI card 3 [G][H]
	case TOWNSIO_MIDI_CARD3GH_DATREG2: //0x0E6C,       // MIDI card 3 [G][H] according to Linux midi.c
	case TOWNSIO_MIDI_CARD3GH_CMDREG2: //0x0E6D,       // MIDI card 3 [G][H]
	case TOWNSIO_MIDI_CARD3GH_FIFODAT: //0x0E6A,       // MIDI card 3 [G][H] according to Linux midi.c
	case TOWNSIO_MIDI_CARD3GH_FIFORET: //0x0E6B,       // MIDI card 3 [G][H]
		return state.cards[3].IOReadByte(ioport,townsPtr->state.townsTime);

	case TOWNSIO_MIDI_CARD1GEN2_DATREG1: //0x04A8,     // 2nd Gen MIDI-1 according to Linux midi.c
	case TOWNSIO_MIDI_CARD1GEN2_CMDREG1: //0x04A9,     // 2nd Gen MIDI-1
	case TOWNSIO_MIDI_CARD1GEN2_DATREG2: //0x04AC,     // 2nd Gen MIDI-1 according to Linux midi.c
	case TOWNSIO_MIDI_CARD1GEN2_CMDREG2: //0x04AD,     // 2nd Gen MIDI-1
	case TOWNSIO_MIDI_CARD1GEN2_FIFODAT: //0x04AA,     // 2nd Gen MIDI-1 according to Linux midi.c
	case TOWNSIO_MIDI_CARD1GEN2_FIFOREG: //0x04AB,     // 2nd Gen MIDI-1
		return state.cards[4].IOReadByte(ioport,townsPtr->state.townsTime);

	case TOWNSIO_MIDI_INT_MASK_SEND: //0x0E70,    // MIDI SEND interrupt MASK
		if(true==state.cards[0].enabled ||
		   true==state.cards[1].enabled ||
		   true==state.cards[2].enabled ||
		   true==state.cards[3].enabled)
		{
		}
		break;
	case TOWNSIO_MIDI_INT_MASK_RECEIVE: //0x0E71, // MIDI RECEIVE interrupt MASK
		if(true==state.cards[0].enabled ||
		   true==state.cards[1].enabled ||
		   true==state.cards[2].enabled ||
		   true==state.cards[3].enabled)
		{
		}
		break;

	case TOWNSIO_MIDI_INT_MASK_SEND_FMT401GEN2: //0x4A0, // MIDI INT Mask for FMT-401 Second Gen (according to Linux midi.c)
		if(true==state.cards[4].enabled)
		{
		}
		break;
	case TOWNSIO_MIDI_INT_MASK_RECEIVE_FMT401GEN2: //0x4A1, // MIDI INT Mask for FMT-401 Second Gen (according to Linux midi.c)
		if(true==state.cards[4].enabled)
		{
		}
		break;
	}

	return 0xFF;
}


uint32_t TownsMIDI::SerializeVersion(void) const
{
	return 0;
}

void TownsMIDI::SpecificSerialize(std::vector <unsigned char> &data,std::string FName) const
{
}

bool TownsMIDI::SpecificDeserialize(const unsigned char *&data,std::string FName,uint32_t version)
{
	return true;
}