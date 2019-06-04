float mAmpRec = 45;  // actual about 43 mA
float mAmpSleep = 2.8; // actual about 2.6 mA
byte nBatPacks = 1;
float mAhPerBat = 12000.0; // assume 12Ah per battery pack; good batteries should be 14000

uint32_t freeMB;
uint32_t filesPerCard;
csd_t m_csd;


/* DISPLAY FUNCTIONS
 *  
 */

time_t autoStartTime;
 
void printDigits(int digits){
  // utility function for digital clock display: prints preceding colon and leading 0
  display.print(":");
  printZero(digits);
  display.print(digits);
}

void printZero(int val){
  if(val<10) display.print('0');
}

#define noSet 0
#define setRecDur 1
#define setRecSleep 2
#define setYear 3
#define setMonth 4
#define setDay 5
#define setHour 6
#define setMinute 7
#define setSecond 8
#define setMode 9
#define setStartHour 10
#define setStartMinute 11
#define setEndHour 12
#define setEndMinute 13
#define setFsamp 14

void manualSettings(){
  boolean startRec = 0, startUp, startDown;
  Serial.print("Gain:");
  Serial.println(gainSetting);
  Serial.println("Read EEPROM");
  readEEPROM();
  calcGain();
  Serial.print("Gain:");
  Serial.println(gainSetting);

  autoStartTime = getTeensy3Time();

// get free space on cards

    cDisplay();
    display.print("Snap Init");
    display.setTextSize(1);
    display.setCursor(0, 16);
    display.println("Card Free/Total MB");
    
    freeMB = 0; //reset
    Serial.println(); Serial.println();
    Serial.print("Card:"); 
    display.display();
    
    // Initialize the SD card

    SPI.setMOSI(7);
    SPI.setSCK(14);
    SPI.setMISO(12);
  
    if(sd.begin(10, SD_SCK_MHZ(50))){
      sd.card()->readCSD(&m_csd);
      
      //uint32_t volFree = sd.FsVolume.freeClusterCount();
      uint32_t volMB = uint32_t ( 0.000512 * sdCardCapacity(&m_csd));    
      
      Serial.print("Volume (MB): ");
      Serial.println((uint32_t) volMB);

      if (volMB < 200) freeMB = 0;
      else
        freeMB = volMB - 200; // take off 200 MB to be safe
      
      display.print(freeMB);
      display.print("/");
      display.println(volMB);
      display.display();
    }
    else{
      Serial.println("Unable to access the SD card");
      //Serial.println(card.errorCode());
     // Serial.println(card.errorData());
      display.println("  None");
      display.display();
  }
  
  // make sure settings valid (if EEPROM corrupted or not set yet)
  
  if (rec_dur < 0 | rec_dur>100000) {
    rec_dur = 60;
    writeEEPROMlong(0, rec_dur);  //long
  }
  if (rec_int<0 | rec_int>100000) {
    rec_int = 60;
    writeEEPROMlong(4, rec_int);  //long
  }
  if (startHour<0 | startHour>23) {
    startHour = 0;
    EEPROM.write(8, startHour); //byte
  }
  if (startMinute<0 | startMinute>59) {
    startMinute = 0;
    EEPROM.write(9, startMinute); //byte
  }
  if (endHour<0 | endHour>23) {
    endHour = 0;
    EEPROM.write(10, endHour); //byte
  }
  if (endMinute<0 | endMinute>59) {
    endMinute = 0;
    EEPROM.write(11, endMinute); //byte
  }
  if (recMode<0 | recMode>1) {
    recMode = 0;
    EEPROM.write(12, recMode); //byte
  }
  if (isf<0 | isf>4) {
    isf = 3;
    EEPROM.write(13, isf); //byte
  }
  if (gainSetting<0 | gainSetting>15) {
    gainSetting = 4;
    EEPROM.write(14, gainSetting); //byte
  }


//  // if LOG.CSV present, skip manual settings
//  #if USE_SDFS==1
//    FsFile logFile = sd.open("LOG.CSV");
//  #else
//    File logFile = sd.open("LOG.CSV");
//  #endif
//  if(logFile){
//    startRec = 1;
//    logFile.close();
//  }
  
  while(startRec==0){
    static int curSetting = noSet;
    static int newYear, newMonth, newDay, newHour, newMinute, newSecond, oldYear, oldMonth, oldDay, oldHour, oldMinute, oldSecond;
    
    // Check for mode change
    boolean selectVal = digitalRead(SELECT);
    if(selectVal==0){
      curSetting += 1;
      while(digitalRead(SELECT)==0){ // wait until let go of button
        delay(10);
      }
      if(recMode==MODE_NORMAL & (curSetting>8) & (curSetting<14)) curSetting = 14;
      if(recMode==MODE_NORMAL & (curSetting>14)) curSetting = 0;
   }

    cDisplay();

    t = getTeensy3Time();

    if (t - autoStartTime > 600) startRec = 1; //autostart if no activity for 10 minutes
    switch (curSetting){
      case noSet:
        if (settingsChanged) {
          writeEEPROM();
          settingsChanged = 0;
          autoStartTime = getTeensy3Time();  //reset autoStartTime
        }
        display.print("UP+DN->Rec"); 
        // Check for start recording
        startUp = digitalRead(UP);
        startDown = digitalRead(DOWN);
        if(startUp==0 & startDown==0) {
          cDisplay();
          writeEEPROM(); //save settings
          display.print("Starting..");
          display.display();
          delay(1500);
          startRec = 1;  //start recording
        }
        break;
      case setRecDur:
        rec_dur = updateVal(rec_dur, 1, 3600);
        display.print("Rec:");
        display.print(rec_dur);
        display.println("s");
        break;
      case setRecSleep:
        rec_int = updateVal(rec_int, 0, 3600 * 24);
        display.print("Slp:");
        display.print(rec_int);
        display.println("s");
        break;
      case setYear:
        oldYear = year(t);
        newYear = updateVal(oldYear,2000, 2100);
        if(oldYear!=newYear) setTeensyTime(hour(t), minute(t), second(t), day(t), month(t), newYear);
        display.print("Year:");
        display.print(year(getTeensy3Time()));
        break;
      case setMonth:
        oldMonth = month(t);
        newMonth = updateVal(oldMonth, 1, 12);
        if(oldMonth != newMonth) setTeensyTime(hour(t), minute(t), second(t), day(t), newMonth, year(t));
        display.print("Month:");
        display.print(month(getTeensy3Time()));
        break;
      case setDay:
        oldDay = day(t);
        newDay = updateVal(oldDay, 1, 31);
        if(oldDay!=newDay) setTeensyTime(hour(t), minute(t), second(t), newDay, month(t), year(t));
        display.print("Day:");
        display.print(day(getTeensy3Time()));
        break;
      case setHour:
        oldHour = hour(t);
        newHour = updateVal(oldHour, 0, 23);
        if(oldHour!=newHour) setTeensyTime(newHour, minute(t), second(t), day(t), month(t), year(t));
        display.print("Hour:");
        display.print(hour(getTeensy3Time()));
        break;
      case setMinute:
        oldMinute = minute(t);
        newMinute = updateVal(oldMinute, 0, 59);
        if(oldMinute!=newMinute) setTeensyTime(hour(t), newMinute, second(t), day(t), month(t), year(t));
        display.print("Minute:");
        display.print(minute(getTeensy3Time()));
        break;
      case setSecond:
        oldSecond = second(t);
        newSecond = updateVal(oldSecond, 0, 59);
        if(oldSecond!=newSecond) setTeensyTime(hour(t), minute(t), newSecond, day(t), month(t), year(t));
        display.print("Second:");
        display.print(second(getTeensy3Time()));
        break;
      case setFsamp:
        isf = updateVal(isf, 0, 5);
        display.printf("SF: %.1f",lhi_fsamps[isf]/1000.0f);
        break;
    }
    displaySettings();
    displayClock(getTeensy3Time(), BOTTOM);
    display.display();
    delay(10);
  }
}

void setTeensyTime(int hr, int mn, int sc, int dy, int mh, int yr){
  tmElements_t tm;
  tm.Year = yr - 1970;
  tm.Month = mh;
  tm.Day = dy;
  tm.Hour = hr;
  tm.Minute = mn;
  tm.Second = sc;
  time_t newtime;
  newtime = makeTime(tm); 
  Teensy3Clock.set(newtime); 
  autoStartTime = getTeensy3Time();
}
  
int updateVal(long curVal, long minVal, long maxVal){
  boolean upVal = digitalRead(UP);
  boolean downVal = digitalRead(DOWN);
  static int heldDown = 0;
  static int heldUp = 0;

  if(upVal==0){
    settingsChanged = 1;
    if (heldUp < 20) delay(200);
      curVal += 1;
      heldUp += 1;
    }
    else heldUp = 0;
    
    if(downVal==0){
      settingsChanged = 1;
      if(heldDown < 20) delay(200);
      if(curVal < 10) { // going down to 0, go back to slow mode
        heldDown = 0;
      }
        curVal -= 1;
        heldDown += 1;
    }
    else heldDown = 0;

    if (curVal < minVal) curVal = maxVal;
    if (curVal > maxVal) curVal = minVal;
    return curVal;
}

void cDisplay(){
    display.clearDisplay();
    display.setTextColor(WHITE);
    display.setTextSize(2);
    display.setCursor(0,0);
}

void displaySettings(){
  t = getTeensy3Time();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 18);

  display.print("Rec:");
  display.print(rec_dur);
  display.println("s ");
  
  display.print("Sleep:");
  display.print(rec_int);
  display.println("s  ");

  display.printf("%.1f kHz",lhi_fsamps[isf]/1000.0f);

  display.print(" ");
  display.printf("%.1f",gainDb);
  display.print("dB gain");

  display.setTextSize(1);
  uint32_t totalRecSeconds = 0;

  float fileBytes = (2 * rec_dur * lhi_fsamps[isf]) + 44;
  float fileMB = (fileBytes + 32768) / 1000 / 1000; // add cluster size so don't underestimate fileMB
  float dielFraction = 1.0; //diel mode decreases time spent recording, increases time in sleep
  
  float recFraction = ((float) rec_dur * dielFraction) / (float) (rec_dur + rec_int);
  float sleepFraction = 1 - recFraction;
  float avgCurrentDraw = (mAmpRec * recFraction) + (mAmpSleep * sleepFraction);

//  Serial.print("Rec Fraction Sleep Fraction Avg Power:");
//  Serial.print(rec_dur);
//  Serial.print("  ");
//  Serial.print(recFraction);
//  Serial.print("  ");
//  Serial.print(rec_int);
//  Serial.print("  ");
//  Serial.print(sleepFraction);
//  Serial.print("  ");
//  Serial.println(avgCurrentDraw);

  uint32_t powerSeconds = uint32_t (3600.0 * (mAhPerBat / avgCurrentDraw));

  filesPerCard = 0;
  if(freeMB==0) filesPerCard = 0;
  else{
    filesPerCard = (uint32_t) floor(freeMB / fileMB);
  }
  totalRecSeconds += (filesPerCard * rec_dur);
  //display.setCursor(60, 18 + (n*8));  // display file count for debugging
  //display.print(n+1); display.print(":");display.print(filesPerCard[n]); 

  float totalSecondsMemory = totalRecSeconds / recFraction;
  if(powerSeconds < totalSecondsMemory){
   // displayClock(getTeensy3Time() + powerSeconds, 45, 0);
    display.setCursor(0, 46);
    display.print("Battery Limit:");
    display.print(powerSeconds / 86400);
    display.print("d");
  }
  else{
  //  displayClock(getTeensy3Time() + totalRecSeconds + totalSleepSeconds, 45, 0);
    display.setCursor(0, 46);
    display.print("Memory Limit:");
    display.print(totalSecondsMemory / 86400);
    display.print("d");
  }
}


void displayClock(time_t t, int loc){
  display.setTextSize(1);
  display.setCursor(0,loc);
  display.print(year(t));
  display.print('-');
  display.print(month(t));
  display.print('-');
  display.print(day(t));
  display.print("  ");
  printZero(hour(t));
  display.print(hour(t));
  printDigits(minute(t));
  printDigits(second(t));
}

void printTime(time_t t){
  Serial.print(year(t));
  Serial.print('-');
  Serial.print(month(t));
  Serial.print('-');
  Serial.print(day(t));
  Serial.print(" ");
  Serial.print(hour(t));
  Serial.print(':');
  Serial.print(minute(t));
  Serial.print(':');
  Serial.println(second(t));
}

void readEEPROM(){
  rec_dur = readEEPROMlong(0);
  rec_int = readEEPROMlong(4);
  startHour = EEPROM.read(8);
  startMinute = EEPROM.read(9);
  endHour = EEPROM.read(10);
  endMinute = EEPROM.read(11);
  recMode = EEPROM.read(12);
  isf = EEPROM.read(13);
  gainSetting = EEPROM.read(14);
}

union {
  byte b[4];
  long lval;
}u;

long readEEPROMlong(int address){
  u.b[0] = EEPROM.read(address);
  u.b[1] = EEPROM.read(address + 1);
  u.b[2] = EEPROM.read(address + 2);
  u.b[3] = EEPROM.read(address + 3);
  return u.lval;
}

void writeEEPROMlong(int address, long val){
  u.lval = val;
  EEPROM.write(address, u.b[0]);
  EEPROM.write(address + 1, u.b[1]);
  EEPROM.write(address + 2, u.b[2]);
  EEPROM.write(address + 3, u.b[3]);
}

void writeEEPROM(){
  writeEEPROMlong(0, rec_dur);  //long
  writeEEPROMlong(4, rec_int);  //long
  EEPROM.write(8, startHour); //byte
  EEPROM.write(9, startMinute); //byte
  EEPROM.write(10, endHour); //byte
  EEPROM.write(11, endMinute); //byte
  EEPROM.write(12, recMode); //byte
  EEPROM.write(13, isf); //byte
  EEPROM.write(14, gainSetting); //byte
}

