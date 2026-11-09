// Bench diagnostics, on when sleepDiags = 1.
//
// Wake log: USB is disconnected during deep sleep and takes about a second to come back, so
// lines printed right after a wake are lost. Each wake in sleepUntil() is stored here and the
// whole log is printed at the next Record Start, when USB is back.
//
// No-audio report: when the audio watchdog (checkAudio() in the main sketch) finds no audio
// for 3 s, print the I2S, DMA and clock registers, and show them on the display before the
// first audio restart. A codec that is powered down still gives zeros, because the Teensy
// generates the I2S clocks; no data at all means the Teensy I2S receiver or its DMA has stopped.

#define WAKE_LOG_N 16
uint32_t wakeLogRtc[WAKE_LOG_N];
long wakeLogSecs[WAKE_LOG_N];
uint8_t wakeLogWho[WAKE_LOG_N];
uint8_t wakeLogClk[WAKE_LOG_N];
uint8_t wakeLogPin[WAKE_LOG_N];
uint8_t wakeLogDs[WAKE_LOG_N][3];  // DS3231 seconds, control, status; FF FF FF if no reply
int wakeLogCount = 0;              // wakes since the log was last printed

void wakeLogAdd(int who, long secs){
  int i = wakeLogCount % WAKE_LOG_N;
  byte reg[16];
  wakeLogRtc[i] = Teensy3Clock.get();
  wakeLogSecs[i] = secs;
  wakeLogWho[i] = who;
  wakeLogClk[i] = clockSource;
  wakeLogPin[i] = digitalRead(DS3231_INT_PIN);
  if(readDSRegs(reg, 16) == 16){
    wakeLogDs[i][0] = reg[0];
    wakeLogDs[i][1] = reg[14];
    wakeLogDs[i][2] = reg[15];
  }
  else wakeLogDs[i][0] = wakeLogDs[i][1] = wakeLogDs[i][2] = 0xFF;
  wakeLogCount++;
}

void printWakeLog(){
  if(wakeLogCount == 0) return;
  int first = (wakeLogCount > WAKE_LOG_N) ? wakeLogCount - WAKE_LOG_N : 0;
  Serial.printf("wake log (16 DS INT, 35 RTC alarm, 36 LPTMR), %d wakes\n", wakeLogCount);
  if(first) Serial.printf(" first %d not kept\n", first);
  for(int n = first; n < wakeLogCount; n++){
    int i = n % WAKE_LOG_N;
    Serial.printf(" wake %d after %ld s asked, clk %d, int RTC %lu, INT pin %d, DS sec %02X ctrl %02X stat %02X\n",
      wakeLogWho[i], wakeLogSecs[i], wakeLogClk[i], (unsigned long) wakeLogRtc[i], wakeLogPin[i],
      wakeLogDs[i][0], wakeLogDs[i][1], wakeLogDs[i][2]);
  }
  wakeLogCount = 0;
}

void reportAudioStall(boolean showDisplay){
  int ch = i2sRxDmaChannel();
  volatile uint8_t *chcfg = &DMAMUX0_CHCFG0;
  uint16_t citer1 = 0, citer2 = 0, csr = 0;
  uint32_t daddr = 0;
  if(ch >= 0){
    uint32_t tcd = 0x40009000 + ch * 32;          // DMA transfer control descriptor
    citer1 = *(volatile uint16_t *)(tcd + 0x16);  // samples left in the buffer
    delay(10);                                    // 480 samples at 48 kHz if DMA is running
    citer2 = *(volatile uint16_t *)(tcd + 0x16);
    csr = *(volatile uint16_t *)(tcd + 0x1C);
    daddr = *(volatile uint32_t *)(tcd + 0x10);
  }

  Serial.printf("NO AUDIO for %lu s, buf_count %ld, queue %d, audio mem %d max %d\n",
    (millis() - lastAudioMillis) / 1000, (long) buf_count, queue1.available(),
    AudioMemoryUsage(), AudioMemoryUsageMax());
  Serial.printf(" I2S TCSR %08lX RCSR %08lX MCR %08lX MDR %08lX TCR2 %08lX RCR2 %08lX\n",
    (unsigned long) I2S0_TCSR, (unsigned long) I2S0_RCSR, (unsigned long) I2S0_MCR,
    (unsigned long) I2S0_MDR, (unsigned long) I2S0_TCR2, (unsigned long) I2S0_RCR2);
  Serial.printf(" DMA ch %d cfg %02X ERQ %08lX ERR %08lX ES %08lX INT %08lX CITER %u then %u CSR %04X DADDR %08lX\n",
    ch, (ch >= 0) ? chcfg[ch] : 0, (unsigned long) DMA_ERQ, (unsigned long) DMA_ERR,
    (unsigned long) DMA_ES, (unsigned long) DMA_INT, citer1, citer2, csr, (unsigned long) daddr);
  Serial.printf(" NVIC dma irq %d audio irq %d, SCGC6 %08lX SCGC7 %08lX\n",
    (ch >= 0) ? (NVIC_IS_ENABLED(IRQ_DMA_CH0 + ch) != 0) : -1, NVIC_IS_ENABLED(IRQ_SOFTWARE) != 0,
    (unsigned long) SIM_SCGC6, (unsigned long) SIM_SCGC7);
  Serial.printf(" MCG S %02X C1 %02X C5 %02X C6 %02X, PMSTAT %02X\n",
    MCG_S, MCG_C1, MCG_C5, MCG_C6, SMC_PMSTAT);
  printWakeLog();

  if(showDisplay){
    display.ssd1306_command(SSD1306_DISPLAYON);
    cDisplay();
    display.println("NO AUDIO");
    display.setTextSize(1);
    display.printf("T%08lX R%08lX\n", (unsigned long) I2S0_TCSR, (unsigned long) I2S0_RCSR);
    display.printf("ch%d ES%08lX\n", ch, (unsigned long) DMA_ES);
    display.printf("ERQ%04lX CIT%u>%u\n", (unsigned long) (DMA_ERQ & 0xFFFF), citer1, citer2);
    display.printf("MCG%02X mem%d q%d\n", MCG_S, AudioMemoryUsage(), queue1.available());
    display.display();
  }
}
