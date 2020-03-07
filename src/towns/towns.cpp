#include <iostream>

#include "cpputil.h"
#include "towns.h"
#include "townsdef.h"
#include "townsmap.h"
#include "render.h"
#include "outside_world.h"
#include "i486symtable.h"



void FMTowns::State::PowerOn(void)
{
	Reset();
	townsTime=0;
	nextFastDevicePollingTime=FAST_DEVICE_POLLING_INTERVAL;
	freq=FREQUENCY_DEFAULT;
	resetReason=0;
}
void FMTowns::State::Reset(void)
{
	clockBalance=0;

	tbiosVersion=TBIOS_UNKNOWN;
	TBIOS_physicalAddr=0;
	MOS_work_linearAddr=0;
	MOS_work_physicalAddr=0;
	mouseBIOSActive=false;

	serialROMBitCount=0;
	lastSerialROMCommand=0;

	// resetReason should survive Reset.
}


////////////////////////////////////////////////////////////


FMTowns::Variable::Variable()
{
	freeRunTimerShift=0;
	Reset();
}

void FMTowns::Variable::Reset(void)
{
	// freeRunTimerShift should survive Reset.
	nextRenderingTime=0;
	disassemblePointer.SEG=0;
	disassemblePointer.OFFSET=0;

	nVM2HostParam=0;
}


////////////////////////////////////////////////////////////


FMTowns::FMTowns() : 
	physMem(&cpu,&mem),
	keyboard(this,&pic),
	crtc(this),
	pic(this),
	dmac(this),
	cdrom(this,&pic,&dmac),
	fdc(this,&pic,&dmac),
	rtc(this),
	sound(this),
	gameport(this),
	timer(this,&pic)
{
	townsType=TOWNSTYPE_2_MX;

	cpu.mouseBIOSInterceptorPtr=this;

	debugger.ioLabel=FMTownsIOMap();
	debugger.GetSymTable().MakeDOSIntFuncLabel();
	MakeINTInfo(debugger.GetSymTable());

	abort=false;
	allDevices.push_back(&pic);
	allDevices.push_back(&dmac);
	allDevices.push_back(&physMem);
	allDevices.push_back(&crtc);
	allDevices.push_back(&fdc);
	allDevices.push_back(&cdrom);
	allDevices.push_back(&rtc);
	allDevices.push_back(&sound);
	allDevices.push_back(&gameport);
	allDevices.push_back(&timer);
	allDevices.push_back(&keyboard);

	fastDevices.push_back(&sound);
	fastDevices.push_back(&timer);

	physMem.SetMainRAMSize(4*1024*1024);

	physMem.SetVRAMSize(1024*1024);
	physMem.SetCVRAMSize(32768);
	physMem.SetSpriteRAMSize(512*1024);
	physMem.SetWaveRAMSize(64*1024);
	physMem.SetDICRAMSize(32768);

	physMem.SetUpMemoryAccess();

	// Free-run counter since FM TOWNS 2UG [2] pp.801
	// Didn't it exist since the first model FM TOWNS 2?
	// I vaguely rember I used something similar when I wrote my first flight simulator 
	// submitted to Japan National High School Students' Programming Contest.
	// FM TOWNS 2UG didn't exist then.
	// I'm positive that I was using the second-generation FM TOWNS then.
	// I'll check if I can find the source code from my old backups.

	// Do range I/O mapping first, then do single I/O mapping.
	// Range I/O mapping may wipe single I/O mapping.

	// Range I/O mappings >>>
	io.AddDevice(this,TOWNSIO_FREERUN_TIMER_LOW/*0x26*/,TOWNSIO_MACHINE_ID_HIGH/*0x31*/);
	io.AddDevice(&crtc,TOWNSIO_MX_HIRES/*0x470*/,TOWNSIO_MX_IMGOUT_ADDR_D3/*0x477*/);
	io.AddDevice(&keyboard,TOWNSIO_KEYBOARD_DATA/*0x600*/,TOWNSIO_KEYBOARD_IRQ/*0x604*/);
	io.AddDevice(&fdc,TOWNSIO_FDC_STATUS_COMMAND/*0x200*/,TOWNSIO_FDC_DRIVE_SWITCH/*0x20E*/);
	io.AddDevice(&physMem,TOWNSIO_CMOS_BASE,TOWNSIO_CMOS_END-1);


	// Individual I/O mappings >>>

	io.AddDevice(&crtc,TOWNSIO_CRTC_ADDRESS);//             0x440,
	io.AddDevice(&crtc,TOWNSIO_CRTC_DATA_LOW);//            0x442,
	io.AddDevice(&crtc,TOWNSIO_CRTC_DATA_HIGH);//           0x443,
	io.AddDevice(&crtc,TOWNSIO_HSYNC_VSYNC);//              0xFDA0,
	io.AddDevice(&crtc,TOWNSIO_VIDEO_OUT_CTRL_ADDRESS);//   0x448,
	io.AddDevice(&crtc,TOWNSIO_VIDEO_OUT_CTRL_DATA);//      0x44A,
	io.AddDevice(&crtc,TOWNSIO_ANALOGPALETTE_CODE);//=  0xFD90,
	io.AddDevice(&crtc,TOWNSIO_ANALOGPALETTE_BLUE);//=  0xFD92,
	io.AddDevice(&crtc,TOWNSIO_ANALOGPALETTE_RED);//=   0xFD94,
	io.AddDevice(&crtc,TOWNSIO_ANALOGPALETTE_GREEN);//= 0xFD96,


	io.AddDevice(this,TOWNSIO_SERIAL_ROM_CTRL); //        0x32,
	io.AddDevice(this,TOWNSIO_FMR_RESOLUTION); // 0x400
	io.AddDevice(this,TOWNSIO_VM_HOST_IF_CMD_STATUS);
	io.AddDevice(this,TOWNSIO_VM_HOST_IF_DATA);


	io.AddDevice(&pic,TOWNSIO_PIC_PRIMARY_ICW1);//          0x00
	io.AddDevice(&pic,TOWNSIO_PIC_PRIMARY_ICW2_3_4_OCW);//  0x02
	io.AddDevice(&pic,TOWNSIO_PIC_SECONDARY_ICW1);//        0x10
	io.AddDevice(&pic,TOWNSIO_PIC_SECONDARY_ICW2_3_4_OCW);//0x12


	io.AddDevice(&dmac,TOWNSIO_DMAC_INITIALIZE);//          0xA0,
	io.AddDevice(&dmac,TOWNSIO_DMAC_CHANNEL);//             0xA1,
	io.AddDevice(&dmac,TOWNSIO_DMAC_COUNT_LOW);//           0xA2,
	io.AddDevice(&dmac,TOWNSIO_DMAC_COUNT_HIGH);//          0xA3,
	io.AddDevice(&dmac,TOWNSIO_DMAC_ADDRESS_LOWEST);//      0xA4,
	io.AddDevice(&dmac,TOWNSIO_DMAC_ADDRESS_MIDLOW);//      0xA5,
	io.AddDevice(&dmac,TOWNSIO_DMAC_ADDRESS_MIDHIGH);//     0xA6,
	io.AddDevice(&dmac,TOWNSIO_DMAC_ADDRESS_HIGHEST);//     0xA7,
	io.AddDevice(&dmac,TOWNSIO_DMAC_DEVICE_CONTROL_LOW);//  0xA8,
	io.AddDevice(&dmac,TOWNSIO_DMAC_DEVICE_CONTROL_HIGH);// 0xA9,
	io.AddDevice(&dmac,TOWNSIO_DMAC_MODE_CONTROL);//        0xAA,
	io.AddDevice(&dmac,TOWNSIO_DMAC_STATUS);//              0xAB,
	io.AddDevice(&dmac,TOWNSIO_DMAC_TEMPORARY_REG_LOW);//   0xAC,
	io.AddDevice(&dmac,TOWNSIO_DMAC_TEMPORARY_REG_HIGH);//  0xAD,
	io.AddDevice(&dmac,TOWNSIO_DMAC_REQUEST);//             0xAE,
	io.AddDevice(&dmac,TOWNSIO_DMAC_MASK);//                0xAF,


	io.AddDevice(this,TOWNSIO_RESET_REASON);
	io.AddDevice(&physMem,TOWNSIO_FMR_VRAM_OR_MAINRAM);
	io.AddDevice(&physMem,TOWNSIO_SYSROM_DICROM);
	io.AddDevice(&physMem,TOWNSIO_DICROM_BANK);
	io.AddDevice(&physMem,TOWNSIO_MEMSIZE);
	io.AddDevice(&physMem,TOWNSIO_FMR_VRAMMASK);
	io.AddDevice(&physMem,TOWNSIO_FMR_VRAMDISPLAYMODE);
	io.AddDevice(&physMem,TOWNSIO_FMR_VRAMPAGESEL);
	io.AddDevice(&physMem,TOWNSIO_TVRAM_WRITE);


	io.AddDevice(&cdrom,TOWNSIO_CDROM_MASTER_CTRL_STATUS);
	io.AddDevice(&cdrom,TOWNSIO_CDROM_COMMAND_STATUS);
	io.AddDevice(&cdrom,TOWNSIO_CDROM_PARAMETER_DATA);
	io.AddDevice(&cdrom,TOWNSIO_CDROM_TRANSFER_CTRL);
	io.AddDevice(&cdrom,TOWNSIO_CDROM_SUBCODE_STATUS);
	io.AddDevice(&cdrom,TOWNSIO_CDROM_SUBCODE_DATA);


	io.AddDevice(&rtc,TOWNSIO_RTC_DATA);//                 0x70,
	io.AddDevice(&rtc,TOWNSIO_RTC_COMMAND);//              0x80,


	io.AddDevice(&gameport,TOWNSIO_GAMEPORT_A_INPUT);  //0x4D0,
	io.AddDevice(&gameport,TOWNSIO_GAMEPORT_B_INPUT);  //0x4D2,
	io.AddDevice(&gameport,TOWNSIO_GAMEPORT_OUTPUT);   //0x4D6,


	io.AddDevice(&sound,TOWNSIO_SOUND_MUTE);//              0x4D5, // [2] pp.18,
	io.AddDevice(&sound,TOWNSIO_SOUND_STATUS_ADDRESS0);//   0x4D8, // [2] pp.18,
	io.AddDevice(&sound,TOWNSIO_SOUND_DATA0);//             0x4DA, // [2] pp.18,
	io.AddDevice(&sound,TOWNSIO_SOUND_ADDRESS1);//          0x4DC, // [2] pp.18,
	io.AddDevice(&sound,TOWNSIO_SOUND_DATA1);//             0x4DE, // [2] pp.18,
	io.AddDevice(&sound,TOWNSIO_SOUND_INT_REASON);//        0x4E9, // [2] pp.19,
	io.AddDevice(&sound,TOWNSIO_SOUND_PCM_INT_MASK);//      0x4EA, // [2] pp.19,
	io.AddDevice(&sound,TOWNSIO_SOUND_PCM_INT);//           0x4EB, // [2] pp.19,
	io.AddDevice(&sound,TOWNSIO_SOUND_PCM_ENV);//           0x4F0, // [2] pp.19,
	io.AddDevice(&sound,TOWNSIO_SOUND_PCM_PAN);//           0x4F1, // [2] pp.19,
	io.AddDevice(&sound,TOWNSIO_SOUND_PCM_FDL);//           0x4F2, // [2] pp.19,
	io.AddDevice(&sound,TOWNSIO_SOUND_PCM_FDH);//           0x4F3, // [2] pp.19,
	io.AddDevice(&sound,TOWNSIO_SOUND_PCM_LSL);//           0x4F4, // [2] pp.19,
	io.AddDevice(&sound,TOWNSIO_SOUND_PCM_LSH);//           0x4F5, // [2] pp.19,
	io.AddDevice(&sound,TOWNSIO_SOUND_PCM_ST);//            0x4F6, // [2] pp.19,
	io.AddDevice(&sound,TOWNSIO_SOUND_PCM_CTRL);//          0x4F7, // [2] pp.19,
	io.AddDevice(&sound,TOWNSIO_SOUND_PCM_CH_ON_OFF);//     0x4F8, // [2] pp.19,


	io.AddDevice(&timer,TOWNSIO_TIMER0_COUNT);//             0x40,
	io.AddDevice(&timer,TOWNSIO_TIMER1_COUNT);//             0x42,
	io.AddDevice(&timer,TOWNSIO_TIMER2_COUNT);//             0x44,
	io.AddDevice(&timer,TOWNSIO_TIMER_0_1_2_CTRL);//         0x46,
	io.AddDevice(&timer,TOWNSIO_TIMER3_COUNT);//             0x50,
	io.AddDevice(&timer,TOWNSIO_TIMER4_COUNT);//             0x52,
	io.AddDevice(&timer,TOWNSIO_TIMER5_COUNT);//             0x54,
	io.AddDevice(&timer,TOWNSIO_TIMER_3_4_5_CTRL);//         0x56,
	io.AddDevice(&timer,TOWNSIO_TIMER_INT_CTRL_INT_REASON);

	PowerOn();
}

unsigned int FMTowns::MachineID(void) const
{
	const int i80286=0;
	const int i80386=1;
	const int i80486SX=2;
	const int i80486DX=2;
	const int i80386SX=3;

	unsigned int lowByte=0,highByte=0;

	switch(townsType)
	{
	case FMR_50_60:
		lowByte=0xF8|i80386;
		break;
	case FMR_50S:
		lowByte=0xE8|i80386;
		break;
	case FMR_70:
		lowByte=0xF0|i80386;
		break;
	default:
	case TOWNSTYPE_MODEL1_2:  // 1st Gen: model1: model2
	case TOWNSTYPE_1F_2F:     // 2nd Gen: 1F:2F
	case TOWNSTYPE_10F_20F:   // 3rd Gen: 10F:20F
		lowByte=i80386; // [2] pp.775
		break;
	case TOWNSTYPE_UX:
		lowByte=i80386SX; // [2] pp.781
		break;
	case TOWNSTYPE_2_CX:
		lowByte=i80386; // [2] pp.775  Was it 386?
		break;
	case TOWNSTYPE_2_UG:
	case TOWNSTYPE_2_HG:
	case TOWNSTYPE_2_HR:
		lowByte=i80486SX; // [2] pp.781
		break;
	case TOWNSTYPE_2_UR:
	case TOWNSTYPE_2_MA:
	case TOWNSTYPE_2_MX:
	case TOWNSTYPE_2_ME:
	case TOWNSTYPE_2_MF_FRESH:
		lowByte=i80486DX; // [2] pp.826
		break;
	}

	switch(townsType)
	{
	case FMR_50_60:
	case FMR_50S:
	case FMR_70:
		highByte=0xFF;
		break;
	default:
	case TOWNSTYPE_MODEL1_2:  // 1st Gen: model1: model2
		highByte=0x1; // [2] pp.775
		break;
	case TOWNSTYPE_1F_2F: // 1F,2F
		highByte=0x2; // [2] pp.775
		break;
	case TOWNSTYPE_10F_20F:   // 3rd Gen: 10F:20F
		highByte=0x4; // [2] pp.826
		break;
	case TOWNSTYPE_UX:
		highByte=0x3; // [2] pp.826
		break;
	case TOWNSTYPE_2_CX:
		highByte=0x5; // [2] pp.826
		break;
	case TOWNSTYPE_2_UG:
		highByte=0x6; // [2] pp.826
		break;
	case TOWNSTYPE_2_HG:
		highByte=0x8; // [2] pp.826
		break;
	case TOWNSTYPE_2_HR:
		highByte=0x7; // [2] pp.826
		break;
	case TOWNSTYPE_2_UR:
		highByte=0x9; // [2] pp.826
		break;
	case TOWNSTYPE_2_MA:
		highByte=0xB; // [2] pp.826
		break;
	case TOWNSTYPE_2_MX: // MX
		highByte=0xC; // [2] pp.826
		break;
	case TOWNSTYPE_2_ME:
		highByte=0xD; // [2] pp.826
		break;
	case TOWNSTYPE_2_MF_FRESH:
		highByte=0xF; // [2] pp.826
		break;
	}

	return (highByte<<8)|lowByte;
}

bool FMTowns::CheckAbort(void) const
{
	bool ab=false;
	if(true==abort)
	{
		std::cout << "FMTowns:" << abortReason << std::endl;
		ab=true;
	}
	if(true==cpu.abort)
	{
		std::cout << cpu.DeviceName() << ':' << cpu.abortReason << std::endl;
		ab=true;
	}
	if(true==physMem.abort)
	{
		std::cout << physMem.DeviceName() << ':' <<  physMem.abortReason << std::endl;
		ab=true;
	}
	for(auto devPtr : allDevices)
	{
		if(true==devPtr->abort)
		{
			std::cout << devPtr->DeviceName() << ':' <<  devPtr->abortReason << std::endl;
			ab=true;
		}
	}
	return ab;
}

bool FMTowns::LoadROMImages(const char dirName[])
{
	if(true!=physMem.LoadROMImages(dirName))
	{
		abort=true;
		abortReason="Unable to load ROM images.";
		return false;
	}
	return true;
}

void FMTowns::PowerOn(void)
{
	state.PowerOn();
	cpu.PowerOn();
	for(auto devPtr : allDevices)
	{
		devPtr->PowerOn();
	}
}
void FMTowns::Reset(void)
{
	var.Reset();
	state.Reset();
	cpu.Reset();
	for(auto devPtr : allDevices)
	{
		devPtr->Reset();
	}

	var.disassemblePointer.SEG=cpu.state.CS().value;
	var.disassemblePointer.OFFSET=cpu.state.EIP;
}

unsigned int FMTowns::RunOneInstruction(void)
{
	auto clocksPassed=cpu.RunOneInstruction(mem,io);
	state.clockBalance+=clocksPassed*1000;

	// Since last update, clockBalance*1000/freq nano seconds have passed.
	// Eg.  66MHz ->  66 clocks passed means 1 micro second.
	//                clockBalance is 66000.
	//                clockBalance/freq=1000.  1000 nano seconds.
	state.townsTime+=(state.clockBalance/state.freq);
	state.clockBalance%=state.freq;

	var.disassemblePointer.SEG=cpu.state.CS().value;
	var.disassemblePointer.OFFSET=cpu.state.EIP;

	return clocksPassed;
}

/* virtual */ void FMTowns::InterceptMouseBIOS(void)
{
	if(0==cpu.GetAH())
	{
		state.MOS_work_linearAddr=cpu.state.GS().baseLinearAddr+cpu.GetEDI();
		state.MOS_work_physicalAddr=cpu.LinearAddressToPhysicalAddress(state.MOS_work_linearAddr,mem);

		i486DX::SegmentRegister CS;
		cpu.LoadSegmentRegister(CS,0x110,mem);
		state.TBIOS_physicalAddr=cpu.LinearAddressToPhysicalAddress(CS.baseLinearAddr,mem);
		state.tbiosVersion=IdentifyTBIOS(state.TBIOS_physicalAddr);

		state.mouseBIOSActive=true;

		std::cout << "Identified TBIOS as: " << TBIOSIDENTtoString(state.tbiosVersion) << std::endl;
	}
	else if(1==cpu.GetAH())
	{
		state.mouseBIOSActive=false;
	}
}

void FMTowns::RunScheduledTasks(void)
{
	for(auto devPtr : allDevices)
	{
		if(devPtr->commonState.scheduleTime<=state.townsTime)
		{
			// Device may make another schedule in the call back.
			// UnscheduleDeviceCallBack must not wipe a new schedule.
			// Therefore, UnscheduleDeviceCallBack and then RunScheduledTask.
			// Not the other way round.
			UnscheduleDeviceCallBack(*devPtr);
			devPtr->RunScheduledTask(state.townsTime);
		}
	}
}

void FMTowns::ScheduleDeviceCallBack(Device &dev,long long int timer)
{
	dev.commonState.scheduleTime=timer;
}
void FMTowns::UnscheduleDeviceCallBack(Device &dev)
{
	dev.commonState.scheduleTime=TIME_NO_SCHEDULE;
}

void FMTowns::RunFastDevicePolling(void)
{
	if(state.nextFastDevicePollingTime<state.townsTime)
	{
		for(auto devPtr : fastDevices)
		{
			devPtr->RunScheduledTask(state.townsTime);
		}
		state.nextFastDevicePollingTime=state.townsTime+FAST_DEVICE_POLLING_INTERVAL;
	}
}

bool FMTowns::CheckRenderingTimer(TownsRender &render,Outside_World &world)
{
	if(var.nextRenderingTime<=state.townsTime)
	{
		render.BuildImage(crtc,physMem);
		world.Render(render.GetImage());
		var.nextRenderingTime=state.townsTime+TOWNS_RENDERING_FREQUENCY;
		return true;
	}
	return false;
}

void FMTowns::SetUpVRAMAccess(bool breakOnRead,bool breakOnWrite)
{
	physMem.SetUpVRAMAccess(breakOnRead,breakOnWrite);
}

void FMTowns::ForceRender(class TownsRender &render,class Outside_World &world)
{
	render.BuildImage(crtc,physMem);
	world.Render(render.GetImage());
}

////////////////////////////////////////////////////////////


unsigned int FMTowns::FetchByteCS_EIP(int offset) const
{
	return cpu.FetchInstructionByte(offset,mem);
}

i486DX::Instruction FMTowns::FetchInstruction(void) const
{
	return cpu.FetchInstruction(mem);
}
std::vector <std::string> FMTowns::GetStackText(unsigned int numBytes) const
{
	std::vector <std::string> text;
	for(unsigned int offsetHigh=0; offsetHigh<numBytes; offsetHigh+=16)
	{
		auto addressSize=cpu.GetStackAddressingSize();
		std::string line;
		line="SS+"+cpputil::Uitox(offsetHigh)+":";
		for(unsigned int offsetLow=0; offsetLow<16 && offsetHigh+offsetLow<numBytes; ++offsetLow)
		{
			line+=cpputil::Ubtox(cpu.FetchByte(addressSize,cpu.state.SS(),cpu.state.ESP()+offsetHigh+offsetLow,mem));
			line.push_back(' ');
		}
		text.push_back(line);
	}
	return text;
}
void FMTowns::PrintStack(unsigned int numBytes) const
{
	for(auto s : GetStackText(numBytes))
	{
		std::cout << s << std::endl;
	}
}
void FMTowns::PrintDisassembly(void) const
{
	auto inst=FetchInstruction();
	auto disasm=cpu.Disassemble(inst,cpu.state.CS(),cpu.state.EIP,mem,debugger.GetSymTable());
	std::cout << disasm << std::endl;
}

std::vector <std::string> FMTowns::GetRealModeIntVectorsText(void) const
{
	std::vector <std::string> text;
	for(int i=0; i<256; i+=4)
	{
		std::string str;
		for(int j=0; j<4; ++j)
		{
			if(0<j)
			{
				str+=" | ";
			}
			auto ij=i+j;
			str+=cpputil::Ubtox(ij)+" ";
			unsigned int offset=physMem.state.RAM[ij*4]|((unsigned int)physMem.state.RAM[ij*4+1]<<8);
			unsigned int seg=physMem.state.RAM[ij*4+2]|((unsigned int)physMem.state.RAM[ij*4+3]<<8);
			str+=cpputil::Ustox(seg)+":"+cpputil::Ustox(offset);
		}
		text.push_back(str);
	}
	return text;
}
void FMTowns::DumpRealModeIntVectors(void) const
{
	for(auto s : GetRealModeIntVectorsText())
	{
		std::cout << s << std::endl;
	}
}
std::vector <std::string> FMTowns::GetCallStackText(void) const
{
	return debugger.GetCallStackText(cpu);
}
void FMTowns::PrintCallStack(void) const
{
	for(auto str : GetCallStackText())
	{
		std::cout << str << std::endl;
	}
}
void FMTowns::PrintPIC(void) const
{
	for(auto str : pic.GetStateText())
	{
		std::cout << str << std::endl;
	}
}

void FMTowns::PrintDMAC(void) const
{
	for(auto str : dmac.GetStateText())
	{
		std::cout << str << std::endl;
	}
}

void FMTowns::PrintFDC(void) const
{
	for(auto str : fdc.GetStatusText())
	{
		std::cout << str << std::endl;
	}
}

void FMTowns::PrintTimer(void) const
{
	for(auto str : timer.GetStatusText())
	{
		std::cout << str << std::endl;
	}
}

void FMTowns::PrintSound(void) const
{
	for(auto str : sound.state.ym2612.GetStatusText())
	{
		std::cout << str << std::endl;
	}
}

void FMTowns::PrintStatus(void) const
{
	std::cout << "Towns TIME (Nano-Seconds): " << state.townsTime << std::endl;
	cpu.PrintState();
	PrintStack(32);
	if(""!=debugger.externalBreakReason)
	{
		std::cout << debugger.externalBreakReason << std::endl;
	}
	PrintDisassembly();
}

/* static */ void FMTowns::MakeINTInfo(class i486SymbolTable &symTable)
{
	symTable.AddINTLabel(0x93,"Disk");
	symTable.AddINTFuncLabel(0x93,0x00,"Set Mode");
	symTable.AddINTFuncLabel(0x93,0x01,"Get Mode");
	symTable.AddINTFuncLabel(0x93,0x02,"Get Status");
	symTable.AddINTFuncLabel(0x93,0x03,"Restore");
	symTable.AddINTFuncLabel(0x93,0x04,"Seek HSG");
	symTable.AddINTFuncLabel(0x93,0x05,"Read HSG");
	symTable.AddINTFuncLabel(0x93,0x14,"Seek MSF");
	symTable.AddINTFuncLabel(0x93,0x15,"Read MSF");
	symTable.AddINTFuncLabel(0x93,0x50,"CDDA Play");
	symTable.AddINTFuncLabel(0x93,0x51,"Get Play Info");
	symTable.AddINTFuncLabel(0x93,0x52,"CDDA Stop");
	symTable.AddINTFuncLabel(0x93,0x53,"Get Play State");
	symTable.AddINTFuncLabel(0x93,0x54,"Read TOC");
	symTable.AddINTFuncLabel(0x93,0x55,"CDDA Pause");
	symTable.AddINTFuncLabel(0x93,0x56,"CDDA Resume");

	symTable.AddINTLabel(0x90,"Keyboard");
	symTable.AddINTFuncLabel(0x90,0x00,"Init");
	symTable.AddINTFuncLabel(0x90,0x01,"Config Buffer");
	symTable.AddINTFuncLabel(0x90,0x02,"Set Code System");
	symTable.AddINTFuncLabel(0x90,0x03,"Get Code System");
	symTable.AddINTFuncLabel(0x90,0x04,"Set Lock");
	symTable.AddINTFuncLabel(0x90,0x05,"Set Click Sound");
	symTable.AddINTFuncLabel(0x90,0x06,"Clear Buffer");
	symTable.AddINTFuncLabel(0x90,0x07,"Check Input");
	symTable.AddINTFuncLabel(0x90,0x08,"Get Shift-Key State");
	symTable.AddINTFuncLabel(0x90,0x09,"Inkey");
	symTable.AddINTFuncLabel(0x90,0x0A,"Input Matrix");
	symTable.AddINTFuncLabel(0x90,0x0B,"Push Key Code");
	symTable.AddINTFuncLabel(0x90,0x0C,"Set PF Key INT");
	symTable.AddINTFuncLabel(0x90,0x0D,"Get PF Key INT");
	symTable.AddINTFuncLabel(0x90,0x0E,"Assign Key");
	symTable.AddINTFuncLabel(0x90,0x0F,"Get Key Assignment");

	symTable.AddINTLabel(0x94,"Printer");
	symTable.AddINTLabel(0x96,"Calendar");
	symTable.AddINTLabel(0x97,"Timer");
	symTable.AddINTLabel(0x98,"Clock");
	symTable.AddINTLabel(0x9B,"RS232C");
	symTable.AddINTLabel(0x9E,"Beep");

	symTable.AddINTLabel(0xAE,"INT-Manager");
	symTable.AddINTFuncLabel(0xAE,0x00,"Set INT Data Block Addr");
	symTable.AddINTFuncLabel(0xAE,0x01,"Get INT Data Block Addr");
	symTable.AddINTFuncLabel(0xAE,0x02,"Set INT Mask");
	symTable.AddINTFuncLabel(0xAE,0x03,"Get INT Mask");
	symTable.AddINTFuncLabel(0xAE,0x04,"Get INT Data Block Table");

	symTable.AddINTLabel(0xAF,"SysService");
	symTable.AddINTFuncLabel(0xAF,0x00,"JIS to SJIS");
	symTable.AddINTFuncLabel(0xAF,0x01,"SJIS to JIS");
	symTable.AddINTFuncLabel(0xAF,0x02,"Get CPU Type");
	symTable.AddINTFuncLabel(0xAF,0x03,"JIS to SJIS 2");
	symTable.AddINTFuncLabel(0xAF,0x04,"SJIS to JIS 2");
	symTable.AddINTFuncLabel(0xAF,0x05,"Get Machine Info");

	symTable.AddINTLabel(0x8E,"Exp-SysService");
	symTable.AddINTFuncLabel(0x8E,0x00,"Get System Info");
	symTable.AddINTFuncLabel(0x8E,0x01,"Printer Feeder Control");
	symTable.AddINTFuncLabel(0x8E,0x02,"? Used in OAK1.SYS Init");
	symTable.AddINTFuncLabel(0x8E,0x20,"(Prob)Get Available Protected-Mode Mem in KB");
	symTable.AddINTFuncLabel(0x8E,0x21,"(Prob)Alloc Protected-Mode Mem in KB");
}
