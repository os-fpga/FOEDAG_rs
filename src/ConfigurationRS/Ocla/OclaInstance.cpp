#include "OclaInstance.h"

OclaInstance::OclaInstance(std::string type, uint32_t version, uint32_t id, uint32_t memory_depth, uint32_t num_of_probes, uint32_t baseaddr, uint32_t idx)
    : m_type(type)
        , m_version(version)
        , m_id(id)
        , m_memory_depth(memory_depth)
        , m_num_of_probes(num_of_probes)
        , m_baseaddr(baseaddr)
        , m_index(idx) {}

OclaInstance::~OclaInstance() {}

std::string OclaInstance::get_type() const { return m_type; }

uint32_t OclaInstance::get_id() const { return m_id; }

uint32_t OclaInstance::get_version() const { return m_version; }

uint32_t OclaInstance::get_index() const { return m_index; }

uint32_t OclaInstance::get_memory_depth() const { return m_memory_depth; }

uint32_t OclaInstance::get_num_of_probes() const { return m_num_of_probes; }

uint32_t OclaInstance::get_baseaddr() const { return m_baseaddr; }
