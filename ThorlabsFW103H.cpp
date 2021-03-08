///////////////////////////////////////////////////////////////////////////////
// FILE:          ThorlabsFW103H.cpp
// PROJECT:       Micro-Manager
// SUBSYSTEM:     DeviceAdapters
//-----------------------------------------------------------------------------
// DESCRIPTION:   Controls the FW103H filter wheel
// COPYRIGHT:     Imperial College London 2020
// LICENSE:       GPL
// AUTHOR:        Leo Rowe-Brown
//-----------------------------------------------------------------------------

// #pragma comment(lib, "Thorlabs.MotionControl.Benchtop.StepperMotor.lib")

#ifdef WIN32
#include <windows.h>
#endif
// #include "FixSnprintf.h"

#include "Thorlabs.MotionControl.Benchtop.StepperMotor.h"
#include "ThorlabsFW103H.h"
#include <string>
#include <math.h>
#include "../../MMDevice/ModuleInterface.h"
#include <sstream>
#include <stdlib.h>
#include <time.h>

const char* g_FilterWheelDeviceName = "FW103H Filter Wheel";
const char* g_SerialNumberProp = "Serial Number";
const char* g_PollProp = "Polling time (ms)";

const int g_default_maxSpeed = 8000;
const int g_move_timeout = 5000;  // timeout in ms for moving wheel positions
const double g_real_to_device_units = 7.0/9.0 + 1137;
const double g_real_to_device_speed_units = 61083.979375;
const int g_default_poll = 100; // device poll time in ms

///////////////////////////////////////////////////////////////////////////////
// Exported MMDevice API
///////////////////////////////////////////////////////////////////////////////

// declare functions (prototype)
/*MODULE_API void InitializeModuleData();
MODULE_API MM::Device* CreateDevice(const char* deviceName, char* serialNumber);
MODULE_API void DeleteDevice(MM::Device* pDevice);*/


MODULE_API void InitializeModuleData()
{
   RegisterDevice(g_FilterWheelDeviceName, MM::StateDevice, "FW103H filter wheel");
}

MODULE_API MM::Device* CreateDevice(const char* deviceName)//, char* serialNumber)
{
   if (deviceName == 0)
      return 0;

   // decide which device class to create based on the deviceName parameter
   if (strcmp(deviceName, g_FilterWheelDeviceName) == 0)
   {
      // create filterwheel
      return new ThorlabsFilterWheel();//serialNumber);
   }
   // ...supplied name not recognized
   return 0;
}

MODULE_API void DeleteDevice(MM::Device* pDevice)
{
   delete pDevice;
}

ThorlabsFilterWheel::ThorlabsFilterWheel() ://char* SerialNumber) : 
   serialNumber_("40154488"),
   numPos_(6), 
   initialized_(false), 
   changedTime_(0.0),
   position_(0),
   homed_(false),
	polltime_(g_default_poll),
   maxSpeed_(g_default_maxSpeed)
{
   InitializeDefaultErrorMessages();
   // set device specific error messages

   SetErrorText(ERR_UNKNOWN_POSITION, "Invalid filter wheel position.");
   SetErrorText(ERR_INVALID_SPEED, "Invalid filter wheel speed.");
	SetErrorText(ERR_MOVE_TIMEOUT, "Timed out during move command.");
	SetErrorText(ERR_HOME_TIMEOUT, "Timed out during home command.");
	SetErrorText(ERR_POLL_CHANGE_FORBIDDEN, "Poll time change forbidden");

   // Serial Number
   CPropertyAction* pAct = new CPropertyAction (this, &ThorlabsFilterWheel::OnSerialNumber);
   CreateProperty(g_SerialNumberProp, serialNumber_.c_str(), MM::String, false, pAct, true);

	// Poll time
	pAct = new CPropertyAction (this, &ThorlabsFilterWheel::OnPollTime);
   CreateProperty(g_PollProp, CDeviceUtils::ConvertToString(polltime_), MM::Integer, false, pAct, true);

   EnableDelay(); // signals that the dealy setting will be used
}

ThorlabsFilterWheel::~ThorlabsFilterWheel()
{
   Shutdown();
}

void ThorlabsFilterWheel::GetName(char* Name) const
{
   CDeviceUtils::CopyLimitedString(Name, g_FilterWheelDeviceName);
}


int ThorlabsFilterWheel::Initialize()
{
   if (initialized_)
      return DEVICE_OK;

	// define error text
	SetErrorText(ERR_HOME_TIMEOUT, "Device timed-out: no response received within expected time interval after homing.");
	SetErrorText(ERR_MOVE_TIMEOUT, "Device timed-out: no response received within expected time interval after moving.");
	
	// set property list
	// -----------------

	// Name
	int ret = CreateProperty(MM::g_Keyword_Name, g_FilterWheelDeviceName, MM::String, true);
	if (DEVICE_OK != ret)
		return ret;

	// Description
	ret = CreateProperty(MM::g_Keyword_Description, "Thorlabs FW103H filter wheel driver", MM::String, true);
	if (DEVICE_OK != ret)
		return ret;

	// Set timer for the Busy signal, or we'll get a time-out the first time we check the state of the shutter, for good measure, go back 'delay' time into the past
	changedTime_ = GetCurrentMMTime();   

	// create default positions and labels
	const int bufSize = 1024;
	char buf[bufSize];
	stepAngle_ = 360.0/numPos_;
	for (long i=0; i<numPos_; i++)
	{
		snprintf(buf, bufSize, "Filter-%ld", i + 1);
		SetPositionLabel(i, buf);
	}

	// State
	// -----
	CPropertyAction* pAct = new CPropertyAction (this, &ThorlabsFilterWheel::OnState);
	ret = CreateProperty(MM::g_Keyword_State, "0", MM::Integer, false, pAct);
	if (ret != DEVICE_OK)
		return ret;

	// Speed
	// -----
	pAct = new CPropertyAction (this, &ThorlabsFilterWheel::OnSpeed);
	ret = CreateProperty(MM::g_Keyword_Speed, CDeviceUtils::ConvertToString(maxSpeed_), MM::Integer, false, pAct);
	if (ret != DEVICE_OK)
		return ret;
	SetPropertyLimits(MM::g_Keyword_Speed, 0, maxSpeed_);

	// Label
	// -----
	pAct = new CPropertyAction (this, &ThorlabsFilterWheel::OnLabel);
	ret = CreateProperty(MM::g_Keyword_Label, "", MM::String, false, pAct);
	if (ret != DEVICE_OK)
		return ret;

	// DEBUGGING serial number list //
	/* TLI_BuildDeviceList();
	short n = TLI_GetDeviceListSize();
	char serialNos[100];
	TLI_GetDeviceListByTypeExt(serialNos, 100, 40);
	// serial number print
	//serialNos[99] = '\0';
	char msgout[256];
	sprintf(msgout, "Serial numbers of (%d) devices: %s\n", serialNos, n);
	LogMessage(msgout);
	char msgout2[256];
	sprintf(msgout2, "Attempting to init device with SN %s\n", serialNumber_.c_str());
	LogMessage(msgout2); */
	//////////////////////////////////

	// initialise hardware
	int init_ret = Kinesis_Initialize(g_move_timeout);
	if (init_ret != DEVICE_OK){
		printf("Failed to initialise FW103H device %s\n", serialNumber_.c_str());
		return init_ret;
	}
	// Now get the current speed of the wheel
	double init_speed = Kinesis_GetSpeed();
	LogMessage("Initial speed (and max speed in real units?) is " + std::to_string((long double)init_speed));
	printf("speed: %.2f \n", init_speed);

	// bother setting speed? meh

	ret = UpdateStatus();
	if (ret != DEVICE_OK)
		return ret;

	initialized_ = true;

	return DEVICE_OK;
}

bool ThorlabsFilterWheel::Busy()
{
   MM::MMTime interval = GetCurrentMMTime() - changedTime_;
   MM::MMTime delay(GetDelayMs()*1000.0);
   if (interval < delay)
      return true;
   else
      return false;
}


int ThorlabsFilterWheel::Shutdown()
{
   if (initialized_)
   {
      initialized_ = false;
      // shutdown comms to device
	  Kinesis_Shutdown();
   }
   return DEVICE_OK;
}

///////////////////////////////////////////////////////////////////////////////
// Action handlers
///////////////////////////////////////////////////////////////////////////////

int ThorlabsFilterWheel::OnState(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::BeforeGet)
   {
      printf("Getting position of Wheel device\n");
      // listen for input for pos?
      pProp->Set(position_);
      // nothing to do, let the caller to use cached property
   }
   else if (eAct == MM::AfterSet)
   {
      // Set timer for the Busy signal
      changedTime_ = GetCurrentMMTime();

      long pos;
  
      pProp->Get(pos);
      //char* deviceName;
      //GetName(deviceName);
      printf("Moving to position %ld\n", position_);
      if (pos >= numPos_ || pos < 0)
      {
         pProp->Set(position_); // revert
         return ERR_UNKNOWN_POSITION;
      }
      // do actual moving
		int ret = Kinesis_SetPosition(pos * stepAngle_, g_move_timeout);
      if (ret != 0){
			return ret;
      }
		position_ = pos;
   }

   return DEVICE_OK;
}

int ThorlabsFilterWheel::OnSpeed(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::BeforeGet)
   {
      pProp->Set((long)speed_);
   }
   else if (eAct == MM::AfterSet)
   {
      long speed;
      pProp->Get(speed);
      if (speed < 0 || speed > g_default_maxSpeed)
      {
         pProp->Set((long)speed_); // revert
         return ERR_INVALID_SPEED;
      }
      // do actual speed change here!
      //
		int ret = Kinesis_SetSpeed(speed);
		if (ret != 0){ // error handling for speed change not implemented yet
			LogMessage("Failed to set speed with error code " + std::to_string((long long)ret));
			return ret;
		}
		speed_ = speed;
   }

   return DEVICE_OK;
}

int ThorlabsFilterWheel::OnSerialNumber(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::BeforeGet)
   {
		// property class stores as C-style string
		// set the property from the current attribute serialNumber_
      pProp->Set(serialNumber_.c_str());
   }
	else if (eAct == MM::AfterSet)
	{
		if (initialized_)
		{
		// gets serialNumber from property object
		std::string serialNumber;
		pProp->Get(serialNumber);
		serialNumber_ = serialNumber;
      }

		pProp->Get(serialNumber_);
   }

   return DEVICE_OK;
}

int ThorlabsFilterWheel::OnPollTime(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	if (eAct == MM::BeforeGet)
	{
	// int polltime = SBC_PollingDuration(serialNumber_.c_str(), 1); // <- implement soon?
		pProp->Set(polltime_);
	}
   else if (eAct == MM::AfterSet)
   {
      if (initialized_)
      {
         pProp->Set(polltime_);
         return ERR_POLL_CHANGE_FORBIDDEN;
      }
		// revert
      pProp->Get(polltime_);
   }

   return DEVICE_OK;
}

///////////////////////////////////////////////////////////////////////////////
// Kinesis API commands
///////////////////////////////////////////////////////////////////////////////

int __cdecl ThorlabsFilterWheel::Kinesis_Initialize(int timeout){
// find device with serial number
// Build list of connected device
	bool deviceFound = 0;
	if (TLI_BuildDeviceList() == 0)
	{
	// get device list size 
		short n = TLI_GetDeviceListSize();
		// get BBD serial numbers
		char serialNos[100];
		TLI_GetDeviceListByTypeExt(serialNos, 100, 40);
		// output list of matching devices
		char *searchContext = nullptr;
		// split devices into tokens by comma
		char *p = strtok_s(serialNos, ",", &searchContext);
		int i = 0;
		while (p != nullptr)
		{
			TLI_DeviceInfo deviceInfo;
			// get device info from device
			TLI_GetDeviceInfo(p, &deviceInfo);
			// get strings from device info structure
			char desc[65];
			strncpy_s(desc, deviceInfo.description, 64);
			desc[64] = '\0';
			char serialNo[9];
			strncpy_s(serialNo, deviceInfo.serialNo, 8);
			serialNo[8] = '\0';
			printf("serialNo %s\n", serialNo);
			printf("serialNumber %s\n", serialNumber_.c_str());
         // safer to use strncmp? Think about str lengths
         if(strncmp(serialNo, serialNumber_.c_str(), 8) == 0){
				printf("Found Device %s! : %s\r\n", serialNo, desc);
				deviceFound = 1;
         }
         // next token in list
         p = strtok_s(nullptr, ",", &searchContext);
		}
	}
	// replace these with LogMessage sometime
	printf("bool %d\n", deviceFound);
	if (deviceFound) {
		printf("device bool is true\n");
	}
    if(!(deviceFound)){
		printf("Device not found\n");
		return DEVICE_NOT_CONNECTED;
    }
   // open device
   if(SBC_Open(serialNumber_.c_str()) == 0)
   {
		// start the device polling at [polltime_]ms intervals
		SBC_StartPolling(serialNumber_.c_str(), 1, polltime_);

		// enable device so that it can move
		SBC_EnableChannel(serialNumber_.c_str(), 1);

		Sleep(3000);
		// Home device
		Kinesis_Home();

		// wait for completion
			bool complete = 0;
			int timeoutCounter = 0;
			while(!complete)
	  {
		// wait for a message to arrive
		while(SBC_MessageQueueSize(serialNumber_.c_str(), 1) == 0)
		{
			if (timeoutCounter * 100 > timeout){
				return ERR_HOME_TIMEOUT;
			}
			Sleep(100);
			timeoutCounter++;
			
		}
		WORD messageType;
		WORD messageId;
		DWORD messageData;
		SBC_GetNextMessage(serialNumber_.c_str(), 1, &messageType, &messageId, &messageData);
		complete = (messageType == 2 || messageId == 1);
	  }
   }

   return DEVICE_OK;
}

int __cdecl ThorlabsFilterWheel::Kinesis_Home(){
   // Home device
   SBC_ClearMessageQueue(serialNumber_.c_str(), 1);
   SBC_Home(serialNumber_.c_str(), 1);
   printf("Device %s homing\r\n", serialNumber_.c_str());
   return 0;
}

int __cdecl ThorlabsFilterWheel::Kinesis_SetPosition(double position, int timeout){
   // move to position  (degrees) (channel 1)
   SBC_ClearMessageQueue(serialNumber_.c_str(), 1);

   int move_ret = SBC_MoveToPosition(serialNumber_.c_str(), 1, position*g_real_to_device_units);
   if (move_ret != 0){
	   printf("Device %s failed to move\r\n", serialNumber_.c_str());
	   return move_ret;
   }

   
   printf("Device %s moving\r\n", serialNumber_.c_str());

   // wait for completion
   bool complete = 0;
   int timeoutCounter = 0;
   while(!complete)
   {
   // wait for a message to arrive
   while(SBC_MessageQueueSize(serialNumber_.c_str(), 1) == 0)
   {
		// time counter * 10ms
		if (timeoutCounter * 10 > timeout){
			return ERR_MOVE_TIMEOUT;
		}
		Sleep(10);
		timeoutCounter++;
			
   }
   WORD messageType;
   WORD messageId;
   DWORD messageData;
   SBC_GetNextMessage(serialNumber_.c_str(), 1, &messageType, &messageId, &messageData);
   complete = (messageType == 2 || messageId == 1);
   }
   printf("Time taken to move: %d\r\n", timeoutCounter * 10); 
   // get actual position
	// using request and getposition for robustness
	SBC_RequestPosition(serialNumber_.c_str(), 1);
	Sleep(polltime_ + 10);
   double pos = SBC_GetPosition(serialNumber_.c_str(), 1);
   printf("Device %s moved to %d\r\n", serialNumber_.c_str(), (int)(pos/g_real_to_device_units) );
   return DEVICE_OK;
}


double __cdecl ThorlabsFilterWheel::Kinesis_GetSpeed(){
   int currentVelocity, currentAcceleration;
   int ret;
   ret = SBC_GetVelParams(serialNumber_.c_str(), 1, &currentAcceleration, &currentVelocity);
   if (ret != 0){
	   return ret;
   }
   //return currentVelocity;
   return currentVelocity/g_real_to_device_speed_units;
}

int __cdecl ThorlabsFilterWheel::Kinesis_SetSpeed(int speed){
   // value handling done at higher level (Thorlabs State Device)
   if(speed > 0)
   {
		speed *= g_real_to_device_speed_units;
		int currentVelocity, currentAcceleration;
		int ret, retset;
		ret = SBC_GetVelParams(serialNumber_.c_str(), 1, &currentAcceleration, &currentVelocity);
		retset = SBC_SetVelParams(serialNumber_.c_str(), 1, currentAcceleration, speed);
		if (ret){
			return ret;
		}
      else if (retset != 0){
			return retset;
      }
   }
   return DEVICE_OK;
}

int __cdecl ThorlabsFilterWheel::Kinesis_SendCmd(){
   return ERROR_CALL_NOT_IMPLEMENTED;
}

int ThorlabsFilterWheel::Kinesis_Shutdown(){
	// set back to max speed (default)
   Kinesis_SetSpeed(maxSpeed_);
	// stop polling
   SBC_StopPolling(serialNumber_.c_str(), 1);
   // close device
   SBC_Close(serialNumber_.c_str());
   return DEVICE_OK;
}