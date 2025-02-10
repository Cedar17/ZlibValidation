#include "lib_group.h"

LibGroup::LibGroup(si2drGroupIdT group, si2drErrorT &err) : group_(group), err_(err) {}

LibGroup::~LibGroup() {}

si2drNamesIdT LibGroup::getNames() { return si2drGroupGetNames(group_, &err_); }

std::string LibGroup::getType() {
  si2drStringT type = si2drGroupGetGroupType(group_, &err_);
  return type ? std::string(type) : std::string();
}

si2drAttrsIdT LibGroup::getAttrs() { return si2drGroupGetAttrs(group_, &err_); }

si2drGroupsIdT LibGroup::getGroups() { return si2drGroupGetGroups(group_, &err_); }