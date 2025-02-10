#ifndef LIB_GROUP_H
#define LIB_GROUP_H

#include <string>
#include "si2dr_liberty.h"

class LibGroup {
public:
  LibGroup(si2drGroupIdT group, si2drErrorT &err);
  ~LibGroup();
  si2drNamesIdT getNames();
  std::string getType();
  si2drAttrsIdT getAttrs();
  si2drGroupsIdT getGroups();

private:
  si2drGroupIdT group_;
  si2drErrorT &err_;
};

#endif // LIB_GROUP_H