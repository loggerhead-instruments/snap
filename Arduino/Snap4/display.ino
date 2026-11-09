float mAmpRec = 50;  
float mAmpSleep = 2.0; // measured 1.38 mA at 4 V with deep sleep (v4.40); rounded up to be conservative
float mAmpSleepCardOff = 1.0; // SD Sleep set to Power off: measured 0.7 mA at 4 V (v4.50); rounded up
byte nBatPacks = 1;
float mAhPerBat = 24000.0; // Snap LiPo pack; 12000.0 for the D-cell build (hex/Snap4-D-cell.ino.hex)

float mAmpMono[9] = { 35, 37, 38, 41, 41, 48, 64, 69, 79};
float mAmpStereo[9] = { 36, 38, 43, 47, 53, 65, 91, 100, 107};
// stereo record power consumption 1 TB SanDisk exFAT
// 8 kHz = 36 mA
// 16 kHz = 38 mA
// 32 kHz = 43 mA
// 44.1 kHz = 47 mA
// 48 kHz = 48 mA
// 96 kHz = 62 mA
// 200 kHz = 90 mA
// 250 kHz = 100 mA
// 300 kHz = 107 mA

// mono
// 8 kHz = 35 mA
// 16 kHz = 37 mA
// 32 kHz = 38 mA
// 44.1 kHz = 41 mA
// 48 kHz = 41 mA
// 96 kHz = 48 mA
// 200 kHz = 64 mA
// 250 kHz = 69 mA
// 300 kHz = 79 mA

// stereo record power consumption 256 GB Samsung exFAT; 14 mA sleep 
// 8 kHz = 46 mA
// 16 kHz = 46 mA
// 32 kHz = 48 mA
// 44.1 kHz = 49 mA
// 48 kHz = 49 mA
// 96 kHz = 53 mA
// 200 kHz = 60 mA
// 250 kHz = 64 mA
// 300 kHz = 95 mA

csd_t m_csd;
uint32_t cardTotalMB = 0;
uint32_t clusterBytes = 32768;  // card's cluster size, set when the card is read (512 KB on a 1 TB exFAT card)
uint32_t powerSeconds;       // deployment length the batteries allow
float totalSecondsMemory;    // deployment length the card allows

int curMenuItem = 0;
volatile int maxMenuItem = 10;
char *menuItem[] = {"Start",
                     "Record",
                     "Sleep",
                     "Rate",
                     "Gain",
                     "Time",
                     "Battery",
                     "Delay",
                     "Cutoff",
                     "SD Sleep"
                     };

char *helpText[] = {"ENTER:Start RecordingUP/DN:scroll menu",
                    "ENTER:Set Record Dur\nUP/DN:scroll menu",
                    "ENTER:Set Sleep Dur\nUP/DN:scroll menu",
                    "ENTER:Set Sample RateUP/DN:scroll menu",
                    "ENTER:Set Gain (dB)\nUP/DN:scroll menu",
                    "ENTER:Set Date/Time\nUP/DN:scroll menu",
                    "ENTER:Set #Bat Packs\nUP/DN:scroll menu",
                    "ENTER:Set Delay Days\nUP/DN:scroll menu",
                    "ENTER:Set Low V Stop\nUP/DN:scroll menu",
                    "ENTER:SD power down\nUP/DN:scroll menu"
                    };

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

#define setStart 0
#define setRecDur 1
#define setRecSleep 2
#define setFsamp 3
#define setGain 4
#define setDateTime 5
#define setBatPacks 6
#define setDelay 7
#define setCutoff 8
#define setSdSleep 9
// Snap: no Channel (one channel connected), Mode or Diel items; diel mode can still be set
// from the setup page or setup.txt

void manualSettings(){
  boolean startRec = 0, startUp, startDown;
  readEEPROM();
  calcGain();

  autoStartTime = getTeensy3Time(0);

// get free space on cards
    cDisplay();
    display.print("Snap 4");
    display.setTextSize(1);
    display.setCursor(0, 16);
    display.println("Card Free/Total MB");

    // After 5 card failures in a row during a deployment, rest for an hour with the card,
    // codec and hydrophone off before trying again, for as long as the batteries last
    if(resumeDeployment & (cardFailCount() >= 5)){
      Serial.printf("Card failed %d times in a row: retry in 1 hour\n", cardFailCount());
      display.println("Card failed");
      display.println("Retry in 1 hour");
      display.display();
      delay(5000);
      display.ssd1306_command(SSD1306_DISPLAYOFF);
      digitalWrite(hydroPowPin, LOW);
      audioSleep();
      i2sPinsLow();
      cardOff();
      sleepUntil(getTeensy3Time(1) + 3600, 1);
      digitalWrite(hydroPowPin, HIGH);
      AudioInit(isf);  // restores the I2S pins; setup() sets up the audio again after the menu
      display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
      cDisplay();
      display.print("Snap 4");
      display.setTextSize(1);
      display.setCursor(0, 16);
      display.println("Card Free/Total MB");
    }

    // power cards on
    for (int n=0; n<1; n++){
      digitalWrite(sdPowSelect[n], HIGH);
    }
    cardIsOn = true;
    delay(1000);
    
    // Initialize the SD card
    SPI.setMOSI(7);
    SPI.setSCK(14);
    SPI.setMISO(12);
      
    for (int n=0; n<1; n++){
      freeMB[n] = 0; //reset
      
      Serial.println(); Serial.println();
      Serial.print("Card:"); Serial.println(n + 1);
      display.print(n + 1); display.print("    ");
      display.display();
      delay(100);

      // Start the card and count its free space. A failure right after a reset
      // is often temporary, so retry with a power cycle before giving up.
      int32_t volFree = 0;
      boolean cardBegun = 0;
      for(int attempt = 0; attempt < 5; attempt++){
        watchdogRefresh();
        if(attempt > 0){
          Serial.print("Card retry ");
          Serial.println(attempt);
          sdPowerCycle(n);
        }
        cardBegun = sd.begin(chipSelect[n]);
        if(cardBegun){
          volFree = sd.vol()->freeClusterCount();
          if(volFree == 0){  // 0 is also returned if any read fails while counting
            delay(3000);
            volFree = sd.vol()->freeClusterCount();
          }
          if(volFree > 0) break;
        }
      }

      // A free count of 0 (exFAT) or -1 (FAT32) means a read failed: a card this logger
      // filled still has the 200 MB margin free. Restart and try again, as for a card that
      // does not start, rather than stopping as if the card were full.
      if(cardBegun & (volFree <= 0)){
        Serial.println("Unable to count free space");
        display.print("Card Read Fail");
        display.display();
        delay(5000);
        resetFunc();
      }

      if(cardBegun){
        Serial.print("volFree:");
        Serial.println(volFree);

        float fs = 0.000512 * volFree * sd.vol()->sectorsPerCluster();
        clusterBytes = sd.vol()->sectorsPerCluster() * 512UL;

        uint32_t freeSpace = (uint32_t) fs;
        uint32_t volumeMB = uint32_t ( 0.000512 * (float) sd.card()->sectorCount());

        Serial.print("Volume MB ");
        Serial.println(volumeMB);
        cardTotalMB = volumeMB;
        
        if (freeSpace < 200) freeMB[n] = 0;
        else
          freeMB[n] = freeSpace - 200; // take off 200 MB to be safe

        Serial.print("Free space (MB): ");
        Serial.println((uint32_t) freeMB[n]);
        
        display.print(freeMB[n]);
        display.print("/");
        display.println(volumeMB);
        display.display();
        delay(1000);
      }
      else{
        Serial.println("Unable to access the SD card");
        display.print("Card Fail");
        display.display();
        delay(5000);
        resetFunc();  // power-cycle the card and restart rather than halting
    }
    // digitalWrite(sdPowSelect[n], LOW);
  }

  LoadScript(); // secret settings accessible from card 1
  calcGain();
  writeEEPROM(); // update EEPROM in case any settings changed from card

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
  if (isf<0 | isf>=9) {
    isf = 4;
    EEPROM.write(13, isf); //byte
  }
  if (nBatPacks<0 | nBatPacks>12){
    nBatPacks = 1;
    EEPROM.write(14, nBatPacks); //byte
  }
  if (gainSetting<0 | gainSetting>15) {
    gainSetting = 4;
    EEPROM.write(15, gainSetting); //byte
  }
  if (NCHAN<1 | NCHAN>2) {
    NCHAN = 1;
    EEPROM.write(16, NCHAN); //byte
  }
  if (delayDays>365) {
    delayDays = 0;
    writeEEPROMlong(18, delayDays);
  }
  if (voltCutoff!=0 & (voltCutoff<30 | voltCutoff>34)) {
    voltCutoff = 33;  // default 3.3 V
    EEPROM.write(24, voltCutoff); //byte
  }
  if (sdSleep > 1) {
    sdSleep = 1;  // default: power the card off
    EEPROM.write(EE_SD_SLEEP, sdSleep);
  }
  if (EEPROM.read(EE_SD_RESTARTS) == 255) EEPROM.write(EE_SD_RESTARTS, 0);     // never written
  if (EEPROM.read(EE_LOW_BATT_STOP) == 255) EEPROM.write(EE_LOW_BATT_STOP, 0);
  if (EEPROM.read(EE_CARD_FULL_STOP) == 255) EEPROM.write(EE_CARD_FULL_STOP, 0);
  if (EEPROM.read(EE_WDOG_RESETS) == 255) EEPROM.write(EE_WDOG_RESETS, 0);
  if (EEPROM.read(EE_CLOCK_SWITCH) == 255) EEPROM.write(EE_CLOCK_SWITCH, 0);
  if (EEPROM.read(EE_AUDIO_RESTARTS) == 255) EEPROM.write(EE_AUDIO_RESTARTS, 0);
  if (EEPROM.read(EE_CLOCK_GAPS) == 255) EEPROM.write(EE_CLOCK_GAPS, 0);

  // After a restart during a deployment, skip the menu and go back to recording
  if(resumeDeployment){
    cDisplay();
    display.println("Resuming");
    display.setTextSize(1);
    display.print("after restart");
    display.display();
    delay(2000);
    startRec = 1;
  }

  // Main Menu Loop
   while(startRec==0){
    static int newYear, newMonth, newDay, newHour, newMinute, newSecond, oldYear, oldMonth, oldDay, oldHour, oldMinute, oldSecond;
    watchdogRefresh();
    checkSerialCommands();  // settings and time from USB (tools/logger-setup.html)
    t = getTeensy3Time(0);
    if (t - autoStartTime > 600) startRec = 1; //autostart if no activity for 10 minutes
    
    // Check for button press
    boolean selectVal = digitalRead(UP);
    if(selectVal==0){
      while(digitalRead(UP)==0){
        delay(10); // wait until let go
      }
      curMenuItem++;
      if(curMenuItem>=maxMenuItem) curMenuItem = 0;
    }
    
    selectVal = digitalRead(DOWN);
    if(selectVal==0){
      while(digitalRead(DOWN)==0){
        delay(10); // wait until let go
      }
      curMenuItem--;
      if(curMenuItem<0) curMenuItem = maxMenuItem - 1;
    }


    // Enter pressed from main menu
    selectVal = digitalRead(SELECT);
    if(selectVal==0){
      while(digitalRead(SELECT)==0){ // wait until let go of button
        delay(10);
      }

      // Process enter
      switch (curMenuItem){
        case setStart:
            cDisplay();
            writeEEPROM(); //save settings
            Serial.print("Current Card "); Serial.println(currentCard);
            Serial.print("File MB "); Serial.println(fileMB);
            Serial.print("Free MB "); Serial.println(freeMB[currentCard]);
            Serial.print("Current Card Files "); Serial.println(filesPerCard[currentCard]);
            display.println("Starting..");
            display.setTextSize(1);
            display.print("Press UP+DN to Stop");
            display.display();
            getTeensy3Time(1); // sync Teensy clock to DS3231
            delay(2000);
            startRec = 1;  //start recording 
            break;
        case setRecDur:
            while(digitalRead(SELECT)==1){
              rec_dur = updateVal(rec_dur, 1, 3600);
              cDisplay();
              display.println("Record:");
              display.print(rec_dur);
              display.println("s");
              displaySettings();
              displayVoltage();
              display.display();
              delay(2);
            }
            while(digitalRead(SELECT)==0); // wait to let go
            curMenuItem = setStart;
            break;
          
        case setRecSleep:
          while(digitalRead(SELECT)==1){
            rec_int = updateVal(rec_int, 0, 3600 * 24);
            // Snap: no minimum file length in continuous mode (LS1/LS2 force 600 s)
            cDisplay();
            display.println("Sleep:");
            display.print(rec_int);
            display.println("s");
            displaySettings();
            displayVoltage();
            display.display();
            delay(2);
          }
          while(digitalRead(SELECT)==0); // wait to let go
          curMenuItem = setStart;
          break;

        case setFsamp:
          while(digitalRead(SELECT)==1){
            isf = updateVal(isf, 0, 8);
            cDisplay();
            display.println("Rate");
            display.printf("%.1f kHz",lhi_fsamps[isf]/1000.0f);
            displaySettings();
            displayVoltage();
            display.display();
            delay(2);
          }
          while(digitalRead(SELECT)==0); // wait to let go
          curMenuItem = setStart;
          break;

        case setGain:
          while(digitalRead(SELECT)==1){
            gainSetting = updateVal(gainSetting, 0, 15);
            calcGain();
            cDisplay();
            display.print("Gain:");
            display.println(gainSetting);
            display.print(gainDb);
            display.print("dB");
            displaySettings();
            displayVoltage();
            display.display();
            delay(2);
          }
          while(digitalRead(SELECT)==0); // wait to let go
          curMenuItem = setStart;
          break;
          
        case setDateTime:
          while(digitalRead(SELECT)==1){
            oldYear = year(t);
            newYear = updateVal(oldYear, 2026, 2100);  // a DS3231 reading before 2026 counts as failed
            if(oldYear!=newYear){
//              Serial.print("Old year "); Serial.println(oldYear);
//              Serial.print("New year "); Serial.println(newYear);
              setTime2(hour(t), minute(t), second(t), day(t), month(t), newYear - 2000U);
              autoStartTime = getTeensy3Time(0);
//              Serial.print("After set Year "); Serial.println(year(t));
            }
            cDisplay();
            display.println("Year:");
            display.print(year(getTeensy3Time(0)));
            displaySettings();
            displayVoltage();
            display.display();
            delay(2);
          }
          while(digitalRead(SELECT)==0); // wait to let go
          delay(2); // debounce
          
          while(digitalRead(SELECT)==1){
            oldMonth = month(t);
            newMonth = updateVal(oldMonth, 1, 12);
            if(oldMonth != newMonth){
              setTime2(hour(t), minute(t), second(t), day(t), newMonth, year(t) - 2000U);
              autoStartTime = getTeensy3Time(0);
            }
            cDisplay();
            display.println("Month:");
            display.print(month(getTeensy3Time(0)));
            displaySettings();
            displayVoltage();
            display.display();
            delay(2);
          }
          while(digitalRead(SELECT)==0); // wait to let go
          delay(2); // debounce

          while(digitalRead(SELECT)==1){
            oldDay = day(t);
            newDay = updateVal(oldDay, 1, 31);
            if(oldDay!=newDay){
              setTime2(hour(t), minute(t), second(t), newDay, month(t), year(t) - 2000U);
              autoStartTime = getTeensy3Time(0);
            }
            cDisplay();
            display.println("Day:");
            display.print(day(getTeensy3Time(0)));
            displaySettings();
            displayVoltage();
            display.display();
            delay(2);
          }
          while(digitalRead(SELECT)==0); // wait to let go
          delay(2); // debounce

          while(digitalRead(SELECT)==1){
            oldHour = hour(t);
            newHour = updateVal(oldHour, 0, 23);
            if(oldHour!=newHour){
              setTime2(newHour, minute(t), second(t), day(t), month(t), year(t) - 2000U);
              autoStartTime = getTeensy3Time(0);
            }
            cDisplay();
            display.println("Hour:");
            display.print(hour(getTeensy3Time(0)));
            displaySettings();  
            displayVoltage();
            display.display();
            delay(2);
          }
          while(digitalRead(SELECT)==0); // wait to let go
          delay(2); // debounce

          while(digitalRead(SELECT)==1){
            oldMinute = minute(t);
            newMinute = updateVal(oldMinute, 0, 59);
            if(oldMinute!=newMinute){
              setTime2(hour(t), newMinute, second(t), day(t), month(t), year(t) - 2000U);
              autoStartTime = getTeensy3Time(0);
            }
            cDisplay();
            display.println("Minute:");
            display.print(minute(getTeensy3Time(0)));
            displaySettings();
            displayVoltage();
            display.display();
            delay(2);
          }
          while(digitalRead(SELECT)==0); // wait to let go
          delay(2); // debounce

          while(digitalRead(SELECT)==1){
            oldSecond = second(t);
            newSecond = updateVal(oldSecond, 0, 59);
            if(oldSecond!=newSecond){
              setTime2(hour(t), minute(t), newSecond, day(t), month(t), year(t) - 2000U);
              autoStartTime = getTeensy3Time(0);
            }
            cDisplay();
            display.println("Second:");
            display.print(second(getTeensy3Time(0)));
            displaySettings();
            displayVoltage();
            display.display();
            delay(2);
          }
          while(digitalRead(SELECT)==0); // wait to let go
          curMenuItem = setStart;
          break;

          case setDelay:
            while(digitalRead(SELECT)==1){
                delayDays = updateVal(delayDays, 0, 365);
                cDisplay();
                display.print("delayDays:");
                display.print(delayDays);
                display.display();
                delay(2);
              }
              while(digitalRead(SELECT)==0); // wait to let go
              curMenuItem = setStart;
              break;

        case setBatPacks:
          while(digitalRead(SELECT)==1){
              nBatPacks = updateVal(nBatPacks, 1, 12);
              cDisplay();
              display.print("BatPacks:");
              display.print(nBatPacks);
              display.display();
              delay(2);
            }
            while(digitalRead(SELECT)==0); // wait to let go
            curMenuItem = setStart;
            break;

        case setCutoff:
          {
            int cutoffVal = (voltCutoff == 0) ? 29 : voltCutoff;  // 29 shows as Off, below 3.0 V
            while(digitalRead(SELECT)==1){
              cutoffVal = updateVal(cutoffVal, 29, 34);
              voltCutoff = (cutoffVal == 29) ? 0 : cutoffVal;
              cDisplay();
              display.println("V Cutoff:");
              if(voltCutoff == 0) display.print("Off");
              else{
                display.print(voltCutoff / 10.0, 1);
                display.print("V");
              }
              displaySettings();
              displayVoltage();
              display.display();
              delay(2);
            }
          }
          while(digitalRead(SELECT)==0); // wait to let go
          curMenuItem = setStart;
          break;

        case setSdSleep:
          while(digitalRead(SELECT)==1){
            sdSleep = updateVal(sdSleep, 0, 1);
            cDisplay();
            display.println("SD Sleep");
            display.setTextSize(1);
            display.setCursor(0, 16);
            display.println(sdSleep ? "Power off" : "Keep powered");
            display.setCursor(0, 26);
            display.print("Sleeps 2 min or more");
            displaySettings();
            displayVoltage();
            display.display();
            delay(2);
          }
          while(digitalRead(SELECT)==0); // wait to let go
          curMenuItem = setStart;
          break;

      }
      
      if (settingsChanged) {
        writeEEPROM();
        settingsChanged = 0;
        autoStartTime = getTeensy3Time(0);  //reset autoStartTime
      }
    }
    
    cDisplay();
    displayMenu();
    displaySettings();
    displayVoltage();
    displayChannels();
    displayClock(getTeensy3Time(0), BOTTOM);
    display.display();
    delay(10);
  }
  calcDeployment();  // files that fit, also when the menu was skipped
}

// Keep the internal RTC on DS3231 time so it can take over if the DS3231 fails
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
  if(internalRtcOK & (Teensy3Clock.get() != newtime)) Teensy3Clock.set(newtime);
  autoStartTime = newtime;  // not getTeensy3Time(): this runs inside it, mid-sync
}
  
int updateVal(long curVal, long minVal, long maxVal){
  watchdogRefresh();  // every setting screen loops here
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

    if (heldUp > 100) curVal += 4; //if held up for a while skip an additional 4
    if (heldUp > 200) curVal += 55; //if held up for a while skip an additional 4
    
    if(downVal==0){
      settingsChanged = 1;
      if(heldDown < 20) delay(200);
      if(curVal < 61) { // going down to 0, go back to slow mode
        heldDown = 0;
      }
        curVal -= 1;
        heldDown += 1;
    }
    else heldDown = 0;

    if(heldDown > 100) curVal -= 4;
    if(heldDown > 200) curVal -= 55;

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
  t = getTeensy3Time(0);
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 38);
  display.print("Rec");
  display.setCursor(25, 38);
  display.print("Slp");
  display.setCursor(50, 38);
  display.print("kHz");
  display.setCursor(75, 38);
  display.print("dB");
  
  display.setCursor(0, 46);
  display.print(rec_dur);
  
  display.setCursor(25, 46);
  display.print(rec_int);

  display.setCursor(50, 46);
  display.printf("%.0f",lhi_fsamps[isf]/1000.0f);

  display.setCursor(75, 46);
  display.printf("%.0f",gainDb);

  calcDeployment();

  if(powerSeconds < totalSecondsMemory){
   // displayClock(getTeensy3Time(0) + powerSeconds, 45, 0);
    display.setCursor(92, 38);
    display.print("Lim B");
    display.setCursor(92, 46);
    display.print((int) powerSeconds / 86400);
    display.print("d");
  }
  else{
    display.setCursor(92, 38);
    display.print("Lim M");
    display.setCursor(92, 46);
    display.print((int) totalSecondsMemory / 86400);
    display.print("d");
  }

  display.setCursor(118, 46);
  if(recMode==MODE_DIEL){
    display.print("D");
  }
  else{
    display.print("N");
  }
}

// Estimate how long the batteries and the card will last with the current settings.
// Also sets filesPerCard, which checkSD() counts down while recording.
void calcDeployment(){
  float totalRecSeconds = 0;

  if(NCHAN==1) mAmpRec = mAmpMono[isf];
  if(NCHAN==2) mAmpRec = mAmpStereo[isf];

  // A file takes whole clusters, up to almost a cluster more than its size
  uint64_t fileBytes = (uint64_t) NCHAN * 2 * rec_dur * lhi_fsamps[isf] + 44;
  uint64_t fileClusters = (fileBytes + clusterBytes - 1) / clusterBytes;
  fileMB = (float) (fileClusters * clusterBytes) / 1000.0 / 1000.0;
  float dielFraction = 1.0; //diel mode decreases time spent recording, increases time in sleep
  if(recMode==MODE_DIEL){
    float dielHours, dielMinutes;
    if(startHour>endHour){
      dielHours = (24 - startHour) + endHour; //  22 to 05 = 7
    }
    else{
      dielHours = endHour - startHour; //e.g. 10 to 12 = 2; 
    }
    if(startMinute > endMinute){
      dielMinutes = (60-startMinute) + endMinute;
    }
    else{
      dielMinutes = endMinute - startMinute;
    }
    dielMinutes += (dielHours * 60);
    dielFraction = dielMinutes / (24.0 * 60.0); // fraction of day recording in diel mode
  }

  float recDraw = mAmpRec;
  float recFraction = ((float) rec_dur * dielFraction) / (float) (rec_dur + rec_int);
  float sleepFraction = 1 - recFraction;
  // the card is switched off only for sleeps of 2 min or more (diel nights are not counted)
  boolean cardOffInSleep = sdSleep & (rec_int >= SD_SLEEP_SECONDS + wakeahead);
  float sleepDraw = cardOffInSleep ? mAmpSleepCardOff : mAmpSleep;
  float avgCurrentDraw = (recDraw * recFraction) + (sleepDraw * sleepFraction);

  powerSeconds = uint32_t (3600.0 * (nBatPacks * mAhPerBat / avgCurrentDraw));

//  Serial.print("fileMB FreeMBCards totalRecSeconds: ");
//  Serial.print(fileMB); Serial.print(" ");

  for(int n=0; n<4; n++){
    filesPerCard[n] = 0;
    if(freeMB[n]<=fileMB) filesPerCard[n] = 0;
    else{
      filesPerCard[n] = (uint32_t) floor(freeMB[n] / fileMB);
    }
    
    totalRecSeconds += (filesPerCard[n] * rec_dur);
  }

  totalSecondsMemory = totalRecSeconds / recFraction;
//
//  Serial.print("Power Sec:");
//  Serial.print(powerSeconds);
//  Serial.print(" "); Serial.print((float) powerSeconds / 86400.0);
//  Serial.print("  Mem Sec:");
//  Serial.print(totalSecondsMemory);
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
  nBatPacks = EEPROM.read(14);
  gainSetting = EEPROM.read(15);
  NCHAN = EEPROM.read(16);
  NCHAN = 1;  // Snap: one channel connected, always mono; byte 16 is kept as on LS1/LS2
  delayDays = readEEPROMlong(18);
  voltCutoff = EEPROM.read(24);
  sdSleep = EEPROM.read(EE_SD_SLEEP);
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
  EEPROM.write(14, nBatPacks); //byte
  EEPROM.write(15, gainSetting); //byte
  EEPROM.write(16, NCHAN); //byte
  writeEEPROMlong(18, delayDays); //long
  EEPROM.write(24, voltCutoff); //byte
  EEPROM.write(EE_SD_SLEEP, sdSleep); //byte
}

void displayMenu(){
  display.setTextSize(2);
  display.println(menuItem[curMenuItem]);
  display.setTextSize(1);
  display.println(helpText[curMenuItem]);
}

void displayVoltage(){
  display.setTextSize(1);
  display.setCursor(100, 0);
  display.print(readVoltage(),1);
  display.print("V");
}

void displayChannels(){
  display.setTextSize(1);
  display.setCursor(100, 8);
  if(rtcFail & ((millis() / 1000) % 2)) display.print("RTC!");  // alternate with channels each second
  else if(NCHAN==1) display.print("Mono");
  else {
    display.setCursor(90, 8);
    display.print("Stereo");
  }
}
