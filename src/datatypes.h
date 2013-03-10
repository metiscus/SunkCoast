typedef struct
{
  int x, y;
} Point;
#define NULL_POINT {0,0}

typedef struct
{
  float x, y;
} FPoint;
#define NULL_FPOINT {0.0f,0.0f}

typedef struct
{
  Point a, b;
} Rectangle;
#define NULL_RECTANGLE {NULL_POINT, NULL_POINT}

typedef struct
{
  FPoint a, b;
} FRectangle;
#define NULL_FRECTANGLE {NULL_FPOINT, NULL_FPOINT}

typedef struct
{
  int r,g,b,a;
} Color;
#define NULL_COLOR {0xff,0xff,0xff,0xff}

typedef enum
{
  IMAGE_FONT = 0,
  IMAGE_MAX,
}  ImageID;
#define NULL_IMAGEID (0);

typedef struct
{
  Point pos;
  Point size;
  ImageID image;
} SpriteData;
#define NULL_SPRITEDATA {NULL_POINT, NULL_POINT, 0}

typedef enum
{
  DIR_UP=0,
  DIR_RIGHT,
  DIR_DOWN,
  DIR_LEFT,
} Direction;

typedef enum
{
  TILE_NONE=0,
  TILE_WALL,
  TILE_HIDE,
  TILE_MAX,
} TileType;
#define NULL_TILETYPE (TILE_NONE)

typedef struct
{
  Point frame;
  TileType type;
  bool seen;
  bool visible;
} Tile;
#define NULL_TILE {NULL_POINT, NULL_TILETYPE, false, false}

#define TILEMAP_WIDTH (60)
#define TILEMAP_HEIGHT (20)
extern const Point tilemap_size;
extern const Point tile_size;
typedef struct
{
  SpriteData spriteData;
  Tile tiles[TILEMAP_WIDTH*TILEMAP_HEIGHT];  
  Point size;
  int numTiles;
} TileMap;
#define NULL_TILEMAP (tilemap_null_tileMap());

typedef enum
{
  IT_CONCH,
  IT_CHARM,
  IT_MAX,
} ItemType;

typedef struct
{  
  bool active;
  SpriteData sprite;
  Point frame;
  ItemType type;
  int subtype;
  Point pos;
} Item;
#define NULL_ITEM { false, NULL_SPRITEDATA, NULL_POINT, IT_MAX, 0, NULL_POINT}

typedef struct
{
  bool active;
  SpriteData sprite;
  Point frame;
  Point pos;
  int turn;
  bool player;
  int speed;
  int o2;
  int maxo2;
  int o2timer;
  bool o2depletes;
  int strength;
  const char* name;
  bool sentient;
  bool containso2;
} Entity;
#define NULL_ENTITY { false, NULL_SPRITEDATA, NULL_POINT, NULL_POINT,\
                      INT_MAX, false, 100, 100, 100, 0, false, 4, NULL,\
                      false, false}

#define MAX_ITEMS (64)
#define MAX_ENTITIES (64)
typedef struct
{
  Entity entities[MAX_ENTITIES];
  Item items[MAX_ITEMS];
  TileMap tileMap;  
} FathomData;
#define NULL_FATHOMDATA (game_null_fathomdata());

#define MAX_FATHOMS (32)
typedef struct
{
  int current;
  FathomData fathoms[MAX_FATHOMS];
} GameData;
#define NULL_GAMEDATA (game_null_gamedata());

int min(int a, int b);
int max(int a, int b);
int minmax(int lower, int upper, int n);
Point pointAddPoint(Point a, Point b);
FPoint fpointAddFPoint(FPoint a, FPoint b);
Rectangle rectangleAddPoint(Rectangle a, Point b);
FRectangle frectangleAddFPoint(FRectangle a, FPoint b);
Point pointInverse(Point a);
FPoint fpointInverse(FPoint a);
Point pointMultiply(Point a, int n);
FPoint fpointMultiply(FPoint a, float f);
Rectangle rectangleMultiply(Rectangle a, int n);
FRectangle frectangleMultiply(FRectangle a, float n);

FPoint pointToFPoint(Point in);
Point fpointToPoint(FPoint in);
FRectangle rectangleToFRectangle(Rectangle in);
Rectangle frectangleToRectangle(FRectangle in);

bool rectangleIntersect(Rectangle a, Rectangle b);
bool frectangleIntersect(FRectangle a, FRectangle b);
bool pointInRectangle(Point p, Rectangle r);
bool fpointInFRectangle(FPoint p, FRectangle r);

Point directionToPoint(Direction d);

Point getFrameFromAscii(char c, int colour);
