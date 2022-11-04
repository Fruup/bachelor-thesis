// This code is from the CUDA sample "simpleVulkan".

#pragma once

#include <stdexcept>

#include <Windows.h>
#include <aclapi.h>

class WindowsSecurityAttributes
{
protected:
    SECURITY_ATTRIBUTES m_winSecurityAttributes;
    PSECURITY_DESCRIPTOR m_winPSecurityDescriptor;

public:
    WindowsSecurityAttributes();
    SECURITY_ATTRIBUTES *operator&();
    ~WindowsSecurityAttributes();
};
