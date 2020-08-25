// Water/gravity code size estimate
// New code:
//    3656 w/water, 2854 w/o = 802 bytes
// Old code base:
//    5752 w/water, 4862 w/o = 890 bytes
// Old code special:
//    5372          4354

// Code changes:
// * Water state simplified between tiles - saves corner cases (code space)
// * No longer asymmetric - special tiles part of the normal sketch
// * Dripper and dirt tiles integrated into normal tiles instead of taking over the whole tile
// * Changed plants to be more data-centric to save code space
// * All comms are expendable. Removes a lot of error checking code.
// * Removed dim() from blinklib to save > 100 bytes
// * Removed all use of makeColorHSB
// * Added setColorOnFace2 to accept a pointer instead of raw Color value
// * Simplified sun system (leaves & branches don't block sun)
// * Genericized systems (evaporation, bug transfer)

// Nice-to-haves:
// * Sun pulses do half and half to show direction within the tile

#define null 0

// Defines to enable debug code
#define DEBUG_COMMS         0
#define DETECT_COMM_ERRORS  0
#define USE_DATA_SPONGE     0
#define TRACK_FRAME_TIME    0
//#define NO_STACK_WATCHER
#define DEBUG_PLANT_ENERGY  0
#define DEBUG_SPAWN_BUG     0

#if TRACK_FRAME_TIME
unsigned long currentTime = 0;
byte frameTime;
byte worstFrameTime = 0;
#endif

#if USE_DATA_SPONGE
byte sponge[25];
// Aug 20: 45-49 bytes
// Aug 21: 50, 17 
// Aug 22: 10, 5
// Aug 23: 24
#endif

#define MAX(x,y) ((x) > (y) ? (x) : (y))
#define MIN(x,y) ((x) < (y) ? (x) : (y))

#define OPPOSITE_FACE(f) (((f) < 3) ? ((f) + 3) : ((f) - 3))

#define CCW_FROM_FACE CCW_FROM_FACE1
#define CCW_FROM_FACE1(f, amt) (((f) - (amt)) + (((f) >= (amt)) ? 0 : 6))

#define CW_FROM_FACE CW_FROM_FACE1
#define CW_FROM_FACE2(f, amt) (((f) + (amt)) % FACE_COUNT)
//byte cwFromFace[] = { 0, 1, 2, 3, 4, 5, 0, 1, 2, 3, 4, 5 };

// =================================================================================================
//
// COLORS
//
// =================================================================================================

#define HUE_DRIPPER          106    // HSB=106,255,255  RGB=0,255,128
#define RGB_DRIPPER    0,255,128

#define HUE_DIRT              32    // HSB=32,255,255   RGB=255,191,0     HSB=32,255,128   RGB=128,96,0
#define RGB_DIRT        128,96,0

#define HUE_SUN               42    // HSB=42,255,255   RGB=255,251,0     HSB=42,255,192   RGB=191,188,0
                                    // HSB=42,255,128   RGB=128,125,0     HSB=42,255,64    RGB=64,63,0
#define RGB_SUN        255,251,0
#define RGB_SUN_PULSE    64,63,0
                                    
#define HUE_BUG               55    // HSB=55,255,255   RGB=179,255,0     HSB=55,255,192   RGB=134,191,0
#define RGB_BUG_OFF    134,191,0
#define RGB_BUG_ON     179,255,0

#define HUE_WATER            171    // HSB=171,255,128  RGB=2,0,128
#define RGB_WATER        2,0,128

#define HUE_LEAF              85    // HSB=85,255,255   RGB=0,255,0       HSB=85,255,192   RGB=0,191,0
#define RGB_LEAF_OFF     0,191,0
#define RGB_LEAF_ON      0,255,0

#define HUE_BRANCH            28    // HSB=28,255,160   RGB=161,107,0     HSB=28,255,192   RGB=191,128,0
#define RGB_BRANCH_OFF 161,107,0
#define RGB_BRANCH_ON  191,128,0

#define HUE_FLOWER           233    // HSB=233,255,255  RGB=255,0,132     HSB=233,255,192  RGB=191,0,99
#define RGB_FLOWER_OFF 192,160,160// 191,0,99
#define RGB_FLOWER_ON  255,192,192//255,0,132

#if DEBUG_COLORS
#define COLOR_PLANT_GROWTH1 makeColorRGB( 128,  0,  64)
#define COLOR_PLANT_GROWTH2 makeColorRGB(  64,  0, 128)
#define COLOR_PLANT_GROWTH3 makeColorRGB(  64, 64,  64)
#endif

// =================================================================================================
//
// COMMUNICATIONS
//
// =================================================================================================

#define TOGGLE_COMMAND 1
#define TOGGLE_DATA 0
struct FaceValue
{
  byte value  : 4;
  byte toggle : 1;
  byte ack    : 1;
};

enum FaceFlag
{
  FaceFlag_NeighborPresent    = 1<<0,
  
  FaceFlag_WaterFull          = 1<<2,
  FaceFlag_NeighborWaterFull  = 1<<3,
  
  FaceFlag_Debug              = 1<<7
};

struct FaceState
{
  FaceValue faceValueIn;
  FaceValue faceValueOut;
  byte lastCommandIn;
  byte flags;

  // ---------------------------
  // Application-specific fields
  byte waterLevel;
  byte waterAdded;
  byte waterStored;   // dirt tiles hold on to water
  // ---------------------------

#if DEBUG_COMMS
  byte ourState;
  byte neighborState;
#endif
};
FaceState faceStates[FACE_COUNT];

enum Command
{
  Command_None,
  Command_SetWaterFull,   // Tells neighbor we are full of water
  Command_WaterAdd,       // Adds water to this face
  Command_GravityDir,     // Propagates the gravity direction
  Command_DistEnergy,     // Distribute plant energy
  Command_SendSun,        // Send sunlight from Sun tiles
  Command_GatherSun,      // Gather sun from leaves to root
  Command_TransferBug,    // Attempt to transfer a bug from one tile to another (must be confirmed)
  Command_TransferBugCW,  // Same as above, but transfer to the face CW from the receiver
  Command_BugAccepted,    // Confirmation of the transfer - sender must tolerate this never arriving, or arriving late
  Command_RainbowCheck,   // Follows a sunbeam looking for a rainbow
  Command_RainbowFound,   // Found a rainbow!
  Command_ChildBranchGrew,// Sent down a plant to tell it that something grew

#if DEBUG_COMMS
  Command_UpdateState,
#endif
  
  Command_MAX
};

struct CommandAndData
{
  Command command : 4;
  byte data : 4;
};

#define COMM_QUEUE_SIZE 4
CommandAndData commQueues[FACE_COUNT][COMM_QUEUE_SIZE];
byte commInsertionIndexes[FACE_COUNT];

#if DETECT_COMM_ERRORS
#define COMM_INDEX_ERROR_OVERRUN 0xFF
#define COMM_INDEX_OUT_OF_SYNC   0xFE
#define COMM_DATA_OVERRUN        0xFD
#define ErrorOnFace(f) (commInsertionIndexes[f] > COMM_QUEUE_SIZE)
#endif


#if DEBUG_COMMS
// Timer used to toggle between green & blue
Timer sendNewStateTimer;
#endif

// =================================================================================================
//
// TILE STATE
//
// =================================================================================================

// Gravity is a constant velocity (not accelerative)
#define GRAVITY_RATE 200 // ms to fall from pixel to pixel
Timer gravityTimer;

enum TileFlags
{
  TileFlag_HasDripper = 1<<0,
  TileFlag_HasDirt    = 1<<1,
  TileFlag_HasSun     = 1<<2,
  TileFlag_PulseSun   = 1<<3,
  TileFlag_SpawnedBug = 1<<4,
  TileFlag_HasBug     = 1<<5,
};
byte tileFlags = 0;

// =================================================================================================
//
// SYSTEMS
//
// =================================================================================================

// -------------------------------------------------------------------------------------------------
// DRIPPER/WATER
//
#define DRIPPER_AMOUNT 4
#define DRIPPER_RATE 2000   // ms between drips
Timer dripperTimer;
byte dripperSpeedScale = 0;

#define MAX_WATER_LEVEL 15  // water level can actually go above this, but will stop accepting new water
#define WATER_FLOW_AMOUNT 4

#define EVAPORATION_RATE 5000
Timer evaporationTimer;

#define WATER_FELL_TIMEOUT 2000
Timer waterFellTimer;

// -------------------------------------------------------------------------------------------------
// GRAVITY
//
// This timer is used by gravity tiles to broadcast the gravity direction
// It is also used by normal tiles to know not to accept any new gravity for a while
#define SEND_GRAVITY_RATE 3000
#define CHECK_GRAVITY_RATE (SEND_GRAVITY_RATE-1000)
Timer sendGravityTimer;
byte gravityUpFace = 0;

// -------------------------------------------------------------------------------------------------
// DIRT
//

// Used with plants. This is the rate that...
// ...collected sun is transmitted down the plant to the dirt
// ...dirt tiles distributes energy up to its plants
// ...plants distribute excess energy upwards to continue growth
#define PLANT_ENERGY_RATE 5000
Timer plantEnergyTimer;

// Timer that controls how often a plant can grow (or wither)
#define PLANT_GROWTH_RATE 5000
Timer plantGrowthTimer;

// -------------------------------------------------------------------------------------------------
// PLANT
//
enum PlantExitFace
{
  PlantExitFace_None,
  PlantExitFace_OneAcross,
  PlantExitFace_Fork,
};

/*
struct PlantStateNode
{
  // Energy and state progression
  byte energyMaintain     : 1;  // plant energy required to stay alive (1 or 2)
  byte energyGrow         : 2;  // plant energy required to progress to the next state
  byte growState1         : 2;  // state offset when growing (1) [0=no growth, 1=+1, 2=+2, 3=+?]
  byte growState2         : 2;  // state offset when growing (2) [same, but also added to growState1]
  byte witherState        : 2;  // state offset (backwards) when withering (0=dead)

  // Growth into neighbors
  PlantExitFace exitFace  : 2;  // none, one across, fork

  // Render info
  int  faceLUTIndexes     : 12; // color lookup for each face (none, leaf, branch, flower)
};
*/

struct PlantStateNode
{
  // Energy and state progression
  byte energyGrow         : 2;  // plant energy required to progress to the next state
  byte growState1         : 4;  // state offset when growing (1)
  byte growState2         : 2;  // state offset when growing (2) [same, but also added to growState1]
  byte witherState        : 4;  // state offset (backwards) when withering (0=dead)
  
  // Flags
  byte waitForGrowth      : 1;  // flag to only proceed once a child branch has gathered sun

  // Growth into neighbors
  PlantExitFace exitFace  : 2;  // none, one across, fork

  // Render info
  byte faceLUTIndexes     : 8;
};

PlantStateNode plantStateGraphTree[] =
{
// BASE  
//  E   G1 G2 W   WFG  EXITS                      R  4 3 2 0
  { 1,  1, 0, 0,  0,   PlantExitFace_None,        0b00000000 }, // START
  { 2,  1, 0, 0,  0,   PlantExitFace_None,        0b00000000 }, // GROWING
  { 3,  1, 0, 1,  0,   PlantExitFace_OneAcross,   0b00010000 }, // LEAF
  { 1,  1, 0, 1,  1,   PlantExitFace_OneAcross,   0b00010000 }, // LEAF (WAIT FOR GROWTH)
  { 1,  1, 0, 1,  0,   PlantExitFace_OneAcross,   0b00100000 }, // BRANCH
  { 0,  0, 0, 1,  0,   PlantExitFace_OneAcross,   0b00100000 }, // BRANCH

// BRANCHES
//  E   G1 G2 W   WFG  EXITS                      R  4 3 2 0
  { 1,  1, 0, 0,  0,   PlantExitFace_None,        0b00000000 }, // START
  { 2,  1, 0, 0,  0,   PlantExitFace_None,        0b00000000 }, // GROWING
  { 3,  2, 0, 0,  0,   PlantExitFace_None,        0b00000001 }, // BASE LEAF
  { 3,  1, 0, 2,  0,   PlantExitFace_None,        0b00000010 }, // BASE BRANCH
  { 3,  1, 1, 1,  0,   PlantExitFace_None,        0b00010010 }, // BASE BRANCH + CENTER LEAF (CHOICE)
  { 3,  2, 0, 1,  0,   PlantExitFace_None,        0b00010110 }, // BASE BRANCH + CENTER LEAF + LEFT LEAF
  { 3,  1, 0, 2,  0,   PlantExitFace_None,        0b01010010 }, // BASE BRANCH + CENTER LEAF + RIGHT LEAF

// FLOWER or BRANCH (1/8 chance of flower)
//  E   G1 G2 W   WFG  EXITS                      R  4 3 2 0
  { 3,  1, 4, 3,  0,   PlantExitFace_None,        0b01010110 }, // BASE BRANCH + THREE LEAVES (CHOICE)
  { 0,  1, 3, 4,  0,   PlantExitFace_None,        0b01010110 }, // BASE BRANCH + THREE LEAVES (CHOICE)
  { 0,  2, 1, 5,  0,   PlantExitFace_None,        0b01010110 }, // BASE BRANCH + THREE LEAVES (CHOICE)

// FLOWER
//  E  G1 G2  W   WFG  EXITS                      R  4 3 2 0
  { 3,  3, 0, 5,  0,   PlantExitFace_None,        0b00110010 }, // BASE BRANCH + FLOWER
  { 0,  0, 0, 1,  0,   PlantExitFace_None,        0b01110110 }, // BASE BRANCH + FLOWER + TWO LEAVES

// CENTER BRANCH or FORK
//  E  G1 G2  W   WFG  EXITS                      R  4 3 2 0
  { 0,  3, 2, 6,  0,   PlantExitFace_None,        0b01010110 }, // BASE BRANCH + THREE LEAVES (CHOICE)

// FORK BRANCH
//  E  G1 G2  W   WFG  EXITS                      R  4 3 2 0
  { 3,  1, 0, 0,  0,   PlantExitFace_None,        0b00000010 }, // BASE BRANCH
  { 3,  2, 0, 4,  0,   PlantExitFace_Fork,        0b10011010 }, // BASE BRANCH + TWO BRANCHES
  { 0,  1, 0, 11, 1,   PlantExitFace_Fork,        0b01010110 }, // START - BASE BRANCH + THREE LEAVES (WAIT FOR GROWTH)
  { 0,  0, 0, 2,  0,   PlantExitFace_Fork,        0b10011010 }, // BASE BRANCH + TWO BRANCHES + CENTER LEAF

// CENTER BRANCH
//  E  G1 G2  W   WFG  EXITS                      R  4 3 2 0
  { 0,  5, 0, 13, 1,   PlantExitFace_OneAcross,   0b01010110 }, // START - BASE BRANCH + THREE LEAVES (WAIT FOR GROWTH)
  { 3,  1, 0, 0,  0,   PlantExitFace_None,        0b00000010 }, // BASE BRANCH
  { 3,  1, 1, 1,  0,   PlantExitFace_OneAcross,   0b00100110 }, // BASE BRANCH + CENTER BRANCH
  { 3,  2, 0, 1,  0,   PlantExitFace_OneAcross,   0b00100110 }, // BASE BRANCH + CENTER BRANCH + LEFT LEAF
  { 3,  1, 0, 2,  0,   PlantExitFace_OneAcross,   0b00100110 }, // BASE BRANCH + CENTER BRANCH + RIGHT LEAF
  { 0,  0, 0, 3,  0,   PlantExitFace_OneAcross,   0b01100110 }, // BASE BRANCH + CENTER BRANCH + TWO LEAVES
};

enum PlantFaceType
{
  PlantFaceType_None,
  PlantFaceType_Leaf,
  PlantFaceType_Branch,
  PlantFaceType_Flower
};

bool hasPlant;
byte plantStateNodeIndex;
byte plantRootFace;
byte plantEnergy;
#if DEBUG_PLANT_ENERGY
byte debugPlantEnergy;
#endif

bool plantChildBranchGrew;   // flag that a child branch has grown somewhere in the plant

byte plantNumLeaves;
byte plantNumBranches;

// -------------------------------------------------------------------------------------------------
// SUN
//
#define SUN_STRENGTH 3
#define GENERATE_SUN_RATE 500
Timer generateSunTimer;

byte sunDirection = 3;

#define SUN_PULSE_RATE 250
Timer sunPulseTimer;

#define SUN_LIT_RATE 1000    // longer than GENERATE_SUN_RATE so it stays set while sun is shining
Timer sunLitTimer;

// Most gathered sunlight a tile can hold
// The practical max will be a lot lower than this because leaves only collect 1 unit at a time
#define MAX_GATHERED_SUN 200
byte gatheredSun;

#define ENERGY_PER_SUN 3

// -------------------------------------------------------------------------------------------------
// RAINBOW
//
#define RAINBOW_TIMEOUT 2000
Timer rainbowTimer;

byte rainbowIndex;
byte rainbowDirection;

#define RAINBOW_INTENSITY1  255
#define RAINBOW_INTENSITY2  192
#define RAINBOW_INTENSITY3  128

#define COLOR_RAINBOW_1  RAINBOW_INTENSITY1,                  0,                  0
#define COLOR_RAINBOW_2  RAINBOW_INTENSITY1, RAINBOW_INTENSITY3,                  0
#define COLOR_RAINBOW_3  RAINBOW_INTENSITY1, RAINBOW_INTENSITY2,                  0
#define COLOR_RAINBOW_4  RAINBOW_INTENSITY1, RAINBOW_INTENSITY1,                  0
#define COLOR_RAINBOW_5  RAINBOW_INTENSITY2, RAINBOW_INTENSITY1,                  0
#define COLOR_RAINBOW_6  RAINBOW_INTENSITY3, RAINBOW_INTENSITY1,                  0
#define COLOR_RAINBOW_7                   0, RAINBOW_INTENSITY1,                  0
#define COLOR_RAINBOW_8                   0, RAINBOW_INTENSITY1, RAINBOW_INTENSITY1
#define COLOR_RAINBOW_9                   0,                  0, RAINBOW_INTENSITY1
#define COLOR_RAINBOW_10 RAINBOW_INTENSITY3,                  0, RAINBOW_INTENSITY1

// -------------------------------------------------------------------------------------------------
// BUG
//
#define BUG_FLAP_RATE 100
Timer bugFlapTimer;
#define BUG_MOVE_RATE 7

byte bugTargetCorner  = 0;
char bugDistance      = 0;
char bugDirection     = 1;
byte bugFlapOpen      = 0;

// =================================================================================================
//
// SETUP
//
// =================================================================================================

void setup()
{
#if USE_DATA_SPONGE
  // Use our data sponge so that it isn't compiled away
  if (sponge[0])
  {
    sponge[1] = 3;
  }
#endif

#if TRACK_FRAME_TIME
  currentTime = millis();
#endif
  
  resetOurState();
  FOREACH_FACE(f)
  {
    resetCommOnFace(f);
  }
}

// =================================================================================================
//
// COMMUNICATIONS
//
// =================================================================================================

void resetCommOnFace(byte f)
{
  // Clear the queue
  commInsertionIndexes[f] = 0;

  FaceState *faceState = &faceStates[f];

  // Put the current output into its reset state.
  // In this case, all zeroes works for us.
  // Assuming the neighbor is also reset, it means our ACK == their TOGGLE.
  // This allows the next pair to be sent immediately.
  // Also, since the toggle bit is set to TOGGLE_DATA, it will toggle into TOGGLE_COMMAND,
  // which is what we need to start sending a new pair.
  memclr((byte*) &faceState->faceValueOut, sizeof(FaceValue));
  sendValueOnFace(f, faceState->faceValueOut);
}

void sendValueOnFace(byte f, FaceValue faceValue)
{
  byte outVal = *((byte*)&faceValue);
  setValueSentOnFace(outVal, f);
}

// Called by the main program when this tile needs to tell something to
// a neighbor tile.
void enqueueCommOnFace(byte f, Command command, byte data)
{
  // Check here so callers don't all need to check
  if (!(faceStates[f].flags & FaceFlag_NeighborPresent))
  {
    return;
  }
  
  if (commInsertionIndexes[f] >= COMM_QUEUE_SIZE)
  {
    // Buffer overrun - might need to increase queue size to accommodate
#if DETECT_COMM_ERRORS
    commInsertionIndexes[f] = COMM_INDEX_ERROR_OVERRUN;
#endif
    return;
  }

#if DETECT_COMM_ERRORS
  if (data & 0xF0)
  {
    commInsertionIndexes[f] = COMM_DATA_OVERRUN;
  }
#endif
  
  byte index = commInsertionIndexes[f];
  commQueues[f][index].command = command;
  commQueues[f][index].data = data;
  commInsertionIndexes[f]++;
}

// Called every iteration of loop(), preferably before any main processing
// so that we can act on any new data being received.
void updateCommOnFaces()
{
  FOREACH_FACE(f)
  {
    FaceState *faceState = &faceStates[f];

    // Is the neighbor still there?
    if (isValueReceivedOnFaceExpired(f))
    {
      // Lost the neighbor - no longer in sync
      resetCommOnFace(f);
      faceState->flags &= ~FaceFlag_NeighborPresent;
      faceState->flags &= ~FaceFlag_NeighborWaterFull;
      continue;
    }

#if DETECT_COMM_ERRORS
    // If there is any kind of error on the face then do nothing
    // The error can be reset by removing the neighbor
    if (ErrorOnFace(f))
    {
      continue;
    }
#endif

    faceState->flags |= FaceFlag_NeighborPresent;

    // Read the neighbor's face value it is sending to us
    byte val = getLastValueReceivedOnFace(f);
    faceState->faceValueIn = *((FaceValue*)&val);
    
    //
    // RECEIVE
    //

    // Did the neighbor send a new comm?
    // Recognize this when their TOGGLE bit changed from the last value we got.
    if (faceState->faceValueOut.ack != faceState->faceValueIn.toggle)
    {
      // Got a new comm - process it
      byte value = faceState->faceValueIn.value;
      if (faceState->faceValueIn.toggle == TOGGLE_COMMAND)
      {
        // This is the first part of a comm (COMMAND)
        // Save the command value until we get the data
        faceState->lastCommandIn = value;
      }
      else
      {
        // This is the second part of a comm (DATA)
        // Do application-specific stuff with the comm
        // Use the saved command value to determine what to do with the data
        processCommForFace(faceState->lastCommandIn, value, f);
      }

      // Acknowledge that we processed this value so the neighbor can send the next one
      faceState->faceValueOut.ack = faceState->faceValueIn.toggle;
    }
    
    //
    // SEND
    //
    
    // Did the neighbor acknowledge our last comm?
    // Recognize this when their ACK bit equals our current TOGGLE bit.
    if (faceState->faceValueIn.ack == faceState->faceValueOut.toggle)
    {
      // If we just sent the DATA half of the previous comm, check if there 
      // are any more commands to send.
      if (faceState->faceValueOut.toggle == TOGGLE_DATA)
      {
        if (commInsertionIndexes[f] == 0)
        {
          // Nope, no more comms to send - bail and wait
          continue;
        }
      }

      // Send the next value, either COMMAND or DATA depending on the toggle bit

      // Toggle between command and data
      faceState->faceValueOut.toggle = ~faceState->faceValueOut.toggle;
      
      // Grab the first element in the queue - we'll need it either way
      CommandAndData commandAndData = commQueues[f][0];

      // Send either the command or data depending on the toggle bit
      if (faceState->faceValueOut.toggle == TOGGLE_COMMAND)
      {
        faceState->faceValueOut.value = commandAndData.command;
      }
      else
      {
        faceState->faceValueOut.value = commandAndData.data;
  
        // No longer need this comm - shift everything towards the front of the queue
        for (byte commIndex = 1; commIndex < COMM_QUEUE_SIZE; commIndex++)
        {
          commQueues[f][commIndex-1] = commQueues[f][commIndex];
        }

        // Adjust the insertion index since we just shifted the queue
#if DETECT_COMM_ERRORS
        if (commInsertionIndexes[f] == 0)
        {
          // Shouldn't get here - if so something is funky
          commInsertionIndexes[f] = COMM_INDEX_OUT_OF_SYNC;
          continue;
        }
        else
        {
#endif
          commInsertionIndexes[f]--;
#if DETECT_COMM_ERRORS
        }
#endif
      }
    }
  }

  FOREACH_FACE(f)
  {
    // Update the value sent in case anything changed
    sendValueOnFace(f, faceStates[f].faceValueOut);
  }
}

// =================================================================================================
//
// LOOP
//
// =================================================================================================

void loop()
{
#if TRACK_FRAME_TIME
  unsigned long previousTime = currentTime;
  currentTime = millis();

  // Clamp frame time at 255 ms to fit within a byte
  // Hopefully processing doesn't ever take longer than that for one frame
  unsigned long timeSinceLastLoop = currentTime - previousTime;
  frameTime = (timeSinceLastLoop > 255) ? 255 : (timeSinceLastLoop & 0xFF);
  if (frameTime > worstFrameTime)
  {
    worstFrameTime = frameTime;
  }
#endif

  // Detect button clicks
  handleUserInput();

  // Update neighbor presence and comms
  updateCommOnFaces();

#if DEBUG_COMMS
  if (sendNewStateTimer.isExpired())
  {
    FOREACH_FACE(f)
    {
      byte nextVal = faceStates[f].neighborState == 2 ? 3 : 2;
      faceStates[f].neighborState = nextVal;
      enqueueCommOnFace(f, Command_UpdateState, nextVal);
    }
    sendNewStateTimer.set(500);
  }
#else // DEBUG_COMMS

  // System loops
  loopDripper();
  loopWater();
  loopGravity();
  loopSun();
  loopDirt();
  loopPlant();    // call after loopDirt
  loopBug();

  // Update water levels and such
  postProcessState();
  
#endif // DEBUG_COMMS

  // Set the colors on all faces according to what's happening in the tile
  render();
}

// =================================================================================================
//
// GENERAL
// Non-system-specific functions.
//
// =================================================================================================

void handleUserInput()
{
  if (buttonMultiClicked())
  {
    byte clicks = buttonClickCount();

    if (clicks >= 5)
    {
      // Five (or more) clicks resets this tile
      resetOurState();
    }
#if DEBUG_SPAWN_BUG
    else if (clicks == 4)
    {
      if (tileFlags & TileFlag_HasBug)
      {
        tileFlags &= ~TileFlag_HasBug;
      }
      else
      {
        tileFlags |= TileFlag_HasBug;
      }
    }
#endif
    else if (clicks == 3)
    {
    }
  }

  if (buttonDoubleClicked())
  {
    // Cycle through the special tiles
    if (tileFlags & TileFlag_HasDripper)
    {
      tileFlags &= ~TileFlag_HasDripper;
      tileFlags |= TileFlag_HasDirt;
    }
    else if (tileFlags & TileFlag_HasDirt)
    {
      tileFlags &= ~TileFlag_HasDirt;
      tileFlags |= TileFlag_HasSun;
      sunDirection = 3;
    }
    else if (tileFlags & TileFlag_HasSun)
    {
      tileFlags &= ~TileFlag_HasSun;
    }
    else
    {
      tileFlags |= TileFlag_HasDripper;
      gravityUpFace = 0;  // dripper defines gravity
    }
  }

  if (buttonSingleClicked() && !hasWoken())
  {
    // Adjust dripper speed or sun direction
    if (tileFlags & TileFlag_HasDripper)
    {
        dripperSpeedScale++;
        if (dripperSpeedScale > 2)
        {
          dripperSpeedScale = 0;
        }
    }
    else if (tileFlags & TileFlag_HasSun)
    {
      // Cycle around the sun directions
      sunDirection = (sunDirection == 5) ? 0 : (sunDirection + 1);
    }
  }
}

void memclr(byte *ptr, byte len)
{
  while (len > 0)
  {
    *ptr = 0;
    ptr++;
    len--;
  }
}

void resetOurState()
{
  // Clear all tile flags
  tileFlags = 0;

  FOREACH_FACE(f)
  {
    FaceState *faceState = &faceStates[f];

    faceState->flags &= ~FaceFlag_WaterFull;
    faceState->waterLevel = 0;
    faceState->waterAdded = 0;
    faceState->waterStored = 0;
  }

  gatheredSun = 0;
  
  resetPlantState();
  
#if TRACK_FRAME_TIME
  worstFrameTime = 0;
#endif
}

void processCommForFace(Command command, byte value, byte f)
{
  FaceState *faceState = &faceStates[f];

  byte oppositeFace = OPPOSITE_FACE(f);

  switch (command)
  {
    case Command_SetWaterFull:
      if (value)
      {
        faceState->flags |= FaceFlag_NeighborWaterFull;
      }
      else
      {
        faceState->flags &= ~FaceFlag_NeighborWaterFull;
      }
      break;

    // Water received from a neighbor
    case Command_WaterAdd:
      faceState->waterAdded += value;
      break;

    case Command_GravityDir:
      if (!(tileFlags & TileFlag_HasDripper))    // dripper sends out the gravity - doesn't listen to it
      {
        if (sendGravityTimer.isExpired())
        {
          // Value is the CW offset from this face
          gravityUpFace = CW_FROM_FACE(f, value);
          
          // Propagate this on to our neighbors
          propagateGravityDir();

          // Delay before we accept a new value
          // To avoid infinite loops of neighbors sending to each other
          sendGravityTimer.set(CHECK_GRAVITY_RATE);
        }
      }
      break;

    case Command_DistEnergy:
      // Can only accept the energy if there is no plant growing here, or if
      // the energy is coming from the same root.
      // EASTER EGG: Dirt tiles normally start a plant and thus do not accept energy,
      //             but if another plant can attach to face 3 beneath the dirt tile
      //             then it can pass along excess energy. Probably not useful?
      if (!hasPlant || plantRootFace == f)
      {
        plantEnergy += value;
        plantRootFace = f;

        // Need to find out what kind of plant to grow - query the root
        if (!hasPlant)
        {
          hasPlant = true;
          plantStateNodeIndex = 6;
        }
      }
      break;

    case Command_SendSun:
      tileFlags |= TileFlag_PulseSun;

      // Plant leaves absorb sunlight
      // Send the rest in the same direction
      if (hasPlant)
      {
        // Leaves absorb the sun
//        byte sunGatheredByLeaves = MIN(value, plantNumLeaves);
//        gatheredSun = sunGatheredByLeaves;
//        value -= sunGatheredByLeaves;
        gatheredSun = plantNumLeaves;
        
        // Branches just block the sun
//        byte sunBlockedByBranches = MIN(value, plantNumBranches);
//        value -= sunBlockedByBranches;
      }
    
      if (tileFlags & TileFlag_HasDirt)
      {
        // Dirt blocks all sunlight
        value = 0;
      }
  
      // Any remaining sun gets sent on to the opposite face
      if (value > 0)
      {
        enqueueCommOnFace(oppositeFace, Command_SendSun, value);
      }
      break;
      
    case Command_GatherSun:
      // Plants propagate the sun until it gets to the root in the dirt
      if (hasPlant)
      {
        if (gatheredSun < MAX_GATHERED_SUN)
        {
          gatheredSun += value;
        }
      }
      break;

    case Command_TransferBug:
    case Command_TransferBugCW:
      if (!(tileFlags & TileFlag_HasBug) && !(tileFlags & TileFlag_HasDirt) && !(faceStates[f].flags & FaceFlag_WaterFull))
      {
        // No bug in this tile - transfer accepted
        tileFlags |= TileFlag_HasBug;
        bugTargetCorner = (command == Command_TransferBug) ? f : CW_FROM_FACE(f, 1);
        bugDistance  = 64;
        bugDirection = -1;  // start going towards the middle
        bugFlapOpen = 0;    // looks better starting closed
        
        // Notify the sender
        enqueueCommOnFace(f, Command_BugAccepted, value);
      }
      break;

    case Command_BugAccepted:
      // Bug transferred! Remove ours
      tileFlags &= ~TileFlag_HasBug;
      break;

    case Command_RainbowCheck:
      // Must have sun for a rainbow
      if (!sunLitTimer.isExpired())
      {
        byte incAmount = 1;
        if (waterFellTimer.isExpired() && value < 3)
        {
          incAmount = 0;
        }
        value += incAmount;
        if (value >= 5)
        {
          // Found enough for a rainbow!
          rainbowIndex = value - 1;
          rainbowDirection = f;
          rainbowTimer.set(RAINBOW_TIMEOUT);

          // Send a message back up the chain
          value -= 2;
          enqueueCommOnFace(f, Command_RainbowFound, value);
        }
        else
        {
          // Not enough for a rainbow yet
          // Pass on the new value
          enqueueCommOnFace(oppositeFace, Command_RainbowCheck, value);
        }
      }
      break;

    case Command_RainbowFound:
      // Show our part of the rainbow!
      rainbowIndex = value;
      rainbowTimer.set(RAINBOW_TIMEOUT);
      rainbowDirection = oppositeFace;

      // Propagate the message until it depletes
      if (value >= 1)
      {
        value--;
        enqueueCommOnFace(oppositeFace, Command_RainbowFound, value);
      }
      break;

    case Command_ChildBranchGrew:
      if (hasPlant)
      {
        plantChildBranchGrew = true;
        if (!(tileFlags & TileFlag_HasDirt))
        {
          enqueueCommOnFace(plantRootFace, Command_ChildBranchGrew, 0);
        }
      }
      break;
      
#if DEBUG_COMMS
    case Command_UpdateState:
      faceState->ourState = value;
      break;
#endif
  }
}

// =================================================================================================
//
// SYSTEMS
//
// =================================================================================================

int CW_FROM_FACE1(int f, int amt) { return ((f + amt) % FACE_COUNT); }

// -------------------------------------------------------------------------------------------------
// DRIPPER
// -------------------------------------------------------------------------------------------------

void loopDripper()
{
  if (!(tileFlags & TileFlag_HasDripper))
  {
    return;
  }

  if (!dripperTimer.isExpired())
  {
    return;
  }

  dripperTimer.set(DRIPPER_RATE >> dripperSpeedScale);

  // Add water to face 0, if not full
  if (faceStates[0].flags & FaceFlag_WaterFull)
  {
    return;
  }

  faceStates[0].waterLevel += DRIPPER_AMOUNT;
}

// -------------------------------------------------------------------------------------------------
// WATER
// -------------------------------------------------------------------------------------------------

struct WaterFlowCommand
{
  byte srcFace : 3;
  byte dstFace : 3;
  byte isNeighbor : 1;
  byte halfWater : 1;
};

WaterFlowCommand waterFlowSequence[] =
{
  { 3, 3, 1, 0 },   // fall from face 3 down out of this tile
  { 0, 3, 0, 0 },   // top row falls into bottom row
  { 1, 2, 0, 0 },   // ...
  { 5, 4, 0, 0 },   // ...

  // When flowing to sides, every other command has 'halfWater' set
  // so that the first will send half and then the next will send the
  // rest (the other half).
  // This has a side effect "bug" where, if there is a left neighbor,
  // but not a right, it will only send half the water. While if there
  // is a right neighbor but not a left it will send it all.
  // Hope no one notices shhhhh
  { 5, 0, 0, 1 },   // flow to sides
  { 5, 5, 1, 0 },
  { 4, 3, 0, 1 },
  { 4, 4, 1, 0 },
  { 0, 5, 0, 1 },
  { 0, 1, 0, 0 },
  { 3, 4, 0, 1 },
  { 3, 2, 0, 0 },
  { 1, 0, 0, 1 },
  { 1, 1, 1, 0 },
  { 2, 3, 0, 1 },
  { 2, 2, 1, 0 }
};

void loopWater()
{
  if (!gravityTimer.isExpired())
  {
    return;
  }

  for (int waterFlowIndex = 0; waterFlowIndex < 16; waterFlowIndex++)
  {
    WaterFlowCommand command = waterFlowSequence[waterFlowIndex];

    byte srcFace = command.srcFace;
    byte dstFace = command.dstFace;

    // Factor in gravity direction
    // Faces increase CW around the tile - need the CCW value to offset
    byte gravityCCWAmount = 6 - gravityUpFace;
    srcFace = CCW_FROM_FACE(srcFace, gravityCCWAmount);
    dstFace = CCW_FROM_FACE(dstFace, gravityCCWAmount);
    
    FaceState *faceStateSrc = &faceStates[srcFace];
    FaceState *faceStateDst = &faceStates[dstFace];

    byte amountToSend = MIN(faceStateSrc->waterLevel >> command.halfWater, WATER_FLOW_AMOUNT);
    if (amountToSend > 0)
    {
      if (command.isNeighbor)
      {
        if (faceStateSrc->flags & FaceFlag_NeighborPresent)
        {
          // Only send to neighbor if they haven't set their 'full' flag
          if (!(faceStateDst->flags & FaceFlag_NeighborWaterFull))
          {
            enqueueCommOnFace(dstFace, Command_WaterAdd, amountToSend);
            faceStateSrc->waterLevel -= amountToSend;
            if (faceStateSrc->waterLevel == 0)
            {
                waterFellTimer.set(WATER_FELL_TIMEOUT);
            }
          }
        }
      }
      else if ((faceStateDst->flags & FaceFlag_WaterFull) == 0)
      {
        faceStateDst->waterAdded += amountToSend;
        faceStateSrc->waterLevel -= amountToSend;
        if (faceStateSrc->waterLevel == 0)
        {
          waterFellTimer.set(WATER_FELL_TIMEOUT);
        }
      }
    }
  }
}

// -------------------------------------------------------------------------------------------------
// GRAVITY
// -------------------------------------------------------------------------------------------------

void loopGravity()
{
  // Only dripper tiles send out gravity - everyone else propagates it
  if (!(tileFlags & TileFlag_HasDripper))
  {
    return;
  }
  
  if (sendGravityTimer.isExpired())
  {
    // Tell neighbors how many faces away is "up".
    // Since tiles can be oriented any arbitrary direction, this 
    // information must be relative to the face that is sending it.
    propagateGravityDir();

    sendGravityTimer.set(SEND_GRAVITY_RATE);
  }
}

void propagateGravityDir()
{
  // Start sending from our version of "up" to make things easier to iterate
  byte f = gravityUpFace;
  char cwFromUp = 3;
  do
  {
    enqueueCommOnFace(f, Command_GravityDir, cwFromUp);

    cwFromUp -= (cwFromUp > 0) ? 1 : -5;

    f += (f < 5) ? 1 : -5;
    if (f == gravityUpFace)
    {
      break;  // done when we wrap around back to the start
    }
  } while(1);
}

// -------------------------------------------------------------------------------------------------
// DIRT
// -------------------------------------------------------------------------------------------------

void loopDirt()
{
  // Generate energy for plants!
  if (!plantEnergyTimer.isExpired())
  {
    return;
  }

  if (!(tileFlags & TileFlag_HasDirt))
  {
    return;
  }

  byte *reservoir = &faceStates[3].waterStored;
  if (*reservoir == 0)
  {
    return;
  }

  // Energy generation formula:
  // Up to X water = X energy (so that a plant can sprout without needing sunlight)
  // After that Y water + Y sun = ENERGY_PER_SUN*Y energy

  // X water
  byte energyToDistribute = (*reservoir > 5) ? 5 : *reservoir;
  *reservoir -= energyToDistribute;

  // Y sun + Y water
  byte minWaterOrSun = MIN(*reservoir, gatheredSun);
  *reservoir -= minWaterOrSun;
  gatheredSun = 0;
  energyToDistribute += minWaterOrSun * ENERGY_PER_SUN;

  // Don't generate any energy if the plant is submerged
  // Only need to check the three faces above ground
  if (faceStates[0].flags & FaceFlag_WaterFull &&
      faceStates[1].flags & FaceFlag_WaterFull &&
      faceStates[5].flags & FaceFlag_WaterFull)
  {
    energyToDistribute = 0;
  }

  plantEnergy = energyToDistribute;
}

// -------------------------------------------------------------------------------------------------
// PLANTS
// -------------------------------------------------------------------------------------------------

void loopPlant()
{
  // Plants use energy periodically to stay alive and grow

  if (!plantEnergyTimer.isExpired())
  {
    return;
  }

  if (!hasPlant)
  {
    if (plantEnergy == 0)
    {
      // No plant growing and no energy to try starting one
      return;
    }

    if (!(tileFlags & TileFlag_HasDirt))
    {
      // No dirt - can't start a plant
      return;
    }
    
    // Try to start growing a new plant
    // TODO : Different plant types!
    hasPlant = true;
    plantStateNodeIndex = 0;
    plantRootFace = 3;
  }

#if DEBUG_PLANT_ENERGY
  debugPlantEnergy = plantEnergy;
#endif

  // Handle gathered sunlight
  // Transmit it down to the root
  if (gatheredSun > 0)
  {
    enqueueCommOnFace(plantRootFace, Command_GatherSun, gatheredSun);
    gatheredSun = 0;
  }
  
  PlantStateNode *plantStateNode = &plantStateGraphTree[plantStateNodeIndex];

  // Use energy to maintain the current state
  byte energyMaintain = 1 + plantNumLeaves;
  if (plantEnergy < energyMaintain)
  {
    // Not enough energy - plant is dying
    // Wither plant
    if (plantStateNode->witherState > 0)
    {
      plantStateNodeIndex -= plantStateNode->witherState;
    }
    else
    {
      // Can't wither any further - just go away entirely
      resetPlantState();
    }
    return;
  }

  // Have enough energy to maintain - deduct it
  plantEnergy -= energyMaintain;
  
  // Is there enough extra energy to grow?
  if (plantStateNode->growState1 != 0)
  {
    if (plantEnergy >= plantStateNode->energyGrow)
    {
      // Some states wait for a child branch to grow
      if (!plantStateNode->waitForGrowth || plantChildBranchGrew)
      {
        // Some states choose between two next states
        byte stateOffset = plantStateNode->growState1;
        if (plantStateNode->growState2 != 0)
        {
          // Randomly choose between the two
          if (millis() & 0x1)
          {
            stateOffset += plantStateNode->growState2;
          }
        }
  
        // Deduct energy and grow!
        plantEnergy -= plantStateNode->energyGrow;
        plantStateNodeIndex += stateOffset;

        // Next state might need to wait for a child branch!
        plantChildBranchGrew = false;

        // Tell the root plant something grew - in case it is waiting on us
        enqueueCommOnFace(plantRootFace, Command_ChildBranchGrew, 0);
      }
    }
  }

  // Pass on any unused energy
  if (plantStateNode->exitFace != PlantExitFace_None)
  {
    byte energyToSend = (plantStateNode->exitFace == PlantExitFace_Fork) ? (plantEnergy >> 1) : plantEnergy;
    energyToSend = MIN(15, energyToSend);

    if (energyToSend > 0)
    {
      byte exitFace;
      switch (plantStateNode->exitFace)
      {
        case PlantExitFace_OneAcross:
          exitFace = CW_FROM_FACE(plantRootFace, 3);
          enqueueCommOnFace(exitFace, Command_DistEnergy, energyToSend);
          break;
          
        case PlantExitFace_Fork:
          exitFace = CW_FROM_FACE(plantRootFace, 2);
          enqueueCommOnFace(exitFace, Command_DistEnergy, energyToSend);
          byte exitFace = CW_FROM_FACE(plantRootFace, 4);
          enqueueCommOnFace(exitFace, Command_DistEnergy, energyToSend);
          break;
      }
    }
  }

  // Leftover energy is wasted
  plantEnergy = 0;
}

void resetPlantState()
{
  hasPlant = false;
  plantEnergy = 0;
  plantChildBranchGrew = false;

  // Allow a new bug to spawn
  tileFlags &= ~TileFlag_SpawnedBug;
}

// -------------------------------------------------------------------------------------------------
// SUN
// -------------------------------------------------------------------------------------------------

void loopSun()
{
  if (!(tileFlags & TileFlag_HasSun))
  {
    return;
  }
  
  if (!generateSunTimer.isExpired())
  {
    return;
  }
  
  // Send out sunlight in the appropriate direction
  enqueueCommOnFace(sunDirection, Command_SendSun, SUN_STRENGTH);
  tileFlags |= TileFlag_PulseSun;

  // Check for rainbows in certain directions of sunlight
  // Must be relative to gravity's up direction
  byte downLeft = CW_FROM_FACE(gravityUpFace, 4);
  byte downRight = CW_FROM_FACE(gravityUpFace, 2);
  if (sunDirection == downLeft || sunDirection == downRight)
  {
    enqueueCommOnFace(sunDirection, Command_RainbowCheck, 0);
  }

  generateSunTimer.set(GENERATE_SUN_RATE);
}

// -------------------------------------------------------------------------------------------------
// BUG
// -------------------------------------------------------------------------------------------------

void loopBug()
{
  if (!(tileFlags & TileFlag_HasBug))
  {
    return;
  }

/*
  // Submerged bugs die :(
  if (faceStates[0].flags & FaceFlag_WaterFull &&
      faceStates[1].flags & FaceFlag_WaterFull &&
      faceStates[2].flags & FaceFlag_WaterFull &&
      faceStates[3].flags & FaceFlag_WaterFull &&
      faceStates[4].flags & FaceFlag_WaterFull &&
      faceStates[5].flags & FaceFlag_WaterFull)
  {
    tileFlags &= ~TileFlag_HasBug;
    return;
  }
  */

  // Did the bug flap its wings?
  if (!bugFlapTimer.isExpired())
  {
    return;
  }
  bugFlapTimer.set(BUG_FLAP_RATE);
  
  bugFlapOpen = 1 - bugFlapOpen;

  // Move the bug along its path
  bugDistance += (bugDirection > 0) ? BUG_MOVE_RATE : -BUG_MOVE_RATE;
  if (bugDistance > 64)
  {
    // Start moving back towards the center
    bugDistance = 64;
    bugDirection = -1;
    
    // While doing this, try to transfer to neighbor cell
    // If the transfer is accepted then the bug will leave this tile
    byte otherFace = CCW_FROM_FACE(bugTargetCorner, 1);
    byte tryTransferToFace = 0;
    Command command = Command_None;

    if (faceStates[bugTargetCorner].flags & FaceFlag_NeighborPresent && !(faceStates[bugTargetCorner].flags & FaceFlag_NeighborWaterFull))
    {
      tryTransferToFace = bugTargetCorner;
      command = Command_TransferBugCW;
    }
    
    if (faceStates[otherFace].flags & FaceFlag_NeighborPresent && !(faceStates[otherFace].flags & FaceFlag_NeighborWaterFull))
    {
      // Choose the other face if the first face isn't present
      // If both faces are present, then do a coin flip
      // Using the LSB of the time *should* be random enough for a coin flip
      if (tryTransferToFace == 0 || millis() & 0x1)
      {
        tryTransferToFace = otherFace;
        command = Command_TransferBug;
      }
    }

    if (command != Command_None)
    {
      enqueueCommOnFace(tryTransferToFace, command, 0); // data is DC
    }
  }
  else if (bugDistance < 0)
  {
    bugDistance = 0;
    bugDirection = 1;
    // Pick a different corner (50% opposite, 25% opposite-left, 25% opposite-right)
    char offset = (millis() & 0x1) ? 3 : ((millis() & 0x2) ? 2 : 4);
    bugTargetCorner = CW_FROM_FACE(bugTargetCorner, offset);
  }
}

void trySpawnBug()
{
  // Each plant tile can only spawn one bug
  if (tileFlags & TileFlag_SpawnedBug)
  {
    return;
  }

  // Even if we don't end up spawning a bug - don't try again in this tile
  tileFlags |= TileFlag_SpawnedBug;

  // Check if we already have a bug
  if (tileFlags & TileFlag_HasBug)
  {
    return;
  }  

  // Only 1/8 chance to spawn a bug
  if (millis() & 0x7 != 0)
  {
    return;
  }
  
  tileFlags |= TileFlag_HasBug;
}

// -------------------------------------------------------------------------------------------------
// POST PROCESSING
// -------------------------------------------------------------------------------------------------

void postProcessState()
{
  accumulateWater();
  evaporateWater();

  // These timers are used by multiple systems
  // So need to reset here after all system processing
  if (gravityTimer.isExpired())
  {
    gravityTimer.set(GRAVITY_RATE);
  }
  if (plantEnergyTimer.isExpired())
  {
    plantEnergyTimer.set(PLANT_ENERGY_RATE);
  }
}

void accumulateWater()
{
  FOREACH_FACE(f)
  {
    FaceState *faceState = &faceStates[f];

    // Accumulate any water that flowed into us
    faceState->waterLevel += faceState->waterAdded;
    faceState->waterAdded = 0;

    // Dirt stores water
    if ((tileFlags & TileFlag_HasDirt) && (f >= 2) && (f <= 4))
    {
      if (faceState->waterStored < MAX_WATER_LEVEL)
      {
        byte waterToStore = MIN(faceState->waterLevel, MAX_WATER_LEVEL - faceState->waterStored);
        faceState->waterStored += waterToStore;
        faceState->waterLevel -= waterToStore;
      }
    }

    // Now check if we need to toggle our "full" flag
    // If our full state changed, notify the neighbor
    if (faceState->flags & FaceFlag_WaterFull)
    {
      if (faceState->waterLevel < MAX_WATER_LEVEL)
      {
        // Was full, but now it's not
        faceState->flags &= ~FaceFlag_WaterFull;

        // Tell neighbor we changed full state
        enqueueCommOnFace(f, Command_SetWaterFull, 0);
      }
    }
    else
    {
      if (faceState->waterLevel >= MAX_WATER_LEVEL)
      {
        // Wasn't full, but now it is
        faceState->flags |= FaceFlag_WaterFull;

        // Tell neighbor we changed full state
        enqueueCommOnFace(f, Command_SetWaterFull, 1);
      }
    }
  }
}

void evaporateWater()
{
  // Water evaporation/seepage
  if (!evaporationTimer.isExpired())
  {
    return;
  }
  evaporationTimer.set(EVAPORATION_RATE);
  
  byte amountToEvaporate = 1;
  FOREACH_FACE(f)
  {
    byte *waterToEvaporate = &faceStates[f].waterLevel;

    // Dirt faces evaporate from the stored amount
    if ((tileFlags & TileFlag_HasDirt) && (f >= 2) && (f <= 4))
    {
      waterToEvaporate = &faceStates[f].waterStored;
    }
    
    if (*waterToEvaporate > 0)
    {
      *waterToEvaporate -= amountToEvaporate;
    }
  }
}

// =================================================================================================
//
// RENDER
//
// =================================================================================================

void render()
{
  Color color;

  setColor(color);

  // Sun in the very background
  {
    if (tileFlags & TileFlag_PulseSun)
    {
      tileFlags &= ~TileFlag_PulseSun;
      sunPulseTimer.set(SUN_PULSE_RATE);
      sunLitTimer.set(SUN_LIT_RATE);
    }

    // Default to normal sun
    color = makeColorRGB(RGB_SUN_PULSE);

    // Rainbow overrides that
    if (!rainbowTimer.isExpired())
    {
      switch (rainbowIndex)
      {
        case 0: color = makeColorRGB(COLOR_RAINBOW_1); break;
        case 1: color = makeColorRGB(COLOR_RAINBOW_4); break;
        case 2: color = makeColorRGB(COLOR_RAINBOW_6); break;
        case 3: color = makeColorRGB(COLOR_RAINBOW_8); break;
        case 4: color = makeColorRGB(COLOR_RAINBOW_10); break;
      }
    }

    if (!sunPulseTimer.isExpired() || !rainbowTimer.isExpired())
    {
      setColor(color);
    }
  }

  // Water comes next
  color = makeColorRGB(RGB_WATER);
  FOREACH_FACE(f)
  {
    if (faceStates[f].waterLevel > 0)
    {
      setColorOnFace2(&color, f);
    }
  }

  // PLANTS
  if (hasPlant)
  {
    PlantStateNode *plantStateNode = &plantStateGraphTree[plantStateNodeIndex];
  
    int lutBits = plantStateNode->faceLUTIndexes;
    byte targetFace = plantRootFace;

    plantNumLeaves = 0;
    plantNumBranches = 0;
  
    for(uint8_t f = 0; f <= 4 ; ++f) // one short because face 5 isn't used
    {
      // Face 1 is also not used
      if (f != 1)
      {
        byte lutIndex = lutBits & 0x3;
        if (lutIndex != PlantFaceType_None)
        {
          switch (lutIndex)
          {
            case PlantFaceType_Leaf:   plantNumLeaves++;   color = sunLitTimer.isExpired() ? makeColorRGB(RGB_LEAF_OFF)   : makeColorRGB(RGB_LEAF_ON); break;
            case PlantFaceType_Branch: plantNumBranches++; color = sunLitTimer.isExpired() ? makeColorRGB(RGB_BRANCH_OFF) : makeColorRGB(RGB_BRANCH_ON); break;
            case PlantFaceType_Flower: plantNumLeaves++;   color = sunLitTimer.isExpired() ? makeColorRGB(RGB_FLOWER_OFF) : makeColorRGB(RGB_FLOWER_ON); trySpawnBug(); break;
          }
          setColorOnFace2(&color, targetFace);
        }
        lutBits >>= 2;
      }
      targetFace += (targetFace == 5) ? -5 : 1;
    }
  }

  // Bug in front of plant
  if (tileFlags & TileFlag_HasBug)
  {
    color = sunLitTimer.isExpired() ? makeColorRGB(RGB_BUG_OFF) : makeColorRGB(RGB_BUG_ON);
    setColorOnFace2(&color, CW_FROM_FACE(bugTargetCorner, bugFlapOpen ? 0 : 1));
    setColorOnFace2(&color, CW_FROM_FACE(bugTargetCorner, bugFlapOpen ? 5 : 4));
  }

  if (tileFlags & TileFlag_HasDripper)
  {
    // Dripper is always on face 0
    color = makeColorRGB(RGB_DRIPPER);
    setColorOnFace2(&color, 0);
  }
  
  if (tileFlags & TileFlag_HasSun)
  {
    byte targetFace = CW_FROM_FACE(sunDirection, 3);
    color = makeColorRGB(RGB_SUN);
    setColorOnFace2(&color, targetFace);
  }

  if (tileFlags & TileFlag_HasDirt)
  {
    color = makeColorRGB(RGB_DIRT);
    setColorOnFace2(&color, 2);
    setColorOnFace2(&color, 3);
    setColorOnFace2(&color, 4);
  }

#if DEBUG_COLORS
  // Debug to show gravity
  setColorOnFace(WHITE, gravityUpFace);
#endif

#if DEBUG_PLANT_ENERGY
  if (debugPlantEnergy & 0x1)
  {
    setColorOnFace(WHITE, 1);
  }
  if (debugPlantEnergy & 0x2)
  {
    setColorOnFace(WHITE, 2);
  }
  if (debugPlantEnergy & 0x4)
  {
    setColorOnFace(WHITE, 3);
  }
  if (debugPlantEnergy & 0x8)
  {
    setColorOnFace(WHITE, 4);
  }
  if (debugPlantEnergy & 0x10)
  {
    setColorOnFace(WHITE, 5);
  }
#endif

#if DETECT_COMM_ERRORS
  // Error codes
  FOREACH_FACE(f)
  {
    if (faceStates[f].flags & FaceFlag_Debug)
    {
      setColorOnFace(makeColorRGB(255,128,64), f);
    }

    if (ErrorOnFace(f))
    {
      setColorOnFace(makeColorRGB(255,0,0), f);
    }
  }
#endif

}
