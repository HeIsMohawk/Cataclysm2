#include "item.h"
#include "item_type.h"
#include "game.h"
#include "globals.h"
#include "cuss.h"
#include "entity.h"
#include "attack.h"
#include "files.h"    // For CUSS_DIR
#include "rng.h"
#include <sstream>

Item::Item(Item_type* T)
{
  type = T;
  count = 1;
  ammo = NULL;
  charges = 0;
  subcharges = 0;
  hp = 100;   // TODO: Use Item_type durability instead
  active = false;
  if (type) {
    uid = GAME.get_item_uid();
    charges = type->default_charges();
    if (charges > 0 && get_item_class() == ITEM_CLASS_TOOL) {
// Tools have subcharges, so set that up
      Item_type_tool* tool = static_cast<Item_type_tool*>(type);
      subcharges = tool->subcharges;
    }
    if (type->volume < 100) {
      hp = type->volume;
    }
    if (hp < 5) {
      hp = 5;
    }
  } else {
    uid = -1;
  }
}

Item::Item(const Item &rhs)
{
  type        = rhs.type;
  count       = rhs.count;
  uid         = rhs.uid;
  ammo        = rhs.ammo;
  charges     = rhs.charges;
  subcharges  = rhs.subcharges;
  active     = rhs.active;

  if (!rhs.contents.empty()) {
    contents.clear();
    for (int i = 0; i < rhs.contents.size(); i++) {
      contents.push_back( rhs.contents[i] );
    }
  }
}

Item::~Item()
{
}

Item& Item::operator=(const Item& rhs)
{
  type       = rhs.type;
  ammo       = rhs.ammo;
  count      = rhs.count;
  charges    = rhs.charges;
  subcharges = rhs.subcharges;
  uid        = rhs.uid;    // Will this cause bugs?

  if (!rhs.contents.empty()) {
    contents.clear();
    for (int i = 0; i < rhs.contents.size(); i++) {
      contents.push_back( rhs.contents[i] );
    }
  }

  return *this;
}

Item_class Item::get_item_class()
{
  if (!type) {
    return ITEM_CLASS_MISC;
  }
  return type->get_class();
}

Item_action Item::default_action()
{
  if (!type) {
    return IACT_NULL;
  }
  return type->default_action();
}

bool Item::is_real()
{
  return type;
}

bool Item::can_reload()
{
// TODO: Reloadable tools
  return get_item_class() == ITEM_CLASS_LAUNCHER;
}

int Item::time_to_reload()
{
  if (!can_reload() || !is_real()) {
    return 0;
  }
  return type->time_to_reload();
}

int Item::time_to_fire()
{
  if (!is_real()) {
    return 0;
  }
  return type->time_to_fire();
}

int Item::get_uid()
{
  if (!is_real()) {
    return -1;
  }
  return uid;
}

glyph Item::top_glyph()
{
  if (type) {
    return type->sym;
  }
  return glyph();
}

std::string Item::get_data_name()
{
  if (type) {
    return type->get_data_name();
  }
  return "Typeless item";
}

std::string Item::get_name()
{
  if (type) {
    return type->get_name();
  }
  return "Typeless item";
}

std::string Item::get_name_indefinite()
{
// TODO: Unique items?
  std::string article = (has_flag(ITEM_FLAG_PLURAL) ? "some" : "a");
  if (type) {
    std::stringstream ret;
    switch (type->get_class()) {
      case ITEM_CLASS_AMMO:
        ret << "a box of " << type->name; // TODO: Not always box?
        break;
      default:
        ret << article << " " << type->name;
    }
// Display FULL info on contained items
    if (!contents.empty()) {
      ret << " of " << contents[0].get_name_full();
    }
    return ret.str();
  }
  return "a typeless item";
}

std::string Item::get_name_definite()
{
// TODO: Check Item_type for "plural" flag
// TODO: Unique items?
  if (type) {
    std::stringstream ret;
    ret << "the " << type->name;
    return ret.str();
  }
  return "the typeless item";
}

std::string Item::get_name_full()
{
  if (!type) {
    return "typeless item (0)";
  }
  std::stringstream ret;
  ret << get_name();
  if (!contents.empty()) {
    ret << " of " << contents[0].get_name_full();
  }

// Display the number of charges for items that use them
  if (type->uses_charges()) {
    ret << " (" << charges << ")";
  }
    
  if (active) {
    ret << " <c=yellow>[on]<c=/>";
  }

  return ret.str();
}

std::string Item::get_description()
{
  if (type) {
    return type->description;
  }
  return "Undefined item - type is NULL (this is a bug)";
}

std::string Item::get_description_full()
{
  if (!type) {
    return "Undefined item - type is NULL (this is a bug)";
  }
  std::stringstream ret;
  ret << type->description << std::endl << std::endl <<
         type->get_property_description();

  return ret.str();
}

int Item::get_weight()
{
  if (!type) {
    return 0;
  }
  if (get_item_class() == ITEM_CLASS_AMMO) {
    return (charges * type->weight) / 100;
  }
  if (has_flag(ITEM_FLAG_LIQUID) ||
      (get_item_class() == ITEM_CLASS_FOOD && !has_flag(ITEM_FLAG_CONSTANT))) {
    return (charges * type->weight);
  }
  return type->weight * count;
}

int Item::get_volume()
{
  if (!type) {
    return 0;
  }
  if (get_item_class() == ITEM_CLASS_AMMO) {
    Item_type_ammo* ammo = static_cast<Item_type_ammo*>(type);
    return (charges * type->volume) / ammo->count;
  }
  if (has_flag(ITEM_FLAG_LIQUID) ||
      (get_item_class() == ITEM_CLASS_FOOD && !has_flag(ITEM_FLAG_CONSTANT))) {
    return (charges * type->volume);
  }

/* Some containers, like a plastic bag, can hold more than their own volume
 * (since when they're empty you can wad them up).  If that's the case, return
 * the total volume of its contents instead!
 */
  if (get_item_class() == ITEM_CLASS_CONTAINER) {
    int own_volume = type->volume;
    int contents_volume = get_volume_capacity_used();
    if (own_volume > contents_volume) {
      return own_volume;
    }
    return contents_volume;
  }

// Everything else just returns its type's volume.
  return type->volume;
}

int Item::get_volume_capacity()
{
  if (!is_real()) {
    return 0;
  }

  if (get_item_class() == ITEM_CLASS_CONTAINER) {
    Item_type_container* container = static_cast<Item_type_container*>(type);
    return container->capacity;
  }

/* Even though "volume capacity" means something totally different for
 * containers and clothing, put clothing in this function too.  It makes sense,
 * and there shouldn't be any overlap.
 */
  if (get_item_class() == ITEM_CLASS_CLOTHING) {
    Item_type_clothing* clothing = static_cast<Item_type_clothing*>(type);
    return clothing->carry_capacity;
  }

// We're neither a container nor clothing, return 0.
  return 0;
}

int Item::get_volume_capacity_used()
{
  int ret = 0;
  for (int i = 0; i < contents.size(); i++) {
    ret += contents[i].get_volume();
  }
  return ret;
}

bool Item::has_flag(Item_flag itf)
{
  if (!type) {
    return false;
  }
  return type->has_flag(itf);
}

bool Item::covers(Body_part part)
{
  if (!is_real() || get_item_class() != ITEM_CLASS_CLOTHING) {
    return false;
  }
  Item_type_clothing* clothing = static_cast<Item_type_clothing*>(type);
  return clothing->covers[part];
}

int Item::get_damage(Damage_type dtype)
{
  if (type) {
    return type->damage[dtype];
  }
  return 0;
}

int Item::get_to_hit()
{
  if (type) {
    return type->to_hit;
  }
  return 0;
}

int Item::get_base_attack_speed()
{
  Stats stats;
  return get_base_attack_speed(stats);
}

int Item::get_base_attack_speed(Stats stats)
{
  if (!type) {
    return 0;
  }
// TODO: Do we really need Item_type::attack_speed?
  int ret = 50 + type->attack_speed;

  int min_weight_penalty = stats.strength;
  int penalty_per_pound  = (2 * (20 - stats.strength) ) / 3;
  int wgt = get_weight();
  if (stats.strength < 20 && wgt >= min_weight_penalty) {
    wgt -= min_weight_penalty;
// Divide by 10 since the penalty is per pound - 1 unit of weight is 0.1 lbs
    ret += (wgt * penalty_per_pound) / 10;
  }

// TODO: Tweak this section - this is very guess-y.
  int min_volume_penalty = stats.dexterity * 3;
  int penalty_per_10_volume = (2 * (20 - stats.dexterity) ) / 3;
  int vol = get_volume();
  if (stats.dexterity < 20 && vol >= min_volume_penalty) {
    vol -= min_volume_penalty;
    ret += (vol * penalty_per_10_volume) / 10;
  }

  return ret;
}

int Item::get_max_charges()
{
  if (get_item_class() == ITEM_CLASS_LAUNCHER) {
    Item_type_launcher* launcher = static_cast<Item_type_launcher*>(type);
    return launcher->capacity;
  }
  return 0;
}

bool Item::combines()
{
  if (!is_real()) {
    return false;
  }
  return type->always_combines();
}

bool Item::combine_by_charges()
{
  if (!is_real()) {
    return false;
  }
  return type->combine_by_charges();
}

// TODO: Use stats & skills.
Ranged_attack Item::get_thrown_attack()
{
  Ranged_attack ret;
  if (!type) {
    return ret;
  }
// If the ranged speed is 0, then set it based on our weight
  if (type->thrown_speed == 0) {
    ret.speed = 50 + 5 * type->weight;
    ret.speed += type->volume / 10;
  } else {
    ret.speed = type->thrown_speed;
  }
// Copy variance from type
  ret.variance = type->thrown_variance;
// Add variance for heavy items
  Dice extra_variance;
  extra_variance.number = 1 + type->weight / 20;
  extra_variance.sides  = type->volume / 5;
  ret.variance += extra_variance;
// Copy damage; note that thrown_dmg_percent defaults to 50
  for (int i = 0; i < DAMAGE_MAX; i++) {
    ret.damage[i] = (type->damage[i] * type->thrown_dmg_percent) / 100;
  }
  return ret;
}

// TODO: Use stats & skills.
Ranged_attack Item::get_fired_attack()
{
  if (!type || get_item_class() != ITEM_CLASS_LAUNCHER || charges <= 0 ||
      !ammo) {
    return Ranged_attack();
  }

  Item_type_launcher* launcher = static_cast<Item_type_launcher*>(type);
  Item_type_ammo*     itammo   = static_cast<Item_type_ammo*>    (ammo);

  Ranged_attack ret;
  ret.speed = launcher->fire_ap;
  ret.range = itammo->range;
  ret.variance = launcher->accuracy + itammo->accuracy;
// TODO: Can fired items ever be non-pierce?
  ret.damage       [DAMAGE_PIERCE] = itammo->damage + launcher->damage;
  ret.armor_divisor[DAMAGE_PIERCE] = itammo->armor_pierce;

  return ret;
}

bool Item::combine_with(const Item& rhs)
{
// TODO: Handle combining items of different damage levels etc.
  if (!combines()) {
    return false;
  }
  if (type != rhs.type) {
    return false;
  }
  if (combine_by_charges()) {
    charges += rhs.charges;
  } else {
    count += rhs.count;
  }
  return true;
}

bool Item::place_in_its_container()
{
  if (is_real() && get_item_class() == ITEM_CLASS_FOOD) {
    Item_type_food* food = static_cast<Item_type_food*>(type);
    if (food->container.empty()) {
      return false;
    }
    Item_type* container_type = ITEM_TYPES.lookup_name( food->container );
    if (container_type) {
      Item tmp(container_type);
      if (!tmp.add_contents(*this)) {
        debugmsg("%s could not be placed in its container (%s)",
                 get_data_name().c_str(), food->container.c_str());
        return false;
      }
      *this = tmp;
      return true;
    } else {
      debugmsg("%s has non-existant container '%s'", get_data_name().c_str(),
               food->container.c_str());
    }
  }
  return false;
}

bool Item::add_contents(Item it)
{
// TODO: Require water-tight containers for liquids?
  if (!is_real() || get_item_class() != ITEM_CLASS_CONTAINER ||
      !it.is_real()) {
    return false;
  }
  if (get_volume_capacity_used() + it.get_volume() > get_volume_capacity()) {
    return false;
  }
  contents.push_back(it);
  return true;
}

bool Item::reload(Entity* owner, int ammo_uid)
{
  if (!owner) {
    return false;
  }
  Item* ammo_used = owner->ref_item_uid(ammo_uid);
  if (!ammo_used) {
    return false;
  }
  int charges_available = get_max_charges() - charges;
  if (charges_available <= 0) {
    return false;
  }
  if (ammo_used->charges > charges_available) {
    ammo_used->charges -= charges_available;
    charges = get_max_charges();
  } else {
    charges += ammo_used->charges;
    owner->remove_item_uid(ammo_uid);
  }
  ammo = ammo_used->type;
  if (get_item_class() == ITEM_CLASS_TOOL) {  // Need to set up subcharges too
    Item_type_tool* tool = static_cast<Item_type_tool*>(type);
    subcharges = tool->subcharges;
  }
  return true;
}

bool Item::damage(int dam)
{
  if (dam < 0) {
    return false;
  }
  hp -= dam;
  if (hp <= 0) {
    return true;
  }
  return false;
}

bool Item::absorb_damage(Damage_type damtype, int dam)
{
  if (!is_real() || get_item_class() == ITEM_CLASS_CLOTHING) {
    return false; // Don't do anything
  }
  Item_type_clothing* clothing = static_cast<Item_type_clothing*>(type);
/* TODO:  Item_type_clothing should, eventually, use a Damage_set for its armor
 *        ratings.  Once it does, the following can be simplified.
 */
  int armor = 0;
  switch (damtype) {
    case DAMAGE_BASH:
      armor = clothing->armor_bash;
      break;
    case DAMAGE_CUT:
      armor = clothing->armor_cut;
      break;
    case DAMAGE_PIERCE:
      armor = clothing->armor_pierce;
      break;
  }

  armor = rng(0, armor);  // Absorb some of the damage!
  int damage_to_item = armor;
  if (dam < armor) {
    damage_to_item = dam;
  }
  damage_to_item = dice(2, damage_to_item + 1) - 2;
// Reduce the damage by the amount of the armor used
  dam -= armor;
  return damage(damage_to_item);
}

bool Item::power_on()
{
// TODO: Can we power on other classes?
  if (!is_real() || get_item_class() != ITEM_CLASS_TOOL) {
    return false;
  }
  if (charges == 0 && subcharges == 0) {
    return false;
  }
  if (active) {
    return false;
  }
  GAME.add_active_item(this);
  active = true;
  return true;
}

bool Item::power_off()
{
  if (!is_real() || !active) {
    return false;
  }
  active = false;
  GAME.remove_active_item(this);
  return true;
}

bool Item::process_active()
{
// TODO: Can we power on other classes?
  if (!is_real() || !active || get_item_class() != ITEM_CLASS_TOOL) {
    return false;
  }
  Item_type_tool* tool = static_cast<Item_type_tool*>(type);
  subcharges--;
  if (subcharges <= 0) {
    charges--;
    if (charges >= 0) {
      subcharges += tool->subcharges;
    } else {
      bool destroy = tool->powered_action.destroy_if_chargeless;
// We're truly out of power!
      charges = 0;
      subcharges = 0;
// If we're on a timer, activate the timer function!
      bool did_countdown = false;
      if (tool->countdown_action.real) {
        tool->countdown_action.activate(this);
        destroy |= tool->countdown_action.destroy_if_chargeless;
        did_countdown = true;
      }
      power_off();  // This removes us from Game::active_items
// We have to destroy the item AFTER removing it from Game:active_items!
      if (destroy) {
        GAME.destroy_item(this);
      }
      return did_countdown;
    }
  }

// Finally, do what we're meant to do while active.
  if (tool->powered_action.real) {
    tool->powered_action.activate(this);
  }
  return true;
}

Item_action Item::show_info(Entity* user)
{
  if (!type) {
    return IACT_NULL;
  }
  Window w_info(0, 0, 80, 24);
  cuss::interface i_info;
  if (!i_info.load_from_file(CUSS_DIR + "/i_item_info.cuss")) {
    return IACT_NULL;
  }

  i_info.set_data("item_name",  get_name());
  i_info.set_data("num_weight", get_weight());
  i_info.set_data("num_volume", get_volume());
  i_info.set_data("num_bash",   get_damage(DAMAGE_BASH));
  i_info.set_data("num_cut",    get_damage(DAMAGE_CUT));
  i_info.set_data("num_pierce", get_damage(DAMAGE_PIERCE));
  i_info.set_data("num_to_hit", get_to_hit());
  if (user) {
    i_info.set_data("num_speed",  get_base_attack_speed(user->stats));
  } else {
    i_info.set_data("num_speed",  get_base_attack_speed());
  }
// get_desciption_full() includes type-specific info, e.g. nutrition for food
  i_info.set_data("description", get_description_full());
  i_info.draw(&w_info);
  while (true) {
    long ch = input();
    if (ch == 'd' || ch == 'D') {
      return IACT_DROP;
    } else if (ch == 'w') {
      return IACT_WIELD;
    } else if (ch == KEY_ESC || ch == 'q' || ch == 'Q') {
      return IACT_NULL;
    }
  }
  return IACT_NULL;
}

std::string list_items(std::vector<Item> *items)
{
  if (items->empty()) {
    return "nothing.";
  }
  std::stringstream item_text;
  for (int i = 0; i < items->size(); i++) {
    item_text << (*items)[i].get_name_indefinite();
    if (i == items->size() - 2) {
      item_text << " and ";
    } else if (items->size() >= 3 && i < items->size() - 2) {
      item_text << ", ";
    }
  }
  item_text << ".";
  return item_text.str();
}
