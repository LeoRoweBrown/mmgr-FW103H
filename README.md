# mmgr-FW103H
MicroManager device adapter for FW103Hfilter wheel from Thorlabs

Installation from built DLL (see releases):
Move `mmgr_dal_ThorlabsFW103H.DLL` into Micro-manager (root) installation path along with the Kinesis driver DLLs:
`Thorlabs.MotionControl.Benchtop.StepperMotor.dll`
`Thorlabs.MotionControl.DeviceManager.dll`

Building (visual studio 2022):
See https://micro-manager.org/Building_Micro-Manager_Device_Adapters.
1) Copy repository to your projects\micro-manager\mmCoreAndDevices\DeviceAdapters directory
2) In solution explorer, right click solution, add->existing project. Select mmgr-FW103H-main\ThorlabsFW103H.vcxproj.
3) Right click the ThorlabsFW103H project in the solution explorer, right click build to compile the DLL.
