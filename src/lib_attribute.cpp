#include "lib_attribute.hpp"

LibAttribute::LibAttribute(si2drAttrIdT attr, si2drErrorT &err) : attr_(attr), err_(err) {}

LibAttribute::~LibAttribute() {}

std::string LibAttribute::getName() {
  si2drStringT name = si2drAttrGetName(attr_, &err_);
  return name ? std::string(name) : std::string();
}

bool LibAttribute::isComplex() { return si2drAttrGetAttrType(attr_, &err_) ? 1 : 0; }

si2drValuesIdT LibAttribute::getValues() { return si2drComplexAttrGetValues(attr_, &err_); }

long int LibAttribute::getInt() { return si2drSimpleAttrGetInt32Value(attr_, &err_); }

double LibAttribute::getFloat() { return si2drSimpleAttrGetFloat64Value(attr_, &err_); }

std::string LibAttribute::getString() {
  si2drStringT str = si2drSimpleAttrGetStringValue(attr_, &err_);
  return str ? std::string(str) : std::string();
}