#ifndef __OCLASIGNAL_H__
#define __OCLASIGNAL_H__

#include <cstdint>
#include <string>

enum oc_signal_type_t { SIGNAL, PLACEHOLDER };

class OclaSignal {
 private:
  std::string m_name;
  std::string m_orig_name;
  uint32_t m_bitpos;
  uint32_t m_bitwidth;
  uint32_t m_value;
  oc_signal_type_t m_type;
  uint32_t m_index;

 public:
  OclaSignal();
  OclaSignal(std::string name, uint32_t bitpos, uint32_t bitwidth,
             uint32_t value, oc_signal_type_t type, uint32_t idx);
  ~OclaSignal();
  std::string get_orig_name() const;
  void set_orig_name(std::string orig_name);
  std::string get_name() const;
  void set_name(std::string name);
  uint32_t get_bitpos() const;
  void set_bitpos(uint32_t bitpos);
  uint32_t get_bitwidth() const;
  void set_bitwidth(uint32_t bitwidth);
  uint32_t get_value() const;
  void set_value(uint32_t value);
  oc_signal_type_t get_type() const;
  void set_type(oc_signal_type_t type);
  uint32_t get_index() const;
  void set_index(uint32_t idx);
};

#endif  //__OCLASIGNAL_H__
