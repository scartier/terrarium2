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
// * Remove sun/rainbow

#define null 0

// Defines to enable debug code
#define DEBUG_COMMS         0
#define DETECT_COMM_ERRORS  0
#define USE_DATA_SPONGE     0
#define TRACK_FRAME_TIME    0
//#define NO_STACK_WATCHER
#define DEBUG_PLANT_ENERGY  0
#define DEBUG_SPAWN_BUG     0

#define INCLUDE_FLORA       0
#define INCLUDE_FAUNA       0

#if TRACK_FRAME_TIME
unsigned long currentTime = 0;
byte frameTime;
byte worstFrameTime = 0;
#endif

#if USE_DATA_SPONGE
byte sponge[23];
// Aug 20: 45-49 bytes
// Aug 21: 50, 17 
// Aug 22: 10, 5
// Aug 23: 24
// Aug 26 flora: 55-59
// Aug 29 before plant state reduction: 22, (after) 48
// Aug 30 after making plant parameter table: 21, 23
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
#define RGB_DIRT_FERTILE 96,128,0

#define HUE_BUG               55    // HSB=55,255,255   RGB=179,255,0     HSB=55,255,192   RGB=134,191,0
#define RGB_BUG        179,255,0

#define HUE_WATER            171    // HSB=171,255,128  RGB=2,0,128
#define RGB_WATER        0,0,96

#define HUE_LEAF              85    // HSB=85,255,255   RGB=0,255,0       HSB=85,255,192   RGB=0,191,0

#define HUE_BRANCH            28    // HSB=28,255,160   RGB=161,107,0     HSB=28,255,192   RGB=191,128,0
#define RGB_BRANCH     191,128,0

#define HUE_FLOWER           233    // HSB=233,255,255  RGB=255,0,132     HSB=233,255,192  RGB=191,0,99
#define RGB_FLOWER   255,192,192

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
  Command_SetWaterFull,     // Tells neighbor we are full of water
  Command_WaterAdd,         // Adds water to this face
  Command_GravityDir,       // Propagates the gravity direction
  Command_DistEnergy,       // Distribute plant energy
  Command_TransferBug,      // Attempt to transfer a bug from one tile to another (must be confirmed)
  Command_TransferBugCW,    // Same as above, but transfer to the face CW from the receiver
  Command_BugAccepted,      // Confirmation of the transfer - sender must tolerate this never arriving, or arriving late
  Command_ChildBranchGrew,  // Sent down a plant to tell it that something grew
  Command_GatherSun,        // Plant leaves gather sun and send it down to the root
  Command_QueryPlantType,   // Ask the root what plant should grow
  Command_PlantType,        // Sending the plant type

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
  TileFlag_HasDripper    = 1<<0,
  TileFlag_HasDirt       = 1<<1,
  TileFlag_DirtIsFertile = 1<<2,
  TileFlag_Submerged     = 1<<3,
  TileFlag_SpawnedBug    = 1<<4,
  TileFlag_HasBug        = 1<<5,
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

#define PLANT_ENERGY_FROM_WATER 3
#define PLANT_ENERGY_PER_SUN    1

// -------------------------------------------------------------------------------------------------
// PLANT
//
enum PlantType
{
  PlantType_Tree,
  PlantType_Vine,
  PlantType_Seaweed,
  PlantType_Bush,
  
  PlantType_None = 99
};

enum PlantExitFace
{
  PlantExitFace_None,
  PlantExitFace_OneAcross,
  PlantExitFace_Fork,
  PlantExitFace_AcrossOffset,
};

struct PlantStateNode
{
  // Energy and state progression
  byte growState1         : 4;  // state offset when growing (1)
  byte growState2         : 2;  // state offset when growing (2) [same, but also added to growState1]
  byte witherState        : 3;  // state offset (backwards) when withering (0=dead)
  
  // Flags
  byte waitForGrowth      : 1;  // flag to only proceed once a child branch has grown

  // Growth into neighbors
  PlantExitFace exitFace  : 2;  // none, one across, fork

  // Render info
  byte faceRenderIndex    : 4;
};

enum PlantFaceType
{
  PlantFaceType_None,
  PlantFaceType_Leaf,
  PlantFaceType_Branch,
  PlantFaceType_Flower
};

byte plantRenderLUTIndexes[] = 
{//  4 3 2 0
  0b00000000,   //  0: EMPTY
  0b00000001,   //  1: BASE LEAF
  0b00000010,   //  2: BASE BRANCH
  0b00010001,   //  3: BASE LEAF + CENTER LEAF
  0b00010010,   //  4: BASE BRANCH + CENTER LEAF
  0b00100010,   //  5: BASE BRANCH + CENTER BRANCH
  0b01100110,   //  6: BASE BRANCH + CENTER BRANCH + SIDE LEAVES
  0b01000101,   //  7: BASE LEAF + FORK LEAVES
  0b01000110,   //  8: BASE BRANCH + FORK LEAVES
  0b10001010,   //  9: BASE BRANCH + FORK BRANCHES
  0b10011010,   // 10: BASE BRANCH + FORK BRANCHES + CENTER LEAF
  0b00110010,   // 11: BASE BRANCH + FLOWER
  0b01110110,   // 12: BASE BRANCH + FLOWER + SIDE LEAVES

  0b00000101,   // 13: BASE LEAF + LEFT LEAF
  0b01000001,   // 14: BASE LEAF + RIGHT LEAF
};

PlantStateNode plantStateGraphTree[] =
{
// BASE  
//  G1 G2 W   WFG  EXITS                      R
  { 1, 0, 0,  0,   PlantExitFace_None,        0 }, // START
  { 1, 0, 0,  0,   PlantExitFace_OneAcross,   4 }, // LEAF
  { 1, 0, 1,  1,   PlantExitFace_OneAcross,   4 }, // LEAF (WAIT FOR GROWTH)
  { 1, 0, 0,  0,   PlantExitFace_OneAcross,   5 }, // BRANCH
  { 0, 0, 1,  0,   PlantExitFace_OneAcross,   5 }, // BRANCH

// BRANCHES
//  G1 G2 W   WFG  EXITS                      R
  { 1, 0, 0,  0,   PlantExitFace_None,        0 }, // START
  { 2, 0, 0,  0,   PlantExitFace_None,        1 }, // BASE LEAF
  { 1, 0, 0,  0,   PlantExitFace_None,        2 }, // BASE BRANCH (WITHER STATE)

// FLOWER or BRANCH (1/8 chance of flower)
//  G1 G2 W   WFG  EXITS                      R
  { 3, 1, 1,  0,   PlantExitFace_None,        4 }, // BASE BRANCH + CENTER LEAF (CHOICE)
  { 2, 1, 2,  0,   PlantExitFace_None,        4 }, // BASE BRANCH + CENTER LEAF (CHOICE)
  { 1, 1, 3,  0,   PlantExitFace_None,        4 }, // BASE BRANCH + CENTER LEAF (CHOICE)

// CENTER BRANCH or FORK
//  G1 G2 W   WFG  EXITS                      R
  { 4, 4, 3,  0,   PlantExitFace_None,        4 }, // BASE BRANCH + CENTER LEAF (CHOICE)

// FLOWER
//  G1 G2 W   WFG  EXITS                      R
  { 1, 0, 5,  0,   PlantExitFace_None,        11 }, // BASE BRANCH + FLOWER
  { 0, 0, 1,  0,   PlantExitFace_None,        12 }, // BASE BRANCH + FLOWER + TWO LEAVES

// WITHER STATE
//  G1 G2 W   WFG  EXITS                      R
  { 1, 0, 0,  0,   PlantExitFace_None,        2 }, // BASE BRANCH (WITHER STATE)

// FORK BRANCH
//  G1 G2 W   WFG  EXITS                      R
  { 1, 0, 1,  1,   PlantExitFace_Fork,        8 }, // START - BASE BRANCH + TWO LEAVES (WAIT FOR GROWTH)
  { 1, 0, 2,  0,   PlantExitFace_Fork,        9 }, // BASE BRANCH + TWO BRANCHES
  { 0, 0, 1,  0,   PlantExitFace_Fork,        10 }, // BASE BRANCH + TWO BRANCHES + CENTER LEAF

// CENTER BRANCH
//  G1 G2 W   WFG  EXITS                      R
  { 1, 0, 4,  1,   PlantExitFace_OneAcross,   4 }, // START - BASE BRANCH + CENTER LEAF (WAIT FOR GROWTH)
  { 1, 0, 5,  0,   PlantExitFace_OneAcross,   5 }, // BASE BRANCH + CENTER BRANCH
  { 0, 0, 1,  0,   PlantExitFace_OneAcross,   6 }, // BASE BRANCH + CENTER BRANCH + TWO LEAVES
};

PlantStateNode plantStateGraphVine[] =
{
// BASE  
//  G1 G2 W   WFG  EXITS                      R
  { 1, 0, 0,  0,   PlantExitFace_None,        0 }, // START
  { 0, 0, 0,  0,   PlantExitFace_AcrossOffset,4 }, // LEAF

// BRANCHES
//  G1 G2 W   WFG  EXITS                      R
  { 1, 0, 0,  0,   PlantExitFace_None,        0 }, // START
  { 1, 0, 0,  0,   PlantExitFace_None,        1 }, // BASE LEAF
  { 0, 0, 1,  0,   PlantExitFace_AcrossOffset,3 }, // BASE LEAF + CENTER LEAF
};

PlantStateNode plantStateGraphSeaweed[] =
{
// BASE
//  G1 G2 W   WFG  EXITS                      R
  { 1, 0, 0,  0,   PlantExitFace_None,        0 }, // START
  { 0, 0, 0,  0,   PlantExitFace_AcrossOffset,4 }, // LEAF

// BRANCHES
//  G1 G2 W   WFG  EXITS                      R
  { 1, 0, 0,  0,   PlantExitFace_None,        0 }, // START
  { 1, 0, 0,  0,   PlantExitFace_None,        1 }, // BASE LEAF
  { 0, 0, 1,  0,   PlantExitFace_AcrossOffset,3 }, // BASE LEAF + CENTER LEAF
};

PlantStateNode plantStateGraphBush[] =
{
// BASE
//  G1 G2 W   WFG  EXITS                      R
  { 1, 0, 0,  0,   PlantExitFace_None,        0 }, // START
  { 0, 0, 0,  0,   PlantExitFace_Fork,        8 }, // FORK LEAVES

// BRANCHES
//  G1 G2 W   WFG  EXITS                      R
  { 1, 0, 0,  0,   PlantExitFace_None,        0 }, // START
  { 1, 1, 0,  0,   PlantExitFace_None,        1 }, // BASE LEAF (CHOICE)

// FORK BRANCH
//  G1 G2 W   WFG  EXITS                      R
  { 0, 0, 1,  0,   PlantExitFace_Fork,        7 }, // BASE LEAF + FORK LEAVES

// CENTER BRANCH
//  G1 G2 W   WFG  EXITS                      R
  { 1, 0, 2,  1,   PlantExitFace_OneAcross,   4 }, // START - BASE BRANCH + CENTER LEAF (WAIT FOR GROWTH)
  { 0, 0, 3,  0,   PlantExitFace_OneAcross,   5 }, // BASE BRANCH + CENTER BRANCH
//  { 0, 0, 1,  0,   PlantExitFace_OneAcross,   6 }, // BASE BRANCH + CENTER BRANCH + TWO LEAVES

};

struct PlantParams
{
  PlantStateNode *stateGraph;
  byte branchStartIndex   : 3;
  byte leafColorR         : 3;
  byte leafColorG         : 3;
  byte leafColorB         : 3;
  byte maintainCost       : 1;  // 0=1, 1=2
  byte growCost           : 1;  // 0=3, 1=?
  byte retainEnergy       : 1;
};

#define RGB_LEAF           0>>5, 255>>5,  0>>5
#define RGB_LEAF_VINE      0>>5, 192>>5, 32>>5
#define RGB_LEAF_SEAWEED 128>>5, 160>>5,  0>>5
#define RGB_LEAF_BUSH    192>>5,  32>>5,  0>>5

PlantParams plantParams[] =
{
  // State graph              Br  Leaf color         M   G   R
  {  plantStateGraphTree,     5,  RGB_LEAF,          0,  0,  1 },
  {  plantStateGraphVine,     2,  RGB_LEAF_VINE,     0,  0,  1 },
  {  plantStateGraphSeaweed,  2,  RGB_LEAF_SEAWEED,  0,  0,  1 },
  {  plantStateGraphBush,     2,  RGB_LEAF_BUSH,     1,  0,  0 },
};

// Timer that controls how often a plant must pay its maintenance cost or else wither.
// Should be slightly longer than the energy distribution rate so there's always time
// for energy to get there first.
#define PLANT_MAINTAIN_RATE (PLANT_ENERGY_RATE + 2000)
Timer plantMaintainTimer;

char plantType = PlantType_None;
byte plantStateNodeIndex;
byte plantRootFace;
byte plantExitFaceOffset;     // for plants that are gravity based
byte plantEnergy;
#if DEBUG_PLANT_ENERGY
byte debugPlantEnergy;
#endif

bool plantChildBranchGrew;    // flag that a child branch has grown somewhere in the plant

byte plantNumLeaves;
byte plantNumBranches;

// Sun
#define MAX_GATHERED_SUN 200
byte gatheredSun;

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
  loopDirt();
  loopPlantMaintain();
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
        trySpawnBug();
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
    else if (tileFlags & TileFlag_HasDirt)
    {
      tileFlags ^= TileFlag_DirtIsFertile;
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
      if ((plantType == PlantType_None) || plantRootFace == f)
      {
        plantEnergy += value;
        plantRootFace = f;

        // Need to find out what kind of plant to grow - query the root
        if (plantType == PlantType_None)
        {
          enqueueCommOnFace(plantRootFace, Command_QueryPlantType, 0);
        }
        else
        {
          // Just received some energy - check if plant can grow
          loopPlantGrow();
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

    case Command_ChildBranchGrew:
      if (plantType != PlantType_None)
      {
        plantChildBranchGrew = true;
        if (!(tileFlags & TileFlag_HasDirt))
        {
          enqueueCommOnFace(plantRootFace, Command_ChildBranchGrew, 0);
        }
      }
      break;

    case Command_GatherSun:
      if (gatheredSun < MAX_GATHERED_SUN)
      {
        gatheredSun += value;
      }
      break;

    case Command_QueryPlantType:
      if (plantType != PlantType_None)
      {
        // Send the plant type back
        enqueueCommOnFace(f, Command_PlantType, plantType);
      }
      break;

    case Command_PlantType:
      if (plantType == PlantType_None)
      {
        plantType = value;
        plantStateNodeIndex = plantParams[plantType].branchStartIndex;
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
          }
        }
      }
      else if ((faceStateDst->flags & FaceFlag_WaterFull) == 0)
      {
        faceStateDst->waterAdded += amountToSend;
        faceStateSrc->waterLevel -= amountToSend;
      }
    }
  }

  // Determine if the tile is fully submerged (water in every face)
  tileFlags &= ~TileFlag_Submerged;
  if ((faceStates[0].waterLevel > 0) &&
      (faceStates[1].waterLevel > 0) &&
      (faceStates[2].waterLevel > 0) &&
      (faceStates[3].waterLevel > 0) &&
      (faceStates[4].waterLevel > 0) &&
      (faceStates[5].waterLevel > 0))
  {
    tileFlags |= TileFlag_Submerged;
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
  plantEnergyTimer.set(PLANT_ENERGY_RATE);

  if (!(tileFlags & TileFlag_HasDirt))
  {
    return;
  }

  // Dirt must be set to fertile for it to grow plants
  if (!(tileFlags & TileFlag_DirtIsFertile))
  {
    return;
  }

  // Suck water from the side dirt tiles to the center
  if (faceStates[3].waterStored < MAX_WATER_LEVEL)
  {
    byte waterTransferred = MIN(faceStates[2].waterStored, MAX_WATER_LEVEL - faceStates[3].waterStored);
    faceStates[2].waterStored -= waterTransferred;
    faceStates[3].waterStored += waterTransferred;
    waterTransferred = MIN(faceStates[4].waterStored, MAX_WATER_LEVEL - faceStates[3].waterStored);
    faceStates[4].waterStored -= waterTransferred;
    faceStates[3].waterStored += waterTransferred;
  }
  
  byte *reservoir = &faceStates[3].waterStored;
  if (*reservoir == 0)
  {
    return;
  }

  // Energy generation formula:
  // Up to X water = X energy (so that a plant can sprout without needing sunlight)
  // After that Y water + Y sun = PLANT_ENERGY_PER_SUN*Y energy

  // X water
  byte energyToDistribute = (*reservoir > PLANT_ENERGY_FROM_WATER) ? PLANT_ENERGY_FROM_WATER : *reservoir;
  *reservoir -= energyToDistribute;

  // Y sun + Y water
  byte minWaterOrSun = MIN(*reservoir, gatheredSun);
  *reservoir -= minWaterOrSun;
  gatheredSun = 0;
  energyToDistribute += minWaterOrSun * PLANT_ENERGY_PER_SUN;

  plantEnergy += energyToDistribute;
  loopPlantGrow();
}

// -------------------------------------------------------------------------------------------------
// PLANTS
// -------------------------------------------------------------------------------------------------

void loopPlantMaintain()
{
  // Plants use energy periodically to stay alive and grow

  if (!plantMaintainTimer.isExpired())
  {
    return;
  }
  plantMaintainTimer.set(PLANT_MAINTAIN_RATE);

  if (plantType == PlantType_None)
  {
    return;
  }

  PlantStateNode *plantStateNode = &plantParams[plantType].stateGraph[plantStateNodeIndex];

  // For gravity-based plants, rotate towards the desired direction (up or down)
  char rootRelativeToGravity = plantRootFace - gravityUpFace;
  if (rootRelativeToGravity < 0)
  {
    rootRelativeToGravity += 6;
  }
  plantExitFaceOffset = 0;
  if (plantType == PlantType_Vine)
  {
    switch (rootRelativeToGravity)
    {
      case 0:                          break;   // pointing directly down - all's good
      case 1: 
      case 2: plantExitFaceOffset = 5; break;   // rotate CCW by one face
      case 3: plantEnergy = 0;         break;   // pointing directly up - cannnot grow
      case 4:
      case 5: plantExitFaceOffset = 1; break;   // rotate CW by one face
    }
  }
  else if (plantType == PlantType_Seaweed)
  {
    switch (rootRelativeToGravity)
    {
      case 0: plantEnergy = 0;         break;   // pointing directly down - cannnot grow
      case 1: 
      case 2: plantExitFaceOffset = 1; break;   // rotate CCW by one face
      case 3:                          break;   // pointing directly up - all's good
      case 4:
      case 5: plantExitFaceOffset = 5; break;   // rotate CW by one face
    }
  }

  // Only seaweed can live under water
  bool plantIsSubmerged = tileFlags & TileFlag_Submerged;
  bool plantIsAquatic = plantType == PlantType_Seaweed;
  if (plantIsSubmerged != plantIsAquatic)
  {
    plantEnergy = 0;
  }
  
  // Use energy to maintain the current state
  if (plantEnergy < (plantParams[plantType].maintainCost + 1))
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
  plantEnergy -= plantParams[plantType].maintainCost + 1;
}

void loopPlantGrow()
{
  // Plants use energy to stay alive and grow
  // This function is triggered when this tile receives plant energy from the root.
  // For dirt tiles, this is from loopDirt().
  // Otherwise it is when this tile receives a DistEnergy message from the root face.

  // If we don't already have a plant, try to grow one
  if (plantType == PlantType_None)
  {
    if (plantEnergy == 0)
    {
      // No plant growing and no energy to try starting one
      return;
    }

    if (!(tileFlags & TileFlag_HasDirt))
    {
      // No dirt or not fertile - can't start a plant
      return;
    }
    
    // Try to start growing a new plant
    // Determine plant type by
    //   (1) tile orientation relative to gravity
    //   (2) being underwater
    if (gravityUpFace == 5 || gravityUpFace == 0 || gravityUpFace == 1)
    {
      // If submerged, grow seaweed
      if (tileFlags & TileFlag_Submerged)
      {
        plantType = PlantType_Seaweed;
      }
      else if (gravityUpFace == 0)
      {
        plantType = PlantType_Tree;
      }
      else
      {
        //plantType = PlantType_Bush;
      }
    }
    else
    {
      plantType = PlantType_Vine;
    }

    if (plantType == PlantType_None)
    {
      // Didn't find a plant type to grow - bail
      return;
    }

    plantStateNodeIndex = 0;
    plantRootFace = 3;
  }

  // First thing, send gathered sun down to the root
  gatheredSun += plantNumLeaves;
  if (gatheredSun > 0)
  {
    enqueueCommOnFace(plantRootFace, Command_GatherSun, gatheredSun);
  }

#if DEBUG_PLANT_ENERGY
  debugPlantEnergy = plantEnergy;
#endif

  PlantStateNode *plantStateNode = &plantParams[plantType].stateGraph[plantStateNodeIndex];

  // Use energy to maintain the current state
  byte energyMaintain = 1;
  if (plantEnergy <= energyMaintain)
  {
    // Not enough energy to do anything
    return;
  }
  
  byte energyForGrowth = plantEnergy - energyMaintain;
  plantEnergy -= energyForGrowth;

  // Can we grow?
  if (plantStateNode->growState1 != 0)
  {
    // Is there enough energy to grow?
    if (energyForGrowth >= (plantParams[plantType].growCost + 3))
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
        energyForGrowth -= plantParams[plantType].growCost + 3;
        plantStateNodeIndex += stateOffset;

        // Next state might need to wait for a child branch!
        plantChildBranchGrew = false;

        // Tell the root plant something grew - in case it is waiting on us
        enqueueCommOnFace(plantRootFace, Command_ChildBranchGrew, 0);
      }
    }
    else
    {
      // Didn't grow so hold on to all our energy so maybe we can next time
      if (plantParams[plantType].retainEnergy)
      {
        plantEnergy += energyForGrowth;
      }
      return;
    }
  }

  // Pass on any unused energy
  if (plantStateNode->exitFace != PlantExitFace_None)
  {
    byte energyToSend = (plantStateNode->exitFace == PlantExitFace_Fork) ? (energyForGrowth >> 1) : energyForGrowth;
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

          // Adjust in case of rounding
          // This means the right fork will always get more energy than the left
          energyToSend = MIN(15, energyForGrowth - energyToSend);
          
          exitFace = CW_FROM_FACE(plantRootFace, 4);
          enqueueCommOnFace(exitFace, Command_DistEnergy, energyToSend);
          break;

        case PlantExitFace_AcrossOffset:
          exitFace = CW_FROM_FACE(plantRootFace, 3 + plantExitFaceOffset);
          enqueueCommOnFace(exitFace, Command_DistEnergy, energyToSend);
          break;
      }
    }
  }
}

void resetPlantState()
{
  plantType = PlantType_None;
  plantEnergy = 0;
  plantChildBranchGrew = false;
  gatheredSun = 0;
#if DEBUG_PLANT_ENERGY
  debugPlantEnergy = 0;
#endif

  // Allow a new bug to spawn
  tileFlags &= ~TileFlag_SpawnedBug;
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

  // Timers here are used by multiple systems
  // So need to reset here after all system processing
  if (gravityTimer.isExpired())
  {
    gravityTimer.set(GRAVITY_RATE);
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

  // WATER
  color = makeColorRGB(RGB_WATER);
  FOREACH_FACE(f)
  {
    if (faceStates[f].waterLevel > 0)
    {
      setColorOnFace2(&color, f);
    }
  }

  if (tileFlags & TileFlag_HasDirt)
  {
    color = makeColorRGB(RGB_DIRT);
    setColorOnFace2(&color, 2);
    setColorOnFace2(&color, 4);
    if (tileFlags & TileFlag_DirtIsFertile)
    {
      color = makeColorRGB(RGB_DIRT_FERTILE);
    }
    setColorOnFace2(&color, 3);
  }

  // PLANTS
  if (plantType != PlantType_None)
  {
    PlantStateNode *plantStateNode = &plantParams[plantType].stateGraph[plantStateNodeIndex];
  
    int lutBits = plantRenderLUTIndexes[plantStateNode->faceRenderIndex];
    byte targetFace = plantRootFace;

    plantNumLeaves = 0;
    plantNumBranches = 0;
  
    for(uint8_t f = 0; f <= 4 ; ++f) // one short because face 5 isn't used
    {
      // Face 1 is also not used
      if (f == 1)
      {
        // Take this opportunity to adjust the target face based on potential gravity influence
        targetFace = CW_FROM_FACE(targetFace, plantExitFaceOffset + 1);
        continue;
      }
      
      byte lutIndex = lutBits & 0x3;
      if (lutIndex != PlantFaceType_None)
      {
        switch (lutIndex)
        {
          case PlantFaceType_Leaf:   plantNumLeaves++;   color = makeColorRGB(plantParams[plantType].leafColorR<<5, plantParams[plantType].leafColorG<<5, plantParams[plantType].leafColorB<<5);   break;
          case PlantFaceType_Branch: plantNumBranches++; color = makeColorRGB(RGB_BRANCH); break;
          case PlantFaceType_Flower: plantNumLeaves++;   color = makeColorRGB(RGB_FLOWER); trySpawnBug(); break;
        }
        setColorOnFace2(&color, targetFace);
      }
      lutBits >>= 2;
      targetFace = CW_FROM_FACE(targetFace, 1);
    }
  }

  // BUG
  if (tileFlags & TileFlag_HasBug)
  {
    color = makeColorRGB(RGB_BUG);
    setColorOnFace2(&color, CW_FROM_FACE(bugTargetCorner, bugFlapOpen ? 0 : 1));
    setColorOnFace2(&color, CW_FROM_FACE(bugTargetCorner, bugFlapOpen ? 5 : 4));
  }

  if (tileFlags & TileFlag_HasDripper)
  {
    // Dripper is always on face 0
    color = makeColorRGB(RGB_DRIPPER);
    setColorOnFace2(&color, 0);
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

  // Debug output overrides eveeeerything
  FOREACH_FACE(f)
  {
    if (faceStates[f].flags & FaceFlag_Debug)
    {
      setColorOnFace(makeColorRGB(255,128,64), f);
    }
  }
}
