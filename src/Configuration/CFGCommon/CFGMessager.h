#ifndef CFGMessage_H
#define CFGMessage_H

#include <string>
#include <vector>

enum CFGMessageType {
  CFGMessageType_INFO,
  CFGMessageType_ERROR,
  CFGMessageType_ERROR_APPEND
};

struct CFGMessage {
  CFGMessage(const CFGMessageType t, const std::string &m) :
    type(t), 
    msg(m) {

  }
  const CFGMessageType type = CFGMessageType_INFO;
  const std::string msg = "";
};

class CFGMessager {
public:
  void add_msg(const std::string &m);
  void add_error(const std::string &m);
  void append_error(const std::string &m);
  void clear();
  std::vector<CFGMessage> msgs;
};

#endif
