#ifndef __OCLAPROBE_H__
#define __OCLAPROBE_H__

#include "OclaSignal.h"
#include <vector>
#include <cstdint>

class OclaProbe {
private:
    std::vector<OclaSignal> m_signals;
    uint32_t m_instance_index;
    uint32_t m_index;

public:
    OclaProbe(uint32_t idx = 0);
    ~OclaProbe();
    void add_signal(const OclaSignal& signal);
    uint32_t get_instance_index() const;
    void set_instance_index(uint32_t instance_index);
    uint32_t get_index() const;
    void set_index(uint32_t idx);
    uint32_t get_total_bitwidth();
    std::vector<OclaSignal>& get_signals();
};

#endif //__OCLAPROBE_H__
