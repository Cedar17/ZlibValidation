#ifndef ITERATORS_H
#define ITERATORS_H

#include "si2dr_liberty.h"

#include "lib_attribute.hpp"
#include "lib_group.hpp"

class GroupsIterator {
public:
  GroupsIterator(si2drGroupsIdT groups, si2drErrorT &err);
  ~GroupsIterator();
  void next();
  bool end();
  LibGroup get();

private:
  si2drGroupsIdT groups_;
  si2drGroupIdT group_;
  si2drErrorT &err_;
};

class AttributesIterator {
public:
  AttributesIterator(si2drAttrsIdT attrs, si2drErrorT &err);
  ~AttributesIterator();
  void next();
  bool end();
  LibAttribute get();

private:
  si2drAttrsIdT attrs_;
  si2drAttrIdT attr_;
  si2drErrorT &err_;
};

class ValuesIterator {
public:
  ValuesIterator(si2drValuesIdT values, si2drErrorT &err);
  ~ValuesIterator();
  void next();
  bool end();
  
  si2drValuesIdT values_;
  si2drValueTypeT vtype_;
  si2drInt32T int_;
  si2drFloat64T float_;
  si2drStringT str_;
  si2drBooleanT bool_;
  si2drExprT *exprp_;
  si2drErrorT &err_;
};

#endif // ITERATORS_H