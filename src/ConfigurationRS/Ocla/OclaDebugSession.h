#ifndef __OCLADEBUGSESSION_H__
#define __OCLADEBUGSESSION_H__

#include "OclaDomain.h"
#include "OclaInstance.h"
#include "OclaProbe.h"
#include "OclaSignal.h"
#include "OclaIP.h"

#include <vector>
#include <string>
#include <cstdint>

class OclaDebugSession {
    private:
        std::vector<OclaDomain> m_clock_domains;
        std::string m_filepath;
        bool m_loaded;
        OclaSignal parse_signal(std::string signal_str);
        void parse(std::string ocla_str);

    public:
        OclaDebugSession();
        ~OclaDebugSession();
        std::vector<OclaDomain>& get_clock_domains();
        std::vector<OclaInstance> get_instances();
        std::vector<OclaProbe> get_probes(uint32_t instance_index);
        std::string get_filepath() const;
        bool is_loaded() const;
        void load(std::string filepath);
        void unload();
};

#endif  //__OCLADEBUGSESSION_H__
