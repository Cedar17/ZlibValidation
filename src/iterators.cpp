#include "iterators.hpp"

GroupsIterator::GroupsIterator(si2drGroupsIdT groups, si2drErrorT &err)
    : groups_(groups), err_(err) {}

GroupsIterator::~GroupsIterator() { si2drIterQuit(groups_, &err_); }

void GroupsIterator::begin() { group_ = si2drIterNextGroup(groups_, &err_); }
void GroupsIterator::next() { group_ = si2drIterNextGroup(groups_, &err_); }
bool GroupsIterator::end() { return si2drObjectIsNull(group_, &err_); }

LibGroup GroupsIterator::get() { return LibGroup(group_, err_); }

AttributesIterator::AttributesIterator(si2drAttrsIdT attrs, si2drErrorT &err)
    : attrs_(attrs), err_(err) {}

AttributesIterator::~AttributesIterator() { si2drIterQuit(attrs_, &err_); }

void AttributesIterator::begin() { attr_ = si2drIterNextAttr(attrs_, &err_); }
void AttributesIterator::next() { attr_ = si2drIterNextAttr(attrs_, &err_); }
bool AttributesIterator::end() { return si2drObjectIsNull(attr_, &err_); }

LibAttribute AttributesIterator::get() { return LibAttribute(attr_, err_); }