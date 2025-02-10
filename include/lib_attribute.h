#ifndef LIB_ATTRIBUTE_H
#define LIB_ATTRIBUTE_H

#include <string>
#include "si2dr_liberty.h"

class LibAttribute {
public:
  LibAttribute(si2drAttrIdT attr, si2drErrorT &err);
  ~LibAttribute();
  std::string getName();
  si2drAttrTypeT isComplex();
  si2drValuesIdT getValues();
  long int getInt();
  double getFloat();
  std::string getString();

private:
  si2drAttrIdT attr_;
  si2drErrorT &err_;
};

#endif // LIB_ATTRIBUTE_H