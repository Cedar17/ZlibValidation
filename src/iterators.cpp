#include "iterators.hpp"

GroupsIterator::GroupsIterator(si2drGroupsIdT groups, si2drErrorT &err) : groups_(groups), err_(err) {
  group_ = si2drIterNextGroup(groups_, &err_);
}
GroupsIterator::~GroupsIterator() { si2drIterQuit(groups_, &err_); }

void GroupsIterator::next() { group_ = si2drIterNextGroup(groups_, &err_); }
bool GroupsIterator::end() { return si2drObjectIsNull(group_, &err_); }

LibGroup GroupsIterator::get() { return LibGroup(group_, err_); }

AttributesIterator::AttributesIterator(si2drAttrsIdT attrs, si2drErrorT &err) : attrs_(attrs), err_(err) {
  attr_ = si2drIterNextAttr(attrs_, &err_);
}
AttributesIterator::~AttributesIterator() { si2drIterQuit(attrs_, &err_); }

void AttributesIterator::next() { attr_ = si2drIterNextAttr(attrs_, &err_); }
bool AttributesIterator::end() { return si2drObjectIsNull(attr_, &err_); }

LibAttribute AttributesIterator::get() { return LibAttribute(attr_, err_); }

ValuesIterator::ValuesIterator(si2drValuesIdT values, si2drErrorT &err) : values_(values), err_(err) {
  si2drIterNextComplexValue(values_, &vtype_, &int_, &float_, &str_, &bool_, &exprp_, &err_);
}
ValuesIterator::~ValuesIterator() { si2drIterQuit(values_, &err_); }

void ValuesIterator::next() {
  si2drIterNextComplexValue(values_, &vtype_, &int_, &float_, &str_, &bool_, &exprp_, &err_);
}
bool ValuesIterator::end() { return vtype_ == SI2DR_UNDEFINED_VALUETYPE; }