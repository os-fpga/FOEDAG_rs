#ifndef CFGMessage_H
#define CFGMessage_H

#include <string>
#include <vector>

enum CFGMessageType {
  INFO,
  ERROR,
  ERROR_APPEND
};

struct CFGMessage {
  CFGMessage(const CFGMessageType t, const std::string &m) :
    type(t), 
    msg(m) {

  }
  const CFGMessageType type = CFGMessageType::INFO;
  const std::string msg = "";
};

class CFGMessager {
public:
  void add_msg(const std::string &m) {
    msgs.push_back(CFGMessage(CFGMessageType::INFO, m));
  }
  void add_error(const std::string &m) {
    msgs.push_back(CFGMessage(CFGMessageType::ERROR, m));
  }
  void append_error(const std::string &m) {
    msgs.push_back(CFGMessage(CFGMessageType::ERROR_APPEND, m));
  }
  void clear() {
    msgs.clear();
  }
  std::vector<CFGMessage> msgs;
};

#endif