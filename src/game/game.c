#include "main.h"

char messages[TILEMAP_HEIGHT][TILEMAP_WIDTH];
int numMessages;
bool midDrop;
bool midUse;
bool midFire;
int fireIndex;

Entity game_null_entity()
{
  SpriteData nullSpriteData = NULL_SPRITEDATA;
  Point nullPoint = NULL_POINT;
  Item nullItem = NULL_ITEM;

  Entity out;
  out.active = false;
  out.sprite = nullSpriteData;
  out.frame = nullPoint;
  out.pos = nullPoint;
  out.turn = INT_MAX;
  out.player = false;
  out.speed = 100;
  out.o2 = 100;
  out.maxo2 = 100;
  out.o2timer = 0;
  out.strength = 4;
  out.name = NULL;
  out.flags = EF_SENTIENT;
  out.lastKnownPlayerPos = nullPoint;
  out.hunting = false;
  out.scared = false;
  out.xp = 1;
  out.level = 0;
  out.blindTimer = 0;

  int i;
  for(i=0; i<MAX_INVENTORY; i++)
    out.inventory[i] = nullItem;
  return out;
}

FathomData game_null_fathomdata()
{
  FathomData out;
  int i;
  Entity nullEntity = NULL_ENTITY;
  for(i=0; i<MAX_ENTITIES; i++)
    out.entities[i] = nullEntity;

  Item nullItem = NULL_ITEM;
  for(i=0; i<MAX_ITEMS; i++)
    out.items[i] = nullItem;

  out.tileMap = tilemap_generate();
  return out;
}

void game_reset_gamedata(GameData* game)
{
  game->current = 0;

  item_shuffleTypes((int*)game->charmTypes, CHARM_MAX);
  item_shuffleTypes((int*)game->conchTypes, CONCH_MAX);

  int i;
  for(i=0; i<MAX_FATHOMS; i++)
  {
    int j;
    game->fathoms[i] = game_null_fathomdata();
    int threat = 5+i*3;
    while(threat > 0)
    {
      int type = ET_MAX_ENEMY;
      while(type >= ET_MAX_ENEMY)
      {
        type = 0;
        for(j=0; j<i/2+1; j++)
          type += sys_randint(3);
      }
      game_spawn(&game->fathoms[i], spawn_entity(type));
      threat -= type+1;
    }    
    for(j=0; j<(MAX_FATHOMS-i)/4; j++)
      game_spawn(&game->fathoms[i], spawn_entity(ET_BUBBLE));
    int numSpawns;
    numSpawns = sys_randint(3);
    for(j=0; j<numSpawns; j++)
      game_place(&game->fathoms[i], spawn_item(game, IT_CONCH));
    numSpawns = sys_randint(2);
    for(j=0; j<numSpawns; j++)
      game_place(&game->fathoms[i], spawn_item(game, IT_CHARM));
  }
  game_spawn(&game->fathoms[0], spawn_entity(ET_SCUBA));    

  // this isn't quite the right place for this stuff anymore
  numMessages = 0;
  game_addGlobalMessage("Welcome to Sunk Coast.");
  midDrop = false;
}

bool _do_drop(FathomData* fathom, Entity* e, int index);

bool game_hasCharm(const Entity *e, CharmSubType charm)
{
  int i;
  for(i=0; i<MAX_ITEMS; i++)
  {
    if(e->inventory[i].type != IT_CHARM)
      continue;
    if(e->inventory[i].charmSubtype != charm)
      continue;
    if(!e->inventory[i].worn)
      continue;
    return true;
  }
  return false;
}

bool game_hurt(FathomData *fathom, Entity *e, int amount)
{
  int i;
  Entity nullEntity = NULL_ENTITY;
  e->o2 -= amount;
  if(amount > 0 && e->flags & EF_SCARES)
    e->scared = true;
  if(e->o2 <= 0)
  {    
    if(game_hasCharm(e, CHARM_RESURRECT))
    {
      e->o2 = e->maxo2-10;      
      Item nullItem = NULL_ITEM;
      for(i=0; i<MAX_INVENTORY; i++)
      {
        if(!e->inventory[i].active)
          continue;
        if(e->inventory[i].type != IT_CHARM)
          continue;
        if(e->inventory[i].charmSubtype != CHARM_RESURRECT)
          continue;
        if(!e->inventory[i].worn)
          continue;
        e->inventory[i] = nullItem;
      }
      game_addMessage(fathom, e->pos, "%s come back to life", e->name);
      return false;
    }
    for(i=0; i<MAX_INVENTORY; i++)
    {
      if(!e->inventory[i].active)
        continue;
      _do_drop(fathom, e, i);
    }
    *e = nullEntity;
    return true;
  }
  if(e->flags & EF_SPLITS && e->o2 > 5)
  {
    e->o2 = e->o2/2;
    Point nearby = pointAddPoint(e->pos, directionToPoint(sys_randint(4)));
    if(tilemap_visible(&fathom->tileMap, e->pos) && game_pointFree(fathom, nearby))
    {
      game_spawnAt(fathom, *e, nearby);
      game_addMessage(fathom, nearby, "%s split", e->name);
    }
  }
  return false;
}

int game_pointEntityIndex(const FathomData *fathom, Point p)
{
  int i;
  for(i=0; i<MAX_ENTITIES; i++)
  {
    const Entity* e = &fathom->entities[i];
    if(!e->active)
      continue;
    if(e->pos.x != p.x || e->pos.y != p.y)
      continue;
    return i;
  }
  return -1;
}

bool game_pointFree(const FathomData *fathom, Point p)
{
  if(tilemap_collides(&fathom->tileMap, p))
    return false;
  if(game_pointEntityIndex(fathom, p) != -1)
    return false;
  return true;
}

int _game_turnCmp(const void* a, const void* b)
{
  Entity* eA = (Entity*)a;
  Entity* eB = (Entity*)b;
  if (eA->turn == eB->turn)
    return 0;
  else if (eA->turn < eB->turn)
    return -1;
  else
    return 1;
}

void _game_sortEntities(FathomData* fathom)
{
    qsort((void*)&fathom->entities[0], MAX_ENTITIES, sizeof(Entity), _game_turnCmp);
    int minTurn = fathom->entities[0].turn;
    int i;
    for(i=0; i<MAX_ENTITIES; i++)
    {
      if(!fathom->entities[i].active)
        continue;
      fathom->entities[i].turn -= minTurn;
    }
}

Point _game_getSpawnPoint(const FathomData* fathom)
{
  Point spawnPoint = NULL_POINT;
  int i;
  for(i=0; i<TILEMAP_WIDTH*TILEMAP_HEIGHT; i++)
  {
    spawnPoint.x = sys_randint(TILEMAP_WIDTH);
    spawnPoint.y = sys_randint(TILEMAP_WIDTH);
    if(game_pointFree(fathom, spawnPoint))
      break;
  }
  return spawnPoint;
}

void game_place(FathomData* fathom, Item item)
{
  item.pos = _game_getSpawnPoint(fathom);
  item.active = true;

  int i;  
  for(i=0; i<MAX_ITEMS; i++)
  {
    if(fathom->items[i].active)
      continue;
    fathom->items[i] = item;
    return;
  }
  LOG("Couldn't find a free item space, not placing.");
}

void game_spawnAt(FathomData* fathom, Entity entity, Point pos)
{
  if(pos.x > 0 && pos.y > 0 && game_pointFree(fathom, pos))
    entity.pos = pos;
  else
    entity.pos = _game_getSpawnPoint(fathom);

  entity.active = true;
  entity.turn = entity.speed;

  int i;
  for(i=0; i<MAX_ENTITIES; i++)
  {
    if(fathom->entities[i].active)
      continue;
    fathom->entities[i] = entity;
    _game_sortEntities(fathom);
    return;
  }
  LOG("Couldn't find a free entity space, not spawning.");
}
void game_spawn(FathomData* fathom, Entity entity)
{
  game_spawnAt(fathom, entity, _game_getSpawnPoint(fathom));
}

const FathomData* _aStarFathom;
Point _aStarStartPoint;
Point _aStarFinishPoint;

uint8_t _get_map_cost (const uint32_t x, const uint32_t y)
{
  Point p = NULL_POINT;
  p.x = x;
  p.y = y;
  if(p.x == _aStarStartPoint.x && p.y == _aStarStartPoint.y)
    return 1;
  if(p.x == _aStarFinishPoint.x && p.y == _aStarFinishPoint.y)
    return 1;
  if(!game_pointFree(_aStarFathom, p))
    return COST_BLOCKED;
  else
    return 1;
}

int game_nextLevel(int level)
{
  return 1 << (level+3);
}

void _draw_hud(const GameData* game, Entity e, Point offset)
{
  char string[TILEMAP_WIDTH];
  int o2col = 2;
  if(e.o2 < e.maxo2/4)
    o2col = 6;
  snprintf(string, TILEMAP_WIDTH, "  O2: %3d/%3d", e.o2, e.maxo2);
  sys_drawString(offset, string, TILEMAP_WIDTH, o2col);

  Point xpPos = offset;
  xpPos.y += 1;
  snprintf(string, TILEMAP_WIDTH, " lvl:  %2d      xp: %d/%d", e.level+1, e.xp, game_nextLevel(e.level));
  sys_drawString(xpPos, string, TILEMAP_WIDTH, 2);

  Point strengthPos = offset;
  strengthPos.x += 14;
  snprintf(string, TILEMAP_WIDTH, "str: %d", e.strength);
  sys_drawString(strengthPos, string, TILEMAP_WIDTH, 2);

  Point fathomPos = offset;
  fathomPos.x = TILEMAP_WIDTH-12;
  fathomPos.y++;
  snprintf(string, TILEMAP_WIDTH, "fathoms: %d", (game->current+1)*10);
  sys_drawString(fathomPos, string, TILEMAP_WIDTH, 2);

  Point inventoryPos = offset;
  inventoryPos.x = TILEMAP_WIDTH-MAX_INVENTORY*4;
  int i;
  for(i=0; i<MAX_INVENTORY; i++)
  {
    Item item = e.inventory[i];
    int itemcol = 2;
    if(item.worn)
      itemcol = 4;
    snprintf(string, TILEMAP_WIDTH, "%d:", i+1);
    sys_drawString(inventoryPos, string, TILEMAP_WIDTH, itemcol);
    inventoryPos.x += 2;
    sys_drawSprite(item.sprite, item.frame, inventoryPos);
    inventoryPos.x += 2;
  }
}


int _get_playerIndex(const FathomData* fathom)
{
  int i;
  for(i=0; i<MAX_ENTITIES; i++)
  {
    if(!fathom->entities[i].active)
      continue;
    if(!fathom->entities[i].player)
      continue;
    return i;
  }
  return -1;
}

bool _game_isBlind(const FathomData* fathom)
{
  bool blind = false;
  int i;
  for(i=0; i<MAX_ENTITIES; i++)
  {
    const Entity* e = &fathom->entities[i];
    if(!e->active)
      continue;
    if(!e->player)
      continue;
    blind = e->blindTimer > 0;
  }
  return blind;
}

void game_draw(const GameData* game, Point offset)
{
  const FathomData* fathom = &game->fathoms[game->current];
 
  bool blind = _game_isBlind(fathom);
  if(!blind)
    tilemap_draw(fathom->tileMap, offset);

  int i;
  for(i=0; i<MAX_ITEMS; i++)
  {
    const Item* item = &fathom->items[i];
    if(!item->active)
      continue;
    if(!tilemap_visible(&fathom->tileMap, item->pos))
      continue;
    Point drawPos = pointAddPoint(offset, item->pos);
    if(!blind)
      sys_drawSprite(item->sprite, item->frame, drawPos);
  }
  for(i=0; i<MAX_ENTITIES; i++)
  {
    const Entity* e = &fathom->entities[i];
    if(!e->active)
      continue;
    if(!tilemap_visible(&fathom->tileMap, e->pos))
      continue;
    Point drawPos = pointAddPoint(offset, e->pos);
    if(!blind || e->player)
      sys_drawSprite(e->sprite, e->frame, drawPos);
  }
  //if(sys_inputDown(INPUT_A))
  //  _draw_route(&fathom->tileMap, fathom->entities[0].pos, fathom->entities[1].pos, fathom->entities[0].sprite, fathom->entities[0].frame);

  Point messagePos = NULL_POINT;
  for(i=0; i<numMessages; i++)
  {    
    sys_drawString(messagePos, messages[i], TILEMAP_WIDTH, 1);
    messagePos.y++;
  }
  int playerIndex = _get_playerIndex(fathom);
  if(playerIndex != -1)
  {
    Point pos = offset;
    pos.y += TILEMAP_HEIGHT;
    _draw_hud(game, fathom->entities[playerIndex], pos);
  }
}

void _do_move(FathomData* fathom, Entity* e, Point move)
{
  if(game_hasCharm(e, CHARM_HASTE))
  {
    if(sys_randint(15)==0)
    {
      Direction d = sys_randint(4);
      move = directionToPoint(d);
    }
  }
  Point newPoint = pointAddPoint(e->pos, move);
  bool isWall = tilemap_collides(&fathom->tileMap, newPoint);
  bool isEntity = false;
  int i;
  for(i=0; i<MAX_ENTITIES; i++)
  {
    if(i==0)
      continue;
    Entity* victim = &fathom->entities[i];
    if(!victim->active)
      continue;
    if(newPoint.x != victim->pos.x)
      continue;
    if(newPoint.y != victim->pos.y)
      continue;
    
    int strength = e->strength;
    if(game_hasCharm(e, CHARM_BRUTE))
      strength += 6;    

    int amount = sys_randint(strength);
    if(amount > 0 && e->flags & EF_STEALS)
    {
      int space = -1;
      int j;
      for(j=0; j<MAX_INVENTORY; j++)
      {
        if(!e->inventory[j].active)
          space = j;
      }
      int slot = sys_randint(MAX_INVENTORY);
      if(victim->inventory[slot].active && space != -1)
      {
        e->inventory[space] = victim->inventory[slot];
        e->inventory[space].worn = false;
        Item nullItem = NULL_ITEM;
        victim->inventory[slot] = nullItem;
        amount = 0;
        game_addMessage(fathom, newPoint, "%s stole %s %s from %s", e->name, item_subtypeDescription(e->inventory[space].subtype), item_typeName(e->inventory[space].type), victim->name);
        e->scared = true;
        return;
      }      
    }

    bool killed = amount*10 >= victim->o2;
    if(amount == 0)
      game_addMessage(fathom, newPoint, "%s missed %s", e->name, victim->name);
    else if (killed)
      game_addMessage(fathom, newPoint, "%s killed %s", e->name, victim->name);
    else
      game_addMessage(fathom, newPoint, "%s hit %s", e->name, victim->name);

    if(amount > 2 && victim->flags & EF_INKY && !killed)
    {
      e->blindTimer = sys_randint(8)+5;
      game_addMessage(fathom, e->pos, "%s squirted %s with ink", victim->name, e->name);
      victim->flags &= ~EF_INKY;
    }

    if(killed)
    {
      e->xp += victim->xp;
      if(e->xp >= game_nextLevel(e->level))
      {
        e->level++;
        e->maxo2 += 10;
        e->o2 += 10;
        e->strength += 1;
        game_addMessage(fathom, e->pos, "%s leveled up", e->name);
      }
      if(victim->flags & EF_CONTAINSO2 && e->flags & EF_O2DEPLETES)
      {
        int boost = (sys_randint(5)+4)*5;
        e->o2 = min(e->o2 + boost, e->maxo2);
        game_addMessage(fathom, e->pos, "%s sucked down %d o2", e->name, boost);
      }
    }

    game_hurt(fathom, victim, amount*10);

    isEntity = true;
    break;
  }

  if(!isWall && !isEntity)
  {
    e->pos = newPoint;
    if(game_hasCharm(e, CHARM_BRUTE))
    {
      // stomp on seaweed
      Tile nullTile = NULL_TILE;
      int index = tilemap_indexFromTilePosition(&fathom->tileMap, e->pos);

      if(fathom->tileMap.tiles[index].type == TILE_HIDE)
        fathom->tileMap.tiles[index] = nullTile;
    }
  }
}

void _do_pickup(FathomData* fathom, Entity* e)
{
  Item nullItem = NULL_ITEM;

  int i;
  for(i=0; i<MAX_ITEMS; i++)
  {
    Item* item = &fathom->items[i];
    if(!item->active)
      continue;
    if(item->pos.x != e->pos.x || item->pos.y != e->pos.y)
      continue;

    int j;
    for(j=0; j<MAX_INVENTORY; j++)
    {
      if(!e->inventory[j].active)
      {
        e->inventory[j] = *item;
        game_addMessage(fathom, e->pos, "%s picked up %s %s", e->name, item_subtypeDescription(item->subtype), item_typeName(item->type));
        *item = nullItem;
        break;
      }
    }
    if(j==MAX_INVENTORY)
      game_addMessage(fathom, e->pos, "%s have no room for %s %s", e->name, item_subtypeDescription(item->subtype), item_typeName(item->type));
  }
}

bool _do_drop(FathomData* fathom, Entity* e, int index)
{
  Item nullItem = NULL_ITEM;
  Item item = e->inventory[index];
  item.pos = e->pos;
  if(!e->inventory[index].active)
  {
    LOG("Dropping non existent item");
    return false;
  }
  int empty = -1;
  int i;  
  for(i=0; i<MAX_ITEMS; i++)
  {
    if(!fathom->items[i].active)
    {
      empty = i;
      break;
    }
  }
  if(empty == -1)
  {
    game_addMessage(fathom, e->pos, "%s can't drop %s %s, too cluttered", e->name, item_subtypeDescription(item.subtype), item_typeName(item.type));
    return false;
  }
  item.worn = false;
  fathom->items[empty] = item;  
  e->inventory[index] = nullItem;
  game_addMessage(fathom, e->pos, "%s dropped %s %s", e->name, item_subtypeDescription(item.subtype), item_typeName(item.type));
  return true;
}

bool _do_use(FathomData* fathom, Entity* e, int index)
{
  Item* item = &e->inventory[index];
  if(!item->active)
  {
    LOG("Using non existent item");
    return false;
  }
  if(item->type == IT_CHARM)
  {
    item->worn = !item->worn;
    if(item->worn)
      game_addMessage(fathom, e->pos, "%s put on %s %s", e->name, item_subtypeDescription(item->subtype), item_typeName(item->type));
    else
      game_addMessage(fathom, e->pos, "%s took off %s %s", e->name, item_subtypeDescription(item->subtype), item_typeName(item->type));
    return true;
  }
  if(item->type == IT_CONCH)
  {
    game_addGlobalMessage("Point %s %s in which direction?", item_subtypeDescription(item->subtype), item_typeName(item->type));
    midFire = true;
    fireIndex = index;
    return false;
  }
  return false;
}

void _do_fire(GameData* game, Entity* e, int index, Direction direction)
{
  FathomData* fathom = &game->fathoms[game->current];
  Item* item = &e->inventory[index];
  Point vector = directionToPoint(direction);
  Point pos = pointAddPoint(e->pos, vector);
  int distance = 3 + sys_randint(3);
  int i;

  Entity nullEntity = NULL_ENTITY;

  switch(item->conchSubtype)
  {
    case CONCH_BUBBLE:
    {
      int spawnType = sys_randint(ET_MAX_ENEMY);
      for(i=0; i<distance; i++)
      {
        if(sys_randint(4)==0)
          game_spawnAt(fathom, spawn_entity(spawnType), pos);
        else
          game_spawnAt(fathom, spawn_entity(ET_BUBBLE), pos);
        pos = pointAddPoint(pos, vector);
      }
      break;
    }
    case CONCH_DIG:
    {
      Tile nullTile = NULL_TILE;
      for(i=0; i<distance; i++)
      {
        int index = tilemap_indexFromTilePosition(&fathom->tileMap, pos);
        if(index != -1)
          fathom->tileMap.tiles[index] = nullTile;
        pos = pointAddPoint(pos, vector);
      }
      break;
    }
    case CONCH_JUMP:
    {
      pos = pointAddPoint(pos, pointMultiply(vector, distance));
      Point invert = pointInverse(vector);
      for(i=distance-1; i>0; i--)
      {          
        if(game_pointFree(fathom, pos))
        {
          e->pos = pos;
          break;
        }
        pos = pointAddPoint(pos, invert);
      }
      break;
    }
    case CONCH_DEATH:
    {
      if(e->o2 > 4)
        e->o2 = e->o2/4;
      for(i=0; i<distance; i++)
      {
        int index = game_pointEntityIndex(fathom, pos);
        if(index != -1)
          fathom->entities[index] = nullEntity;
        pos = pointAddPoint(pos, vector);          
      }
      break;
    }
    case CONCH_POLYMORPH:
    {
      for(i=0; i<distance; i++)
      {
        int index = game_pointEntityIndex(fathom, pos);
        if(index != -1)
        {
          bool player = fathom->entities[index].player;
          fathom->entities[index] = nullEntity;
          Entity e = spawn_entity(sys_randint(ET_MAX_ENEMY));
          e.player = player;
          game_spawnAt(fathom, e, pos);
        }
        int j;
        for(j=0; j<MAX_ITEMS; j++)
        {
          Item* item = &fathom->items[j];
          if(!item->active)
            continue;
          if(pos.x != item->pos.x || pos.y != item->pos.y)
            continue;
          *item = spawn_item(game, item->type);
          item->pos = pos;
          item->active = true;
        }
        pos = pointAddPoint(pos, vector);
      }
      break;
    }
    case CONCH_MAPPING:
    {
      for(i=0; i<fathom->tileMap.size.x*fathom->tileMap.size.y; i++)
        fathom->tileMap.tiles[i].seen = true;
      break;
    }
    default:
      LOG("Trying to cast invalid conch");
      break;
  }
  game_addMessage(fathom, e->pos, "%s fires %s %s", e->name, item_subtypeDescription(item->subtype), item_typeName(item->type));
  if(sys_randint(5)==0)
  {
    Item nullItem = NULL_ITEM;
    game_addMessage(fathom, e->pos, "the %s %s falls apart", item_subtypeDescription(item->subtype), item_typeName(item->type));
    *item = nullItem;
  }
}

void _do_turn(FathomData* fathom, Entity* e)
{
  bool waterBreath = game_hasCharm(e, CHARM_WATERBREATH);
  if(waterBreath && e->o2 > e->maxo2/4)
    e->o2 = e->maxo2/4;
  if(e->flags & EF_O2DEPLETES && !waterBreath)
  {
    e->o2timer++;
    if(e->o2timer >= 5)
    {
      Entity copy = *e;
      if(game_hurt(fathom, e, 1))
        game_addMessage(fathom, copy.pos, "%s drowned", copy.name);
      e->o2timer = 0;
    }
  }

  if(e->player)
  {
    int i;
    for(i=0; i<MAX_ITEMS; i++)
    {
      Item* item = &fathom->items[i];
      if(!item->active)
        continue;
      if(e->pos.x != item->pos.x || e->pos.y != item->pos.y)
        continue;
      game_addMessage(fathom, item->pos, "%s see a %s %s", e->name, item_subtypeDescription(item->subtype), item_typeName(item->type));
    }
  }

  int turnAdd = e->speed+sys_randint(e->speed);
  if(game_hasCharm(e, CHARM_HASTE))
    turnAdd = (turnAdd*2)/3;

  if(e->blindTimer > 0)
    e->blindTimer--;

  e->turn += turnAdd;
}

void _game_dive(GameData* game, int entityIndex, int depth)
{ 
  Entity nullEntity = NULL_ENTITY; 
  FathomData* currentFathom = &game->fathoms[game->current];
  int newFathomIndex = game->current + depth;
  Entity e = currentFathom->entities[entityIndex];
  if(newFathomIndex < 0)
  {
    game_addGlobalMessage("%s are on the surface", e.name);
    return;
  }
  if(newFathomIndex >= MAX_FATHOMS)
  {
    game_addGlobalMessage("%s are on the bottom of the ocean", e.name);
    return;
  }
  currentFathom->entities[entityIndex] = nullEntity;
  int newDepth = game->current+depth;
  if(e.flags & EF_O2DEPLETES)
  {
    Entity copy = e;
    if(game_hurt(currentFathom, &e, 10))
    {
      game_addGlobalMessage("%s drowned while %s", copy.name, depth > 0 ? "diving" : "rising");
      return;
    }
  }
  game_spawnAt(&game->fathoms[newDepth], e, e.pos);

  if(e.player)
  {
    int i;
    for(i=0; i<MAX_ENTITIES; i++)
    {
      Entity *other = &currentFathom->entities[i];
      if(!other->active)
        continue;
      if(other->player)
        continue;
      if(!(other->flags & EF_SENTIENT))
        continue;
      if(!tilemap_visible(&currentFathom->tileMap, other->pos))
        continue;
      game_addGlobalMessage("%s follows %s %s", other->name, e.name, depth > 0 ? "down" : "up");
      _game_dive(game, i, depth); // join in      
    }
    game->current += depth;
  }
}

void _game_recalcFov(FathomData* fathom)
{
  int i;
  for(i=0; i<MAX_ENTITIES; i++)
  {
    int range = 9;
    if(game_hasCharm(&fathom->entities[i], CHARM_DARKNESS))
    {
      tilemap_forgetSeen(&fathom->tileMap);
      range = 4;
    }
    if(!fathom->entities[i].player)
      continue;
    if(_game_isBlind(fathom))
      range = 1;
    tilemap_recalcFov(&fathom->tileMap, fathom->entities[i].pos, range);
  }
}


Point _get_aiPath(const FathomData* fathom, Point start, Point end)
{
  Point move = NULL_POINT;
  _aStarFathom = fathom;
  _aStarStartPoint = start;
  _aStarFinishPoint = end;
  astar_t *as = astar_new(TILEMAP_WIDTH, TILEMAP_HEIGHT, _get_map_cost, NULL);
  astar_set_origin (as, 0, 0);
  astar_set_steering_penalty (as, 0);
  astar_set_movement_mode (as, DIR_CARDINAL);
  astar_run (as, start.x, start.y, end.x, end.y);
  if(astar_have_route(as))
  {
    direction_t * directions, * dir;
    astar_get_directions (as, &directions);    
    dir = directions;
    move.x += astar_get_dx(as, *dir);
    move.y += astar_get_dy(as, *dir);
    astar_free_directions (directions);
  }
  return move;
}

void _game_ai(GameData* game, Entity* e)
{
  FathomData* fathom = &game->fathoms[game->current];
  Point pos = e->pos;
  Point nullPoint = NULL_POINT;
  Point move = nullPoint;
  if(e->flags & EF_SENTIENT && e->blindTimer == 0)
  {
    int player =  _get_playerIndex(fathom);
    bool visible = tilemap_visible(&fathom->tileMap, pos);
    if(player != -1 && visible)
    {
      e->lastKnownPlayerPos = fathom->entities[player].pos;
      e->hunting = true;
    }
    if(e->hunting || e->scared)
    {
      move = _get_aiPath(fathom, pos, e->lastKnownPlayerPos);
      if(e->scared)
      {
        move = pointInverse(move);
        if(sys_randint(3)==0)
          move = nullPoint;
      }
      if(sys_randint(10)==0)
        e->hunting = false;
      if(sys_randint(30)==0)
        e->scared = false;
    }
  }
  if(move.x == 0 && move.y == 0)
  {
    int dir = sys_randint(4);
    move = directionToPoint(dir);
  }
  _do_move(fathom, e, move);
}

bool _game_player(GameData* game, Entity* e)
{
  FathomData* fathom = &game->fathoms[game->current];
  if(midFire)
  {
    int i=0;
    for(i=0; i<4; i++)
    {
      if(sys_inputPressed(INPUT_UP+i))
      {
        midFire = false;
        _do_fire(game, e, fireIndex, i);
        return true;
      }      
    }
    if(sys_inputPressed(INPUT_ANY))
    {
      game_addGlobalMessage("Nevermind.");
      midFire = false;
      return false;
    }
  }
  if(midDrop || midUse)
  {
    int i=0;
    for(i=0; i<MAX_INVENTORY; i++)
    {
      if(sys_inputPressed(INPUT_1 + i))
      {
        if(!e->inventory[i].active)
        {
          midDrop = false;
          midUse = false;
          game_addGlobalMessage("No item in slot %d. Nevermind.", i);
          return false;
        }
        bool didAction = false;
        if(midDrop)
          didAction = _do_drop(fathom, e, i);
        if(midUse)
          didAction = _do_use(fathom, e, i);
        midDrop = false;
        midUse = false;
        return didAction;
      }
    }
    if(sys_inputPressed(INPUT_ANY))
    {
      game_addGlobalMessage("Nevermind.");
      midDrop = false;
      midUse = false;      
    }
    return false;
  }
  if(sys_inputPressed(INPUT_ESC))
    numMessages = 0;
  Point move = NULL_POINT;
  if(sys_inputPressed(INPUT_UP))
    move.y--;
  if(sys_inputPressed(INPUT_RIGHT))
    move.x++;
  if(sys_inputPressed(INPUT_DOWN))
    move.y++;
  if(sys_inputPressed(INPUT_LEFT))
    move.x--;
  if(move.x != 0 || move.y != 0)
  {
    numMessages = 0;
    _do_move(fathom, e, move);
    return true;
  }
  if(sys_inputPressed(INPUT_DIVE))
  {
    numMessages = 0;
    _game_dive(game, 0, 1);
    return true;
  }
  if(sys_inputPressed(INPUT_RISE))
  {
    numMessages = 0;
    _game_dive(game, 0, -1);
    return true;
  }
  if(sys_inputPressed(INPUT_PICKUP))
  {
    numMessages = 0;
    _do_pickup(fathom, e);
    return true;
  }
  if(sys_inputPressed(INPUT_DROP))
  {
    numMessages = 0;
    game_addGlobalMessage("Drop which item %d-%d?", 1, MAX_INVENTORY+1);
    midDrop = true;
  }
  if(sys_inputPressed(INPUT_USE))
  {
    numMessages = 0;
    game_addGlobalMessage("Use which item %d-%d?", 1, MAX_INVENTORY+1);
    midUse = true;
  }
  return false;
}

bool game_update(GameData* game)
{
  FathomData* fathom = &game->fathoms[game->current];
  Entity* e = &fathom->entities[0];
  if(!e->active)
    return true;
  bool doTurn = !e->player;
  if(e->player)
    doTurn = _game_player(game, e);
  else
    _game_ai(game, e);

  if(doTurn)
  {    
    _do_turn(fathom, e);
    _game_sortEntities(fathom);
    _game_recalcFov(fathom);
  }
  int i;
  bool anyPlayer = false;
  for(i=0; i<MAX_ENTITIES; i++)
  {
    if(fathom->entities[i].player && fathom->entities[i].active)
      anyPlayer = true;
  }
  return fathom->entities[0].player || !anyPlayer;
}

void game_addMessage(const FathomData* fathom, Point p, const char *str, ...)
{
  if(tilemap_visible(&fathom->tileMap, p))
  {
    if(numMessages >= TILEMAP_HEIGHT)
    {
      LOG("Not enough space for messages.");
      return;
    } 
    va_list args;
    va_start(args, str);
    vsnprintf(messages[numMessages], TILEMAP_WIDTH, str, args);
    numMessages++;
  }
}

void game_addGlobalMessage(const char *str, ...)
{
  if(numMessages >= TILEMAP_HEIGHT)
  {
    LOG("Not enough space for messages.");
    return;
  }
  va_list args;
  va_start(args, str);
  vsnprintf(messages[numMessages], TILEMAP_WIDTH, str, args);
  numMessages++;
}
