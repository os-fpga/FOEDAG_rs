#ifndef __OCLAEXCEPTION_H__
#define __OCLAEXCEPTION_H__

#include <iostream>
#include <map>

enum ocla_error { UNKNOWN_ERROR = -1, NOT_IMPLEMENTED = -2 };

class OclaException : public std::exception {
 public:
  OclaException(ocla_error err, std::string msg = "");
  const char *what() const throw();
  const char *getMessage() const { return m_msg.c_str(); }
  ocla_error getError() const { return m_err; }

 private:
  ocla_error m_err;
  std::string m_msg;
};

#endif  //__OCLAEXCEPTION_H__
