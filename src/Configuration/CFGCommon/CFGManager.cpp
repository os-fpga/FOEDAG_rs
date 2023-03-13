#include <stdio.h>

#include "CFGHelper.h"
#include "Compiler/CompilerRS.h"

static const FOEDAG::CompilerRS* m_compiler = nullptr;

void CFGManager::post_msg(const std::string& msg) {
	if (m_compiler != nullptr) {
		m_compiler->Message(msg);
	} else {
		printf("Info : %s\n", msg.c_str());
	}
}

void CFGManager::post_err(const std::string& msg) {
	if (m_compiler != nullptr) {
		m_compiler->ErrorMessage(msg);
	} else {
		printf("Error: %s\n", msg.c_str());
	}
}

void CFGManager::post_err_no_append(const std::string& msg) {
	if (m_compiler != nullptr) {
		m_compiler->ErrorMessage(msg, false);
	} else {
		printf("Error: %s\n", msg.c_str());
	}
}

void CFGManager::register_compiler(const void* ptr) {
	// We just need to register compiler once
	CFG_ASSERT(m_compiler == nullptr);
	CFG_ASSERT(ptr != nullptr);
	const FOEDAG::CompilerRS* compiler = reinterpret_cast<const FOEDAG::CompilerRS*>(ptr);
	CFG_ASSERT(compiler != nullptr);
	m_compiler = compiler;
}
