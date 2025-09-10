#pragma once

#include <Windows.h>
#include <functional>

// RAII wrapper for Windows HANDLE objects
class RAIIHandle
{
public:
    // Default constructor - creates an invalid handle
    RAIIHandle() : m_handle(INVALID_HANDLE_VALUE) {}

    // Constructor with explicit handle
    explicit RAIIHandle(HANDLE handle) : m_handle(handle) {}

    // Move constructor
    RAIIHandle(RAIIHandle &&other) noexcept : m_handle(other.m_handle)
    {
        other.m_handle = INVALID_HANDLE_VALUE;
    }

    // Move assignment
    RAIIHandle &operator=(RAIIHandle &&other) noexcept
    {
        if (this != &other)
        {
            Reset();
            m_handle = other.m_handle;
            other.m_handle = INVALID_HANDLE_VALUE;
        }
        return *this;
    }

    // Destructor - automatically closes the handle
    ~RAIIHandle()
    {
        Reset();
    }

    // Delete copy constructor and copy assignment
    RAIIHandle(const RAIIHandle &) = delete;
    RAIIHandle &operator=(const RAIIHandle &) = delete;

    // Get the underlying handle
    HANDLE Get() const { return m_handle; }

    // Check if handle is valid
    bool IsValid() const { return m_handle != INVALID_HANDLE_VALUE && m_handle != NULL; }

    // Check if handle is invalid
    bool IsInvalid() const { return !IsValid(); }

    // Release ownership of the handle (returns the handle and sets internal to invalid)
    HANDLE Release()
    {
        HANDLE temp = m_handle;
        m_handle = INVALID_HANDLE_VALUE;
        return temp;
    }

    // Reset the handle (closes current handle if valid)
    void Reset(HANDLE newHandle = INVALID_HANDLE_VALUE)
    {
        if (m_handle != INVALID_HANDLE_VALUE && m_handle != NULL)
        {
            CloseHandle(m_handle);
        }
        m_handle = newHandle;
    }

    // Swap with another RAIIHandle
    void Swap(RAIIHandle &other) noexcept
    {
        HANDLE temp = m_handle;
        m_handle = other.m_handle;
        other.m_handle = temp;
    }

    // Operator overloads for convenience
    operator HANDLE() const { return m_handle; }

    bool operator==(const RAIIHandle &other) const { return m_handle == other.m_handle; }
    bool operator!=(const RAIIHandle &other) const { return m_handle != other.m_handle; }

private:
    HANDLE m_handle;
};

// RAII wrapper for Event handles
class RAIIEvent
{
public:
    // Default constructor - creates an invalid handle
    RAIIEvent() : m_event(NULL) {}

    // Constructor with explicit handle
    explicit RAIIEvent(HANDLE event) : m_event(event) {}

    // Move constructor
    RAIIEvent(RAIIEvent &&other) noexcept : m_event(other.m_event)
    {
        other.m_event = NULL;
    }

    // Move assignment
    RAIIEvent &operator=(RAIIEvent &&other) noexcept
    {
        if (this != &other)
        {
            Reset();
            m_event = other.m_event;
            other.m_event = NULL;
        }
        return *this;
    }

    // Destructor - automatically closes the event
    ~RAIIEvent()
    {
        Reset();
    }

    // Delete copy constructor and copy assignment
    RAIIEvent(const RAIIEvent &) = delete;
    RAIIEvent &operator=(const RAIIEvent &) = delete;

    // Get the underlying handle
    HANDLE Get() const { return m_event; }

    // Check if handle is valid
    bool IsValid() const { return m_event != NULL; }

    // Check if handle is invalid
    bool IsInvalid() const { return !IsValid(); }

    // Release ownership of the handle (returns the handle and sets internal to NULL)
    HANDLE Release()
    {
        HANDLE temp = m_event;
        m_event = NULL;
        return temp;
    }

    // Reset the handle (closes current handle if valid)
    void Reset(HANDLE newEvent = NULL)
    {
        if (m_event != NULL)
        {
            CloseHandle(m_event);
        }
        m_event = newEvent;
    }

    // Create a new manual reset event
    bool CreateManualResetEvent(BOOL initialState = FALSE, LPSECURITY_ATTRIBUTES lpEventAttributes = NULL)
    {
        Reset(); // Close any existing event
        m_event = CreateEvent(lpEventAttributes, TRUE, initialState, NULL);
        return IsValid();
    }

    // Operator overloads for convenience
    operator HANDLE() const { return m_event; }

    bool operator==(const RAIIEvent &other) const { return m_event == other.m_event; }
    bool operator!=(const RAIIEvent &other) const { return m_event != other.m_event; }

private:
    HANDLE m_event;
};
