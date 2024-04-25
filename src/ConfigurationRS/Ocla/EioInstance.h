#ifndef __EIOINSTANCE_H__
#define __EIOINSTANCE_H__

#include <cstdint>
#include <string>
#include <vector>

enum eio_probe_type_t { IO_INPUT, IO_OUTPUT };

struct eio_signal_t {
  std::string orig_name;
  std::string name;
  uint32_t bitpos;
  uint32_t bitwidth;
  uint32_t idx;
};

struct eio_probe_t {
  std::vector<eio_signal_t> signal_list;
  eio_probe_type_t type;
  uint32_t probe_width;
  uint32_t idx;
};

class EioInstance {
 public:
  EioInstance(uint32_t baseaddr, uint32_t index);
  ~EioInstance();
  std::vector<eio_probe_t>& get_probes();
  void add_probe(eio_probe_t& probe);
  uint32_t get_baseaddr() const;
  uint32_t get_index() const;

 private:
  uint32_t m_baseaddr;
  uint32_t m_index;
  std::vector<eio_probe_t> m_probes;
};

#endif  //__EIOINSTANCE_H__
