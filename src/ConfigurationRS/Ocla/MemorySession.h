#ifndef __MEMORYSESSION_H__
#define __MEMORYSESSION_H__

#include "OclaSession.h"

class MemorySession : public OclaSession {
 public:
  MemorySession();
  virtual ~MemorySession();
  virtual void load(std::string bitasmfile);
  virtual void unload();
  virtual uint32_t get_instance_count();
  virtual Ocla_INSTANCE_INFO get_instance_info(uint32_t instance);
  virtual std::vector<Ocla_PROBE_INFO> get_probe_info(uint32_t instance);
};

#endif  //__MEMORYSESSION_H__
