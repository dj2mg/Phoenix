// ---------------------------------------------------------------- /
// Test of RX switch
// ---------------------------------------------------------------- /

#include <Wire.h>
#include <Adafruit_MCP23X08.h>

//
// The BOARD ADDRESS is the only adjustable parameter in this program
//
#define RX_SWITCH_MCP23008_ADDR 0x20 

// Define GP bits
#define INPUT_RX1   0x0001
#define INPUT_RX2   0x0002
#define INPUT_TX    0x0004
#define INPUT_MASK  0x0007
#define ENABLE_VLF  0x0080

static Adafruit_MCP23X08 mcpBPF;

uint16_t GP_state;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  delay(5000);

  // Set Wire2 I2C bus to 100KHz and start
  Wire2.setClock(100000UL);
  Wire2.begin();
  
  while (!mcpBPF.begin_I2C(RX_SWITCH_MCP23008_ADDR,&Wire2)){
    Serial.println("BPF MCP23008 not found at 0x"+String(RX_SWITCH_MCP23008_ADDR,HEX));
    delay(5000);
  }

  // Enable the address pins A0, A1, and A2.  
  mcpBPF.enableAddrPins();
  // Set all chip pins to be outputs
  for (int i=0;i<16;i++){
    mcpBPF.pinMode(i, OUTPUT);
  }
  // All off for startup.
  GP_state = 0;
  mcpBPF.writeGPIO(0); 

}

void print_state(void){
  uint16_t input = GP_state & INPUT_MASK;
  Serial.print("INPUT: ");
  switch(input) {
    case INPUT_RX1:
      Serial.print("RX1 [0b");
      Serial.print(INPUT_RX1,BIN);
      Serial.println("]");
      break;
    case INPUT_RX2:
      Serial.print("RX2 [0b");
      Serial.print(INPUT_RX2,BIN);
      Serial.println("]");
      break;
    case INPUT_TX:
      Serial.print("INPUT_TX [0b");
      Serial.print(INPUT_TX,BIN);
      Serial.println("]");
      break;
    default:
      Serial.print("No input: [0b");
      Serial.print(input, BIN);
      Serial.print("]");
      break;
  }

  Serial.print("VLF: ");
  uint16_t vlf = GP_state & ENABLE_VLF;
  if (vlf)
      Serial.print("ON [0b");
  else
      Serial.print("OFF [0b");
  Serial.print(vlf, BIN);
  Serial.println("]");
}

void loop() {
  // print the state and selection menu
  Serial.println("Current state:");
  print_state();
  Serial.println("");
  Serial.println("Select option and hit enter:");
  Serial.println("N   - No input");
  Serial.println("1   - Select RX1");
  Serial.println("2   - Select RX2");
  Serial.println("3   - Select TX");
  Serial.println("V   - Toggle VLF");
  
  while (Serial.available() == 0) {}       //wait for data available
  String selection = Serial.readString();  //read until timeout
  selection.trim();                        // remove any \r \n whitespace at the end of the String
  
  Serial.print("Selection was-> ");
  Serial.println(selection);
  if (selection == "N"){
      Serial.println("Disabling input");
      GP_state = GP_state & ~INPUT_MASK;
  } 
  if (selection == "1"){
      Serial.println("Selecting RX1");
      GP_state = (GP_state & ~INPUT_MASK) | INPUT_RX1;
  } 
  if (selection == "2") {
      Serial.println("Selecting RX2");
      GP_state = (GP_state & ~INPUT_MASK) | INPUT_RX2;
  }
  if (selection == "3") {
      Serial.println("Selecting TX");
      GP_state = (GP_state & ~INPUT_MASK) | INPUT_TX;
  }
  if (selection == "V"){
      Serial.println("Toggling VLF");
      GP_state = GP_state ^ ENABLE_VLF;
  } 

  mcpBPF.writeGPIO(GP_state);
  Serial.print("Written to GP: [");
  Serial.print(GP_state, BIN);
  Serial.println("]");
  Serial.println("-------------------------\n");

}

