#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cwchar>
#include <exception>
#include <math.h>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <stdexcept>
#include <string>
#include <vector>
#include <spdlog/spdlog.h>

#include "objects/units.h"
#include "spdlog/logger.h"
#include "curses.h"
#include "game/field.h"
#include "player/player.h"
#include "utils/utils.h"

#define LOGGER "logger"

#define HILL ' ' 
#define DEN 'D' 
#define GOLD 'G' 
#define SILVER 'S' 
#define BRONZE 'B' 
#define FREE char(46)
#define DEF 'T'

Player::Player(Position nucleus_pos, Field* field, int iron, Audio* audio) 
    : cur_range_(4), resource_curve_(3), 
    oxygen_boast_(0), max_oxygen_(100), max_resources_(70),  total_oxygen_(5.5), bound_oxygen_(0){

  field_ = field;
  audio_ = audio;

  nucleus_ = Nucleus(nucleus_pos); 
  neurons_[nucleus_pos] = std::make_shared<Nucleus>(nucleus_);
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
    "nucleus " SYMBOL_DEN " potential" + end, std::to_string(nucleus_.voltage()) + "/" + 
      std::to_string(nucleus_.max_voltage()),
  };
}

// getter 
std::map<std::string, Potential> Player::potential() { 
  std::shared_lock sl(mutex_potentials_);
  return potential_; 
}

Position Player::nucleus_pos() { 
  std::shared_lock sl(mutex_nucleus_); 
  return nucleus_.pos_;
}

int Player::cur_range() { 
  return cur_range_;
}

int Player::resource_curve() {
  std::shared_lock sl(mutex_resources_);
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
void Player::set_enemy(Player *enemy) {
  enemy_ = enemy;
}

void Player::set_resource_curve(int resource_curve) {
  std::unique_lock ul(mutex_resources_);
  resource_curve_ = resource_curve;
}

void Player::set_iron(int iron) {
  std::unique_lock ul(mutex_resources_);
  resources_[Resources::IRON].first = iron;
}

// methods 

Position Player::GetPositionOfClosestNeuron(Position pos, int unit) const {
  int min_dist = 999;
  Position closest_nucleus_pos = {-1, -1}; 
  for (const auto& it : neurons_) {
    if (it.second->type_ != unit)
      continue;
    int dist = utils::dist(pos, it.first);
    if(dist < min_dist) {
      closest_nucleus_pos = it.first;
      min_dist = dist;
    }
  }
  return closest_nucleus_pos;
}

std::string Player::GetNucleusLive() {
  return std::to_string(nucleus_.voltage()) + " / " + std::to_string(nucleus_.max_voltage());
}

bool Player::HasLost() {
  std::shared_lock sl(mutex_nucleus_);
  return nucleus_.voltage() >= nucleus_.max_voltage();
}

std::shared_ptr<Neuron> Player::GetNeuron(Position pos, int unit) {
  std::unique_lock ul(mutex_all_neurons_);
  if (neurons_.count(pos) > 0 && (unit == -1 || neurons_.at(pos)->type_ == unit))
    return neurons_.at(pos);
  throw std::invalid_argument("No neuron at this postion!");
}

int Player::GetNeuronTypeAtPosition(Position pos) {
  std::unique_lock ul(mutex_all_neurons_);
  if (neurons_.count(pos) > 0)
    return neurons_.at(pos)->type_;
  return -1;
}

bool Player::IsNeuronBlocked(Position pos) {
  std::unique_lock ul(mutex_all_neurons_);
  if (neurons_.count(pos) > 0)
    return neurons_.at(pos)->blocked();
  return false;
}

std::vector<Position> Player::GetAllPositionsOfNeurons(int type) {
  std::shared_lock sl(mutex_all_neurons_);
  std::vector<Position> positions;
  for (const auto& it : neurons_)
    if (type == -1 || it.second->type_ == type)
      positions.push_back(it.first);
  return positions;
}

Position Player::GetRandomNeuron(std::vector<int> type) {
  std::shared_lock sl(mutex_all_neurons_);
  // Get all positions at which there are activated neurons
  std::vector<Position> activated_neuron_postions;
  for (const auto& it : neurons_)
    if (std::find(type.begin(), type.end(), it.second->type_) != type.end())
      activated_neuron_postions.push_back(it.first);
  // If none return invalied position.
  if (activated_neuron_postions.size() == 0)
    return {-1, -1};
  // If only one, return this position.
  if (activated_neuron_postions.size() == 1)
    return activated_neuron_postions.front();
  // Otherwise, get random index and return position at index.
  int ran = utils::getrandom_int(0, activated_neuron_postions.size()-1);
  if (audio_ != nullptr)
    ran = audio_->RandomInt(0, activated_neuron_postions.size()-1);
  return activated_neuron_postions[ran];
}

void Player::ResetWayForSynapse(Position pos, Position way_position) {
  std::unique_lock ul(mutex_all_neurons_);
  if (neurons_.count(pos) && neurons_.at(pos)->type_ == UnitsTech::SYNAPSE)
    neurons_.at(pos)->set_way_points({way_position});
}

void Player::AddWayPosForSynapse(Position pos, Position way_position) {
  std::unique_lock ul(mutex_all_neurons_);
  if (neurons_.count(pos) && neurons_.at(pos)->type_ == UnitsTech::SYNAPSE) {
    auto cur_war = neurons_.at(pos)->ways_points();
    cur_war.push_back(way_position);
    neurons_.at(pos)->set_way_points(cur_war);
  }
}

void Player::SwitchSwarmAttack(Position pos) {
  std::unique_lock ul(mutex_all_neurons_);
  if (neurons_.count(pos) && neurons_.at(pos)->type_ == UnitsTech::SYNAPSE)
    neurons_.at(pos)->set_swarm(!neurons_.at(pos)->swarm());
}

void Player::ChangeIpspTargetForSynapse(Position pos, Position target_pos) {
  std::unique_lock ul(mutex_all_neurons_);
  if (neurons_.count(pos) && neurons_.at(pos)->type_ == UnitsTech::SYNAPSE)
    neurons_.at(pos)->set_ipsp_target_pos(target_pos);
}

void Player::ChangeEpspTargetForSynapse(Position pos, Position target_pos) {
  std::unique_lock ul(mutex_all_neurons_);
  if (neurons_.count(pos) && neurons_.at(pos)->type_ == UnitsTech::SYNAPSE)
    neurons_.at(pos)->set_epsp_target_pos(target_pos);
}

double Player::Faktor(int limit, double cur) {
  return (limit-cur)/(resource_curve_*limit);
}

void Player::IncreaseResources(bool inc_iron) {
  std::unique_lock ul_resources(mutex_resources_);
  // Add oxygen based on oxygen-boast and current oxygen.
  auto faktor = Faktor(max_oxygen_, total_oxygen_) * 1;
  resources_[Resources::OXYGEN].first += oxygen_boast_* faktor;
  int cur_oxygen = resources_[Resources::OXYGEN].first;
  total_oxygen_ = bound_oxygen_ + cur_oxygen;
  // Add iron if inc_iron=True, the player has less than 10 oxygen and only max 2 irons.
  if (inc_iron && resources_[Resources::IRON].first < 3)
    resources_[Resources::IRON].first++;
  // Add other resources based on current oxygen.
  for (auto& it : resources_) {
    if (it.first != Resources::IRON && it.first != OXYGEN && it.second.second)
      it.second.first += log(cur_oxygen+1) * Faktor(max_resources_, it.second.first) * 1;
  }
}

bool Player::DistributeIron(int resource) {
  spdlog::get(LOGGER)->debug("Player::DistributeIron: resource={}", resource);
  std::unique_lock ul(mutex_resources_);
  if (resources_.count(resource) == 0) {
    spdlog::get(LOGGER)->error("Player::DistributeIron: invalid resource!");
    return false;
  }
  else if (resource == Resources::OXYGEN) {
    if (resources_.at(Resources::IRON).first > 0) {
      spdlog::get(LOGGER)->info("Player::DistributeIron: updating oxygen!");
      oxygen_boast_++;
      resources_[Resources::IRON].first--;
    }
    else  {
      spdlog::get(LOGGER)->info("Player::DistributeIron: oxygen: not enough iron!");
      return false;
    }
  }
  else if (resources_[Resources::IRON].first < 2 || resources_[resource].second) {
    spdlog::get(LOGGER)->info("Player::DistributeIron: other: not enough or already enabled!");
    return false;
  }
  else {
    spdlog::get(LOGGER)->info("Player::DistributeIron: updating other resource!");
    resources_[resource].second = true;
    resources_[Resources::IRON].first-=2;
  }
  spdlog::get(LOGGER)->info("Player::DistributeIron: success!");
  return true;
}

Costs Player::GetMissingResources(int unit, int boast) {
  spdlog::get(LOGGER)->debug("Player::GetMissingResources: {}, {}", unit, boast);
  std::shared_lock sl(mutex_resources_);
  // Get costs for desired unit
  Costs needed = units_costs_.at(unit);

  // Check costs and add to missing.
  std::map<int, double> missing;
  for (const auto& it : needed)
    if (resources_.at(it.first).first < it.second*boast) 
      missing[it.first] = it.second - resources_.at(it.first).first;
  spdlog::get(LOGGER)->debug("Player::GetMissingResources: returnin {} missing resources", missing.size());
  return missing;
}

bool Player::TakeResources(int type, int boast) {
  if (GetMissingResources(type, boast).size() > 0) {
    spdlog::get(LOGGER)->error("Player::TakeResources: taking resources with out enough resources!");
    return false;
  }
  std::unique_lock ul_resources(mutex_resources_);
  auto costs = units_costs_.at(type);
  for (const auto& it : costs) {
    resources_[it.first].first -= it.second*boast;
    if (it.first == Resources::OXYGEN && (type == UnitsTech::SYNAPSE || type == UnitsTech::ACTIVATEDNEURON))
      bound_oxygen_ += it.second;
  }
  return true;
}

bool Player::AddNeuron(Position pos, int neuron_type, Position epsp_target, Position ipsp_target) {
  std::unique_lock ul(mutex_all_neurons_);
  if (!TakeResources(neuron_type))
    return false;
  if (neuron_type == UnitsTech::ACTIVATEDNEURON) {
    std::shared_lock sl(mutex_technologies_);
    int speed_boast = technologies_.at(UnitsTech::DEF_SPEED).first * 40;
    int potential_boast = technologies_.at(UnitsTech::DEF_POTENTIAL).first;
    neurons_[pos] = std::make_shared<ActivatedNeuron>(pos, potential_boast, speed_boast);
  }
  else if (neuron_type == UnitsTech::SYNAPSE)
    neurons_[pos] = std::make_shared<Synapse>(pos, technologies_.at(UnitsTech::SWARM).first*3+1, 
        technologies_.at(UnitsTech::WAY).first, epsp_target, ipsp_target);
  else if (neuron_type == UnitsTech::NUCLEUS) {
    neurons_[pos] = std::make_shared<Nucleus>(pos);
    std::shared_lock ul_resources(mutex_resources_);
    // Increase max resource if new nucleus is built.
    max_oxygen_ += 10;
    max_resources_ += 10;
  }
  return true;
}

bool Player::AddPotential(Position synapes_pos, int unit) {
  spdlog::get(LOGGER)->debug("Player::AddPotential.");
  if (!TakeResources(unit))
    return false;
  // Get way and target:
  auto synapse = GetNeuron(synapes_pos, UnitsTech::SYNAPSE);
  std::shared_lock sl_neurons(mutex_all_neurons_);
  // Check if synapses is blocked.
  if (synapse->blocked()) 
    return true;
  
  // Create way.
  spdlog::get(LOGGER)->debug("Player::AddPotential: get way for potential.");
  // TODO: Move calculating way to creating synapses/ chanding target.
  synapse->UpdateIpspTargetIfNotSet(enemy_->GetRandomNeuron(
        {UnitsTech::ACTIVATEDNEURON, UnitsTech::SYNAPSE, UnitsTech::NUCLEUS}));
  auto way = field_->GetWayForSoldier(synapes_pos, synapse->GetWayPoints(unit));

  // Add potential.
  std::unique_lock ul_potentials(mutex_potentials_);
  std::shared_lock sl_technologies(mutex_technologies_);
  // Get boast from technologies.
  int potential_boast = technologies_.at(UnitsTech::ATK_POTENIAL).first;
  int speed_boast = 50*technologies_.at(UnitsTech::ATK_POTENIAL).first;
  int duration_boast = technologies_.at(UnitsTech::ATK_DURATION).first;
  sl_technologies.unlock();
  if (unit == UnitsTech::EPSP) {
    spdlog::get(LOGGER)->debug("Player::AddPotential: epsp - get num epsps to create.");
    // Increase num of currently stored epsps and get number of epsps to create.
    size_t num_epsps_to_create = neurons_.at(synapes_pos)->AddEpsp();
    spdlog::get(LOGGER)->debug("Player::AddPotential: epsp - creating {} epsps.", num_epsps_to_create);
    for (size_t i=0; i<num_epsps_to_create; i++)
      potential_[utils::create_id("epsp")] = Epsp(synapes_pos, way, potential_boast, speed_boast);
  }
  else if (unit == UnitsTech::IPSP) {
    spdlog::get(LOGGER)->debug("Player::AddPotential: ipsp - creating 1 ipsp.");
    potential_[utils::create_id("ipsp")] = Ipsp(synapes_pos, way, potential_boast, speed_boast, duration_boast);
  }
  spdlog::get(LOGGER)->debug("Player::AddPotential: done.");
  return true;
}

bool Player::AddTechnology(int technology) {
  std::unique_lock ul(mutex_technologies_);

  // Check if technology exists, resources are missing and whether already fully researched.
  if (technologies_.count(technology) == 0)
    return false;
  if (GetMissingResources(technology, technologies_[technology].first+1).size() > 0 
      || technologies_[technology].first == technologies_[technology].second)
    return false;
  if (!TakeResources(technology,  technologies_[technology].first+1))
    return false;
 
  // Handle technology.
  std::unique_lock ul_resources(mutex_resources_);
  technologies_[technology].first++;
  if (technology == UnitsTech::WAY) {
    for (auto& it : neurons_)
      if (it.second->type_ == UnitsTech::SYNAPSE)
        it.second->set_availible_ways(technologies_[technology].first);
  }
  else if (technology == UnitsTech::SWARM) {
    for (auto& it : neurons_)
      if (it.second->type_ == UnitsTech::SYNAPSE)
        it.second->set_max_stored(technologies_[technology].first*3+1);
  }
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
      else if (it.second.way_.size() == 0)
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
  if (neurons_.count(pos) > 0)
    neurons_[pos]->set_blocked(blocked);
}

void Player::HandleDef(Player* enemy) {
  std::unique_lock ul(mutex_all_neurons_);
  auto cur_time = std::chrono::steady_clock::now();
  for (auto& neuron : neurons_) {
    if (neuron.second->type_ != UnitsTech::ACTIVATEDNEURON)
      continue;
    // Check if activated neurons recharge is done.
    if (utils::get_elapsed(neuron.second->last_action(), cur_time) > neuron.second->speed() 
        && !neuron.second->blocked()) {
      // Check for potentials in range of activated neuron.
      for (const auto& potential : enemy->potential()) {
        int distance = utils::dist(neuron.first, potential.second.pos_);
        if (distance < 3) {
          enemy->NeutralizePotential(potential.first, neuron.second->potential_slowdown());
          neuron.second->set_last_action(cur_time);  // neuron did action, so update last_action_.
          break;
        }
      }
    }
  }
}

void Player::NeutralizePotential(std::string id, int potential) {
  spdlog::get(LOGGER)->debug("Player::NeutralizePotential: {}", potential);
  std::unique_lock ul(mutex_potentials_);
  if (potential_.count(id) > 0) {
    spdlog::get(LOGGER)->debug("Player::NeutralizePotential: left potential: {}", 
        potential_.at(id).potential_);
    potential_.at(id).potential_ -= potential;
    // Remove potential only if not already at it's target (length of way is greater than zero).
    if (potential_.at(id).potential_ == 0 && potential_.at(id).way_.size() > 0) {
      spdlog::get(LOGGER)->debug("Player::NeutralizePotential: deleting potential...");
      potential_.erase(id);
      spdlog::get(LOGGER)->debug("Player::NeutralizePotential: done.");
    }
  }
}

void Player::AddPotentialToNeuron(Position pos, int potential) {
  std::unique_lock ul_all_neurons(mutex_all_neurons_);
  std::unique_lock ul_nucleus(mutex_nucleus_);
  spdlog::get(LOGGER)->debug("Player::AddPotentialToNeuron: {}", potential);

  if (neurons_.count(pos) > 0) {
    spdlog::get(LOGGER)->debug("Player::AddPotentialToNeuron: left potential: {}", neurons_[pos]->voltage());
    if (neurons_.at(pos)->IncreaseVoltage(potential)) {
      int type = neurons_.at(pos)->type_;
      spdlog::get(LOGGER)->debug("Player::AddPotentialToNeuron: erasing {}", type);
      neurons_.erase(pos);
      spdlog::get(LOGGER)->debug("Player::AddPotentialToNeuron: erasing done");
      // Potentially deactivate all neurons formally in range of the destroyed nucleus.
      if (type == UnitsTech::NUCLEUS) {
        ul_all_neurons.unlock();
        CheckNeuronsAfterNucleusDies();
        // Remove added max resources when nucleus dies.
        std::unique_lock ul_resources(mutex_resources_);
        max_oxygen_ -= 10;
        max_resources_ -= 10;
      }
      spdlog::get(LOGGER)->debug("Player::AddPotentialToNeuron: done.");
    }
  }
  // Seperatly check main nucleus.
  if (nucleus_.pos_.first == pos.first || nucleus_.pos_.first == pos.second)
    nucleus_.IncreaseVoltage(potential); // return value ignored.
}

void Player::CheckNeuronsAfterNucleusDies() {
  spdlog::get(LOGGER)->debug("Player::CheckNeuronsAfterNucleusDies");
  // Get all nucleus.
  std::vector<Position> all_nucleus = GetAllPositionsOfNeurons(UnitsTech::NUCLEUS);
  std::shared_lock sl(mutex_all_neurons_);
  std::vector<Position> neurons_to_remove;
  for (const auto& neuron : neurons_) {
    // Nucleus are not affekted.
    if (neuron.second->type_ == UnitsTech::NUCLEUS)
      continue;
    // Add first and if in range of a nucleus, remove again.
    neurons_to_remove.push_back(neuron.first);
    for (const auto& nucleus_pos : all_nucleus)
      if (utils::dist(neuron.first, nucleus_pos) <= cur_range_)
        neurons_to_remove.pop_back();
  }
  sl.unlock();
  std::unique_lock ul(mutex_all_neurons_);
  for (const auto& it : neurons_to_remove) {
    neurons_.erase(it);
  }
}

std::string Player::GetPotentialIdIfPotential(Position pos, int unit) {
  std::shared_lock sl(mutex_potentials_);
  for (const auto& it : potential_)
    if (it.second.pos_ == pos && (unit == -1 || it.second.type_ == unit))
      return it.first;
  return "";
}

bool Player::IsActivatedResource(int resource) {
  if (resources_.count(resource) == 0) {
    spdlog::get(LOGGER)->error("Player::IsActivatedResource: Checking for resource which doesn't exist: {}", resource);
    return false;
  }
  return resources_.at(resource).second;
}

choice_mapping_t Player::GetOptionsForSynapes(Position pos) {
  std::shared_lock sl_technologies(mutex_technologies_);
  auto synapse = GetNeuron(pos);
  choice_mapping_t mapping;
  
  mapping[1] = {"(Re-)set way.", (technologies_.at(UnitsTech::WAY).first > 0) ? COLOR_AVAILIBLE : COLOR_DEFAULT};
  mapping[2] = {"Add way-point.", (synapse->ways_points().size() < synapse->num_availible_ways()) 
    ? COLOR_AVAILIBLE : COLOR_DEFAULT};
  mapping[3] = {"Select target for ipsp.", (technologies_.at(UnitsTech::TARGET).first > 0) 
    ? COLOR_AVAILIBLE : COLOR_DEFAULT};
  mapping[4] = {"Select target for epsp.", (technologies_.at(UnitsTech::TARGET).first > 1) 
    ? COLOR_AVAILIBLE : COLOR_DEFAULT};
  mapping[5] = {(synapse->swarm()) ? "Turn swarm-attack off" : "Turn swarm-attack on", 
    (technologies_.at(UnitsTech::SWARM).first > 0) ? COLOR_AVAILIBLE : COLOR_DEFAULT};
  return mapping;
}