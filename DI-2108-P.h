/*
 *   DI_2108_P class, used for communicating with DATAQ Model DI-2108-P High
 *   Speed DAQ on a linux machine. Tested on ubuntu 14.04
 *
 *   Uses libusb to communicate with device.
 *
 *   Developed by Sam de Jong
 */




#include <iostream>
#include <stdio.h>
#include <string.h>
#include <sstream>
#include <unistd.h>
#include <vector>
#include </usr/include/libusb-1.0/libusb.h>

#ifndef DI_2108_P_h
#define DI_2108_P_h 1


//#include "DI-2108-P.cpp"

using namespace std;

class DI_2108_P{
  
 public:
  
  DI_2108_P();                   //empty constructor
  DI_2108_P(int nChannels);      //constructor with number of channels
  void Initialize();             //initialize the DI-2108-P
  void close();                  //close the interface to the DI-2108-P
  string sendMessage(string message);  //send a message to the DI-2108-P, return the response
  void startScan();              //start scanning
  void stopScan();               //stop scanning
  void reset();                  //reset the interface   
  vector<double> getReadings();  //get the voltage readings.

  //status booleans
  bool isRunning() { return isRun;}      //is the DI-2108-P running?
  bool isRec(){ return isRecording;}     //is it reading data?
  bool isInitialized() {return isInit;}  //is the interface initiaized?

  
  void setNChannels(int nChannels);
  void setRange(int channel, int range);


 private:
  //device address - can get with lsusb on linux
  static const int vID = 0x0683;   //vendor ID
  static const int pID = 0x2109;   //product ID
  
  //a libusb session
  libusb_context *ctx;
  
  //device handle
  libusb_device_handle *dev_handle;
  
  bool isRun;
  bool isRecording;
  bool isInit;

  vector<int> ranges;
  vector<double> multiplier;
  vector<double> divisor;
  int nChan;

};

#endif
