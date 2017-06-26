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

#include "DI-2108-P.h"

using namespace std;

//default constructor
DI_2108_P::DI_2108_P(){
  nChan=1;
  ranges.resize(nChan);     //resize the range vector to be the same size as the number of channels
  multiplier.resize(nChan);
  divisor.resize(nChan);

  //set status booleans to false
  isRun=false;
  isRecording=false;
  isInit=false;
}

//constructor with number of channels
DI_2108_P::DI_2108_P(int nChannels){
  nChan=nChannels;
  ranges.resize(nChan);     //resize the range vector to be the same size as the number of channels
  multiplier.resize(nChan);
  divisor.resize(nChan);

  //set status booleans to false
  isRun=false;
  isRecording=false;
  isInit=false;
}


void DI_2108_P::Initialize(){
  libusb_device **devs; //pointer to pointer of device, used to retrieve a list of devices

  ctx = NULL; //a libusb session

  int r; //for return values
  ssize_t cnt; //holding number of devices in list

  r = libusb_init(&ctx); //initialize the library for the session we just declared
  if(r < 0) {
    cout<<"Init Error "<<r<<endl; //there was an error
     return;
  }
  libusb_set_debug(ctx, 3); //set verbosity level to 3, as suggested in the documentation
  
  cnt = libusb_get_device_list(ctx, &devs); //get the list of devices
  if(cnt < 0) {
    cout<<"Get Device Error"<<endl; //there was an error
    return;
  }
  cout<<cnt<<" Devices in list."<<endl;
  
  dev_handle = libusb_open_device_with_vid_pid(ctx, vID, pID);
  if(dev_handle == NULL){
    cout<<"Cannot open device"<<endl;
     return;
  }
  else
    cout<<"Device Opened"<<endl;

  libusb_free_device_list(devs, 1); //free the list, unref the devices in it
  
  if(libusb_kernel_driver_active(dev_handle, 0) == 1) { //find out if kernel driver is attached
    cout<<"Kernel Driver Active"<<endl;
    if(libusb_detach_kernel_driver(dev_handle, 0) == 0) //detach it
      cout<<"Kernel Driver Detached!"<<endl;
  }
  r = libusb_claim_interface(dev_handle, 0); //claim interface 0
  if(r < 0) {
    cout<<"Cannot Claim Interface"<<endl;
    return;
  }
  cout<<"Claimed Interface"<<endl;
  

 this->sendMessage("info 0");

 isInit=true; //initialization successful

}

void DI_2108_P::reset(){
  if(!isInit) return;  //don't try to reset if the device isn't initialized
  libusb_reset_device(dev_handle);
  isRun=false;	
  isInit=false;
}


void DI_2108_P::close(){
  if(!isInit) return; //don't close if the device isn't initialized
  int r;
  
  r = libusb_release_interface(dev_handle, 0); //release the claimed interface
  if(r!=0) {
    cout<<"Cannot Release Interface"<<endl;
    return;
  }
  cout<<"Released Interface"<<endl;
  
  libusb_close(dev_handle); //close the device we opened
  libusb_exit(ctx); //needs to be called to end the session
  isInit=false;

}


string DI_2108_P::sendMessage(string message){
  if(!isInit){
    //can't send a message if the device isn't initialized
    return "error\n";
  }
  
  int r; //for return values

  string error = "error";

  message = message+"\r";
  

  unsigned char recieved[1000000];
  unsigned char sent[50];
  strcpy((char*) sent, message.c_str());  //convert the message to a char*
  
  int actual; //used to find out how many bytes were written

  //send the message
  r = libusb_bulk_transfer(dev_handle, (1 | LIBUSB_ENDPOINT_OUT), sent, sizeof(sent), &actual, 0); 

  if(r == 0 && actual == sizeof(sent)){
    sleep(0.1);
    //recieve the message
    r = libusb_bulk_transfer(dev_handle, (1 | LIBUSB_ENDPOINT_IN), recieved, sizeof(recieved), &actual, 0);
    
  }else {
    cout<<"Write Error"<<endl;
    return libusb_strerror((libusb_error)r);  //if there was an error, return the error
  } 
  std::string sName(reinterpret_cast<char*>(recieved));  //convert recieved message into a string
  return sName;


}

//set the number of channels
void DI_2108_P::setNChannels(int nChannels){
  nChan = nChannels;
  ranges.resize(nChan);   //resize the range vector to be the same size as the number of channels
  multiplier.resize(nChan);
  divisor.resize(nChan);
}

//set the range of a channel
void DI_2108_P::setRange(int channel, int range){
  if(channel>nChan) return;  //if the the channel is out of bounds, do nothing
  if(range>4||range<0) return;        //valid range values are 0-4.
  ranges[channel] = range;

  //determine the values to scale the ADC value by (dependant on range)
  if(range==0){                     //+/-10V range
    multiplier[channel]=10;
    divisor[channel] = 1./32768;
  }else if(range==1){               //+/-5V range
    multiplier[channel]=5;
    divisor[channel] = 1./32768;
  }else if(range==2){               //+/-2.5V range
    multiplier[channel]=2.5;
    divisor[channel] = 1./32768;
  }else if(range==3){               //0-10V range
    multiplier[channel]=10;
    divisor[channel] = 1./65536;
  }else if(range==4){               //0-5V range
    multiplier[channel]=5;
    divisor[channel] = 1./65536;
  }
}

//start scanning
void DI_2108_P::startScan(){
  if(!isInit) return; //don't start if the device isn't initialized

  this->sendMessage("ps 0");  //request 16 byte package size

  /*
   *  To activate a channel, need to sent this message to the device:
   *  
   *    slist num1 num2
   *
   *  num1 is the 'offset', num2 is a 16bit number which includes the channel and range setting:
   *
   *          range        chan
   *    0000  0000   0000  0000
   *
   *  so for channel 2 at 0-10V scale, send
   *
   *    0000 0011 0000 0010 = 770
   *
   */


  stringstream ss;
  for(int i=0; i<nChan; i++){
    ss<<i;
    string slistString = "slist "+ss.str()+" ";  
    ss.str("");

    int b=ranges[i]<<8;  //bit shift the range setting by 8 (see above)
    
    int channelMask = i+b;  //add the bit shifted range to the channel number
    ss<<channelMask;
    slistString = slistString+ss.str();
    ss.str("");
    
    this->sendMessage(slistString);  //send the slist command

        
  }

  sleep(0.5);

  this->sendMessage("start 0");  //start scanning
  isRun=true;   //set run boolean to true

}

//stop scanning
void DI_2108_P::stopScan(){
  if(!isInit) return;   //don't do anything if it's not initialized
  isRun=false;          //set run boolean to false
  this->sendMessage("stop 0");

}


//get the voltage measurements from the device
vector<double> DI_2108_P::getReadings(){
  
  isRecording=true;  //set recording boolean to true for the duration of this method

  vector<double> readings;  //create a vector containing the reading
  readings.resize(8);
  
  if(!isInit){   //if device is not initialized, send a vector with only 0 entries
     isRecording=false;
    return readings;
  }

  int r; //for return values

  unsigned char recieved[10000000];
    
  int actual; //used to find out how many bytes were written
  
  //get data from the device
  r = libusb_bulk_transfer(dev_handle, (1 | LIBUSB_ENDPOINT_IN), recieved, 10000000, &actual, 0);

  /*
   *  The device returns a 16 bit value for each channel. libusb converts this whole message
   *  into a char* array. Each char is 8 bits, so the measurement in each channel is composed
   *  of two chars: the one at 2n and the one at 2n+1.
   */

  for(int i=0; i<nChan; i++){
    short combined = (recieved[2*i+1] << 8 ) | (recieved[2*i] & 0xff);  //bit shift the char at 2n+1 by 8 and combine with the one at i.

    double add = 0;  //some range settings require addition of half the multiplier.
    if(ranges[i]>2) add=multiplier[i]/2;
    
    readings[i] = multiplier[i]*(double)combined*divisor[i]+add;  //convert the ADC counts into voltage and put it into the vector
    
  }

  if(r!=0) cout<<libusb_strerror((libusb_error)r)<<endl; //if there was a problem, print an error message
  
  isRecording=false;  //recording is finished

  return readings;  //return the vector containing the measurements

  
}
 
