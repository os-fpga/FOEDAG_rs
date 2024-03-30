#include "OclaSignal.h"

OclaSignal::OclaSignal(std::string name, uint32_t pos, uint32_t bitwidth, uint32_t value, oc_signal_type_t type, uint32_t idx)
    : m_name(name)
    , m_pos(pos)
    , m_bitwidth(bitwidth)
    , m_value(value)
    , m_type(type)
    , m_index(idx) {}

OclaSignal::OclaSignal() : m_index(0) {}

OclaSignal::~OclaSignal() {}

std::string OclaSignal::get_name() const { return m_name; }

void OclaSignal::set_name(std::string name) { m_name = name; }

uint32_t OclaSignal::get_pos() const { return m_pos; }

void OclaSignal::set_pos(uint32_t pos) { m_pos = pos; }

uint32_t OclaSignal::get_bitwidth() const { return m_bitwidth; }

void OclaSignal::set_bitwidth(uint32_t bitwidth) { m_bitwidth = bitwidth; }

uint32_t OclaSignal::get_value() const { return m_value; }

void OclaSignal::set_value(uint32_t value) { m_value = value; }

oc_signal_type_t OclaSignal::get_type() const { return m_type; }

void OclaSignal::set_type(oc_signal_type_t type) { m_type = type; }

uint32_t OclaSignal::get_index() const { return m_index; }

void OclaSignal::set_index(uint32_t idx) { m_index = idx; }
