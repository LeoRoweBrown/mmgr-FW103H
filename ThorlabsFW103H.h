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

#define ERR_UNKNOWN_POSITION    100
#define ERR_INVALID_SPEED       101
#define ERR_MOVE_TIMEOUT        102
#define ERR_HOME_TIMEOUT        103

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

private:
   // char* serialNumber_ ;
   std::string(serialNumber_);
   long numPos_;
   bool initialized_;
   MM::MMTime changedTime_;
   long position_;
   bool homed_;
   long maxSpeed_;
   long speed_;
   double stepAngle_;
};

// static class for Kinesis API commands

class Kinesis_API
{
public:
   static int Initialize(const char* serialNumber, int timeout);
   static int Home(const char* serialNumber);
   static int Shutdown(const char* serialNumber);
   static int SetPosition(const char* serialNumber, double position, int timeout);
   static double GetSpeed(const char* serialNumber);
   static int SetSpeed(const char* serialNumber, int speed);
   static int SendCmd(const char* serialNumber);
};