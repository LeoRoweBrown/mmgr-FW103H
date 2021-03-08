///////////////////////////////////////////////////////////////////////////////
// FILE:          ThorlabsFW103H.h
// PROJECT:       Micro-Manager
// SUBSYSTEM:     DeviceAdapters
//-----------------------------------------------------------------------------
// DESCRIPTION:   Controls the FW103H filter wheel
// COPYRIGHT:     Imperial College London 2020
// LICENSE:       GPL
// AUTHOR:        Leo Rowe-Brown
//-----------------------------------------------------------------------------


#include "../../MMDevice/MMDevice.h"
#include "../../MMDevice/DeviceBase.h"
#include "../../MMDevice/ModuleInterface.h"

#include <string>

#define ERR_UNKNOWN_POSITION          100
#define ERR_INVALID_SPEED             101
#define ERR_MOVE_TIMEOUT              102
#define ERR_HOME_TIMEOUT		        103
#define ERR_POLL_CHANGE_FORBIDDEN     104

// CRTP
class ThorlabsFilterWheel : public CStateDeviceBase<ThorlabsFilterWheel>
{
public:
   ThorlabsFilterWheel();
   ~ThorlabsFilterWheel();
  
   // MMDevice API
   // ------------
   int Initialize();
   int Shutdown();
  
   void GetName(char* pszName) const;
   bool Busy();
   unsigned long GetNumberOfPositions()const {return numPos_;}

   // action interface
   // ----------------
   int OnState(MM::PropertyBase* pProp, MM::ActionType eAct);
   int OnSpeed(MM::PropertyBase* pProp, MM::ActionType eAct);
   int OnSerialNumber(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnPollTime(MM::PropertyBase* pProp, MM::ActionType eAct);

   // Kinesis API commands
   int Kinesis_Initialize(int timeout);
   int Kinesis_Home();
   int Kinesis_Shutdown();
   int Kinesis_SetPosition(double position, int timeout);
   double Kinesis_GetSpeed();
   int Kinesis_SetSpeed(int speed);
   int Kinesis_SendCmd();

private:
   // char* serialNumber_ ;
   std::string serialNumber_;
   long numPos_;
   bool initialized_;
   MM::MMTime changedTime_;
   long position_;
   bool homed_;
   long maxSpeed_;
   long speed_;
   double stepAngle_;
	long polltime_;
};