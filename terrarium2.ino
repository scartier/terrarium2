// TERRARIUM
// (c) 2020 Scott Cartier

// TODO
// * Make bug/fish/crawly die as necessary
// * Proper random function

#define null 0
#define DC 0      // don't care value

// Defines to enable debug code
#define DEBUG_COMMS         0
#define DETECT_COMM_ERRORS  0
#define USE_DATA_SPONGE     0
#define TRACK_FRAME_TIME    0
//#define NO_STACK_WATCHER
#define DEBUG_PLANT_ENERGY  0
#define DEBUG_SPAWN_BUG     0
#define DEBUG_SPAWN_FISH    0
#define DEBUG_SPAWN_CRAWLY  0
#define DIM_COLORS          0

#define OPTIMIZE
#define OLD_COMM_QUEUE    // testing new comm method

// Stuff to disable to get more code space
#define DISABLE_CHILD_BRANCH_GREW     // disabling save 60 bytes
#define ENABLE_DIRT_RESERVOIR         // enabling saves 52 bytes

#ifndef BGA_CUSTOM_BLINKLIB
#error "This code requires BGA's Custom Blinklib"
#endif

#if TRACK_FRAME_TIME
unsigned long currentTime = 0;
byte frameTime;
byte worstFrameTime = 0;
#endif

#if USE_DATA_SPONGE
#warning DATA SPONGE ENABLED
byte sponge[81];
// Aug 26 flora: 55-59
// Aug 29 before plant state reduction: 22, (after) 48
// Aug 30 after making plant parameter table: 21, 23
// Sep 2: after adding branch stages: 18
// Sep 7: Testing Bruno's blinklib: 2 (before), 14 (after)
// Sep 8: Remove empty start states for each plant branch stage: 37
// Sep 8: Switch to data centric CW/CCW/OPPOSITE macros: 25
// Sep 10: 19(??)
// Sep 10: Latest BGA blinklib: 238?
// Sep 13: Code optimizations (at expense of data): 132
// Sep 15: Code optimizations (at expense of data): 46
// Sep 16: Went back to packing water table data: 81
#endif

#define MAX(x,y) ((x) > (y) ? (x) : (y))
#define MIN(x,y) ((x) < (y) ? (x) : (y))

byte faceOffsetArray[] = { 0, 1, 2, 3, 4, 5, 0, 1, 2, 3, 4, 5 };
unsigned long faceOffsetArrayLong[] = { 0x10543210, 0x21054321, 0x32105432, 0x43210543, 0x54321054, 0x05432105 };

#define CCW_FROM_FACE CCW_FROM_FACE2
#define CCW_FROM_FACE1(f, amt) (((f) - (amt)) + (((f) >= (amt)) ? 0 : 6))
#define CCW_FROM_FACE2(f, amt) faceOffsetArray[6 + (f) - (amt)]

#define CW_FROM_FACE CW_FROM_FACE2
#define CW_FROM_FACE1(f, amt) (((f) + (amt)) % FACE_COUNT)
#define CW_FROM_FACE2(f, amt) faceOffsetArray[(f) + (amt)]
#define CW_FROM_FACE3(f, amt) ((faceOffsetArrayLong[f] >> (amt << 2)) & 0xF)

#define OPPOSITE_FACE OPPOSITE_FACE2
#define OPPOSITE_FACE1(f) (((f) < 3) ? ((f) + 3) : ((f) - 3))
#define OPPOSITE_FACE2(f) CW_FROM_FACE((f), 3)

#define SET_COLOR_ON_FACE(c,f) setColorOnFace(c, f)
//#define SET_COLOR_ON_FACE(c,f) setColorOnFace2(&c, f)
//#define SET_COLOR_ON_FACE(c,f) blinkbios_pixel_block.pixelBuffer[f].as_uint16 = c.as_uint16
//#define SET_COLOR_ON_FACE(c,f) faceColors[f] = c
//#define SET_COLOR_ON_FACE(c,f) setColorOnFace3(c.as_uint16, f)

//byte randVal = 0xCC;
#define RANDOM_BYTE() RANDOM_BYTE3
#define RANDOM_BYTE1 millis()
#define RANDOM_BYTE2 randVal
#define RANDOM_BYTE3 millis8()

// =================================================================================================
//
// COLORS
//
// =================================================================================================

#define RGB_TO_U16_WITH_DIM(r,g,b) ((((uint16_t)r>>3>>DIM_COLORS) & 0x1F)<<1 | (((uint16_t)g>>3>>DIM_COLORS) & 0x1F)<<6 | (((uint16_t)b>>3>>DIM_COLORS) & 0x1F)<<11)

#define RGB_DRIPPER       0>>DIM_COLORS,255>>DIM_COLORS,128>>DIM_COLORS
#define RGB_DRIPPER_R     0
#define RGB_DRIPPER_G     255
#define RGB_DRIPPER_B     128

#define RGB_DIRT          128>>DIM_COLORS,96>>DIM_COLORS,0
#define RGB_DIRT_R        128
#define RGB_DIRT_G        96
#define RGB_DIRT_B        0

#define RGB_DIRT_FERTILE  96>>DIM_COLORS,128>>DIM_COLORS,0
#define RGB_FERTILE_R     96
#define RGB_FERTILE_G     128
#define RGB_FERTILE_B     0

#define RGB_BUG           179>>DIM_COLORS,255>>DIM_COLORS,0
#define RGB_BUG_R         179
#define RGB_BUG_G         255
#define RGB_BUG_B         0

#define RGB_WATER         0,0,96>>DIM_COLORS
#define RGB_WATER_R       0
#define RGB_WATER_G       0
#define RGB_WATER_B       96

#define RGB_BRANCH        191>>DIM_COLORS,128>>DIM_COLORS,0
#define RGB_BRANCH_R      191
#define RGB_BRANCH_G      128
#define RGB_BRANCH_B      0

#define RGB_FLOWER        255>>DIM_COLORS,192>>DIM_COLORS,192>>DIM_COLORS
#define RGB_FLOWER_R      255
#define RGB_FLOWER_G      192
#define RGB_FLOWER_B      192

#define RGB_FLOWER_USED   255>>DIM_COLORS,128>>DIM_COLORS,128>>DIM_COLORS
#define RGB_FLOWER_USED_R 255
#define RGB_FLOWER_USED_G 128
#define RGB_FLOWER_USED_B 128

#define RGB_CRAWLY 64>>DIM_COLORS,255>>DIM_COLORS,64>>DIM_COLORS
#define RGB_CRAWLY_R      64
#define RGB_CRAWLY_G      255
#define RGB_CRAWLY_B      64

#define U16_DRIPPER       RGB_TO_U16_WITH_DIM(RGB_DRIPPER_R,      RGB_DRIPPER_G,      RGB_DRIPPER_B     )
#define U16_CRAWLY        RGB_TO_U16_WITH_DIM(RGB_CRAWLY_R,       RGB_CRAWLY_G,       RGB_CRAWLY_B      )
#define U16_BUG           RGB_TO_U16_WITH_DIM(RGB_BUG_R,          RGB_BUG_G,          RGB_BUG_B         )
#define U16_WATER         RGB_TO_U16_WITH_DIM(RGB_WATER_R,        RGB_WATER_G,        RGB_WATER_B       )
#define U16_DIRT          RGB_TO_U16_WITH_DIM(RGB_DIRT_R,         RGB_DIRT_G,         RGB_DIRT_B        )
#define U16_FERTILE       RGB_TO_U16_WITH_DIM(RGB_FERTILE_R,      RGB_FERTILE_G,      RGB_FERTILE_B     )
#define U16_BRANCH        RGB_TO_U16_WITH_DIM(RGB_BRANCH_R,       RGB_BRANCH_G,       RGB_BRANCH_B      )
#define U16_LEAF          0x89AB
#define U16_FLOWER        RGB_TO_U16_WITH_DIM(RGB_FLOWER_R,       RGB_FLOWER_G,       RGB_FLOWER_B      )
#define U16_FLOWER_USED   RGB_TO_U16_WITH_DIM(RGB_FLOWER_USED_R,  RGB_FLOWER_USED_G,  RGB_FLOWER_USED_B )
#define U16_FISH          0xABCD

#define U16_DEBUG         RGB_TO_U16_WITH_DIM(255, 128, 64)

struct ColorByte
{
  byte r : 3;
  byte g : 3;
  byte b : 2;
};

// =================================================================================================
//
// COMMUNICATIONS
//
// =================================================================================================

#define TOGGLE_COMMAND 1
#define TOGGLE_DATA 0
struct FaceValue
{
  byte value  : 6;
  byte toggle : 1;
  byte ack    : 1;
};

enum Command
{
  Command_None,
  Command_SetWaterFull,     // Tells neighbor we are full of water
  Command_AddWater,         // Adds water to this face
  Command_GravityDir,       // Propagates the gravity direction
  Command_DistEnergy,       // Distribute plant energy
  Command_TransferBug,      // Attempt to transfer a bug from one tile to another (must be confirmed)
  Command_Accepted,         // Confirmation of the transfer - sender must tolerate this never arriving, or arriving late
#ifndef DISABLE_CHILD_BRANCH_GREW
  Command_ChildBranchGrew,  // Sent down a plant to tell it that something grew
#endif
  Command_GatherSun,        // Plant leaves gather sun and send it down to the root
  Command_QueryPlantType,   // Ask the root what plant should grow
  Command_PlantType,        // Sending the plant type
  Command_TransferFish,     // Attempt to transfer a fish from one tile to another (must be confirmed)
  Command_TransferCrawly,   // Attempt to transfer a crawly from one tile to another (must be confirmed)
  Command_FishTail,         // Turn on the fish tail because the tile above us has a fish at the bottom

#if DEBUG_COMMS
  Command_UpdateState,
#endif
  
  Command_MAX
};

struct CommandAndData
{
#ifndef OLD_COMM_QUEUE
  byte face;
#endif
  Command command : 8;
  byte data : 8;
};

#ifdef OLD_COMM_QUEUE
#define COMM_QUEUE_SIZE 4
CommandAndData commQueues[FACE_COUNT][COMM_QUEUE_SIZE];
byte commInsertionIndexes[FACE_COUNT];
#else
#define COMM_QUEUE_SIZE 12
CommandAndData commQueue[COMM_QUEUE_SIZE];
byte commInsertionIndex;
#endif

#if DETECT_COMM_ERRORS
#define COMM_INDEX_ERROR_OVERRUN 0xFF
#define COMM_INDEX_OUT_OF_SYNC   0xFE
#define COMM_DATA_OVERRUN        0xFD
#define ErrorOnFace(f) (commInsertionIndexes[f] > COMM_QUEUE_SIZE)
#endif

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
  Command lastCommandIn;
  byte flags;

  // ---------------------------
  // Application-specific fields
  byte waterLevel;
  byte waterAdded;
#ifndef ENABLE_DIRT_RESERVOIR
  byte waterStored;   // dirt tiles hold on to water
#endif
  // ---------------------------

#if DEBUG_COMMS
  byte ourState;
  byte neighborState;
#endif
};
FaceState faceStates[FACE_COUNT];

byte numNeighbors;

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
  TileFlag_HasDripper     = 1<<0,
  TileFlag_HasDirt        = 1<<1,
  TileFlag_DirtIsFertile  = 1<<2,
  TileFlag_Submerged      = 1<<3,
  TileFlag_SpawnedCritter = 1<<4,
  TileFlag_HasBug         = 1<<5,
  TileFlag_HasFish        = 1<<6,
  TileFlag_HasCrawly      = 1<<7,

  //TileFlag_ShowPlantEnergy = 1<<7,
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

#define DEFAULT_GRAVITY_UP_FACE 0
byte gravityUpFace = DEFAULT_GRAVITY_UP_FACE;
byte *gravityRelativeFace = &faceOffsetArray[DEFAULT_GRAVITY_UP_FACE];

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

#ifdef ENABLE_DIRT_RESERVOIR
byte reservoir;
#endif

// -------------------------------------------------------------------------------------------------
// PLANT
//
enum PlantType
{
  PlantType_Tree,
  PlantType_Vine,
  PlantType_Seaweed,
  PlantType_Dangle,

  PlantType_MAX,
  
  PlantType_None = 7
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
  byte growState1         : 2;  // state offset when growing (1)
  byte growState2         : 2;  // state offset when growing (2) [same, but also added to growState1]
  
  // Flags
  byte isWitherState      : 1;  // state plant will jump back to when withering
  byte waitForGrowth      : 1;  // flag to only proceed once a child branch has grown
  byte advanceStage       : 1;  // plant advances to next stage in child
  byte UNUSED             : 1;  // BUFFER TO BYTE ALIGN THE NEXT FIELD

  // Growth into neighbors
  PlantExitFace exitFace  : 2;  // none, one across, fork, gravity-based

  // Render info
  byte faceRenderIndex    : 5;
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

  0b00010000,   // 13: CENTER LEAF
  0b00100000,   // 14: CENTER BRANCH
  
  0b00110001,   // 15: BASE LEAF + CENTER FLOWER
  0b01110101,   // 16: BASE LEAF + CENTER FLOWER + SIDE LEAVES

  0b01010001,   // 17: BASE LEAF + CENTER LEAF + LEFT LEAF (for debug mostly)
  0b00010101,   // 18: BASE LEAF + CENTER LEAF + RIGHT LEAF (for debug mostly)

  0b11010001,   // 19: BASE LEAF + CENTER LEAF + LEFT FLOWER
  0b00011101,   // 20: BASE LEAF + CENTER LEAF + RIGHT FLOWER
};

PlantStateNode plantStateGraphTree[] =
{
// BASE  
//  G1 G2 W   WFG  AS,  UN,  EXITS                       R
  { 1, 0, 1,  1,   1,   DC,  PlantExitFace_OneAcross,    13 }, // CENTER LEAF (WAIT FOR GROWTH)
  { 0, 0, 0,  0,   1,   DC,  PlantExitFace_OneAcross,    14 }, // CENTER BRANCH

// STAGE 1 (TRUNK)
//  G1 G2 W   WFG  AS,  UN,  EXITS                       R
  { 1, 1, 1,  0,   0,   DC,  PlantExitFace_None,         1 }, // BASE LEAF (WITHER STATE) (CHOICE)
  { 2, 0, 0,  1,   0,   DC,  PlantExitFace_OneAcross,    4 }, // BASE BRANCH + CENTER LEAF (WAIT FOR GROWTH)
  { 1, 0, 0,  1,   1,   DC,  PlantExitFace_OneAcross,    4 }, // BASE BRANCH + CENTER LEAF (WAIT FOR GROWTH) (ADVANCE STAGE)
  { 1, 0, 1,  0,   0,   DC,  PlantExitFace_OneAcross,    5 }, // BASE BRANCH + CENTER BRANCH
  { 0, 0, 0,  0,   0,   DC,  PlantExitFace_OneAcross,    6 }, // BASE BRANCH + CENTER BRANCH + SIDE LEAVES

// STAGE 2 (BRANCHES)
//  G1 G2 W   WFG  AS,  UN,  EXITS                       R
  { 1, 1, 1,  0,   0,   DC,  PlantExitFace_None,         1 }, // BASE LEAF (WITHER STATE) (CHOICE)
  { 2, 0, 0,  1,   0,   DC,  PlantExitFace_Fork,         8 }, // BASE BRANCH + TWO LEAVES (WAIT FOR GROWTH)
  { 1, 0, 0,  1,   1,   DC,  PlantExitFace_Fork,         8 }, // BASE BRANCH + TWO LEAVES (WAIT FOR GROWTH) (ADVANCE STAGE)
  { 1, 0, 1,  0,   0,   DC,  PlantExitFace_Fork,         9 }, // BASE BRANCH + TWO BRANCHES
  { 0, 0, 0,  0,   0,   DC,  PlantExitFace_Fork,         10 }, // BASE BRANCH + TWO BRANCHES + CENTER LEAF

// STAGE 3 (FLOWERS)
//  G1 G2 W   WFG  AS,  UN,  EXITS                       R
  { 1, 0, 1,  0,   0,   DC,  PlantExitFace_None,         1 }, // BASE LEAF (WITHER STATE)
  { 1, 0, 1,  0,   0,   DC,  PlantExitFace_None,         11 }, // BASE BRANCH + FLOWER (WITHER STATE)
  { 0, 0, 0,  0,   0,   DC,  PlantExitFace_None,         12 }, // BASE BRANCH + FLOWER + TWO LEAVES
};

PlantStateNode plantStateGraphVine[] =
{
// BASE  
//  G1 G2 W   WFG  AS,  UN,  EXITS                       R
  { 0, 0, 0,  0,   1,   DC,  PlantExitFace_AcrossOffset, 13 }, // LEAF

// STAGE 1 (NORMAL)
//  G1 G2 W   WFG  AS,  UN,  EXITS                       R
  { 1, 0, 1,  0,   0,   DC,  PlantExitFace_None,         1 }, // BASE LEAF
  { 0, 0, 0,  0,   1,   DC,  PlantExitFace_AcrossOffset, 3 }, // BASE LEAF + CENTER LEAF
  
// STAGE 2 (MAYBE FLOWER)
//  G1 G2 W   WFG  AS,  UN,  EXITS                       R
  { 1, 1, 1,  0,   0,   DC,  PlantExitFace_None,         1 }, // BASE LEAF
  { 0, 0, 0,  0,   0,   DC,  PlantExitFace_AcrossOffset, 3 }, // BASE LEAF + CENTER LEAF (end state 50%)
  { 1, 1, 0,  0,   0,   DC,  PlantExitFace_AcrossOffset, 3 }, // BASE LEAF + CENTER LEAF
  { 0, 0, 0,  0,   0,   DC,  PlantExitFace_AcrossOffset, 3 }, // BASE LEAF + CENTER LEAF (end state 25%)
  { 1, 1, 0,  0,   0,   DC,  PlantExitFace_AcrossOffset, 3 }, // BASE LEAF + CENTER LEAF (choose flower side)
  { 0, 0, 0,  0,   0,   DC,  PlantExitFace_AcrossOffset, 19 }, // BASE LEAF + CENTER LEAF + LEFT FLOWER
  { 0, 0, 0,  0,   0,   DC,  PlantExitFace_AcrossOffset, 20 }, // BASE LEAF + CENTER LEAF + RIGHT FLOWER
};

PlantStateNode plantStateGraphSeaweed[] =
{
// BASE
//  G1 G2 W   WFG  AS,  UN,  EXITS                       R
  { 0, 0, 0,  0,   1,   DC,  PlantExitFace_AcrossOffset, 13 }, // LEAF

// STAGE 1 (NORMAL)
//  G1 G2 W   WFG  AS,  UN,  EXITS                       R
  { 1, 1, 1,  0,   0,   DC,  PlantExitFace_None,         1 }, // BASE LEAF
  { 0, 0, 0,  0,   0,   DC,  PlantExitFace_AcrossOffset, 3 }, // BASE LEAF + CENTER LEAF
  { 0, 0, 0,  0,   1,   DC,  PlantExitFace_AcrossOffset, 3 }, // BASE LEAF + CENTER LEAF

// STAGE 2 (NORMAL)
//  G1 G2 W   WFG  AS,  UN,  EXITS                       R
  { 1, 1, 1,  0,   0,   DC,  PlantExitFace_None,         1 }, // BASE LEAF
  { 0, 0, 0,  0,   0,   DC,  PlantExitFace_AcrossOffset, 3 }, // BASE LEAF + CENTER LEAF
  { 0, 0, 0,  0,   1,   DC,  PlantExitFace_AcrossOffset, 3 }, // BASE LEAF + CENTER LEAF

// STAGE 3 (FLOWER)
//  G1 G2 W   WFG  AS,  UN,  EXITS                       R
  { 1, 0, 1,  0,   0,   DC,  PlantExitFace_None,         1 }, // BASE LEAF
  { 1, 0, 1,  0,   0,   DC,  PlantExitFace_None,         15 }, // BASE LEAF + CENTER FLOWER
  { 0, 0, 0,  0,   0,   DC,  PlantExitFace_None,         16 }, // BASE LEAF + CENTER FLOWER + SIDE LEAVES
};

PlantStateNode plantStateGraphDangle[] =
{
// BASE
//  G1 G2 W   WFG  AS,  UN,  EXITS                       R
  { 1, 0, 0,  1,   1,   DC,  PlantExitFace_OneAcross,    13 }, // CENTER LEAF (WAIT FOR GROWTH)
  { 0, 0, 0,  0,   1,   DC,  PlantExitFace_OneAcross,    14 }, // CENTER BRANCH

// STAGE 1 (STALK)
//  G1 G2 W   WFG  AS,  UN,  EXITS                       R
  { 1, 0, 1,  0,   0,   DC,  PlantExitFace_None,         1 }, // BASE LEAF
  { 1, 0, 0,  1,   1,   DC,  PlantExitFace_OneAcross,    4 }, // BASE BRANCH + CENTER LEAF
  { 1, 0, 1,  0,   1,   DC,  PlantExitFace_OneAcross,    5 }, // BASE BRANCH + CENTER BRANCH
  { 0, 0, 0,  0,   1,   DC,  PlantExitFace_OneAcross,    6 }, // BASE BRANCH + CENTER BRANCH + SIDE LEAVES

// STAGE 2 (DROOP)
//  G1 G2 W   WFG  AS,  UN,  EXITS                       R
  { 1, 1, 1,  0,   0,   DC,  PlantExitFace_None,         1 }, // BASE LEAF
  { 0, 0, 0,  0,   0,   DC,  PlantExitFace_AcrossOffset, 3 }, // BASE LEAF + CENTER LEAF
  { 0, 0, 0,  0,   1,   DC,  PlantExitFace_AcrossOffset, 3 }, // BASE LEAF + CENTER LEAF (ADVANCE STAGE)

// STAGE 3 (FLOWER)
//  G1 G2 W   WFG  AS,  UN,  EXITS                       R
  { 1, 0, 1,  0,   0,   DC,  PlantExitFace_None,         1 }, // BASE LEAF
  { 1, 0, 1,  0,   0,   DC,  PlantExitFace_None,         15 }, // BASE LEAF + CENTER FLOWER
  { 0, 0, 0,  0,   0,   DC,  PlantExitFace_None,         16 }, // BASE LEAF + CENTER FLOWER + SIDE LEAVES
};

struct PlantParams
{
  PlantStateNode *stateGraph;
  uint16_t leafColor;
};

PlantParams plantParams[] =
{
  // State graph              Leaf color
  {  plantStateGraphTree,     RGB_TO_U16_WITH_DIM(  0, 255,   0)  },
  {  plantStateGraphVine,     RGB_TO_U16_WITH_DIM(  0, 192,  64)  },
  {  plantStateGraphSeaweed,  RGB_TO_U16_WITH_DIM(128, 160,   0)  },
  {  plantStateGraphDangle,   RGB_TO_U16_WITH_DIM(160,  64,   0)  },
};

byte plantBranchStageIndexes[][4] =
{
  { 0, 2, 7, 12 },
  { 0, 1, 3, 0 },
  { 0, 1, 4, 7 },
  { 0, 2, 6, 9 },
};

// Timer that controls how often a plant must pay its maintenance cost or else wither.
// Should be slightly longer than the energy distribution rate so there's always time
// for energy to get there first.
#define PLANT_MAINTAIN_RATE (PLANT_ENERGY_RATE + 2000)
Timer plantMaintainTimer;

byte plantType = PlantType_None;
byte plantStateNodeIndex;
byte plantRootFace;
byte plantWitherState1;
byte plantWitherState2;
byte plantNumLeaves;
bool plantHasFlower = false;
byte plantEnergy;
#if DEBUG_PLANT_ENERGY
byte debugPlantEnergy;
#endif

byte plantBranchStage;
char plantExitFaceOffset;     // for plants that are gravity based

bool plantIsInResetState;
#ifndef DISABLE_CHILD_BRANCH_GREW
bool plantChildBranchGrew;
#endif


// Sun
#define MAX_GATHERED_SUN 200
byte gatheredSun;

// -------------------------------------------------------------------------------------------------
// BUG
//
#define BUG_FLAP_RATE 100
Timer bugFlapTimer;
#define BUG_NUM_FLAPS 7

byte bugTargetCorner  = 0;
char bugDistance      = 0;
char bugDirection     = 1;
bool bugFlapOpen      = false;

// -------------------------------------------------------------------------------------------------
// FISH
//
#define FISH_MOVE_RATE 2000
Timer fishMoveTimer;
Timer fishTailTimer;

enum FishTopFace
{
  FishTopFace_Face1,
  FishTopFace_Face3,
  FishTopFace_Face5,
};

//byte fishFacesToColor[][2] = { { 1, 2 }, { 3, 3 }, { 5, 4 } };    // top and bottom fish faces

enum FishSwimDir
{
  FishSwimDir_Left,
  FishSwimDir_Right,
};

struct FishInfo
{
  FishTopFace topFace : 2;
  FishSwimDir swimDir : 1;
  byte colorIndex     : 3;
};
FishInfo fishInfo;

bool fishTransferAccepted = false;

byte fishTailColorIndex;

uint16_t fishColors[] =
{
  RGB_TO_U16_WITH_DIM(242, 113, 102),
  RGB_TO_U16_WITH_DIM(255, 133,  33),
  RGB_TO_U16_WITH_DIM(250, 252, 104),
  RGB_TO_U16_WITH_DIM(181, 255,  33),
  RGB_TO_U16_WITH_DIM( 33, 255, 207),
  RGB_TO_U16_WITH_DIM(200, 104, 252),
  RGB_TO_U16_WITH_DIM( 64,  64,  64),
  RGB_TO_U16_WITH_DIM(200, 200, 200),
};

// -------------------------------------------------------------------------------------------------
// CRAWLY
//

#define CRAWLY_MOVE_RATE 1000
Timer crawlyMoveTimer;

enum CrawlDir
{
  CrawlDir_CW,
  CrawlDir_CCW,
};

#define CRAWLY_INVALID_FACE 6

CrawlDir crawlyDir;
byte crawlyHeadFace = CRAWLY_INVALID_FACE;
byte crawlyTailFace = CRAWLY_INVALID_FACE;
byte crawlyFadeFace = CRAWLY_INVALID_FACE;
bool crawlyTransferAttempted = false;
bool crawlyTransferAccepted = false;
char crawlyTransferDelay = 0;
byte crawlySpawnFace;
byte crawlyRateScale = 0;

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
    sponge[0] = 3;
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
#ifdef OLD_COMM_QUEUE
  commInsertionIndexes[f] = 0;
#else
  commInsertionIndex = 0;
#endif

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

#ifdef OLD_COMM_QUEUE
  if (commInsertionIndexes[f] >= COMM_QUEUE_SIZE)
  {
    // Buffer overrun - might need to increase queue size to accommodate
#if DETECT_COMM_ERRORS
    commInsertionIndexes[f] = COMM_INDEX_ERROR_OVERRUN;
#endif
    return;
  }
#else
  if (commInsertionIndex >= COMM_QUEUE_SIZE)
  {
    // Outgoing queue is full - just drop the packet
    return;
  }
#endif

#if DETECT_COMM_ERRORS
  if (data & 0xF0)
  {
    commInsertionIndexes[f] = COMM_DATA_OVERRUN;
  }
#endif

#ifdef OLD_COMM_QUEUE
  byte index = commInsertionIndexes[f];
  commQueues[f][index].command = command;
  commQueues[f][index].data = data;
  commInsertionIndexes[f]++;
#else
  commQueue[commInsertionIndex].face = f;
  commQueue[commInsertionIndex].command = command;
  commQueue[commInsertionIndex].data = data;
  commInsertionIndex++;
#endif
}

// Called every iteration of loop(), preferably before any main processing
// so that we can act on any new data being received.
void updateCommOnFaces()
{
  numNeighbors = 0;

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

      // No neighbor is a good place to spawn a crawly
      crawlySpawnFace = f;
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
    numNeighbors++;

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
        faceState->lastCommandIn = (Command) value;
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
#ifndef OLD_COMM_QUEUE
      byte commIndex = 0;

      for (; commIndex < commInsertionIndex; commIndex++)
      {
        if (commQueue[commIndex].face == f)
        {
          break;
        }
      }
#endif

      // If we just sent the DATA half of the previous comm, check if there 
      // are any more commands to send.
      if (faceState->faceValueOut.toggle == TOGGLE_DATA)
      {
#ifdef OLD_COMM_QUEUE
        if (commInsertionIndexes[f] == 0)
        {
          // Nope, no more comms to send - bail and wait
          continue;
        }
#else
        if (commIndex >= commInsertionIndex)
        {
          continue;
        }
#endif
      }

      // Send the next value, either COMMAND or DATA depending on the toggle bit

      // Toggle between command and data
      faceState->faceValueOut.toggle = ~faceState->faceValueOut.toggle;
      
      // Grab the first element in the queue - we'll need it either way
#ifdef OLD_COMM_QUEUE
      // [OPTIMIZE] saved 18 bytes - changed to a pointer
      CommandAndData *commandAndData = &commQueues[f][0];
#else
      CommandAndData *commandAndData = &commQueue[commIndex];
#endif

      // Send either the command or data depending on the toggle bit
      if (faceState->faceValueOut.toggle == TOGGLE_COMMAND)
      {
        faceState->faceValueOut.value = commandAndData->command;
      }
      else
      {
        faceState->faceValueOut.value = commandAndData->data;
  
        // No longer need this comm - shift everything towards the front of the queue
#ifdef OLD_COMM_QUEUE
        for (byte commIndex = 1; commIndex < COMM_QUEUE_SIZE; commIndex++)
        {
          commQueues[f][commIndex-1] = commQueues[f][commIndex];
        }
#else
        for (; commIndex < commInsertionIndex; commIndex++)
        {
          commQueue[commIndex] = commQueue[commIndex+1];
        }
#endif

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

#ifdef OLD_COMM_QUEUE
          commInsertionIndexes[f]--;
#else
          commInsertionIndex--;
#endif

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

  //randVal = (randVal << 1) ^ millis8();
  //randVal = millis8();

  // Detect button clicks
  handleUserInput();

  // Update neighbor presence and comms
  updateCommOnFaces();

  // Update the helper array to translate logical to physical faces
  gravityRelativeFace = &faceOffsetArray[gravityUpFace];

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
  loopFish();
  loopCrawly();

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

    if (clicks == 5)
    {
      // Five clicks resets this tile
      resetOurState();
    }
#if DEBUG_PLANT_ENERGY
    else if (clicks == 6)
    {
      tileFlags ^= TileFlag_ShowPlantEnergy;
    }
#endif
#if DEBUG_SPAWN_BUG
    if (clicks == 4)
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
#if DEBUG_SPAWN_FISH
    if (clicks == 4)
    {
      if (tileFlags & TileFlag_HasFish)
      {
        tileFlags &= ~TileFlag_HasFish;
      }
      else
      {
        tileFlags |= TileFlag_HasFish;
        fishInfo.topFace = FishTopFace_Face5;
        fishInfo.colorIndex = RANDOM_BYTE() & 0x7;
      }
    }
#endif
#if DEBUG_SPAWN_CRAWLY
    if (clicks == 4)
    {
      if (tileFlags & TileFlag_HasCrawly)
      {
        tileFlags &= ~TileFlag_HasCrawly;
      }
      else
      {
        tileFlags |= TileFlag_HasCrawly;
        crawlyDir = CrawlDir_CW;
        crawlyHeadFace = 0;
        crawlyTailFace = CRAWLY_INVALID_FACE;
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
      gravityUpFace = DEFAULT_GRAVITY_UP_FACE;  // dripper defines gravity
    }
  }

  if (buttonSingleClicked())// && !hasWoken())
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

    if (tileFlags & TileFlag_HasCrawly)
    {
      crawlyRateScale = crawlyRateScale ? 0 : 1;
    }

    // Button clicks are also how we try to spawn critters from flowers
    trySpawnCritter();
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

    faceState->waterLevel = 0;
    faceState->waterAdded = 0;
#ifndef ENABLE_DIRT_RESERVOIR
    faceState->waterStored = 0;
#endif
  }

#ifdef ENABLE_DIRT_RESERVOIR
  reservoir = 0;
#endif

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
    case Command_AddWater:
      faceState->waterAdded += value;
      
      // Clear our full flag since the neighbor clearly thinks we are not full
      // If we are actually full then this will get set and we'll send another message to update the neighbor
      faceState->flags &= ~FaceFlag_WaterFull;
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
      if (!(tileFlags & TileFlag_HasBug) && !(tileFlags & TileFlag_HasDirt) && !(faceStates[f].flags & FaceFlag_WaterFull))
      {
        // No bug in this tile - transfer accepted
        tileFlags |= TileFlag_HasBug;
#if 1
        bugTargetCorner = CW_FROM_FACE(f, value);
#else
        bugTargetCorner = (command == Command_TransferBug) ? f : CW_FROM_FACE(f, 1);
#endif
        bugDistance  = BUG_NUM_FLAPS;
        bugDirection = -1;  // start going towards the middle
        //bugFlapOpen = 0;    // looks better starting closed
        
        // Notify the sender
        enqueueCommOnFace(f, Command_Accepted, command);
      }
      break;

    case Command_TransferFish:
      if (!(tileFlags & TileFlag_HasFish) && !(tileFlags & TileFlag_HasDirt) && (tileFlags & TileFlag_Submerged))
      {
        // Transfer accepted
        tileFlags |= TileFlag_HasFish;
        fishInfo = *((FishInfo*) &value);
        fishMoveTimer.set(FISH_MOVE_RATE);
        
        // Notify the sender
        enqueueCommOnFace(f, Command_Accepted, command);
      }
      break;

    case Command_FishTail:
      fishTailColorIndex = value;
      fishTailTimer.set(FISH_MOVE_RATE);
      break;

    case Command_TransferCrawly:
      if (!(tileFlags & TileFlag_HasCrawly))
      {
        // Transfer accepted
        tileFlags |= TileFlag_HasCrawly;
        enqueueCommOnFace(f, Command_Accepted, command);

        crawlyDir = (CrawlDir) (value & 0x1);
        crawlyRateScale = value >> 1;
        crawlyHeadFace = f;
        crawlyTailFace = CRAWLY_INVALID_FACE;
        //crawlyFadeFace = CRAWLY_INVALID_FACE;

        crawlyMoveTimer.set(CRAWLY_MOVE_RATE>>1);
        crawlyTransferDelay = 2;  // countdown: 2 = don't show, 1 = don't move
      }
      break;

    case Command_Accepted:
      if (value == Command_TransferBug)
      {
        // Bug transferred! Remove ours
        tileFlags &= ~TileFlag_HasBug;
      }
      else if (value == Command_TransferFish)
      {
        // Fish transferred!
        // Don't turn off our fish until the timer expires
        fishTransferAccepted = true;
      }
      else if (value == Command_TransferCrawly)
      {
        // Crawly transferred!
        crawlyTransferAccepted = true;
      }
      break;

#ifndef DISABLE_CHILD_BRANCH_GREW
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
#endif

    case Command_GatherSun:
      if (gatheredSun < MAX_GATHERED_SUN)
      {
        gatheredSun += value;
      }
      break;

    case Command_QueryPlantType:
      if (plantType != PlantType_None)
      {
        // Combine the plant type and branch stage into one value to send

        // Branch stage optionally increments
        byte stage = plantBranchStage;
        if (plantParams[plantType].stateGraph[plantStateNodeIndex].advanceStage)
        {
          stage++;
        }

        enqueueCommOnFace(f, Command_PlantType, stage | (plantType << 2));
      }
      break;

    case Command_PlantType:
      if (plantType == PlantType_None)
      {
        plantBranchStage = value & 0x3;
        plantType = value >> 2;
        plantStateNodeIndex = plantBranchStageIndexes[plantType][plantBranchStage];
        plantIsInResetState = true;
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

// Experimenting with different implementations for these
int CW_FROM_FACE_FUNC(int f, int amt) { return ((f + amt) % FACE_COUNT); }
int CCW_FROM_FACE_FUNC(int f, int amt) { return ((f - amt) + ((f >= amt) ? 0 : 6)); }

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

// Changing these bit fields to be full bytes saves code space due to the unpacking required
// However, by making dstFace 4 bits instead of 3 I found the difference is smaller
// If full bytes = 0 extra, 3/3/1 is 30 bytes extra, 3/4/1 is 16 bytes extra
struct WaterFlowCommand
{
  byte srcFace : 3;
  byte dstFace : 4;
  byte halfWater : 1;
};

WaterFlowCommand waterFlowSequence[] =
{
  { 3, 3, 0 },   // fall from face 3 down out of this tile
  { 0, 3, 0 },   // top row falls into bottom row
  { 1, 2, 0 },   // ...
  { 5, 4, 0 },   // ...

  // When flowing to sides, every other command has 'halfWater' set
  // so that the first will send half and then the next will send the
  // rest (the other half).
  // This has a side effect "bug" where, if there is a left neighbor,
  // but not a right, it will only send half the water. While if there
  // is a right neighbor but not a left it will send it all.
  // Hope no one notices shhhhh
  { 5, 0, 1 },   // flow to sides
  { 5, 5, 0 },
  { 4, 3, 1 },
  { 4, 4, 0 },
  { 0, 5, 1 },
  { 0, 1, 0 },
  { 3, 4, 1 },
  { 3, 2, 0 },
  { 1, 0, 1 },
  { 1, 1, 0 },
  { 2, 3, 1 },
  { 2, 2, 0 }
};

void loopWater()
{
  if (!gravityTimer.isExpired())
  {
    return;
  }

#ifdef OPTIMIZE
  // Assume the tile is submerged (has water in all six faces)
  // This flag will be cleared below if this isn't true
  tileFlags |= TileFlag_Submerged;
#endif

  for (int waterFlowIndex = 0; waterFlowIndex < 16; waterFlowIndex++)
  {
    WaterFlowCommand command = waterFlowSequence[waterFlowIndex];

    // Get src/dst faces factoring in gravity direction
#ifdef OPTIMIZE   // [OPTIMIZE] saves 10 bytes
    byte srcFace = gravityRelativeFace[command.srcFace];
    byte dstFace = gravityRelativeFace[command.dstFace];
#else
    byte srcFace = command.srcFace;
    byte dstFace = command.dstFace;

    // Factor in gravity direction
    // Faces increase CW around the tile - need the CCW value to offset
    byte gravityCCWAmount = 6 - gravityUpFace;
    srcFace = CCW_FROM_FACE(srcFace, gravityCCWAmount);
    dstFace = CCW_FROM_FACE(dstFace, gravityCCWAmount);
#endif

    FaceState *faceStateSrc = &faceStates[srcFace];
    FaceState *faceStateDst = &faceStates[dstFace];

    byte amountToSend = MIN(faceStateSrc->waterLevel >> command.halfWater, WATER_FLOW_AMOUNT);
    if (amountToSend > 0)
    {
      if (command.srcFace == command.dstFace)
      {
        // Sending to a neighbor tile
        if (faceStateSrc->flags & FaceFlag_NeighborPresent)
        {
          // Only send to neighbor if they haven't set their 'full' flag
          if (!(faceStateDst->flags & FaceFlag_NeighborWaterFull))
          {
            enqueueCommOnFace(dstFace, Command_AddWater, amountToSend);
            faceStateSrc->waterLevel -= amountToSend;
          }
        }
      }
      else if (!(faceStateDst->flags & FaceFlag_WaterFull))
      {
        faceStateDst->waterAdded += amountToSend;
        faceStateSrc->waterLevel -= amountToSend;
      }
    }
    else
    {
#ifdef OPTIMIZE
      // No water in this face - tile isn't submerged
      tileFlags &= ~TileFlag_Submerged;
#endif
    }
  }

#ifndef OPTIMIZE // [OPTIMIZE] saves 68 bytes - sacrifices some accuracy - meh
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
#endif
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

#ifdef OPTIMIZE   // [OPTIMIZE] saves 4 bytes
    cwFromUp--;
    if (cwFromUp < 0)
    {
      cwFromUp = 5;
    }
#else
    cwFromUp -= (cwFromUp > 0) ? 1 : -5;
#endif

#ifdef OPTIMIZE   // [OPTIMIZE] saves 2 bytes
    f++;
    if (f >= 6)
    {
      f = 0;
    }
#else
    f += (f < 5) ? 1 : -5;
#endif

    // Done when we wrap around back to the start
  } while(f != gravityUpFace);
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

#ifdef ENABLE_DIRT_RESERVOIR
  if (reservoir == 0)
  {
    return;
  }
#else
  byte *reservoir = &faceStates[3].waterStored;

  // Suck water from the side dirt tiles to the center
  if (*reservoir < MAX_WATER_LEVEL)
  {
    if (faceStates[2].waterStored > 0)
    {
      faceStates[2].waterStored--;
      *reservoir += 1;
    }
    if (faceStates[4].waterStored > 0)
    {
      faceStates[4].waterStored--;
      *reservoir += 1;
    }
  }

  if (*reservoir == 0)
  {
    return;
  }
#endif

  // Energy generation formula:
  // Up to X water = X energy (so that a plant can sprout without needing sunlight)
  // After that Y water + Y sun = PLANT_ENERGY_PER_SUN*Y energy

  // X water
#ifndef ENABLE_DIRT_RESERVOIR
  byte energyToDistribute = (*reservoir > PLANT_ENERGY_FROM_WATER) ? PLANT_ENERGY_FROM_WATER : *reservoir;
  *reservoir -= energyToDistribute;
#else
  byte energyToDistribute = MIN(reservoir, PLANT_ENERGY_FROM_WATER);
  reservoir -= energyToDistribute;
#endif

  // Y sun + Y water
#ifndef ENABLE_DIRT_RESERVOIR
  byte minWaterOrSun = MIN(*reservoir, gatheredSun);
  *reservoir -= minWaterOrSun;
#else
  byte minWaterOrSun = MIN(reservoir, gatheredSun);
  reservoir -= minWaterOrSun;
#endif
  energyToDistribute += minWaterOrSun * PLANT_ENERGY_PER_SUN;
  gatheredSun = 0;

  plantEnergy += energyToDistribute;
  loopPlantGrow();
}

// -------------------------------------------------------------------------------------------------
// PLANTS
// -------------------------------------------------------------------------------------------------

char plantExitFaceOffsetArray[PlantType_MAX][6] =
{
  {  0, -1, -1, 99,  1,  1 },   // tree
  {  0, -1, -1, 99,  1,  1 },   // vine
  { 99,  1,  1,  0, -1, -1 },   // seaweed
  {  0, -1, -1, 99,  1,  1 },   // Dangle
};

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
#if 1
  char rootRelativeToGravity = plantRootFace - gravityUpFace;
  if (rootRelativeToGravity < 0)
  {
    rootRelativeToGravity += 6;
  }
#else
  byte rootRelativeToGravity = gravityRelativeFace[plantRootFace];
#endif

  plantExitFaceOffset = 0;
  if (plantStateNode->exitFace == PlantExitFace_AcrossOffset)
  {
#ifdef OPTIMIZE // [OPTIMIZE] saves 24 bytes - consumes 24 bytes of data
    plantExitFaceOffset = plantExitFaceOffsetArray[plantType][rootRelativeToGravity];
    if (plantExitFaceOffset == 99)
    {
      plantExitFaceOffset = 0;
      plantEnergy = 0;
    }
#else
    if (plantType == PlantType_Seaweed)
    {
      switch (rootRelativeToGravity)
      {
        case 0: plantEnergy = 0;          break;   // pointing directly down - cannnot grow
        case 1:
        case 2: plantExitFaceOffset = 1;  break;   // rotate CW by one face
        case 3:                           break;   // pointing directly up - all's good
        case 4:
        case 5: plantExitFaceOffset = -1; break;   // rotate CCW by one face
      }
    }
    else
    {
      switch (rootRelativeToGravity)
      {
        case 0:                           break;   // pointing directly down - all's good
        case 1:
        case 2: plantExitFaceOffset = -1; break;   // rotate CCW by one face
        case 3: plantEnergy = 0;          break;   // pointing directly up - cannnot grow
        case 4:
        case 5: plantExitFaceOffset = 1;  break;   // rotate CW by one face
      }
     }
#endif
  }

  // Only seaweed can live under water
  bool plantIsSubmerged = tileFlags & TileFlag_Submerged;
  bool plantIsAquatic = plantType == PlantType_Seaweed;
  if (plantIsSubmerged != plantIsAquatic)
  {
    plantEnergy = 0;
  }
  
  // Use energy to maintain the current state
  byte maintainCost = 1;
  if (plantEnergy < maintainCost)
  {
#ifdef OPTIMIZE // [OPTIMIZE] saves 6 bytes
    // Not enough energy - plant is dying
    // Wither plant
    plantStateNodeIndex = plantWitherState2;
    plantWitherState2 = 0;
    if (plantStateNodeIndex == 0)
    {
      plantStateNodeIndex = plantWitherState1;
      plantWitherState1 = 0;
      if (plantStateNodeIndex == 0)
      {
        // Can't wither any further - just go away entirely
        resetPlantState();
      }
    }
#else
    // Not enough energy - plant is dying
    // Wither plant
    if (plantWitherState2 > 0)
    {
      plantStateNodeIndex = plantWitherState2;
      plantWitherState2 = 0;
    }
    else if (plantWitherState1 > 0)
     {
       plantStateNodeIndex = plantWitherState1;
       plantWitherState1 = 0;
    }
    else
    {
      // Can't wither any further - just go away entirely
      resetPlantState();
    }
#endif
    return;
  }

  // Have enough energy to maintain - deduct it
  plantEnergy -= maintainCost;
}

PlantType plantTypeSelection[6][2] =
{
  { PlantType_Tree,     PlantType_Tree },
  { PlantType_Dangle,   PlantType_Dangle },
  { PlantType_Vine,     PlantType_Vine },
  { PlantType_Vine,     PlantType_Vine },
  { PlantType_Vine,     PlantType_Vine },
  { PlantType_Dangle,   PlantType_Dangle },
};
PlantType plantTypeSelectionSubmerged[6][2] =
{
  { PlantType_Seaweed,  PlantType_Seaweed },
  { PlantType_Seaweed,  PlantType_Seaweed },
  { PlantType_None,     PlantType_None },
  { PlantType_None,     PlantType_None },
  { PlantType_None,     PlantType_None },
  { PlantType_Seaweed,  PlantType_Seaweed },
};

void loopPlantGrow()
{
  // Plants use energy to stay alive and grow
  // This function is triggered when this tile receives plant energy from the root.
  // For dirt tiles, this is from loopDirt().
  // Otherwise it is when this tile receives a DistEnergy message from its plant root.

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
    byte randChoice = RANDOM_BYTE() & 0x1;
    plantType = (tileFlags & TileFlag_Submerged) ? plantTypeSelectionSubmerged[gravityUpFace][randChoice] : plantTypeSelection[gravityUpFace][randChoice];

    if (plantType == PlantType_None)
    {
      // Didn't find a plant type to grow - bail
      return;
    }

    plantBranchStage = 0;
    plantStateNodeIndex = 0;
    plantRootFace = 3;
    plantIsInResetState = true;
  }

  // Send gathered sun down to the root
  gatheredSun += plantNumLeaves;
  if (gatheredSun > 0)
  {
    enqueueCommOnFace(plantRootFace, Command_GatherSun, gatheredSun);
    gatheredSun = 0;
  }

#if DEBUG_PLANT_ENERGY
  debugPlantEnergy = plantEnergy;
#endif

  PlantStateNode *plantStateNode = &plantParams[plantType].stateGraph[plantStateNodeIndex];

  // Use energy to maintain the current state
  byte energyMaintain = 1;
  if (plantEnergy <= energyMaintain)
  {
    // Not enough energy to do anything else
    return;
  }
  
  byte energyForGrowth = plantEnergy - energyMaintain;

  // Assume we are spending/sending all remaining energy
  plantEnergy -= energyForGrowth;

  // Can we grow?
  if (plantStateNode->growState1 != 0 || plantIsInResetState)
  {
    // Is there enough energy to grow?
    byte growCost = 3;
    if (energyForGrowth >= growCost)
    {
      // Some states wait for a child branch to grow
#ifndef DISABLE_CHILD_BRANCH_GREW
      if (plantIsInResetState || !plantStateNode->waitForGrowth || plantChildBranchGrew)
#endif
      {
        if (plantIsInResetState)
        {
          // Come out of reset and show the first real state
          plantIsInResetState = false;
        }
        else
        {
          // Track wither states
          if (plantStateNode->isWitherState)
          {
            if (plantWitherState1 == 0)
            {
              plantWitherState1 = plantStateNodeIndex;
            }
            else
            {
              plantWitherState2 = plantStateNodeIndex;
            }
          }
  
          // Some states choose between two next states
          byte stateOffset = plantStateNode->growState1;
          if (plantStateNode->growState2 != 0)
          {
            // Randomly choose between the two
            if (RANDOM_BYTE() & 0x1)
            {
              stateOffset += plantStateNode->growState2;
            }
          }

          // Move to the next state
          plantStateNodeIndex += stateOffset;
        }
  
        // Deduct energy
        energyForGrowth -= growCost;

        // Next state might need to wait for a child branch!
#ifndef DISABLE_CHILD_BRANCH_GREW
        plantChildBranchGrew = false;

        // Tell the root plant that something grew - in case it is waiting on us
        enqueueCommOnFace(plantRootFace, Command_ChildBranchGrew, 0);
#endif
      }
    }
    else
    {
      // Didn't grow so hold on to all our energy - maybe we can grow next time
      plantEnergy += energyForGrowth;
      return;
    }
  }

  // Pass on any unused energy
  if (plantStateNode->exitFace != PlantExitFace_None)
  {
    if (energyForGrowth > 0)
    {
      switch (plantStateNode->exitFace)
      {
        case PlantExitFace_OneAcross:
          sendEnergyToFace(3, energyForGrowth);
          break;
          
        case PlantExitFace_Fork:
          sendEnergyToFace(2, energyForGrowth>>1);
          sendEnergyToFace(4, (energyForGrowth+1)>>1);  // right branch gets the "round up" when odd
          break;

        case PlantExitFace_AcrossOffset:
          sendEnergyToFace(3 + plantExitFaceOffset, energyForGrowth);
          break;
      }
    }
  }
}

void sendEnergyToFace(byte cwAmount, byte energyToSend)
{
  byte exitFace = CW_FROM_FACE(plantRootFace, cwAmount);
  enqueueCommOnFace(exitFace, Command_DistEnergy, energyToSend);
}

void resetPlantState()
{
  plantType = PlantType_None;
  plantEnergy = 0;
#ifndef DISABLE_CHILD_BRANCH_GREW
  plantChildBranchGrew = false;
#endif
  gatheredSun = 0;
#if DEBUG_PLANT_ENERGY
  debugPlantEnergy = 0;
#endif

  plantWitherState1 = 0;
  plantWitherState2 = 0;

  // Allow a new critter to spawn
  tileFlags &= ~TileFlag_SpawnedCritter;
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
  
  bugFlapOpen = !bugFlapOpen;

  // Move the bug along its path
#if 1 // saves 6 bytes
  bugDistance += bugDirection;
#else
  bugDistance += (bugDirection > 0) ? BUG_MOVE_RATE : -BUG_MOVE_RATE;
#endif
  if (bugDistance >= BUG_NUM_FLAPS)
  {
    // Start moving back towards the center
#if 0 // saves 6 bytes - imperceptibly less accurate
    bugDistance = 64;
#endif
    bugDirection = -1;
    
    // While doing this, try to transfer to neighbor cell
    // If the transfer is accepted then the bug will leave this tile
    byte otherFace = CCW_FROM_FACE(bugTargetCorner, 1);

    char tryTransferToFace = -1;
    byte cwOffset = 0;

    if (faceStates[bugTargetCorner].flags & FaceFlag_NeighborPresent && !(faceStates[bugTargetCorner].flags & FaceFlag_NeighborWaterFull))
    {
      tryTransferToFace = bugTargetCorner;
      cwOffset = 1;
    }
    
    if (faceStates[otherFace].flags & FaceFlag_NeighborPresent && !(faceStates[otherFace].flags & FaceFlag_NeighborWaterFull))
    {
      // Choose the other face if the first face isn't present
      // If both faces are present, then do a coin flip
      // Using the LSB of the time *should* be random enough for a coin flip
      if (tryTransferToFace == -1 || RANDOM_BYTE() & 0x1)
      {
        tryTransferToFace = otherFace;
        cwOffset = 0;
      }
    }

    if (tryTransferToFace != -1)
    {
      enqueueCommOnFace(tryTransferToFace, Command_TransferBug, cwOffset);
    }
  }
  else if (bugDistance < 0)
  {
#if 0 // saves 4 bytes - imperceptibly less accurate
    bugDistance = 0;
#endif
    bugDirection = 1;
    // Pick a different corner (50% opposite, 25% opposite-left, 25% opposite-right)
    char offset = (RANDOM_BYTE() & 0x1) ? 3 : ((RANDOM_BYTE() & 0x2) ? 2 : 4);
    bugTargetCorner = CW_FROM_FACE(bugTargetCorner, offset);
  }
}

// -------------------------------------------------------------------------------------------------
// FISH
// -------------------------------------------------------------------------------------------------

// Information about how fish move from tile to tile
struct FishMovementInfo
{
  FishTopFace nextTopFace : 2;  // if the fish moves, where it will be within the destination tile
  byte isNeighbor         : 1;  // flag if we need to attempt to transfer to a neighbor tile
  byte destFace           : 3;  // the outgoing face to attempt the transfer (gravity relative)
};
FishMovementInfo fishMovementInfoLeft[3][2] =
{
  { { FishTopFace_Face3, 1, 0  }, { FishTopFace_Face3, 0, DC } },
  { { FishTopFace_Face5, 0, DC }, { FishTopFace_Face5, 1, 3  } },
  { { FishTopFace_Face1, 1, 5  }, { FishTopFace_Face1, 1, 4  } }
};
FishMovementInfo fishMovementInfoRight[3][2] =
{
  { { FishTopFace_Face5, 1, 1  }, { FishTopFace_Face5, 1, 2  } },
  { { FishTopFace_Face1, 0, DC }, { FishTopFace_Face1, 1, 3  } },
  { { FishTopFace_Face3, 1, 0  }, { FishTopFace_Face3, 0, DC } }
};
FishMovementInfo *fishChosenMovementInfo = null;

void loopFish()
{
  if (!(tileFlags & TileFlag_HasFish))
  {
    return;
  }

  // Did the fish finish moving to its destination?
  if (!fishMoveTimer.isExpired())
  {
    return;
  }

  byte downFace = gravityRelativeFace[3];

  if (fishChosenMovementInfo != null)
  {
    if (fishChosenMovementInfo->isNeighbor)
    {
      // If we tried to transfer the fish, check if it was accepted
      if (fishTransferAccepted)
      {
        // Fish was transferred - turn ours off
        tileFlags &= ~TileFlag_HasFish;
    
        // If we transferred upwards then the tail is still visible in this tile
        if (fishChosenMovementInfo->nextTopFace == FishTopFace_Face3)
        {
          fishTailColorIndex = fishInfo.colorIndex;
          fishTailTimer.set(FISH_MOVE_RATE);
        }
      }
      else
      {
        // Fish was not transferred - turn around and try the other direction
        fishInfo.swimDir = (fishInfo.swimDir == FishSwimDir_Left) ? FishSwimDir_Right : FishSwimDir_Left;
      }
    }
    else
    {
      // Moved within our own cell - that's easy
      fishInfo.topFace = fishChosenMovementInfo->nextTopFace;
    }
  }

  // Done processing the previous move info
  fishChosenMovementInfo = null;
  
  // If fish moved to a neighbor then nothing left to do here
  if (!(tileFlags & TileFlag_HasFish))
  {
    return;
  }

  // Fish is still here - set up for the next move
  fishMoveTimer.set(FISH_MOVE_RATE);

  // If our tail pokes into a neighbor then tell that tile to show it
  if (fishInfo.topFace == FishTopFace_Face3)
  {
    enqueueCommOnFace(downFace, Command_FishTail, fishInfo.colorIndex);
  }

  // Pick the next move based on direction (left or right) and coin flip (up or down)
  byte randUpOrDown = RANDOM_BYTE() & 0x1;
  if (!(tileFlags & TileFlag_Submerged))
  {
    // If this tile isn't full of water then force the fish to go down to try to find some
    randUpOrDown = 1;
  }
  fishChosenMovementInfo = fishInfo.swimDir ? &fishMovementInfoRight[fishInfo.topFace][randUpOrDown] : &fishMovementInfoLeft[fishInfo.topFace][randUpOrDown];

  if (fishChosenMovementInfo->isNeighbor)
  {
    // Next move is trying to go into a neighbor cell
    // Send a transfer request to the given neighbor
    // Will be ignored if the neighbor doesn't exist or can't accept the fish

    // Mark that the transfer hasn't yet been accepted
    // This flag will be set when we receive an ack from the neighbor
    // The ack can be received any time until our next move, which is 1 sec
    fishTransferAccepted = false;

    // Convert from logical to physical face
    byte transferFace = gravityRelativeFace[fishChosenMovementInfo->destFace];

    // Construct the fish info struct for the receiver so they don't need to do any expensive calcs
    FishInfo nextFishInfo = fishInfo;
    nextFishInfo.topFace = fishChosenMovementInfo->nextTopFace;
    enqueueCommOnFace(transferFace, Command_TransferFish, *((byte*)&nextFishInfo));
  }
}

// -------------------------------------------------------------------------------------------------
// CRAWLY
// -------------------------------------------------------------------------------------------------

void loopCrawly()
{
  if (!(tileFlags & TileFlag_HasCrawly))
  {
    return;
  }

  // Did the crawly finish moving?
  if (!crawlyMoveTimer.isExpired())
  {
    return;
  }
  crawlyMoveTimer.set(crawlyRateScale ? (CRAWLY_MOVE_RATE>>1) : CRAWLY_MOVE_RATE);

  // First two loops after a transfer we don't want to move
  crawlyTransferDelay--;
  if (crawlyTransferDelay > 0)
  {
    return;
  }

  // First, resolve the results of the previous move
  // Did we attempt to transfer to a neighbor tile and, if so, was it accepted?
  if (crawlyTransferAttempted)
  {
    crawlyTransferAttempted = false;
    if (crawlyTransferAccepted)
    {
      // Crawly moved to a new tile
      // Start invalidating it here
      moveCrawlyToFace(CRAWLY_INVALID_FACE);
      return;
    }
  }
  
  // If crawly is in the process of leaving then just let it go
  if (crawlyHeadFace == CRAWLY_INVALID_FACE)
  {
    // Keep invalidating the crawly in this tile until it is fully gone
    moveCrawlyToFace(CRAWLY_INVALID_FACE);

    if (crawlyFadeFace == CRAWLY_INVALID_FACE)
    {
      // Crawly is fully gone from this tile
      tileFlags &= ~TileFlag_HasCrawly;
    }

    return;
  }

  // Got here so crawly is still in this tile
  // Determine where it will (try to) go next
  // 1. If no neighbor at destination, proceed CW or CCW
  // 2. If neighbor at this face, try to transfer to the new tile
  // 3. If transfer was blocked, turn around
  byte cwFace = CW_FROM_FACE(crawlyHeadFace, 1);
  byte ccwFace = CW_FROM_FACE(crawlyHeadFace, 5);
  byte cwFace2 = CW_FROM_FACE(crawlyHeadFace, 2);
  byte ccwFace2 = CW_FROM_FACE(crawlyHeadFace, 4);
  byte forwardFace = crawlyDir ? ccwFace : cwFace;
  bool canMoveForward = !(faceStates[forwardFace].flags & FaceFlag_NeighborPresent);
  byte backFace = crawlyDir ? cwFace : ccwFace;
  bool canMoveBackward = !(faceStates[backFace].flags & FaceFlag_NeighborPresent);

  byte nextFace = CRAWLY_INVALID_FACE;
  if (!(faceStates[crawlyHeadFace].flags & FaceFlag_NeighborPresent))
  {
    // No tile next to us, just keep crawling crawling
    nextFace = forwardFace;
  }
  else if (canMoveForward)
  {
    nextFace = forwardFace;
  }
  else if (canMoveBackward)
  {
    // Turn around!
    nextFace = backFace;
    crawlyDir = (CrawlDir) ~crawlyDir;
  }
  else
  {
    //...stay put I guess?
  }

  // Move! Also kick off a transfer attempt if the destination allows it.
  if (nextFace != CRAWLY_INVALID_FACE)
  {
    moveCrawlyToFace(nextFace);

    // Only time crawly has a choice of where to go is at a narrow entrance
    // Either she can transfer to the other tile or continue across the gap within the same tile
    forwardFace = crawlyDir ? ccwFace2 : cwFace2;
    canMoveForward = !(faceStates[forwardFace].flags & FaceFlag_NeighborPresent);
    if (!canMoveForward || (RANDOM_BYTE() & 0x1))
    {
      // If the destination has a neighbor, try to transfer to it
      if (faceStates[crawlyHeadFace].flags & FaceFlag_NeighborPresent)
      {
        // Try to transfer to the neighbor tile
        crawlyTransferAttempted = true;
        crawlyTransferAccepted = false;
        enqueueCommOnFace(crawlyHeadFace, Command_TransferCrawly, crawlyDir | (crawlyRateScale << 1));
      }
    }
  }
}

void moveCrawlyToFace(byte face)
{
  crawlyFadeFace = crawlyTailFace;
  crawlyTailFace = crawlyHeadFace;
  crawlyHeadFace = face;
}

// -------------------------------------------------------------------------------------------------

void trySpawnCritter()
{
  // Each tile can only spawn one critter
  if (tileFlags & TileFlag_SpawnedCritter)
  {
    return;
  }

  // Can only spawn a critter when there is a flower
  if (!plantHasFlower)
  {
    return;
  }

  // Check if we already have a critter
  if (tileFlags & (TileFlag_HasBug | TileFlag_HasFish | TileFlag_HasCrawly))
  {
    return;
  }

  // When under water, spawn a fish
  // Otherwise spawn a bug or crawly
  if (tileFlags & TileFlag_Submerged)
  {
    tileFlags |= TileFlag_HasFish;
  }
  else if (numNeighbors >= 5)
  {
    tileFlags |= TileFlag_HasBug;
  }
  else
  {
    tileFlags |= TileFlag_HasCrawly;
    crawlyDir = (CrawlDir) (RANDOM_BYTE() & 0x1);
    crawlyHeadFace = crawlySpawnFace;
    crawlyRateScale = 0;
  }

  tileFlags |= TileFlag_SpawnedCritter;
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
#ifndef ENABLE_DIRT_RESERVOIR
      if (faceState->waterStored < MAX_WATER_LEVEL)
      {
        byte waterToStore = MIN(faceState->waterLevel, MAX_WATER_LEVEL - faceState->waterStored);
        faceState->waterStored += waterToStore;
        faceState->waterLevel -= waterToStore;
      }
#else
      if (reservoir < MAX_WATER_LEVEL)
      {
        byte waterToStore = MIN(faceState->waterLevel, MAX_WATER_LEVEL - reservoir);
        reservoir += waterToStore;
        faceState->waterLevel -= waterToStore;
      }
#endif
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
  
  FOREACH_FACE(f)
  {
#ifndef ENABLE_DIRT_RESERVOIR
    byte *waterToEvaporate = &faceStates[f].waterLevel;
    
    // Dirt faces evaporate from the stored amount
    if ((tileFlags & TileFlag_HasDirt) && (f >= 2) && (f <= 4))
    {
      waterToEvaporate = &faceStates[f].waterStored;
    }

    if (*waterToEvaporate > 0)
    {
      *waterToEvaporate -= 1;
    }
#else
    if (faceStates[f].waterLevel > 0)
    {
      faceStates[f].waterLevel--;
    }
#endif
  }

#ifdef ENABLE_DIRT_RESERVOIR
  // Dirt faces evaporate from the stored amount
  if (reservoir > 0)
  {
    reservoir--;
  }
#endif
}

// =================================================================================================
//
// RENDER
//
// =================================================================================================

void render()
{
  Color color;

  setColor(OFF);

  // WATER
  color.as_uint16 = U16_WATER;
  FOREACH_FACE(f)
  {
    if (faceStates[f].waterLevel > 0)
    {
      SET_COLOR_ON_FACE(color, f);
    }
  }

  // DIRT
  if (tileFlags & TileFlag_HasDirt)
  {
    color.as_uint16 = U16_DIRT;
    SET_COLOR_ON_FACE(color, 2);
    SET_COLOR_ON_FACE(color, 4);
    if (tileFlags & TileFlag_DirtIsFertile)
    {
      color.as_uint16 = U16_FERTILE;
    }
    SET_COLOR_ON_FACE(color, 3);
   }

  // PLANTS
  if (plantType != PlantType_None && !plantIsInResetState)
  {
    PlantStateNode *plantStateNode = &plantParams[plantType].stateGraph[plantStateNodeIndex];
    int lutBits = plantRenderLUTIndexes[plantStateNode->faceRenderIndex];
    byte targetFace = plantRootFace;

    plantNumLeaves = 0;
    plantHasFlower = false;
    uint16_t flowerColor = tileFlags & TileFlag_SpawnedCritter ? U16_FLOWER_USED : U16_FLOWER;
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
          case PlantFaceType_Leaf:   plantNumLeaves++;   color.as_uint16 = plantParams[plantType].leafColor;   break;
          case PlantFaceType_Branch:                     color.as_uint16 = U16_BRANCH; break;
          case PlantFaceType_Flower: plantNumLeaves++;   color.as_uint16 = flowerColor; plantHasFlower = true; break;
        }
        SET_COLOR_ON_FACE(color, targetFace);
      }
      lutBits >>= 2;
      targetFace = CW_FROM_FACE(targetFace, 1);
    }
  }

  // BUG
  if (tileFlags & TileFlag_HasBug)
  {
    color.as_uint16 = U16_BUG;
    SET_COLOR_ON_FACE(color, CW_FROM_FACE(bugTargetCorner, bugFlapOpen ? 0 : 1));
    SET_COLOR_ON_FACE(color, CW_FROM_FACE(bugTargetCorner, bugFlapOpen ? 5 : 4));
  }

  // FISH
  if (!fishTailTimer.isExpired())
  {
    color.as_uint16 = fishColors[fishTailColorIndex];
    SET_COLOR_ON_FACE(color, gravityUpFace);
  }
  if ((tileFlags & TileFlag_HasFish) &&
      fishChosenMovementInfo != null)
  {
    color.as_uint16 = fishColors[fishInfo.colorIndex];
    byte fishTopActual = (byte) fishInfo.topFace;
    fishTopActual = (fishTopActual << 1) | 0x1;    // convert enum to 1, 3, or 5
    fishTopActual = gravityRelativeFace[fishTopActual];
    SET_COLOR_ON_FACE(color, fishTopActual);
    if (fishInfo.topFace == FishTopFace_Face1)
    {
      SET_COLOR_ON_FACE(color, gravityRelativeFace[2]);
    }
    else if (fishInfo.topFace == FishTopFace_Face5)
    {
      SET_COLOR_ON_FACE(color, gravityRelativeFace[4]);
    }
  }

  // CRAWLY
  if (tileFlags & TileFlag_HasCrawly && crawlyTransferDelay < 2)
  {
    color.as_uint16 = U16_CRAWLY;
    if (crawlyHeadFace != CRAWLY_INVALID_FACE)
    {
      SET_COLOR_ON_FACE(color, crawlyHeadFace);
    }
    if (crawlyTailFace != CRAWLY_INVALID_FACE)
    {
      SET_COLOR_ON_FACE(color, crawlyTailFace);
    }
    if (crawlyFadeFace != CRAWLY_INVALID_FACE)
    {
      SET_COLOR_ON_FACE(color, crawlyFadeFace);
    }
  }

  // DRIPPER
  if (tileFlags & TileFlag_HasDripper)
  {
    // Dripper is always on face 0
    color.as_uint16 = U16_DRIPPER;
    SET_COLOR_ON_FACE(color, 0);
  }

#if DEBUG_COLORS
  // Debug to show gravity
  setColorOnFace(WHITE, gravityUpFace);
#endif

#if DEBUG_PLANT_ENERGY
  if (tileFlags & TileFlag_ShowPlantEnergy)
  {
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
  }
#endif

  // Debug output overrides eveeeerything
  FOREACH_FACE(f)
  {
    color.as_uint16 = U16_DEBUG;
    if (faceStates[f].flags & FaceFlag_Debug)
    {
      setColorOnFace(color, f);
    }
  }

}

/*

2020-Sep-18
* Experimented with different rand algorithms. Added millis8() alternative to millis() that returns an 8-bit value. Saves 6 bytes <shrug>
* Allow random plant selection between two choices

2020-Sep-18
* Remove experimental and unused NEW_RENDER code
* Change fish & plant color code to use uint16_t, which removed need for makeColorFromByte and shaved ~50 bytes

2020-Sep-18
* Add fish critter type
* Add crawly critter type
* Critter now spawns when tile with flower is clicked
* 'Slope' plant type renamed to 'Dangle' for whatever reason idk
* Parameterization of plant type determination for easier plant expansion
* Code space optimizations
  - Remove 'child branch grew' feature of plants :(
  - Rework dirt reservoir - smaller AND works better :)
  - Disable datagrams in blinklib
  - Set 'as_uint16' color field directly instead of calling makeColorRGB() duh
  - Expand comm payload size to use all 8 bits of the face value
  - New data-centric method for CW_FROM_FACE, CCW_FROM_FACE, and OPPOSITE_FACE
  - ^^^ This also enables a new way to get faces relative to gravity
  - Bug move algorithm smallified (tm)
  - Remove use of blinklib::hasWoken()
  - Smallified (tm) algorithm to detect submerged tiles
  - Parameterize determination of plantExitFaceOffset (data vs. code tradeoff)
  - Plant wither state smallification (c)
  
*/
