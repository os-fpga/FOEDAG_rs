#ifndef __OCLADOMAIN_H__
#define __OCLADOMAIN_H__

#include "OclaInstance.h"
#include "OclaProbe.h"

#include "OclaIP.h"

#include <vector>
#include <string>
#include <cstdint>

enum oc_domain_type_t {
    NATIVE,
    AXI
};

struct oc_trigger_t {
    uint32_t instance_index;
    std::string signal;
    ocla_trigger_config cfg;
};


class OclaDomain {
    private:
        std::vector<OclaInstance> m_instances;
        std::vector<OclaProbe> m_probes;
        oc_domain_type_t m_type;
        uint32_t m_index;
        std::vector<oc_trigger_t> m_triggers;
        ocla_config m_config;

    public:
        OclaDomain(oc_domain_type_t type, uint32_t idx = 0);
        //     : m_type(type)
        //     , m_index(idx)
        // {
        //     m_config.mode = ocla_trigger_mode::CONTINUOUS;
        //     m_config.condition = ocla_trigger_condition::DEFAULT;
        //     m_config.enable_fix_sample_size = false;
        //     m_config.sample_size = 0;
        // }

        ~OclaDomain();

        void add_probe(const OclaProbe& probe);

        void add_instance(const OclaInstance& instance);

        oc_domain_type_t get_type() const;

        uint32_t get_index() const;

        void set_index(uint32_t idx);

        std::vector<OclaProbe>& get_probes();

        std::vector<OclaInstance>& get_instances();

        ocla_config get_config() const;

        void set_config(ocla_config &config);

        void add_trigger(oc_trigger_t &trig);

        std::vector<oc_trigger_t> get_triggers();
};

#endif //__OCLADOMAIN_H__
