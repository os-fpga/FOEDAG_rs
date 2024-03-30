#ifndef __OCLAINSTANCE_H__
#define __OCLAINSTANCE_H__

#include <cstdint>
#include <string>

class OclaInstance {
 private:
  std::string m_type;
  uint32_t m_version;
  uint32_t m_id;
  uint32_t m_memory_depth;
  uint32_t m_num_of_probes;
  uint32_t m_baseaddr;
  uint32_t m_index;

 public:
  OclaInstance(std::string type, uint32_t version, uint32_t id,
               uint32_t memory_depth, uint32_t num_of_probes, uint32_t baseaddr,
               uint32_t idx);
  ~OclaInstance();
  std::string get_type() const;
  uint32_t get_id() const;
  uint32_t get_version() const;
  uint32_t get_index() const;
  uint32_t get_memory_depth() const;
  uint32_t get_num_of_probes() const;
  uint32_t get_baseaddr() const;
};

#endif  //__OCLAINSTANCE_H__
