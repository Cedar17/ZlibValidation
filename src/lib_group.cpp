#include "lib_group.hpp"

LibGroup::LibGroup(si2drGroupIdT group, si2drErrorT &err) : group_(group), err_(err) {}

LibGroup::~LibGroup() {}

std::string LibGroup::getName() { 
  si2drNamesIdT names = si2drGroupGetNames(group_, &err_);
  si2drStringT name = si2drIterNextName(names, &err_);
  si2drIterQuit(names, &err_);
  return name ? std::string(name) : std::string();
}

std::string LibGroup::getType() {
  si2drStringT type = si2drGroupGetGroupType(group_, &err_);
  return type ? std::string(type) : std::string();
}

si2drAttrsIdT LibGroup::getAttrs() { return si2drGroupGetAttrs(group_, &err_); }

si2drGroupsIdT LibGroup::getGroups() { return si2drGroupGetGroups(group_, &err_); }