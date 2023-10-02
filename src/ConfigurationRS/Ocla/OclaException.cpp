#include "OclaException.h"

static std::map<ocla_error, std::string> error_messages = {
    {UNKNOWN_ERROR, "Unknown error"},
    {NOT_IMPLEMENTED, "Not implemented"},
};

OclaException::OclaException(ocla_error err, std::string msg)
    : m_err(err), m_msg(msg) {}

const char *OclaException::what() const throw() {
  ocla_error err = m_err;

  if (error_messages.find(err) == error_messages.end()) {
    err = UNKNOWN_ERROR;
  }

  return error_messages[err].c_str();
}