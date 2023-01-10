int RTCAddress = 0x68;

volatile byte ds_second;
volatile byte ds_minute;
volatile byte ds_hour;
volatile byte ds_day;
volatile byte ds_month;
volatile byte ds_year;

#define SECONDS_IN_MINUTE 60
#define SECONDS_IN_HOUR 3600
#define SECONDS_IN_DAY 86400
#define SECONDS_IN_YEAR 31536000
#define SECONDS_IN_LEAP 31622400

void setTime2(int thour,int tminute,int tsecond,int tday,int tmonth,int tyear)
{
  Wire.beginTransmission(RTCAddress);
  Wire.write(0x00);  //starting address is 00
  Wire.write(((tsecond/10)<<4) | ((tsecond%10)&0x0F));  //seconds, 
  Wire.write(((tminute/10)<<4) | ((tminute%10)&0x0F));  //min
  Wire.write((((thour/10)&0x03)<<4) | ((thour)%10&0x0F)); //hour  0x20 will set to 12 hour AM/PM mode
  Wire.write(0x01); // day of week(1-7)
  Wire.write(((tday/10)<<4) | ((tday%10)&0x0F)); // date (1-31)
  Wire.write((tmonth/10)<<4 | ((tmonth%10)&0x0F)); //  month
  Wire.write((tyear/10)<<4 | ((tyear%10)&0x0F)); //  year 2010=0x0A
  int ecode=Wire.endTransmission(); //end transmission
//  Serial.println(ecode);
}

void set_alarm(int thour, int tminute)
{
  Wire.beginTransmission(RTCAddress);
  Wire.write(0x07); //0x0B Alarm1
  Wire.write(0x00); //seconds
  Wire.write(((tminute/10)<<4) | ((tminute%10)&0x0F)); // Minutes
  Wire.write((((thour/10)&0x03)<<4) | ((thour)%10&0x0F)); //hour
  Wire.write(0x80); //alarm when hour minutes and seconds match
  Wire.endTransmission();

  Wire.beginTransmission(RTCAddress);
  Wire.write(0x0E); //Control register
  Wire.write(0x01); //enable Alarm 1 interrupt
  Wire.write(0x00); //clear existing interrupts
  Wire.endTransmission();
}

void reset_alarm()
{
  Wire.beginTransmission(RTCAddress);
  Wire.write(0x0F); //Control register
  Wire.write(0x00); //clear existing interrupts
  Wire.endTransmission();
}
boolean readRTC()
{
  Serial.println("Read DS3231: ");
  int i = 0;
  byte buff[7];
  Wire.beginTransmission(RTCAddress); 
  Wire.write(0x00);        //sends address to read from starting with seconds
  Wire.endTransmission(0); //end transmission
  
  Wire.requestFrom(RTCAddress, 7, 0);    // request 7 bytes from clock (seconds through Year)
  int bytesavail=Wire.available();

  while(Wire.available())   // ((Wire.available())&&(i<6))
  { 
    buff[i] = Wire.read();  // receive one byte
    i++;
  }
   Wire.endTransmission();

  if (i==7)  // All bytes received?
  {
      ds_second=(10*(buff[0]>>4)) + ((buff[0]&0x0F)) ;//lower 4 bits is seconds, upper 3 bits are 10 seconds
      ds_minute=(10*(buff[1]>>4)) + ((buff[1]&0x0F)) ;//lower 4 bits is minutes, upper 3 bits are 10 minutes
      ds_hour=(10*((buff[2]>>4)&0x03))+((buff[2]&0x0F));  //bits 4 and 5 are 10 hours; bit 0-3 are hour
      ds_day=(10*(buff[4]>>4))+((buff[4]&0x0F)); 
      ds_month=(10*(buff[5]>>4))+((buff[5]&0x0F)); 
      ds_year=(10*(buff[6]>>4))+((buff[6]&0x0F)); 
     // t = RTCToUNIXTime(ds_year, ds_month, ds_day, ds_hour, ds_minute, ds_second);  
      tmElements_t tm;
      tm.Second = ds_second;
      tm.Minute = ds_minute;
      tm.Hour = ds_hour;
      tm.Day = ds_day;
      tm.Month = ds_month;
      tm.Year = ds_year + 30U;
      t = makeTime(tm);
      Serial.println(t);
      return 1;
  }
  else {
    Serial.println("Clock read fail");
    return 0;
  }
}

int rtcStatus(){
  byte val[2];
  
  Wire.beginTransmission(RTCAddress); 
  Wire.write(0x0F);        //sends address to read from starting with seconds
  Wire.endTransmission(); //end transmission

  Wire.requestFrom(RTCAddress, 1);
  val[0] = Wire.read();
  //val[1] = Wire.read();
  Wire.endTransmission();

  return val[0];
}
