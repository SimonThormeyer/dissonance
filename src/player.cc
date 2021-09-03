#include <cmath>
#include <cwchar>
#include <math.h>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <vector>

#include "codes.h"
#include "curses.h"
#include "field.h"
#include "player.h"
#include "units.h"
#include "codes.h"
#include "utils.h"

#define HILL ' ' 
#define DEN 'D' 
#define GOLD 'G' 
#define SILVER 'S' 
#define BRONZE 'B' 
#define FREE char(46)
#define DEF 'T'

Player::Player(Position nucleus_pos, int iron) : cur_range_(4), resource_curve_(3), 
    oxygen_boast_(0), max_oxygen_(100), max_resources_(70),  total_oxygen_(5.5){
  nucleus_ = Nucleus(nucleus_pos); 
  all_neurons_.insert(nucleus_pos);
  all_nucleus_[nucleus_pos] = nucleus_;
  resources_ = {
    { Resources::IRON, {iron, false}},
    { Resources::OXYGEN, {5.5, true}},
    { Resources::POTASSIUM, {0, false}},
    { Resources::CHLORIDE, {0, false}},
    { Resources::GLUTAMATE, {0, false}},
    { Resources::DOPAMINE, {0, false}},
    { Resources::SEROTONIN, {0, false}},
  };
  technologies_ = {
    {UnitsTech::WAY, {0,3}},
    {UnitsTech::SWARM, {0,3}},
    {UnitsTech::TARGET, {0,2}},
    {UnitsTech::TOTAL_OXYGEN, {0,3}},
    {UnitsTech::TOTAL_RESOURCE, {0,3}},
    {UnitsTech::CURVE, {0,2}},
    {UnitsTech::ATK_POTENIAL, {0,3}},
    {UnitsTech::ATK_SPEED, {0,3}},
    {UnitsTech::ATK_DURATION, {0,3}},
    {UnitsTech::DEF_POTENTIAL, {0,3}},
    {UnitsTech::DEF_SPEED, {0,3}},
    {UnitsTech::NUCLEUS_RANGE, {0,3}},
  };
}

std::vector<std::string> Player::GetCurrentStatusLine() {
  std::scoped_lock scoped_lock(mutex_resources_);
  std::string end = ": ";
  return { 
    "RESOURCES",
    "",
    "Iron " SYMBOL_IRON + end, std::to_string(resources_.at(Resources::IRON).first), "",
    "oxygen boast: ", std::to_string(oxygen_boast_), "",
    "resource curve slowdown: ", std::to_string(resource_curve_), "",
    "total oxygen " SYMBOL_OXYGEN + end, std::to_string(total_oxygen_), "",
    "bound oxygen: ", std::to_string(bound_oxygen_), "",
    "oxygen: ", std::to_string(resources_.at(Resources::OXYGEN).first), "",
    "potassium " SYMBOL_POTASSIUM + end, std::to_string(resources_.at(Resources::POTASSIUM).first), "",
    "chloride " SYMBOL_CHLORIDE + end, std::to_string(resources_.at(Resources::CHLORIDE).first), "",
    "glutamate " SYMBOL_GLUTAMATE + end, std::to_string(resources_.at(Resources::GLUTAMATE).first), "",
    "dopamine " SYMBOL_DOPAMINE + end, std::to_string(resources_.at(Resources::DOPAMINE).first), "",
    "serotonin " SYMBOL_SEROTONIN + end, std::to_string(resources_.at(Resources::SEROTONIN).first), "",
    "nucleus " SYMBOL_DEN " potential" + end, std::to_string(nucleus_.lp_) + "/" + std::to_string(nucleus_.max_lp_),
  };
}

// getter 
std::map<std::string, Potential> Player::potential() { 
  std::shared_lock sl(mutex_potentials_);
  return potential_; 
}
std::map<std::string, Potential> Player::epsps() { 
  std::shared_lock sl(mutex_potentials_);
  std::map<std::string, Potential> epsps;
  for (auto it : potential_) {
    if (it.second.type_ == UnitsTech::EPSP)
      epsps[it.first] = it.second;
  }
  return epsps; 
}

std::map<std::string, Potential> Player::ipsps() { 
  std::shared_lock sl(mutex_potentials_);
  std::map<std::string, Potential> ipsps;
  for (auto it : potential_) {
    if (it.second.type_ == UnitsTech::IPSP)
      ipsps[it.first] = it.second;
  }
  return ipsps; 
}

std::map<Position, Nucleus> Player::all_nucleus() { 
  std::shared_lock sl(mutex_all_neurons_);
  return all_nucleus_; 
}
std::map<Position, Synapse> Player::synapses() { 
  std::shared_lock sl(mutex_all_neurons_);
  return synapses_; 
}

std::map<Position, ActivatedNeuron> Player::activated_neurons() { 
  std::shared_lock sl(mutex_all_neurons_);
  return activated_neurons_; 
}

std::set<Position> Player::neurons() {
  std::shared_lock sl(mutex_all_neurons_);
  return all_neurons_;
}

Position Player::nucleus_pos() { 
  std::shared_lock sl(mutex_nucleus_); 
  return nucleus_.pos_;
}
int Player::nucleus_potential() { 
  std::shared_lock sl(mutex_nucleus_); 
  return nucleus_.lp_;
}
int Player::cur_range() { 
  return cur_range_;
}

int Player::resource_curve() {
  return resource_curve_;
}

int Player::iron() {
  std::shared_lock sl(mutex_resources_);
  return resources_.at(Resources::IRON).first;
}

int Player::bound_oxygen() {
  std::shared_lock sl(mutex_resources_);
  return bound_oxygen_;
}

int Player::max_oxygen() {
  std::shared_lock sl(mutex_resources_);
  return max_oxygen_;
}

int Player::oxygen_boast() {
  std::shared_lock sl(mutex_resources_);
  return oxygen_boast_;
}

std::map<int, Resource> Player::resources() {
  std::shared_lock sl(mutex_resources_);
  return resources_;
}

std::map<int, TechXOf> Player::technologies() {
  std::shared_lock sl(mutex_technologies_);
  return technologies_;
}

// setter 
void Player::set_resource_curve(int resource_curve) {
  std::unique_lock ul(mutex_resources_);
  resource_curve_ = resource_curve;
}

void Player::set_iron(int iron) {
  std::unique_lock ul(mutex_resources_);
  resources_[Resources::IRON].first = iron;
}

// methods 

bool Player::HasLost() {
  std::shared_lock sl(mutex_nucleus_);
  return nucleus_.lp_ >= nucleus_.max_lp_;
}

void Player::ResetWayForSynapse(Position pos, Position way_position) {
  std::unique_lock ul(mutex_all_neurons_);
  synapses_.at(pos).ways_ = {way_position};
}

void Player::AddWayPosForSynapse(Position pos, Position way_position) {
  std::unique_lock ul(mutex_all_neurons_);
  synapses_.at(pos).ways_.push_back(way_position);
}

void Player::SwitchSwarmAttack(Position pos) {
  std::unique_lock ul(mutex_all_neurons_);
  synapses_.at(pos).swarm_ = !synapses_.at(pos).swarm_;
}

void Player::ChangeIpspTargetForSynapse(Position pos, Position target_pos) {
  std::unique_lock ul(mutex_all_neurons_);
  synapses_.at(pos).ipsp_target_ = target_pos;
}

void Player::ChangeEpspTargetForSynapse(Position pos, Position target_pos) {
  std::unique_lock ul(mutex_all_neurons_);
  synapses_.at(pos).epsp_target_ = target_pos;
}


double Player::Faktor(int limit, double cur) {
  return (limit-cur)/(resource_curve_*limit);
}

void Player::IncreaseResources() {
  std::unique_lock ul_resources(mutex_resources_);
  for (auto& it : resources_) {
    int cur_oxygen = resources_[Resources::OXYGEN].first;
    if (it.first == Resources::IRON) 
      continue;
    // Add oxygen based on oxygen-boast and current oxygen.
    else if (it.first == Resources::OXYGEN) {
      it.second.first += oxygen_boast_* Faktor(max_oxygen_, total_oxygen_) * 1;
      total_oxygen_ = bound_oxygen_ + it.second.first;
    }
    // Add other resources based on current oxygen.
    else if (it.second.second)
      it.second.first += log(cur_oxygen+1) * Faktor(max_resources_, it.second.first) * 1;

    // Add iron (every 2.5sec and randomly.
    auto cur_time = std::chrono::steady_clock::now();
    if (utils::get_elapsed(last_iron_, cur_time) > 2000) {
      if (utils::getrandom_int(0, cur_oxygen*0.65) == 0)
        resources_[Resources::IRON].first++;
      last_iron_ = cur_time;
    }
  }
}

bool Player::DistributeIron(int resource) {
  std::unique_lock ul(mutex_resources_);
  if (resources_.count(resource) == 0) {
    return false;
  }
  else if (resource == Resources::OXYGEN) {
    if (resources_.at(Resources::IRON).first > 0) {
      oxygen_boast_++;
      resources_[Resources::IRON].first--;
    }
    else 
      return false;
  }
  else if (resources_[Resources::IRON].first < 2 || resources_[resource].second) {
    return false;
  }
  else {
    resources_[resource].second = true;
    resources_[Resources::IRON].first-=2;
  }
  return true;
}

Costs Player::GetMissingResources(int unit, int boast) {
  std::shared_lock sl(mutex_resources_);
  // Get costs for desired unit
  Costs needed = units_costs_.at(unit);

  // Check costs and add to missing.
  std::map<int, double> missing;
  for (const auto& it : needed) {
    if (resources_.at(it.first).first < it.second*boast) 
      missing[it.first] = it.second - resources_.at(it.first).first;
  }
  return missing;
}

void Player::TakeResources(Costs costs, int boast) {
  std::unique_lock ul_resources(mutex_resources_);
  for (const auto& it : costs) {
    resources_[it.first].first -= it.second*boast;
    if (it.first == Resources::OXYGEN)
      bound_oxygen_ += it.second;
  }
}

void Player::AddNeuron(Position pos, int neuron, Position epsp_target, Position ipsp_target) {
  std::unique_lock ul(mutex_all_neurons_);
  TakeResources(units_costs_.at(neuron));
  all_neurons_.insert(pos);

  if (neuron == UnitsTech::ACTIVATEDNEURON) {
    std::shared_lock sl(mutex_technologies_);
    int speed_boast = technologies_.at(UnitsTech::DEF_SPEED).first * 40;
    int potential_boast = technologies_.at(UnitsTech::DEF_POTENTIAL).first;
    activated_neurons_[pos] = ActivatedNeuron(pos, potential_boast, speed_boast);
  }
  else if (neuron == UnitsTech::SYNAPSE)
    synapses_[pos] = Synapse(pos, technologies_.at(UnitsTech::SWARM).first*3+1, 
        technologies_.at(UnitsTech::WAY).first, epsp_target, ipsp_target);
  else if (neuron == UnitsTech::NUCLEUS) {
    all_nucleus_[pos] = Nucleus(pos);
    std::shared_lock sl(mutex_resources_);
    max_oxygen_ += 10;
    max_resources_ += 10;
  }
}

void Player::AddPotential(Position synapes_pos, Field* field, int unit) {
  TakeResources(units_costs_.at(unit));

  // Get way and target:
  std::shared_lock sl_neurons(mutex_all_neurons_);
  auto synapse = synapses_.at(synapes_pos);
  // Check if synapses is blocked.
  if (synapse.blocked_)
    return;
  // Create way.
  auto way_points = synapse.ways_;
  if (unit == UnitsTech::EPSP)
    way_points.push_back(synapse.epsp_target_);
  else 
    way_points.push_back(synapse.ipsp_target_);
  std::list<Position> way = field->GetWayForSoldier(synapes_pos, way_points);

  // Add potential.
  std::unique_lock ul_potentials(mutex_potentials_);
  std::shared_lock sl_technologies(mutex_technologies_);
  // Get boast from technologies.
  int potential_boast = technologies_.at(UnitsTech::ATK_POTENIAL).first;
  int speed_boast = technologies_.at(UnitsTech::ATK_POTENIAL).first;
  int duration_boast = technologies_.at(UnitsTech::ATK_DURATION).first;
  sl_technologies.unlock();
  // Add Epsp:
  if (unit == UnitsTech::EPSP) {
    // If swarm attack is activated and maximum capacity is reached, create epsps.
    if (synapse.swarm_) {
      synapses_[synapes_pos].stored_++;
      if (synapses_.at(synapes_pos).stored_ >= synapses_.at(synapes_pos).max_stored_) {
        while (synapses_.at(synapes_pos).stored_-- > 0)
          potential_[utils::create_id("epsp")] = Epsp(synapes_pos, way, potential_boast, speed_boast);
      }
    }
    // Otherwise simply create epsp 
    else {
      potential_[utils::create_id("epsp")] = Epsp(synapes_pos, way, potential_boast, speed_boast);
    }
  }
  // Add Ipsp:
  else if (unit == UnitsTech::IPSP)
    potential_[utils::create_id("ipsp")] = Ipsp(synapes_pos, way, potential_boast, speed_boast, duration_boast);
}

bool Player::AddTechnology(int technology) {
  std::unique_lock ul(mutex_technologies_);
  std::unique_lock ul_resources(mutex_resources_);
  ul_resources.unlock();

  // Check if technology exists
  if (technologies_.count(technology) == 0)
    return false;
  
  // Check if missing resources
  if (GetMissingResources(technology, technologies_[technology].first+1).size() > 0)
    return false;
  
  // Check if is already fully researched
  if (technologies_[technology].first == technologies_[technology].second)
    return false;
 
  // Handle technology.
  technologies_[technology].first++;
  if (technology == UnitsTech::WAY)
    for (auto& it : synapses_) it.second.availible_ways_ = technologies_[technology].first;
  else if (technology == UnitsTech::SWARM)
    for (auto& it : synapses_) it.second.max_stored_ = technologies_[technology].first*3+1;
  else if (technology == UnitsTech::TOTAL_OXYGEN)
    max_oxygen_ += 20;
  else if (technology == UnitsTech::TOTAL_RESOURCE)
    max_resources_ += 20;
  else if (technology == UnitsTech::CURVE)
    resource_curve_--;
  else if (technology == UnitsTech::NUCLEUS_RANGE)
    cur_range_++;
  return true;
}

void Player::MovePotential(Player* enemy) {
  // Move soldiers along the way to it's target and check if target is reached.
  std::shared_lock sl_potenial(mutex_potentials_);
  std::vector<std::string> potential_to_remove;
  auto cur_time = std::chrono::steady_clock::now();

  for (auto& it : potential_) {
    // If target not yet reached and it is time for the next action, move potential
    if (it.second.way_.size() > 0 && utils::get_elapsed(it.second.last_action_, cur_time) > it.second.speed_) {
      it.second.pos_ = it.second.way_.front(); 
      it.second.way_.pop_front();
      it.second.last_action_ = cur_time;  // potential did action, so update last_action_.
    }
    // If target is reached, handle epsp and ipsp seperatly.
    // Epsp: add potential to target and add epsp to list of potentials to remove.
    if (it.second.type_ == UnitsTech::EPSP) {
      if (it.second.way_.size() == 0) {
        enemy->AddPotentialToNeuron(it.second.pos_, it.second.potential_);
        potential_to_remove.push_back(it.first); // remove 
      }
    }
    // Ipsp: check if just time is up -> remove ipsp, otherwise -> block target.
    else {
      // If duration since last action is reached, add ipsp to list of potentials to remove and unblock target.
      if (utils::get_elapsed(it.second.last_action_, cur_time) > it.second.duration_*1000) {
        enemy->SetBlockForNeuron(it.second.pos_, false);  // unblock target.
        potential_to_remove.push_back(it.first); // remove 
      }
      else
        enemy->SetBlockForNeuron(it.second.pos_, true);  // block target
    }
  }
  sl_potenial.unlock();

  // Remove potential which has reached it's target.
  std::unique_lock ul_potential(mutex_potentials_);
  for (const auto& it : potential_to_remove)
    potential_.erase(it);
}

void Player::SetBlockForNeuron(Position pos, bool blocked) {
  std::unique_lock ul(mutex_all_neurons_);
  if (activated_neurons_.count(pos) > 0)
    activated_neurons_[pos].blocked_ = blocked;
  else if (synapses_.count(pos) > 0)
    synapses_[pos].blocked_ = blocked;
}

void Player::HandleDef(Player* enemy) {
  std::shared_lock sl(mutex_all_neurons_);
  auto cur_time = std::chrono::steady_clock::now();
  for (auto& activated_neuron : activated_neurons_) {
    // Check if activated neurons recharge is done.
    if (utils::get_elapsed(activated_neuron.second.last_action_, cur_time) > activated_neuron.second.speed_
        && !activated_neuron.second.blocked_) {
      // Check for potentials in range of activated neuron.
      for (const auto& potential : enemy->potential()) {
        int distance = utils::dist(activated_neuron.first, potential.second.pos_);
        if (distance < 3) {
          enemy->NeutralizePotential(potential.first, activated_neuron.second.potential_slowdown_);
          activated_neuron.second.last_action_ = cur_time;  // neuron did action, so update last_action_.
          break;
        }
      }
    }
  }
}

void Player::NeutralizePotential(std::string id, int potential) {
  std::unique_lock ul(mutex_potentials_);
  if (potential_.count(id) > 0) {
    potential_.at(id).potential_ -= potential;
    // Remove potential only if not already at it's target (length of way is greater than zero).
    if (potential_.at(id).potential_ == 0 && potential_.at(id).way_.size() > 0)
      potential_.erase(id);
  }
}

void Player::AddPotentialToNeuron(Position pos, int potential) {
  std::unique_lock ul_all_neurons(mutex_all_neurons_);
  std::unique_lock ul_nucleus(mutex_nucleus_);
  if (synapses_.count(pos) > 0) {
    synapses_[pos].lp_ += potential;
    if (synapses_[pos].lp_ >= synapses_[pos].max_lp_) {
      synapses_.erase(pos);
      all_neurons_.erase(pos);
    }
  }
  else if (activated_neurons_.count(pos) > 0) {
    activated_neurons_[pos].lp_ += potential;
    if (activated_neurons_[pos].lp_ >= activated_neurons_[pos].max_lp_) {
      activated_neurons_.erase(pos);
      all_neurons_.erase(pos);
    }
  }
  else if (all_nucleus_.count(pos) > 0) {
    all_nucleus_[pos].lp_ += potential;
    if (all_nucleus_[pos].lp_ >= all_nucleus_[pos].max_lp_) {
      all_nucleus_.erase(pos);
      all_neurons_.erase(pos);
      ul_all_neurons.unlock();
      CheckNeuronsAfterNexusDies();
    }
  }
  if (nucleus_.pos_.first == pos.first || nucleus_.pos_.first == pos.second) {
    nucleus_.lp_ += potential;
  }
}

void Player::CheckNeuronsAfterNexusDies() {
  std::shared_lock sl(mutex_all_neurons_);
  std::vector<Position> activated_neurons_to_remove;
  for (const auto& activated_neuron : activated_neurons_) {
    bool in_range = false;
    for (const auto& nucleus : all_nucleus_) {
      if (utils::dist(activated_neuron.first, nucleus.first) <= cur_range_)
        in_range = true;
    }
    if (!in_range) activated_neurons_to_remove.push_back(activated_neuron.first);
  }
  std::vector<Position> synapses_to_remove;
  for (const auto& synapses : synapses_) {
    bool in_range = false;
    for (const auto& nucleus : all_nucleus_) {
      if (utils::dist(synapses.first, nucleus.first) <= cur_range_)
        in_range = true;
    }
    if (!in_range) synapses_to_remove.push_back(synapses.first);
  }
  sl.unlock();
  std::unique_lock ul(mutex_all_neurons_);
  for (const auto& it : activated_neurons_to_remove) {
    activated_neurons_.erase(it);
    all_neurons_.erase(it);
  }
  for (const auto& it : synapses_to_remove) {
    synapses_.erase(it);
    all_neurons_.erase(it);
  }
}

std::string Player::GetPotentialIdIfPotential(Position pos, int unit) {
  std::shared_lock sl(mutex_potentials_);
  for (const auto& it : potential_) {
    if (it.second.pos_ == pos && (unit == -1 || it.second.type_ == unit)) {
      return it.first;
    }
  }
  return "";
}

bool Player::IsActivatedResource(int resource) {
  return resources_.at(resource).second;
}

Synapse Player::GetSynapse(Position pos) {
  std::shared_lock sl(mutex_all_neurons_);
  return synapses_.at(pos);
}
